#include "McpAutomationBridgeSubsystem.h"

#include "MCP/McpConsolidatedActionRouting.h"

#define MCP_REGISTER_DIRECT(ActionName, MethodName) RegisterHandler(TEXT(ActionName), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return MethodName(R, A, P, S); })

void UMcpAutomationBridgeSubsystem::RegisterBlueprintAndDomainHandlers()
{
    RegisterHandler(
        TEXT("manage_blueprint"),
        [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S)
        {
            FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
            if (McpConsolidatedActions::IsWidgetAuthoringAction(SubAction))
            {
                const TSharedPtr<FJsonObject> RoutedPayload =
                    McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                return HandleManageWidgetAuthoringAction(
                    R,
                    TEXT("manage_widget_authoring"),
                    RoutedPayload,
                    S);
            }
            static const TSet<FString> GraphSubActions = {
                TEXT("create_node"), TEXT("delete_node"), TEXT("connect_pins"),
                TEXT("break_pin_links"), TEXT("set_node_property"),
                TEXT("create_reroute_node"), TEXT("get_node_details"),
                TEXT("get_graph_details"), TEXT("get_pin_details"),
                TEXT("list_node_types"), TEXT("set_pin_default_value"),
                TEXT("list_animbp_graphs"), TEXT("get_transition_rule_graph")};
            if (GraphSubActions.Contains(SubAction))
            {
                const TSharedPtr<FJsonObject> RoutedPayload =
                    McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                return HandleBlueprintGraphAction(R, A, RoutedPayload, S);
            }
            return HandleBlueprintAction(R, A, P, S);
        });

    MCP_REGISTER_DIRECT("manage_geometry", HandleGeometryAction);
    MCP_REGISTER_DIRECT("manage_skeleton", HandleManageSkeleton);
    MCP_REGISTER_DIRECT("manage_texture", HandleManageTextureAction);
    MCP_REGISTER_DIRECT("manage_gas", HandleManageGASAction);
    MCP_REGISTER_DIRECT("manage_character", HandleManageCharacterAction);
    MCP_REGISTER_DIRECT("manage_combat", HandleManageCombatAction);
    MCP_REGISTER_DIRECT("manage_ai", HandleManageAIAction);
    MCP_REGISTER_DIRECT("manage_inventory", HandleManageInventoryAction);
    MCP_REGISTER_DIRECT("manage_interaction", HandleManageInteractionAction);
    MCP_REGISTER_DIRECT("manage_widget_authoring", HandleManageWidgetAuthoringAction);
    MCP_REGISTER_DIRECT("manage_splines", HandleManageSplinesAction);
    MCP_REGISTER_DIRECT("manage_pcg", HandleManagePCGAction);
    MCP_REGISTER_DIRECT("manage_pipeline", HandlePipelineAction);
    MCP_REGISTER_DIRECT("manage_behavior_tree", HandleBehaviorTreeAction);

    RegisterHandler(
        TEXT("manage_networking"),
        [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S)
        {
            const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
            if (McpConsolidatedActions::IsInputAction(SubAction))
            {
                const TSharedPtr<FJsonObject> RoutedPayload =
                    McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                return HandleInputAction(R, TEXT("manage_input"), RoutedPayload, S);
            }
            if (McpConsolidatedActions::IsGameFrameworkAction(SubAction))
            {
                const TSharedPtr<FJsonObject> RoutedPayload =
                    McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                return HandleManageGameFrameworkAction(
                    R,
                    TEXT("manage_game_framework"),
                    RoutedPayload,
                    S);
            }
            if (McpConsolidatedActions::IsSessionAction(SubAction))
            {
                const TSharedPtr<FJsonObject> RoutedPayload =
                    McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                return HandleManageSessionsAction(R, TEXT("manage_sessions"), RoutedPayload, S);
            }
            return HandleManageNetworkingAction(R, A, P, S);
        });
}

#undef MCP_REGISTER_DIRECT
