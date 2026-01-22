# Action Reference

> Auto-generated from tool definitions. Do not edit manually.

Generated: 2026-01-22T05:56:01.721Z

Total Tools: 37

## Table of Contents

Total Actions: 2665

- [configure_tools](#configure-tools) (3 actions)
- [manage_asset](#manage-asset) (99 actions)
- [control_actor](#control-actor) (45 actions)
- [control_editor](#control-editor) (84 actions)
- [manage_level](#manage-level) (94 actions)
- [manage_motion_design](#manage-motion-design) (10 actions)
- [animation_physics](#animation-physics) (166 actions)
- [manage_effect](#manage-effect) (79 actions)
- [build_environment](#build-environment) (73 actions)
- [manage_sequence](#manage-sequence) (139 actions)
- [manage_audio](#manage-audio) (134 actions)
- [manage_lighting](#manage-lighting) (61 actions)
- [manage_performance](#manage-performance) (20 actions)
- [manage_geometry](#manage-geometry) (80 actions)
- [manage_skeleton](#manage-skeleton) (54 actions)
- [manage_material_authoring](#manage-material-authoring) (73 actions)
- [manage_character](#manage-character) (78 actions)
- [manage_combat](#manage-combat) (67 actions)
- [manage_ai](#manage-ai) (103 actions)
- [manage_widget_authoring](#manage-widget-authoring) (73 actions)
- [manage_networking](#manage-networking) (73 actions)
- [manage_volumes](#manage-volumes) (41 actions)
- [manage_data](#manage-data) (62 actions)
- [manage_build](#manage-build) (48 actions)
- [manage_editor_utilities](#manage-editor-utilities) (45 actions)
- [manage_gameplay_systems](#manage-gameplay-systems) (50 actions)
- [manage_gameplay_primitives](#manage-gameplay-primitives) (62 actions)
- [manage_character_avatar](#manage-character-avatar) (60 actions)
- [manage_asset_plugins](#manage-asset-plugins) (248 actions)
- [manage_livelink](#manage-livelink) (64 actions)
- [manage_xr](#manage-xr) (272 actions)
- [manage_accessibility](#manage-accessibility) (50 actions)
- [manage_ui](#manage-ui) (11 actions)
- [manage_gameplay_abilities](#manage-gameplay-abilities) (31 actions)
- [manage_attribute_sets](#manage-attribute-sets) (6 actions)
- [manage_gameplay_cues](#manage-gameplay-cues) (3 actions)
- [test_gameplay_abilities](#test-gameplay-abilities) (4 actions)

---

## configure_tools

**Category:** core

MCP meta-tool: filter which MCP tools are visible by category. NOT an Unreal Engine tool.

### Actions

| Action | Description |
|--------|-------------|
| `set_categories` | - |
| `list_categories` | - |
| `get_status` | - |

---

## manage_asset

**Category:** core

Assets, Materials, dependencies; Blueprints (SCS, graph nodes).

### Actions

| Action | Description |
|--------|-------------|
| `list` | - |
| `import` | - |
| `duplicate` | - |
| `rename` | - |
| `move` | - |
| `delete` | - |
| `delete_asset` | - |
| `delete_assets` | - |
| `create_folder` | - |
| `search_assets` | - |
| `get_dependencies` | - |
| `get_source_control_state` | - |
| `analyze_graph` | - |
| `get_asset_graph` | - |
| `create_thumbnail` | - |
| `set_tags` | - |
| `get_metadata` | - |
| `set_metadata` | - |
| `validate` | - |
| `fixup_redirectors` | - |
| `find_by_tag` | - |
| `generate_report` | - |
| `create_material` | - |
| `create_material_instance` | - |
| `create_render_target` | - |
| `generate_lods` | - |
| `add_material_parameter` | - |
| `list_instances` | - |
| `reset_instance_parameters` | - |
| `exists` | - |
| `get_material_stats` | - |
| `nanite_rebuild_mesh` | - |
| `enable_nanite_mesh` | - |
| `set_nanite_settings` | - |
| `batch_nanite_convert` | - |
| `add_material_node` | - |
| `connect_material_pins` | - |
| `remove_material_node` | - |
| `break_material_connections` | - |
| `get_material_node_details` | - |
| `rebuild_material` | - |
| `create_metasound` | - |
| `add_metasound_node` | - |
| `connect_metasound_nodes` | - |
| `remove_metasound_node` | - |
| `set_metasound_variable` | - |
| `create_oscillator` | - |
| `create_envelope` | - |
| `create_filter` | - |
| `create_sequencer_node` | - |
| `create_procedural_music` | - |
| `import_audio_to_metasound` | - |
| `export_metasound_preset` | - |
| `configure_audio_modulation` | - |
| `bp_create` | - |
| `bp_get` | - |
| `bp_compile` | - |
| `bp_add_component` | - |
| `bp_set_default` | - |
| `bp_modify_scs` | - |
| `bp_get_scs` | - |
| `bp_add_scs_component` | - |
| `bp_remove_scs_component` | - |
| `bp_reparent_scs_component` | - |
| `bp_set_scs_transform` | - |
| `bp_set_scs_property` | - |
| `bp_ensure_exists` | - |
| `bp_probe_handle` | - |
| `bp_add_variable` | - |
| `bp_remove_variable` | - |
| `bp_rename_variable` | - |
| `bp_add_function` | - |
| `bp_add_event` | - |
| `bp_remove_event` | - |
| `bp_add_construction_script` | - |
| `bp_set_variable_metadata` | - |
| `bp_create_node` | - |
| `bp_add_node` | - |
| `bp_delete_node` | - |
| `bp_connect_pins` | - |
| `bp_break_pin_links` | - |
| `bp_set_node_property` | - |
| `bp_create_reroute_node` | - |
| `bp_get_node_details` | - |
| `bp_get_graph_details` | - |
| `bp_get_pin_details` | - |
| `bp_list_node_types` | - |
| `bp_set_pin_default_value` | - |
| `query_assets_by_predicate` | - |
| `bp_implement_interface` | - |
| `bp_add_macro` | - |
| `bp_create_widget_binding` | - |
| `bp_add_custom_event` | - |
| `bp_set_replication_settings` | - |
| `bp_add_event_dispatcher` | - |
| `bp_bind_event` | - |
| `get_blueprint_dependencies` | - |
| `validate_blueprint` | - |
| `compile_blueprint_batch` | - |

---

## control_actor

**Category:** core

Spawn actors, transforms, physics, components, tags, attachments.

### Actions

| Action | Description |
|--------|-------------|
| `spawn` | - |
| `spawn_blueprint` | - |
| `delete` | - |
| `delete_by_tag` | - |
| `duplicate` | - |
| `apply_force` | - |
| `set_transform` | - |
| `get_transform` | - |
| `set_visibility` | - |
| `add_component` | - |
| `set_component_properties` | - |
| `get_components` | - |
| `add_tag` | - |
| `remove_tag` | - |
| `find_by_tag` | - |
| `find_by_name` | - |
| `list` | - |
| `set_blueprint_variables` | - |
| `create_snapshot` | - |
| `attach` | - |
| `detach` | - |
| `inspect_object` | - |
| `set_property` | - |
| `get_property` | - |
| `inspect_class` | - |
| `list_objects` | - |
| `get_component_property` | - |
| `set_component_property` | - |
| `get_metadata` | - |
| `restore_snapshot` | - |
| `export` | - |
| `delete_object` | - |
| `find_by_class` | - |
| `get_bounding_box` | - |
| `query_actors_by_predicate` | - |
| `get_all_component_properties` | - |
| `batch_set_component_properties` | - |
| `clone_component_hierarchy` | - |
| `serialize_actor_state` | - |
| `deserialize_actor_state` | - |
| `get_actor_bounds` | - |
| `batch_transform_actors` | - |
| `get_actor_references` | - |
| `replace_actor_class` | - |
| `merge_actors` | - |

---

## control_editor

**Category:** core

PIE, viewport, console, screenshots, profiling, CVars, UBT, widgets.

### Actions

| Action | Description |
|--------|-------------|
| `play` | - |
| `stop` | - |
| `stop_pie` | - |
| `pause` | - |
| `resume` | - |
| `set_game_speed` | - |
| `eject` | - |
| `possess` | - |
| `set_camera` | - |
| `set_camera_position` | - |
| `set_camera_fov` | - |
| `set_view_mode` | - |
| `set_viewport_resolution` | - |
| `console_command` | - |
| `execute_command` | - |
| `screenshot` | - |
| `step_frame` | - |
| `start_recording` | - |
| `stop_recording` | - |
| `create_bookmark` | - |
| `jump_to_bookmark` | - |
| `set_preferences` | - |
| `set_viewport_realtime` | - |
| `open_asset` | - |
| `simulate_input` | - |
| `create_input_action` | - |
| `create_input_mapping_context` | - |
| `add_mapping` | - |
| `remove_mapping` | - |
| `profile` | - |
| `show_fps` | - |
| `set_quality` | - |
| `set_resolution` | - |
| `set_fullscreen` | - |
| `run_ubt` | - |
| `run_tests` | - |
| `subscribe` | - |
| `unsubscribe` | - |
| `spawn_category` | - |
| `start_session` | - |
| `lumen_update_scene` | - |
| `subscribe_to_event` | - |
| `unsubscribe_from_event` | - |
| `get_subscribed_events` | - |
| `configure_event_channel` | - |
| `get_event_history` | - |
| `clear_event_subscriptions` | - |
| `start_background_job` | - |
| `get_job_status` | - |
| `cancel_job` | - |
| `get_active_jobs` | - |
| `play_sound` | - |
| `create_widget` | - |
| `show_widget` | - |
| `add_widget_child` | - |
| `set_cvar` | - |
| `get_project_settings` | - |
| `validate_assets` | - |
| `set_project_setting` | - |
| `batch_execute` | - |
| `parallel_execute` | - |
| `queue_operations` | - |
| `flush_operation_queue` | - |
| `capture_viewport` | - |
| `get_last_error_details` | - |
| `suggest_fix_for_error` | - |
| `validate_operation_preconditions` | - |
| `get_operation_history` | - |
| `get_available_actions` | - |
| `explain_action_parameters` | - |
| `get_class_hierarchy` | - |
| `validate_action_input` | - |
| `get_action_statistics` | - |
| `get_bridge_health` | - |
| `configure_megalights` | - |
| `get_light_budget_stats` | - |
| `convert_to_substrate` | - |
| `batch_substrate_migration` | - |
| `record_input_session` | - |
| `playback_input_session` | - |
| `capture_viewport_sequence` | - |
| `set_editor_mode` | - |
| `get_selection_info` | - |
| `toggle_realtime_rendering` | - |

---

## manage_level

**Category:** core

Levels, streaming, World Partition, data layers, HLOD; PCG graphs.

### Actions

| Action | Description |
|--------|-------------|
| `load` | - |
| `save` | - |
| `save_as` | - |
| `save_level_as` | - |
| `stream` | - |
| `create_level` | - |
| `create_light` | - |
| `build_lighting` | - |
| `set_metadata` | - |
| `load_cells` | - |
| `set_datalayer` | - |
| `export_level` | - |
| `import_level` | - |
| `list_levels` | - |
| `get_summary` | - |
| `delete` | - |
| `validate_level` | - |
| `cleanup_invalid_datalayers` | - |
| `add_sublevel` | - |
| `load_level` | - |
| `save_current_level` | - |
| `stream_level` | - |
| `bake_lightmap` | - |
| `spawn_light` | - |
| `create_datalayer` | - |
| `create_new_level` | - |
| `create_sublevel` | - |
| `configure_level_streaming` | - |
| `set_streaming_distance` | - |
| `configure_level_bounds` | - |
| `enable_world_partition` | - |
| `configure_grid_size` | - |
| `create_data_layer` | - |
| `assign_actor_to_data_layer` | - |
| `configure_hlod_layer` | - |
| `create_minimap_volume` | - |
| `open_level_blueprint` | - |
| `add_level_blueprint_node` | - |
| `connect_level_blueprint_nodes` | - |
| `create_level_instance` | - |
| `create_packed_level_actor` | - |
| `get_level_structure_info` | - |
| `configure_world_partition` | - |
| `create_streaming_volume` | - |
| `configure_large_world_coordinates` | - |
| `create_world_partition_cell` | - |
| `configure_runtime_loading` | - |
| `configure_world_settings` | - |
| `create_pcg_graph` | - |
| `create_pcg_subgraph` | - |
| `add_pcg_node` | - |
| `connect_pcg_pins` | - |
| `set_pcg_node_settings` | - |
| `add_landscape_data_node` | - |
| `add_spline_data_node` | - |
| `add_volume_data_node` | - |
| `add_actor_data_node` | - |
| `add_texture_data_node` | - |
| `add_surface_sampler` | - |
| `add_mesh_sampler` | - |
| `add_spline_sampler` | - |
| `add_volume_sampler` | - |
| `add_bounds_modifier` | - |
| `add_density_filter` | - |
| `add_height_filter` | - |
| `add_slope_filter` | - |
| `add_distance_filter` | - |
| `add_bounds_filter` | - |
| `add_self_pruning` | - |
| `add_transform_points` | - |
| `add_project_to_surface` | - |
| `add_copy_points` | - |
| `add_merge_points` | - |
| `add_static_mesh_spawner` | - |
| `add_actor_spawner` | - |
| `add_spline_spawner` | - |
| `execute_pcg_graph` | - |
| `set_pcg_partition_grid_size` | - |
| `get_pcg_info` | - |
| `create_biome_rules` | - |
| `blend_biomes` | - |
| `export_pcg_to_static` | - |
| `import_pcg_preset` | - |
| `debug_pcg_execution` | - |
| `create_pcg_hlsl_node` | - |
| `enable_pcg_gpu_processing` | - |
| `configure_pcg_mode_brush` | - |
| `export_pcg_hlsl_template` | - |
| `batch_execute_pcg_with_gpu` | - |
| `get_world_partition_cells` | - |
| `stream_level_async` | - |
| `get_streaming_levels_status` | - |
| `configure_hlod_settings` | - |
| `build_hlod_for_level` | - |

---

## manage_motion_design

**Category:** authoring

Motion Design (Avalanche) tools: Cloners, Effectors, Mograph.

### Actions

| Action | Description |
|--------|-------------|
| `create_cloner` | - |
| `configure_cloner_pattern` | - |
| `add_effector` | - |
| `animate_effector` | - |
| `create_mograph_sequence` | - |
| `create_radial_cloner` | - |
| `create_spline_cloner` | - |
| `add_noise_effector` | - |
| `configure_step_effector` | - |
| `export_mograph_to_sequence` | - |

---

## animation_physics

**Category:** utility

Animation BPs, Montages, IK, retargeting + Chaos destruction/vehicles.

### Actions

| Action | Description |
|--------|-------------|
| `create_animation_bp` | - |
| `play_montage` | - |
| `setup_ragdoll` | - |
| `activate_ragdoll` | - |
| `configure_vehicle` | - |
| `create_blend_space` | - |
| `create_state_machine` | - |
| `setup_ik` | - |
| `create_procedural_anim` | - |
| `create_blend_tree` | - |
| `setup_retargeting` | - |
| `setup_physics_simulation` | - |
| `cleanup` | - |
| `create_animation_asset` | - |
| `add_notify` | - |
| `create_animation_sequence` | - |
| `set_sequence_length` | - |
| `add_bone_track` | - |
| `set_bone_key` | - |
| `set_curve_key` | - |
| `add_notify_state` | - |
| `add_sync_marker` | - |
| `set_root_motion_settings` | - |
| `set_additive_settings` | - |
| `create_montage` | - |
| `add_montage_section` | - |
| `add_montage_slot` | - |
| `set_section_timing` | - |
| `add_montage_notify` | - |
| `set_blend_in` | - |
| `set_blend_out` | - |
| `link_sections` | - |
| `create_blend_space_1d` | - |
| `create_blend_space_2d` | - |
| `add_blend_sample` | - |
| `set_axis_settings` | - |
| `set_interpolation_settings` | - |
| `create_aim_offset` | - |
| `add_aim_offset_sample` | - |
| `create_anim_blueprint` | - |
| `add_state_machine` | - |
| `add_state` | - |
| `add_transition` | - |
| `set_transition_rules` | - |
| `add_blend_node` | - |
| `add_cached_pose` | - |
| `add_slot_node` | - |
| `add_layered_blend_per_bone` | - |
| `set_anim_graph_node_value` | - |
| `create_control_rig` | - |
| `add_control` | - |
| `add_rig_unit` | - |
| `connect_rig_elements` | - |
| `create_pose_library` | - |
| `create_ik_rig` | - |
| `add_ik_chain` | - |
| `add_ik_goal` | - |
| `create_ik_retargeter` | - |
| `set_retarget_chain_mapping` | - |
| `get_animation_info` | - |
| `create_pose_search_database` | - |
| `configure_motion_matching` | - |
| `add_trajectory_prediction` | - |
| `create_animation_modifier` | - |
| `setup_ml_deformer` | - |
| `configure_ml_deformer_training` | - |
| `get_motion_matching_state` | - |
| `set_motion_matching_goal` | - |
| `list_pose_search_databases` | - |
| `get_control_rig_controls` | - |
| `set_control_value` | - |
| `reset_control_rig` | - |
| `chaos_create_geometry_collection` | - |
| `chaos_fracture_uniform` | - |
| `chaos_fracture_clustered` | - |
| `chaos_fracture_radial` | - |
| `chaos_fracture_slice` | - |
| `chaos_fracture_brick` | - |
| `chaos_flatten_fracture` | - |
| `chaos_set_geometry_collection_materials` | - |
| `chaos_set_damage_thresholds` | - |
| `chaos_set_cluster_connection_type` | - |
| `chaos_set_collision_particles_fraction` | - |
| `chaos_set_remove_on_break` | - |
| `chaos_create_field_system_actor` | - |
| `chaos_add_transient_field` | - |
| `chaos_add_persistent_field` | - |
| `chaos_add_construction_field` | - |
| `chaos_add_field_radial_falloff` | - |
| `chaos_add_field_radial_vector` | - |
| `chaos_add_field_uniform_vector` | - |
| `chaos_add_field_noise` | - |
| `chaos_add_field_strain` | - |
| `chaos_create_anchor_field` | - |
| `chaos_set_dynamic_state` | - |
| `chaos_enable_clustering` | - |
| `chaos_get_geometry_collection_stats` | - |
| `chaos_create_geometry_collection_cache` | - |
| `chaos_record_geometry_collection_cache` | - |
| `chaos_apply_cache_to_collection` | - |
| `chaos_remove_geometry_collection_cache` | - |
| `chaos_create_wheeled_vehicle_bp` | - |
| `chaos_add_vehicle_wheel` | - |
| `chaos_remove_wheel_from_vehicle` | - |
| `chaos_configure_engine_setup` | - |
| `chaos_configure_transmission_setup` | - |
| `chaos_configure_steering_setup` | - |
| `chaos_configure_differential_setup` | - |
| `chaos_configure_suspension_setup` | - |
| `chaos_configure_brake_setup` | - |
| `chaos_set_vehicle_mesh` | - |
| `chaos_set_wheel_class` | - |
| `chaos_set_wheel_offset` | - |
| `chaos_set_wheel_radius` | - |
| `chaos_set_vehicle_mass` | - |
| `chaos_set_drag_coefficient` | - |
| `chaos_set_center_of_mass` | - |
| `chaos_create_vehicle_animation_instance` | - |
| `chaos_set_vehicle_animation_bp` | - |
| `chaos_get_vehicle_config` | - |
| `chaos_create_cloth_config` | - |
| `chaos_create_cloth_shared_sim_config` | - |
| `chaos_apply_cloth_to_skeletal_mesh` | - |
| `chaos_remove_cloth_from_skeletal_mesh` | - |
| `chaos_set_cloth_mass_properties` | - |
| `chaos_set_cloth_gravity` | - |
| `chaos_set_cloth_damping` | - |
| `chaos_set_cloth_collision_properties` | - |
| `chaos_set_cloth_stiffness` | - |
| `chaos_set_cloth_tether_stiffness` | - |
| `chaos_set_cloth_aerodynamics` | - |
| `chaos_set_cloth_anim_drive` | - |
| `chaos_set_cloth_long_range_attachment` | - |
| `chaos_get_cloth_config` | - |
| `chaos_get_cloth_stats` | - |
| `chaos_create_flesh_asset` | - |
| `chaos_create_flesh_component` | - |
| `chaos_set_flesh_simulation_properties` | - |
| `chaos_set_flesh_stiffness` | - |
| `chaos_set_flesh_damping` | - |
| `chaos_set_flesh_incompressibility` | - |
| `chaos_set_flesh_inflation` | - |
| `chaos_set_flesh_solver_iterations` | - |
| `chaos_bind_flesh_to_skeleton` | - |
| `chaos_set_flesh_rest_state` | - |
| `chaos_create_flesh_cache` | - |
| `chaos_record_flesh_simulation` | - |
| `chaos_get_flesh_asset_info` | - |
| `chaos_get_physics_destruction_info` | - |
| `chaos_list_geometry_collections` | - |
| `chaos_list_chaos_vehicles` | - |
| `chaos_get_plugin_status` | - |
| `create_anim_layer` | - |
| `stack_anim_layers` | - |
| `configure_squash_stretch` | - |
| `create_rigging_layer` | - |
| `configure_layer_blend_mode` | - |
| `create_control_rig_physics` | - |
| `configure_ragdoll_profile` | - |
| `blend_ragdoll_to_animation` | - |
| `get_bone_transforms` | - |
| `apply_pose_asset` | - |
| `create_animation_blueprint` | - |
| `play_anim_montage` | - |
| `apply_animation_modifier` | - |
| `get_chaos_plugin_status` | - |

---

## manage_effect

**Category:** utility

Niagara/Cascade particles, debug shapes, VFX graph authoring.

### Actions

| Action | Description |
|--------|-------------|
| `particle` | - |
| `niagara` | - |
| `debug_shape` | - |
| `spawn_niagara` | - |
| `create_dynamic_light` | - |
| `create_niagara_system` | - |
| `create_niagara_emitter` | - |
| `create_volumetric_fog` | - |
| `create_particle_trail` | - |
| `create_environment_effect` | - |
| `create_impact_effect` | - |
| `create_niagara_ribbon` | - |
| `activate` | - |
| `activate_effect` | - |
| `deactivate` | - |
| `reset` | - |
| `advance_simulation` | - |
| `add_niagara_module` | - |
| `connect_niagara_pins` | - |
| `remove_niagara_node` | - |
| `set_niagara_parameter` | - |
| `clear_debug_shapes` | - |
| `cleanup` | - |
| `list_debug_shapes` | - |
| `add_emitter_to_system` | - |
| `set_emitter_properties` | - |
| `add_spawn_rate_module` | - |
| `add_spawn_burst_module` | - |
| `add_spawn_per_unit_module` | - |
| `add_initialize_particle_module` | - |
| `add_particle_state_module` | - |
| `add_force_module` | - |
| `add_velocity_module` | - |
| `add_acceleration_module` | - |
| `add_size_module` | - |
| `add_color_module` | - |
| `add_sprite_renderer_module` | - |
| `add_mesh_renderer_module` | - |
| `add_ribbon_renderer_module` | - |
| `add_light_renderer_module` | - |
| `add_collision_module` | - |
| `add_kill_particles_module` | - |
| `add_camera_offset_module` | - |
| `add_user_parameter` | - |
| `set_parameter_value` | - |
| `bind_parameter_to_source` | - |
| `add_skeletal_mesh_data_interface` | - |
| `add_static_mesh_data_interface` | - |
| `add_spline_data_interface` | - |
| `add_audio_spectrum_data_interface` | - |
| `add_collision_query_data_interface` | - |
| `add_event_generator` | - |
| `add_event_receiver` | - |
| `configure_event_payload` | - |
| `enable_gpu_simulation` | - |
| `add_simulation_stage` | - |
| `get_niagara_info` | - |
| `validate_niagara_system` | - |
| `create_niagara_module` | - |
| `add_niagara_script` | - |
| `add_data_interface` | - |
| `setup_niagara_fluids` | - |
| `create_fluid_simulation` | - |
| `add_chaos_integration` | - |
| `create_niagara_sim_cache` | - |
| `configure_niagara_lod` | - |
| `export_niagara_system` | - |
| `import_niagara_module` | - |
| `configure_niagara_determinism` | - |
| `create_niagara_data_interface` | - |
| `configure_gpu_simulation` | - |
| `batch_compile_niagara` | - |
| `get_niagara_parameters` | - |
| `set_niagara_variable` | - |
| `activate_niagara` | - |
| `deactivate_niagara` | - |
| `reset_niagara` | - |
| `spawn_niagara_actor` | - |
| `modify_niagara_parameter` | - |

---

## build_environment

**Category:** world

Landscapes, foliage, procedural terrain, sky/fog, water, weather.

### Actions

| Action | Description |
|--------|-------------|
| `create_landscape` | - |
| `sculpt` | - |
| `sculpt_landscape` | - |
| `add_foliage` | - |
| `paint_foliage` | - |
| `create_procedural_terrain` | - |
| `create_procedural_foliage` | - |
| `add_foliage_instances` | - |
| `get_foliage_instances` | - |
| `remove_foliage` | - |
| `paint_landscape` | - |
| `paint_landscape_layer` | - |
| `modify_heightmap` | - |
| `set_landscape_material` | - |
| `create_landscape_grass_type` | - |
| `generate_lods` | - |
| `bake_lightmap` | - |
| `export_snapshot` | - |
| `import_snapshot` | - |
| `delete` | - |
| `create_sky_sphere` | - |
| `create_sky_atmosphere` | - |
| `configure_sky_atmosphere` | - |
| `create_fog_volume` | - |
| `create_exponential_height_fog` | - |
| `configure_exponential_height_fog` | - |
| `create_volumetric_cloud` | - |
| `configure_volumetric_cloud` | - |
| `set_time_of_day` | - |
| `create_water_body_ocean` | - |
| `create_water_body_lake` | - |
| `create_water_body_river` | - |
| `configure_water_body` | - |
| `configure_water_waves` | - |
| `get_water_body_info` | - |
| `list_water_bodies` | - |
| `set_river_depth` | - |
| `set_ocean_extent` | - |
| `set_water_static_mesh` | - |
| `set_river_transitions` | - |
| `set_water_zone` | - |
| `get_water_surface_info` | - |
| `get_wave_info` | - |
| `configure_wind` | - |
| `create_weather_system` | - |
| `configure_rain_particles` | - |
| `configure_snow_particles` | - |
| `configure_lightning` | - |
| `configure_weather_preset` | - |
| `query_water_bodies` | - |
| `configure_ocean_waves` | - |
| `create_landscape_spline` | - |
| `configure_foliage_density` | - |
| `batch_paint_foliage` | - |
| `configure_sky_atmosphere` | - |
| `create_volumetric_cloud` | - |
| `configure_wind_directional` | - |
| `get_terrain_height_at` | - |
| `add_foliage_type` | - |
| `configure_foliage_lod` | - |
| `configure_foliage_placement` | - |
| `get_foliage_types` | - |
| `configure_landscape_lod` | - |
| `get_landscape_info` | - |
| `import_heightmap` | - |
| `export_heightmap` | - |
| `configure_sun_position` | - |
| `configure_sun_color` | - |
| `configure_sun_atmosphere` | - |
| `set_skylight_intensity` | - |
| `set_sun_intensity` | - |
| `create_time_of_day_controller` | - |
| `get_water_depth_info` | - |

---

## manage_sequence

**Category:** utility

Sequencer cinematics, Level Sequences, keyframes, MRQ renders.

### Actions

| Action | Description |
|--------|-------------|
| `create` | - |
| `open` | - |
| `duplicate` | - |
| `rename` | - |
| `delete` | - |
| `list` | - |
| `get_metadata` | - |
| `set_metadata` | - |
| `create_master_sequence` | - |
| `add_subsequence` | - |
| `remove_subsequence` | - |
| `get_subsequences` | - |
| `export_sequence` | - |
| `add_actor` | - |
| `add_actors` | - |
| `remove_actors` | - |
| `bind_actor` | - |
| `unbind_actor` | - |
| `get_bindings` | - |
| `add_spawnable_from_class` | - |
| `add_track` | - |
| `remove_track` | - |
| `list_tracks` | - |
| `list_track_types` | - |
| `add_section` | - |
| `remove_section` | - |
| `get_tracks` | - |
| `set_track_muted` | - |
| `set_track_solo` | - |
| `set_track_locked` | - |
| `add_shot_track` | - |
| `add_shot` | - |
| `remove_shot` | - |
| `get_shots` | - |
| `add_camera` | - |
| `create_cine_camera_actor` | - |
| `configure_camera_settings` | - |
| `add_camera_cut_track` | - |
| `add_camera_cut` | - |
| `add_keyframe` | - |
| `remove_keyframe` | - |
| `get_keyframes` | - |
| `get_properties` | - |
| `set_properties` | - |
| `set_display_rate` | - |
| `set_tick_resolution` | - |
| `set_work_range` | - |
| `set_view_range` | - |
| `set_playback_range` | - |
| `get_playback_range` | - |
| `get_sequence_info` | - |
| `play` | - |
| `pause` | - |
| `stop` | - |
| `set_playback_speed` | - |
| `play_sequence` | - |
| `pause_sequence` | - |
| `stop_sequence` | - |
| `scrub_to_time` | - |
| `create_queue` | - |
| `add_job` | - |
| `remove_job` | - |
| `clear_queue` | - |
| `get_queue` | - |
| `configure_job` | - |
| `set_sequence` | - |
| `set_map` | - |
| `configure_output` | - |
| `set_resolution` | - |
| `set_frame_rate` | - |
| `set_output_directory` | - |
| `set_file_name_format` | - |
| `add_render_pass` | - |
| `remove_render_pass` | - |
| `get_render_passes` | - |
| `configure_render_pass` | - |
| `configure_anti_aliasing` | - |
| `set_spatial_sample_count` | - |
| `set_temporal_sample_count` | - |
| `add_burn_in` | - |
| `remove_burn_in` | - |
| `configure_burn_in` | - |
| `start_render` | - |
| `stop_render` | - |
| `get_render_status` | - |
| `get_render_progress` | - |
| `add_console_variable` | - |
| `remove_console_variable` | - |
| `configure_high_res_settings` | - |
| `set_tile_count` | - |
| `create_media_track` | - |
| `configure_sequence_streaming` | - |
| `create_event_trigger_track` | - |
| `add_procedural_camera_shake` | - |
| `configure_sequence_lod` | - |
| `create_camera_cut_track` | - |
| `configure_mrq_settings` | - |
| `batch_render_sequences` | - |
| `get_sequence_bindings` | - |
| `configure_audio_track` | - |
| `sequence_create` | - |
| `sequence_open` | - |
| `sequence_delete` | - |
| `sequence_duplicate` | - |
| `sequence_list` | - |
| `sequence_rename` | - |
| `sequence_add_actor` | - |
| `sequence_add_actors` | - |
| `sequence_add_camera` | - |
| `sequence_add_track` | - |
| `sequence_remove_track` | - |
| `sequence_add_keyframe` | - |
| `sequence_add_section` | - |
| `sequence_add_spawnable_from_class` | - |
| `sequence_get_bindings` | - |
| `sequence_get_metadata` | - |
| `sequence_get_properties` | - |
| `sequence_set_properties` | - |
| `sequence_set_display_rate` | - |
| `sequence_set_tick_resolution` | - |
| `sequence_set_work_range` | - |
| `sequence_set_view_range` | - |
| `sequence_play` | - |
| `sequence_pause` | - |
| `sequence_stop` | - |
| `sequence_set_playback_speed` | - |
| `sequence_list_tracks` | - |
| `sequence_list_track_types` | - |
| `sequence_remove_actors` | - |
| `sequence_set_track_muted` | - |
| `sequence_set_track_solo` | - |
| `sequence_set_track_locked` | - |
| `add_animation_track` | - |
| `add_camera_track` | - |
| `add_transform_track` | - |
| `add_sequencer_keyframe` | - |
| `delete_sequence` | - |
| `duplicate_sequence` | - |
| `list_sequences` | - |

---

## manage_audio

**Category:** utility

Audio playback, mixes, MetaSounds + Wwise/FMOD/Bink middleware.

### Actions

| Action | Description |
|--------|-------------|
| `create_sound_cue` | - |
| `play_sound_at_location` | - |
| `play_sound_2d` | - |
| `create_audio_component` | - |
| `create_sound_mix` | - |
| `push_sound_mix` | - |
| `pop_sound_mix` | - |
| `set_sound_mix_class_override` | - |
| `clear_sound_mix_class_override` | - |
| `set_base_sound_mix` | - |
| `prime_sound` | - |
| `play_sound_attached` | - |
| `spawn_sound_at_location` | - |
| `fade_sound_in` | - |
| `fade_sound_out` | - |
| `create_ambient_sound` | - |
| `create_sound_class` | - |
| `set_sound_attenuation` | - |
| `create_reverb_zone` | - |
| `enable_audio_analysis` | - |
| `fade_sound` | - |
| `set_doppler_effect` | - |
| `set_audio_occlusion` | - |
| `add_cue_node` | - |
| `connect_cue_nodes` | - |
| `set_cue_attenuation` | - |
| `set_cue_concurrency` | - |
| `create_metasound` | - |
| `add_metasound_node` | - |
| `connect_metasound_nodes` | - |
| `add_metasound_input` | - |
| `add_metasound_output` | - |
| `set_metasound_default` | - |
| `set_class_properties` | - |
| `set_class_parent` | - |
| `add_mix_modifier` | - |
| `configure_mix_eq` | - |
| `create_attenuation_settings` | - |
| `configure_distance_attenuation` | - |
| `configure_spatialization` | - |
| `configure_occlusion` | - |
| `configure_reverb_send` | - |
| `create_dialogue_voice` | - |
| `create_dialogue_wave` | - |
| `set_dialogue_context` | - |
| `create_reverb_effect` | - |
| `create_source_effect_chain` | - |
| `add_source_effect` | - |
| `create_submix_effect` | - |
| `get_audio_info` | - |
| `mw_connect_wwise_project` | - |
| `mw_post_wwise_event` | - |
| `mw_post_wwise_event_at_location` | - |
| `mw_stop_wwise_event` | - |
| `mw_set_rtpc_value` | - |
| `mw_set_rtpc_value_on_actor` | - |
| `mw_get_rtpc_value` | - |
| `mw_set_wwise_switch` | - |
| `mw_set_wwise_switch_on_actor` | - |
| `mw_set_wwise_state` | - |
| `mw_load_wwise_bank` | - |
| `mw_unload_wwise_bank` | - |
| `mw_get_loaded_banks` | - |
| `mw_create_wwise_component` | - |
| `mw_configure_wwise_component` | - |
| `mw_configure_spatial_audio` | - |
| `mw_configure_room` | - |
| `mw_configure_portal` | - |
| `mw_set_listener_position` | - |
| `mw_get_wwise_event_duration` | - |
| `mw_create_wwise_trigger` | - |
| `mw_set_wwise_game_object` | - |
| `mw_unset_wwise_game_object` | - |
| `mw_post_wwise_trigger` | - |
| `mw_set_aux_send` | - |
| `mw_configure_wwise_occlusion` | - |
| `mw_set_wwise_project_path` | - |
| `mw_get_wwise_status` | - |
| `mw_configure_wwise_init` | - |
| `mw_restart_wwise_engine` | - |
| `mw_connect_fmod_project` | - |
| `mw_play_fmod_event` | - |
| `mw_play_fmod_event_at_location` | - |
| `mw_stop_fmod_event` | - |
| `mw_set_fmod_parameter` | - |
| `mw_set_fmod_global_parameter` | - |
| `mw_get_fmod_parameter` | - |
| `mw_load_fmod_bank` | - |
| `mw_unload_fmod_bank` | - |
| `mw_get_fmod_loaded_banks` | - |
| `mw_create_fmod_component` | - |
| `mw_configure_fmod_component` | - |
| `mw_set_fmod_bus_volume` | - |
| `mw_set_fmod_bus_paused` | - |
| `mw_set_fmod_bus_mute` | - |
| `mw_set_fmod_vca_volume` | - |
| `mw_apply_fmod_snapshot` | - |
| `mw_release_fmod_snapshot` | - |
| `mw_set_fmod_listener_attributes` | - |
| `mw_get_fmod_event_info` | - |
| `mw_configure_fmod_occlusion` | - |
| `mw_configure_fmod_attenuation` | - |
| `mw_set_fmod_studio_path` | - |
| `mw_get_fmod_status` | - |
| `mw_configure_fmod_init` | - |
| `mw_restart_fmod_engine` | - |
| `mw_set_fmod_3d_attributes` | - |
| `mw_get_fmod_memory_usage` | - |
| `mw_pause_all_fmod_events` | - |
| `mw_resume_all_fmod_events` | - |
| `mw_create_bink_media_player` | - |
| `mw_open_bink_video` | - |
| `mw_play_bink` | - |
| `mw_pause_bink` | - |
| `mw_stop_bink` | - |
| `mw_seek_bink` | - |
| `mw_set_bink_looping` | - |
| `mw_set_bink_rate` | - |
| `mw_set_bink_volume` | - |
| `mw_get_bink_duration` | - |
| `mw_get_bink_time` | - |
| `mw_get_bink_status` | - |
| `mw_create_bink_texture` | - |
| `mw_configure_bink_texture` | - |
| `mw_set_bink_texture_player` | - |
| `mw_draw_bink_to_texture` | - |
| `mw_configure_bink_buffer_mode` | - |
| `mw_configure_bink_sound_track` | - |
| `mw_configure_bink_draw_style` | - |
| `mw_get_bink_dimensions` | - |
| `mw_get_audio_middleware_info` | - |
| `list_metasound_assets` | - |
| `get_metasound_inputs` | - |
| `trigger_metasound` | - |

---

## manage_lighting

**Category:** world

Lights (point, spot, rect, sky), GI, shadows, volumetric fog.

### Actions

| Action | Description |
|--------|-------------|
| `spawn_light` | - |
| `create_light` | - |
| `spawn_sky_light` | - |
| `create_sky_light` | - |
| `ensure_single_sky_light` | - |
| `create_lightmass_volume` | - |
| `create_lighting_enabled_level` | - |
| `create_dynamic_light` | - |
| `setup_global_illumination` | - |
| `configure_shadows` | - |
| `set_exposure` | - |
| `set_ambient_occlusion` | - |
| `setup_volumetric_fog` | - |
| `build_lighting` | - |
| `list_light_types` | - |
| `create_post_process_volume` | - |
| `configure_pp_blend` | - |
| `configure_pp_priority` | - |
| `get_post_process_settings` | - |
| `configure_bloom` | - |
| `configure_dof` | - |
| `configure_motion_blur` | - |
| `configure_color_grading` | - |
| `configure_white_balance` | - |
| `configure_vignette` | - |
| `configure_chromatic_aberration` | - |
| `configure_film_grain` | - |
| `configure_lens_flares` | - |
| `create_sphere_reflection_capture` | - |
| `create_box_reflection_capture` | - |
| `create_planar_reflection` | - |
| `recapture_scene` | - |
| `configure_ray_traced_shadows` | - |
| `configure_ray_traced_gi` | - |
| `configure_ray_traced_reflections` | - |
| `configure_ray_traced_ao` | - |
| `configure_path_tracing` | - |
| `create_scene_capture_2d` | - |
| `create_scene_capture_cube` | - |
| `capture_scene` | - |
| `set_light_channel` | - |
| `set_actor_light_channel` | - |
| `configure_lightmass_settings` | - |
| `build_lighting_quality` | - |
| `configure_indirect_lighting_cache` | - |
| `configure_volumetric_lightmap` | - |
| `configure_lumen_gi` | - |
| `set_lumen_reflections` | - |
| `tune_lumen_performance` | - |
| `create_lumen_volume` | - |
| `set_virtual_shadow_maps` | - |
| `configure_megalights_scene` | - |
| `get_megalights_budget` | - |
| `optimize_lights_for_megalights` | - |
| `configure_gi_settings` | - |
| `bake_lighting_preview` | - |
| `get_light_complexity` | - |
| `configure_volumetric_fog` | - |
| `create_light_batch` | - |
| `configure_shadow_settings` | - |
| `validate_lighting_setup` | - |

---

## manage_performance

**Category:** utility

Profiling, benchmarks, scalability, LOD, Nanite optimization.

### Actions

| Action | Description |
|--------|-------------|
| `start_profiling` | - |
| `stop_profiling` | - |
| `run_benchmark` | - |
| `show_fps` | - |
| `show_stats` | - |
| `generate_memory_report` | - |
| `set_scalability` | - |
| `set_resolution_scale` | - |
| `set_vsync` | - |
| `set_frame_rate_limit` | - |
| `enable_gpu_timing` | - |
| `configure_texture_streaming` | - |
| `configure_lod` | - |
| `apply_baseline_settings` | - |
| `optimize_draw_calls` | - |
| `merge_actors` | - |
| `configure_occlusion_culling` | - |
| `optimize_shaders` | - |
| `configure_nanite` | - |
| `configure_world_partition` | - |

---

## manage_geometry

**Category:** world

Procedural meshes via Geometry Script: booleans, UVs, collision.

### Actions

| Action | Description |
|--------|-------------|
| `create_box` | - |
| `create_sphere` | - |
| `create_cylinder` | - |
| `create_cone` | - |
| `create_capsule` | - |
| `create_torus` | - |
| `create_plane` | - |
| `create_disc` | - |
| `create_stairs` | - |
| `create_spiral_stairs` | - |
| `create_ring` | - |
| `create_arch` | - |
| `create_pipe` | - |
| `create_ramp` | - |
| `boolean_union` | - |
| `boolean_subtract` | - |
| `boolean_intersection` | - |
| `boolean_trim` | - |
| `self_union` | - |
| `extrude` | - |
| `inset` | - |
| `outset` | - |
| `bevel` | - |
| `offset_faces` | - |
| `shell` | - |
| `revolve` | - |
| `chamfer` | - |
| `extrude_along_spline` | - |
| `bridge` | - |
| `loft` | - |
| `sweep` | - |
| `duplicate_along_spline` | - |
| `loop_cut` | - |
| `edge_split` | - |
| `quadrangulate` | - |
| `bend` | - |
| `twist` | - |
| `taper` | - |
| `noise_deform` | - |
| `smooth` | - |
| `relax` | - |
| `stretch` | - |
| `spherify` | - |
| `cylindrify` | - |
| `triangulate` | - |
| `poke` | - |
| `mirror` | - |
| `array_linear` | - |
| `array_radial` | - |
| `simplify_mesh` | - |
| `subdivide` | - |
| `remesh_uniform` | - |
| `merge_vertices` | - |
| `remesh_voxel` | - |
| `weld_vertices` | - |
| `fill_holes` | - |
| `remove_degenerates` | - |
| `auto_uv` | - |
| `project_uv` | - |
| `transform_uvs` | - |
| `unwrap_uv` | - |
| `pack_uv_islands` | - |
| `recalculate_normals` | - |
| `flip_normals` | - |
| `recompute_tangents` | - |
| `generate_collision` | - |
| `generate_complex_collision` | - |
| `simplify_collision` | - |
| `generate_lods` | - |
| `set_lod_settings` | - |
| `set_lod_screen_sizes` | - |
| `convert_to_nanite` | - |
| `convert_to_static_mesh` | - |
| `get_mesh_info` | - |
| `create_procedural_box` | - |
| `boolean_mesh_operation` | - |
| `generate_mesh_uvs` | - |
| `create_mesh_from_spline` | - |
| `configure_nanite_settings` | - |
| `export_geometry_to_file` | - |

---

## manage_skeleton

**Category:** authoring

Skeletal meshes, sockets, physics assets; media players/sources.

### Actions

| Action | Description |
|--------|-------------|
| `create_skeleton` | - |
| `add_bone` | - |
| `remove_bone` | - |
| `rename_bone` | - |
| `set_bone_transform` | - |
| `set_bone_parent` | - |
| `create_virtual_bone` | - |
| `create_socket` | - |
| `configure_socket` | - |
| `auto_skin_weights` | - |
| `set_vertex_weights` | - |
| `normalize_weights` | - |
| `prune_weights` | - |
| `copy_weights` | - |
| `mirror_weights` | - |
| `create_physics_asset` | - |
| `add_physics_body` | - |
| `configure_physics_body` | - |
| `add_physics_constraint` | - |
| `configure_constraint_limits` | - |
| `bind_cloth_to_skeletal_mesh` | - |
| `assign_cloth_asset_to_mesh` | - |
| `create_morph_target` | - |
| `set_morph_target_deltas` | - |
| `import_morph_targets` | - |
| `get_skeleton_info` | - |
| `list_bones` | - |
| `list_sockets` | - |
| `list_physics_bodies` | - |
| `create_media_player` | - |
| `create_file_media_source` | - |
| `create_stream_media_source` | - |
| `create_media_texture` | - |
| `create_media_playlist` | - |
| `create_media_sound_wave` | - |
| `delete_media_asset` | - |
| `get_media_info` | - |
| `add_to_playlist` | - |
| `remove_from_playlist` | - |
| `get_playlist` | - |
| `open_source` | - |
| `open_url` | - |
| `close` | - |
| `play` | - |
| `pause` | - |
| `stop` | - |
| `seek` | - |
| `set_rate` | - |
| `set_looping` | - |
| `get_duration` | - |
| `get_time` | - |
| `get_state` | - |
| `bind_to_texture` | - |
| `unbind_from_texture` | - |

---

## manage_material_authoring

**Category:** authoring

Materials, expressions, parameters, landscape layers, textures.

### Actions

| Action | Description |
|--------|-------------|
| `create_material` | - |
| `set_blend_mode` | - |
| `set_shading_model` | - |
| `set_material_domain` | - |
| `add_texture_sample` | - |
| `add_texture_coordinate` | - |
| `add_scalar_parameter` | - |
| `add_vector_parameter` | - |
| `add_static_switch_parameter` | - |
| `add_math_node` | - |
| `add_world_position` | - |
| `add_vertex_normal` | - |
| `add_pixel_depth` | - |
| `add_fresnel` | - |
| `add_reflection_vector` | - |
| `add_panner` | - |
| `add_rotator` | - |
| `add_noise` | - |
| `add_voronoi` | - |
| `add_if` | - |
| `add_switch` | - |
| `add_custom_expression` | - |
| `connect_nodes` | - |
| `disconnect_nodes` | - |
| `create_material_function` | - |
| `add_function_input` | - |
| `add_function_output` | - |
| `use_material_function` | - |
| `create_material_instance` | - |
| `set_scalar_parameter_value` | - |
| `set_vector_parameter_value` | - |
| `set_texture_parameter_value` | - |
| `create_landscape_material` | - |
| `create_decal_material` | - |
| `create_post_process_material` | - |
| `add_landscape_layer` | - |
| `configure_layer_blend` | - |
| `compile_material` | - |
| `get_material_info` | - |
| `create_noise_texture` | - |
| `create_gradient_texture` | - |
| `create_pattern_texture` | - |
| `create_normal_from_height` | - |
| `create_ao_from_mesh` | - |
| `resize_texture` | - |
| `adjust_levels` | - |
| `adjust_curves` | - |
| `blur` | - |
| `sharpen` | - |
| `invert` | - |
| `desaturate` | - |
| `channel_pack` | - |
| `channel_extract` | - |
| `combine_textures` | - |
| `set_compression_settings` | - |
| `set_texture_group` | - |
| `set_lod_bias` | - |
| `configure_virtual_texture` | - |
| `set_streaming_priority` | - |
| `get_texture_info` | - |
| `create_substrate_material` | - |
| `set_substrate_properties` | - |
| `configure_sss_profile` | - |
| `configure_exposure` | - |
| `convert_material_to_substrate` | - |
| `batch_convert_to_substrate` | - |
| `create_material_expression_template` | - |
| `configure_landscape_material_layer` | - |
| `create_material_instance_batch` | - |
| `get_material_dependencies` | - |
| `validate_material` | - |
| `configure_material_lod` | - |
| `export_material_template` | - |

---

## manage_character

**Category:** gameplay

Characters, movement, locomotion + Inventory (items, equipment).

### Actions

| Action | Description |
|--------|-------------|
| `create_character_blueprint` | - |
| `configure_capsule_component` | - |
| `configure_mesh_component` | - |
| `configure_camera_component` | - |
| `configure_movement_speeds` | - |
| `configure_jump` | - |
| `configure_rotation` | - |
| `add_custom_movement_mode` | - |
| `configure_nav_movement` | - |
| `setup_mantling` | - |
| `setup_vaulting` | - |
| `setup_climbing` | - |
| `setup_sliding` | - |
| `setup_wall_running` | - |
| `setup_grappling` | - |
| `setup_footstep_system` | - |
| `map_surface_to_sound` | - |
| `configure_footstep_fx` | - |
| `get_character_info` | - |
| `create_interaction_component` | - |
| `configure_interaction_trace` | - |
| `configure_interaction_widget` | - |
| `add_interaction_events` | - |
| `create_interactable_interface` | - |
| `create_door_actor` | - |
| `configure_door_properties` | - |
| `create_switch_actor` | - |
| `configure_switch_properties` | - |
| `create_chest_actor` | - |
| `configure_chest_properties` | - |
| `create_lever_actor` | - |
| `setup_destructible_mesh` | - |
| `configure_destruction_levels` | - |
| `configure_destruction_effects` | - |
| `configure_destruction_damage` | - |
| `add_destruction_component` | - |
| `create_trigger_actor` | - |
| `configure_trigger_events` | - |
| `configure_trigger_filter` | - |
| `configure_trigger_response` | - |
| `get_interaction_info` | - |
| `inv_create_item_data_asset` | - |
| `inv_set_item_properties` | - |
| `inv_create_item_category` | - |
| `inv_assign_item_category` | - |
| `inv_create_inventory_component` | - |
| `inv_configure_inventory_slots` | - |
| `inv_add_inventory_functions` | - |
| `inv_configure_inventory_events` | - |
| `inv_set_inventory_replication` | - |
| `inv_create_pickup_actor` | - |
| `inv_configure_pickup_interaction` | - |
| `inv_configure_pickup_respawn` | - |
| `inv_configure_pickup_effects` | - |
| `inv_create_equipment_component` | - |
| `inv_define_equipment_slots` | - |
| `inv_configure_equipment_effects` | - |
| `inv_add_equipment_functions` | - |
| `inv_configure_equipment_visuals` | - |
| `inv_create_loot_table` | - |
| `inv_add_loot_entry` | - |
| `inv_configure_loot_drop` | - |
| `inv_set_loot_quality_tiers` | - |
| `inv_create_crafting_recipe` | - |
| `inv_configure_recipe_requirements` | - |
| `inv_create_crafting_station` | - |
| `inv_add_crafting_component` | - |
| `inv_get_inventory_info` | - |
| `configure_locomotion_state` | - |
| `query_interaction_targets` | - |
| `configure_inventory_slot` | - |
| `batch_add_inventory_items` | - |
| `configure_equipment_socket` | - |
| `get_character_stats_snapshot` | - |
| `apply_status_effect` | - |
| `configure_footstep_system` | - |
| `set_movement_mode` | - |
| `configure_mantle_vault` | - |

---

## manage_combat

**Category:** gameplay

Weapons, projectiles, damage, melee; GAS abilities and effects.

### Actions

| Action | Description |
|--------|-------------|
| `create_weapon_blueprint` | - |
| `configure_weapon_mesh` | - |
| `configure_weapon_sockets` | - |
| `set_weapon_stats` | - |
| `configure_hitscan` | - |
| `configure_projectile` | - |
| `configure_spread_pattern` | - |
| `configure_recoil_pattern` | - |
| `configure_aim_down_sights` | - |
| `create_projectile_blueprint` | - |
| `configure_projectile_movement` | - |
| `configure_projectile_collision` | - |
| `configure_projectile_homing` | - |
| `create_damage_type` | - |
| `configure_damage_execution` | - |
| `setup_hitbox_component` | - |
| `setup_reload_system` | - |
| `setup_ammo_system` | - |
| `setup_attachment_system` | - |
| `setup_weapon_switching` | - |
| `configure_muzzle_flash` | - |
| `configure_tracer` | - |
| `configure_impact_effects` | - |
| `configure_shell_ejection` | - |
| `create_melee_trace` | - |
| `configure_combo_system` | - |
| `create_hit_pause` | - |
| `configure_hit_reaction` | - |
| `setup_parry_block_system` | - |
| `configure_weapon_trails` | - |
| `get_combat_info` | - |
| `create_combo_sequence` | - |
| `apply_damage_with_effects` | - |
| `configure_weapon_trace` | - |
| `create_projectile_pool` | - |
| `configure_gas_effect` | - |
| `grant_gas_ability` | - |
| `configure_melee_trace` | - |
| `get_combat_stats` | - |
| `configure_block_parry` | - |
| `add_ability_system_component` | - |
| `configure_asc` | - |
| `create_attribute_set` | - |
| `add_attribute` | - |
| `set_attribute_base_value` | - |
| `set_attribute_clamping` | - |
| `create_gameplay_ability` | - |
| `set_ability_tags` | - |
| `set_ability_costs` | - |
| `set_ability_cooldown` | - |
| `set_ability_targeting` | - |
| `add_ability_task` | - |
| `set_activation_policy` | - |
| `set_instancing_policy` | - |
| `create_gameplay_effect` | - |
| `set_effect_duration` | - |
| `add_effect_modifier` | - |
| `set_modifier_magnitude` | - |
| `add_effect_execution_calculation` | - |
| `add_effect_cue` | - |
| `set_effect_stacking` | - |
| `set_effect_tags` | - |
| `create_gameplay_cue_notify` | - |
| `configure_cue_trigger` | - |
| `set_cue_effects` | - |
| `add_tag_to_asset` | - |
| `get_gas_info` | - |

---

## manage_ai

**Category:** gameplay

AI Controllers, BT, EQS, perception, State Trees, MassAI, NPCs.

### Actions

| Action | Description |
|--------|-------------|
| `create_ai_controller` | - |
| `assign_behavior_tree` | - |
| `assign_blackboard` | - |
| `create_blackboard_asset` | - |
| `add_blackboard_key` | - |
| `set_key_instance_synced` | - |
| `create_behavior_tree` | - |
| `add_composite_node` | - |
| `add_task_node` | - |
| `add_decorator` | - |
| `add_service` | - |
| `configure_bt_node` | - |
| `create_eqs_query` | - |
| `add_eqs_generator` | - |
| `add_eqs_context` | - |
| `add_eqs_test` | - |
| `configure_test_scoring` | - |
| `add_ai_perception_component` | - |
| `configure_sight_config` | - |
| `configure_hearing_config` | - |
| `configure_damage_sense_config` | - |
| `set_perception_team` | - |
| `create_state_tree` | - |
| `add_state_tree_state` | - |
| `add_state_tree_transition` | - |
| `configure_state_tree_task` | - |
| `create_smart_object_definition` | - |
| `add_smart_object_slot` | - |
| `configure_slot_behavior` | - |
| `add_smart_object_component` | - |
| `bind_statetree` | - |
| `create_mass_entity_config` | - |
| `configure_mass_entity` | - |
| `add_mass_spawner` | - |
| `spawn_mass_entity` | - |
| `destroy_mass_entity` | - |
| `query_mass_entities` | - |
| `set_mass_entity_fragment` | - |
| `get_statetree_state` | - |
| `trigger_statetree_transition` | - |
| `list_statetree_states` | - |
| `create_smart_object` | - |
| `query_smart_objects` | - |
| `claim_smart_object` | - |
| `release_smart_object` | - |
| `get_ai_info` | - |
| `bt_add_node` | - |
| `bt_connect_nodes` | - |
| `bt_remove_node` | - |
| `bt_break_connections` | - |
| `bt_set_node_properties` | - |
| `configure_nav_mesh_settings` | - |
| `set_nav_agent_properties` | - |
| `rebuild_navigation` | - |
| `create_nav_modifier_component` | - |
| `set_nav_area_class` | - |
| `configure_nav_area_cost` | - |
| `create_nav_link_proxy` | - |
| `configure_nav_link` | - |
| `set_nav_link_type` | - |
| `create_smart_link` | - |
| `configure_smart_link_behavior` | - |
| `get_navigation_info` | - |
| `create_convai_character` | - |
| `configure_character_backstory` | - |
| `configure_character_voice` | - |
| `configure_convai_lipsync` | - |
| `start_convai_session` | - |
| `stop_convai_session` | - |
| `send_text_to_character` | - |
| `get_character_response` | - |
| `configure_convai_actions` | - |
| `get_convai_info` | - |
| `create_inworld_character` | - |
| `configure_inworld_settings` | - |
| `configure_inworld_scene` | - |
| `start_inworld_session` | - |
| `stop_inworld_session` | - |
| `send_message_to_character` | - |
| `get_character_emotion` | - |
| `get_character_goals` | - |
| `trigger_inworld_event` | - |
| `get_inworld_info` | - |
| `configure_audio2face` | - |
| `process_audio_to_blendshapes` | - |
| `configure_blendshape_mapping` | - |
| `start_audio2face_stream` | - |
| `stop_audio2face_stream` | - |
| `get_audio2face_status` | - |
| `configure_ace_emotions` | - |
| `get_ace_info` | - |
| `get_ai_npc_info` | - |
| `list_available_ai_backends` | - |
| `ai_assistant_query` | - |
| `ai_assistant_explain_feature` | - |
| `ai_assistant_suggest_fix` | - |
| `configure_state_tree_node` | - |
| `debug_behavior_tree` | - |
| `query_eqs_results` | - |
| `configure_mass_ai_fragment` | - |
| `spawn_mass_ai_entities` | - |
| `get_ai_perception_data` | - |
| `configure_smart_object` | - |

---

## manage_widget_authoring

**Category:** utility

UMG widgets: buttons, text, sliders. Layouts, bindings, HUDs.

### Actions

| Action | Description |
|--------|-------------|
| `create_widget_blueprint` | - |
| `set_widget_parent_class` | - |
| `add_canvas_panel` | - |
| `add_horizontal_box` | - |
| `add_vertical_box` | - |
| `add_overlay` | - |
| `add_grid_panel` | - |
| `add_uniform_grid` | - |
| `add_wrap_box` | - |
| `add_scroll_box` | - |
| `add_size_box` | - |
| `add_scale_box` | - |
| `add_border` | - |
| `add_text_block` | - |
| `add_rich_text_block` | - |
| `add_image` | - |
| `add_button` | - |
| `add_check_box` | - |
| `add_slider` | - |
| `add_progress_bar` | - |
| `add_text_input` | - |
| `add_combo_box` | - |
| `add_spin_box` | - |
| `add_list_view` | - |
| `add_tree_view` | - |
| `set_anchor` | - |
| `set_alignment` | - |
| `set_position` | - |
| `set_size` | - |
| `set_padding` | - |
| `set_z_order` | - |
| `set_render_transform` | - |
| `set_visibility` | - |
| `set_style` | - |
| `set_clipping` | - |
| `create_property_binding` | - |
| `bind_text` | - |
| `bind_visibility` | - |
| `bind_color` | - |
| `bind_enabled` | - |
| `bind_on_clicked` | - |
| `bind_on_hovered` | - |
| `bind_on_value_changed` | - |
| `create_widget_animation` | - |
| `add_animation_track` | - |
| `add_animation_keyframe` | - |
| `set_animation_loop` | - |
| `create_main_menu` | - |
| `create_pause_menu` | - |
| `create_settings_menu` | - |
| `create_loading_screen` | - |
| `create_hud_widget` | - |
| `add_health_bar` | - |
| `add_ammo_counter` | - |
| `add_minimap` | - |
| `add_crosshair` | - |
| `add_compass` | - |
| `add_interaction_prompt` | - |
| `add_objective_tracker` | - |
| `add_damage_indicator` | - |
| `create_inventory_ui` | - |
| `create_dialog_widget` | - |
| `create_radial_menu` | - |
| `get_widget_info` | - |
| `preview_widget` | - |
| `create_widget_template` | - |
| `configure_widget_binding_batch` | - |
| `create_widget_animation_advanced` | - |
| `configure_widget_navigation` | - |
| `validate_widget_accessibility` | - |
| `create_hud_layout` | - |
| `configure_safe_zone` | - |
| `batch_localize_widgets` | - |

---

## manage_networking

**Category:** utility

Replication, RPCs, prediction, sessions; GameModes, teams.

### Actions

| Action | Description |
|--------|-------------|
| `set_property_replicated` | - |
| `set_replication_condition` | - |
| `configure_net_update_frequency` | - |
| `configure_net_priority` | - |
| `set_net_dormancy` | - |
| `configure_replication_graph` | - |
| `create_rpc_function` | - |
| `configure_rpc_validation` | - |
| `set_rpc_reliability` | - |
| `set_owner` | - |
| `set_autonomous_proxy` | - |
| `check_has_authority` | - |
| `check_is_locally_controlled` | - |
| `configure_net_cull_distance` | - |
| `set_always_relevant` | - |
| `set_only_relevant_to_owner` | - |
| `configure_net_serialization` | - |
| `set_replicated_using` | - |
| `configure_push_model` | - |
| `configure_client_prediction` | - |
| `configure_server_correction` | - |
| `add_network_prediction_data` | - |
| `configure_movement_prediction` | - |
| `configure_net_driver` | - |
| `set_net_role` | - |
| `configure_replicated_movement` | - |
| `get_networking_info` | - |
| `debug_replication_graph` | - |
| `configure_net_relevancy` | - |
| `get_rpc_statistics` | - |
| `configure_prediction_settings` | - |
| `simulate_network_conditions` | - |
| `get_session_players` | - |
| `configure_team_settings` | - |
| `send_server_rpc` | - |
| `get_net_role_info` | - |
| `configure_dormancy` | - |
| `configure_local_session_settings` | - |
| `configure_session_interface` | - |
| `configure_split_screen` | - |
| `set_split_screen_type` | - |
| `add_local_player` | - |
| `remove_local_player` | - |
| `configure_lan_play` | - |
| `host_lan_server` | - |
| `join_lan_server` | - |
| `enable_voice_chat` | - |
| `configure_voice_settings` | - |
| `set_voice_channel` | - |
| `mute_player` | - |
| `set_voice_attenuation` | - |
| `configure_push_to_talk` | - |
| `get_sessions_info` | - |
| `create_game_mode` | - |
| `create_game_state` | - |
| `create_player_controller` | - |
| `create_player_state` | - |
| `create_game_instance` | - |
| `create_hud_class` | - |
| `set_default_pawn_class` | - |
| `set_player_controller_class` | - |
| `set_game_state_class` | - |
| `set_player_state_class` | - |
| `configure_game_rules` | - |
| `setup_match_states` | - |
| `configure_round_system` | - |
| `configure_team_system` | - |
| `configure_scoring_system` | - |
| `configure_spawn_system` | - |
| `configure_player_start` | - |
| `set_respawn_rules` | - |
| `configure_spectating` | - |
| `get_game_framework_info` | - |

---

## manage_volumes

**Category:** world

Volumes (trigger, physics, audio, nav) and splines (meshes).

### Actions

| Action | Description |
|--------|-------------|
| `create_trigger_volume` | - |
| `create_trigger_box` | - |
| `create_trigger_sphere` | - |
| `create_trigger_capsule` | - |
| `create_blocking_volume` | - |
| `create_kill_z_volume` | - |
| `create_pain_causing_volume` | - |
| `create_physics_volume` | - |
| `create_audio_volume` | - |
| `create_reverb_volume` | - |
| `create_cull_distance_volume` | - |
| `create_precomputed_visibility_volume` | - |
| `create_lightmass_importance_volume` | - |
| `create_nav_mesh_bounds_volume` | - |
| `create_nav_modifier_volume` | - |
| `create_camera_blocking_volume` | - |
| `set_volume_extent` | - |
| `set_volume_properties` | - |
| `get_volumes_info` | - |
| `create_spline_actor` | - |
| `add_spline_point` | - |
| `remove_spline_point` | - |
| `set_spline_point_position` | - |
| `set_spline_point_tangents` | - |
| `set_spline_point_rotation` | - |
| `set_spline_point_scale` | - |
| `set_spline_type` | - |
| `create_spline_mesh_component` | - |
| `set_spline_mesh_asset` | - |
| `configure_spline_mesh_axis` | - |
| `set_spline_mesh_material` | - |
| `scatter_meshes_along_spline` | - |
| `configure_mesh_spacing` | - |
| `configure_mesh_randomization` | - |
| `create_road_spline` | - |
| `create_river_spline` | - |
| `create_fence_spline` | - |
| `create_wall_spline` | - |
| `create_cable_spline` | - |
| `create_pipe_spline` | - |
| `get_splines_info` | - |

---

## manage_data

**Category:** utility

Data assets, tables, save games, tags, config; modding/PAK/UGC.

### Actions

| Action | Description |
|--------|-------------|
| `create_data_asset` | - |
| `create_primary_data_asset` | - |
| `get_data_asset_info` | - |
| `set_data_asset_property` | - |
| `create_data_table` | - |
| `add_data_table_row` | - |
| `remove_data_table_row` | - |
| `get_data_table_row` | - |
| `get_data_table_rows` | - |
| `import_data_table_csv` | - |
| `export_data_table_csv` | - |
| `empty_data_table` | - |
| `create_curve_table` | - |
| `add_curve_row` | - |
| `get_curve_value` | - |
| `import_curve_table_csv` | - |
| `export_curve_table_csv` | - |
| `create_save_game_blueprint` | - |
| `save_game_to_slot` | - |
| `load_game_from_slot` | - |
| `delete_save_slot` | - |
| `does_save_exist` | - |
| `get_save_slot_names` | - |
| `create_gameplay_tag` | - |
| `add_native_gameplay_tag` | - |
| `request_gameplay_tag` | - |
| `check_tag_match` | - |
| `create_tag_container` | - |
| `add_tag_to_container` | - |
| `remove_tag_from_container` | - |
| `has_tag` | - |
| `get_all_gameplay_tags` | - |
| `read_config_value` | - |
| `write_config_value` | - |
| `get_config_section` | - |
| `flush_config` | - |
| `reload_config` | - |
| `configure_mod_loading_paths` | - |
| `scan_for_mod_paks` | - |
| `load_mod_pak` | - |
| `unload_mod_pak` | - |
| `validate_mod_pak` | - |
| `configure_mod_load_order` | - |
| `list_installed_mods` | - |
| `enable_mod` | - |
| `disable_mod` | - |
| `check_mod_compatibility` | - |
| `get_mod_info` | - |
| `configure_asset_override_paths` | - |
| `register_mod_asset_redirect` | - |
| `restore_original_asset` | - |
| `list_asset_overrides` | - |
| `export_moddable_headers` | - |
| `create_mod_template_project` | - |
| `configure_exposed_classes` | - |
| `get_sdk_config` | - |
| `configure_mod_sandbox` | - |
| `set_allowed_mod_operations` | - |
| `validate_mod_content` | - |
| `get_security_config` | - |
| `get_modding_info` | - |
| `reset_mod_system` | - |

---

## manage_build

**Category:** utility

UBT, cook/package, plugins, DDC; tests, profiling, validation.

### Actions

| Action | Description |
|--------|-------------|
| `run_ubt` | - |
| `generate_project_files` | - |
| `compile_shaders` | - |
| `cook_content` | - |
| `package_project` | - |
| `configure_build_settings` | - |
| `get_build_info` | - |
| `configure_platform` | - |
| `get_platform_settings` | - |
| `get_target_platforms` | - |
| `validate_assets` | - |
| `audit_assets` | - |
| `get_asset_size_info` | - |
| `get_asset_references` | - |
| `configure_chunking` | - |
| `create_pak_file` | - |
| `configure_encryption` | - |
| `list_plugins` | - |
| `enable_plugin` | - |
| `disable_plugin` | - |
| `get_plugin_info` | - |
| `clear_ddc` | - |
| `get_ddc_stats` | - |
| `configure_ddc` | - |
| `list_tests` | - |
| `run_tests` | - |
| `run_test` | - |
| `get_test_results` | - |
| `get_test_info` | - |
| `list_functional_tests` | - |
| `run_functional_test` | - |
| `get_functional_test_results` | - |
| `start_trace` | - |
| `stop_trace` | - |
| `get_trace_status` | - |
| `enable_visual_logger` | - |
| `disable_visual_logger` | - |
| `get_visual_logger_status` | - |
| `start_stats_capture` | - |
| `stop_stats_capture` | - |
| `get_memory_report` | - |
| `get_performance_stats` | - |
| `validate_asset` | - |
| `validate_assets_in_path` | - |
| `validate_blueprint` | - |
| `check_map_errors` | - |
| `fix_redirectors` | - |
| `get_redirectors` | - |

---

## manage_editor_utilities

Editor modes, content browser, selection, collision, subsystems.

### Actions

| Action | Description |
|--------|-------------|
| `set_editor_mode` | - |
| `configure_editor_preferences` | - |
| `set_grid_settings` | - |
| `set_snap_settings` | - |
| `navigate_to_path` | - |
| `sync_to_asset` | - |
| `create_collection` | - |
| `add_to_collection` | - |
| `show_in_explorer` | - |
| `select_actor` | - |
| `select_actors_by_class` | - |
| `select_actors_by_tag` | - |
| `deselect_all` | - |
| `group_actors` | - |
| `ungroup_actors` | - |
| `get_selected_actors` | - |
| `create_collision_channel` | - |
| `create_collision_profile` | - |
| `configure_channel_responses` | - |
| `get_collision_info` | - |
| `create_physical_material` | - |
| `set_friction` | - |
| `set_restitution` | - |
| `configure_surface_type` | - |
| `get_physical_material_info` | - |
| `create_game_instance_subsystem` | - |
| `create_world_subsystem` | - |
| `create_local_player_subsystem` | - |
| `get_subsystem_info` | - |
| `set_timer` | - |
| `clear_timer` | - |
| `clear_all_timers` | - |
| `get_active_timers` | - |
| `create_event_dispatcher` | - |
| `bind_to_event` | - |
| `unbind_from_event` | - |
| `broadcast_event` | - |
| `create_blueprint_interface` | - |
| `begin_transaction` | - |
| `end_transaction` | - |
| `cancel_transaction` | - |
| `undo` | - |
| `redo` | - |
| `get_transaction_history` | - |
| `get_editor_utilities_info` | - |

---

## manage_gameplay_systems

**Category:** gameplay

Targeting, checkpoints, objectives, photo mode, dialogue, HLOD.

### Actions

| Action | Description |
|--------|-------------|
| `create_targeting_component` | - |
| `configure_lock_on_target` | - |
| `configure_aim_assist` | - |
| `create_checkpoint_actor` | - |
| `save_checkpoint` | - |
| `load_checkpoint` | - |
| `create_objective` | - |
| `set_objective_state` | - |
| `configure_objective_markers` | - |
| `create_world_marker` | - |
| `create_ping_system` | - |
| `configure_marker_widget` | - |
| `enable_photo_mode` | - |
| `configure_photo_mode_camera` | - |
| `take_photo_mode_screenshot` | - |
| `create_quest_data_asset` | - |
| `create_dialogue_tree` | - |
| `add_dialogue_node` | - |
| `play_dialogue` | - |
| `create_instanced_static_mesh_component` | - |
| `create_hierarchical_instanced_static_mesh` | - |
| `add_instance` | - |
| `remove_instance` | - |
| `get_instance_count` | - |
| `create_hlod_layer` | - |
| `configure_hlod_settings` | - |
| `build_hlod` | - |
| `assign_actor_to_hlod` | - |
| `create_string_table` | - |
| `add_string_entry` | - |
| `get_string_entry` | - |
| `import_localization` | - |
| `export_localization` | - |
| `set_culture` | - |
| `get_available_cultures` | - |
| `create_device_profile` | - |
| `configure_scalability_group` | - |
| `set_quality_level` | - |
| `get_scalability_settings` | - |
| `set_resolution_scale` | - |
| `get_gameplay_systems_info` | - |
| `create_objective_chain` | - |
| `configure_checkpoint_data` | - |
| `create_dialogue_node` | - |
| `configure_targeting_priority` | - |
| `configure_localization_entry` | - |
| `create_quest_stage` | - |
| `configure_minimap_icon` | - |
| `set_game_state` | - |
| `configure_save_system` | - |

---

## manage_gameplay_primitives

**Category:** gameplay

Universal gameplay building blocks: state machines, values, factions, zones, conditions, spawners.

### Actions

| Action | Description |
|--------|-------------|
| `create_value_tracker` | - |
| `modify_value` | - |
| `set_value` | - |
| `get_value` | - |
| `add_value_threshold` | - |
| `configure_value_decay` | - |
| `configure_value_regen` | - |
| `pause_value_changes` | - |
| `create_actor_state_machine` | - |
| `add_actor_state` | - |
| `add_actor_state_transition` | - |
| `set_actor_state` | - |
| `get_actor_state` | - |
| `configure_state_timer` | - |
| `create_faction` | - |
| `set_faction_relationship` | - |
| `assign_to_faction` | - |
| `get_faction` | - |
| `modify_reputation` | - |
| `get_reputation` | - |
| `add_reputation_threshold` | - |
| `check_faction_relationship` | - |
| `attach_to_socket` | - |
| `detach_from_parent` | - |
| `transfer_control` | - |
| `configure_attachment_rules` | - |
| `get_attached_actors` | - |
| `get_attachment_parent` | - |
| `create_schedule` | - |
| `add_schedule_entry` | - |
| `set_schedule_active` | - |
| `get_current_schedule_entry` | - |
| `skip_to_schedule_entry` | - |
| `create_world_time` | - |
| `set_world_time` | - |
| `get_world_time` | - |
| `set_time_scale` | - |
| `pause_world_time` | - |
| `add_time_event` | - |
| `get_time_period` | - |
| `create_zone` | - |
| `set_zone_property` | - |
| `get_zone_property` | - |
| `get_actor_zone` | - |
| `add_zone_enter_event` | - |
| `add_zone_exit_event` | - |
| `create_condition` | - |
| `create_compound_condition` | - |
| `evaluate_condition` | - |
| `add_condition_listener` | - |
| `add_interactable_component` | - |
| `configure_interaction` | - |
| `set_interaction_enabled` | - |
| `get_nearby_interactables` | - |
| `focus_interaction` | - |
| `execute_interaction` | - |
| `create_spawner` | - |
| `configure_spawner` | - |
| `set_spawner_enabled` | - |
| `configure_spawn_conditions` | - |
| `despawn_managed_actors` | - |
| `get_spawned_count` | - |

---

## manage_character_avatar

**Category:** authoring

MetaHuman, Groom/Hair, Mutable, Ready Player Me avatar systems.

### Actions

| Action | Description |
|--------|-------------|
| `import_metahuman` | - |
| `spawn_metahuman_actor` | - |
| `get_metahuman_component` | - |
| `set_body_type` | - |
| `set_face_parameter` | - |
| `set_skin_tone` | - |
| `set_hair_style` | - |
| `set_eye_color` | - |
| `configure_metahuman_lod` | - |
| `enable_body_correctives` | - |
| `enable_neck_correctives` | - |
| `set_quality_level` | - |
| `configure_face_rig` | - |
| `set_body_part` | - |
| `get_metahuman_info` | - |
| `list_available_presets` | - |
| `apply_preset` | - |
| `export_metahuman_settings` | - |
| `create_groom_asset` | - |
| `import_groom` | - |
| `create_groom_binding` | - |
| `spawn_groom_actor` | - |
| `attach_groom_to_skeletal_mesh` | - |
| `configure_hair_simulation` | - |
| `set_hair_width` | - |
| `set_hair_root_scale` | - |
| `set_hair_tip_scale` | - |
| `set_hair_color` | - |
| `configure_hair_physics` | - |
| `configure_hair_rendering` | - |
| `enable_hair_simulation` | - |
| `get_groom_info` | - |
| `create_customizable_object` | - |
| `compile_customizable_object` | - |
| `create_customizable_instance` | - |
| `set_bool_parameter` | - |
| `set_int_parameter` | - |
| `set_float_parameter` | - |
| `set_color_parameter` | - |
| `set_vector_parameter` | - |
| `set_texture_parameter` | - |
| `set_transform_parameter` | - |
| `set_projector_parameter` | - |
| `update_skeletal_mesh` | - |
| `bake_customizable_instance` | - |
| `get_parameter_info` | - |
| `get_instance_info` | - |
| `spawn_customizable_actor` | - |
| `load_avatar_from_url` | - |
| `load_avatar_from_glb` | - |
| `create_rpm_actor` | - |
| `apply_avatar_to_character` | - |
| `configure_rpm_materials` | - |
| `set_rpm_outfit` | - |
| `get_avatar_metadata` | - |
| `cache_avatar` | - |
| `clear_avatar_cache` | - |
| `create_rpm_animation_blueprint` | - |
| `retarget_rpm_animation` | - |
| `get_rpm_info` | - |

---

## manage_asset_plugins

**Category:** utility

Import plugins (USD, Alembic, glTF, Datasmith, Houdini, Substance).

### Actions

| Action | Description |
|--------|-------------|
| `create_interchange_pipeline` | - |
| `configure_interchange_pipeline` | - |
| `import_with_interchange` | - |
| `import_fbx_with_interchange` | - |
| `import_obj_with_interchange` | - |
| `export_with_interchange` | - |
| `set_interchange_translator` | - |
| `get_interchange_translators` | - |
| `configure_import_asset_type` | - |
| `set_interchange_result_container` | - |
| `get_interchange_import_result` | - |
| `cancel_interchange_import` | - |
| `create_interchange_source_data` | - |
| `set_interchange_pipeline_stack` | - |
| `configure_static_mesh_settings` | - |
| `configure_skeletal_mesh_settings` | - |
| `configure_animation_settings` | - |
| `configure_material_settings` | - |
| `create_usd_stage` | - |
| `open_usd_stage` | - |
| `close_usd_stage` | - |
| `get_usd_stage_info` | - |
| `create_usd_prim` | - |
| `get_usd_prim` | - |
| `set_usd_prim_attribute` | - |
| `get_usd_prim_attribute` | - |
| `add_usd_reference` | - |
| `add_usd_payload` | - |
| `set_usd_variant` | - |
| `create_usd_layer` | - |
| `set_edit_target_layer` | - |
| `save_usd_stage` | - |
| `export_actor_to_usd` | - |
| `export_level_to_usd` | - |
| `export_static_mesh_to_usd` | - |
| `export_skeletal_mesh_to_usd` | - |
| `export_material_to_usd` | - |
| `export_animation_to_usd` | - |
| `enable_usd_live_edit` | - |
| `spawn_usd_stage_actor` | - |
| `configure_usd_asset_cache` | - |
| `get_usd_prim_children` | - |
| `import_alembic_file` | - |
| `import_alembic_static_mesh` | - |
| `import_alembic_skeletal_mesh` | - |
| `import_alembic_geometry_cache` | - |
| `import_alembic_groom` | - |
| `configure_alembic_import_settings` | - |
| `set_alembic_sampling_settings` | - |
| `set_alembic_compression_type` | - |
| `set_alembic_normal_generation` | - |
| `reimport_alembic_asset` | - |
| `get_alembic_info` | - |
| `create_geometry_cache_track` | - |
| `play_geometry_cache` | - |
| `set_geometry_cache_time` | - |
| `export_to_alembic` | - |
| `import_gltf` | - |
| `import_glb` | - |
| `import_gltf_static_mesh` | - |
| `import_gltf_skeletal_mesh` | - |
| `export_to_gltf` | - |
| `export_to_glb` | - |
| `export_level_to_gltf` | - |
| `export_actor_to_gltf` | - |
| `configure_gltf_export_options` | - |
| `set_gltf_export_scale` | - |
| `set_gltf_texture_format` | - |
| `set_draco_compression` | - |
| `export_material_to_gltf` | - |
| `export_animation_to_gltf` | - |
| `get_gltf_export_messages` | - |
| `configure_gltf_material_baking` | - |
| `import_datasmith_file` | - |
| `import_datasmith_cad` | - |
| `import_datasmith_revit` | - |
| `import_datasmith_sketchup` | - |
| `import_datasmith_3dsmax` | - |
| `import_datasmith_rhino` | - |
| `import_datasmith_solidworks` | - |
| `import_datasmith_archicad` | - |
| `configure_datasmith_import_options` | - |
| `set_datasmith_tessellation_quality` | - |
| `reimport_datasmith_scene` | - |
| `get_datasmith_scene_info` | - |
| `update_datasmith_scene` | - |
| `configure_datasmith_lightmap` | - |
| `create_datasmith_runtime_actor` | - |
| `configure_datasmith_materials` | - |
| `export_datasmith_scene` | - |
| `sync_datasmith_changes` | - |
| `import_speedtree_model` | - |
| `import_speedtree_9` | - |
| `import_speedtree_atlas` | - |
| `configure_speedtree_wind` | - |
| `set_speedtree_wind_type` | - |
| `set_speedtree_wind_speed` | - |
| `configure_speedtree_lod` | - |
| `set_speedtree_lod_distances` | - |
| `set_speedtree_lod_transition` | - |
| `create_speedtree_material` | - |
| `configure_speedtree_collision` | - |
| `get_speedtree_info` | - |
| `connect_to_bridge` | - |
| `disconnect_bridge` | - |
| `get_bridge_status` | - |
| `import_megascan_surface` | - |
| `import_megascan_3d_asset` | - |
| `import_megascan_3d_plant` | - |
| `import_megascan_decal` | - |
| `import_megascan_atlas` | - |
| `import_megascan_brush` | - |
| `search_fab_assets` | - |
| `download_fab_asset` | - |
| `configure_megascan_import_settings` | - |
| `import_hda` | - |
| `instantiate_hda` | - |
| `spawn_hda_actor` | - |
| `get_hda_parameters` | - |
| `set_hda_float_parameter` | - |
| `set_hda_int_parameter` | - |
| `set_hda_bool_parameter` | - |
| `set_hda_string_parameter` | - |
| `set_hda_color_parameter` | - |
| `set_hda_vector_parameter` | - |
| `set_hda_ramp_parameter` | - |
| `set_hda_multi_parameter` | - |
| `cook_hda` | - |
| `bake_hda_to_actors` | - |
| `bake_hda_to_blueprint` | - |
| `configure_hda_input` | - |
| `set_hda_world_input` | - |
| `set_hda_geometry_input` | - |
| `set_hda_curve_input` | - |
| `get_hda_outputs` | - |
| `get_hda_cook_status` | - |
| `connect_to_houdini_session` | - |
| `import_sbsar_file` | - |
| `create_substance_instance` | - |
| `get_substance_parameters` | - |
| `set_substance_float_parameter` | - |
| `set_substance_int_parameter` | - |
| `set_substance_bool_parameter` | - |
| `set_substance_color_parameter` | - |
| `set_substance_string_parameter` | - |
| `set_substance_image_input` | - |
| `render_substance_textures` | - |
| `get_substance_outputs` | - |
| `create_material_from_substance` | - |
| `apply_substance_to_material` | - |
| `configure_substance_output_size` | - |
| `randomize_substance_seed` | - |
| `export_substance_textures` | - |
| `reimport_sbsar` | - |
| `get_substance_graph_info` | - |
| `set_substance_output_format` | - |
| `batch_render_substances` | - |
| `get_asset_plugins_info` | - |
| `util_execute_python_script` | - |
| `util_execute_python_file` | - |
| `util_execute_python_command` | - |
| `util_configure_python_paths` | - |
| `util_add_python_path` | - |
| `util_remove_python_path` | - |
| `util_get_python_paths` | - |
| `util_create_python_editor_utility` | - |
| `util_run_startup_scripts` | - |
| `util_get_python_output` | - |
| `util_clear_python_output` | - |
| `util_is_python_available` | - |
| `util_get_python_version` | - |
| `util_reload_python_module` | - |
| `util_get_python_info` | - |
| `util_create_editor_utility_widget` | - |
| `util_create_editor_utility_blueprint` | - |
| `util_add_menu_entry` | - |
| `util_remove_menu_entry` | - |
| `util_add_toolbar_button` | - |
| `util_remove_toolbar_button` | - |
| `util_register_editor_command` | - |
| `util_unregister_editor_command` | - |
| `util_execute_editor_command` | - |
| `util_create_blutility_action` | - |
| `util_run_editor_utility` | - |
| `util_get_editor_scripting_info` | - |
| `util_activate_modeling_tool` | - |
| `util_deactivate_modeling_tool` | - |
| `util_get_active_tool` | - |
| `util_select_mesh_elements` | - |
| `util_clear_mesh_selection` | - |
| `util_get_mesh_selection` | - |
| `util_set_sculpt_brush` | - |
| `util_configure_sculpt_brush` | - |
| `util_execute_sculpt_stroke` | - |
| `util_apply_mesh_operation` | - |
| `util_undo_mesh_operation` | - |
| `util_accept_tool_result` | - |
| `util_cancel_tool` | - |
| `util_set_tool_property` | - |
| `util_get_tool_properties` | - |
| `util_list_available_tools` | - |
| `util_enter_modeling_mode` | - |
| `util_get_modeling_tools_info` | - |
| `util_create_sprite` | - |
| `util_create_flipbook` | - |
| `util_add_flipbook_keyframe` | - |
| `util_create_tile_map` | - |
| `util_create_tile_set` | - |
| `util_set_tile_map_layer` | - |
| `util_spawn_paper_sprite_actor` | - |
| `util_spawn_paper_flipbook_actor` | - |
| `util_configure_sprite_collision` | - |
| `util_configure_sprite_material` | - |
| `util_get_sprite_info` | - |
| `util_get_paper2d_info` | - |
| `util_create_procedural_mesh_component` | - |
| `util_create_mesh_section` | - |
| `util_update_mesh_section` | - |
| `util_clear_mesh_section` | - |
| `util_clear_all_mesh_sections` | - |
| `util_set_mesh_section_visible` | - |
| `util_set_mesh_collision` | - |
| `util_set_mesh_vertices` | - |
| `util_set_mesh_triangles` | - |
| `util_set_mesh_normals` | - |
| `util_set_mesh_uvs` | - |
| `util_set_mesh_colors` | - |
| `util_set_mesh_tangents` | - |
| `util_convert_procedural_to_static_mesh` | - |
| `util_get_procedural_mesh_info` | - |
| `util_create_level_variant_sets` | - |
| `util_create_variant_set` | - |
| `util_delete_variant_set` | - |
| `util_add_variant` | - |
| `util_remove_variant` | - |
| `util_duplicate_variant` | - |
| `util_activate_variant` | - |
| `util_deactivate_variant` | - |
| `util_get_active_variant` | - |
| `util_add_actor_binding` | - |
| `util_remove_actor_binding` | - |
| `util_capture_property` | - |
| `util_configure_variant_dependency` | - |
| `util_export_variant_configuration` | - |
| `util_get_variant_manager_info` | - |
| `util_get_utility_plugins_info` | - |
| `util_list_utility_plugins` | - |
| `util_get_plugin_status` | - |

---

## manage_livelink

**Category:** utility

Live Link motion capture: sources, subjects, presets, face tracking.

### Actions

| Action | Description |
|--------|-------------|
| `add_livelink_source` | - |
| `remove_livelink_source` | - |
| `list_livelink_sources` | - |
| `get_source_status` | - |
| `get_source_type` | - |
| `configure_source_settings` | - |
| `add_messagebus_source` | - |
| `discover_messagebus_sources` | - |
| `remove_all_sources` | - |
| `list_livelink_subjects` | - |
| `get_subject_role` | - |
| `get_subject_state` | - |
| `enable_subject` | - |
| `disable_subject` | - |
| `pause_subject` | - |
| `unpause_subject` | - |
| `clear_subject_frames` | - |
| `get_subject_static_data` | - |
| `get_subject_frame_data` | - |
| `add_virtual_subject` | - |
| `remove_virtual_subject` | - |
| `configure_subject_settings` | - |
| `get_subject_frame_times` | - |
| `get_subjects_by_role` | - |
| `create_livelink_preset` | - |
| `load_livelink_preset` | - |
| `apply_livelink_preset` | - |
| `add_preset_to_client` | - |
| `build_preset_from_client` | - |
| `save_livelink_preset` | - |
| `get_preset_sources` | - |
| `get_preset_subjects` | - |
| `add_livelink_controller` | - |
| `configure_livelink_controller` | - |
| `set_controller_subject` | - |
| `set_controller_role` | - |
| `enable_controller_evaluation` | - |
| `disable_controller_evaluation` | - |
| `set_controlled_component` | - |
| `get_controller_info` | - |
| `configure_livelink_timecode` | - |
| `set_timecode_provider` | - |
| `get_livelink_timecode` | - |
| `configure_time_sync` | - |
| `set_buffer_settings` | - |
| `configure_frame_interpolation` | - |
| `configure_face_source` | - |
| `configure_arkit_mapping` | - |
| `set_face_neutral_pose` | - |
| `get_face_blendshapes` | - |
| `configure_blendshape_remap` | - |
| `apply_face_to_skeletal_mesh` | - |
| `configure_face_retargeting` | - |
| `get_face_tracking_status` | - |
| `configure_skeleton_mapping` | - |
| `create_retarget_asset` | - |
| `configure_bone_mapping` | - |
| `configure_curve_mapping` | - |
| `apply_mocap_to_character` | - |
| `get_skeleton_mapping_info` | - |
| `get_livelink_info` | - |
| `list_available_roles` | - |
| `list_source_factories` | - |
| `force_livelink_tick` | - |

---

## manage_xr

XR (VR/AR/MR) + Virtual Production (nDisplay, Composure, DMX).

### Actions

| Action | Description |
|--------|-------------|
| `get_openxr_info` | - |
| `configure_openxr_settings` | - |
| `set_tracking_origin` | - |
| `get_tracking_origin` | - |
| `create_xr_action_set` | - |
| `add_xr_action` | - |
| `bind_xr_action` | - |
| `get_xr_action_state` | - |
| `trigger_haptic_feedback` | - |
| `stop_haptic_feedback` | - |
| `get_hmd_pose` | - |
| `get_controller_pose` | - |
| `get_hand_tracking_data` | - |
| `enable_hand_tracking` | - |
| `disable_hand_tracking` | - |
| `get_eye_tracking_data` | - |
| `enable_eye_tracking` | - |
| `get_view_configuration` | - |
| `set_render_scale` | - |
| `get_supported_extensions` | - |
| `get_quest_info` | - |
| `configure_quest_settings` | - |
| `enable_passthrough` | - |
| `disable_passthrough` | - |
| `configure_passthrough_style` | - |
| `enable_scene_capture` | - |
| `get_scene_anchors` | - |
| `get_room_layout` | - |
| `enable_quest_hand_tracking` | - |
| `get_quest_hand_pose` | - |
| `enable_quest_face_tracking` | - |
| `get_quest_face_state` | - |
| `enable_quest_eye_tracking` | - |
| `get_quest_eye_gaze` | - |
| `enable_quest_body_tracking` | - |
| `get_quest_body_state` | - |
| `create_spatial_anchor` | - |
| `save_spatial_anchor` | - |
| `load_spatial_anchors` | - |
| `delete_spatial_anchor` | - |
| `configure_guardian_bounds` | - |
| `get_guardian_geometry` | - |
| `get_steamvr_info` | - |
| `configure_steamvr_settings` | - |
| `configure_chaperone_bounds` | - |
| `get_chaperone_geometry` | - |
| `create_steamvr_overlay` | - |
| `set_overlay_texture` | - |
| `show_overlay` | - |
| `hide_overlay` | - |
| `destroy_overlay` | - |
| `get_tracked_device_count` | - |
| `get_tracked_device_info` | - |
| `get_lighthouse_info` | - |
| `trigger_steamvr_haptic` | - |
| `get_steamvr_action_manifest` | - |
| `set_steamvr_action_manifest` | - |
| `enable_steamvr_skeletal_input` | - |
| `get_skeletal_bone_data` | - |
| `configure_steamvr_render` | - |
| `get_arkit_info` | - |
| `configure_arkit_session` | - |
| `start_arkit_session` | - |
| `pause_arkit_session` | - |
| `configure_world_tracking` | - |
| `get_tracked_planes` | - |
| `get_tracked_images` | - |
| `add_reference_image` | - |
| `enable_people_occlusion` | - |
| `disable_people_occlusion` | - |
| `enable_arkit_face_tracking` | - |
| `get_arkit_face_blendshapes` | - |
| `get_arkit_face_geometry` | - |
| `enable_body_tracking` | - |
| `get_body_skeleton` | - |
| `create_arkit_anchor` | - |
| `remove_arkit_anchor` | - |
| `get_light_estimation` | - |
| `enable_scene_reconstruction` | - |
| `get_scene_mesh` | - |
| `perform_raycast` | - |
| `get_camera_intrinsics` | - |
| `get_arcore_info` | - |
| `configure_arcore_session` | - |
| `start_arcore_session` | - |
| `pause_arcore_session` | - |
| `get_arcore_planes` | - |
| `get_arcore_points` | - |
| `create_arcore_anchor` | - |
| `remove_arcore_anchor` | - |
| `enable_depth_api` | - |
| `get_depth_image` | - |
| `enable_geospatial` | - |
| `get_geospatial_pose` | - |
| `create_geospatial_anchor` | - |
| `resolve_cloud_anchor` | - |
| `host_cloud_anchor` | - |
| `enable_arcore_augmented_images` | - |
| `get_arcore_light_estimate` | - |
| `perform_arcore_raycast` | - |
| `get_varjo_info` | - |
| `configure_varjo_settings` | - |
| `enable_varjo_passthrough` | - |
| `disable_varjo_passthrough` | - |
| `configure_varjo_depth_test` | - |
| `enable_varjo_eye_tracking` | - |
| `get_varjo_gaze_data` | - |
| `calibrate_varjo_eye_tracking` | - |
| `enable_foveated_rendering` | - |
| `configure_foveated_rendering` | - |
| `enable_varjo_mixed_reality` | - |
| `configure_varjo_chroma_key` | - |
| `get_varjo_camera_intrinsics` | - |
| `enable_varjo_depth_estimation` | - |
| `get_varjo_environment_cubemap` | - |
| `configure_varjo_markers` | - |
| `get_hololens_info` | - |
| `configure_hololens_settings` | - |
| `enable_spatial_mapping` | - |
| `disable_spatial_mapping` | - |
| `get_spatial_mesh` | - |
| `configure_spatial_mapping_quality` | - |
| `enable_scene_understanding` | - |
| `get_scene_objects` | - |
| `enable_qr_tracking` | - |
| `get_tracked_qr_codes` | - |
| `create_world_anchor` | - |
| `save_world_anchor` | - |
| `load_world_anchors` | - |
| `enable_hololens_hand_tracking` | - |
| `get_hololens_hand_mesh` | - |
| `enable_hololens_eye_tracking` | - |
| `get_hololens_gaze_ray` | - |
| `register_voice_command` | - |
| `unregister_voice_command` | - |
| `get_registered_voice_commands` | - |
| `get_xr_system_info` | - |
| `list_xr_devices` | - |
| `set_xr_device_priority` | - |
| `reset_xr_orientation` | - |
| `configure_xr_spectator` | - |
| `get_xr_runtime_name` | - |
| `vp_create_ndisplay_config` | - |
| `vp_add_cluster_node` | - |
| `vp_remove_cluster_node` | - |
| `vp_add_viewport` | - |
| `vp_remove_viewport` | - |
| `vp_set_viewport_camera` | - |
| `vp_configure_viewport_region` | - |
| `vp_set_projection_policy` | - |
| `vp_configure_warp_blend` | - |
| `vp_list_cluster_nodes` | - |
| `vp_create_led_wall` | - |
| `vp_configure_led_wall_size` | - |
| `vp_configure_icvfx_camera` | - |
| `vp_add_icvfx_camera` | - |
| `vp_remove_icvfx_camera` | - |
| `vp_configure_inner_frustum` | - |
| `vp_configure_outer_viewport` | - |
| `vp_set_chromakey_settings` | - |
| `vp_configure_light_cards` | - |
| `vp_set_stage_settings` | - |
| `vp_set_sync_policy` | - |
| `vp_configure_genlock` | - |
| `vp_set_primary_node` | - |
| `vp_configure_network_settings` | - |
| `vp_get_ndisplay_info` | - |
| `vp_create_composure_element` | - |
| `vp_delete_composure_element` | - |
| `vp_add_composure_layer` | - |
| `vp_remove_composure_layer` | - |
| `vp_attach_child_layer` | - |
| `vp_detach_child_layer` | - |
| `vp_add_input_pass` | - |
| `vp_add_transform_pass` | - |
| `vp_add_output_pass` | - |
| `vp_configure_chroma_keyer` | - |
| `vp_bind_render_target` | - |
| `vp_get_composure_info` | - |
| `vp_create_ocio_config` | - |
| `vp_load_ocio_config` | - |
| `vp_get_ocio_colorspaces` | - |
| `vp_get_ocio_displays` | - |
| `vp_set_display_view` | - |
| `vp_add_colorspace_transform` | - |
| `vp_apply_ocio_look` | - |
| `vp_configure_viewport_ocio` | - |
| `vp_set_ocio_working_colorspace` | - |
| `vp_get_ocio_info` | - |
| `vp_create_remote_control_preset` | - |
| `vp_load_remote_control_preset` | - |
| `vp_expose_property` | - |
| `vp_unexpose_property` | - |
| `vp_expose_function` | - |
| `vp_create_controller` | - |
| `vp_bind_controller` | - |
| `vp_get_exposed_properties` | - |
| `vp_set_exposed_property_value` | - |
| `vp_get_exposed_property_value` | - |
| `vp_start_web_server` | - |
| `vp_stop_web_server` | - |
| `vp_get_web_server_status` | - |
| `vp_create_layout_group` | - |
| `vp_get_remote_control_info` | - |
| `vp_create_dmx_library` | - |
| `vp_import_gdtf` | - |
| `vp_create_fixture_type` | - |
| `vp_add_fixture_mode` | - |
| `vp_add_fixture_function` | - |
| `vp_create_fixture_patch` | - |
| `vp_assign_fixture_to_universe` | - |
| `vp_configure_dmx_port` | - |
| `vp_create_artnet_port` | - |
| `vp_create_sacn_port` | - |
| `vp_send_dmx` | - |
| `vp_receive_dmx` | - |
| `vp_set_fixture_channel_value` | - |
| `vp_get_fixture_channel_value` | - |
| `vp_add_dmx_component` | - |
| `vp_configure_dmx_component` | - |
| `vp_list_dmx_universes` | - |
| `vp_list_dmx_fixtures` | - |
| `vp_create_dmx_sequencer_track` | - |
| `vp_get_dmx_info` | - |
| `vp_create_osc_server` | - |
| `vp_stop_osc_server` | - |
| `vp_create_osc_client` | - |
| `send_osc_message` | - |
| `send_osc_bundle` | - |
| `bind_osc_address` | - |
| `unbind_osc_address` | - |
| `bind_osc_to_property` | - |
| `list_osc_servers` | - |
| `list_osc_clients` | - |
| `configure_osc_dispatcher` | - |
| `get_osc_info` | - |
| `list_midi_devices` | - |
| `open_midi_input` | - |
| `close_midi_input` | - |
| `open_midi_output` | - |
| `close_midi_output` | - |
| `send_midi_note_on` | - |
| `send_midi_note_off` | - |
| `send_midi_cc` | - |
| `send_midi_pitch_bend` | - |
| `send_midi_program_change` | - |
| `bind_midi_to_property` | - |
| `unbind_midi` | - |
| `configure_midi_learn` | - |
| `add_midi_device_component` | - |
| `get_midi_info` | - |
| `create_timecode_provider` | - |
| `set_timecode_provider` | - |
| `get_current_timecode` | - |
| `set_frame_rate` | - |
| `configure_ltc_timecode` | - |
| `configure_aja_timecode` | - |
| `configure_blackmagic_timecode` | - |
| `configure_system_time_timecode` | - |
| `enable_timecode_genlock` | - |
| `disable_timecode_genlock` | - |
| `set_custom_timestep` | - |
| `configure_genlock_source` | - |
| `get_timecode_provider_status` | - |
| `synchronize_timecode` | - |
| `create_timecode_synchronizer` | - |
| `add_timecode_source` | - |
| `list_timecode_providers` | - |
| `get_timecode_info` | - |
| `get_virtual_production_info` | - |
| `list_active_vp_sessions` | - |
| `reset_vp_state` | - |

---

## manage_accessibility

**Category:** utility

Accessibility: colorblind, subtitles, audio, motor, cognitive.

### Actions

| Action | Description |
|--------|-------------|
| `create_colorblind_filter` | - |
| `configure_colorblind_mode` | - |
| `set_colorblind_severity` | - |
| `configure_high_contrast_mode` | - |
| `set_high_contrast_colors` | - |
| `set_ui_scale` | - |
| `configure_text_to_speech` | - |
| `set_font_size` | - |
| `configure_screen_reader` | - |
| `set_visual_accessibility_preset` | - |
| `create_subtitle_widget` | - |
| `configure_subtitle_style` | - |
| `set_subtitle_font_size` | - |
| `configure_subtitle_background` | - |
| `configure_speaker_identification` | - |
| `add_directional_indicators` | - |
| `configure_subtitle_timing` | - |
| `set_subtitle_preset` | - |
| `configure_mono_audio` | - |
| `configure_audio_visualization` | - |
| `create_sound_indicator_widget` | - |
| `configure_visual_sound_cues` | - |
| `set_audio_ducking` | - |
| `configure_screen_narrator` | - |
| `set_audio_balance` | - |
| `set_audio_accessibility_preset` | - |
| `configure_control_remapping` | - |
| `create_control_remapping_ui` | - |
| `configure_hold_vs_toggle` | - |
| `configure_auto_aim_strength` | - |
| `configure_one_handed_mode` | - |
| `set_input_timing_tolerance` | - |
| `configure_button_holds` | - |
| `configure_quick_time_events` | - |
| `set_cursor_size` | - |
| `set_motor_accessibility_preset` | - |
| `configure_difficulty_presets` | - |
| `configure_objective_reminders` | - |
| `configure_navigation_assistance` | - |
| `configure_motion_sickness_options` | - |
| `set_game_speed` | - |
| `configure_tutorial_options` | - |
| `configure_ui_simplification` | - |
| `set_cognitive_accessibility_preset` | - |
| `create_accessibility_preset` | - |
| `apply_accessibility_preset` | - |
| `export_accessibility_settings` | - |
| `import_accessibility_settings` | - |
| `get_accessibility_info` | - |
| `reset_accessibility_defaults` | - |

---

## manage_ui

**Category:** utility

Runtime UI management: spawn widgets, hierarchy, viewport control.

### Actions

| Action | Description |
|--------|-------------|
| `create_widget` | - |
| `remove_widget_from_viewport` | - |
| `get_all_widgets` | - |
| `get_widget_hierarchy` | - |
| `set_input_mode` | - |
| `show_mouse_cursor` | - |
| `set_widget_visibility` | - |
| `create_hud` | - |
| `set_widget_text` | - |
| `set_widget_image` | - |
| `add_widget_child` | - |

---

## manage_gameplay_abilities

**Category:** gameplay

Create and configure Gameplay Abilities, Effects, and Ability Tasks.

### Actions

| Action | Description |
|--------|-------------|
| `create_gameplay_ability` | - |
| `set_ability_tags` | - |
| `set_ability_costs` | - |
| `set_ability_cooldown` | - |
| `set_ability_targeting` | - |
| `add_ability_task` | - |
| `set_activation_policy` | - |
| `set_instancing_policy` | - |
| `create_gameplay_effect` | - |
| `set_effect_duration` | - |
| `add_effect_modifier` | - |
| `set_modifier_magnitude` | - |
| `add_effect_execution_calculation` | - |
| `add_effect_cue` | - |
| `set_effect_stacking` | - |
| `set_effect_tags` | - |
| `add_tag_to_asset` | - |
| `get_gas_info` | - |
| `add_ability_system_component` | - |
| `configure_asc` | - |
| `add_attribute` | - |
| `create_attribute_set` | - |
| `set_attribute_base_value` | - |
| `set_attribute_clamping` | - |
| `configure_cue_trigger` | - |
| `set_cue_effects` | - |
| `create_gameplay_cue_notify` | - |
| `test_activate_ability` | - |
| `test_apply_effect` | - |
| `test_get_attribute` | - |
| `test_get_gameplay_tags` | - |

---

## manage_attribute_sets

**Category:** gameplay

Create Blueprint AttributeSets and add Ability System Components.

### Actions

| Action | Description |
|--------|-------------|
| `add_ability_system_component` | - |
| `configure_asc` | - |
| `create_attribute_set` | - |
| `add_attribute` | - |
| `set_attribute_base_value` | - |
| `set_attribute_clamping` | - |

---

## manage_gameplay_cues

**Category:** gameplay

Create and configure Gameplay Cue Notifies (Static/Actor).

### Actions

| Action | Description |
|--------|-------------|
| `create_gameplay_cue_notify` | - |
| `configure_cue_trigger` | - |
| `set_cue_effects` | - |

---

## test_gameplay_abilities

**Category:** gameplay

Runtime testing of GAS: Activate abilities, apply effects, query attributes.

### Actions

| Action | Description |
|--------|-------------|
| `test_activate_ability` | - |
| `test_apply_effect` | - |
| `test_get_attribute` | - |
| `test_get_gameplay_tags` | - |

---
