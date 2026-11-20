// manage_networking audit_replication: the replication mistakes a read of a Blueprint finds before a multiplayer test
// does, each with the node to fix. rpc_not_replicated: an RPC on an actor that does not replicate, so a Server or
// Multicast call never leaves the machine it is made on. reliable_on_tick: Event Tick reaches a reliable RPC, every
// frame, which fills the reliable buffer and drops the client. polled_on_tick: Event Tick reads a replicated variable
// with no RepNotify, polling every frame for a change a RepNotify would report once. Reads only.

#include "Domains/Networking/McpNetPrivate.h"

#include "Domains/BlueprintGraph/Lint/McpBPLintFlow.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"

namespace McpNetAuditDetail
{
struct FFinding
{
    const TCHAR* Rule;
    const UEdGraphNode* Node;
    FString Detail;
};

// The RPC flags a custom event or a function entry carries; 0 for any other node.
uint32 RpcFlags(const UEdGraphNode* Node)
{
    if (const UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node))
    {
        return Event->FunctionFlags & FUNC_Net ? Event->FunctionFlags : 0;
    }
    const UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node);
    const uint32 Flags = Entry ? static_cast<uint32>(Entry->GetExtraFlags()) : 0;
    return Flags & FUNC_Net ? Flags : 0;
}

// Every node Event Tick runs, and the pure nodes feeding them.
TSet<const UEdGraphNode*> TickRuns(const UEdGraph* Graph)
{
    static const FName TickEvent(TEXT("ReceiveTick"));
    TArray<const UEdGraphNode*> Ticks;
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        const UK2Node_Event* Event = Cast<UK2Node_Event>(Node);
        if (Event && Event->EventReference.GetMemberName() == TickEvent)
        {
            Ticks.Add(Event);
        }
    }
    TSet<const UEdGraphNode*> Runs = McpBPLint::Flood(Ticks);
    for (const UEdGraphNode* Ran : Runs.Array())
    {
        for (const UEdGraphPin* Pin : Ran->Pins)
        {
            if (McpBPLint::IsExecPin(Pin) || Pin->Direction != EGPD_Input)
            {
                continue;
            }
            for (const UEdGraphPin* Linked : Pin->LinkedTo)
            {
                Runs.Add(Linked->GetOwningNode());
            }
        }
    }
    return Runs;
}

TArray<FFinding> Audit(const UBlueprint* Blueprint, bool bReplicates, int32& OutRpcs)
{
    TArray<FFinding> Findings;
    TSet<FName> Polled; // replicated variables with no RepNotify
    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if ((Variable.PropertyFlags & CPF_Net) && Variable.RepNotifyFunc.IsNone())
        {
            Polled.Add(Variable.VarName);
        }
    }
    OutRpcs = 0;
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (const UEdGraph* Graph : Graphs)
    {
        if (!Graph || Blueprint->MacroGraphs.Contains(Graph) || Blueprint->DelegateSignatureGraphs.Contains(Graph))
        {
            continue;
        }
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && RpcFlags(Node))
            {
                ++OutRpcs;
                if (!bReplicates)
                {
                    Findings.Add({TEXT("rpc_not_replicated"), Node, TEXT("This RPC is on an actor that does not replicate, so a Server or Multicast call runs only where it is made. Make the actor replicate (manage_networking configure_replication, setting net_role, role ROLE_SimulatedProxy) or make it a plain event.")});
                }
            }
        }
        for (const UEdGraphNode* Ran : TickRuns(Graph))
        {
            const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Ran);
            const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
            const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Ran);
            if (Function && Function->HasAllFunctionFlags(FUNC_Net | FUNC_NetReliable))
            {
                Findings.Add({TEXT("reliable_on_tick"), Ran, FString::Printf(TEXT("Event Tick calls the reliable RPC %s every frame: reliable calls are resent until they arrive, so the buffer fills and the client is dropped. Make it unreliable (manage_networking configure_rpc, setting reliability), or call it only when the value changes."), *Function->GetName())});
            }
            else if (Get && Get->VariableReference.IsSelfContext() && Polled.Contains(Get->GetVarName()))
            {
                Findings.Add({TEXT("polled_on_tick"), Ran, FString::Printf(TEXT("Event Tick reads the replicated variable %s every frame to see whether it changed: give it a RepNotify (manage_networking configure_replication, setting rep_notify) and react there, once per change."), *Get->GetVarName().ToString())});
            }
        }
    }
    return Findings;
}
} // namespace McpNetAuditDetail

namespace McpNetworkingHandlers
{
bool HandleAuditReplication(FNetworkingActionContext& Context)
{
    using namespace McpNetAuditDetail;
    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, GetJsonStringField(Context.Payload, TEXT("blueprintPath")));
    if (!Blueprint)
    {
        return true;
    }
    const bool bReplicates = McpBlueprintReplicates(Blueprint);
    int32 Rpcs = 0;
    const TArray<FFinding> Findings = Audit(Blueprint, bReplicates, Rpcs);
    const int32 Cap = FMath::Clamp(GetJsonIntField(Context.Payload, TEXT("maxFindings"), 50), 1, 200);
    TArray<TSharedPtr<FJsonValue>> Rows;
    TSharedPtr<FJsonObject> Counts = MakeShared<FJsonObject>();
    for (const FFinding& Finding : Findings)
    {
        Counts->SetNumberField(Finding.Rule, Counts->HasField(Finding.Rule) ? Counts->GetNumberField(Finding.Rule) + 1 : 1);
        if (Rows.Num() < Cap)
        {
            TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("rule"), Finding.Rule);
            Row->SetStringField(TEXT("graph"), GetNameSafe(Finding.Node->GetGraph()));
            Row->SetStringField(TEXT("node"), Finding.Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
            Row->SetStringField(TEXT("nodeId"), Finding.Node->NodeGuid.ToString());
            Row->SetStringField(TEXT("detail"), Finding.Detail);
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    Context.ResultJson->SetArrayField(TEXT("findings"), Rows);
    Context.ResultJson->SetObjectField(TEXT("counts"), Counts);
    Context.ResultJson->SetNumberField(TEXT("findingCount"), Findings.Num());
    Context.ResultJson->SetBoolField(TEXT("replicates"), bReplicates);
    Context.ResultJson->SetNumberField(TEXT("rpcCount"), Rpcs);
    Context.ResultJson->SetStringField(TEXT("blueprintPath"), Blueprint->GetOutermost()->GetName());
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, Findings.Num() == 0
        ? FString::Printf(TEXT("No replication findings in %s (%d RPCs; replicates: %s)."), *Blueprint->GetName(), Rpcs, bReplicates ? TEXT("yes") : TEXT("no"))
        : FString::Printf(TEXT("%d replication finding(s) in %s."), Findings.Num(), *Blueprint->GetName()), Context.ResultJson);
    return true;
}
} // namespace McpNetworkingHandlers
