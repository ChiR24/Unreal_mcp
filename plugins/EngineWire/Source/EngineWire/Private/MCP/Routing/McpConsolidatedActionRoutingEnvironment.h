#pragma once

#include "CoreMinimal.h"

namespace McpConsolidatedActions
{
inline const TArray<FString>& Lighting()
{
	static const TArray<FString> Actions = {
		TEXT("spawn_light"), TEXT("create_light"), TEXT("spawn_sky_light"),
		TEXT("create_sky_light"), TEXT("ensure_single_sky_light"),
		TEXT("create_lightmass_volume"),
		TEXT("create_lighting_enabled_level"), TEXT("create_dynamic_light"),
		TEXT("setup_global_illumination"), TEXT("configure_shadows"),
		TEXT("set_exposure"), TEXT("set_ambient_occlusion"),
		TEXT("setup_volumetric_fog"), TEXT("build_lighting"),
		TEXT("list_light_types")
	};
	return Actions;
}

inline const TArray<FString>& Splines()
{
	static const TArray<FString> Actions = {
		TEXT("create_spline_actor"), TEXT("add_spline_point"),
		TEXT("remove_spline_point"), TEXT("set_spline_point_position"),
		TEXT("set_spline_point_tangents"), TEXT("set_spline_point_rotation"),
		TEXT("set_spline_point_scale"), TEXT("set_spline_type"),
		TEXT("create_spline_mesh_component"), TEXT("set_spline_mesh_asset"),
		TEXT("configure_spline_mesh_axis"),
		TEXT("set_spline_mesh_material"),
		TEXT("scatter_meshes_along_spline"),
		TEXT("configure_mesh_spacing"),
		TEXT("configure_mesh_randomization"), TEXT("create_road_spline"),
		TEXT("create_river_spline"), TEXT("create_fence_spline"),
		TEXT("create_wall_spline"), TEXT("create_cable_spline"),
		TEXT("create_pipe_spline"), TEXT("get_splines_info")
	};
	return Actions;
}

inline const TArray<FString>& Rendering()
{
	static const TArray<FString> Actions = {
		TEXT("configure_ray_traced_shadows"),
		TEXT("configure_ray_traced_gi"),
		TEXT("configure_ray_traced_reflections"),
		TEXT("configure_ray_traced_ao"),
		TEXT("configure_path_tracing"),
		TEXT("set_light_channel"),
		TEXT("set_actor_light_channel"),
		TEXT("configure_lightmass_settings"),
		TEXT("build_lighting_quality"),
		TEXT("configure_indirect_lighting_cache"),
		TEXT("create_sphere_reflection_capture"),
		TEXT("create_box_reflection_capture"),
		TEXT("configure_reflection_capture_resolution"),
		TEXT("configure_capture_resolution"),
		TEXT("configure_capture_offset"),
		TEXT("recapture_scene"),
		TEXT("create_planar_reflection"),
		TEXT("configure_planar_reflection"),
		TEXT("configure_ssr_settings"),
		TEXT("configure_lumen_reflection_settings"),
		TEXT("configure_pp_blend"),
		TEXT("set_pp_white_balance"),
		TEXT("set_pp_color_grading"),
		TEXT("set_pp_lut"),
		TEXT("configure_tonemapper"),
		TEXT("set_tonemapper_type"),
		TEXT("configure_bloom"),
		TEXT("set_bloom_intensity"),
		TEXT("set_bloom_threshold"),
		TEXT("configure_lens_flare"),
		TEXT("configure_dof"),
		TEXT("set_dof_method"),
		TEXT("set_focal_distance"),
		TEXT("set_aperture"),
		TEXT("configure_bokeh"),
		TEXT("configure_motion_blur"),
		TEXT("set_motion_blur_amount"),
		TEXT("set_motion_blur_max"),
		TEXT("configure_exposure"),
		TEXT("set_exposure_method"),
		TEXT("set_exposure_compensation"),
		TEXT("set_exposure_min_max"),
		TEXT("configure_ssao"),
		TEXT("configure_gtao"),
		TEXT("configure_vignette"),
		TEXT("configure_chromatic_aberration"),
		TEXT("configure_grain"),
		TEXT("configure_screen_percentage"),
		TEXT("create_scene_capture_2d"),
		TEXT("create_scene_capture_cube"),
		TEXT("configure_capture_source"),
		TEXT("assign_render_target"),
		TEXT("capture_scene")
	};
	return Actions;
}

inline const TArray<FString>& PCG()
{
	static const TArray<FString> Actions = {
		TEXT("create_pcg_graph"), TEXT("create_pcg_subgraph"),
		TEXT("add_pcg_node"), TEXT("connect_pcg_pins"),
		TEXT("set_pcg_node_settings"),
		TEXT("add_landscape_data_node"), TEXT("add_spline_data_node"),
		TEXT("add_volume_data_node"), TEXT("add_actor_data_node"),
		TEXT("add_texture_data_node"), TEXT("add_surface_sampler"),
		TEXT("add_mesh_sampler"), TEXT("add_spline_sampler"),
		TEXT("add_volume_sampler"), TEXT("add_bounds_modifier"),
		TEXT("add_density_filter"), TEXT("add_height_filter"),
		TEXT("add_slope_filter"), TEXT("add_distance_filter"),
		TEXT("add_bounds_filter"), TEXT("add_self_pruning"),
		TEXT("add_transform_points"), TEXT("add_project_to_surface"),
		TEXT("add_copy_points"), TEXT("add_merge_points"),
		TEXT("add_static_mesh_spawner"), TEXT("add_actor_spawner"),
		TEXT("add_spline_spawner"), TEXT("execute_pcg_graph"),
		TEXT("set_pcg_partition_grid_size")
	};
	return Actions;
}
}
