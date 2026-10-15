#include "Domains/BlueprintGraph/McpBPGraphBatchSteps.h"

#include "Domains/BlueprintGraph/Expression/McpBPGraphMathExpr.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

namespace McpBlueprintGraphHandlers::GraphBatch
{
namespace
{
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

// The step's own fields over the batch's shared ones (blueprintPath; RunStep settles a graphName the step leaves out).
TSharedPtr<FJsonObject> BuildStepPayload(const TSharedPtr<FJsonObject>& Batch,
                                         const TSharedPtr<FJsonObject>& Step, const FString& Edit)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Batch->Values)
    {
        if (Pair.Key != TEXT("operations") && Pair.Key != TEXT("graphName"))
        {
            Out->SetField(Pair.Key, Pair.Value);
        }
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Step->Values)
    {
        // One Blueprint per batch: the pre-check, compile and save all use the
        // batch's, so a step naming another would edit it and never save it.
        if (Pair.Key != TEXT("blueprintPath") && Pair.Key != TEXT("assetPath"))
        {
            Out->SetField(Pair.Key, Pair.Value);
        }
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

void CopyStepFields(const FMcpCapturedResponse& Reply, const TSharedPtr<FJsonObject>& Entry)
{
    static const TCHAR* const Keep[] = {
        TEXT("nodeGuid"), TEXT("nodeName"), TEXT("connected"), TEXT("appliedValue"),
        TEXT("conversionInserted"), TEXT("conversionNodeId"), TEXT("placementWarning"), TEXT("resultNodeGuid"),
        TEXT("replacedLinks"), TEXT("warnings"), TEXT("inputPins"), TEXT("boundToMembers"), TEXT("eventNodeGuid"), TEXT("eventName")};
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

// A sub-step on the node a create step made: the step's Blueprint and graph, and the node.
TSharedPtr<FJsonObject> NodeStepPayload(const TSharedPtr<FJsonObject>& StepPayload, const FString& Guid)
{
    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    for (const TCHAR* Key : {TEXT("blueprintPath"), TEXT("assetPath"), TEXT("graphName")})
    {
        FString Value;
        if (StepPayload->TryGetStringField(Key, Value)) Payload->SetStringField(Key, Value);
    }
    Payload->SetStringField(TEXT("nodeId"), Guid);
    return Payload;
}

// expression on a create step: a Math Expression node gets its formula as set_node_property Expression gives it,
// with the same checks, before pinDefaults so those can name its inputs. The field was dropped, and the node came
// back with no pins.
FString ApplyExpression(const FActionContext& Parent, const TSharedPtr<FJsonObject>& StepPayload, const FString& Guid,
                        const FString& StepId, const TSharedPtr<FJsonObject>& Entry, FString& OutErrorCode)
{
    FString Expression, Unused;
    if (!StepPayload->TryGetStringField(TEXT("expression"), Expression)) return FString();
    const TSharedPtr<FJsonObject> Payload = NodeStepPayload(StepPayload, Guid);
    Payload->SetStringField(TEXT("propertyName"), TEXT("Expression"));
    Payload->SetStringField(TEXT("propertyValue"), Expression);
    const FMcpCapturedResponse Reply = RunStep(Parent, Payload, TEXT("set_node_property"), StepId + TEXT(".expression"), Unused);
    if (!Reply.bSuccess)
    {
        OutErrorCode = Reply.ErrorCode.IsEmpty() ? TEXT("EXPRESSION_INVALID") : Reply.ErrorCode;
        return TEXT("expression: ") + Reply.Message;
    }
    CopyStepFields(Reply, Entry);
    return FString();
}

// pinDefaults on a create step: {"InString": "Hi"} sets each pin on the new node
// and reports, per pin, the literal the pin actually stored.
FString ApplyPinDefaults(const FActionContext& Parent, const TSharedPtr<FJsonObject>& StepPayload,
                         const TSharedPtr<FJsonObject>& Step, const FString& Guid, const FString& StepId,
                         const TSharedPtr<FJsonObject>& Entry)
{
    const TSharedPtr<FJsonObject>* Defaults = nullptr;
    if (!Step->TryGetObjectField(TEXT("pinDefaults"), Defaults)) return FString();
    // Only a pin that stored something other than its literal is named.
    TSharedPtr<FJsonObject> Applied = MakeShared<FJsonObject>();
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Defaults)->Values)
    {
        const TSharedPtr<FJsonObject> Payload = NodeStepPayload(StepPayload, Guid);
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
        if (Reply.Result.IsValid() && Reply.Result->TryGetStringField(TEXT("appliedValue"), AppliedValue) &&
            McpAppliedValueDiffers(Pair.Value, AppliedValue))
        {
            Applied->SetStringField(Pair.Key, AppliedValue);
        }
    }
    if (Applied->Values.Num() > 0) Entry->SetObjectField(TEXT("pinDefaults"), Applied);
    return FString();
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
        return DescribeUnbatchableEdit(Edit);
    }
    const TSharedPtr<FJsonObject> Step = *StepPtr;
    Entry->SetStringField(TEXT("edit"), Edit);
    FString Alias;
    if (Step->TryGetStringField(TEXT("id"), Alias)) Entry->SetStringField(TEXT("id"), Alias);
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
    // A pin or node property that stored the literal asked for answers nothing new; one that changed it stays named.
    FString AppliedValue;
    if (Entry->TryGetStringField(TEXT("appliedValue"), AppliedValue) &&
        !McpAppliedValueDiffers(Step->HasField(TEXT("propertyValue")) ? Step->TryGetField(TEXT("propertyValue"))
                                                                       : Step->TryGetField(TEXT("value")), AppliedValue))
    {
        Entry->RemoveField(TEXT("appliedValue"));
    }
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
    OutErrorCode = TEXT("PIN_DEFAULT_FAILED");
    FString DefaultsError = ApplyExpression(Context, Payload, Guid, StepId, Entry, OutErrorCode);
    DefaultsError = DefaultsError.IsEmpty() ? ApplyPinDefaults(Context, Payload, Step, Guid, StepId, Entry) : DefaultsError;
    if (!DefaultsError.IsEmpty())
    {
        // A failed step leaves nothing behind. Its node used to stay, so "the
        // steps before it were applied" was not the whole truth, and re-running
        // the batch from this step stacked a second copy of the node.
        // By guid over every graph: a step may target a function graph, not the batch's.
        RemoveNodeWithLiterals(Context.Blueprint, FindBatchNode(Context.Blueprint, Guid));
        Entry->RemoveField(TEXT("nodeGuid"));
        Entry->RemoveField(TEXT("nodeName"));
        // The pins it set, and the pin listing, went with the node; echoing them read as applied.
        Entry->RemoveField(TEXT("pinDefaults"));
        Entry->RemoveField(TEXT("pins"));
        return DefaultsError;
    }
    // A pin default can rebuild the node (a spawn node's Class adds its ExposeOnSpawn pins), and an expression makes
    // the inputs: list the pins it has now.
    if (Step->HasField(TEXT("pinDefaults")) || Step->HasField(TEXT("expression")))
    {
        Entry->SetStringField(TEXT("pins"), DescribeNodePins(FindBatchNode(Context.Blueprint, Guid)));
    }
    if (!Alias.IsEmpty())
    {
        State.Aliases.Add(Alias, Guid);
        NodeIds->SetStringField(Alias, Guid);
        // add_function also names its return node ("$<id>_return"), a BindEvent step the custom event it wired ("$<id>_event").
        for (const TPair<const TCHAR*, const TCHAR*>& Second : {TPair<const TCHAR*, const TCHAR*>(TEXT("resultNodeGuid"), TEXT("_return")),
                                                                TPair<const TCHAR*, const TCHAR*>(TEXT("eventNodeGuid"), TEXT("_event"))})
        {
            FString SecondGuid;
            if (Reply.Result->TryGetStringField(Second.Key, SecondGuid))
            {
                State.Aliases.Add(Alias + Second.Value, SecondGuid);
                NodeIds->SetStringField(Alias + Second.Value, SecondGuid);
            }
        }
    }
    return FString();
}

FString DescribeUnbatchableEdit(const FString& Edit)
{
    // A step with an unknown edit (set_pin_default) was told it had none.
    return FString::Printf(TEXT("%s: add_variable, add_function, add_event, add_event_dispatcher, create_node, "
                                "connect_pins, set_pin_default_value, set_node_property or create_reroute_node%s"),
        Edit.IsEmpty() ? TEXT("each step needs `edit`") : *FString::Printf(TEXT("`edit` '%s' is not a step this batch runs"), *Edit),
        Edit == TEXT("arrange_nodes") ? TEXT(". The batch already moves each node it places without posX and posY "
                                             "beside a node it is wired to.")
                                      : TEXT(""));
}

FString DescribeExpressionStep(const UBlueprint* Blueprint, const FJsonObject& Step, TSet<FName>& DeclaredBools)
{
    FString Edit, Name, Type, Expression;
    Step.TryGetStringField(TEXT("edit"), Edit);
    if (Edit == TEXT("add_variable") && Step.TryGetStringField(TEXT("variableName"), Name) &&
        Step.TryGetStringField(TEXT("variableType"), Type) &&
        (Type.Equals(TEXT("Boolean"), ESearchCase::IgnoreCase) || Type.Equals(TEXT("Bool"), ESearchCase::IgnoreCase)))
    {
        DeclaredBools.Add(FName(*Name));
    }
    const bool bCreateWithExpression = Edit == TEXT("create_node") && Step.TryGetStringField(TEXT("expression"), Expression);
    if (!bCreateWithExpression && (Edit != TEXT("set_node_property") || !Step.TryGetStringField(TEXT("propertyName"), Name) ||
        !Name.Equals(TEXT("Expression"), ESearchCase::IgnoreCase) ||
        !Step.TryGetStringField(TEXT("propertyValue"), Expression)))
    {
        return FString();
    }
    // The step itself writes a unary minus as 0 - x, so the pre-check judges that text.
    Expression = McpBlueprintMathExpression::RewriteUnaryMinus(Expression);
    const FString Problems = McpBlueprintMathExpression::DescribeProblems(Blueprint, Expression, DeclaredBools);
    return Problems.IsEmpty() ? FString()
                              : FString::Printf(TEXT("The expression '%s' would not compile: %s"), *Expression, *Problems);
}
}
