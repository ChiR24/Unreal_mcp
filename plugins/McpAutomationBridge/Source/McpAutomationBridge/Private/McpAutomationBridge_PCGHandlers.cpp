#include "McpAutomationBridge_PCGHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleManagePCGAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (Action != TEXT("manage_pcg"))
    {
        return false;
    }

#if !WITH_EDITOR
    SendAutomationError(Socket, RequestId, TEXT("manage_pcg requires an editor build."), TEXT("EDITOR_ONLY"));
    return true;
#elif !MCP_HAS_PCG
    SendAutomationError(Socket, RequestId, TEXT("PCG plugin support is not available in this build."), TEXT("PCG_PLUGIN_NOT_AVAILABLE"));
    return true;
#else
    if (!Payload.IsValid())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    if (!FModuleManager::Get().IsModuleLoaded(TEXT("PCG")) &&
        (!FModuleManager::Get().ModuleExists(TEXT("PCG")) || !FModuleManager::Get().LoadModulePtr<IModuleInterface>(TEXT("PCG"))))
    {
        SendAutomationError(Socket, RequestId, TEXT("PCG plugin is not enabled in this project."), TEXT("PCG_PLUGIN_NOT_ENABLED"));
        return true;
    }

    const FString SubAction = McpPCGHandlers::NormalizePCGSubAction(Payload);
    if (SubAction.IsEmpty() || !McpConsolidatedActions::IsPCGAction(SubAction))
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown PCG subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
        return true;
    }

    const bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);

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

    FString GraphRawPath = McpPCGHandlers::GetFirstStringField(Payload, {TEXT("graphPath"), TEXT("assetPath")});
    if (GraphRawPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'graphPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString GraphPath;
    FString Error;
    UPCGGraph* Graph = McpPCGHandlers::LoadPCGGraph(GraphRawPath, GraphPath, Error);
    if (!Graph)
    {
        SendAutomationError(Socket, RequestId, Error, TEXT("ASSET_NOT_FOUND"));
        return true;
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
