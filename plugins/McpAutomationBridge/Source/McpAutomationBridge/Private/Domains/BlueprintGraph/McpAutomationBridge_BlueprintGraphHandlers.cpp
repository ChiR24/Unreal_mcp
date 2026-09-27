#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

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
        // ResolveUClass rejects a Blueprint's own names ("BP_Door_C", "/Game/X/BP_Door"),
        // which are what a caller writes; the shared class-pin resolver takes both.
        OutResolvedClass = ResolveUClass(MemberClass);
        if (!OutResolvedClass)
        {
            OutResolvedClass = ResolveTargetClassFromString(MemberClass);
        }
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

// "Function not found" hid the usual cause: the memberClass itself resolved nothing.
FString DescribeMissingFunction(UBlueprint* Blueprint, const FString& MemberName,
                                const FString& MemberClass, UClass* ResolvedClass)
{
    if (!MemberClass.IsEmpty() && !ResolvedClass)
    {
        return FString::Printf(TEXT("memberClass '%s' is not a class: pass a native class name (KismetMathLibrary) "
                                    "or a Blueprint asset path (/Game/Folder/BP_Name)."), *MemberClass);
    }
    UClass* HintClass = ResolvedClass ? ResolvedClass : (Blueprint ? Blueprint->GeneratedClass.Get() : nullptr);
    return FString::Printf(TEXT("Function '%s' not found.%s"), *MemberName, *SuggestMemberFix(HintClass, MemberName));
}
}

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
        McpBlueprintGraphHandlers::HandleNodeMutationAction(Context))
    {
        return true;
    }
    // A read never compiles. SendResponse compiles a Blueprint it finds dirty,
    // and a read cannot have dirtied it - a failed build_graph or an uncompiled
    // edit in the editor did. inspect_graph compiled such a Blueprint, and the
    // compile cleared the editor's whole undo history.
    Context.bDeferCompile = true;
    if (McpBlueprintGraphHandlers::HandleNodeQueryAction(Context) ||
        McpBlueprintGraphHandlers::HandleNodeDetailAction(Context))
    {
        return true;
    }

    Context.SendError(
        FString::Printf(TEXT("Unknown subAction: %s"), *Context.SubAction),
        TEXT("INVALID_SUBACTION"));
    return true;
}
