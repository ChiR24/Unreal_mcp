#include "McpAutomationBridgeSubsystem.h"

#include "MCP/McpConsolidatedActionRouting.h"

#define MCP_REGISTER_DIRECT(ActionName, MethodName) RegisterHandler(TEXT(ActionName), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return MethodName(R, A, P, S); })

void UMcpAutomationBridgeSubsystem::RegisterSystemAndEditorHandlers()
{
    MCP_REGISTER_DIRECT("console_command", HandleConsoleCommandAction);
    MCP_REGISTER_DIRECT("batch_console_commands", HandleConsoleCommandAction);
    MCP_REGISTER_DIRECT("inspect", HandleInspectAction);
    MCP_REGISTER_DIRECT("list_blueprints", HandleListBlueprints);
    MCP_REGISTER_DIRECT("manage_world_partition", HandleWorldPartitionAction);
    MCP_REGISTER_DIRECT("manage_render", HandleRenderAction);
    MCP_REGISTER_DIRECT("manage_input", HandleInputAction);
    MCP_REGISTER_DIRECT("control_actor", HandleControlActorAction);
    MCP_REGISTER_DIRECT("manage_level", HandleLevelAction);
    MCP_REGISTER_DIRECT("manage_sequence", HandleSequenceAction);

    RegisterHandler(
        TEXT("system_control"),
        [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S)
        {
            const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
            if (McpConsolidatedActions::IsPerformanceAction(SubAction))
            {
                const TSharedPtr<FJsonObject> RoutedPayload =
                    McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                return HandlePerformanceAction(R, TEXT("manage_performance"), RoutedPayload, S);
            }
            return HandleSystemControlAction(R, A, P, S);
        });
}

#undef MCP_REGISTER_DIRECT
