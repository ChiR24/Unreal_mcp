#pragma once

#include "CoreMinimal.h"

namespace McpConsolidatedActions
{
inline const TArray<FString>& ManageAICore()
{
	static const TArray<FString> Actions = {
		TEXT("create_ai_controller"), TEXT("assign_behavior_tree"),
		TEXT("assign_blackboard"), TEXT("create_blackboard_asset"),
		TEXT("add_blackboard_key"), TEXT("set_key_instance_synced"),
		TEXT("create_behavior_tree"), TEXT("add_composite_node"),
		TEXT("add_task_node"), TEXT("add_decorator"), TEXT("add_service"),
		TEXT("configure_bt_node"), TEXT("create_eqs_query"),
		TEXT("add_eqs_generator"), TEXT("add_eqs_context"),
		TEXT("add_eqs_test"), TEXT("configure_test_scoring"),
		TEXT("add_ai_perception_component"), TEXT("configure_sight_config"),
		TEXT("configure_hearing_config"),
		TEXT("configure_damage_sense_config"), TEXT("set_perception_team"),
		TEXT("create_state_tree"), TEXT("add_state_tree_state"),
		TEXT("add_state_tree_transition"), TEXT("configure_state_tree_task"),
		TEXT("create_smart_object_definition"),
		TEXT("add_smart_object_slot"), TEXT("configure_slot_behavior"),
		TEXT("add_smart_object_component"),
		TEXT("create_mass_entity_config"), TEXT("configure_mass_entity"),
		TEXT("add_mass_spawner"), TEXT("get_ai_info"),
		TEXT("create_blackboard"), TEXT("setup_perception"),
		TEXT("create_nav_link_proxy"), TEXT("set_focus"), TEXT("clear_focus"),
		TEXT("set_blackboard_value"), TEXT("get_blackboard_value"),
		TEXT("run_behavior_tree"), TEXT("stop_behavior_tree"),
		TEXT("create"), TEXT("add_node"), TEXT("connect_nodes"),
		TEXT("remove_node"), TEXT("break_connections"),
		TEXT("set_node_properties"), TEXT("configure_nav_mesh_settings"),
		TEXT("set_nav_agent_properties"), TEXT("rebuild_navigation"),
		TEXT("create_nav_modifier_component"), TEXT("set_nav_area_class"),
		TEXT("configure_nav_area_cost"), TEXT("configure_nav_link"),
		TEXT("set_nav_link_type"), TEXT("create_smart_link"),
		TEXT("configure_smart_link_behavior"), TEXT("get_navigation_info")
	};
	return Actions;
}

inline const TArray<FString>& BehaviorTree()
{
	static const TArray<FString> Actions = {
		TEXT("create"), TEXT("add_node"), TEXT("connect_nodes"),
		TEXT("remove_node"), TEXT("break_connections"),
		TEXT("set_node_properties"), TEXT("add_subnode"),
		TEXT("get_tree")
	};
	return Actions;
}

inline const TArray<FString>& Navigation()
{
	static const TArray<FString> Actions = {
		TEXT("configure_nav_mesh_settings"),
		TEXT("set_nav_agent_properties"), TEXT("rebuild_navigation"),
		TEXT("create_nav_modifier_component"), TEXT("set_nav_area_class"),
		TEXT("configure_nav_area_cost"), TEXT("create_nav_link_proxy"),
		TEXT("configure_nav_link"), TEXT("set_nav_link_type"),
		TEXT("create_smart_link"), TEXT("configure_smart_link_behavior"),
		TEXT("get_navigation_info")
	};
	return Actions;
}

inline TArray<FString> ManageAI()
{
	TArray<FString> Actions = ManageAICore();
	AppendUniqueActions(Actions, BehaviorTree());
	AppendUniqueActions(Actions, Navigation());
	return Actions;
}
}
