#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"
#include "MCP/Routing/McpConsolidatedActionRouting.h"

DEFINE_LOG_CATEGORY(LogMcpAIHandlers);

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

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Missing subAction parameter"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    if (McpConsolidatedActions::BehaviorTree().Contains(SubAction))
    {
        return HandleBehaviorTreeAction(RequestId, TEXT("manage_behavior_tree"), Payload, RequestingSocket);
    }

    if (McpConsolidatedActions::Navigation().Contains(SubAction))
    {
        return HandleManageNavigationAction(RequestId, TEXT("manage_navigation"), Payload, RequestingSocket);
    }

    using FAIHandler = bool (*)(UMcpAutomationBridgeSubsystem*, const FString&, const TSharedPtr<FJsonObject>&,
                                TSharedPtr<FMcpBridgeWebSocket>);
    static const TMap<FString, FAIHandler> Handlers = {
        {TEXT("create_ai_controller"),           &McpAIHandlers::HandleCreateAIController},
        {TEXT("assign_behavior_tree"),           &McpAIHandlers::HandleAssignBehaviorTree},
        {TEXT("assign_blackboard"),              &McpAIHandlers::HandleAssignBlackboard},
        {TEXT("create_blackboard_asset"),        &McpAIHandlers::HandleCreateBlackboardAsset},
        {TEXT("add_blackboard_key"),             &McpAIHandlers::HandleAddBlackboardKey},
        {TEXT("set_key_instance_synced"),        &McpAIHandlers::HandleSetKeyInstanceSynced},
        {TEXT("create_behavior_tree"),           &McpAIHandlers::HandleCreateBehaviorTree},
        {TEXT("add_composite_node"),             &McpAIHandlers::HandleAddCompositeNode},
        {TEXT("add_task_node"),                  &McpAIHandlers::HandleAddTaskNode},
        {TEXT("add_decorator"),                  &McpAIHandlers::HandleAddDecorator},
        {TEXT("add_service"),                    &McpAIHandlers::HandleAddService},
        {TEXT("configure_bt_node"),              &McpAIHandlers::HandleConfigureBehaviorTreeNode},
        {TEXT("create_eqs_query"),               &McpAIHandlers::HandleCreateEQSQuery},
        {TEXT("add_eqs_generator"),              &McpAIHandlers::HandleAddEQSGenerator},
        {TEXT("add_eqs_context"),                &McpAIHandlers::HandleAddEQSContext},
        {TEXT("add_eqs_test"),                   &McpAIHandlers::HandleAddEQSTest},
        {TEXT("configure_test_scoring"),         &McpAIHandlers::HandleConfigureEQSTestScoring},
        {TEXT("add_ai_perception_component"),    &McpAIHandlers::HandleAddAIPerceptionComponent},
        {TEXT("configure_sight_config"),         &McpAIHandlers::HandleConfigureSightConfig},
        {TEXT("configure_hearing_config"),       &McpAIHandlers::HandleConfigureHearingConfig},
        {TEXT("configure_damage_sense_config"),  &McpAIHandlers::HandleConfigureDamageSenseConfig},
        {TEXT("set_perception_team"),            &McpAIHandlers::HandleSetPerceptionTeam},
        {TEXT("create_state_tree"),              &McpAIHandlers::HandleCreateStateTree},
        {TEXT("add_state_tree_state"),           &McpAIHandlers::HandleAddStateTreeState},
        {TEXT("add_state_tree_transition"),      &McpAIHandlers::HandleAddStateTreeTransition},
        {TEXT("configure_state_tree_task"),      &McpAIHandlers::HandleConfigureStateTreeTask},
        {TEXT("create_smart_object_definition"), &McpAIHandlers::HandleCreateSmartObjectDefinition},
        {TEXT("add_smart_object_slot"),          &McpAIHandlers::HandleAddSmartObjectSlot},
        {TEXT("configure_slot_behavior"),        &McpAIHandlers::HandleConfigureSmartObjectSlotBehavior},
        {TEXT("add_smart_object_component"),     &McpAIHandlers::HandleAddSmartObjectComponent},
        {TEXT("create_mass_entity_config"),      &McpAIHandlers::HandleCreateMassEntityConfig},
        {TEXT("configure_mass_entity"),          &McpAIHandlers::HandleConfigureMassEntity},
        {TEXT("add_mass_spawner"),               &McpAIHandlers::HandleAddMassSpawner},
        {TEXT("get_ai_info"),                    &McpAIHandlers::HandleGetAIInfo},
        {TEXT("set_ai_perception"),              &McpAIHandlers::HandleSetAIPerception},
        {TEXT("create_nav_modifier"),            &McpAIHandlers::HandleCreateNavModifier},
        {TEXT("set_ai_movement"),                &McpAIHandlers::HandleSetAIMovement},
        {TEXT("create_blackboard"),              &McpAIHandlers::HandleCreateBlackboard},
        {TEXT("setup_perception"),               &McpAIHandlers::HandleSetupPerception},
        {TEXT("set_focus"),                      &McpAIHandlers::HandleSetFocus},
        {TEXT("clear_focus"),                    &McpAIHandlers::HandleClearFocus},
        {TEXT("set_blackboard_value"),           &McpAIHandlers::HandleSetBlackboardValue},
        {TEXT("get_blackboard_value"),           &McpAIHandlers::HandleGetBlackboardValue},
        {TEXT("run_behavior_tree"),              &McpAIHandlers::HandleRunBehaviorTree},
        {TEXT("stop_behavior_tree"),             &McpAIHandlers::HandleStopBehaviorTree},
    };
    if (const FAIHandler* Handler = Handlers.Find(SubAction))
    {
        return (*Handler)(this, RequestId, Payload, RequestingSocket);
    }

    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Unknown AI action: %s"), *SubAction),
                        TEXT("UNKNOWN_ACTION"));
    return true;
}
