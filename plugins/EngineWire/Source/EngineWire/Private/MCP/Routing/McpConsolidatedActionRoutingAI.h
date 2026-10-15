#pragma once

#include "CoreMinimal.h"

namespace McpConsolidatedActions
{
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
}
