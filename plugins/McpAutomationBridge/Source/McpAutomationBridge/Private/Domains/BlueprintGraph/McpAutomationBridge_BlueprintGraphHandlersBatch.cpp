#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviourNodes.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersBatchSteps.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintDiagnostics.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

// build_graph: one call that runs a list of graph edits. Wiring one event chain
// used to cost a round trip per node, per link and per pin default; a batch runs
// the SAME single-step handlers in-process (their replies are parked by
// FMcpResponseCaptureRegistry instead of sent), so every step keeps its checks.
// The batch stops at the first failing step; steps before it stay applied and
// their node ids come back so the caller can continue from there.
namespace McpBlueprintGraphHandlers
{
namespace
{
bool IsCallFunctionType(const FString& NodeType)
{
    return NodeType == TEXT("CallFunction") || NodeType == TEXT("K2Node_CallFunction") ||
           NodeType == TEXT("FunctionCall");
}
}

namespace GraphBatch
{
// delete_node / break_pin_links stay single calls so each keeps its own consent
// gate. The member steps let one batch declare what its own nodes use: a new
// Blueprint used to cost a call per variable or function before the graph could be built.
bool IsBatchableEdit(const FString& Edit)
{
    return Edit == TEXT("create_node") || Edit == TEXT("connect_pins") ||
           Edit == TEXT("set_pin_default_value") || Edit == TEXT("set_node_property") ||
           Edit == TEXT("create_reroute_node") || Edit == TEXT("add_variable") || Edit == TEXT("add_function") ||
           Edit == TEXT("add_event") || Edit == TEXT("add_event_dispatcher");
}

bool RunBlueprintMemberStep(const FActionContext& Parent, const FString& Edit, const FString& StepId,
                            const TSharedPtr<FJsonObject>& Payload)
{
    struct FBatchMemberStep
    {
        const TCHAR* Edit;
        bool (*Handler)(const McpBlueprintHandlers::FBlueprintActionContext&);
    };
    static const FBatchMemberStep Members[] = {
        {TEXT("add_variable"), &McpBlueprintHandlers::HandleBlueprintAddVariable},
        {TEXT("add_function"), &McpBlueprintHandlers::HandleBlueprintAddFunction},
        {TEXT("add_event"), &McpBlueprintHandlers::HandleBlueprintAddEvent},
        {TEXT("add_event_dispatcher"), &McpBlueprintHandlers::HandleBlueprintAddEventDispatcher}};
    for (const FBatchMemberStep& Member : Members)
    {
        if (Edit == Member.Edit)
        {
            Member.Handler(McpBlueprintHandlers::BuildBlueprintActionContext(*Parent.Subsystem, StepId, Edit, Payload,
                                                                            Parent.RequestingSocket));
            return true;
        }
    }
    return false;
}

UEdGraphNode* FindBatchNode(UBlueprint* Blueprint, const FString& Guid)
{
    FGuid Parsed;
    return Blueprint && FGuid::Parse(Guid, Parsed) ? FBlueprintEditorUtils::GetNodeByGUID(Blueprint, Parsed) : nullptr;
}

// Every function and variable a step names is resolved before any step runs. A
// misspelled name at step 10 used to leave steps 0-9 applied, and the caller had
// to continue the half-built graph by node guid.
FString PrecheckSteps(const FActionContext& Context, const TArray<TSharedPtr<FJsonValue>>& Steps,
                      int32& OutIndex, FString& OutCode)
{
    const bool bWidgetBlueprint = FindObject<UObject>(Context.Blueprint, TEXT("WidgetTree")) != nullptr;
    TSet<FName> Declared;
    for (int32 Index = 0; Index < Steps.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Step = nullptr;
        if (!Steps[Index].IsValid() || !Steps[Index]->TryGetObject(Step) || Step == nullptr)
        {
            continue;
        }
        FString Edit, NodeType, Member, MemberClass;
        (*Step)->TryGetStringField(TEXT("edit"), Edit);
        (*Step)->TryGetStringField(TEXT("nodeType"), NodeType);
        (*Step)->TryGetStringField(TEXT("memberName"), Member);
        if (!(*Step)->TryGetStringField(TEXT("memberClass"), MemberClass))
        {
            (*Step)->TryGetStringField(TEXT("targetClass"), MemberClass);
        }
        if (Edit == TEXT("add_variable") || Edit == TEXT("add_function") || Edit == TEXT("add_event") ||
            Edit == TEXT("add_event_dispatcher"))
        {
            if ((*Step)->HasField(TEXT("pinDefaults")))
            {
                OutIndex = Index;
                OutCode = TEXT("INVALID_OPERATION");
                return TEXT("pinDefaults applies to create_node steps; a member step (add_variable, add_function, "
                            "add_event, add_event_dispatcher) makes no node to set pins on.");
            }
            // What a later step can name: the variable, function, custom event or dispatcher,
            // under every field the member handlers read (add_function and add_event_dispatcher
            // also take name or memberName).
            Declared.Add(FName(*McpGetFirstStringField(*Step, {TEXT("variableName"), TEXT("functionName"),
                                                                TEXT("customEventName"), TEXT("eventName"),
                                                                TEXT("dispatcherName"), TEXT("name"),
                                                                TEXT("memberName")})));
            continue;
        }
        if (Edit != TEXT("create_node") || Member.IsEmpty())
        {
            continue;
        }
        OutIndex = Index;
        UClass* ResolvedClass = nullptr;
        if (IsCallFunctionType(NodeType) && !(MemberClass.IsEmpty() && Declared.Contains(FName(*Member))) &&
            !ResolveGraphCallFunction(Context.Blueprint, Member, MemberClass, ResolvedClass))
        {
            OutCode = TEXT("FUNCTION_NOT_FOUND");
            return DescribeMissingFunction(Context.Blueprint, Member, MemberClass, ResolvedClass);
        }
        const FName Variable(*Member);
        bool bSetNode = false;
        if (ParseVariableNodeType(NodeType, bSetNode) && MemberClass.IsEmpty() && !bWidgetBlueprint &&
            !Declared.Contains(Variable) &&
            FBlueprintEditorUtils::FindNewVariableIndex(Context.Blueprint, Variable) == INDEX_NONE &&
            !(Context.Blueprint->GeneratedClass && Context.Blueprint->GeneratedClass->FindPropertyByName(Variable)))
        {
            OutCode = TEXT("VARIABLE_NOT_FOUND");
            return FString::Printf(TEXT("Variable '%s' not found in the Blueprint, its components or any parent "
                                        "class, and no earlier add_variable step declares it."), *Member);
        }
        const FString Behaviour = PrecheckBehaviourNode(Context, NodeType, Member, MemberClass, Declared, OutCode);
        if (!Behaviour.IsEmpty())
        {
            return Behaviour;
        }
    }
    OutIndex = INDEX_NONE;
    return FString();
}

bool RunGraphBatch(FActionContext& Context, int32 MaxSteps, bool bCompile)
{
    const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
    if (!Context.Payload->TryGetArrayField(TEXT("operations"), Steps) || Steps->Num() == 0 ||
        Steps->Num() > MaxSteps)
    {
        Context.SendError(FString::Printf(
            TEXT("build_graph needs `operations`: 1-%d steps, each {edit, ...that edit's params}, "
                 "optionally `id` to name a created node for later steps as \"$id\"."),
            MaxSteps), TEXT("INVALID_OPERATIONS"));
        return true;
    }

    int32 BadIndex = INDEX_NONE;
    FString BadCode;
    const FString Precheck = PrecheckSteps(Context, *Steps, BadIndex, BadCode);
    if (!Precheck.IsEmpty())
    {
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetNumberField(TEXT("succeeded"), 0);
        Details->SetNumberField(TEXT("failedIndex"), BadIndex);
        Context.SendErrorWithDetails(FString::Printf(
            TEXT("build_graph checked every step before running any: operations[%d] would fail. %s "
                 "Nothing was applied."), BadIndex, *Precheck), BadCode, Details);
        return true;
    }

    FBatchState State;
    for (UEdGraphNode* Existing : Context.TargetGraph->Nodes)
    {
        float Width = 0.0f;
        float Height = 0.0f;
        if (Existing)
        {
            McpGraphLayout::EstimateNodeExtent(*Existing, Width, Height);
            State.OriginX = FMath::Max(State.OriginX, Existing->NodePosX + Width + 240.0f);
            // "$entry" is the graph's own entry node (a Construction Script's
            // exec start), which used to need an inspect_graph call to find.
            if (Existing->IsA<UK2Node_FunctionEntry>())
            {
                State.Aliases.Add(TEXT("entry"), Existing->NodeGuid.ToString());
            }
        }
    }

    TArray<TSharedPtr<FJsonValue>> Results;
    TSharedPtr<FJsonObject> NodeIds = MakeShared<FJsonObject>();
    for (int32 Index = 0; Index < Steps->Num(); ++Index)
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetNumberField(TEXT("index"), Index);
        FString ErrorCode;
        FString Error;
        {
            // The step's own save is deferred to the one this batch makes below.
            FMcpDeferAssetSaves DeferSave;
            Error = RunBatchStep(Context, State, (*Steps)[Index], Index, Entry, NodeIds, ErrorCode);
        }
        Entry->SetBoolField(TEXT("success"), Error.IsEmpty());
        Results.Add(MakeShared<FJsonValueObject>(Entry));
        if (Error.IsEmpty())
        {
            continue;
        }
        // The steps before this one stay applied, so they are saved as before.
        SaveLoadedAssetThrottled(Context.Blueprint);
        Entry->SetStringField(TEXT("error"), Error);
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetArrayField(TEXT("results"), Results);
        Details->SetObjectField(TEXT("nodeIds"), NodeIds);
        Details->SetNumberField(TEXT("succeeded"), Index);
        Details->SetNumberField(TEXT("failedIndex"), Index);
        FString Reason = Error;
        Reason.RemoveFromEnd(TEXT("."));
        Context.SendErrorWithDetails(FString::Printf(
            TEXT("build_graph stopped at operations[%d]: %s. The %d step(s) before it were applied; "
                 "their node ids are in the error detail's `nodeIds`."), Index, *Reason, Index), ErrorCode, Details);
        return true;
    }

    SettleAutoPlacedNodes(Context.Blueprint, State);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("results"), Results);
    Result->SetObjectField(TEXT("nodeIds"), NodeIds);
    Result->SetNumberField(TEXT("succeeded"), Results.Num());
    // Every step ran, so the Blueprint changed. The compile below leaves it clean, which the reply funnel
    // reads as "unchanged", so the batch says it itself: the receipt then names and lists the Blueprint.
    Context.NameBlueprint(Result, /*bChanged=*/true);
    if (!bCompile)
    {
        // The caller compiles and saves once its own work is done.
        Context.bDeferCompile = true;
        Context.SendResponse(FString::Printf(TEXT("Ran %d graph operations."), Results.Num()), Result);
        return true;
    }
    FString FirstError;
    const bool bCompiled = McpCompileBlueprintWithDiagnostics(Context.Blueprint, Result, FirstError, 12);
    Result->SetBoolField(TEXT("saved"), SaveLoadedAssetThrottled(Context.Blueprint));
    Context.SendResponse(bCompiled
        ? FString::Printf(TEXT("Ran %d graph operations; the blueprint compiles."), Results.Num())
        : FString::Printf(TEXT("Ran %d graph operations. WARNING: the blueprint does not compile: %s"),
                          Results.Num(), FirstError.IsEmpty() ? TEXT("no compiler message") : *FirstError),
        Result);
    return true;
}
} // namespace GraphBatch

bool HandleGraphBatchAction(FActionContext& Context)
{
    return Context.SubAction == TEXT("build_graph") &&
           GraphBatch::RunGraphBatch(Context, GraphBatch::MaxBatchSteps, /*bCompile=*/true);
}
}
