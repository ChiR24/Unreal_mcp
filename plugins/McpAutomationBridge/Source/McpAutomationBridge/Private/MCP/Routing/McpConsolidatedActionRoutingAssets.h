#pragma once

#include "CoreMinimal.h"

namespace McpConsolidatedActions
{
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
		TEXT("set_two_sided"), TEXT("set_node_position")
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

} // namespace McpConsolidatedActions
