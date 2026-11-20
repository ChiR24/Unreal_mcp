#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "Core/Routing/McpConsolidatedActionRouting.h"
#include "Domains/BlueprintGraph/Code/McpGraphCode.h"
#include "Domains/BlueprintGraph/Lint/McpBPLint.h"
#include "Domains/BlueprintGraph/Slice/McpBPSlice.h"

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
            It->HasAnyClassFlags(CLASS_Deprecated) || McpIsCompileDebrisClass(*It))
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
        // A Blueprint's own names ("BP_Door_C", "/Game/X/BP_Door") are what a caller writes; ResolveClassByName takes them.
        OutResolvedClass = ResolveClassByName(MemberClass);
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
}

bool UEngineWireSubsystem::HandleBlueprintGraphAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (!Payload.IsValid())
    {
        SendAutomationError(
            RequestingSocket,
            RequestId,
            TEXT("Missing payload for blueprint graph action."),
            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // connect_nodes (edit_graph's folded name for connect_pins, the word callers bring from the material graph) runs as it.
    const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(Payload);
    McpBlueprintGraphHandlers::FActionContext Context{
        this,
        RequestId,
        Payload,
        RequestingSocket,
        SubAction == TEXT("connect_nodes") ? FString(TEXT("connect_pins")) : SubAction};

    if (McpBlueprintGraphHandlers::HandleListNodeTypes(Context) ||
        McpBlueprintGraphHandlers::HandleFindFunctions(Context))
    {
        return true;
    }

    // A lint (and a slice) reads every graph, so a Blueprint without an event graph (a function library) is linted too; so does
    // read_graph_as_code unless it names one graph, and write_graph_from_code, whose (fn) forms need none.
    const bool bLint = Context.SubAction == TEXT("lint_blueprint") || Context.SubAction == TEXT("slice_variable") || Context.SubAction == TEXT("slice_function") || Context.SubAction == TEXT("explain_variable") || Context.SubAction == TEXT("explain_function");
    const bool bCode = Context.SubAction == TEXT("read_graph_as_code") || Context.SubAction == TEXT("write_graph_from_code");
    if (!McpBlueprintGraphHandlers::PrepareBlueprintAndGraph(Context, /*bRequireGraph=*/bCode ? Payload->HasField(TEXT("graphName")) : !bLint))
    {
        return true;
    }
    if (bLint || bCode)
    {
        Context.bDeferCompile = true; // a read never compiles; write_graph_from_code compiles once itself
        return bLint ? McpBPLint::HandleLint(Context) || McpBPSlice::HandleSlice(Context) || McpBPSlice::HandleExplain(Context) : McpGraphCode::HandleReadGraphAsCode(Context) || McpGraphCode::HandleWriteGraphFromCode(Context);
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
