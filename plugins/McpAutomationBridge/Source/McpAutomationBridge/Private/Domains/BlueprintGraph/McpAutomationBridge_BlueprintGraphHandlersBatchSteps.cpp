#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersBatchSteps.h"

#include "Domains/BlueprintGraph/Expression/McpAutomationBridge_BlueprintGraphMathExpression.h"

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
        TEXT("conversionInserted"), TEXT("conversionNodeId"), TEXT("placementWarning"), TEXT("resultNodeGuid"),
        TEXT("replacedLinks"), TEXT("warnings"), TEXT("inputPins"), TEXT("boundToMembers")};
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
        // A step with an unknown edit (set_pin_default) was told it had none.
        return FString::Printf(TEXT("%s: add_variable, add_function, add_event, add_event_dispatcher, create_node, "
                                    "connect_pins, set_pin_default_value, set_node_property or create_reroute_node"),
            Edit.IsEmpty() ? TEXT("each step needs `edit`") : *FString::Printf(TEXT("`edit` '%s' is not a step this batch runs"), *Edit));
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
    OutErrorCode = TEXT("PIN_DEFAULT_FAILED");
    const FString DefaultsError = ApplyPinDefaults(Context, Payload, Step, Guid, StepId, Entry);
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
    if (!Alias.IsEmpty())
    {
        State.Aliases.Add(Alias, Guid);
        NodeIds->SetStringField(Alias, Guid);
        // add_function also names its return node: "$<id>_return".
        FString ReturnGuid;
        if (Reply.Result->TryGetStringField(TEXT("resultNodeGuid"), ReturnGuid))
        {
            State.Aliases.Add(Alias + TEXT("_return"), ReturnGuid);
            NodeIds->SetStringField(Alias + TEXT("_return"), ReturnGuid);
        }
    }
    return FString();
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
    if (Edit != TEXT("set_node_property") || !Step.TryGetStringField(TEXT("propertyName"), Name) ||
        !Name.Equals(TEXT("Expression"), ESearchCase::IgnoreCase) ||
        !Step.TryGetStringField(TEXT("propertyValue"), Expression))
    {
        return FString();
    }
    const FString Problems = McpBlueprintMathExpression::DescribeProblems(Blueprint, Expression, DeclaredBools);
    return Problems.IsEmpty() ? FString()
                              : FString::Printf(TEXT("The expression '%s' would not compile: %s"), *Expression, *Problems);
}
}
