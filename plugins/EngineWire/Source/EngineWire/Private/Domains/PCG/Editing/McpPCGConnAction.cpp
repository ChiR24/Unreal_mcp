#include "Domains/PCG/Editing/McpPCGConnect.h"
#include "Domains/PCG/Editing/McpPCGEditState.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Domains/PCG/Editor/McpPCGEditor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
namespace
{
UEdGraphPin* FindEditorPin(UEdGraph* EditorGraph, UPCGNode* Model, FName Label, EEdGraphPinDirection Direction)
{
    for (UEdGraphNode* Node : EditorGraph->Nodes)
    {
        if (!Node) continue;
        const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Node->GetClass(), TEXT("PCGNode"));
        if (!Property || Property->GetObjectPropertyValue_InContainer(Node) != Model) continue;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->PinName == Label && Pin->Direction == Direction) return Pin;
        }
    }
    return nullptr;
}
}

bool HandleConnectPCGPins(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath)
{
    const FString SourceNodeId = GetJsonStringField(Payload, TEXT("sourceNodeId"));
    const FString TargetNodeId = GetJsonStringField(Payload, TEXT("targetNodeId"));
    if (SourceNodeId.IsEmpty() || TargetNodeId.IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("connect_pcg_pins requires sourceNodeId and targetNodeId."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    UPCGNode* SourceNode = FindPCGNode(Graph, SourceNodeId);
    UPCGNode* TargetNode = FindPCGNode(Graph, TargetNodeId);
    if (!SourceNode || !TargetNode)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not resolve source or target PCG node."), TEXT("NODE_NOT_FOUND"));
        return true;
    }
    FString Error, Code;
    if (!CheckPCGGraphRevision(Graph, Payload, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("STALE_STATE"));
        return true;
    }
    FName SourcePin, TargetPin;
    if (!TryResolvePCGPinLabel(SourceNode, true, McpGetFirstStringField(Payload, {TEXT("sourcePin"), TEXT("outputName")}), SourcePin, Error) ||
        !TryResolvePCGPinLabel(TargetNode, false, McpGetFirstStringField(Payload, {TEXT("targetPin"), TEXT("inputName")}), TargetPin, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("PIN_NOT_FOUND"));
        return true;
    }
    UPCGPin* Source = SourceNode->GetOutputPin(SourcePin);
    UPCGPin* Target = TargetNode->GetInputPin(TargetPin);
    if (!ValidatePCGConnection(Source, Target, Error, Code))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, Code);
        return true;
    }
    bool bAmbiguous = false;
    UEdGraph* EditorGraph = FindExistingPCGEditorGraph(Graph, bAmbiguous);
    UEdGraphPin* EditorSource = EditorGraph ? FindEditorPin(EditorGraph, SourceNode, SourcePin, EGPD_Output) : nullptr;
    UEdGraphPin* EditorTarget = EditorGraph ? FindEditorPin(EditorGraph, TargetNode, TargetPin, EGPD_Input) : nullptr;
    if (bAmbiguous || (EditorGraph && (!EditorSource || !EditorTarget)))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("The existing editor graph does not match the model graph."), TEXT("EDITOR_GRAPH_MISMATCH"));
        return true;
    }
    if (EditorSource && EditorTarget)
    {
        const FPinConnectionResponse Response = EditorGraph->GetSchema()->CanCreateConnection(EditorSource, EditorTarget);
        if (Response.Response != CONNECT_RESPONSE_MAKE && !HasPCGEdge(Source, Target))
        {
            Bridge->SendAutomationError(Socket, RequestId, Response.Message.ToString(), TEXT("NATIVE_SCHEMA_REFUSED"));
            return true;
        }
    }
    FPCGEditState State(Graph);
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Connect PCG Pins")),
        bSave ? EMcpMutationDurability::DurablePackageWrite : EMcpMutationDurability::EditorStateOnly, State.GetObjects());
    if (!HasPCGEdge(Source, Target))
    {
        if (EditorSource && EditorTarget) EditorGraph->GetSchema()->TryCreateConnection(EditorSource, EditorTarget);
        else Graph->AddLabeledEdge(SourceNode, SourcePin, TargetNode, TargetPin);
    }
    auto Result = McpHandlerUtils::CreateResultObject();
    TSharedPtr<FJsonObject> EditorState;
    const bool bConnected = HasPCGEdge(Source, Target);
    const bool bSynchronized = bConnected && SyncPCGEditorGraph(Graph, EditorState, Error);
    if (!bConnected || !bSynchronized)
    {
        const bool bRestored = State.Restore();
        Result->SetBoolField(TEXT("committed"), false);
        Result->SetBoolField(TEXT("rolledBack"), bRestored);
        Result->SetBoolField(TEXT("saved"), false);
        Bridge->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to connect PCG pins."), Result,
            bRestored ? TEXT("CONNECT_FAILED") : TEXT("ROLLBACK_FAILED"));
        return true;
    }
    Result->SetObjectField(TEXT("editorGraph"), EditorState);
    Result->SetStringField(TEXT("graphPath"), GraphPath);
    Result->SetStringField(TEXT("sourceNodeId"), SourceNode->GetName());
    Result->SetStringField(TEXT("targetNodeId"), TargetNode->GetName());
    Result->SetStringField(TEXT("sourcePin"), SourcePin.ToString());
    Result->SetStringField(TEXT("targetPin"), TargetPin.ToString());
    Result->SetBoolField(TEXT("committed"), true);
    Result->SetBoolField(TEXT("rolledBack"), false);
    Result->SetStringField(TEXT("revision"), GetPCGGraphRevision(Graph));
    Transaction.DescribeInto(Result);
    bool bSaved = false;
    const bool bSuccess = SaveGraphIfRequested(Graph, bSave, bSaved, Error);
    Result->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(Result, Graph);
    Bridge->SendAutomationResponse(Socket, RequestId, bSuccess, bSuccess ? TEXT("PCG pins connected.") : Error, Result,
        bSuccess ? FString() : TEXT("SAVE_FAILED"));
    return true;
}
}
#endif
