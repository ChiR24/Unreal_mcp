#include "McpAutomationBridge_NavigationHandlersPrivate.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpNavigationHandlers, Log, All);

bool UMcpAutomationBridgeSubsystem::HandleManageNavigationAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    (void)Action;

#if WITH_EDITOR
    const FString SubAction = McpNavigationHandlers::GetJsonStringFieldNav(Payload, TEXT("subAction"), TEXT(""));

    UE_LOG(LogMcpNavigationHandlers, Verbose, TEXT("HandleManageNavigationAction: SubAction=%s"), *SubAction);

    if (SubAction == TEXT("configure_nav_mesh_settings"))
        return McpNavigationHandlers::HandleConfigureNavMeshSettings(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_nav_agent_properties"))
        return McpNavigationHandlers::HandleSetNavAgentProperties(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("rebuild_navigation"))
        return McpNavigationHandlers::HandleRebuildNavigation(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_nav_modifier_component"))
        return McpNavigationHandlers::HandleCreateNavModifierComponent(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_nav_area_class"))
        return McpNavigationHandlers::HandleSetNavAreaClass(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("configure_nav_area_cost"))
        return McpNavigationHandlers::HandleConfigureNavAreaCost(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_nav_link_proxy"))
        return McpNavigationHandlers::HandleCreateNavLinkProxy(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("configure_nav_link"))
        return McpNavigationHandlers::HandleConfigureNavLink(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_nav_link_type"))
        return McpNavigationHandlers::HandleSetNavLinkType(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("create_smart_link"))
        return McpNavigationHandlers::HandleCreateSmartLink(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("configure_smart_link_behavior"))
        return McpNavigationHandlers::HandleConfigureSmartLinkBehavior(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("get_navigation_info"))
        return McpNavigationHandlers::HandleGetNavigationInfo(this, RequestId, Payload, Socket);

    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown navigation subAction: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("Navigation operations require editor build"), nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
