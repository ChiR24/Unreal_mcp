#include "Domains/PCG/McpPCGPrivate.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
namespace
{
// The graph's node names, which FindPCGNode takes, for a refusal that found none.
FString DescribePCGGraphNodeNames(UPCGGraph* Graph)
{
    TArray<FString> Names;
    for (const UPCGNode* Node : Graph->GetNodes())
    {
        if (Node)
        {
            Names.Add(Node->GetName());
        }
    }
    Names.Add(TEXT("input"));
    Names.Add(TEXT("output"));
    return FString::Join(Names, TEXT(", "));
}

// Applies the payload's settings object, convenience settings and metadata to Node, saves when asked and replies
// with the node result. bEdit: an existing node, found by nodeName rather than titled by it.
bool ConfigurePCGNode(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph,
    const FString& GraphPath, UPCGNode* Node, UPCGSettings* Settings, const TCHAR* Message, bool bEdit)
{
    FString Error;
    int32 AppliedSettings = 0;
    // A node this call added goes again with a refused setting: it stayed in the graph, unsaved and unannounced.
    const auto RefuseSettings = [&]()
    {
        if (!bEdit)
        {
            Graph->RemoveNode(Node);
        }
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SETTINGS"));
        return true;
    };
    const TSharedPtr<FJsonObject>* SettingsObject = nullptr;
    if (Payload->TryGetObjectField(TEXT("settings"), SettingsObject) && SettingsObject && SettingsObject->IsValid())
    {
        if (!ApplySettingsObject(Settings, *SettingsObject, Error, AppliedSettings))
        {
            return RefuseSettings();
        }
    }

    int32 AppliedConvenienceSettings = 0;
    if (!ApplyPCGConvenienceSettings(SubAction, Settings, Payload, Error, AppliedConvenienceSettings))
    {
        return RefuseSettings();
    }

    // An edit that changed nothing (or sent only a meshPath or class the node has no slot for) answered "updated".
    const bool bMoves = Payload->HasField(TEXT("x")) || Payload->HasField(TEXT("y"));
    if (bEdit && AppliedSettings + AppliedConvenienceSettings == 0 && !bMoves && !Payload->HasField(TEXT("title")))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Nothing to change: send settings (property values), title, x and y, "
            "texturePath, meshPath (a node with a mesh) or actorClass (an actor spawner). Nothing was changed."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    ApplyNodeMetadata(Node, Payload, !bEdit);
    Node->UpdateAfterSettingsChangeDuringCreation();
    Settings->PostEditChange();

    bool bSaved = false;
    if (!SaveGraphIfRequested(Graph, bSave, bSaved, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = BuildNodeResult(Graph, Node, GraphPath);
    Result->SetNumberField(TEXT("settingsApplied"), AppliedSettings + AppliedConvenienceSettings);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Bridge->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}
}

bool HandleAddPCGNode(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath)
{
    // The generic add_pcg_node names its class (settingsClass, else nodeType; it requires one); a typed kind is its class.
    const FString NodeType = SubAction == TEXT("add_pcg_node")
        ? McpGetFirstStringField(Payload, {TEXT("settingsClass"), TEXT("nodeType")}) : SubAction;
    UClass* SettingsClass = ResolvePCGSettingsClass(NodeType);
    if (!SettingsClass)
    {
        Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Could not resolve PCG settings class '%s'."), *NodeType), TEXT("CLASS_NOT_FOUND"));
        return true;
    }

    UPCGSettings* DefaultSettings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(SettingsClass, DefaultSettings);
    if (!Node || !DefaultSettings)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to add PCG node."), TEXT("CREATE_FAILED"));
        return true;
    }

    return ConfigurePCGNode(Bridge, RequestId, SubAction, Payload, Socket, bSave, Graph, GraphPath, Node, DefaultSettings,
        TEXT("PCG node added."), false);
}

bool HandleConnectPCGPins(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath)
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
        const FString Missing = !SourceNode && !TargetNode
            ? FString::Printf(TEXT("source '%s' and target '%s'"), *SourceNodeId, *TargetNodeId)
            : !SourceNode ? FString::Printf(TEXT("source '%s'"), *SourceNodeId) : FString::Printf(TEXT("target '%s'"), *TargetNodeId);
        Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No PCG node matches %s. The graph's nodes: %s."), *Missing,
            *DescribePCGGraphNodeNames(Graph)), TEXT("NODE_NOT_FOUND"));
        return true;
    }

    FString Error;
    const FString SourcePinLabel = McpGetFirstStringField(Payload, {TEXT("sourcePin"), TEXT("outputName")});
    const FString TargetPinLabel = McpGetFirstStringField(Payload, {TEXT("targetPin"), TEXT("inputName")});
    FName SourcePin;
    FName TargetPin;
    if (!TryResolvePCGPinLabel(SourceNode, true, SourcePinLabel, SourcePin, Error) ||
        !TryResolvePCGPinLabel(TargetNode, false, TargetPinLabel, TargetPin, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("PIN_NOT_FOUND"));
        return true;
    }

    UPCGPin* SourcePinObject = SourceNode->GetOutputPin(SourcePin);
    UPCGPin* TargetPinObject = TargetNode->GetInputPin(TargetPin);
    if (!HasPCGEdge(SourcePinObject, TargetPinObject))
    {
        Graph->AddLabeledEdge(SourceNode, SourcePin, TargetNode, TargetPin);
    }

    if (!HasPCGEdge(SourcePinObject, TargetPinObject))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to connect PCG pins."), TEXT("CONNECT_FAILED"));
        return true;
    }

    bool bSaved = false;
    if (!SaveGraphIfRequested(Graph, bSave, bSaved, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("graphPath"), GraphPath);
    Result->SetStringField(TEXT("sourceNodeId"), SourceNode->GetName());
    Result->SetStringField(TEXT("targetNodeId"), TargetNode->GetName());
    Result->SetStringField(TEXT("sourcePin"), SourcePin.ToString());
    Result->SetStringField(TEXT("targetPin"), TargetPin.ToString());
    Result->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(Result, Graph);
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG pins connected."), Result);
    return true;
}

bool HandleSetPCGNodeSettings(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath)
{
    const FString NodeId = McpGetFirstStringField(Payload, {TEXT("nodeId"), TEXT("nodeName")});
    UPCGNode* Node = FindPCGNode(Graph, NodeId);
    if (!Node)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not resolve PCG node."), TEXT("NODE_NOT_FOUND"));
        return true;
    }

    UPCGSettings* Settings = Node->GetSettings();
    if (!Settings)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("PCG node has no editable settings."), TEXT("SETTINGS_NOT_FOUND"));
        return true;
    }

    return ConfigurePCGNode(Bridge, RequestId, SubAction, Payload, Socket, bSave, Graph, GraphPath, Node, Settings,
        TEXT("PCG node settings updated."), true);
}
}
#endif
