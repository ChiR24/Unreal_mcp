#pragma once

#include "CoreMinimal.h"

namespace McpConsolidatedActions
{
inline const TArray<FString>& WidgetAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_widget_blueprint"), TEXT("set_widget_parent_class"),
		TEXT("add_canvas_panel"), TEXT("add_horizontal_box"),
		TEXT("add_vertical_box"), TEXT("add_overlay"),
		TEXT("add_grid_panel"), TEXT("add_uniform_grid"),
		TEXT("add_wrap_box"), TEXT("add_scroll_box"), TEXT("add_size_box"),
		TEXT("add_scale_box"), TEXT("add_border"), TEXT("add_text_block"),
		TEXT("add_rich_text_block"), TEXT("add_image"), TEXT("add_button"),
		TEXT("add_check_box"), TEXT("add_slider"), TEXT("add_progress_bar"),
		TEXT("add_text_input"), TEXT("add_combo_box"), TEXT("add_spin_box"),
		TEXT("add_list_view"), TEXT("add_tree_view"), TEXT("set_anchor"),
		TEXT("set_alignment"), TEXT("set_position"), TEXT("set_size"),
		TEXT("set_padding"), TEXT("set_z_order"),
		TEXT("set_render_transform"), TEXT("set_visibility"),
		TEXT("set_style"), TEXT("set_clipping"),
		TEXT("bind_text"),
		TEXT("bind_visibility"), TEXT("bind_color"), TEXT("bind_enabled"),
		TEXT("bind_on_clicked"), TEXT("bind_on_hovered"),
		TEXT("bind_on_value_changed"), TEXT("create_widget_animation"),
		TEXT("add_animation_track"), TEXT("add_animation_keyframe"),
		TEXT("get_widget_info"),
		TEXT("preview_widget"), TEXT("add_safe_zone"), TEXT("add_spacer"),
		TEXT("add_widget_component"), TEXT("add_widget_switcher"),
		TEXT("bind_localized_text"),
		TEXT("delete_animation"), TEXT("get_widget_slot_info"),
		TEXT("remove_widget"), TEXT("rename_widget"),
		TEXT("reparent_widget"), TEXT("set_font"),
		TEXT("set_localization_key"), TEXT("set_margin")
	};
	return Actions;
}
}
