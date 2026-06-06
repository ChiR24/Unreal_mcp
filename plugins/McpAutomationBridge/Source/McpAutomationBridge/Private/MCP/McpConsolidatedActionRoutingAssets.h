#pragma once

#include "CoreMinimal.h"

namespace McpConsolidatedActions
{
inline const TArray<FString>& ManageAssetCore()
{
	static const TArray<FString> Actions = {
		TEXT("list"), TEXT("import"), TEXT("duplicate"), TEXT("duplicate_asset"),
		TEXT("rename"), TEXT("rename_asset"), TEXT("move"), TEXT("move_asset"),
		TEXT("delete"), TEXT("delete_asset"), TEXT("delete_assets"),
		TEXT("create_folder"), TEXT("search_assets"), TEXT("get_dependencies"),
		TEXT("get_source_control_state"), TEXT("analyze_graph"),
		TEXT("get_asset_graph"), TEXT("create_thumbnail"), TEXT("set_tags"),
		TEXT("get_metadata"), TEXT("set_metadata"), TEXT("validate"),
		TEXT("fixup_redirectors"), TEXT("find_by_tag"), TEXT("generate_report"),
		TEXT("create_material"), TEXT("create_material_instance"),
		TEXT("create_render_target"), TEXT("generate_lods"),
		TEXT("add_material_parameter"), TEXT("list_instances"),
		TEXT("reset_instance_parameters"), TEXT("exists"),
		TEXT("get_material_stats"), TEXT("nanite_rebuild_mesh"),
		TEXT("bulk_rename"), TEXT("bulk_delete"),
		TEXT("source_control_checkout"), TEXT("source_control_submit"),
		TEXT("add_material_node"), TEXT("connect_material_pins"),
		TEXT("remove_material_node"), TEXT("break_material_connections"),
		TEXT("get_material_node_details"), TEXT("rebuild_material")
	};
	return Actions;
}

inline const TArray<FString>& MaterialAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_material"), TEXT("set_blend_mode"),
		TEXT("set_shading_model"), TEXT("set_material_domain"),
		TEXT("add_texture_sample"), TEXT("add_texture_coordinate"),
		TEXT("add_scalar_parameter"), TEXT("add_vector_parameter"),
		TEXT("add_static_switch_parameter"), TEXT("add_math_node"),
		TEXT("add_world_position"), TEXT("add_vertex_normal"),
		TEXT("add_pixel_depth"), TEXT("add_fresnel"),
		TEXT("add_reflection_vector"), TEXT("add_panner"), TEXT("add_rotator"),
		TEXT("add_noise"), TEXT("add_voronoi"), TEXT("add_if"),
		TEXT("add_switch"), TEXT("add_custom_expression"),
		TEXT("connect_nodes"), TEXT("connect_material_pins"),
		TEXT("disconnect_nodes"), TEXT("break_material_connections"),
		TEXT("create_material_function"), TEXT("add_function_input"),
		TEXT("add_function_output"), TEXT("use_material_function"),
		TEXT("get_material_function_info"), TEXT("create_material_instance"),
		TEXT("set_scalar_parameter_value"), TEXT("set_vector_parameter_value"),
		TEXT("set_texture_parameter_value"), TEXT("create_landscape_material"),
		TEXT("create_decal_material"), TEXT("create_post_process_material"),
		TEXT("add_landscape_layer"), TEXT("configure_layer_blend"),
		TEXT("compile_material"), TEXT("get_material_info"), TEXT("find_node"),
		TEXT("get_node_connections"), TEXT("get_node_properties"),
		TEXT("set_static_switch_parameter_value"), TEXT("delete_node"),
		TEXT("update_custom_expression"), TEXT("get_node_chain"),
		TEXT("get_connected_subgraph"), TEXT("add_material_node"),
		TEXT("rebuild_material"), TEXT("set_material_parameter"),
		TEXT("get_material_node_details"), TEXT("remove_material_node"),
		TEXT("set_two_sided")
	};
	return Actions;
}

inline const TArray<FString>& Texture()
{
	static const TArray<FString> Actions = {
		TEXT("create_noise_texture"), TEXT("create_gradient_texture"),
		TEXT("create_pattern_texture"), TEXT("create_normal_from_height"),
		TEXT("create_ao_from_mesh"), TEXT("resize_texture"),
		TEXT("adjust_levels"), TEXT("adjust_curves"), TEXT("blur"),
		TEXT("sharpen"), TEXT("invert"), TEXT("desaturate"),
		TEXT("channel_pack"), TEXT("channel_extract"), TEXT("combine_textures"),
		TEXT("set_compression_settings"), TEXT("set_texture_group"),
		TEXT("set_lod_bias"), TEXT("configure_virtual_texture"),
		TEXT("set_streaming_priority"), TEXT("get_texture_info")
	};
	return Actions;
}

inline TArray<FString> ManageAsset()
{
	TArray<FString> Actions = ManageAssetCore();
	AppendUniqueActions(Actions, MaterialAuthoring());
	AppendUniqueActions(Actions, Texture());
	return Actions;
}
}
