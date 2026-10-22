#include "Domains/PCG/McpPCGPrivate.h"
#include "Domains/PCG/Editing/McpPCGEditState.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Domains/PCG/Editor/McpPCGEditor.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
namespace
{
bool ReplyPCGEditFailure(UEngineWireSubsystem* Bridge, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, FPCGEditState& State, const FString& Error, const TCHAR* Code)
{
    const bool bRestored = State.Restore();
    auto Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("committed"), false);
    Result->SetBoolField(TEXT("saved"), false);
    Result->SetBoolField(TEXT("rolledBack"), bRestored);
    Result->SetBoolField(TEXT("residualChanges"), !bRestored);
    Bridge->SendAutomationResponse(Socket, RequestId, false, Error, Result, bRestored ? Code : TEXT("ROLLBACK_FAILED"));
    return true;
}

bool ConfigurePCGNode(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph,
    const FString& GraphPath, UPCGNode* Node, UPCGSettings* Settings, const TCHAR* Message,
    FPCGEditState& State, FMcpScopedEditorTransaction& Transaction, bool bEdit)
{
    FString Error;
    int32 AppliedSettings = 0;
    if (!ApplyPCGSettingsPayload(Settings, SubAction, Payload, Error, AppliedSettings))
    {
        return ReplyPCGEditFailure(Bridge, RequestId, Socket, State, Error, TEXT("INVALID_SETTINGS"));
    }
    const bool bMoves = Payload->HasField(TEXT("x")) || Payload->HasField(TEXT("y")) || Payload->HasField(TEXT("posX")) || Payload->HasField(TEXT("posY"));
    if (bEdit && AppliedSettings == 0 && !bMoves && !Payload->HasField(TEXT("title")))
    {
        return ReplyPCGEditFailure(Bridge, RequestId, Socket, State,
            TEXT("Nothing to change: send settings (property values), title, x and y, texturePath, meshPath (a node with a mesh) or actorClass (an actor spawner). Nothing was changed."),
            TEXT("INVALID_ARGUMENT"));
    }
    ApplyNodeMetadata(Node, Payload, !bEdit);
    Node->UpdateAfterSettingsChangeDuringCreation();
    Settings->PostEditChange();
    if (GetPCGGraphRevision(Graph).IsEmpty())
        return ReplyPCGEditFailure(Bridge, RequestId, Socket, State, TEXT("Edited state exceeds the authored read budget or has invalid dependencies."), TEXT("PCG_SNAPSHOT_UNAVAILABLE"));
    TSharedPtr<FJsonObject> EditorState;
    if (!SyncPCGEditorGraph(Graph, EditorState, Error))
    {
        return ReplyPCGEditFailure(Bridge, RequestId, Socket, State, Error, TEXT("EDITOR_SYNC_FAILED"));
    }
    auto Result = BuildNodeResult(Graph, Node, GraphPath);
    Result->SetObjectField(TEXT("editorGraph"), EditorState);
    Result->SetNumberField(TEXT("settingsApplied"), AppliedSettings);
    Result->SetBoolField(TEXT("committed"), true);
    Result->SetBoolField(TEXT("rolledBack"), false);
    Result->SetStringField(TEXT("revision"), GetPCGGraphRevision(Graph));
    Transaction.DescribeInto(Result);
    bool bSaved = false;
    if (!SaveGraphIfRequested(Graph, bSave, bSaved, Error))
    {
        Result->SetBoolField(TEXT("saved"), false);
        Bridge->SendAutomationResponse(Socket, RequestId, false, Error, Result, TEXT("SAVE_FAILED"));
        return true;
    }
    Result->SetBoolField(TEXT("saved"), bSaved);
    Bridge->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}
}

bool HandleAddPCGNode(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath)
{
    FString NodeType = McpGetFirstStringField(Payload, {TEXT("settingsClass"), TEXT("nodeType")});
    if (NodeType.IsEmpty()) NodeType = SubAction;
    UClass* SettingsClass = ResolvePCGSettingsClass(NodeType);
    if (!SettingsClass)
    {
        Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Could not resolve PCG settings class '%s'."), *NodeType), TEXT("CLASS_NOT_FOUND"));
        return true;
    }
    FString Error;
    if (!CheckPCGGraphRevision(Graph, Payload, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("STALE_STATE"));
        return true;
    }
    if (!PreparePCGSettings(SettingsClass->GetDefaultObject<UPCGSettings>(), SubAction, Payload, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SETTINGS"));
        return true;
    }
    FPCGEditState State(Graph);
    if (State.GetRevisionBefore().IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not snapshot the graph before editing."), TEXT("SNAPSHOT_FAILED"));
        return true;
    }
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Add PCG Node")),
        bSave ? EMcpMutationDurability::DurablePackageWrite : EMcpMutationDurability::EditorStateOnly, State.GetObjects());
    TSubclassOf<UPCGSettings> SettingsSubclass = SettingsClass;
    UPCGSettings* DefaultSettings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(SettingsSubclass, DefaultSettings);
    if (!Node || !DefaultSettings)
    {
        return ReplyPCGEditFailure(Bridge, RequestId, Socket, State, TEXT("Failed to add PCG node."), TEXT("CREATE_FAILED"));
    }
    return ConfigurePCGNode(Bridge, RequestId, SubAction, Payload, Socket, bSave, Graph, GraphPath, Node, DefaultSettings,
        TEXT("PCG node added."), State, Transaction, false);
}

bool HandleSetPCGNodeSettings(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath)
{
    const FString NodeId = McpGetFirstStringField(Payload, {TEXT("nodeId"), TEXT("nodeName")});
    UPCGNode* Node = FindPCGNode(Graph, NodeId);
    if (!Node)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not resolve PCG node."), TEXT("NODE_NOT_FOUND"));
        return true;
    }
    UPCGSettings* Settings = Node->GetSettings();
    if (!HasOwnedPCGNodeSettings(Graph, Node))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("PCG node has no privately owned editable settings."), TEXT("SETTINGS_NOT_FOUND"));
        return true;
    }
    FString Error;
    if (!CheckPCGGraphRevision(Graph, Payload, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("STALE_STATE"));
        return true;
    }
    if (!ValidatePCGNodeSettings(Graph, Node, SubAction, Payload, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_SETTINGS"));
        return true;
    }
    FPCGEditState State(Graph);
    if (State.GetRevisionBefore().IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not snapshot the graph before editing."), TEXT("SNAPSHOT_FAILED"));
        return true;
    }
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Edit PCG Settings")),
        bSave ? EMcpMutationDurability::DurablePackageWrite : EMcpMutationDurability::EditorStateOnly, State.GetObjects());
    return ConfigurePCGNode(Bridge, RequestId, SubAction, Payload, Socket, bSave, Graph, GraphPath, Node, Settings,
        TEXT("PCG node settings updated."), State, Transaction, true);
}
}
#endif
