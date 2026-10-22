#include "Domains/PCG/McpPCGPrivate.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Domains/PCG/Editor/McpPCGEditor.h"
#include "Domains/PCG/Editing/McpPCGMutations.h"
#include "Domains/PCG/Parameters/McpPCGParameters.h"
#include "Domains/PCG/Execution/McpPCGExecution.h"
#include "Domains/PCG/Data/McpPCGDataActions.h"

bool UEngineWireSubsystem::HandleManagePCGAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (Action != TEXT("manage_pcg"))
    {
        return false;
    }

#if !MCP_HAS_PCG
    SendAutomationError(Socket, RequestId, TEXT("PCG plugin support is not available in this build. Enable the 'Procedural Content Generation Framework (PCG)' plugin (Edit > Plugins), restart the editor, then rebuild this automation plugin so PCG support is compiled in."), TEXT("PCG_PLUGIN_NOT_AVAILABLE"));
    return true;
#else
    if (!Payload.IsValid())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(Payload);
    if (SubAction == TEXT("get_pcg_node_schema") && !FModuleManager::Get().IsModuleLoaded(TEXT("PCG")))
    {
        SendAutomationError(Socket, RequestId, TEXT("Node-instance schema requires already-loaded PCG objects; no module, asset or class load was attempted."), TEXT("PCG_SCHEMA_UNAVAILABLE"));
        return true;
    }
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("PCG")) &&
        (!FModuleManager::Get().ModuleExists(TEXT("PCG")) || !FModuleManager::Get().LoadModulePtr<IModuleInterface>(TEXT("PCG"))))
    {
        SendAutomationError(Socket, RequestId, TEXT("PCG plugin is not enabled in this project. Enable the 'Procedural Content Generation Framework (PCG)' plugin (Edit > Plugins) and restart the editor."), TEXT("PCG_PLUGIN_NOT_ENABLED"));
        return true;
    }

    if (SubAction.IsEmpty() || !McpConsolidatedActions::PCG().Contains(SubAction))
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown PCG subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
        return true;
    }

    if (McpPCGHandlers::IsPCGDataAction(SubAction))
    {
#if MCP_HAS_PCG_EXECUTION
        if (!PCGDataViews.IsValid()) PCGDataViews = MakeShared<McpPCGHandlers::FPCGDataViewStore>();
        return McpPCGHandlers::HandlePCGData(this, RequestId, SubAction, Payload, Socket, *PCGDataViews);
#else
        SendAutomationError(Socket, RequestId, TEXT("Typed PCG data views require UE 5.7+ PCG APIs; no conversion or generation was attempted."), TEXT("PCG_DATA_NOT_AVAILABLE"));
        return true;
#endif
    }
    if (McpPCGHandlers::HandlePCGDiscovery(this, RequestId, SubAction, Payload, Socket)) return true;
    if (McpPCGHandlers::HandlePCGExecution(this, RequestId, SubAction, Payload, Socket)) return true;

    if (SubAction != TEXT("get_pcg_parameters") && GEditor && GEditor->IsTransactionActive())
    {
        SendAutomationError(Socket, RequestId, TEXT("Finish the caller's editor transaction before mutating PCG state."), TEXT("EDITOR_BUSY"));
        return true;
    }
    const bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);
    if (McpPCGHandlers::HandlePCGParameters(this, RequestId, SubAction, Payload, Socket, bSave)) return true;

    if (SubAction == TEXT("create_pcg_graph"))
    {
        return McpPCGHandlers::HandleCreatePCGGraph(this, RequestId, Payload, Socket, bSave);
    }

    if (SubAction == TEXT("create_pcg_subgraph"))
    {
        return McpPCGHandlers::HandleCreatePCGSubgraph(this, RequestId, Payload, Socket, bSave);
    }

    if (SubAction == TEXT("execute_pcg_graph"))
    {
        return McpPCGHandlers::HandleExecutePCGGraph(this, RequestId, Payload, Socket, bSave);
    }

    if (SubAction == TEXT("set_pcg_partition_grid_size"))
    {
        return McpPCGHandlers::HandleSetPCGPartitionGridSize(this, RequestId, Payload, Socket, bSave);
    }

    FString GraphRawPath = McpGetFirstStringField(Payload, {TEXT("graphPath"), TEXT("assetPath")});
    if (GraphRawPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'graphPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString GraphPath;
    FString Error;
    UPCGGraph* Graph = McpPCGHandlers::LoadPCGAsset<UPCGGraph>(GraphRawPath, TEXT("PCG graph"), GraphPath, Error);
    if (!Graph)
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    if (SubAction == TEXT("move_pcg_node") || SubAction == TEXT("disconnect_pcg_pins") || SubAction == TEXT("remove_pcg_node") ||
        SubAction == TEXT("batch_edit_pcg_graph") || SubAction == TEXT("batch_edit_pcg_graph_destructive"))
    {
        return McpPCGHandlers::HandlePCGEditOperations(this, RequestId, SubAction, Payload, Socket, bSave, Graph, GraphPath);
    }

    if (SubAction == TEXT("sync_pcg_editor_graph"))
    {
        return McpPCGHandlers::HandleSyncPCGEditor(this, RequestId, Payload, Socket, bSave, Graph, GraphPath);
    }

    if (SubAction == TEXT("add_pcg_node") || McpPCGHandlers::IsPCGNodeCreationAction(SubAction))
    {
        return McpPCGHandlers::HandleAddPCGNode(this, RequestId, SubAction, Payload, Socket, bSave, Graph, GraphPath);
    }

    if (SubAction == TEXT("connect_pcg_pins"))
    {
        return McpPCGHandlers::HandleConnectPCGPins(this, RequestId, Payload, Socket, bSave, Graph, GraphPath);
    }

    if (SubAction == TEXT("set_pcg_node_settings"))
    {
        return McpPCGHandlers::HandleSetPCGNodeSettings(this, RequestId, SubAction, Payload, Socket, bSave, Graph, GraphPath);
    }

    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unhandled PCG subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
    return true;
#endif
}
