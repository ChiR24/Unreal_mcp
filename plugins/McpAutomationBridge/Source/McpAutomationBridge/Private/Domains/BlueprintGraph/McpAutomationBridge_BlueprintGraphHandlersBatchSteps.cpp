#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersBatchSteps.h"

#if WITH_EDITOR
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"

namespace McpBlueprintGraphHandlers::GraphBatch
{
namespace
{
constexpr int32 AutoColumns = 5;

// Synchronous, non-destructive edits only. delete_node / break_pin_links stay
// single calls so each keeps its own consent gate. add_variable lets one batch
// declare the variables its own Get/Set nodes use: a new Blueprint used to cost
// one add_variable call per variable before the graph could be built.
bool IsBatchableEdit(const FString& Edit)
{
    return Edit == TEXT("create_node") || Edit == TEXT("connect_pins") ||
           Edit == TEXT("set_pin_default_value") || Edit == TEXT("set_node_property") ||
           Edit == TEXT("create_reroute_node") || Edit == TEXT("add_variable");
}

// "from": "$event.then" is shorthand for fromNodeId "$event" + fromPinName "then".
void ExpandEndpoint(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Key,
                    const TCHAR* NodeField, const TCHAR* PinField)
{
    FString Endpoint;
    FString Node;
    FString Pin;
    if (Payload->TryGetStringField(Key, Endpoint) && Endpoint.Split(TEXT("."), &Node, &Pin))
    {
        Payload->SetStringField(NodeField, Node);
        Payload->SetStringField(PinField, Pin);
    }
}

// The step's own fields over the batch's shared ones (blueprintPath, graphName).
TSharedPtr<FJsonObject> BuildStepPayload(const TSharedPtr<FJsonObject>& Batch,
                                         const TSharedPtr<FJsonObject>& Step, const FString& Edit)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Batch->Values)
    {
        if (Pair.Key != TEXT("operations"))
        {
            Out->SetField(Pair.Key, Pair.Value);
        }
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Step->Values)
    {
        Out->SetField(Pair.Key, Pair.Value);
    }
    Out->SetStringField(TEXT("subAction"), Edit);
    ExpandEndpoint(Out, TEXT("from"), TEXT("fromNodeId"), TEXT("fromPinName"));
    ExpandEndpoint(Out, TEXT("to"), TEXT("toNodeId"), TEXT("toPinName"));
    return Out;
}

// "$name" -> the guid an earlier step created under `id: "name"`.
bool ResolveStepAliases(const FBatchState& State, const TSharedPtr<FJsonObject>& Payload,
                        FString& OutError)
{
    static const TCHAR* const RefFields[] = {
        TEXT("fromNodeId"), TEXT("toNodeId"), TEXT("nodeId"),
        TEXT("nodeGuid"), TEXT("sourceNode"), TEXT("targetNode")};
    for (const TCHAR* Field : RefFields)
    {
        FString Ref;
        if (!Payload->TryGetStringField(Field, Ref) || !Ref.StartsWith(TEXT("$")))
        {
            continue;
        }
        const FString* Guid = State.Aliases.Find(Ref.RightChop(1));
        if (Guid == nullptr)
        {
            TArray<FString> Known;
            State.Aliases.GenerateKeyArray(Known);
            const FString KnownList = Known.Num() > 0 ? FString::Join(Known, TEXT(", ")) : FString(TEXT("<none>"));
            OutError = FString::Printf(TEXT("%s '%s' names no earlier step. Aliases defined so far: %s."),
                                       Field, *Ref, *KnownList);
            return false;
        }
        Payload->SetStringField(Field, *Guid);
    }
    return true;
}

FMcpCapturedResponse RunStep(const FActionContext& Parent, const TSharedPtr<FJsonObject>& Payload,
                             const FString& Edit, const FString& StepId, FString& OutPins)
{
    FMcpResponseCaptureRegistry::Get().Begin(StepId);
    FActionContext Step{Parent.Subsystem, StepId, Payload, Parent.RequestingSocket, Edit};
    Step.bDeferCompile = true;
    if (Edit == TEXT("add_variable"))
    {
        // The ordinary handler, which compiles, so later steps can Get/Set it.
        McpBlueprintHandlers::HandleBlueprintAddVariable(McpBlueprintHandlers::BuildBlueprintActionContext(
            *Parent.Subsystem, StepId, Edit, Payload, Parent.RequestingSocket));
    }
    else if (PrepareBlueprintAndGraph(Step))
    {
        const bool bHandled = HandleNodeCreationAction(Step) || HandlePinMutationAction(Step) ||
                              HandleNodeMutationAction(Step);
        (void)bHandled;
    }
    FMcpCapturedResponse Reply = FMcpResponseCaptureRegistry::Get().End(StepId);
    FString Guid;
    if (Reply.bSuccess && Reply.Result.IsValid() && Step.TargetGraph &&
        Reply.Result->TryGetStringField(TEXT("nodeGuid"), Guid))
    {
        OutPins = DescribeNodePins(Step.FindNode(Guid));
    }
    return Reply;
}

// Auto-placed nodes fill a grid right of whatever the graph already holds. A
// slot that turns out to be taken -- auto or caller-chosen -- is retried at the
// overlap guard's own suggestion instead of failing the batch: a hand layout a
// few units off (estimated node sizes are only estimates) used to stop a
// 50-step batch at its third node.
FMcpCapturedResponse RunPlacedStep(const FActionContext& Parent, FBatchState& State,
                                   const TSharedPtr<FJsonObject>& Payload, const FString& Edit,
                                   const FString& StepId, FString& OutPins)
{
    const bool bCreates = Edit == TEXT("create_node") || Edit == TEXT("create_reroute_node");
    const bool bAuto = bCreates && !Payload->HasField(TEXT("posX")) && !Payload->HasField(TEXT("x"));
    if (bAuto)
    {
        const int32 Slot = State.AutoPlaced++;
        Payload->SetNumberField(TEXT("posX"), State.OriginX + (Slot % AutoColumns) * 360.0f);
        Payload->SetNumberField(TEXT("posY"), (Slot / AutoColumns) * 260.0f);
    }
    // The handlers read `x`/`y` before `posX`/`posY`; move them over so a nudge
    // below (which writes posX/posY) takes effect.
    double Coord = 0.0;
    if (Payload->TryGetNumberField(TEXT("x"), Coord))
    {
        Payload->SetNumberField(TEXT("posX"), Coord);
        Payload->RemoveField(TEXT("x"));
    }
    if (Payload->TryGetNumberField(TEXT("y"), Coord))
    {
        Payload->SetNumberField(TEXT("posY"), Coord);
        Payload->RemoveField(TEXT("y"));
    }
    FMcpCapturedResponse Reply = RunStep(Parent, Payload, Edit, StepId, OutPins);
    const TSharedPtr<FJsonObject>* Suggested = nullptr;
    for (int32 Retry = 0; bCreates && Retry < 6 && !Reply.bSuccess && Reply.ErrorCode == TEXT("NODE_OVERLAP") &&
                          Reply.Result.IsValid() && Reply.Result->TryGetObjectField(TEXT("suggestedPosition"), Suggested);
         ++Retry)
    {
        Payload->SetNumberField(TEXT("posX"), (*Suggested)->GetNumberField(TEXT("x")));
        Payload->SetNumberField(TEXT("posY"), (*Suggested)->GetNumberField(TEXT("y")));
        Reply = RunStep(Parent, Payload, Edit, StepId, OutPins);
    }
    return Reply;
}

// pinDefaults on a create step: {"InString": "Hi"} sets each pin on the new node
// and reports, per pin, the literal the pin actually stored.
FString ApplyPinDefaults(const FActionContext& Parent, const TSharedPtr<FJsonObject>& StepPayload,
                         const TSharedPtr<FJsonObject>& Step, const FString& Guid, const FString& StepId,
                         const TSharedPtr<FJsonObject>& Entry)
{
    const TSharedPtr<FJsonObject>* Defaults = nullptr;
    if (!Step->TryGetObjectField(TEXT("pinDefaults"), Defaults))
    {
        return FString();
    }
    TSharedPtr<FJsonObject> Applied = MakeShared<FJsonObject>();
    Entry->SetObjectField(TEXT("pinDefaults"), Applied);
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Defaults)->Values)
    {
        TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
        for (const TCHAR* Key : {TEXT("blueprintPath"), TEXT("assetPath"), TEXT("graphName")})
        {
            FString Value;
            if (StepPayload->TryGetStringField(Key, Value))
            {
                Payload->SetStringField(Key, Value);
            }
        }
        Payload->SetStringField(TEXT("nodeId"), Guid);
        Payload->SetStringField(TEXT("pinName"), Pair.Key);
        Payload->SetField(TEXT("propertyValue"), Pair.Value);
        FString Unused;
        const FMcpCapturedResponse Reply = RunStep(Parent, Payload, TEXT("set_pin_default_value"),
                                                   StepId + TEXT(".") + Pair.Key, Unused);
        if (!Reply.bSuccess)
        {
            return FString::Printf(TEXT("pinDefaults.%s: %s"), *Pair.Key, *Reply.Message);
        }
        FString AppliedValue;
        if (Reply.Result.IsValid() && Reply.Result->TryGetStringField(TEXT("appliedValue"), AppliedValue))
        {
            Applied->SetStringField(Pair.Key, AppliedValue);
        }
    }
    return FString();
}

void CopyStepFields(const FMcpCapturedResponse& Reply, const TSharedPtr<FJsonObject>& Entry)
{
    static const TCHAR* const Keep[] = {
        TEXT("nodeGuid"), TEXT("nodeName"), TEXT("connected"), TEXT("appliedValue"),
        TEXT("conversionInserted"), TEXT("conversionNodeId"), TEXT("placementWarning")};
    for (const TCHAR* Key : Keep)
    {
        const TSharedPtr<FJsonValue> Value = Reply.Result.IsValid() ? Reply.Result->TryGetField(Key)
                                                                    : TSharedPtr<FJsonValue>();
        if (Value.IsValid())
        {
            Entry->SetField(Key, Value);
        }
    }
}
} // namespace

FString RunBatchStep(const FActionContext& Context, FBatchState& State,
                     const TSharedPtr<FJsonValue>& StepValue, int32 Index,
                     const TSharedPtr<FJsonObject>& Entry, const TSharedPtr<FJsonObject>& NodeIds,
                     FString& OutErrorCode)
{
    OutErrorCode = TEXT("INVALID_OPERATION");
    const TSharedPtr<FJsonObject>* StepPtr = nullptr;
    FString Edit;
    if (!StepValue.IsValid() || !StepValue->TryGetObject(StepPtr) ||
        !(*StepPtr)->TryGetStringField(TEXT("edit"), Edit) || !IsBatchableEdit(Edit))
    {
        return TEXT("each step needs `edit`: add_variable, create_node, connect_pins, "
                    "set_pin_default_value, set_node_property or create_reroute_node");
    }
    const TSharedPtr<FJsonObject> Step = *StepPtr;
    Entry->SetStringField(TEXT("edit"), Edit);
    FString Alias;
    if (Step->TryGetStringField(TEXT("id"), Alias))
    {
        Entry->SetStringField(TEXT("id"), Alias);
    }
    const TSharedPtr<FJsonObject> Payload = BuildStepPayload(Context.Payload, Step, Edit);
    FString Error;
    if (!ResolveStepAliases(State, Payload, Error))
    {
        return Error;
    }
    const FString StepId = FString::Printf(TEXT("%s#step%d"), *Context.RequestId, Index);
    FString Pins;
    const FMcpCapturedResponse Reply = RunPlacedStep(Context, State, Payload, Edit, StepId, Pins);
    CopyStepFields(Reply, Entry);
    if (!Reply.bSuccess)
    {
        OutErrorCode = Reply.ErrorCode.IsEmpty() ? TEXT("STEP_FAILED") : Reply.ErrorCode;
        return Reply.bCaptured ? Reply.Message : FString(TEXT("the step sent no reply"));
    }
    FString Guid;
    if (!Reply.Result.IsValid() || !Reply.Result->TryGetStringField(TEXT("nodeGuid"), Guid))
    {
        return FString();
    }
    Entry->SetStringField(TEXT("pins"), Pins);
    if (!Alias.IsEmpty())
    {
        State.Aliases.Add(Alias, Guid);
        NodeIds->SetStringField(Alias, Guid);
    }
    OutErrorCode = TEXT("PIN_DEFAULT_FAILED");
    return ApplyPinDefaults(Context, Payload, Step, Guid, StepId, Entry);
}
}
#endif
