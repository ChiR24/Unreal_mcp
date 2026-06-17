#pragma once

#include "CoreMinimal.h"

namespace McpConsolidatedActions
{
inline const TArray<FString>& ManageBlueprintCore()
{
	static const TArray<FString> Actions = {
		TEXT("create"), TEXT("create_blueprint"), TEXT("get_blueprint"),
		TEXT("get"), TEXT("compile"), TEXT("add_component"),
		TEXT("set_default"), TEXT("modify_scs"), TEXT("get_scs"),
		TEXT("add_scs_component"),
		TEXT("remove_scs_component"), TEXT("reparent_scs_component"),
		TEXT("set_scs_transform"), TEXT("set_scs_property"),
		TEXT("ensure_exists"), TEXT("probe_handle"), TEXT("add_variable"),
		TEXT("remove_variable"), TEXT("rename_variable"), TEXT("add_function"),
		TEXT("remove_function"),
		TEXT("add_event"), TEXT("remove_event"),
		TEXT("add_construction_script"), TEXT("set_variable_metadata"),
		TEXT("set_metadata"), TEXT("create_node"), TEXT("add_node"),
		TEXT("delete_node"), TEXT("connect_pins"), TEXT("break_pin_links"),
		TEXT("set_node_property"), TEXT("create_reroute_node"),
		TEXT("get_node_details"), TEXT("get_graph_details"),
		TEXT("get_pin_details"), TEXT("list_node_types"),
		TEXT("set_pin_default_value")
	};
	return Actions;
}

inline const TArray<FString>& WidgetAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_widget_blueprint"), TEXT("set_widget_parent_class"),
		TEXT("add_canvas_panel"), TEXT("add_horizontal_box"),
		TEXT("add_vertical_box"), TEXT("add_overlay"), TEXT("add_grid_panel"),
		TEXT("add_uniform_grid"), TEXT("add_wrap_box"), TEXT("add_scroll_box"),
		TEXT("add_size_box"), TEXT("add_scale_box"), TEXT("add_border"),
		TEXT("add_text_block"), TEXT("add_rich_text_block"), TEXT("add_image"),
		TEXT("add_button"), TEXT("add_check_box"), TEXT("add_slider"),
		TEXT("add_progress_bar"), TEXT("add_text_input"),
		TEXT("add_combo_box"), TEXT("add_spin_box"), TEXT("add_list_view"),
		TEXT("add_tree_view"), TEXT("set_anchor"), TEXT("set_alignment"),
		TEXT("set_position"), TEXT("set_size"), TEXT("set_padding"),
		TEXT("set_z_order"), TEXT("set_render_transform"),
		TEXT("set_visibility"), TEXT("set_style"), TEXT("set_clipping"),
		TEXT("create_property_binding"), TEXT("bind_text"),
		TEXT("bind_visibility"), TEXT("bind_color"), TEXT("bind_enabled"),
		TEXT("bind_on_clicked"), TEXT("bind_on_hovered"),
		TEXT("bind_on_value_changed"), TEXT("create_widget_animation"),
		TEXT("add_animation_track"), TEXT("add_animation_keyframe"),
		TEXT("set_animation_loop"), TEXT("create_main_menu"),
		TEXT("create_pause_menu"), TEXT("create_settings_menu"),
		TEXT("create_loading_screen"), TEXT("create_hud_widget"),
		TEXT("add_health_bar"), TEXT("add_ammo_counter"),
		TEXT("add_minimap"), TEXT("add_crosshair"), TEXT("add_compass"),
		TEXT("add_interaction_prompt"), TEXT("add_objective_tracker"),
		TEXT("add_damage_indicator"), TEXT("create_inventory_ui"),
		TEXT("create_dialog_widget"), TEXT("create_radial_menu"),
		TEXT("get_widget_info"), TEXT("preview_widget")
	};
	return Actions;
}

inline TArray<FString> ManageBlueprint()
{
	TArray<FString> Actions = ManageBlueprintCore();
	AppendUniqueActions(Actions, WidgetAuthoring());
	return Actions;
}
}
