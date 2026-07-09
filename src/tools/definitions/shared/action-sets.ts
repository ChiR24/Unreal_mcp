export const CINEMATICS_ACTIONS = [
  'create_master_sequence', 'add_subsequence', 'add_shot_track', 'configure_shot_settings',
  'create_cine_camera_actor', 'configure_camera_settings', 'add_camera_cut_track',
  'add_camera_shake_track', 'configure_camera_rig_rail', 'configure_camera_rig_crane',
  'add_fade_track', 'add_level_visibility_track', 'add_material_parameter_track',
  'add_particle_track', 'add_skeletal_animation_track', 'add_transform_track',
  'add_event_track', 'add_property_track'
] as const;

export const MOVIE_RENDER_ACTIONS = [
  'create_render_job', 'configure_output_settings', 'add_render_pass',
  'configure_anti_aliasing', 'configure_console_variables', 'configure_burn_ins',
  'queue_render', 'start_render'
] as const;

export const MEDIA_ACTIONS = [
  'create_media_player', 'create_media_source', 'create_media_texture',
  'create_media_sound_component', 'create_media_playlist', 'play_media',
  'pause_media', 'seek_media'
] as const;

export const RECORD_REPLAY_ACTIONS = [
  'create_take_recorder_panel', 'configure_take_sources', 'start_recording',
  'stop_recording', 'configure_recorded_tracks', 'start_demo_recording',
  'stop_demo_recording', 'configure_demo_settings', 'play_demo', 'pause_demo',
  'seek_demo', 'set_demo_playback_speed', 'configure_killcam_duration',
  'start_killcam'
] as const;

export const MATERIAL_AUTHORING_ACTIONS = [
  'create_material', 'set_blend_mode', 'set_shading_model', 'set_material_domain',
  'add_texture_sample', 'add_texture_coordinate', 'add_scalar_parameter', 'add_vector_parameter',
  'add_static_switch_parameter', 'add_math_node', 'add_world_position', 'add_vertex_normal',
  'add_pixel_depth', 'add_fresnel', 'add_reflection_vector', 'add_panner', 'add_rotator',
  'add_noise', 'add_voronoi', 'add_if', 'add_switch', 'add_custom_expression',
  'connect_nodes', 'connect_material_pins', 'disconnect_nodes', 'break_material_connections',
  'create_material_function', 'add_function_input', 'add_function_output', 'use_material_function',
  'get_material_function_info',
  'create_material_instance', 'set_scalar_parameter_value', 'set_vector_parameter_value', 'set_texture_parameter_value',
  'create_landscape_material', 'create_decal_material', 'create_post_process_material',
  'add_landscape_layer', 'configure_layer_blend',
  'compile_material', 'get_material_info',
  'find_node', 'get_node_connections', 'get_node_properties', 'set_static_switch_parameter_value',
  'delete_node', 'update_custom_expression',
  'get_node_chain', 'get_connected_subgraph',
  'add_material_node', 'rebuild_material', 'set_material_parameter',
  'get_material_node_details', 'remove_material_node',
  'set_two_sided',
] as const;

export const TEXTURE_ACTIONS = [
  'create_noise_texture', 'create_gradient_texture', 'create_pattern_texture',
  'create_normal_from_height', 'create_ao_from_mesh',
  'resize_texture', 'adjust_levels', 'adjust_curves', 'blur', 'sharpen',
  'invert', 'desaturate', 'channel_pack', 'channel_extract', 'combine_textures',
  'set_compression_settings', 'set_texture_group', 'set_lod_bias',
  'configure_virtual_texture', 'set_streaming_priority', 'get_texture_info'
] as const;

export const SKELETON_ACTIONS = [
  'create_skeleton', 'add_bone', 'remove_bone', 'rename_bone',
  'set_bone_transform', 'set_bone_parent', 'create_virtual_bone',
  'create_socket', 'configure_socket', 'auto_skin_weights', 'set_vertex_weights',
  'normalize_weights', 'prune_weights', 'copy_weights', 'mirror_weights',
  'create_physics_asset', 'add_physics_body', 'configure_physics_body',
  'add_physics_constraint', 'configure_constraint_limits',
  'bind_cloth_to_skeletal_mesh', 'assign_cloth_asset_to_mesh',
  'create_morph_target', 'set_morph_target_deltas', 'import_morph_targets',
  'get_skeleton_info', 'list_bones', 'list_sockets', 'list_physics_bodies'
] as const;

export const LIGHTING_ACTIONS = [
  'spawn_light', 'create_light', 'spawn_sky_light', 'create_sky_light', 'ensure_single_sky_light',
  'create_lightmass_volume', 'create_lighting_enabled_level', 'create_dynamic_light',
  'setup_global_illumination', 'configure_shadows', 'set_exposure', 'set_ambient_occlusion',
  'setup_volumetric_fog', 'build_lighting', 'list_light_types'
] as const;

export const SPLINE_ACTIONS = [
  'create_spline_actor', 'add_spline_point', 'remove_spline_point', 'set_spline_point_position',
  'set_spline_point_tangents', 'set_spline_point_rotation', 'set_spline_point_scale', 'set_spline_type',
  'create_spline_mesh_component', 'set_spline_mesh_asset', 'configure_spline_mesh_axis',
  'set_spline_mesh_material', 'scatter_meshes_along_spline', 'configure_mesh_spacing',
  'configure_mesh_randomization', 'create_road_spline', 'create_river_spline', 'create_fence_spline',
  'create_wall_spline', 'create_cable_spline', 'create_pipe_spline', 'get_splines_info'
] as const;

export const ENVIRONMENT_ACTIONS = [
  'import_heightmap', 'export_heightmap', 'create_landscape_layer_info',
  'configure_landscape_material', 'configure_landscape_splines', 'configure_landscape_lod',
  'create_landscape_streaming_proxy', 'create_foliage_type', 'configure_foliage_mesh',
  'configure_foliage_placement', 'configure_foliage_lod', 'configure_foliage_collision',
  'configure_foliage_culling', 'paint_foliage_instances', 'remove_foliage_instances',
  'configure_sky_atmosphere', 'configure_sky_light', 'configure_directional_light_atmosphere',
  'configure_exponential_height_fog', 'configure_volumetric_cloud', 'create_weather_system',
  'configure_rain_particles', 'configure_snow_particles', 'configure_wind', 'configure_lightning',
  'create_time_of_day_system', 'configure_sun_position', 'configure_light_color_curve',
  'configure_sky_color_curve', 'create_water_body_ocean', 'create_water_body_lake',
  'create_water_body_river', 'create_water_body_custom', 'configure_water_waves',
  'configure_water_material', 'configure_water_collision', 'create_buoyancy_component'
] as const;

// RENDER_ACTIONS routes the public `build_environment` actions through the
// internal `manage_render` bridge action. It also includes a small set of
// lighting-quality actions (set_light_channel, set_actor_light_channel,
// configure_lightmass_settings, build_lighting_quality,
// configure_indirect_lighting_cache) because the C++ handlers for those
// actions live in `McpAutomationBridge_RenderLighting.cpp`; moving them to
// LIGHTING_ACTIONS would require relocating the C++ side as well. The two
// action sets are intentionally disjoint, so a given action only ever has
// one routing path through `build_environment`.
export const RENDER_ACTIONS = [
  'configure_ray_traced_shadows', 'configure_ray_traced_gi', 'configure_ray_traced_reflections',
  'configure_ray_traced_ao', 'configure_path_tracing', 'set_light_channel', 'set_actor_light_channel',
  'configure_lightmass_settings', 'build_lighting_quality', 'configure_indirect_lighting_cache',
  'create_sphere_reflection_capture', 'create_box_reflection_capture', 'configure_reflection_capture_resolution', 'configure_capture_resolution',
  'configure_capture_offset', 'recapture_scene', 'create_planar_reflection', 'configure_planar_reflection',
  'configure_ssr_settings', 'configure_lumen_reflection_settings', 'configure_pp_blend',
  'set_pp_white_balance', 'set_pp_color_grading', 'set_pp_lut', 'configure_tonemapper',
  'set_tonemapper_type', 'configure_bloom', 'set_bloom_intensity', 'set_bloom_threshold',
  'configure_lens_flare', 'configure_dof', 'set_dof_method', 'set_focal_distance', 'set_aperture',
  'configure_bokeh', 'configure_motion_blur', 'set_motion_blur_amount', 'set_motion_blur_max',
  'configure_exposure', 'set_exposure_method', 'set_exposure_compensation', 'set_exposure_min_max',
  'configure_ssao', 'configure_gtao', 'configure_vignette', 'configure_chromatic_aberration',
  'configure_grain', 'configure_screen_percentage', 'create_scene_capture_2d', 'create_scene_capture_cube',
  'configure_capture_source', 'assign_render_target', 'capture_scene'
] as const;

export const PERFORMANCE_ACTIONS = [
  'start_profiling', 'stop_profiling', 'run_benchmark', 'show_stats', 'generate_memory_report',
  'set_scalability', 'set_resolution_scale', 'set_vsync', 'set_frame_rate_limit', 'enable_gpu_timing',
  'configure_texture_streaming', 'configure_lod', 'apply_baseline_settings', 'optimize_draw_calls',
  'merge_actors', 'configure_occlusion_culling', 'optimize_shaders', 'configure_nanite',
  'configure_world_partition'
] as const;

export const BEHAVIOR_TREE_ACTIONS = [
  'create', 'add_node', 'connect_nodes', 'remove_node', 'break_connections',
  'set_node_properties', 'add_subnode', 'get_tree'
] as const;

export const NAVIGATION_ACTIONS = [
  'configure_nav_mesh_settings', 'set_nav_agent_properties', 'rebuild_navigation',
  'create_nav_modifier_component', 'set_nav_area_class', 'configure_nav_area_cost',
  'create_nav_link_proxy', 'configure_nav_link', 'set_nav_link_type', 'create_smart_link',
  'configure_smart_link_behavior', 'get_navigation_info'
] as const;

export const WIDGET_AUTHORING_ACTIONS = [
  'create_widget_blueprint', 'set_widget_parent_class', 'add_canvas_panel', 'add_horizontal_box',
  'add_vertical_box', 'add_overlay', 'add_grid_panel', 'add_uniform_grid', 'add_wrap_box',
  'add_scroll_box', 'add_size_box', 'add_scale_box', 'add_border', 'add_text_block',
  'add_rich_text_block', 'add_image', 'add_button', 'add_check_box', 'add_slider', 'add_progress_bar',
  'add_text_input', 'add_combo_box', 'add_spin_box', 'add_list_view', 'add_tree_view',
  'set_anchor', 'set_alignment', 'set_position', 'set_size', 'set_padding', 'set_z_order',
  'set_render_transform', 'set_visibility', 'set_style', 'set_clipping', 'create_property_binding',
  'bind_text', 'bind_visibility', 'bind_color', 'bind_enabled', 'bind_on_clicked', 'bind_on_hovered',
  'bind_on_value_changed', 'create_widget_animation', 'add_animation_track', 'add_animation_keyframe',
  'set_animation_loop', 'create_main_menu', 'create_pause_menu', 'create_settings_menu',
  'create_loading_screen', 'create_hud_widget', 'add_health_bar', 'add_ammo_counter', 'add_minimap',
  'add_crosshair', 'add_compass', 'add_interaction_prompt', 'add_objective_tracker',
  'add_damage_indicator', 'create_inventory_ui', 'create_dialog_widget', 'create_radial_menu',
  'get_widget_info', 'preview_widget'
] as const;

export const SESSION_ACTIONS = [
  'configure_local_session_settings', 'configure_session_interface', 'configure_split_screen',
  'set_split_screen_type', 'add_local_player', 'remove_local_player', 'configure_lan_play',
  'host_lan_server', 'join_lan_server', 'enable_voice_chat', 'configure_voice_settings',
  'set_voice_channel', 'mute_player', 'set_voice_attenuation', 'configure_push_to_talk',
  'get_sessions_info'
] as const;

export const GAME_FRAMEWORK_ACTIONS = [
  'create_game_mode', 'create_game_state', 'create_player_controller',
  'create_player_state', 'create_game_instance', 'create_hud_class',
  'set_default_pawn_class', 'set_player_controller_class',
  'set_game_state_class', 'set_player_state_class', 'configure_game_rules',
  'setup_match_states', 'configure_round_system', 'configure_team_system',
  'configure_scoring_system', 'configure_spawn_system',
  'configure_player_start', 'set_respawn_rules', 'configure_spectating',
  'get_game_framework_info'
] as const;

export const INPUT_ACTIONS = [
  'create_input_action', 'create_input_mapping_context', 'add_mapping', 'remove_mapping',
  'add_legacy_action_mapping', 'remove_legacy_action_mapping',
  'add_legacy_axis_mapping', 'remove_legacy_axis_mapping',
  'map_input_action', 'set_input_trigger', 'set_input_modifier', 'enable_input_mapping',
  'disable_input_action', 'get_input_info'
] as const;

export const VOLUME_ACTIONS = [
  'create_trigger_volume', 'add_trigger_volume', 'create_trigger_box', 'create_trigger_sphere',
  'create_trigger_capsule', 'create_blocking_volume', 'add_blocking_volume', 'create_kill_z_volume',
  'add_kill_z_volume', 'create_pain_causing_volume', 'create_physics_volume', 'add_physics_volume',
  'create_audio_volume', 'create_reverb_volume', 'create_cull_distance_volume', 'add_cull_distance_volume',
  'create_precomputed_visibility_volume', 'create_lightmass_importance_volume', 'create_nav_mesh_bounds_volume',
  'create_nav_modifier_volume', 'create_camera_blocking_volume', 'create_post_process_volume',
  'add_post_process_volume', 'set_volume_extent', 'set_volume_bounds', 'set_volume_properties',
  'remove_volume', 'get_volumes_info'
] as const;

export const PCG_ACTIONS = [
  'create_pcg_graph', 'create_pcg_subgraph', 'add_pcg_node', 'connect_pcg_pins', 'set_pcg_node_settings',
  'add_landscape_data_node', 'add_spline_data_node', 'add_volume_data_node', 'add_actor_data_node', 'add_texture_data_node',
  'add_surface_sampler', 'add_mesh_sampler', 'add_spline_sampler', 'add_volume_sampler',
  'add_bounds_modifier', 'add_density_filter', 'add_height_filter', 'add_slope_filter', 'add_distance_filter', 'add_bounds_filter',
  'add_self_pruning', 'add_transform_points', 'add_project_to_surface', 'add_copy_points', 'add_merge_points',
  'add_static_mesh_spawner', 'add_actor_spawner', 'add_spline_spawner',
  'execute_pcg_graph', 'set_pcg_partition_grid_size'
] as const;
