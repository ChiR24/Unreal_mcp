#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "EdGraphSchema_K2.h"
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

// The editor shows K2_AddLocalRotation as AddLocalRotation, and a caller writes the name it sees: AddLocalRotation
// failed FUNCTION_NOT_FOUND on SceneComponent. The plain name can also be a native function Blueprints cannot call
// (TextRenderComponent's SetText beside the K2_SetText the editor shows as Set Text): the node was made and the
// Blueprint then failed to compile, so a name that is not Blueprint-callable gives way to its K2_ twin.
static UFunction* FindCallableNamed(UClass* Class, const FString& Name)
{
    UFunction* Function = Class->FindFunctionByName(*Name);
    if (Name.StartsWith(TEXT("K2_")) ||
        (Function && Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure)))
    {
        return Function;
    }
    UFunction* Twin = Class->FindFunctionByName(*(TEXT("K2_") + Name));
    if (Twin || Function)
    {
        return Twin ? Twin : Function;
    }
    // The title the editor shows, spaces aside: GetActorTransform is GetTransform, and failed FUNCTION_NOT_FOUND.
    for (TFieldIterator<UFunction> It(Class); It; ++It)
    {
        if (It->HasAnyFunctionFlags(FUNC_BlueprintCallable) && It->HasMetaData(TEXT("DisplayName")) &&
            It->GetMetaData(TEXT("DisplayName")).Replace(TEXT(" "), TEXT("")).Equals(Name, ESearchCase::IgnoreCase))
        {
            return *It;
        }
    }
    return nullptr;
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
        UFunction* Function = FindCallableNamed(OutResolvedClass, MemberName);
        if (!Function && OutResolvedClass->IsChildOf(UBlueprintFunctionLibrary::StaticClass()))
        {
            Function = FindUniqueLibraryFunction(MemberName);
        }
        return Function;
    }
    // String and Text joined the stock libraries: Conv_IntToText and
    // Concat_StrStr failed without a memberClass. The skeleton class already has a
    // function or custom event added earlier in the same batch, before any compile.
    UClass* Defaults[] = {Blueprint ? Blueprint->GeneratedClass.Get() : nullptr,
        Blueprint ? Blueprint->SkeletonGeneratedClass.Get() : nullptr,
        UKismetSystemLibrary::StaticClass(), UGameplayStatics::StaticClass(),
        UKismetMathLibrary::StaticClass(), UKismetStringLibrary::StaticClass(),
        UKismetTextLibrary::StaticClass()};
    for (UClass* Candidate : Defaults)
    {
        if (UFunction* Function = Candidate ? FindCallableNamed(Candidate, MemberName) : nullptr)
        {
            return Function;
        }
    }
    return nullptr;
}

// targetClass stands in for a missing memberClass, but GetAllActorsOfClass, GetActorOfClass and the other
// functions whose result follows a class pin take it as the class they look for: {memberName:
// GetAllActorsOfClass, targetClass: BP_Enemy} failed FUNCTION_NOT_FOUND on BP_Enemy, and beside a memberClass
// it was dropped, so the pin stayed empty and OutActors an array of plain Actors.
UFunction* ResolveCallNodeFunction(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, const FString& MemberName,
                                   FString& OutOwnerClass, FString& OutOutputClass, UClass*& OutResolvedClass)
{
    FString MemberClass;
    FString TargetClass;
    Payload->TryGetStringField(TEXT("memberClass"), MemberClass);
    Payload->TryGetStringField(TEXT("targetClass"), TargetClass);
    OutOwnerClass = MemberClass.IsEmpty() ? TargetClass : MemberClass;
    OutOutputClass = MemberClass.IsEmpty() ? FString() : TargetClass;
    UFunction* Function = ResolveGraphCallFunction(Blueprint, MemberName, OutOwnerClass, OutResolvedClass);
    if (Function || !MemberClass.IsEmpty() || TargetClass.IsEmpty())
    {
        return Function;
    }
    UClass* LibraryClass = nullptr;
    UFunction* Library = ResolveGraphCallFunction(Blueprint, MemberName, FString(), LibraryClass);
    if (!Library || !Library->HasMetaData(FBlueprintMetadata::MD_DynamicOutputType))
    {
        return nullptr; // targetClass named the owner after all; the error is reported against it
    }
    OutOwnerClass.Empty();
    OutOutputClass = TargetClass;
    OutResolvedClass = LibraryClass;
    return Library;
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
    FString Hint = SuggestMemberFix(HintClass, MemberName);
    // Without a memberClass the stock libraries are searched too, so their near names are the likely fix.
    UClass* Libraries[] = {UKismetMathLibrary::StaticClass(), UKismetSystemLibrary::StaticClass(), UGameplayStatics::StaticClass()};
    for (int32 Index = 0; Hint.IsEmpty() && MemberClass.IsEmpty() && Index < UE_ARRAY_COUNT(Libraries); ++Index)
    {
        Hint = SuggestMemberFix(Libraries[Index], MemberName);
    }
    return FString::Printf(TEXT("Function '%s' not found.%s To find a function by what it does: inspect_graph info "
                                "functions, words saying what the node should do."), *MemberName, *Hint);
}

// The name may be exactly right and live on another class: a function library
// (RemoveAllWidgets is on UWidgetLayoutLibrary while every neighbouring widget
// helper is on UWidgetBlueprintLibrary) or a component class. Only libraries were
// searched, so SetScalarParameterValueOnMaterials asked of PrimitiveComponent came
// back a bare "not found" although MeshComponent declares it.
FString DescribeDeclaringClasses(UClass* Class, const FString& Wanted)
{
    const FName WantedName(*Wanted);
    TArray<UClass*> Declarers;
    for (TObjectIterator<UClass> It; It && Declarers.Num() < 4; ++It)
    {
        const FString Name = It->GetName();
        if (*It == Class || It->HasAnyClassFlags(CLASS_NewerVersionExists | CLASS_Deprecated) ||
            Name.StartsWith(TEXT("SKEL_")) || Name.StartsWith(TEXT("REINST_")) || Name.StartsWith(TEXT("TRASHCLASS_")))
        {
            continue;
        }
        const UFunction* Found = It->FindFunctionByName(WantedName, EIncludeSuperFlag::ExcludeSuper);
        if (Found && Found->HasAnyFunctionFlags(FUNC_BlueprintCallable))
        {
            Declarers.Add(*It);
        }
    }
    if (Declarers.Num() == 1)
    {
        return FString::Printf(TEXT(" '%s' is not on %s, but %s declares it - retry with memberClass '%s'."),
                               *Wanted, *Class->GetName(), *Declarers[0]->GetName(), *Declarers[0]->GetPathName());
    }
    TArray<FString> Paths;
    for (const UClass* Declarer : Declarers)
    {
        Paths.Add(Declarer->GetPathName());
    }
    return Paths.Num() == 0 ? FString() : FString::Printf(
        TEXT(" '%s' is not on %s, but these declare it: %s - retry with the memberClass of the object the node acts on."),
        *Wanted, *Class->GetName(), *FString::Join(Paths, TEXT(", ")));
}
}

bool UEngineWireSubsystem::HandleBlueprintGraphAction(
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

    if (McpBlueprintGraphHandlers::HandleListNodeTypes(Context) ||
        McpBlueprintGraphHandlers::HandleFindFunctions(Context))
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
