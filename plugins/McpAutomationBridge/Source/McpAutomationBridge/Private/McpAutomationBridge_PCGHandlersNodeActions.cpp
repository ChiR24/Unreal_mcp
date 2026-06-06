#include "McpAutomationBridge_PCGHandlersPrivate.h"

#if WITH_EDITOR && MCP_HAS_PCG
namespace McpPCGHandlers
{
bool HandleAddPCGNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath)
{
    FString NodeType = GetFirstStringField(Payload, {TEXT("settingsClass"), TEXT("nodeType")});
    if (NodeType.IsEmpty())
    {
        NodeType = SubAction;
    }
    UClass* SettingsClass = ResolvePCGSettingsClass(NodeType);
    if (!SettingsClass)
    {
        Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Could not resolve PCG settings class '%s'."), *NodeType), TEXT("CLASS_NOT_FOUND"));
        return true;
    }

    TSubclassOf<UPCGSettings> SettingsSubclass;
    SettingsSubclass = SettingsClass;
    UPCGSettings* DefaultSettings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(SettingsSubclass, DefaultSettings);
    if (!Node || !DefaultSettings)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to add PCG node."), TEXT("CREATE_FAILED"));
        return true;
    }

    FString Error;
    const TSharedPtr<FJsonObject>* SettingsObject = nullptr;
    int32 AppliedSettings = 0;
    if (Payload->TryGetObjectField(TEXT("settings"), SettingsObject) && SettingsObject && SettingsObject->IsValid())
    {
        if (!ApplySettingsObject(DefaultSettings, *SettingsObject, Error, AppliedSettings))
        {
            Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SETTINGS"));
            return true;
        }
    }

    int32 AppliedConvenienceSettings = 0;
    if (!ApplyPCGConvenienceSettings(SubAction, DefaultSettings, Payload, Error, AppliedConvenienceSettings))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SETTINGS"));
        return true;
    }

    ApplyNodeMetadata(Node, Payload);
    Node->UpdateAfterSettingsChangeDuringCreation();
    DefaultSettings->PostEditChange();

    bool bSaved = false;
    if (!SaveGraphIfRequested(Graph, bSave, bSaved, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = BuildNodeResult(Graph, Node, GraphPath);
    Result->SetNumberField(TEXT("settingsApplied"), AppliedSettings + AppliedConvenienceSettings);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG node added."), Result);
    return true;
}

bool HandleConnectPCGPins(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath)
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

    FString Error;
    const FString SourcePinLabel = GetFirstStringField(Payload, {TEXT("sourcePin"), TEXT("outputName")});
    const FString TargetPinLabel = GetFirstStringField(Payload, {TEXT("targetPin"), TEXT("inputName")});
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

bool HandleSetPCGNodeSettings(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath)
{
    const FString NodeId = GetFirstStringField(Payload, {TEXT("nodeId"), TEXT("nodeName")});
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

    FString Error;
    int32 AppliedSettings = 0;
    const TSharedPtr<FJsonObject>* SettingsObject = nullptr;
    if (Payload->TryGetObjectField(TEXT("settings"), SettingsObject) && SettingsObject && SettingsObject->IsValid())
    {
        if (!ApplySettingsObject(Settings, *SettingsObject, Error, AppliedSettings))
        {
            Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SETTINGS"));
            return true;
        }
    }

    int32 AppliedConvenienceSettings = 0;
    if (!ApplyPCGConvenienceSettings(SubAction, Settings, Payload, Error, AppliedConvenienceSettings))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SETTINGS"));
        return true;
    }

    ApplyNodeMetadata(Node, Payload);
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
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG node settings updated."), Result);
    return true;
}
}
#endif
