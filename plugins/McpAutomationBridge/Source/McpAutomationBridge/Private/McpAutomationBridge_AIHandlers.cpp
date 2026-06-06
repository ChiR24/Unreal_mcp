#include "McpAutomationBridge_AIHandlerContext.h"
#include "MCP/McpConsolidatedActionRouting.h"

DEFINE_LOG_CATEGORY(LogMcpAIHandlers);

static bool IsManageAIBehaviorTreeGraphSubAction(const FString& SubAction)
{
    return McpConsolidatedActions::IsBehaviorTreeAction(SubAction);
}

static bool IsManageAINavigationSubAction(const FString& SubAction)
{
    return McpConsolidatedActions::IsNavigationAction(SubAction);
}

bool UMcpAutomationBridgeSubsystem::HandleManageAIAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_ai"))
    {
        return false;
    }

#if !WITH_EDITOR
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("AI management is only available in editor builds"),
                        TEXT("EDITOR_ONLY"));
    return true;
#else
    FString SubAction = GetStringFieldAI(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Missing subAction parameter"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    if (IsManageAIBehaviorTreeGraphSubAction(SubAction))
    {
        return HandleBehaviorTreeAction(RequestId, TEXT("manage_behavior_tree"), Payload, RequestingSocket);
    }

    if (IsManageAINavigationSubAction(SubAction))
    {
        return HandleManageNavigationAction(RequestId, TEXT("manage_navigation"), Payload, RequestingSocket);
    }

    if (SubAction == TEXT("create_ai_controller"))
    {
        return McpAIHandlers::HandleCreateAIController(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("assign_behavior_tree"))
    {
        return McpAIHandlers::HandleAssignBehaviorTree(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("assign_blackboard"))
    {
        return McpAIHandlers::HandleAssignBlackboard(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("create_blackboard_asset"))
    {
        return McpAIHandlers::HandleCreateBlackboardAsset(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_blackboard_key"))
    {
        return McpAIHandlers::HandleAddBlackboardKey(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("set_key_instance_synced"))
    {
        return McpAIHandlers::HandleSetKeyInstanceSynced(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("create_behavior_tree"))
    {
        return McpAIHandlers::HandleCreateBehaviorTree(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_composite_node"))
    {
        return McpAIHandlers::HandleAddCompositeNode(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_task_node"))
    {
        return McpAIHandlers::HandleAddTaskNode(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_decorator"))
    {
        return McpAIHandlers::HandleAddDecorator(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_service"))
    {
        return McpAIHandlers::HandleAddService(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("configure_bt_node"))
    {
        return McpAIHandlers::HandleConfigureBehaviorTreeNode(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("create_eqs_query"))
    {
        return McpAIHandlers::HandleCreateEQSQuery(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_eqs_generator"))
    {
        return McpAIHandlers::HandleAddEQSGenerator(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_eqs_context"))
    {
        return McpAIHandlers::HandleAddEQSContext(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_eqs_test"))
    {
        return McpAIHandlers::HandleAddEQSTest(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("configure_test_scoring"))
    {
        return McpAIHandlers::HandleConfigureEQSTestScoring(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_ai_perception_component"))
    {
        return McpAIHandlers::HandleAddAIPerceptionComponent(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("configure_sight_config"))
    {
        return McpAIHandlers::HandleConfigureSightConfig(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("configure_hearing_config"))
    {
        return McpAIHandlers::HandleConfigureHearingConfig(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("configure_damage_sense_config"))
    {
        return McpAIHandlers::HandleConfigureDamageSenseConfig(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("set_perception_team"))
    {
        return McpAIHandlers::HandleSetPerceptionTeam(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("create_state_tree"))
    {
        return McpAIHandlers::HandleCreateStateTree(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_state_tree_state"))
    {
        return McpAIHandlers::HandleAddStateTreeState(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_state_tree_transition"))
    {
        return McpAIHandlers::HandleAddStateTreeTransition(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("configure_state_tree_task"))
    {
        return McpAIHandlers::HandleConfigureStateTreeTask(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("create_smart_object_definition"))
    {
        return McpAIHandlers::HandleCreateSmartObjectDefinition(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_smart_object_slot"))
    {
        return McpAIHandlers::HandleAddSmartObjectSlot(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("configure_slot_behavior"))
    {
        return McpAIHandlers::HandleConfigureSmartObjectSlotBehavior(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_smart_object_component"))
    {
        return McpAIHandlers::HandleAddSmartObjectComponent(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("create_mass_entity_config"))
    {
        return McpAIHandlers::HandleCreateMassEntityConfig(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("configure_mass_entity"))
    {
        return McpAIHandlers::HandleConfigureMassEntity(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("add_mass_spawner"))
    {
        return McpAIHandlers::HandleAddMassSpawner(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("get_ai_info"))
    {
        return McpAIHandlers::HandleGetAIInfo(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("set_ai_perception"))
    {
        return McpAIHandlers::HandleSetAIPerception(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("create_nav_modifier"))
    {
        return McpAIHandlers::HandleCreateNavModifier(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("set_ai_movement"))
    {
        return McpAIHandlers::HandleSetAIMovement(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("create_blackboard"))
    {
        return McpAIHandlers::HandleCreateBlackboard(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("setup_perception"))
    {
        return McpAIHandlers::HandleSetupPerception(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("create_nav_link_proxy"))
    {
        return McpAIHandlers::HandleCreateNavLinkProxy(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("set_focus"))
    {
        return McpAIHandlers::HandleSetFocus(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("clear_focus"))
    {
        return McpAIHandlers::HandleClearFocus(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("set_blackboard_value"))
    {
        return McpAIHandlers::HandleSetBlackboardValue(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("get_blackboard_value"))
    {
        return McpAIHandlers::HandleGetBlackboardValue(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("run_behavior_tree"))
    {
        return McpAIHandlers::HandleRunBehaviorTree(this, RequestId, Payload, RequestingSocket);
    }

    if (SubAction == TEXT("stop_behavior_tree"))
    {
        return McpAIHandlers::HandleStopBehaviorTree(this, RequestId, Payload, RequestingSocket);
    }

    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Unknown AI action: %s"), *SubAction),
                        TEXT("UNKNOWN_ACTION"));
    return true;
#endif
}
