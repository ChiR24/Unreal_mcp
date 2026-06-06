#pragma once

#include "CoreMinimal.h"

namespace McpConsolidatedActions
{
inline const TArray<FString>& BuildEnvironmentCore()
{
	static const TArray<FString> Actions = {
		TEXT("create_landscape"), TEXT("sculpt"), TEXT("sculpt_landscape"),
		TEXT("add_foliage"), TEXT("paint_foliage"),
		TEXT("create_procedural_terrain"),
		TEXT("create_procedural_foliage"),
		TEXT("add_foliage_instances"), TEXT("get_foliage_instances"),
		TEXT("remove_foliage"), TEXT("paint_landscape"),
		TEXT("paint_landscape_layer"), TEXT("modify_heightmap"),
		TEXT("set_landscape_material"), TEXT("create_landscape_grass_type"),
		TEXT("generate_lods"), TEXT("bake_lightmap"),
		TEXT("export_snapshot"), TEXT("import_snapshot"), TEXT("delete"),
		TEXT("create_sky_sphere"), TEXT("set_time_of_day"),
		TEXT("create_fog_volume"),
		TEXT("import_heightmap"), TEXT("export_heightmap"),
		TEXT("create_landscape_layer_info"),
		TEXT("configure_landscape_material"),
		TEXT("configure_landscape_splines"), TEXT("configure_landscape_lod"),
		TEXT("create_landscape_streaming_proxy"),
		TEXT("create_foliage_type"), TEXT("configure_foliage_mesh"),
		TEXT("configure_foliage_placement"), TEXT("configure_foliage_lod"),
		TEXT("configure_foliage_collision"), TEXT("configure_foliage_culling"),
		TEXT("paint_foliage_instances"), TEXT("remove_foliage_instances"),
		TEXT("configure_sky_atmosphere"), TEXT("configure_sky_light"),
		TEXT("configure_directional_light_atmosphere"),
		TEXT("configure_exponential_height_fog"),
		TEXT("configure_volumetric_cloud"), TEXT("create_weather_system"),
		TEXT("configure_rain_particles"), TEXT("configure_snow_particles"),
		TEXT("configure_wind"), TEXT("configure_lightning"),
		TEXT("create_time_of_day_system"), TEXT("configure_sun_position"),
		TEXT("configure_light_color_curve"), TEXT("configure_sky_color_curve"),
		TEXT("create_water_body_ocean"), TEXT("create_water_body_lake"),
		TEXT("create_water_body_river"), TEXT("create_water_body_custom"),
		TEXT("configure_water_waves"), TEXT("configure_water_material"),
		TEXT("configure_water_collision"), TEXT("create_buoyancy_component")
	};
	return Actions;
}

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

inline TArray<FString> BuildEnvironment()
{
	TArray<FString> Actions = BuildEnvironmentCore();
	AppendUniqueActions(Actions, Lighting());
	AppendUniqueActions(Actions, Splines());
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
