#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "Domains/BlueprintGraph/McpBPGraphBatchSteps.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "K2Node_FunctionEntry.h"

// What build_graph checks before a step runs: the payload each step is sent, with its "$id" references resolved
// (moved here from McpBPGraphBatchSteps.cpp), the pre-check's refusal, and dryRun, which makes every check a run can
// make up front and runs nothing.
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

const TCHAR* const NodeRefFields[] = {TEXT("fromNodeId"), TEXT("toNodeId"), TEXT("nodeId"),
                                     TEXT("nodeGuid"), TEXT("sourceNode"), TEXT("targetNode")};

// The ids a step leaves for later ones, as the run registers them from the step's reply: a node-making step's id,
// an add_function's "<id>_return" when it declares outputs, a BindEvent's "<id>_event".
void NoteStepIds(FBatchState& State, const FJsonObject& Step, const FString& Edit, const int32 Index)
{
    FString Id;
    const bool bMakesNode = Edit == TEXT("create_node") || Edit == TEXT("create_reroute_node") ||
                            Edit == TEXT("add_function") || Edit == TEXT("add_event");
    if (!bMakesNode || !Step.TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
    {
        return;
    }
    const FString Made = FString::Printf(TEXT("operations[%d]"), Index); // not a guid, so no node lookup follows it
    State.Aliases.Add(Id, Made);
    const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
    if (Edit == TEXT("add_function") && Step.TryGetArrayField(TEXT("outputs"), Outputs) && Outputs->Num() > 0)
    {
        State.Aliases.Add(Id + TEXT("_return"), Made);
    }
    FString NodeType;
    if (Step.TryGetStringField(TEXT("nodeType"), NodeType) && NodeType.Equals(TEXT("BindEvent"), ESearchCase::IgnoreCase))
    {
        State.Aliases.Add(Id + TEXT("_event"), Made);
    }
}
} // namespace

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

bool PassesPrecheck(const FActionContext& Context, const TArray<TSharedPtr<FJsonValue>>& Steps, const bool bDryRun)
{
    int32 BadIndex = INDEX_NONE;
    FString BadCode;
    const FString Precheck = PrecheckSteps(Context, Steps, BadIndex, BadCode);
    if (Precheck.IsEmpty())
    {
        return true;
    }
    TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
    Details->SetNumberField(TEXT("succeeded"), 0);
    Details->SetNumberField(TEXT("failedIndex"), BadIndex);
    if (bDryRun)
    {
        Details->SetBoolField(TEXT("dryRun"), true);
    }
    Context.SendErrorWithDetails(FString::Printf(
        TEXT("build_graph checked every step before running any: operations[%d] would fail. %s "
             "Nothing was applied."), BadIndex, *Precheck), BadCode, Details);
    return false;
}

bool AnswerDryRun(const FActionContext& Context, const TArray<TSharedPtr<FJsonValue>>& Steps)
{
    // The pre-check passed. Every "$id" is resolved as the run resolves it, against the ids of the steps before it
    // ("$entry" is the graph's own entry node), and every node named by guid must be in the Blueprint.
    FBatchState State;
    for (const UEdGraphNode* Node : Context.TargetGraph->Nodes)
    {
        if (Node && Node->IsA<UK2Node_FunctionEntry>())
        {
            State.Aliases.Add(TEXT("entry"), Node->NodeGuid.ToString());
        }
    }
    TArray<TSharedPtr<FJsonValue>> Problems;
    const auto Problem = [&Problems](const int32 Index, const FString& Edit, const TCHAR* Code, const FString& Error)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("index"), Index);
        Row->SetStringField(TEXT("edit"), Edit);
        Row->SetStringField(TEXT("errorCode"), Code);
        Row->SetStringField(TEXT("error"), Error);
        Problems.Add(MakeShared<FJsonValueObject>(Row));
    };
    int32 WouldCreate = 0;
    for (int32 Index = 0; Index < Steps.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Step = nullptr;
        FString Edit;
        if (!Steps[Index].IsValid() || !Steps[Index]->TryGetObject(Step) || !Step || !(*Step)->TryGetStringField(TEXT("edit"), Edit))
        {
            Problem(Index, Edit, TEXT("INVALID_OPERATION"), DescribeUnbatchableEdit(Edit));
            continue;
        }
        const TSharedPtr<FJsonObject> Payload = BuildStepPayload(Context.Payload, *Step, Edit);
        FString Error;
        if (!ResolveStepAliases(State, Payload, Error))
        {
            Problem(Index, Edit, TEXT("INVALID_OPERATION"), Error);
        }
        for (const TCHAR* Field : NodeRefFields)
        {
            FString Ref;
            FGuid Guid;
            if (Payload->TryGetStringField(Field, Ref) && FGuid::Parse(Ref, Guid) && !FindBatchNode(Context.Blueprint, Ref))
            {
                Problem(Index, Edit, TEXT("NODE_NOT_FOUND"), FString::Printf(TEXT("%s '%s' is no node of the Blueprint."), Field, *Ref));
            }
        }
        NoteStepIds(State, **Step, Edit, Index);
        WouldCreate += Edit == TEXT("create_node") || Edit == TEXT("create_reroute_node") ? 1 : 0;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("dryRun"), true);
    Result->SetNumberField(TEXT("checked"), Steps.Num());
    if (Problems.Num() > 0)
    {
        const TSharedPtr<FJsonObject> First = Problems[0]->AsObject();
        const int32 FirstIndex = static_cast<int32>(First->GetNumberField(TEXT("index")));
        Result->SetNumberField(TEXT("failedIndex"), FirstIndex);
        Result->SetArrayField(TEXT("problems"), Problems);
        Context.SendErrorWithDetails(FString::Printf(TEXT("build_graph dryRun: %d step(s) would fail, first operations[%d]: %s "
            "Nothing ran."), Problems.Num(), FirstIndex, *First->GetStringField(TEXT("error"))),
            First->GetStringField(TEXT("errorCode")), Result);
        return true;
    }
    Result->SetNumberField(TEXT("wouldCreate"), WouldCreate);
    Context.SendResponse(FString::Printf(TEXT("build_graph dryRun: all %d steps pass the checks a run makes before it "
        "starts (members, functions, variables, expressions, $id references, nodes named by guid); the pins of the "
        "nodes it would create are checked only when it runs. Nothing ran."), Steps.Num()), Result);
    return true;
}
} // namespace McpBlueprintGraphHandlers::GraphBatch
