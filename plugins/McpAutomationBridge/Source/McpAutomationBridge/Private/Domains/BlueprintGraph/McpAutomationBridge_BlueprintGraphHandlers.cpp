#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetTextLibrary.h"

namespace McpBlueprintGraphHandlers
{
// A library function named on the wrong library (GetGameTimeInSeconds on
// GameplayStatics) failed a whole batch although a static call has no target
// to get wrong. Take the one library that declares it; two or more stay an error.
static UFunction* FindUniqueLibraryFunction(const FString& Name)
{
    UFunction* Found = nullptr;
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (!It->IsChildOf(UBlueprintFunctionLibrary::StaticClass()) ||
            It->HasAnyClassFlags(CLASS_NewerVersionExists | CLASS_Deprecated) ||
            It->GetName().StartsWith(TEXT("SKEL_")))
        {
            continue;
        }
        UFunction* Candidate = It->FindFunctionByName(*Name, EIncludeSuperFlag::ExcludeSuper);
        if (Candidate && Candidate->HasAnyFunctionFlags(FUNC_BlueprintCallable))
        {
            if (Found)
            {
                return nullptr;
            }
            Found = Candidate;
        }
    }
    return Found;
}

UFunction* ResolveGraphCallFunction(UBlueprint* Blueprint, const FString& MemberName,
                                    const FString& MemberClass, UClass*& OutResolvedClass)
{
    OutResolvedClass = nullptr;
    if (!MemberClass.IsEmpty())
    {
        OutResolvedClass = ResolveUClass(MemberClass);
        if (!OutResolvedClass)
        {
            return nullptr;
        }
        UFunction* Function = OutResolvedClass->FindFunctionByName(*MemberName);
        if (!Function && OutResolvedClass->IsChildOf(UBlueprintFunctionLibrary::StaticClass()))
        {
            Function = FindUniqueLibraryFunction(MemberName);
        }
        return Function;
    }
    // String and Text joined the stock libraries: Conv_IntToText and
    // Concat_StrStr failed without a memberClass.
    UClass* Defaults[] = {Blueprint ? Blueprint->GeneratedClass.Get() : nullptr,
        UKismetSystemLibrary::StaticClass(), UGameplayStatics::StaticClass(),
        UKismetMathLibrary::StaticClass(), UKismetStringLibrary::StaticClass(),
        UKismetTextLibrary::StaticClass()};
    for (UClass* Candidate : Defaults)
    {
        if (UFunction* Function = Candidate ? Candidate->FindFunctionByName(*MemberName) : nullptr)
        {
            return Function;
        }
    }
    return nullptr;
}
}
#endif

bool UMcpAutomationBridgeSubsystem::HandleBlueprintGraphAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_blueprint"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(
            RequestingSocket,
            RequestId,
            TEXT("Missing payload for blueprint graph action."),
            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    McpBlueprintGraphHandlers::FActionContext Context{
        this,
        RequestId,
        Payload,
        RequestingSocket,
        GetJsonStringField(Payload, TEXT("subAction"))};

    if (!McpBlueprintGraphHandlers::ValidateProvidedPaths(Context))
    {
        return true;
    }

    if (McpBlueprintGraphHandlers::HandleListNodeTypes(Context))
    {
        return true;
    }

    if (!McpBlueprintGraphHandlers::PrepareBlueprintAndGraph(Context))
    {
        return true;
    }

    if (McpBlueprintGraphHandlers::HandleGraphBatchAction(Context) ||
        McpBlueprintGraphHandlers::HandleNodeCreationAction(Context) ||
        McpBlueprintGraphHandlers::HandlePinMutationAction(Context) ||
        McpBlueprintGraphHandlers::HandleNodeMutationAction(Context) ||
        McpBlueprintGraphHandlers::HandleNodeQueryAction(Context) ||
        McpBlueprintGraphHandlers::HandleNodeDetailAction(Context))
    {
        return true;
    }

    Context.SendError(
        FString::Printf(TEXT("Unknown subAction: %s"), *Context.SubAction),
        TEXT("INVALID_SUBACTION"));
    return true;
#else
    SendAutomationError(
        RequestingSocket,
        RequestId,
        TEXT("Blueprint graph actions are editor-only."),
        TEXT("EDITOR_ONLY"));
    return true;
#endif
}
