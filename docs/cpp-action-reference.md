# C++ Action Reference

> Auto-generated from C++ handler files. Do not edit manually.

Generated: 2026-01-21T16:47:49.611Z

## Summary

| Metric | Count |
|--------|-------|
| Handler Files | 74 |
| Total Actions | 2670 |
| Lines of Code | 1,26,088 |

## Table of Contents

- [AssetPlugins](#assetplugins) (158 actions) → manage_asset_plugins
- [Control](#control) (153 actions) → control_actor, control_editor
- [XRPlugins](#xrplugins) (142 actions) → manage_xr
- [MaterialAuthoring](#materialauthoring) (133 actions) → manage_material_authoring
- [VirtualProduction](#virtualproduction) (130 actions) → manage_xr
- [UtilityPlugins](#utilityplugins) (100 actions) → manage_asset_plugins
- [Geometry](#geometry) (83 actions) → manage_geometry
- [AudioMiddleware](#audiomiddleware) (81 actions) → manage_audio
- [PhysicsDestruction](#physicsdestruction) (80 actions) → animation_physics
- [AssetWorkflow](#assetworkflow) (73 actions) → manage_asset
- [SequencerConsolidated](#sequencerconsolidated) (70 actions) → manage_sequence
- [LiveLink](#livelink) (64 actions) → manage_livelink
- [GameplayPrimitives](#gameplayprimitives) (62 actions) → manage_gameplay_primitives
- [Lighting](#lighting) (61 actions) → manage_lighting
- [CharacterAvatar](#characteravatar) (60 actions) → manage_character_avatar
- [MetaSound](#metasound) (60 actions) → manage_asset, manage_audio
- [AI](#ai) (56 actions) → manage_ai
- [Environment](#environment) (55 actions) → build_environment
- [Audio](#audio) (52 actions) → manage_audio
- [Accessibility](#accessibility) (50 actions) → manage_accessibility
- [GameplaySystems](#gameplaysystems) (50 actions) → manage_gameplay_systems
- [EditorUtilities](#editorutilities) (45 actions) → manage_editor_utilities
- [Animation](#animation) (43 actions) → animation_physics
- [Character](#character) (43 actions) → manage_character
- [PCG](#pcg) (41 actions) → manage_level
- [AnimationAuthoring](#animationauthoring) (40 actions) → animation_physics
- [Combat](#combat) (40 actions) → manage_combat
- [GAS](#gas) (38 actions) → manage_combat
- [Data](#data) (37 actions) → manage_data
- [Networking](#networking) (37 actions) → manage_networking
- [NiagaraAuthoring](#niagaraauthoring) (36 actions) → manage_effect
- [LevelStructure](#levelstructure) (35 actions) → manage_level
- [MovieRender](#movierender) (33 actions) → manage_sequence
- [Sequence](#sequence) (33 actions) → manage_sequence
- [PostProcess](#postprocess) (31 actions) → manage_lighting
- [AINPC](#ainpc) (30 actions) → manage_ai
- [AudioAuthoring](#audioauthoring) (30 actions) → manage_audio
- [Skeleton](#skeleton) (29 actions) → manage_skeleton
- [Inventory](#inventory) (27 actions) → manage_character
- [Volume](#volume) (26 actions) → manage_volumes
- [Media](#media) (25 actions) → manage_skeleton
- [Modding](#modding) (25 actions) → manage_data
- [Build](#build) (24 actions) → manage_build
- [Testing](#testing) (24 actions) → manage_build
- [Spline](#spline) (22 actions) → manage_volumes
- [Level](#level) (21 actions) → manage_level
- [Texture](#texture) (21 actions) → manage_material_authoring
- [GameFramework](#gameframework) (20 actions) → manage_networking
- [Performance](#performance) (20 actions) → manage_performance
- [Interaction](#interaction) (17 actions) → manage_character
- [NiagaraAdvanced](#niagaraadvanced) (17 actions) → manage_effect
- [Water](#water) (17 actions) → build_environment
- [Blueprint](#blueprint) (16 actions) → manage_asset
- [Sessions](#sessions) (16 actions) → manage_networking
- [Ui](#ui) (16 actions) → manage_ui
- [BlueprintGraph](#blueprintgraph) (12 actions) → manage_asset
- [ControlRig](#controlrig) (12 actions) → animation_physics
- [Navigation](#navigation) (12 actions) → manage_ai
- [MotionDesign](#motiondesign) (10 actions) → manage_motion_design
- [Weather](#weather) (7 actions) → build_environment
- [BehaviorTree](#behaviortree) (6 actions) → manage_ai
- [MaterialGraph](#materialgraph) (6 actions) → manage_asset
- [Effect](#effect) (5 actions) → manage_effect
- [AssetQuery](#assetquery) (4 actions) → manage_asset
- [Input](#input) (4 actions) → control_editor
- [NiagaraGraph](#niagaragraph) (4 actions) → manage_effect
- [Render](#render) (4 actions) → manage_performance
- [WorldPartition](#worldpartition) (4 actions) → manage_level
- [BlueprintCreation](#blueprintcreation) (3 actions) → manage_asset
- [EditorFunction](#editorfunction) (3 actions) → manage_editor_utilities
- [Log](#log) (2 actions) → control_editor
- [Debug](#debug) (1 actions) → control_editor
- [Insights](#insights) (1 actions) → control_editor
- [Test](#test) (1 actions) → manage_build

---

## AssetPlugins

**File:** `McpAutomationBridge_AssetPluginsHandlers.cpp`

**Lines:** 2,655

**Maps to TS Tool(s):** `manage_asset_plugins`

**Actions (158):**

| Action | Status |
|--------|--------|
| `add_usd_payload` | Implemented |
| `add_usd_reference` | Implemented |
| `apply_substance_to_material` | Implemented |
| `bake_hda_to_actors` | Implemented |
| `bake_hda_to_blueprint` | Implemented |
| `batch_render_substances` | Implemented |
| `cancel_interchange_import` | Implemented |
| `close_usd_stage` | Implemented |
| `configure_alembic_import_settings` | Implemented |
| `configure_animation_settings` | Implemented |
| `configure_datasmith_import_options` | Implemented |
| `configure_datasmith_lightmap` | Implemented |
| `configure_datasmith_materials` | Implemented |
| `configure_gltf_export_options` | Implemented |
| `configure_gltf_material_baking` | Implemented |
| `configure_hda_input` | Implemented |
| `configure_import_asset_type` | Implemented |
| `configure_interchange_pipeline` | Implemented |
| `configure_material_settings` | Implemented |
| `configure_megascan_import_settings` | Implemented |
| `configure_skeletal_mesh_settings` | Implemented |
| `configure_speedtree_collision` | Implemented |
| `configure_speedtree_lod` | Implemented |
| `configure_speedtree_wind` | Implemented |
| `configure_static_mesh_settings` | Implemented |
| `configure_substance_output_size` | Implemented |
| `configure_usd_asset_cache` | Implemented |
| `connect_to_bridge` | Implemented |
| `connect_to_houdini_session` | Implemented |
| `cook_hda` | Implemented |
| `create_datasmith_runtime_actor` | Implemented |
| `create_geometry_cache_track` | Implemented |
| `create_interchange_pipeline` | Implemented |
| `create_interchange_source_data` | Implemented |
| `create_material_from_substance` | Implemented |
| `create_speedtree_material` | Implemented |
| `create_substance_instance` | Implemented |
| `create_usd_layer` | Implemented |
| `create_usd_prim` | Implemented |
| `create_usd_stage` | Implemented |
| `disconnect_bridge` | Implemented |
| `download_fab_asset` | Implemented |
| `enable_usd_live_edit` | Implemented |
| `export_actor_to_gltf` | Implemented |
| `export_actor_to_usd` | Implemented |
| `export_animation_to_gltf` | Implemented |
| `export_animation_to_usd` | Implemented |
| `export_datasmith_scene` | Implemented |
| `export_level_to_gltf` | Implemented |
| `export_level_to_usd` | Implemented |
| `export_material_to_gltf` | Implemented |
| `export_material_to_usd` | Implemented |
| `export_skeletal_mesh_to_usd` | Implemented |
| `export_static_mesh_to_usd` | Implemented |
| `export_substance_textures` | Implemented |
| `export_to_alembic` | Implemented |
| `export_to_glb` | Implemented |
| `export_to_gltf` | Implemented |
| `export_with_interchange` | Implemented |
| `get_alembic_info` | Implemented |
| `get_asset_plugins_info` | Implemented |
| `get_bridge_status` | Implemented |
| `get_datasmith_scene_info` | Implemented |
| `get_gltf_export_messages` | Implemented |
| `get_hda_cook_status` | Implemented |
| `get_hda_outputs` | Implemented |
| `get_hda_parameters` | Implemented |
| `get_interchange_import_result` | Implemented |
| `get_interchange_translators` | Implemented |
| `get_speedtree_info` | Implemented |
| `get_substance_graph_info` | Implemented |
| `get_substance_outputs` | Implemented |
| `get_substance_parameters` | Implemented |
| `get_usd_prim` | Implemented |
| `get_usd_prim_attribute` | Implemented |
| `get_usd_prim_children` | Implemented |
| `get_usd_stage_info` | Implemented |
| `import_alembic_file` | Implemented |
| `import_alembic_geometry_cache` | Implemented |
| `import_alembic_groom` | Implemented |
| `import_alembic_skeletal_mesh` | Implemented |
| `import_alembic_static_mesh` | Implemented |
| `import_datasmith_3dsmax` | Implemented |
| `import_datasmith_archicad` | Implemented |
| `import_datasmith_cad` | Implemented |
| `import_datasmith_file` | Implemented |
| `import_datasmith_revit` | Implemented |
| `import_datasmith_rhino` | Implemented |
| `import_datasmith_sketchup` | Implemented |
| `import_datasmith_solidworks` | Implemented |
| `import_fbx_with_interchange` | Implemented |
| `import_glb` | Implemented |
| `import_gltf` | Implemented |
| `import_gltf_skeletal_mesh` | Implemented |
| `import_gltf_static_mesh` | Implemented |
| `import_hda` | Implemented |
| `import_megascan_3d_asset` | Implemented |
| `import_megascan_3d_plant` | Implemented |
| `import_megascan_atlas` | Implemented |
| `import_megascan_brush` | Implemented |
| `import_megascan_decal` | Implemented |
| `import_megascan_surface` | Implemented |
| `import_obj_with_interchange` | Implemented |
| `import_sbsar_file` | Implemented |
| `import_speedtree_9` | Implemented |
| `import_speedtree_atlas` | Implemented |
| `import_speedtree_model` | Implemented |
| `import_with_interchange` | Implemented |
| `instantiate_hda` | Implemented |
| `open_usd_stage` | Implemented |
| `play_geometry_cache` | Implemented |
| `randomize_substance_seed` | Implemented |
| `reimport_alembic_asset` | Implemented |
| `reimport_datasmith_scene` | Implemented |
| `reimport_sbsar` | Implemented |
| `render_substance_textures` | Implemented |
| `save_usd_stage` | Implemented |
| `search_fab_assets` | Implemented |
| `set_alembic_compression_type` | Implemented |
| `set_alembic_normal_generation` | Implemented |
| `set_alembic_sampling_settings` | Implemented |
| `set_datasmith_tessellation_quality` | Implemented |
| `set_draco_compression` | Implemented |
| `set_edit_target_layer` | Implemented |
| `set_geometry_cache_time` | Implemented |
| `set_gltf_export_scale` | Implemented |
| `set_gltf_texture_format` | Implemented |
| `set_hda_bool_parameter` | Implemented |
| `set_hda_color_parameter` | Implemented |
| `set_hda_curve_input` | Implemented |
| `set_hda_float_parameter` | Implemented |
| `set_hda_geometry_input` | Implemented |
| `set_hda_int_parameter` | Implemented |
| `set_hda_multi_parameter` | Implemented |
| `set_hda_ramp_parameter` | Implemented |
| `set_hda_string_parameter` | Implemented |
| `set_hda_vector_parameter` | Implemented |
| `set_hda_world_input` | Implemented |
| `set_interchange_pipeline_stack` | Implemented |
| `set_interchange_result_container` | Implemented |
| `set_interchange_translator` | Implemented |
| `set_speedtree_lod_distances` | Implemented |
| `set_speedtree_lod_transition` | Implemented |
| `set_speedtree_wind_speed` | Implemented |
| `set_speedtree_wind_type` | Implemented |
| `set_substance_bool_parameter` | Implemented |
| `set_substance_color_parameter` | Implemented |
| `set_substance_float_parameter` | Implemented |
| `set_substance_image_input` | Implemented |
| `set_substance_int_parameter` | Implemented |
| `set_substance_output_format` | Implemented |
| `set_substance_string_parameter` | Implemented |
| `set_usd_prim_attribute` | Implemented |
| `set_usd_variant` | Implemented |
| `spawn_hda_actor` | Implemented |
| `spawn_usd_stage_actor` | Implemented |
| `sync_datasmith_changes` | Implemented |
| `update_datasmith_scene` | Implemented |

---

## Control

**File:** `McpAutomationBridge_ControlHandlers.cpp`

**Lines:** 5,720

**Maps to TS Tool(s):** `control_actor`, `control_editor`

**Actions (153):**

| Action | Status |
|--------|--------|
| `add_component` | Implemented |
| `add_mapping` | Implemented |
| `add_tag` | Implemented |
| `add_widget_child` | Implemented |
| `apply_force` | Implemented |
| `apply_force_to_actor` | Implemented |
| `attach` | Implemented |
| `batch_execute` | Implemented |
| `batch_set_component_properties` | Implemented |
| `batch_substrate_migration` | Implemented |
| `batch_transform` | Implemented |
| `batch_transform_actors` | Implemented |
| `cancel_job` | Implemented |
| `capture_viewport` | Implemented |
| `capture_viewport_sequence` | Implemented |
| `clear_event_subscriptions` | Implemented |
| `clone_component_hierarchy` | Implemented |
| `clone_components` | Implemented |
| `configure_event_channel` | Implemented |
| `configure_megalights` | Implemented |
| `console_command` | Implemented |
| `convert_to_substrate` | Implemented |
| `create_bookmark` | Implemented |
| `create_input_action` | Implemented |
| `create_input_mapping_context` | Implemented |
| `create_snapshot` | Implemented |
| `create_widget` | Implemented |
| `delete` | Implemented |
| `delete_by_tag` | Implemented |
| `delete_object` | Implemented |
| `deserialize_actor_state` | Implemented |
| `detach` | Implemented |
| `detaillighting` | Implemented |
| `duplicate` | Implemented |
| `eject` | Implemented |
| `execute_command` | Implemented |
| `explain_action_parameters` | Implemented |
| `export` | Implemented |
| `find_by_class` | Implemented |
| `find_by_name` | Implemented |
| `find_by_tag` | Implemented |
| `flush_operation_queue` | Implemented |
| `focus_actor` | Implemented |
| `get` | Implemented |
| `get_action_statistics` | Implemented |
| `get_active_jobs` | Implemented |
| `get_actor` | Implemented |
| `get_actor_bounds` | Implemented |
| `get_actor_by_name` | Implemented |
| `get_actor_references` | Implemented |
| `get_actor_transform` | Implemented |
| `get_all_component_properties` | Implemented |
| `get_available_actions` | Implemented |
| `get_bounding_box` | Implemented |
| `get_bridge_health` | Implemented |
| `get_class_hierarchy` | Implemented |
| `get_component_property` | Implemented |
| `get_components` | Implemented |
| `get_event_history` | Implemented |
| `get_job_status` | Implemented |
| `get_last_error_details` | Implemented |
| `get_light_budget_stats` | Implemented |
| `get_metadata` | Implemented |
| `get_operation_history` | Implemented |
| `get_project_settings` | Implemented |
| `get_property` | Implemented |
| `get_selection_info` | Implemented |
| `get_subscribed_events` | Implemented |
| `get_transform` | Implemented |
| `inspect_class` | Implemented |
| `inspect_object` | Implemented |
| `jump_to_bookmark` | Implemented |
| `lightcomplexity` | Implemented |
| `lightingonly` | Implemented |
| `lightmapdensity` | Implemented |
| `list` | Implemented |
| `list_actors` | Implemented |
| `list_objects` | Implemented |
| `lit` | Implemented |
| `lumen_update_scene` | Implemented |
| `merge_actors` | Implemented |
| `open_asset` | Implemented |
| `parallel_execute` | Implemented |
| `pause` | Implemented |
| `play` | Implemented |
| `play_sound` | Implemented |
| `playback_input_session` | Implemented |
| `possess` | Implemented |
| `profile` | Implemented |
| `query_actors_by_predicate` | Implemented |
| `queue_operations` | Implemented |
| `record_input_session` | Implemented |
| `reflectionoverride` | Implemented |
| `remove` | Implemented |
| `remove_mapping` | Implemented |
| `remove_tag` | Implemented |
| `replace_actor_class` | Implemented |
| `restore_snapshot` | Implemented |
| `restore_state` | Implemented |
| `resume` | Implemented |
| `run_tests` | Implemented |
| `run_ubt` | Implemented |
| `screenshot` | Implemented |
| `serialize_actor_state` | Implemented |
| `set_actor_transform` | Implemented |
| `set_actor_visibility` | Implemented |
| `set_blueprint_variables` | Implemented |
| `set_camera` | Implemented |
| `set_camera_fov` | Implemented |
| `set_camera_position` | Implemented |
| `set_component_properties` | Implemented |
| `set_component_property` | Implemented |
| `set_cvar` | Implemented |
| `set_editor_mode` | Implemented |
| `set_fullscreen` | Implemented |
| `set_game_speed` | Implemented |
| `set_preferences` | Implemented |
| `set_project_setting` | Implemented |
| `set_property` | Implemented |
| `set_quality` | Implemented |
| `set_resolution` | Implemented |
| `set_transform` | Implemented |
| `set_view_mode` | Implemented |
| `set_viewport_camera` | Implemented |
| `set_viewport_realtime` | Implemented |
| `set_viewport_resolution` | Implemented |
| `set_visibility` | Implemented |
| `shadercomplexity` | Implemented |
| `show_fps` | Implemented |
| `show_widget` | Implemented |
| `simulate_input` | Implemented |
| `spawn` | Implemented |
| `spawn_blueprint` | Implemented |
| `spawn_category` | Implemented |
| `start_background_job` | Implemented |
| `start_recording` | Implemented |
| `start_session` | Implemented |
| `stationarylightoverlap` | Implemented |
| `step_frame` | Implemented |
| `stop` | Implemented |
| `stop_pie` | Implemented |
| `stop_recording` | Implemented |
| `subscribe` | Implemented |
| `subscribe_to_event` | Implemented |
| `suggest_fix_for_error` | Implemented |
| `toggle_realtime_rendering` | Implemented |
| `unlit` | Implemented |
| `unsubscribe` | Implemented |
| `unsubscribe_from_event` | Implemented |
| `validate_action_input` | Implemented |
| `validate_assets` | Implemented |
| `validate_operation_preconditions` | Implemented |
| `wireframe` | Implemented |

---

## XRPlugins

**File:** `McpAutomationBridge_XRPluginsHandlers.cpp`

**Lines:** 1,973

**Maps to TS Tool(s):** `manage_xr`

**Actions (142):**

| Action | Status |
|--------|--------|
| `add_reference_image` | Implemented |
| `add_xr_action` | Implemented |
| `bind_xr_action` | Implemented |
| `calibrate_varjo_eye_tracking` | Implemented |
| `configure_arcore_session` | Implemented |
| `configure_arkit_session` | Implemented |
| `configure_chaperone_bounds` | Implemented |
| `configure_foveated_rendering` | Implemented |
| `configure_guardian_bounds` | Implemented |
| `configure_hololens_settings` | Implemented |
| `configure_openxr_settings` | Implemented |
| `configure_passthrough_style` | Implemented |
| `configure_quest_settings` | Implemented |
| `configure_spatial_mapping_quality` | Implemented |
| `configure_steamvr_render` | Implemented |
| `configure_steamvr_settings` | Implemented |
| `configure_varjo_chroma_key` | Implemented |
| `configure_varjo_depth_test` | Implemented |
| `configure_varjo_markers` | Implemented |
| `configure_varjo_settings` | Implemented |
| `configure_world_tracking` | Implemented |
| `configure_xr_spectator` | Implemented |
| `create_arcore_anchor` | Implemented |
| `create_arkit_anchor` | Implemented |
| `create_geospatial_anchor` | Implemented |
| `create_spatial_anchor` | Implemented |
| `create_steamvr_overlay` | Implemented |
| `create_world_anchor` | Implemented |
| `create_xr_action_set` | Implemented |
| `delete_spatial_anchor` | Implemented |
| `destroy_overlay` | Implemented |
| `disable_hand_tracking` | Implemented |
| `disable_passthrough` | Implemented |
| `disable_people_occlusion` | Implemented |
| `disable_spatial_mapping` | Implemented |
| `disable_varjo_passthrough` | Implemented |
| `enable_arcore_augmented_images` | Implemented |
| `enable_arkit_face_tracking` | Implemented |
| `enable_body_tracking` | Implemented |
| `enable_depth_api` | Implemented |
| `enable_eye_tracking` | Implemented |
| `enable_foveated_rendering` | Implemented |
| `enable_geospatial` | Implemented |
| `enable_hand_tracking` | Implemented |
| `enable_hololens_eye_tracking` | Implemented |
| `enable_hololens_hand_tracking` | Implemented |
| `enable_passthrough` | Implemented |
| `enable_people_occlusion` | Implemented |
| `enable_qr_tracking` | Implemented |
| `enable_quest_body_tracking` | Implemented |
| `enable_quest_eye_tracking` | Implemented |
| `enable_quest_face_tracking` | Implemented |
| `enable_quest_hand_tracking` | Implemented |
| `enable_scene_capture` | Implemented |
| `enable_scene_reconstruction` | Implemented |
| `enable_scene_understanding` | Implemented |
| `enable_spatial_mapping` | Implemented |
| `enable_steamvr_skeletal_input` | Implemented |
| `enable_varjo_depth_estimation` | Implemented |
| `enable_varjo_eye_tracking` | Implemented |
| `enable_varjo_mixed_reality` | Implemented |
| `enable_varjo_passthrough` | Implemented |
| `get_arcore_info` | Implemented |
| `get_arcore_light_estimate` | Implemented |
| `get_arcore_planes` | Implemented |
| `get_arcore_points` | Implemented |
| `get_arkit_face_blendshapes` | Implemented |
| `get_arkit_face_geometry` | Implemented |
| `get_arkit_info` | Implemented |
| `get_body_skeleton` | Implemented |
| `get_camera_intrinsics` | Implemented |
| `get_chaperone_geometry` | Implemented |
| `get_controller_pose` | Implemented |
| `get_depth_image` | Implemented |
| `get_eye_tracking_data` | Implemented |
| `get_geospatial_pose` | Implemented |
| `get_guardian_geometry` | Implemented |
| `get_hand_tracking_data` | Implemented |
| `get_hmd_pose` | Implemented |
| `get_hololens_gaze_ray` | Implemented |
| `get_hololens_hand_mesh` | Implemented |
| `get_hololens_info` | Implemented |
| `get_light_estimation` | Implemented |
| `get_lighthouse_info` | Implemented |
| `get_openxr_info` | Implemented |
| `get_quest_body_state` | Implemented |
| `get_quest_eye_gaze` | Implemented |
| `get_quest_face_state` | Implemented |
| `get_quest_hand_pose` | Implemented |
| `get_quest_info` | Implemented |
| `get_registered_voice_commands` | Implemented |
| `get_room_layout` | Implemented |
| `get_scene_anchors` | Implemented |
| `get_scene_mesh` | Implemented |
| `get_scene_objects` | Implemented |
| `get_skeletal_bone_data` | Implemented |
| `get_spatial_mesh` | Implemented |
| `get_steamvr_action_manifest` | Implemented |
| `get_steamvr_info` | Implemented |
| `get_supported_extensions` | Implemented |
| `get_tracked_device_count` | Implemented |
| `get_tracked_device_info` | Implemented |
| `get_tracked_images` | Implemented |
| `get_tracked_planes` | Implemented |
| `get_tracked_qr_codes` | Implemented |
| `get_tracking_origin` | Implemented |
| `get_varjo_camera_intrinsics` | Implemented |
| `get_varjo_environment_cubemap` | Implemented |
| `get_varjo_gaze_data` | Implemented |
| `get_varjo_info` | Implemented |
| `get_view_configuration` | Implemented |
| `get_xr_action_state` | Implemented |
| `get_xr_runtime_name` | Implemented |
| `get_xr_system_info` | Implemented |
| `hide_overlay` | Implemented |
| `host_cloud_anchor` | Implemented |
| `list_xr_devices` | Implemented |
| `load_spatial_anchors` | Implemented |
| `load_world_anchors` | Implemented |
| `pause_arcore_session` | Implemented |
| `pause_arkit_session` | Implemented |
| `perform_arcore_raycast` | Implemented |
| `perform_raycast` | Implemented |
| `register_voice_command` | Implemented |
| `remove_arcore_anchor` | Implemented |
| `remove_arkit_anchor` | Implemented |
| `reset_xr_orientation` | Implemented |
| `resolve_cloud_anchor` | Implemented |
| `save_spatial_anchor` | Implemented |
| `save_world_anchor` | Implemented |
| `set_overlay_texture` | Implemented |
| `set_render_scale` | Implemented |
| `set_steamvr_action_manifest` | Implemented |
| `set_tracking_origin` | Implemented |
| `set_xr_device_priority` | Implemented |
| `show_overlay` | Implemented |
| `start_arcore_session` | Implemented |
| `start_arkit_session` | Implemented |
| `stop_haptic_feedback` | Implemented |
| `trigger_haptic_feedback` | Implemented |
| `trigger_steamvr_haptic` | Implemented |
| `unregister_voice_command` | Implemented |

---

## MaterialAuthoring

**File:** `McpAutomationBridge_MaterialAuthoringHandlers.cpp`

**Lines:** 3,369

**Maps to TS Tool(s):** `manage_material_authoring`

**Actions (133):**

| Action | Status |
|--------|--------|
| `Add` | Implemented |
| `Append` | Implemented |
| `Clamp` | Implemented |
| `Divide` | Implemented |
| `Frac` | Implemented |
| `Lerp` | Implemented |
| `Multiply` | Implemented |
| `OneMinus` | Implemented |
| `Power` | Implemented |
| `Subtract` | Implemented |
| `add` | Implemented |
| `add_custom_expression` | Implemented |
| `add_fresnel` | Implemented |
| `add_function_input` | Implemented |
| `add_function_output` | Implemented |
| `add_if` | Implemented |
| `add_landscape_layer` | Implemented |
| `add_material_node` | Implemented |
| `add_material_parameter` | Implemented |
| `add_math_node` | Implemented |
| `add_noise` | Implemented |
| `add_panner` | Implemented |
| `add_pixel_depth` | Implemented |
| `add_reflection_vector` | Implemented |
| `add_rotator` | Implemented |
| `add_scalar_parameter` | Implemented |
| `add_static_switch_parameter` | Implemented |
| `add_switch` | Implemented |
| `add_texture_coordinate` | Implemented |
| `add_texture_sample` | Implemented |
| `add_vector_parameter` | Implemented |
| `add_vertex_normal` | Implemented |
| `add_voronoi` | Implemented |
| `add_world_position` | Implemented |
| `append` | Implemented |
| `appendvector` | Implemented |
| `batch_convert_to_substrate` | Implemented |
| `bool` | Implemented |
| `boolparam` | Implemented |
| `clamp` | Implemented |
| `color` | Implemented |
| `colorparam` | Implemented |
| `compile_material` | Implemented |
| `configure_exposure` | Implemented |
| `configure_landscape_material_layer` | Implemented |
| `configure_layer_blend` | Implemented |
| `configure_material_lod` | Implemented |
| `configure_sss_profile` | Implemented |
| `connect_material_pins` | Implemented |
| `connect_nodes` | Implemented |
| `constant` | Implemented |
| `constant2vector` | Implemented |
| `constant3vector` | Implemented |
| `constant4vector` | Implemented |
| `convert_material_to_substrate` | Implemented |
| `create_decal_material` | Implemented |
| `create_landscape_material` | Implemented |
| `create_material` | Implemented |
| `create_material_expression_template` | Implemented |
| `create_material_function` | Implemented |
| `create_material_instance` | Implemented |
| `create_material_instance_batch` | Implemented |
| `create_post_process_material` | Implemented |
| `create_substrate_material` | Implemented |
| `custom` | Implemented |
| `customexpression` | Implemented |
| `depth` | Implemented |
| `disconnect_nodes` | Implemented |
| `div` | Implemented |
| `divide` | Implemented |
| `export_material_template` | Implemented |
| `float` | Implemented |
| `float2` | Implemented |
| `float3` | Implemented |
| `float4` | Implemented |
| `floatparam` | Implemented |
| `frac` | Implemented |
| `fraction` | Implemented |
| `fresnel` | Implemented |
| `functioncall` | Implemented |
| `get_material_dependencies` | Implemented |
| `get_material_info` | Implemented |
| `get_material_stats` | Implemented |
| `hlsl` | Implemented |
| `if` | Implemented |
| `invert` | Implemented |
| `lerp` | Implemented |
| `linearinterpolate` | Implemented |
| `manage_material_authoring` | Implemented |
| `materialfunctioncall` | Implemented |
| `mul` | Implemented |
| `multiply` | Implemented |
| `noise` | Implemented |
| `oneminus` | Implemented |
| `panner` | Implemented |
| `pixeldepth` | Implemented |
| `pow` | Implemented |
| `power` | Implemented |
| `reflectionvector` | Implemented |
| `reflectionvectorws` | Implemented |
| `remove_material_node` | Implemented |
| `rgb` | Implemented |
| `rgba` | Implemented |
| `rotator` | Implemented |
| `scalar` | Implemented |
| `scalarparameter` | Implemented |
| `set_blend_mode` | Implemented |
| `set_material_domain` | Implemented |
| `set_scalar_parameter_value` | Implemented |
| `set_shading_model` | Implemented |
| `set_substrate_properties` | Implemented |
| `set_texture_parameter_value` | Implemented |
| `set_vector_parameter_value` | Implemented |
| `staticswitch` | Implemented |
| `staticswitchparameter` | Implemented |
| `sub` | Implemented |
| `subtract` | Implemented |
| `switch` | Implemented |
| `texcoord` | Implemented |
| `texture` | Implemented |
| `texture2d` | Implemented |
| `texturecoordinate` | Implemented |
| `textureparameter` | Implemented |
| `texturesample` | Implemented |
| `texturesampleparameter2d` | Implemented |
| `use_material_function` | Implemented |
| `uv` | Implemented |
| `validate_material` | Implemented |
| `vector` | Implemented |
| `vectorparameter` | Implemented |
| `vertexnormal` | Implemented |
| `vertexnormalws` | Implemented |
| `worldposition` | Implemented |

---

## VirtualProduction

**File:** `McpAutomationBridge_VirtualProductionHandlers.cpp`

**Lines:** 1,569

**Maps to TS Tool(s):** `manage_xr`

**Actions (130):**

| Action | Status |
|--------|--------|
| `add_cluster_node` | Implemented |
| `add_colorspace_transform` | Implemented |
| `add_composure_layer` | Implemented |
| `add_dmx_component` | Implemented |
| `add_fixture_function` | Implemented |
| `add_fixture_mode` | Implemented |
| `add_icvfx_camera` | Implemented |
| `add_input_pass` | Implemented |
| `add_midi_device_component` | Implemented |
| `add_output_pass` | Implemented |
| `add_timecode_source` | Implemented |
| `add_transform_pass` | Implemented |
| `add_viewport` | Implemented |
| `apply_ocio_look` | Implemented |
| `assign_fixture_to_universe` | Implemented |
| `attach_child_layer` | Implemented |
| `bind_controller` | Implemented |
| `bind_midi_to_property` | Implemented |
| `bind_osc_address` | Implemented |
| `bind_osc_to_property` | Implemented |
| `bind_render_target` | Implemented |
| `close_midi_input` | Implemented |
| `close_midi_output` | Implemented |
| `configure_aja_timecode` | Implemented |
| `configure_blackmagic_timecode` | Implemented |
| `configure_chroma_keyer` | Implemented |
| `configure_dmx_component` | Implemented |
| `configure_dmx_port` | Implemented |
| `configure_genlock` | Implemented |
| `configure_genlock_source` | Implemented |
| `configure_icvfx_camera` | Implemented |
| `configure_inner_frustum` | Implemented |
| `configure_led_wall_size` | Implemented |
| `configure_light_cards` | Implemented |
| `configure_ltc_timecode` | Implemented |
| `configure_midi_learn` | Implemented |
| `configure_network_settings` | Implemented |
| `configure_osc_dispatcher` | Implemented |
| `configure_outer_viewport` | Implemented |
| `configure_system_time_timecode` | Implemented |
| `configure_viewport_ocio` | Implemented |
| `configure_viewport_region` | Implemented |
| `configure_warp_blend` | Implemented |
| `create_artnet_port` | Implemented |
| `create_composure_element` | Implemented |
| `create_controller` | Implemented |
| `create_dmx_library` | Implemented |
| `create_dmx_sequencer_track` | Implemented |
| `create_fixture_patch` | Implemented |
| `create_fixture_type` | Implemented |
| `create_layout_group` | Implemented |
| `create_led_wall` | Implemented |
| `create_ndisplay_config` | Implemented |
| `create_ocio_config` | Implemented |
| `create_osc_client` | Implemented |
| `create_osc_server` | Implemented |
| `create_remote_control_preset` | Implemented |
| `create_sacn_port` | Implemented |
| `create_timecode_provider` | Implemented |
| `create_timecode_synchronizer` | Implemented |
| `delete_composure_element` | Implemented |
| `detach_child_layer` | Implemented |
| `disable_timecode_genlock` | Implemented |
| `enable_timecode_genlock` | Implemented |
| `expose_function` | Implemented |
| `expose_property` | Implemented |
| `get_composure_info` | Implemented |
| `get_current_timecode` | Implemented |
| `get_dmx_info` | Implemented |
| `get_exposed_properties` | Implemented |
| `get_exposed_property_value` | Implemented |
| `get_fixture_channel_value` | Implemented |
| `get_midi_info` | Implemented |
| `get_ndisplay_info` | Implemented |
| `get_ocio_colorspaces` | Implemented |
| `get_ocio_displays` | Implemented |
| `get_ocio_info` | Implemented |
| `get_osc_info` | Implemented |
| `get_remote_control_info` | Implemented |
| `get_timecode_info` | Implemented |
| `get_timecode_provider_status` | Implemented |
| `get_virtual_production_info` | Implemented |
| `get_web_server_status` | Implemented |
| `import_gdtf` | Implemented |
| `list_active_vp_sessions` | Implemented |
| `list_cluster_nodes` | Implemented |
| `list_dmx_fixtures` | Implemented |
| `list_dmx_universes` | Implemented |
| `list_midi_devices` | Implemented |
| `list_osc_clients` | Implemented |
| `list_osc_servers` | Implemented |
| `list_timecode_providers` | Implemented |
| `load_ocio_config` | Implemented |
| `load_remote_control_preset` | Implemented |
| `open_midi_input` | Implemented |
| `open_midi_output` | Implemented |
| `receive_dmx` | Implemented |
| `remove_cluster_node` | Implemented |
| `remove_composure_layer` | Implemented |
| `remove_icvfx_camera` | Implemented |
| `remove_viewport` | Implemented |
| `reset_vp_state` | Implemented |
| `send_dmx` | Implemented |
| `send_midi_cc` | Implemented |
| `send_midi_note_off` | Implemented |
| `send_midi_note_on` | Implemented |
| `send_midi_pitch_bend` | Implemented |
| `send_midi_program_change` | Implemented |
| `send_osc_bundle` | Implemented |
| `send_osc_message` | Implemented |
| `set_chromakey_settings` | Implemented |
| `set_custom_timestep` | Implemented |
| `set_display_view` | Implemented |
| `set_exposed_property_value` | Implemented |
| `set_fixture_channel_value` | Implemented |
| `set_frame_rate` | Implemented |
| `set_ocio_working_colorspace` | Implemented |
| `set_primary_node` | Implemented |
| `set_projection_policy` | Implemented |
| `set_stage_settings` | Implemented |
| `set_sync_policy` | Implemented |
| `set_timecode_provider` | Implemented |
| `set_viewport_camera` | Implemented |
| `start_web_server` | Implemented |
| `stop_osc_server` | Implemented |
| `stop_web_server` | Implemented |
| `synchronize_timecode` | Implemented |
| `unbind_midi` | Implemented |
| `unbind_osc_address` | Implemented |
| `unexpose_property` | Implemented |

---

## UtilityPlugins

**File:** `McpAutomationBridge_UtilityPluginsHandlers.cpp`

**Lines:** 3,389

**Maps to TS Tool(s):** `manage_asset_plugins`

**Actions (100):**

| Action | Status |
|--------|--------|
| `accept_tool_result` | Implemented |
| `activate_modeling_tool` | Implemented |
| `activate_variant` | Implemented |
| `add_actor_binding` | Implemented |
| `add_flipbook_keyframe` | Implemented |
| `add_menu_entry` | Implemented |
| `add_python_path` | Implemented |
| `add_toolbar_button` | Implemented |
| `add_variant` | Implemented |
| `apply_mesh_operation` | Implemented |
| `cancel_tool` | Implemented |
| `capture_property` | Implemented |
| `clear_all_mesh_sections` | Implemented |
| `clear_mesh_section` | Implemented |
| `clear_mesh_selection` | Implemented |
| `clear_python_output` | Implemented |
| `configure_analog_cursor` | Implemented |
| `configure_gamepad_navigation` | Implemented |
| `configure_navigation_rules` | Implemented |
| `configure_python_paths` | Implemented |
| `configure_sculpt_brush` | Implemented |
| `configure_sprite_collision` | Implemented |
| `configure_sprite_material` | Implemented |
| `configure_ui_input_config` | Implemented |
| `configure_variant_dependency` | Implemented |
| `convert_procedural_to_static_mesh` | Implemented |
| `create_blutility_action` | Implemented |
| `create_common_activatable_widget` | Implemented |
| `create_editor_utility_blueprint` | Implemented |
| `create_editor_utility_widget` | Implemented |
| `create_flipbook` | Implemented |
| `create_level_variant_sets` | Implemented |
| `create_mesh_section` | Implemented |
| `create_procedural_mesh_component` | Implemented |
| `create_python_editor_utility` | Implemented |
| `create_sprite` | Implemented |
| `create_tile_map` | Implemented |
| `create_tile_set` | Implemented |
| `create_variant_set` | Implemented |
| `deactivate_modeling_tool` | Implemented |
| `deactivate_variant` | Implemented |
| `delete_variant_set` | Implemented |
| `duplicate_variant` | Implemented |
| `enter_modeling_mode` | Implemented |
| `execute_editor_command` | Implemented |
| `execute_python_command` | Implemented |
| `execute_python_file` | Implemented |
| `execute_python_script` | Implemented |
| `execute_sculpt_stroke` | Implemented |
| `export_variant_configuration` | Implemented |
| `get_active_tool` | Implemented |
| `get_active_variant` | Implemented |
| `get_common_ui_info` | Implemented |
| `get_editor_scripting_info` | Implemented |
| `get_mesh_selection` | Implemented |
| `get_modeling_tools_info` | Implemented |
| `get_paper2d_info` | Implemented |
| `get_plugin_status` | Implemented |
| `get_procedural_mesh_info` | Implemented |
| `get_python_info` | Implemented |
| `get_python_output` | Implemented |
| `get_python_paths` | Implemented |
| `get_python_version` | Implemented |
| `get_sprite_info` | Implemented |
| `get_tool_properties` | Implemented |
| `get_ui_input_config` | Implemented |
| `get_utility_plugins_info` | Implemented |
| `get_variant_manager_info` | Implemented |
| `is_python_available` | Implemented |
| `list_available_tools` | Implemented |
| `list_utility_plugins` | Implemented |
| `register_common_input_metadata` | Implemented |
| `register_editor_command` | Implemented |
| `reload_python_module` | Implemented |
| `remove_actor_binding` | Implemented |
| `remove_menu_entry` | Implemented |
| `remove_python_path` | Implemented |
| `remove_toolbar_button` | Implemented |
| `remove_variant` | Implemented |
| `run_editor_utility` | Implemented |
| `run_startup_scripts` | Implemented |
| `select_mesh_elements` | Implemented |
| `set_default_focus_widget` | Implemented |
| `set_input_action_data` | Implemented |
| `set_mesh_collision` | Implemented |
| `set_mesh_colors` | Implemented |
| `set_mesh_normals` | Implemented |
| `set_mesh_section_visible` | Implemented |
| `set_mesh_tangents` | Implemented |
| `set_mesh_triangles` | Implemented |
| `set_mesh_uvs` | Implemented |
| `set_mesh_vertices` | Implemented |
| `set_sculpt_brush` | Implemented |
| `set_tile_map_layer` | Implemented |
| `set_tool_property` | Implemented |
| `spawn_paper_flipbook_actor` | Implemented |
| `spawn_paper_sprite_actor` | Implemented |
| `undo_mesh_operation` | Implemented |
| `unregister_editor_command` | Implemented |
| `update_mesh_section` | Implemented |

---

## Geometry

**File:** `McpAutomationBridge_GeometryHandlers.cpp`

**Lines:** 4,528

**Maps to TS Tool(s):** `manage_geometry`

**Actions (83):**

| Action | Status |
|--------|--------|
| `array_linear` | Implemented |
| `array_radial` | Implemented |
| `auto_uv` | Implemented |
| `bend` | Implemented |
| `bevel` | Implemented |
| `boolean_intersection` | Implemented |
| `boolean_mesh_operation` | Implemented |
| `boolean_subtract` | Implemented |
| `boolean_trim` | Implemented |
| `boolean_union` | Implemented |
| `bridge` | Implemented |
| `chamfer` | Implemented |
| `configure_nanite_settings` | Implemented |
| `convert_to_nanite` | Implemented |
| `convert_to_static_mesh` | Implemented |
| `create_arch` | Implemented |
| `create_box` | Implemented |
| `create_capsule` | Implemented |
| `create_cone` | Implemented |
| `create_cylinder` | Implemented |
| `create_disc` | Implemented |
| `create_mesh_from_spline` | Implemented |
| `create_pipe` | Implemented |
| `create_plane` | Implemented |
| `create_procedural_box` | Implemented |
| `create_ramp` | Implemented |
| `create_ring` | Implemented |
| `create_sphere` | Implemented |
| `create_spiral_stairs` | Implemented |
| `create_stairs` | Implemented |
| `create_torus` | Implemented |
| `cylindrify` | Implemented |
| `duplicate_along_spline` | Implemented |
| `edge_split` | Implemented |
| `enable_nanite_mesh` | Implemented |
| `export_geometry_to_file` | Implemented |
| `extrude` | Implemented |
| `extrude_along_spline` | Implemented |
| `fill_holes` | Implemented |
| `flip_normals` | Implemented |
| `generate_collision` | Implemented |
| `generate_complex_collision` | Implemented |
| `generate_lods` | Implemented |
| `generate_mesh_uvs` | Implemented |
| `get_mesh_info` | Implemented |
| `inset` | Implemented |
| `loft` | Implemented |
| `loop_cut` | Implemented |
| `merge_vertices` | Implemented |
| `mirror` | Implemented |
| `noise_deform` | Implemented |
| `offset_faces` | Implemented |
| `outset` | Implemented |
| `pack_uv_islands` | Implemented |
| `poke` | Implemented |
| `project_uv` | Implemented |
| `quadrangulate` | Implemented |
| `recalculate_normals` | Implemented |
| `recompute_tangents` | Implemented |
| `relax` | Implemented |
| `remesh_uniform` | Implemented |
| `remesh_voxel` | Implemented |
| `remove_degenerates` | Implemented |
| `revolve` | Implemented |
| `self_union` | Implemented |
| `set_lod_screen_sizes` | Implemented |
| `set_lod_settings` | Implemented |
| `set_nanite_settings` | Implemented |
| `shell` | Implemented |
| `simplify_collision` | Implemented |
| `simplify_mesh` | Implemented |
| `smooth` | Implemented |
| `spherify` | Implemented |
| `split_normals` | Implemented |
| `stretch` | Implemented |
| `subdivide` | Implemented |
| `sweep` | Implemented |
| `taper` | Implemented |
| `transform_uvs` | Implemented |
| `triangulate` | Implemented |
| `twist` | Implemented |
| `unwrap_uv` | Implemented |
| `weld_vertices` | Implemented |

---

## AudioMiddleware

**File:** `McpAutomationBridge_AudioMiddlewareHandlers.cpp`

**Lines:** 1,477

**Maps to TS Tool(s):** `manage_audio`

**Actions (81):**

| Action | Status |
|--------|--------|
| `apply_fmod_snapshot` | Implemented |
| `configure_bink_buffer_mode` | Implemented |
| `configure_bink_draw_style` | Implemented |
| `configure_bink_sound_track` | Implemented |
| `configure_bink_texture` | Implemented |
| `configure_fmod_attenuation` | Implemented |
| `configure_fmod_component` | Implemented |
| `configure_fmod_init` | Implemented |
| `configure_fmod_occlusion` | Implemented |
| `configure_occlusion` | Implemented |
| `configure_portal` | Implemented |
| `configure_room` | Implemented |
| `configure_spatial_audio` | Implemented |
| `configure_wwise_component` | Implemented |
| `configure_wwise_init` | Implemented |
| `connect_fmod_project` | Implemented |
| `connect_wwise_project` | Implemented |
| `create_bink_media_player` | Implemented |
| `create_bink_texture` | Implemented |
| `create_fmod_component` | Implemented |
| `create_wwise_component` | Implemented |
| `create_wwise_trigger` | Implemented |
| `draw_bink_to_texture` | Implemented |
| `get_audio_middleware_info` | Implemented |
| `get_bink_dimensions` | Implemented |
| `get_bink_duration` | Implemented |
| `get_bink_status` | Implemented |
| `get_bink_time` | Implemented |
| `get_fmod_event_info` | Implemented |
| `get_fmod_loaded_banks` | Implemented |
| `get_fmod_memory_usage` | Implemented |
| `get_fmod_parameter` | Implemented |
| `get_fmod_status` | Implemented |
| `get_loaded_banks` | Implemented |
| `get_rtpc_value` | Implemented |
| `get_wwise_event_duration` | Implemented |
| `get_wwise_status` | Implemented |
| `load_fmod_bank` | Implemented |
| `load_wwise_bank` | Implemented |
| `open_bink_video` | Implemented |
| `pause_all_fmod_events` | Implemented |
| `pause_bink` | Implemented |
| `play_bink` | Implemented |
| `play_fmod_event` | Implemented |
| `play_fmod_event_at_location` | Implemented |
| `post_wwise_event` | Implemented |
| `post_wwise_event_at_location` | Implemented |
| `post_wwise_trigger` | Implemented |
| `release_fmod_snapshot` | Implemented |
| `restart_fmod_engine` | Implemented |
| `restart_wwise_engine` | Implemented |
| `resume_all_fmod_events` | Implemented |
| `seek_bink` | Implemented |
| `set_aux_send` | Implemented |
| `set_bink_looping` | Implemented |
| `set_bink_rate` | Implemented |
| `set_bink_texture_player` | Implemented |
| `set_bink_volume` | Implemented |
| `set_fmod_3d_attributes` | Implemented |
| `set_fmod_bus_mute` | Implemented |
| `set_fmod_bus_paused` | Implemented |
| `set_fmod_bus_volume` | Implemented |
| `set_fmod_global_parameter` | Implemented |
| `set_fmod_listener_attributes` | Implemented |
| `set_fmod_parameter` | Implemented |
| `set_fmod_studio_path` | Implemented |
| `set_fmod_vca_volume` | Implemented |
| `set_listener_position` | Implemented |
| `set_rtpc_value` | Implemented |
| `set_rtpc_value_on_actor` | Implemented |
| `set_wwise_game_object` | Implemented |
| `set_wwise_project_path` | Implemented |
| `set_wwise_state` | Implemented |
| `set_wwise_switch` | Implemented |
| `set_wwise_switch_on_actor` | Implemented |
| `stop_bink` | Implemented |
| `stop_fmod_event` | Implemented |
| `stop_wwise_event` | Implemented |
| `unload_fmod_bank` | Implemented |
| `unload_wwise_bank` | Implemented |
| `unset_wwise_game_object` | Implemented |

---

## PhysicsDestruction

**File:** `McpAutomationBridge_PhysicsDestructionHandlers.cpp`

**Lines:** 2,242

**Maps to TS Tool(s):** `animation_physics`

**Actions (80):**

| Action | Status |
|--------|--------|
| `add_construction_field` | Implemented |
| `add_field_noise` | Implemented |
| `add_field_radial_falloff` | Implemented |
| `add_field_radial_vector` | Implemented |
| `add_field_strain` | Implemented |
| `add_field_uniform_vector` | Implemented |
| `add_persistent_field` | Implemented |
| `add_transient_field` | Implemented |
| `add_vehicle_wheel` | Implemented |
| `apply_cache_to_collection` | Implemented |
| `apply_cloth_to_skeletal_mesh` | Implemented |
| `bind_flesh_to_skeleton` | Implemented |
| `configure_brake_setup` | Implemented |
| `configure_differential_setup` | Implemented |
| `configure_engine_setup` | Implemented |
| `configure_steering_setup` | Implemented |
| `configure_suspension_setup` | Implemented |
| `configure_transmission_setup` | Implemented |
| `create_anchor_field` | Implemented |
| `create_chaos_cloth_config` | Implemented |
| `create_chaos_cloth_shared_sim_config` | Implemented |
| `create_field_system_actor` | Implemented |
| `create_flesh_asset` | Implemented |
| `create_flesh_cache` | Implemented |
| `create_flesh_component` | Implemented |
| `create_geometry_collection` | Implemented |
| `create_geometry_collection_cache` | Implemented |
| `create_vehicle_animation_instance` | Implemented |
| `create_wheeled_vehicle_bp` | Implemented |
| `enable_clustering` | Implemented |
| `flatten_fracture` | Implemented |
| `fracture_brick` | Implemented |
| `fracture_clustered` | Implemented |
| `fracture_radial` | Implemented |
| `fracture_slice` | Implemented |
| `fracture_uniform` | Implemented |
| `get_chaos_plugin_status` | Implemented |
| `get_cloth_config` | Implemented |
| `get_cloth_stats` | Implemented |
| `get_flesh_asset_info` | Implemented |
| `get_geometry_collection_stats` | Implemented |
| `get_physics_destruction_info` | Implemented |
| `get_vehicle_config` | Implemented |
| `list_chaos_vehicles` | Implemented |
| `list_geometry_collections` | Implemented |
| `record_flesh_simulation` | Implemented |
| `record_geometry_collection_cache` | Implemented |
| `remove_cloth_from_skeletal_mesh` | Implemented |
| `remove_geometry_collection_cache` | Implemented |
| `remove_wheel_from_vehicle` | Implemented |
| `set_center_of_mass` | Implemented |
| `set_cloth_aerodynamics` | Implemented |
| `set_cloth_anim_drive` | Implemented |
| `set_cloth_collision_properties` | Implemented |
| `set_cloth_damping` | Implemented |
| `set_cloth_gravity` | Implemented |
| `set_cloth_long_range_attachment` | Implemented |
| `set_cloth_mass_properties` | Implemented |
| `set_cloth_stiffness` | Implemented |
| `set_cloth_tether_stiffness` | Implemented |
| `set_cluster_connection_type` | Implemented |
| `set_collision_particles_fraction` | Implemented |
| `set_damage_thresholds` | Implemented |
| `set_drag_coefficient` | Implemented |
| `set_dynamic_state` | Implemented |
| `set_flesh_damping` | Implemented |
| `set_flesh_incompressibility` | Implemented |
| `set_flesh_inflation` | Implemented |
| `set_flesh_rest_state` | Implemented |
| `set_flesh_simulation_properties` | Implemented |
| `set_flesh_solver_iterations` | Implemented |
| `set_flesh_stiffness` | Implemented |
| `set_geometry_collection_materials` | Implemented |
| `set_remove_on_break` | Implemented |
| `set_vehicle_animation_bp` | Implemented |
| `set_vehicle_mass` | Implemented |
| `set_vehicle_mesh` | Implemented |
| `set_wheel_class` | Implemented |
| `set_wheel_offset` | Implemented |
| `set_wheel_radius` | Implemented |

---

## AssetWorkflow

**File:** `McpAutomationBridge_AssetWorkflowHandlers.cpp`

**Lines:** 3,357

**Maps to TS Tool(s):** `manage_asset`

**Actions (73):**

| Action | Status |
|--------|--------|
| `add_material_node` | Implemented |
| `add_material_parameter` | Implemented |
| `analyze_graph` | Implemented |
| `audio` | Implemented |
| `batch_nanite_convert` | Implemented |
| `blueprint` | Implemented |
| `blueprint_list_node_types` | Implemented |
| `blueprints` | Implemented |
| `bp_list_node_types` | Implemented |
| `break_material_connections` | Implemented |
| `compile_blueprint_batch` | Implemented |
| `configure_audio_modulation` | Implemented |
| `configure_nanite_settings` | Implemented |
| `connect_material_pins` | Implemented |
| `convert_to_nanite` | Implemented |
| `create_envelope` | Implemented |
| `create_filter` | Implemented |
| `create_folder` | Implemented |
| `create_material` | Implemented |
| `create_material_instance` | Implemented |
| `create_oscillator` | Implemented |
| `create_procedural_music` | Implemented |
| `create_render_target` | Implemented |
| `create_sequencer_node` | Implemented |
| `create_thumbnail` | Implemented |
| `delete` | Implemented |
| `delete_asset` | Implemented |
| `delete_assets` | Implemented |
| `duplicate` | Implemented |
| `enable_nanite` | Implemented |
| `enable_nanite_mesh` | Implemented |
| `exists` | Implemented |
| `find_by_tag` | Implemented |
| `fixup_redirectors` | Implemented |
| `generate_lods` | Implemented |
| `generate_report` | Implemented |
| `generate_thumbnail` | Implemented |
| `get_asset_graph` | Implemented |
| `get_blueprint_dependencies` | Implemented |
| `get_dependencies` | Implemented |
| `get_material_node_details` | Implemented |
| `get_material_stats` | Implemented |
| `get_metadata` | Implemented |
| `get_source_control_state` | Implemented |
| `import` | Implemented |
| `list` | Implemented |
| `list_instances` | Implemented |
| `material` | Implemented |
| `materials` | Implemented |
| `mesh` | Implemented |
| `meshes` | Implemented |
| `move` | Implemented |
| `nanite_rebuild_mesh` | Implemented |
| `query_assets_by_predicate` | Implemented |
| `rebuild_material` | Implemented |
| `remove_material_node` | Implemented |
| `rename` | Implemented |
| `reset_instance_parameters` | Implemented |
| `save` | Implemented |
| `save_asset` | Implemented |
| `search` | Implemented |
| `search_assets` | Implemented |
| `set_metadata` | Implemented |
| `set_nanite_settings` | Implemented |
| `set_tags` | Implemented |
| `sound` | Implemented |
| `sounds` | Implemented |
| `staticmesh` | Implemented |
| `texture` | Implemented |
| `textures` | Implemented |
| `validate` | Implemented |
| `validate_asset` | Implemented |
| `validate_blueprint` | Implemented |

---

## SequencerConsolidated

**File:** `McpAutomationBridge_SequencerConsolidatedHandlers.cpp`

**Lines:** 2,754

**Maps to TS Tool(s):** `manage_sequence`

**Actions (70):**

| Action | Status |
|--------|--------|
| `add_actor` | Implemented |
| `add_actors` | Implemented |
| `add_camera` | Implemented |
| `add_camera_cut` | Implemented |
| `add_camera_cut_track` | Implemented |
| `add_keyframe` | Implemented |
| `add_procedural_camera_shake` | Implemented |
| `add_section` | Implemented |
| `add_shot` | Implemented |
| `add_shot_track` | Implemented |
| `add_spawnable_from_class` | Implemented |
| `add_subsequence` | Implemented |
| `add_track` | Implemented |
| `bind_actor` | Implemented |
| `configure_audio_track` | Implemented |
| `configure_camera_settings` | Implemented |
| `configure_sequence_lod` | Implemented |
| `configure_sequence_streaming` | Implemented |
| `create` | Implemented |
| `create_camera_cut_track` | Implemented |
| `create_cine_camera_actor` | Implemented |
| `create_event_trigger_track` | Implemented |
| `create_master_sequence` | Implemented |
| `create_media_track` | Implemented |
| `delete` | Implemented |
| `delete_sequence` | Implemented |
| `duplicate` | Implemented |
| `duplicate_sequence` | Implemented |
| `export_sequence` | Implemented |
| `get_bindings` | Implemented |
| `get_keyframes` | Implemented |
| `get_metadata` | Implemented |
| `get_playback_range` | Implemented |
| `get_properties` | Implemented |
| `get_sequence_bindings` | Implemented |
| `get_sequence_info` | Implemented |
| `get_shots` | Implemented |
| `get_subsequences` | Implemented |
| `get_tracks` | Implemented |
| `list` | Implemented |
| `list_sequences` | Implemented |
| `list_track_types` | Implemented |
| `list_tracks` | Implemented |
| `open` | Implemented |
| `pause` | Implemented |
| `pause_sequence` | Implemented |
| `play` | Implemented |
| `play_sequence` | Implemented |
| `remove_actors` | Implemented |
| `remove_keyframe` | Implemented |
| `remove_section` | Implemented |
| `remove_shot` | Implemented |
| `remove_subsequence` | Implemented |
| `remove_track` | Implemented |
| `rename` | Implemented |
| `scrub_to_time` | Implemented |
| `set_display_rate` | Implemented |
| `set_metadata` | Implemented |
| `set_playback_range` | Implemented |
| `set_playback_speed` | Implemented |
| `set_properties` | Implemented |
| `set_tick_resolution` | Implemented |
| `set_track_locked` | Implemented |
| `set_track_muted` | Implemented |
| `set_track_solo` | Implemented |
| `set_view_range` | Implemented |
| `set_work_range` | Implemented |
| `stop` | Implemented |
| `stop_sequence` | Implemented |
| `unbind_actor` | Implemented |

---

## LiveLink

**File:** `McpAutomationBridge_LiveLinkHandlers.cpp`

**Lines:** 1,797

**Maps to TS Tool(s):** `manage_livelink`

**Actions (64):**

| Action | Status |
|--------|--------|
| `add_livelink_controller` | Implemented |
| `add_livelink_source` | Implemented |
| `add_messagebus_source` | Implemented |
| `add_preset_to_client` | Implemented |
| `add_virtual_subject` | Implemented |
| `apply_face_to_skeletal_mesh` | Implemented |
| `apply_livelink_preset` | Implemented |
| `apply_mocap_to_character` | Implemented |
| `build_preset_from_client` | Implemented |
| `clear_subject_frames` | Implemented |
| `configure_arkit_mapping` | Implemented |
| `configure_blendshape_remap` | Implemented |
| `configure_bone_mapping` | Implemented |
| `configure_curve_mapping` | Implemented |
| `configure_face_retargeting` | Implemented |
| `configure_face_source` | Implemented |
| `configure_frame_interpolation` | Implemented |
| `configure_livelink_controller` | Implemented |
| `configure_livelink_timecode` | Implemented |
| `configure_skeleton_mapping` | Implemented |
| `configure_source_settings` | Implemented |
| `configure_subject_settings` | Implemented |
| `configure_time_sync` | Implemented |
| `create_livelink_preset` | Implemented |
| `create_retarget_asset` | Implemented |
| `disable_controller_evaluation` | Implemented |
| `disable_subject` | Implemented |
| `discover_messagebus_sources` | Implemented |
| `enable_controller_evaluation` | Implemented |
| `enable_subject` | Implemented |
| `force_livelink_tick` | Implemented |
| `get_controller_info` | Implemented |
| `get_face_blendshapes` | Implemented |
| `get_face_tracking_status` | Implemented |
| `get_livelink_info` | Implemented |
| `get_livelink_timecode` | Implemented |
| `get_preset_sources` | Implemented |
| `get_preset_subjects` | Implemented |
| `get_skeleton_mapping_info` | Implemented |
| `get_source_status` | Implemented |
| `get_source_type` | Implemented |
| `get_subject_frame_data` | Implemented |
| `get_subject_frame_times` | Implemented |
| `get_subject_role` | Implemented |
| `get_subject_state` | Implemented |
| `get_subject_static_data` | Implemented |
| `get_subjects_by_role` | Implemented |
| `list_available_roles` | Implemented |
| `list_livelink_sources` | Implemented |
| `list_livelink_subjects` | Implemented |
| `list_source_factories` | Implemented |
| `load_livelink_preset` | Implemented |
| `pause_subject` | Implemented |
| `remove_all_sources` | Implemented |
| `remove_livelink_source` | Implemented |
| `remove_virtual_subject` | Implemented |
| `save_livelink_preset` | Implemented |
| `set_buffer_settings` | Implemented |
| `set_controlled_component` | Implemented |
| `set_controller_role` | Implemented |
| `set_controller_subject` | Implemented |
| `set_face_neutral_pose` | Implemented |
| `set_timecode_provider` | Implemented |
| `unpause_subject` | Implemented |

---

## GameplayPrimitives

**File:** `McpAutomationBridge_GameplayPrimitivesHandlers.cpp`

**Lines:** 2,720

**Maps to TS Tool(s):** `manage_gameplay_primitives`

**Actions (62):**

| Action | Status |
|--------|--------|
| `add_actor_state` | Implemented |
| `add_actor_state_transition` | Implemented |
| `add_condition_listener` | Implemented |
| `add_interactable_component` | Implemented |
| `add_reputation_threshold` | Implemented |
| `add_schedule_entry` | Implemented |
| `add_time_event` | Implemented |
| `add_value_threshold` | Implemented |
| `add_zone_enter_event` | Implemented |
| `add_zone_exit_event` | Implemented |
| `assign_to_faction` | Implemented |
| `attach_to_socket` | Implemented |
| `check_faction_relationship` | Implemented |
| `configure_attachment_rules` | Implemented |
| `configure_interaction` | Implemented |
| `configure_spawn_conditions` | Implemented |
| `configure_spawner` | Implemented |
| `configure_state_timer` | Implemented |
| `configure_value_decay` | Implemented |
| `configure_value_regen` | Implemented |
| `create_actor_state_machine` | Implemented |
| `create_compound_condition` | Implemented |
| `create_condition` | Implemented |
| `create_faction` | Implemented |
| `create_schedule` | Implemented |
| `create_spawner` | Implemented |
| `create_value_tracker` | Implemented |
| `create_world_time` | Implemented |
| `create_zone` | Implemented |
| `despawn_managed_actors` | Implemented |
| `detach_from_parent` | Implemented |
| `evaluate_condition` | Implemented |
| `execute_interaction` | Implemented |
| `focus_interaction` | Implemented |
| `get_actor_state` | Implemented |
| `get_actor_zone` | Implemented |
| `get_attached_actors` | Implemented |
| `get_attachment_parent` | Implemented |
| `get_current_schedule_entry` | Implemented |
| `get_faction` | Implemented |
| `get_nearby_interactables` | Implemented |
| `get_reputation` | Implemented |
| `get_spawned_count` | Implemented |
| `get_time_period` | Implemented |
| `get_value` | Implemented |
| `get_world_time` | Implemented |
| `get_zone_property` | Implemented |
| `modify_reputation` | Implemented |
| `modify_value` | Implemented |
| `pause_value_changes` | Implemented |
| `pause_world_time` | Implemented |
| `set_actor_state` | Implemented |
| `set_faction_relationship` | Implemented |
| `set_interaction_enabled` | Implemented |
| `set_schedule_active` | Implemented |
| `set_spawner_enabled` | Implemented |
| `set_time_scale` | Implemented |
| `set_value` | Implemented |
| `set_world_time` | Implemented |
| `set_zone_property` | Implemented |
| `skip_to_schedule_entry` | Implemented |
| `transfer_control` | Implemented |

---

## Lighting

**File:** `McpAutomationBridge_LightingHandlers.cpp`

**Lines:** 1,323

**Maps to TS Tool(s):** `manage_lighting`

**Actions (61):**

| Action | Status |
|--------|--------|
| `bake_lighting_preview` | Implemented |
| `build_lighting` | Implemented |
| `build_lighting_quality` | Implemented |
| `capture_scene` | Implemented |
| `configure_bloom` | Implemented |
| `configure_chromatic_aberration` | Implemented |
| `configure_color_grading` | Implemented |
| `configure_dof` | Implemented |
| `configure_film_grain` | Implemented |
| `configure_gi_settings` | Implemented |
| `configure_indirect_lighting_cache` | Implemented |
| `configure_lens_flares` | Implemented |
| `configure_lightmass_settings` | Implemented |
| `configure_lumen_gi` | Implemented |
| `configure_megalights_scene` | Implemented |
| `configure_motion_blur` | Implemented |
| `configure_path_tracing` | Implemented |
| `configure_pp_blend` | Implemented |
| `configure_pp_priority` | Implemented |
| `configure_ray_traced_ao` | Implemented |
| `configure_ray_traced_gi` | Implemented |
| `configure_ray_traced_reflections` | Implemented |
| `configure_ray_traced_shadows` | Implemented |
| `configure_shadow_settings` | Implemented |
| `configure_shadows` | Implemented |
| `configure_vignette` | Implemented |
| `configure_volumetric_fog` | Implemented |
| `configure_volumetric_lightmap` | Implemented |
| `configure_white_balance` | Implemented |
| `create_box_reflection_capture` | Implemented |
| `create_dynamic_light` | Implemented |
| `create_light` | Implemented |
| `create_light_batch` | Implemented |
| `create_lighting_enabled_level` | Implemented |
| `create_lightmass_volume` | Implemented |
| `create_lumen_volume` | Implemented |
| `create_planar_reflection` | Implemented |
| `create_post_process_volume` | Implemented |
| `create_scene_capture_2d` | Implemented |
| `create_scene_capture_cube` | Implemented |
| `create_sky_light` | Implemented |
| `create_sphere_reflection_capture` | Implemented |
| `ensure_single_sky_light` | Implemented |
| `get_light_complexity` | Implemented |
| `get_megalights_budget` | Implemented |
| `get_post_process_settings` | Implemented |
| `list_light_types` | Implemented |
| `optimize_lights_for_megalights` | Implemented |
| `recapture_scene` | Implemented |
| `set_actor_light_channel` | Implemented |
| `set_ambient_occlusion` | Implemented |
| `set_exposure` | Implemented |
| `set_light_channel` | Implemented |
| `set_lumen_reflections` | Implemented |
| `set_virtual_shadow_maps` | Implemented |
| `setup_global_illumination` | Implemented |
| `setup_volumetric_fog` | Implemented |
| `spawn_light` | Implemented |
| `spawn_sky_light` | Implemented |
| `tune_lumen_performance` | Implemented |
| `validate_lighting_setup` | Implemented |

---

## CharacterAvatar

**File:** `McpAutomationBridge_CharacterAvatarHandlers.cpp`

**Lines:** 1,838

**Maps to TS Tool(s):** `manage_character_avatar`

**Actions (60):**

| Action | Status |
|--------|--------|
| `apply_avatar_to_character` | Implemented |
| `apply_preset` | Implemented |
| `attach_groom_to_skeletal_mesh` | Implemented |
| `bake_customizable_instance` | Implemented |
| `cache_avatar` | Implemented |
| `clear_avatar_cache` | Implemented |
| `compile_customizable_object` | Implemented |
| `configure_face_rig` | Implemented |
| `configure_hair_physics` | Implemented |
| `configure_hair_rendering` | Implemented |
| `configure_hair_simulation` | Implemented |
| `configure_metahuman_lod` | Implemented |
| `configure_rpm_materials` | Implemented |
| `create_customizable_instance` | Implemented |
| `create_customizable_object` | Implemented |
| `create_groom_asset` | Implemented |
| `create_groom_binding` | Implemented |
| `create_rpm_actor` | Implemented |
| `create_rpm_animation_blueprint` | Implemented |
| `enable_body_correctives` | Implemented |
| `enable_hair_simulation` | Implemented |
| `enable_neck_correctives` | Implemented |
| `export_metahuman_settings` | Implemented |
| `get_avatar_metadata` | Implemented |
| `get_groom_info` | Implemented |
| `get_instance_info` | Implemented |
| `get_metahuman_component` | Implemented |
| `get_metahuman_info` | Implemented |
| `get_parameter_info` | Implemented |
| `get_rpm_info` | Implemented |
| `import_groom` | Implemented |
| `import_metahuman` | Implemented |
| `list_available_presets` | Implemented |
| `load_avatar_from_glb` | Implemented |
| `load_avatar_from_url` | Implemented |
| `retarget_rpm_animation` | Implemented |
| `set_body_part` | Implemented |
| `set_body_type` | Implemented |
| `set_bool_parameter` | Implemented |
| `set_color_parameter` | Implemented |
| `set_eye_color` | Implemented |
| `set_face_parameter` | Implemented |
| `set_float_parameter` | Implemented |
| `set_hair_color` | Implemented |
| `set_hair_root_scale` | Implemented |
| `set_hair_style` | Implemented |
| `set_hair_tip_scale` | Implemented |
| `set_hair_width` | Implemented |
| `set_int_parameter` | Implemented |
| `set_projector_parameter` | Implemented |
| `set_quality_level` | Implemented |
| `set_rpm_outfit` | Implemented |
| `set_skin_tone` | Implemented |
| `set_texture_parameter` | Implemented |
| `set_transform_parameter` | Implemented |
| `set_vector_parameter` | Implemented |
| `spawn_customizable_actor` | Implemented |
| `spawn_groom_actor` | Implemented |
| `spawn_metahuman_actor` | Implemented |
| `update_skeletal_mesh` | Implemented |

---

## MetaSound

**File:** `McpAutomationBridge_MetaSoundHandlers.cpp`

**Lines:** 775

**Maps to TS Tool(s):** `manage_asset`, `manage_audio`

**Actions (60):**

| Action | Status |
|--------|--------|
| `add` | Implemented |
| `add_metasound_node` | Implemented |
| `adsr` | Implemented |
| `audioinput` | Implemented |
| `audiooutput` | Implemented |
| `bandpass` | Implemented |
| `bandpassfilter` | Implemented |
| `bpf` | Implemented |
| `chorus` | Implemented |
| `clamp` | Implemented |
| `compressor` | Implemented |
| `configure_audio_modulation` | Implemented |
| `connect_metasound_nodes` | Implemented |
| `create_envelope` | Implemented |
| `create_filter` | Implemented |
| `create_metasound` | Implemented |
| `create_oscillator` | Implemented |
| `create_procedural_music` | Implemented |
| `create_sequencer_node` | Implemented |
| `decay` | Implemented |
| `delay` | Implemented |
| `envelope` | Implemented |
| `export_metasound_preset` | Implemented |
| `filter` | Implemented |
| `flanger` | Implemented |
| `floatinput` | Implemented |
| `gain` | Implemented |
| `highpass` | Implemented |
| `highpassfilter` | Implemented |
| `hpf` | Implemented |
| `import_audio_to_metasound` | Implemented |
| `input` | Implemented |
| `limiter` | Implemented |
| `lowpass` | Implemented |
| `lowpassfilter` | Implemented |
| `lpf` | Implemented |
| `manage_asset` | Implemented |
| `manage_audio` | Implemented |
| `mixer` | Implemented |
| `multiply` | Implemented |
| `noise` | Implemented |
| `noisegenerator` | Implemented |
| `oscillator` | Implemented |
| `output` | Implemented |
| `parameter` | Implemented |
| `phaser` | Implemented |
| `remove_metasound_node` | Implemented |
| `reverb` | Implemented |
| `saw` | Implemented |
| `sawtooth` | Implemented |
| `sawtoothoscillator` | Implemented |
| `set_metasound_variable` | Implemented |
| `sine` | Implemented |
| `sineoscillator` | Implemented |
| `square` | Implemented |
| `squareoscillator` | Implemented |
| `subtract` | Implemented |
| `triangle` | Implemented |
| `triangleoscillator` | Implemented |
| `whitenoise` | Implemented |

---

## AI

**File:** `McpAutomationBridge_AIHandlers.cpp`

**Lines:** 2,527

**Maps to TS Tool(s):** `manage_ai`

**Actions (56):**

| Action | Status |
|--------|--------|
| `add_ai_perception_component` | Implemented |
| `add_blackboard_key` | Implemented |
| `add_composite_node` | Implemented |
| `add_decorator` | Implemented |
| `add_eqs_context` | Implemented |
| `add_eqs_generator` | Implemented |
| `add_eqs_test` | Implemented |
| `add_mass_spawner` | Implemented |
| `add_service` | Implemented |
| `add_smart_object_component` | Implemented |
| `add_smart_object_slot` | Implemented |
| `add_state_tree_state` | Implemented |
| `add_state_tree_transition` | Implemented |
| `add_task_node` | Implemented |
| `ai_assistant_explain_feature` | Implemented |
| `ai_assistant_query` | Implemented |
| `ai_assistant_suggest_fix` | Implemented |
| `assign_behavior_tree` | Implemented |
| `assign_blackboard` | Implemented |
| `bind_statetree` | Implemented |
| `claim_smart_object` | Implemented |
| `configure_bt_node` | Implemented |
| `configure_damage_sense_config` | Implemented |
| `configure_hearing_config` | Implemented |
| `configure_mass_ai_fragment` | Implemented |
| `configure_mass_entity` | Implemented |
| `configure_sight_config` | Implemented |
| `configure_slot_behavior` | Implemented |
| `configure_smart_object` | Implemented |
| `configure_state_tree_node` | Implemented |
| `configure_state_tree_task` | Implemented |
| `configure_test_scoring` | Implemented |
| `create_ai_controller` | Implemented |
| `create_behavior_tree` | Implemented |
| `create_blackboard_asset` | Implemented |
| `create_eqs_query` | Implemented |
| `create_mass_entity_config` | Implemented |
| `create_smart_object` | Implemented |
| `create_smart_object_definition` | Implemented |
| `create_state_tree` | Implemented |
| `debug_behavior_tree` | Implemented |
| `destroy_mass_entity` | Implemented |
| `get_ai_info` | Implemented |
| `get_ai_perception_data` | Implemented |
| `get_statetree_state` | Implemented |
| `list_statetree_states` | Implemented |
| `query_eqs_results` | Implemented |
| `query_mass_entities` | Implemented |
| `query_smart_objects` | Implemented |
| `release_smart_object` | Implemented |
| `set_key_instance_synced` | Implemented |
| `set_mass_entity_fragment` | Implemented |
| `set_perception_team` | Implemented |
| `spawn_mass_ai_entities` | Implemented |
| `spawn_mass_entity` | Implemented |
| `trigger_statetree_transition` | Implemented |

---

## Environment

**File:** `McpAutomationBridge_EnvironmentHandlers.cpp`

**Lines:** 3,514

**Maps to TS Tool(s):** `build_environment`

**Actions (55):**

| Action | Status |
|--------|--------|
| `add_foliage` | Implemented |
| `add_foliage_instances` | Implemented |
| `bake_lightmap` | Implemented |
| `batch_paint_foliage` | Implemented |
| `configure_exponential_height_fog` | Implemented |
| `configure_foliage_density` | Implemented |
| `configure_sky_atmosphere` | Implemented |
| `configure_sun_atmosphere` | Implemented |
| `configure_sun_color` | Implemented |
| `configure_sun_position` | Implemented |
| `configure_volumetric_cloud` | Implemented |
| `create_exponential_height_fog` | Implemented |
| `create_fog_volume` | Implemented |
| `create_landscape_grass_type` | Implemented |
| `create_landscape_spline` | Implemented |
| `create_procedural_foliage` | Implemented |
| `create_procedural_terrain` | Implemented |
| `create_sky_atmosphere` | Implemented |
| `create_sky_sphere` | Implemented |
| `create_time_of_day_controller` | Implemented |
| `create_volumetric_cloud` | Implemented |
| `delete` | Implemented |
| `engine_quit` | Implemented |
| `export_snapshot` | Implemented |
| `find_by_class` | Implemented |
| `generate_lods` | Implemented |
| `get_bounding_box` | Implemented |
| `get_component_property` | Implemented |
| `get_components` | Implemented |
| `get_engine_version` | Implemented |
| `get_feature_flags` | Implemented |
| `get_foliage_instances` | Implemented |
| `get_project_settings` | Implemented |
| `get_property` | Implemented |
| `import_snapshot` | Implemented |
| `inspect_class` | Implemented |
| `inspect_object` | Implemented |
| `modify_heightmap` | Implemented |
| `paint_landscape` | Implemented |
| `paint_landscape_layer` | Implemented |
| `profile` | Implemented |
| `remove_foliage` | Implemented |
| `screenshot` | Implemented |
| `sculpt` | Implemented |
| `sculpt_landscape` | Implemented |
| `set_component_property` | Implemented |
| `set_landscape_material` | Implemented |
| `set_project_setting` | Implemented |
| `set_property` | Implemented |
| `set_quality` | Implemented |
| `set_skylight_intensity` | Implemented |
| `set_sun_intensity` | Implemented |
| `set_time_of_day` | Implemented |
| `show_fps` | Implemented |
| `validate_assets` | Implemented |

---

## Audio

**File:** `McpAutomationBridge_AudioHandlers.cpp`

**Lines:** 1,499

**Maps to TS Tool(s):** `manage_audio`

**Actions (52):**

| Action | Status |
|--------|--------|
| `audio_clear_sound_mix_class_override` | Implemented |
| `audio_create_ambient_sound` | Implemented |
| `audio_create_component` | Implemented |
| `audio_create_reverb_zone` | Implemented |
| `audio_create_sound_class` | Implemented |
| `audio_create_sound_cue` | Implemented |
| `audio_create_sound_mix` | Implemented |
| `audio_enable_audio_analysis` | Implemented |
| `audio_fade_sound` | Implemented |
| `audio_fade_sound_in` | Implemented |
| `audio_fade_sound_out` | Implemented |
| `audio_get_metasound_inputs` | Implemented |
| `audio_list_metasound_assets` | Implemented |
| `audio_play_sound_2d` | Implemented |
| `audio_play_sound_at_location` | Implemented |
| `audio_play_sound_attached` | Implemented |
| `audio_pop_sound_mix` | Implemented |
| `audio_push_sound_mix` | Implemented |
| `audio_set_audio_occlusion` | Implemented |
| `audio_set_doppler_effect` | Implemented |
| `audio_set_sound_attenuation` | Implemented |
| `audio_set_sound_mix_class_override` | Implemented |
| `audio_spawn_sound_at_location` | Implemented |
| `audio_trigger_metasound` | Implemented |
| `clear_sound_mix_class_override` | Implemented |
| `configure_wwise_occlusion` | Implemented |
| `create_ambient_sound` | Implemented |
| `create_audio_component` | Implemented |
| `create_reverb_zone` | Implemented |
| `create_sound_class` | Implemented |
| `create_sound_cue` | Implemented |
| `create_sound_mix` | Implemented |
| `enable_audio_analysis` | Implemented |
| `fade_sound` | Implemented |
| `fade_sound_in` | Implemented |
| `fade_sound_out` | Implemented |
| `get_metasound_inputs` | Implemented |
| `list_metasound_assets` | Implemented |
| `mw_configure_wwise_occlusion` | Implemented |
| `play_sound_2d` | Implemented |
| `play_sound_at_location` | Implemented |
| `play_sound_attached` | Implemented |
| `pop_sound_mix` | Implemented |
| `prime_sound` | Implemented |
| `push_sound_mix` | Implemented |
| `set_audio_occlusion` | Implemented |
| `set_base_sound_mix` | Implemented |
| `set_doppler_effect` | Implemented |
| `set_sound_attenuation` | Implemented |
| `set_sound_mix_class_override` | Implemented |
| `spawn_sound_at_location` | Implemented |
| `trigger_metasound` | Implemented |

---

## Accessibility

**File:** `McpAutomationBridge_AccessibilityHandlers.cpp`

**Lines:** 1,545

**Maps to TS Tool(s):** `manage_accessibility`

**Actions (50):**

| Action | Status |
|--------|--------|
| `add_directional_indicators` | Implemented |
| `apply_accessibility_preset` | Implemented |
| `configure_audio_visualization` | Implemented |
| `configure_auto_aim_strength` | Implemented |
| `configure_button_holds` | Implemented |
| `configure_colorblind_mode` | Implemented |
| `configure_control_remapping` | Implemented |
| `configure_difficulty_presets` | Implemented |
| `configure_high_contrast_mode` | Implemented |
| `configure_hold_vs_toggle` | Implemented |
| `configure_mono_audio` | Implemented |
| `configure_motion_sickness_options` | Implemented |
| `configure_navigation_assistance` | Implemented |
| `configure_objective_reminders` | Implemented |
| `configure_one_handed_mode` | Implemented |
| `configure_quick_time_events` | Implemented |
| `configure_screen_narrator` | Implemented |
| `configure_screen_reader` | Implemented |
| `configure_speaker_identification` | Implemented |
| `configure_subtitle_background` | Implemented |
| `configure_subtitle_style` | Implemented |
| `configure_subtitle_timing` | Implemented |
| `configure_text_to_speech` | Implemented |
| `configure_tutorial_options` | Implemented |
| `configure_ui_simplification` | Implemented |
| `configure_visual_sound_cues` | Implemented |
| `create_accessibility_preset` | Implemented |
| `create_colorblind_filter` | Implemented |
| `create_control_remapping_ui` | Implemented |
| `create_sound_indicator_widget` | Implemented |
| `create_subtitle_widget` | Implemented |
| `export_accessibility_settings` | Implemented |
| `get_accessibility_info` | Implemented |
| `import_accessibility_settings` | Implemented |
| `reset_accessibility_defaults` | Implemented |
| `set_audio_accessibility_preset` | Implemented |
| `set_audio_balance` | Implemented |
| `set_audio_ducking` | Implemented |
| `set_cognitive_accessibility_preset` | Implemented |
| `set_colorblind_severity` | Implemented |
| `set_cursor_size` | Implemented |
| `set_font_size` | Implemented |
| `set_game_speed` | Implemented |
| `set_high_contrast_colors` | Implemented |
| `set_input_timing_tolerance` | Implemented |
| `set_motor_accessibility_preset` | Implemented |
| `set_subtitle_font_size` | Implemented |
| `set_subtitle_preset` | Implemented |
| `set_ui_scale` | Implemented |
| `set_visual_accessibility_preset` | Implemented |

---

## GameplaySystems

**File:** `McpAutomationBridge_GameplaySystemsHandlers.cpp`

**Lines:** 2,454

**Maps to TS Tool(s):** `manage_gameplay_systems`

**Actions (50):**

| Action | Status |
|--------|--------|
| `add_dialogue_node` | Implemented |
| `add_instance` | Implemented |
| `add_string_entry` | Implemented |
| `assign_actor_to_hlod` | Implemented |
| `build_hlod` | Implemented |
| `configure_aim_assist` | Implemented |
| `configure_checkpoint_data` | Implemented |
| `configure_hlod_settings` | Implemented |
| `configure_localization_entry` | Implemented |
| `configure_lock_on_target` | Implemented |
| `configure_marker_widget` | Implemented |
| `configure_minimap_icon` | Implemented |
| `configure_objective_markers` | Implemented |
| `configure_photo_mode_camera` | Implemented |
| `configure_save_system` | Implemented |
| `configure_scalability_group` | Implemented |
| `configure_targeting_priority` | Implemented |
| `create_checkpoint_actor` | Implemented |
| `create_device_profile` | Implemented |
| `create_dialogue_node` | Implemented |
| `create_dialogue_tree` | Implemented |
| `create_hierarchical_instanced_static_mesh` | Implemented |
| `create_hlod_layer` | Implemented |
| `create_instanced_static_mesh_component` | Implemented |
| `create_objective` | Implemented |
| `create_objective_chain` | Implemented |
| `create_ping_system` | Implemented |
| `create_quest_data_asset` | Implemented |
| `create_quest_stage` | Implemented |
| `create_string_table` | Implemented |
| `create_targeting_component` | Implemented |
| `create_world_marker` | Implemented |
| `enable_photo_mode` | Implemented |
| `export_localization` | Implemented |
| `get_available_cultures` | Implemented |
| `get_gameplay_systems_info` | Implemented |
| `get_instance_count` | Implemented |
| `get_scalability_settings` | Implemented |
| `get_string_entry` | Implemented |
| `import_localization` | Implemented |
| `load_checkpoint` | Implemented |
| `play_dialogue` | Implemented |
| `remove_instance` | Implemented |
| `save_checkpoint` | Implemented |
| `set_culture` | Implemented |
| `set_game_state` | Implemented |
| `set_objective_state` | Implemented |
| `set_quality_level` | Implemented |
| `set_resolution_scale` | Implemented |
| `take_photo_mode_screenshot` | Implemented |

---

## EditorUtilities

**File:** `McpAutomationBridge_EditorUtilitiesHandlers.cpp`

**Lines:** 1,493

**Maps to TS Tool(s):** `manage_editor_utilities`

**Actions (45):**

| Action | Status |
|--------|--------|
| `add_to_collection` | Implemented |
| `begin_transaction` | Implemented |
| `bind_to_event` | Implemented |
| `broadcast_event` | Implemented |
| `cancel_transaction` | Implemented |
| `clear_all_timers` | Implemented |
| `clear_timer` | Implemented |
| `configure_channel_responses` | Implemented |
| `configure_editor_preferences` | Implemented |
| `configure_surface_type` | Implemented |
| `create_blueprint_interface` | Implemented |
| `create_collection` | Implemented |
| `create_collision_channel` | Implemented |
| `create_collision_profile` | Implemented |
| `create_event_dispatcher` | Implemented |
| `create_game_instance_subsystem` | Implemented |
| `create_local_player_subsystem` | Implemented |
| `create_physical_material` | Implemented |
| `create_world_subsystem` | Implemented |
| `deselect_all` | Implemented |
| `end_transaction` | Implemented |
| `get_active_timers` | Implemented |
| `get_collision_info` | Implemented |
| `get_editor_utilities_info` | Implemented |
| `get_physical_material_info` | Implemented |
| `get_selected_actors` | Implemented |
| `get_subsystem_info` | Implemented |
| `get_transaction_history` | Implemented |
| `group_actors` | Implemented |
| `navigate_to_path` | Implemented |
| `redo` | Implemented |
| `select_actor` | Implemented |
| `select_actors_by_class` | Implemented |
| `select_actors_by_tag` | Implemented |
| `set_editor_mode` | Implemented |
| `set_friction` | Implemented |
| `set_grid_settings` | Implemented |
| `set_restitution` | Implemented |
| `set_snap_settings` | Implemented |
| `set_timer` | Implemented |
| `show_in_explorer` | Implemented |
| `sync_to_asset` | Implemented |
| `unbind_from_event` | Implemented |
| `undo` | Implemented |
| `ungroup_actors` | Implemented |

---

## Animation

**File:** `McpAutomationBridge_AnimationHandlers.cpp`

**Lines:** 2,736

**Maps to TS Tool(s):** `animation_physics`

**Actions (43):**

| Action | Status |
|--------|--------|
| `activate_ragdoll` | Implemented |
| `add_notify` | Implemented |
| `add_notify_old_unused` | Implemented |
| `add_rig_unit` | Implemented |
| `add_trajectory_prediction` | Implemented |
| `apply_pose_asset` | Implemented |
| `blend_ragdoll_to_animation` | Implemented |
| `chaos_create_cloth_config` | Implemented |
| `chaos_create_cloth_shared_sim_config` | Implemented |
| `chaos_get_plugin_status` | Implemented |
| `cleanup` | Implemented |
| `configure_layer_blend_mode` | Implemented |
| `configure_ml_deformer_training` | Implemented |
| `configure_ragdoll_profile` | Implemented |
| `configure_squash_stretch` | Implemented |
| `configure_vehicle` | Implemented |
| `connect_rig_elements` | Implemented |
| `create_anim_layer` | Implemented |
| `create_animation_asset` | Implemented |
| `create_animation_bp` | Implemented |
| `create_blend_space` | Implemented |
| `create_blend_tree` | Implemented |
| `create_chaos_cloth_config` | Implemented |
| `create_chaos_cloth_shared_sim_config` | Implemented |
| `create_control_rig_physics` | Implemented |
| `create_pose_library` | Implemented |
| `create_procedural_anim` | Implemented |
| `create_rigging_layer` | Implemented |
| `create_state_machine` | Implemented |
| `get_bone_transforms` | Implemented |
| `get_chaos_plugin_status` | Implemented |
| `get_control_rig_controls` | Implemented |
| `get_motion_matching_state` | Implemented |
| `list_pose_search_databases` | Implemented |
| `play_anim_montage` | Implemented |
| `play_montage` | Implemented |
| `reset_control_rig` | Implemented |
| `set_control_value` | Implemented |
| `set_motion_matching_goal` | Implemented |
| `setup_ik` | Implemented |
| `setup_physics_simulation` | Implemented |
| `setup_retargeting` | Implemented |
| `stack_anim_layers` | Implemented |

---

## Character

**File:** `McpAutomationBridge_CharacterHandlers.cpp`

**Lines:** 1,871

**Maps to TS Tool(s):** `manage_character`

**Actions (43):**

| Action | Status |
|--------|--------|
| `add_custom_movement_mode` | Implemented |
| `apply_status_effect` | Implemented |
| `batch_add_inventory_items` | Implemented |
| `configure_camera_component` | Implemented |
| `configure_capsule_component` | Implemented |
| `configure_destruction_damage` | Implemented |
| `configure_destruction_effects` | Implemented |
| `configure_destruction_levels` | Implemented |
| `configure_equipment_socket` | Implemented |
| `configure_footstep_fx` | Implemented |
| `configure_footstep_system` | Implemented |
| `configure_inventory_slot` | Implemented |
| `configure_jump` | Implemented |
| `configure_locomotion_state` | Implemented |
| `configure_mantle_vault` | Implemented |
| `configure_mesh_component` | Implemented |
| `configure_movement_speeds` | Implemented |
| `configure_nav_movement` | Implemented |
| `configure_rotation` | Implemented |
| `configure_trigger_filter` | Implemented |
| `configure_trigger_response` | Implemented |
| `create_character_blueprint` | Implemented |
| `fall` | Implemented |
| `falling` | Implemented |
| `fly` | Implemented |
| `flying` | Implemented |
| `get_character_info` | Implemented |
| `get_character_stats_snapshot` | Implemented |
| `map_surface_to_sound` | Implemented |
| `none` | Implemented |
| `query_interaction_targets` | Implemented |
| `set_movement_mode` | Implemented |
| `setup_climbing` | Implemented |
| `setup_footstep_system` | Implemented |
| `setup_grappling` | Implemented |
| `setup_mantling` | Implemented |
| `setup_sliding` | Implemented |
| `setup_vaulting` | Implemented |
| `setup_wall_running` | Implemented |
| `swim` | Implemented |
| `swimming` | Implemented |
| `walk` | Implemented |
| `walking` | Implemented |

---

## PCG

**File:** `McpAutomationBridge_PCGHandlers.cpp`

**Lines:** 1,726

**Maps to TS Tool(s):** `manage_level`

**Actions (41):**

| Action | Status |
|--------|--------|
| `add_actor_data_node` | Implemented |
| `add_actor_spawner` | Implemented |
| `add_bounds_filter` | Implemented |
| `add_bounds_modifier` | Implemented |
| `add_copy_points` | Implemented |
| `add_density_filter` | Implemented |
| `add_distance_filter` | Implemented |
| `add_height_filter` | Implemented |
| `add_landscape_data_node` | Implemented |
| `add_merge_points` | Implemented |
| `add_mesh_sampler` | Implemented |
| `add_pcg_node` | Implemented |
| `add_project_to_surface` | Implemented |
| `add_self_pruning` | Implemented |
| `add_slope_filter` | Implemented |
| `add_spline_data_node` | Implemented |
| `add_spline_sampler` | Implemented |
| `add_spline_spawner` | Implemented |
| `add_static_mesh_spawner` | Implemented |
| `add_surface_sampler` | Implemented |
| `add_texture_data_node` | Implemented |
| `add_transform_points` | Implemented |
| `add_volume_data_node` | Implemented |
| `add_volume_sampler` | Implemented |
| `batch_execute_pcg_with_gpu` | Implemented |
| `blend_biomes` | Implemented |
| `configure_pcg_mode_brush` | Implemented |
| `connect_pcg_pins` | Implemented |
| `create_biome_rules` | Implemented |
| `create_pcg_graph` | Implemented |
| `create_pcg_hlsl_node` | Implemented |
| `create_pcg_subgraph` | Implemented |
| `debug_pcg_execution` | Implemented |
| `enable_pcg_gpu_processing` | Implemented |
| `execute_pcg_graph` | Implemented |
| `export_pcg_hlsl_template` | Implemented |
| `export_pcg_to_static` | Implemented |
| `get_pcg_info` | Implemented |
| `import_pcg_preset` | Implemented |
| `set_pcg_node_settings` | Implemented |
| `set_pcg_partition_grid_size` | Implemented |

---

## AnimationAuthoring

**File:** `McpAutomationBridge_AnimationAuthoringHandlers.cpp`

**Lines:** 2,664

**Maps to TS Tool(s):** `animation_physics`

**Actions (40):**

| Action | Status |
|--------|--------|
| `add_aim_offset_sample` | Implemented |
| `add_blend_node` | Implemented |
| `add_blend_sample` | Implemented |
| `add_bone_track` | Implemented |
| `add_cached_pose` | Implemented |
| `add_ik_chain` | Implemented |
| `add_layered_blend_per_bone` | Implemented |
| `add_montage_notify` | Implemented |
| `add_montage_section` | Implemented |
| `add_montage_slot` | Implemented |
| `add_notify` | Implemented |
| `add_notify_state` | Implemented |
| `add_slot_node` | Implemented |
| `add_state` | Implemented |
| `add_state_machine` | Implemented |
| `add_sync_marker` | Implemented |
| `add_transition` | Implemented |
| `create_aim_offset` | Implemented |
| `create_anim_blueprint` | Implemented |
| `create_animation_sequence` | Implemented |
| `create_blend_space_1d` | Implemented |
| `create_blend_space_2d` | Implemented |
| `create_ik_retargeter` | Implemented |
| `create_ik_rig` | Implemented |
| `create_montage` | Implemented |
| `get_animation_info` | Implemented |
| `link_sections` | Implemented |
| `set_additive_settings` | Implemented |
| `set_anim_graph_node_value` | Implemented |
| `set_axis_settings` | Implemented |
| `set_blend_in` | Implemented |
| `set_blend_out` | Implemented |
| `set_bone_key` | Implemented |
| `set_curve_key` | Implemented |
| `set_interpolation_settings` | Implemented |
| `set_retarget_chain_mapping` | Implemented |
| `set_root_motion_settings` | Implemented |
| `set_section_timing` | Implemented |
| `set_sequence_length` | Implemented |
| `set_transition_rules` | Implemented |

---

## Combat

**File:** `McpAutomationBridge_CombatHandlers.cpp`

**Lines:** 2,688

**Maps to TS Tool(s):** `manage_combat`

**Actions (40):**

| Action | Status |
|--------|--------|
| `apply_damage_with_effects` | Implemented |
| `configure_aim_down_sights` | Implemented |
| `configure_block_parry` | Implemented |
| `configure_combo_system` | Implemented |
| `configure_damage_execution` | Implemented |
| `configure_gas_effect` | Implemented |
| `configure_hit_reaction` | Implemented |
| `configure_hitscan` | Implemented |
| `configure_impact_effects` | Implemented |
| `configure_melee_trace` | Implemented |
| `configure_muzzle_flash` | Implemented |
| `configure_projectile` | Implemented |
| `configure_projectile_collision` | Implemented |
| `configure_projectile_homing` | Implemented |
| `configure_projectile_movement` | Implemented |
| `configure_recoil_pattern` | Implemented |
| `configure_shell_ejection` | Implemented |
| `configure_spread_pattern` | Implemented |
| `configure_tracer` | Implemented |
| `configure_weapon_mesh` | Implemented |
| `configure_weapon_sockets` | Implemented |
| `configure_weapon_trace` | Implemented |
| `configure_weapon_trails` | Implemented |
| `create_combo_sequence` | Implemented |
| `create_damage_type` | Implemented |
| `create_hit_pause` | Implemented |
| `create_melee_trace` | Implemented |
| `create_projectile_blueprint` | Implemented |
| `create_projectile_pool` | Implemented |
| `create_weapon_blueprint` | Implemented |
| `get_combat_info` | Implemented |
| `get_combat_stats` | Implemented |
| `grant_gas_ability` | Implemented |
| `set_weapon_stats` | Implemented |
| `setup_ammo_system` | Implemented |
| `setup_attachment_system` | Implemented |
| `setup_hitbox_component` | Implemented |
| `setup_parry_block_system` | Implemented |
| `setup_reload_system` | Implemented |
| `setup_weapon_switching` | Implemented |

---

## GAS

**File:** `McpAutomationBridge_GASHandlers.cpp`

**Lines:** 2,396

**Maps to TS Tool(s):** `manage_combat`

**Actions (38):**

| Action | Status |
|--------|--------|
| `add` | Implemented |
| `add_ability_system_component` | Implemented |
| `add_ability_task` | Implemented |
| `add_attribute` | Implemented |
| `add_effect_cue` | Implemented |
| `add_effect_execution_calculation` | Implemented |
| `add_effect_modifier` | Implemented |
| `add_tag_to_asset` | Implemented |
| `additive` | Implemented |
| `configure_asc` | Implemented |
| `configure_cue_trigger` | Implemented |
| `create_attribute_set` | Implemented |
| `create_gameplay_ability` | Implemented |
| `create_gameplay_cue_notify` | Implemented |
| `create_gameplay_effect` | Implemented |
| `divide` | Implemented |
| `division` | Implemented |
| `get_gas_info` | Implemented |
| `multiplicative` | Implemented |
| `multiply` | Implemented |
| `override` | Implemented |
| `set_ability_cooldown` | Implemented |
| `set_ability_costs` | Implemented |
| `set_ability_tags` | Implemented |
| `set_ability_targeting` | Implemented |
| `set_activation_policy` | Implemented |
| `set_attribute_base_value` | Implemented |
| `set_attribute_clamping` | Implemented |
| `set_cue_effects` | Implemented |
| `set_effect_duration` | Implemented |
| `set_effect_stacking` | Implemented |
| `set_effect_tags` | Implemented |
| `set_instancing_policy` | Implemented |
| `set_modifier_magnitude` | Implemented |
| `test_activate_ability` | Implemented |
| `test_apply_effect` | Implemented |
| `test_get_attribute` | Implemented |
| `test_get_gameplay_tags` | Implemented |

---

## Data

**File:** `McpAutomationBridge_DataHandlers.cpp`

**Lines:** 1,358

**Maps to TS Tool(s):** `manage_data`

**Actions (37):**

| Action | Status |
|--------|--------|
| `add_curve_row` | Implemented |
| `add_data_table_row` | Implemented |
| `add_native_gameplay_tag` | Implemented |
| `add_tag_to_container` | Implemented |
| `check_tag_match` | Implemented |
| `create_curve_table` | Implemented |
| `create_data_asset` | Implemented |
| `create_data_table` | Implemented |
| `create_gameplay_tag` | Implemented |
| `create_primary_data_asset` | Implemented |
| `create_save_game_blueprint` | Implemented |
| `create_tag_container` | Implemented |
| `delete_save_slot` | Implemented |
| `does_save_exist` | Implemented |
| `empty_data_table` | Implemented |
| `export_curve_table_csv` | Implemented |
| `export_data_table_csv` | Implemented |
| `flush_config` | Implemented |
| `get_all_gameplay_tags` | Implemented |
| `get_config_section` | Implemented |
| `get_curve_value` | Implemented |
| `get_data_asset_info` | Implemented |
| `get_data_table_row` | Implemented |
| `get_data_table_rows` | Implemented |
| `get_save_slot_names` | Implemented |
| `has_tag` | Implemented |
| `import_curve_table_csv` | Implemented |
| `import_data_table_csv` | Implemented |
| `load_game_from_slot` | Implemented |
| `read_config_value` | Implemented |
| `reload_config` | Implemented |
| `remove_data_table_row` | Implemented |
| `remove_tag_from_container` | Implemented |
| `request_gameplay_tag` | Implemented |
| `save_game_to_slot` | Implemented |
| `set_data_asset_property` | Implemented |
| `write_config_value` | Implemented |

---

## Networking

**File:** `McpAutomationBridge_NetworkingHandlers.cpp`

**Lines:** 1,923

**Maps to TS Tool(s):** `manage_networking`

**Actions (37):**

| Action | Status |
|--------|--------|
| `add_network_prediction_data` | Implemented |
| `check_has_authority` | Implemented |
| `check_is_locally_controlled` | Implemented |
| `configure_client_prediction` | Implemented |
| `configure_dormancy` | Implemented |
| `configure_movement_prediction` | Implemented |
| `configure_net_cull_distance` | Implemented |
| `configure_net_driver` | Implemented |
| `configure_net_priority` | Implemented |
| `configure_net_relevancy` | Implemented |
| `configure_net_serialization` | Implemented |
| `configure_net_update_frequency` | Implemented |
| `configure_prediction_settings` | Implemented |
| `configure_push_model` | Implemented |
| `configure_replicated_movement` | Implemented |
| `configure_replication_graph` | Implemented |
| `configure_rpc_validation` | Implemented |
| `configure_server_correction` | Implemented |
| `configure_team_settings` | Implemented |
| `create_rpc_function` | Implemented |
| `debug_replication_graph` | Implemented |
| `get_net_role_info` | Implemented |
| `get_networking_info` | Implemented |
| `get_rpc_statistics` | Implemented |
| `get_session_players` | Implemented |
| `send_server_rpc` | Implemented |
| `set_always_relevant` | Implemented |
| `set_autonomous_proxy` | Implemented |
| `set_net_dormancy` | Implemented |
| `set_net_role` | Implemented |
| `set_only_relevant_to_owner` | Implemented |
| `set_owner` | Implemented |
| `set_property_replicated` | Implemented |
| `set_replicated_using` | Implemented |
| `set_replication_condition` | Implemented |
| `set_rpc_reliability` | Implemented |
| `simulate_network_conditions` | Implemented |

---

## NiagaraAuthoring

**File:** `McpAutomationBridge_NiagaraAuthoringHandlers.cpp`

**Lines:** 2,658

**Maps to TS Tool(s):** `manage_effect`

**Actions (36):**

| Action | Status |
|--------|--------|
| `add_acceleration_module` | Implemented |
| `add_audio_spectrum_data_interface` | Implemented |
| `add_camera_offset_module` | Implemented |
| `add_collision_module` | Implemented |
| `add_collision_query_data_interface` | Implemented |
| `add_color_module` | Implemented |
| `add_emitter_to_system` | Implemented |
| `add_event_generator` | Implemented |
| `add_event_receiver` | Implemented |
| `add_force_module` | Implemented |
| `add_initialize_particle_module` | Implemented |
| `add_kill_particles_module` | Implemented |
| `add_light_renderer_module` | Implemented |
| `add_mesh_renderer_module` | Implemented |
| `add_particle_state_module` | Implemented |
| `add_ribbon_renderer_module` | Implemented |
| `add_simulation_stage` | Implemented |
| `add_size_module` | Implemented |
| `add_skeletal_mesh_data_interface` | Implemented |
| `add_spawn_burst_module` | Implemented |
| `add_spawn_per_unit_module` | Implemented |
| `add_spawn_rate_module` | Implemented |
| `add_spline_data_interface` | Implemented |
| `add_sprite_renderer_module` | Implemented |
| `add_static_mesh_data_interface` | Implemented |
| `add_user_parameter` | Implemented |
| `add_velocity_module` | Implemented |
| `bind_parameter_to_source` | Implemented |
| `configure_event_payload` | Implemented |
| `create_niagara_emitter` | Implemented |
| `create_niagara_system` | Implemented |
| `enable_gpu_simulation` | Implemented |
| `get_niagara_info` | Implemented |
| `set_emitter_properties` | Implemented |
| `set_parameter_value` | Implemented |
| `validate_niagara_system` | Implemented |

---

## LevelStructure

**File:** `McpAutomationBridge_LevelStructureHandlers.cpp`

**Lines:** 2,894

**Maps to TS Tool(s):** `manage_level`

**Actions (35):**

| Action | Status |
|--------|--------|
| `add_level_blueprint_node` | Implemented |
| `assign_actor_to_data_layer` | Implemented |
| `build_hlod_for_level` | Implemented |
| `configure_grid_size` | Implemented |
| `configure_hlod_layer` | Implemented |
| `configure_hlod_settings` | Implemented |
| `configure_large_world_coordinates` | Implemented |
| `configure_level_bounds` | Implemented |
| `configure_level_streaming` | Implemented |
| `configure_runtime_loading` | Implemented |
| `configure_world_partition` | Implemented |
| `configure_world_settings` | Implemented |
| `connect_level_blueprint_nodes` | Implemented |
| `create_data_layer` | Implemented |
| `create_level` | Implemented |
| `create_level_instance` | Implemented |
| `create_minimap_volume` | Implemented |
| `create_packed_level_actor` | Implemented |
| `create_streaming_volume` | Implemented |
| `create_sublevel` | Implemented |
| `create_world_partition_cell` | Implemented |
| `enable_world_partition` | Implemented |
| `get_level_structure_info` | Implemented |
| `get_streaming_levels_status` | Implemented |
| `get_summary` | Implemented |
| `get_world_partition_cells` | Implemented |
| `hide` | Implemented |
| `load` | Implemented |
| `open_level_blueprint` | Implemented |
| `set_metadata` | Implemented |
| `set_streaming_distance` | Implemented |
| `show` | Implemented |
| `stream_level_async` | Implemented |
| `unload` | Implemented |
| `validate_level` | Implemented |

---

## MovieRender

**File:** `McpAutomationBridge_MovieRenderHandlers.cpp`

**Lines:** 1,146

**Maps to TS Tool(s):** `manage_sequence`

**Actions (33):**

| Action | Status |
|--------|--------|
| `add_burn_in` | Implemented |
| `add_console_variable` | Implemented |
| `add_job` | Implemented |
| `add_render_pass` | Implemented |
| `batch_render_sequences` | Implemented |
| `clear_queue` | Implemented |
| `configure_anti_aliasing` | Implemented |
| `configure_burn_in` | Implemented |
| `configure_high_res_settings` | Implemented |
| `configure_job` | Implemented |
| `configure_mrq_settings` | Implemented |
| `configure_output` | Implemented |
| `configure_render_pass` | Implemented |
| `create_queue` | Implemented |
| `get_queue` | Implemented |
| `get_render_passes` | Implemented |
| `get_render_progress` | Implemented |
| `get_render_status` | Implemented |
| `remove_burn_in` | Implemented |
| `remove_console_variable` | Implemented |
| `remove_job` | Implemented |
| `remove_render_pass` | Implemented |
| `set_file_name_format` | Implemented |
| `set_frame_rate` | Implemented |
| `set_map` | Implemented |
| `set_output_directory` | Implemented |
| `set_resolution` | Implemented |
| `set_sequence` | Implemented |
| `set_spatial_sample_count` | Implemented |
| `set_temporal_sample_count` | Implemented |
| `set_tile_count` | Implemented |
| `start_render` | Implemented |
| `stop_render` | Implemented |

---

## Sequence

**File:** `McpAutomationBridge_SequenceHandlers.cpp`

**Lines:** 2,733

**Maps to TS Tool(s):** `manage_sequence`

**Actions (33):**

| Action | Status |
|--------|--------|
| `create` | Implemented |
| `sequence_add_actor` | Implemented |
| `sequence_add_actors` | Implemented |
| `sequence_add_camera` | Implemented |
| `sequence_add_keyframe` | Implemented |
| `sequence_add_section` | Implemented |
| `sequence_add_spawnable_from_class` | Implemented |
| `sequence_add_track` | Implemented |
| `sequence_create` | Implemented |
| `sequence_delete` | Implemented |
| `sequence_duplicate` | Implemented |
| `sequence_get_bindings` | Implemented |
| `sequence_get_metadata` | Implemented |
| `sequence_get_properties` | Implemented |
| `sequence_list` | Implemented |
| `sequence_list_track_types` | Implemented |
| `sequence_list_tracks` | Implemented |
| `sequence_open` | Implemented |
| `sequence_pause` | Implemented |
| `sequence_play` | Implemented |
| `sequence_remove_actors` | Implemented |
| `sequence_remove_track` | Implemented |
| `sequence_rename` | Implemented |
| `sequence_set_display_rate` | Implemented |
| `sequence_set_playback_speed` | Implemented |
| `sequence_set_properties` | Implemented |
| `sequence_set_tick_resolution` | Implemented |
| `sequence_set_track_locked` | Implemented |
| `sequence_set_track_muted` | Implemented |
| `sequence_set_track_solo` | Implemented |
| `sequence_set_view_range` | Implemented |
| `sequence_set_work_range` | Implemented |
| `sequence_stop` | Implemented |

---

## PostProcess

**File:** `McpAutomationBridge_PostProcessHandlers.cpp`

**Lines:** 1,522

**Maps to TS Tool(s):** `manage_lighting`

**Actions (31):**

| Action | Status |
|--------|--------|
| `build_lighting_quality` | Implemented |
| `capture_scene` | Implemented |
| `configure_bloom` | Implemented |
| `configure_chromatic_aberration` | Implemented |
| `configure_color_grading` | Implemented |
| `configure_dof` | Implemented |
| `configure_film_grain` | Implemented |
| `configure_indirect_lighting_cache` | Implemented |
| `configure_lens_flares` | Implemented |
| `configure_lightmass_settings` | Implemented |
| `configure_motion_blur` | Implemented |
| `configure_path_tracing` | Implemented |
| `configure_pp_blend` | Implemented |
| `configure_pp_priority` | Implemented |
| `configure_ray_traced_ao` | Implemented |
| `configure_ray_traced_gi` | Implemented |
| `configure_ray_traced_reflections` | Implemented |
| `configure_ray_traced_shadows` | Implemented |
| `configure_vignette` | Implemented |
| `configure_volumetric_lightmap` | Implemented |
| `configure_white_balance` | Implemented |
| `create_box_reflection_capture` | Implemented |
| `create_planar_reflection` | Implemented |
| `create_post_process_volume` | Implemented |
| `create_scene_capture_2d` | Implemented |
| `create_scene_capture_cube` | Implemented |
| `create_sphere_reflection_capture` | Implemented |
| `get_post_process_settings` | Implemented |
| `recapture_scene` | Implemented |
| `set_actor_light_channel` | Implemented |
| `set_light_channel` | Implemented |

---

## AINPC

**File:** `McpAutomationBridge_AINPCHandlers.cpp`

**Lines:** 1,438

**Maps to TS Tool(s):** `manage_ai`

**Actions (30):**

| Action | Status |
|--------|--------|
| `configure_ace_emotions` | Implemented |
| `configure_audio2face` | Implemented |
| `configure_blendshape_mapping` | Implemented |
| `configure_character_backstory` | Implemented |
| `configure_character_voice` | Implemented |
| `configure_convai_actions` | Implemented |
| `configure_convai_lipsync` | Implemented |
| `configure_inworld_scene` | Implemented |
| `configure_inworld_settings` | Implemented |
| `create_convai_character` | Implemented |
| `create_inworld_character` | Implemented |
| `get_ace_info` | Implemented |
| `get_ai_npc_info` | Implemented |
| `get_audio2face_status` | Implemented |
| `get_character_emotion` | Implemented |
| `get_character_goals` | Implemented |
| `get_character_response` | Implemented |
| `get_convai_info` | Implemented |
| `get_inworld_info` | Implemented |
| `list_available_ai_backends` | Implemented |
| `process_audio_to_blendshapes` | Implemented |
| `send_message_to_character` | Implemented |
| `send_text_to_character` | Implemented |
| `start_audio2face_stream` | Implemented |
| `start_convai_session` | Implemented |
| `start_inworld_session` | Implemented |
| `stop_audio2face_stream` | Implemented |
| `stop_convai_session` | Implemented |
| `stop_inworld_session` | Implemented |
| `trigger_inworld_event` | Implemented |

---

## AudioAuthoring

**File:** `McpAutomationBridge_AudioAuthoringHandlers.cpp`

**Lines:** 2,191

**Maps to TS Tool(s):** `manage_audio`

**Actions (30):**

| Action | Status |
|--------|--------|
| `add_cue_node` | Implemented |
| `add_metasound_input` | Implemented |
| `add_metasound_node` | Implemented |
| `add_metasound_output` | Implemented |
| `add_mix_modifier` | Implemented |
| `add_source_effect` | Implemented |
| `configure_distance_attenuation` | Implemented |
| `configure_mix_eq` | Implemented |
| `configure_occlusion` | Implemented |
| `configure_reverb_send` | Implemented |
| `configure_spatialization` | Implemented |
| `connect_cue_nodes` | Implemented |
| `connect_metasound_nodes` | Implemented |
| `create_attenuation_settings` | Implemented |
| `create_dialogue_voice` | Implemented |
| `create_dialogue_wave` | Implemented |
| `create_metasound` | Implemented |
| `create_reverb_effect` | Implemented |
| `create_sound_class` | Implemented |
| `create_sound_cue` | Implemented |
| `create_sound_mix` | Implemented |
| `create_source_effect_chain` | Implemented |
| `create_submix_effect` | Implemented |
| `get_audio_info` | Implemented |
| `set_class_parent` | Implemented |
| `set_class_properties` | Implemented |
| `set_cue_attenuation` | Implemented |
| `set_cue_concurrency` | Implemented |
| `set_dialogue_context` | Implemented |
| `set_metasound_default` | Implemented |

---

## Skeleton

**File:** `McpAutomationBridge_SkeletonHandlers.cpp`

**Lines:** 2,693

**Maps to TS Tool(s):** `manage_skeleton`

**Actions (29):**

| Action | Status |
|--------|--------|
| `add_bone` | Implemented |
| `add_physics_body` | Implemented |
| `add_physics_constraint` | Implemented |
| `assign_cloth_asset_to_mesh` | Implemented |
| `auto_skin_weights` | Implemented |
| `bind_cloth_to_skeletal_mesh` | Implemented |
| `configure_constraint_limits` | Implemented |
| `configure_physics_body` | Implemented |
| `configure_socket` | Implemented |
| `copy_weights` | Implemented |
| `create_morph_target` | Implemented |
| `create_physics_asset` | Implemented |
| `create_skeleton` | Implemented |
| `create_socket` | Implemented |
| `create_virtual_bone` | Implemented |
| `get_skeleton_info` | Implemented |
| `import_morph_targets` | Implemented |
| `list_bones` | Implemented |
| `list_physics_bodies` | Implemented |
| `list_sockets` | Implemented |
| `mirror_weights` | Implemented |
| `normalize_weights` | Implemented |
| `prune_weights` | Implemented |
| `remove_bone` | Implemented |
| `rename_bone` | Implemented |
| `set_bone_parent` | Implemented |
| `set_bone_transform` | Implemented |
| `set_morph_target_deltas` | Implemented |
| `set_vertex_weights` | Implemented |

---

## Inventory

**File:** `McpAutomationBridge_InventoryHandlers.cpp`

**Lines:** 2,405

**Maps to TS Tool(s):** `manage_character`

**Actions (27):**

| Action | Status |
|--------|--------|
| `add_crafting_component` | Implemented |
| `add_equipment_functions` | Implemented |
| `add_inventory_functions` | Implemented |
| `add_loot_entry` | Implemented |
| `assign_item_category` | Implemented |
| `configure_equipment_effects` | Implemented |
| `configure_equipment_visuals` | Implemented |
| `configure_inventory_events` | Implemented |
| `configure_inventory_slots` | Implemented |
| `configure_loot_drop` | Implemented |
| `configure_pickup_effects` | Implemented |
| `configure_pickup_interaction` | Implemented |
| `configure_pickup_respawn` | Implemented |
| `configure_recipe_requirements` | Implemented |
| `create_crafting_recipe` | Implemented |
| `create_crafting_station` | Implemented |
| `create_equipment_component` | Implemented |
| `create_inventory_component` | Implemented |
| `create_item_category` | Implemented |
| `create_item_data_asset` | Implemented |
| `create_loot_table` | Implemented |
| `create_pickup_actor` | Implemented |
| `define_equipment_slots` | Implemented |
| `get_inventory_info` | Implemented |
| `set_inventory_replication` | Implemented |
| `set_item_properties` | Implemented |
| `set_loot_quality_tiers` | Implemented |

---

## Volume

**File:** `McpAutomationBridge_VolumeHandlers.cpp`

**Lines:** 1,462

**Maps to TS Tool(s):** `manage_volumes`

**Actions (26):**

| Action | Status |
|--------|--------|
| `create_audio_volume` | Implemented |
| `create_blocking_volume` | Implemented |
| `create_cable_spline` | Implemented |
| `create_camera_blocking_volume` | Implemented |
| `create_cull_distance_volume` | Implemented |
| `create_fence_spline` | Implemented |
| `create_kill_z_volume` | Implemented |
| `create_lightmass_importance_volume` | Implemented |
| `create_nav_mesh_bounds_volume` | Implemented |
| `create_nav_modifier_volume` | Implemented |
| `create_pain_causing_volume` | Implemented |
| `create_physics_volume` | Implemented |
| `create_pipe_spline` | Implemented |
| `create_precomputed_visibility_volume` | Implemented |
| `create_reverb_volume` | Implemented |
| `create_river_spline` | Implemented |
| `create_road_spline` | Implemented |
| `create_trigger_box` | Implemented |
| `create_trigger_capsule` | Implemented |
| `create_trigger_sphere` | Implemented |
| `create_trigger_volume` | Implemented |
| `create_wall_spline` | Implemented |
| `get_splines_info` | Implemented |
| `get_volumes_info` | Implemented |
| `set_volume_extent` | Implemented |
| `set_volume_properties` | Implemented |

---

## Media

**File:** `McpAutomationBridge_MediaHandlers.cpp`

**Lines:** 1,004

**Maps to TS Tool(s):** `manage_skeleton`

**Actions (25):**

| Action | Status |
|--------|--------|
| `add_to_playlist` | Implemented |
| `bind_to_texture` | Implemented |
| `close` | Implemented |
| `create_file_media_source` | Implemented |
| `create_media_player` | Implemented |
| `create_media_playlist` | Implemented |
| `create_media_sound_wave` | Implemented |
| `create_media_texture` | Implemented |
| `create_stream_media_source` | Implemented |
| `delete_media_asset` | Implemented |
| `get_duration` | Implemented |
| `get_media_info` | Implemented |
| `get_playlist` | Implemented |
| `get_state` | Implemented |
| `get_time` | Implemented |
| `open_source` | Implemented |
| `open_url` | Implemented |
| `pause` | Implemented |
| `play` | Implemented |
| `remove_from_playlist` | Implemented |
| `seek` | Implemented |
| `set_looping` | Implemented |
| `set_rate` | Implemented |
| `stop` | Implemented |
| `unbind_from_texture` | Implemented |

---

## Modding

**File:** `McpAutomationBridge_ModdingHandlers.cpp`

**Lines:** 1,231

**Maps to TS Tool(s):** `manage_data`

**Actions (25):**

| Action | Status |
|--------|--------|
| `check_mod_compatibility` | Implemented |
| `configure_asset_override_paths` | Implemented |
| `configure_exposed_classes` | Implemented |
| `configure_mod_load_order` | Implemented |
| `configure_mod_loading_paths` | Implemented |
| `configure_mod_sandbox` | Implemented |
| `create_mod_template_project` | Implemented |
| `disable_mod` | Implemented |
| `enable_mod` | Implemented |
| `export_moddable_headers` | Implemented |
| `get_mod_info` | Implemented |
| `get_modding_info` | Implemented |
| `get_sdk_config` | Implemented |
| `get_security_config` | Implemented |
| `list_asset_overrides` | Implemented |
| `list_installed_mods` | Implemented |
| `load_mod_pak` | Implemented |
| `register_mod_asset_redirect` | Implemented |
| `reset_mod_system` | Implemented |
| `restore_original_asset` | Implemented |
| `scan_for_mod_paks` | Implemented |
| `set_allowed_mod_operations` | Implemented |
| `unload_mod_pak` | Implemented |
| `validate_mod_content` | Implemented |
| `validate_mod_pak` | Implemented |

---

## Build

**File:** `McpAutomationBridge_BuildHandlers.cpp`

**Lines:** 849

**Maps to TS Tool(s):** `manage_build`

**Actions (24):**

| Action | Status |
|--------|--------|
| `audit_assets` | Implemented |
| `clear_ddc` | Implemented |
| `compile_shaders` | Implemented |
| `configure_build_settings` | Implemented |
| `configure_chunking` | Implemented |
| `configure_ddc` | Implemented |
| `configure_encryption` | Implemented |
| `configure_platform` | Implemented |
| `cook_content` | Implemented |
| `create_pak_file` | Implemented |
| `disable_plugin` | Implemented |
| `enable_plugin` | Implemented |
| `generate_project_files` | Implemented |
| `get_asset_references` | Implemented |
| `get_asset_size_info` | Implemented |
| `get_build_info` | Implemented |
| `get_ddc_stats` | Implemented |
| `get_platform_settings` | Implemented |
| `get_plugin_info` | Implemented |
| `get_target_platforms` | Implemented |
| `list_plugins` | Implemented |
| `package_project` | Implemented |
| `run_ubt` | Implemented |
| `validate_assets` | Implemented |

---

## Testing

**File:** `McpAutomationBridge_TestingHandlers.cpp`

**Lines:** 750

**Maps to TS Tool(s):** `manage_build`

**Actions (24):**

| Action | Status |
|--------|--------|
| `check_map_errors` | Implemented |
| `disable_visual_logger` | Implemented |
| `enable_visual_logger` | Implemented |
| `fix_redirectors` | Implemented |
| `get_functional_test_results` | Implemented |
| `get_memory_report` | Implemented |
| `get_performance_stats` | Implemented |
| `get_redirectors` | Implemented |
| `get_test_info` | Implemented |
| `get_test_results` | Implemented |
| `get_trace_status` | Implemented |
| `get_visual_logger_status` | Implemented |
| `list_functional_tests` | Implemented |
| `list_tests` | Implemented |
| `run_functional_test` | Implemented |
| `run_test` | Implemented |
| `run_tests` | Implemented |
| `start_stats_capture` | Implemented |
| `start_trace` | Implemented |
| `stop_stats_capture` | Implemented |
| `stop_trace` | Implemented |
| `validate_asset` | Implemented |
| `validate_assets_in_path` | Implemented |
| `validate_blueprint` | Implemented |

---

## Spline

**File:** `McpAutomationBridge_SplineHandlers.cpp`

**Lines:** 1,513

**Maps to TS Tool(s):** `manage_volumes`

**Actions (22):**

| Action | Status |
|--------|--------|
| `add_spline_point` | Implemented |
| `configure_mesh_randomization` | Implemented |
| `configure_mesh_spacing` | Implemented |
| `configure_spline_mesh_axis` | Implemented |
| `create_cable_spline` | Implemented |
| `create_fence_spline` | Implemented |
| `create_pipe_spline` | Implemented |
| `create_river_spline` | Implemented |
| `create_road_spline` | Implemented |
| `create_spline_actor` | Implemented |
| `create_spline_mesh_component` | Implemented |
| `create_wall_spline` | Implemented |
| `get_splines_info` | Implemented |
| `remove_spline_point` | Implemented |
| `scatter_meshes_along_spline` | Implemented |
| `set_spline_mesh_asset` | Implemented |
| `set_spline_mesh_material` | Implemented |
| `set_spline_point_position` | Implemented |
| `set_spline_point_rotation` | Implemented |
| `set_spline_point_scale` | Implemented |
| `set_spline_point_tangents` | Implemented |
| `set_spline_type` | Implemented |

---

## Level

**File:** `McpAutomationBridge_LevelHandlers.cpp`

**Lines:** 920

**Maps to TS Tool(s):** `manage_level`

**Actions (21):**

| Action | Status |
|--------|--------|
| `add_sublevel` | Implemented |
| `bake_lightmap` | Implemented |
| `build_lighting` | Implemented |
| `create_level` | Implemented |
| `create_light` | Implemented |
| `create_new_level` | Implemented |
| `delete` | Implemented |
| `export_level` | Implemented |
| `import_level` | Implemented |
| `list` | Implemented |
| `list_levels` | Implemented |
| `load` | Implemented |
| `load_level` | Implemented |
| `manage_level` | Implemented |
| `save` | Implemented |
| `save_as` | Implemented |
| `save_current_level` | Implemented |
| `save_level_as` | Implemented |
| `spawn_light` | Implemented |
| `stream` | Implemented |
| `stream_level` | Implemented |

---

## Texture

**File:** `McpAutomationBridge_TextureHandlers.cpp`

**Lines:** 2,057

**Maps to TS Tool(s):** `manage_material_authoring`

**Actions (21):**

| Action | Status |
|--------|--------|
| `adjust_curves` | Implemented |
| `adjust_levels` | Implemented |
| `blur` | Implemented |
| `channel_extract` | Implemented |
| `channel_pack` | Implemented |
| `combine_textures` | Implemented |
| `configure_virtual_texture` | Implemented |
| `create_ao_from_mesh` | Implemented |
| `create_gradient_texture` | Implemented |
| `create_noise_texture` | Implemented |
| `create_normal_from_height` | Implemented |
| `create_pattern_texture` | Implemented |
| `desaturate` | Implemented |
| `get_texture_info` | Implemented |
| `invert` | Implemented |
| `resize_texture` | Implemented |
| `set_compression_settings` | Implemented |
| `set_lod_bias` | Implemented |
| `set_streaming_priority` | Implemented |
| `set_texture_group` | Implemented |
| `sharpen` | Implemented |

---

## GameFramework

**File:** `McpAutomationBridge_GameFrameworkHandlers.cpp`

**Lines:** 1,793

**Maps to TS Tool(s):** `manage_networking`

**Actions (20):**

| Action | Status |
|--------|--------|
| `configure_game_rules` | Implemented |
| `configure_player_start` | Implemented |
| `configure_round_system` | Implemented |
| `configure_scoring_system` | Implemented |
| `configure_spawn_system` | Implemented |
| `configure_spectating` | Implemented |
| `configure_team_system` | Implemented |
| `create_game_instance` | Implemented |
| `create_game_mode` | Implemented |
| `create_game_state` | Implemented |
| `create_hud_class` | Implemented |
| `create_player_controller` | Implemented |
| `create_player_state` | Implemented |
| `get_game_framework_info` | Implemented |
| `set_default_pawn_class` | Implemented |
| `set_game_state_class` | Implemented |
| `set_player_controller_class` | Implemented |
| `set_player_state_class` | Implemented |
| `set_respawn_rules` | Implemented |
| `setup_match_states` | Implemented |

---

## Performance

**File:** `McpAutomationBridge_PerformanceHandlers.cpp`

**Lines:** 570

**Maps to TS Tool(s):** `manage_performance`

**Actions (20):**

| Action | Status |
|--------|--------|
| `apply_baseline_settings` | Implemented |
| `configure_lod` | Implemented |
| `configure_nanite` | Implemented |
| `configure_occlusion_culling` | Implemented |
| `configure_texture_streaming` | Implemented |
| `configure_world_partition` | Implemented |
| `enable_gpu_timing` | Implemented |
| `generate_memory_report` | Implemented |
| `merge_actors` | Implemented |
| `optimize_draw_calls` | Implemented |
| `optimize_shaders` | Implemented |
| `run_benchmark` | Implemented |
| `set_frame_rate_limit` | Implemented |
| `set_resolution_scale` | Implemented |
| `set_scalability` | Implemented |
| `set_vsync` | Implemented |
| `show_fps` | Implemented |
| `show_stats` | Implemented |
| `start_profiling` | Implemented |
| `stop_profiling` | Implemented |

---

## Interaction

**File:** `McpAutomationBridge_InteractionHandlers.cpp`

**Lines:** 1,291

**Maps to TS Tool(s):** `manage_character`

**Actions (17):**

| Action | Status |
|--------|--------|
| `add_destruction_component` | Implemented |
| `add_interaction_events` | Implemented |
| `configure_chest_properties` | Implemented |
| `configure_door_properties` | Implemented |
| `configure_interaction_trace` | Implemented |
| `configure_interaction_widget` | Implemented |
| `configure_switch_properties` | Implemented |
| `configure_trigger_events` | Implemented |
| `create_chest_actor` | Implemented |
| `create_door_actor` | Implemented |
| `create_interactable_interface` | Implemented |
| `create_interaction_component` | Implemented |
| `create_lever_actor` | Implemented |
| `create_switch_actor` | Implemented |
| `create_trigger_actor` | Implemented |
| `get_interaction_info` | Implemented |
| `setup_destructible_mesh` | Implemented |

---

## NiagaraAdvanced

**File:** `McpAutomationBridge_NiagaraAdvancedHandlers.cpp`

**Lines:** 707

**Maps to TS Tool(s):** `manage_effect`

**Actions (17):**

| Action | Status |
|--------|--------|
| `add_chaos_integration` | Implemented |
| `add_data_interface` | Implemented |
| `add_niagara_module` | Implemented |
| `add_niagara_script` | Implemented |
| `batch_compile_niagara` | Implemented |
| `configure_gpu_simulation` | Implemented |
| `configure_niagara_determinism` | Implemented |
| `configure_niagara_lod` | Implemented |
| `connect_niagara_pins` | Implemented |
| `create_fluid_simulation` | Implemented |
| `create_niagara_data_interface` | Implemented |
| `create_niagara_module` | Implemented |
| `create_niagara_sim_cache` | Implemented |
| `export_niagara_system` | Implemented |
| `import_niagara_module` | Implemented |
| `remove_niagara_node` | Implemented |
| `setup_niagara_fluids` | Implemented |

---

## Water

**File:** `McpAutomationBridge_WaterHandlers.cpp`

**Lines:** 1,327

**Maps to TS Tool(s):** `build_environment`

**Actions (17):**

| Action | Status |
|--------|--------|
| `configure_ocean_waves` | Implemented |
| `configure_water_body` | Implemented |
| `configure_water_waves` | Implemented |
| `create_water_body_lake` | Implemented |
| `create_water_body_ocean` | Implemented |
| `create_water_body_river` | Implemented |
| `get_water_body_info` | Implemented |
| `get_water_depth_info` | Implemented |
| `get_water_surface_info` | Implemented |
| `get_wave_info` | Implemented |
| `list_water_bodies` | Implemented |
| `query_water_bodies` | Implemented |
| `set_ocean_extent` | Implemented |
| `set_river_depth` | Implemented |
| `set_river_transitions` | Implemented |
| `set_water_static_mesh` | Implemented |
| `set_water_zone` | Implemented |

---

## Blueprint

**File:** `McpAutomationBridge_BlueprintHandlers.cpp`

**Lines:** 6,398

**Maps to TS Tool(s):** `manage_asset`

**Actions (16):**

| Action | Status |
|--------|--------|
| `bool` | Implemented |
| `boolean` | Implemented |
| `byte` | Implemented |
| `class` | Implemented |
| `double` | Implemented |
| `float` | Implemented |
| `int` | Implemented |
| `int64` | Implemented |
| `integer` | Implemented |
| `name` | Implemented |
| `object` | Implemented |
| `rotator` | Implemented |
| `string` | Implemented |
| `text` | Implemented |
| `transform` | Implemented |
| `vector` | Implemented |

---

## Sessions

**File:** `McpAutomationBridge_SessionsHandlers.cpp`

**Lines:** 1,017

**Maps to TS Tool(s):** `manage_networking`

**Actions (16):**

| Action | Status |
|--------|--------|
| `add_local_player` | Implemented |
| `configure_lan_play` | Implemented |
| `configure_local_session_settings` | Implemented |
| `configure_push_to_talk` | Implemented |
| `configure_session_interface` | Implemented |
| `configure_split_screen` | Implemented |
| `configure_voice_settings` | Implemented |
| `enable_voice_chat` | Implemented |
| `get_sessions_info` | Implemented |
| `host_lan_server` | Implemented |
| `join_lan_server` | Implemented |
| `mute_player` | Implemented |
| `remove_local_player` | Implemented |
| `set_split_screen_type` | Implemented |
| `set_voice_attenuation` | Implemented |
| `set_voice_channel` | Implemented |

---

## Ui

**File:** `McpAutomationBridge_UiHandlers.cpp`

**Lines:** 868

**Maps to TS Tool(s):** `manage_ui`

**Actions (16):**

| Action | Status |
|--------|--------|
| `add_widget_child` | Implemented |
| `create_hud` | Implemented |
| `create_widget` | Implemented |
| `get_all_widgets` | Implemented |
| `get_widget_hierarchy` | Implemented |
| `play_in_editor` | Implemented |
| `remove_widget_from_viewport` | Implemented |
| `save_all` | Implemented |
| `screenshot` | Implemented |
| `set_input_mode` | Implemented |
| `set_widget_image` | Implemented |
| `set_widget_text` | Implemented |
| `set_widget_visibility` | Implemented |
| `show_mouse_cursor` | Implemented |
| `simulate_input` | Implemented |
| `stop_play` | Implemented |

---

## BlueprintGraph

**File:** `McpAutomationBridge_BlueprintGraphHandlers.cpp`

**Lines:** 1,147

**Maps to TS Tool(s):** `manage_asset`

**Actions (12):**

| Action | Status |
|--------|--------|
| `break_pin_links` | Implemented |
| `connect_pins` | Implemented |
| `create_node` | Implemented |
| `create_reroute_node` | Implemented |
| `delete_node` | Implemented |
| `get_graph_details` | Implemented |
| `get_node_details` | Implemented |
| `get_nodes` | Implemented |
| `get_pin_details` | Implemented |
| `list_node_types` | Implemented |
| `set_node_property` | Implemented |
| `set_pin_default_value` | Implemented |

---

## ControlRig

**File:** `McpAutomationBridge_ControlRigHandlers.cpp`

**Lines:** 688

**Maps to TS Tool(s):** `animation_physics`

**Actions (12):**

| Action | Status |
|--------|--------|
| `add_control` | Implemented |
| `add_ik_chain` | Implemented |
| `add_ik_goal` | Implemented |
| `apply_animation_modifier` | Implemented |
| `configure_motion_matching` | Implemented |
| `create_animation_modifier` | Implemented |
| `create_control_rig` | Implemented |
| `create_ik_retargeter` | Implemented |
| `create_ik_rig` | Implemented |
| `create_pose_search_database` | Implemented |
| `set_retarget_chain_mapping` | Implemented |
| `setup_ml_deformer` | Implemented |

---

## Navigation

**File:** `McpAutomationBridge_NavigationHandlers.cpp`

**Lines:** 1,162

**Maps to TS Tool(s):** `manage_ai`

**Actions (12):**

| Action | Status |
|--------|--------|
| `configure_nav_area_cost` | Implemented |
| `configure_nav_link` | Implemented |
| `configure_nav_mesh_settings` | Implemented |
| `configure_smart_link_behavior` | Implemented |
| `create_nav_link_proxy` | Implemented |
| `create_nav_modifier_component` | Implemented |
| `create_smart_link` | Implemented |
| `get_navigation_info` | Implemented |
| `rebuild_navigation` | Implemented |
| `set_nav_agent_properties` | Implemented |
| `set_nav_area_class` | Implemented |
| `set_nav_link_type` | Implemented |

---

## MotionDesign

**File:** `McpAutomationBridge_MotionDesignHandlers.cpp`

**Lines:** 305

**Maps to TS Tool(s):** `manage_motion_design`

**Actions (10):**

| Action | Status |
|--------|--------|
| `add_effector` | Implemented |
| `add_noise_effector` | Implemented |
| `animate_effector` | Implemented |
| `configure_cloner_pattern` | Implemented |
| `configure_step_effector` | Implemented |
| `create_cloner` | Implemented |
| `create_mograph_sequence` | Implemented |
| `create_radial_cloner` | Implemented |
| `create_spline_cloner` | Implemented |
| `export_mograph_to_sequence` | Implemented |

---

## Weather

**File:** `McpAutomationBridge_WeatherHandlers.cpp`

**Lines:** 800

**Maps to TS Tool(s):** `build_environment`

**Actions (7):**

| Action | Status |
|--------|--------|
| `configure_lightning` | Implemented |
| `configure_rain_particles` | Implemented |
| `configure_snow_particles` | Implemented |
| `configure_weather_preset` | Implemented |
| `configure_wind` | Implemented |
| `configure_wind_directional` | Implemented |
| `create_weather_system` | Implemented |

---

## BehaviorTree

**File:** `McpAutomationBridge_BehaviorTreeHandlers.cpp`

**Lines:** 506

**Maps to TS Tool(s):** `manage_ai`

**Actions (6):**

| Action | Status |
|--------|--------|
| `add_node` | Implemented |
| `break_connections` | Implemented |
| `connect_nodes` | Implemented |
| `create` | Implemented |
| `remove_node` | Implemented |
| `set_node_properties` | Implemented |

---

## MaterialGraph

**File:** `McpAutomationBridge_MaterialGraphHandlers.cpp`

**Lines:** 892

**Maps to TS Tool(s):** `manage_asset`

**Actions (6):**

| Action | Status |
|--------|--------|
| `add_node` | Implemented |
| `break_connections` | Implemented |
| `connect_nodes` | Implemented |
| `connect_pins` | Implemented |
| `get_node_details` | Implemented |
| `remove_node` | Implemented |

---

## Effect

**File:** `McpAutomationBridge_EffectHandlers.cpp`

**Lines:** 1,731

**Maps to TS Tool(s):** `manage_effect`

**Actions (5):**

| Action | Status |
|--------|--------|
| `create_niagara_system` | Implemented |
| `debug_shape` | Implemented |
| `niagara` | Implemented |
| `particle` | Implemented |
| `spawn_niagara` | Implemented |

---

## AssetQuery

**File:** `McpAutomationBridge_AssetQueryHandlers.cpp`

**Lines:** 300

**Maps to TS Tool(s):** `manage_asset`

**Actions (4):**

| Action | Status |
|--------|--------|
| `find_by_tag` | Implemented |
| `get_dependencies` | Implemented |
| `get_source_control_state` | Implemented |
| `search_assets` | Implemented |

---

## Input

**File:** `McpAutomationBridge_InputHandlers.cpp`

**Lines:** 246

**Maps to TS Tool(s):** `control_editor`

**Actions (4):**

| Action | Status |
|--------|--------|
| `add_mapping` | Implemented |
| `create_input_action` | Implemented |
| `create_input_mapping_context` | Implemented |
| `remove_mapping` | Implemented |

---

## NiagaraGraph

**File:** `McpAutomationBridge_NiagaraGraphHandlers.cpp`

**Lines:** 278

**Maps to TS Tool(s):** `manage_effect`

**Actions (4):**

| Action | Status |
|--------|--------|
| `add_module` | Implemented |
| `connect_pins` | Implemented |
| `remove_node` | Implemented |
| `set_parameter` | Implemented |

---

## Render

**File:** `McpAutomationBridge_RenderHandlers.cpp`

**Lines:** 190

**Maps to TS Tool(s):** `manage_performance`

**Actions (4):**

| Action | Status |
|--------|--------|
| `attach_render_target_to_volume` | Implemented |
| `create_render_target` | Implemented |
| `lumen_update_scene` | Implemented |
| `nanite_rebuild_mesh` | Implemented |

---

## WorldPartition

**File:** `McpAutomationBridge_WorldPartitionHandlers.cpp`

**Lines:** 347

**Maps to TS Tool(s):** `manage_level`

**Actions (4):**

| Action | Status |
|--------|--------|
| `cleanup_invalid_datalayers` | Implemented |
| `create_datalayer` | Implemented |
| `load_cells` | Implemented |
| `set_datalayer` | Implemented |

---

## BlueprintCreation

**File:** `McpAutomationBridge_BlueprintCreationHandlers.cpp`

**Lines:** 643

**Maps to TS Tool(s):** `manage_asset`

**Actions (3):**

| Action | Status |
|--------|--------|
| `actor` | Implemented |
| `character` | Implemented |
| `pawn` | Implemented |

---

## EditorFunction

**File:** `McpAutomationBridge_EditorFunctionHandlers.cpp`

**Lines:** 1,275

**Maps to TS Tool(s):** `manage_editor_utilities`

**Actions (3):**

| Action | Status |
|--------|--------|
| `high` | Implemented |
| `medium` | Implemented |
| `preview` | Implemented |

---

## Log

**File:** `McpAutomationBridge_LogHandlers.cpp`

**Lines:** 137

**Maps to TS Tool(s):** `control_editor`

**Actions (2):**

| Action | Status |
|--------|--------|
| `subscribe` | Implemented |
| `unsubscribe` | Implemented |

---

## Debug

**File:** `McpAutomationBridge_DebugHandlers.cpp`

**Lines:** 43

**Maps to TS Tool(s):** `control_editor`

**Actions (1):**

| Action | Status |
|--------|--------|
| `spawn_category` | Implemented |

---

## Insights

**File:** `McpAutomationBridge_InsightsHandlers.cpp`

**Lines:** 42

**Maps to TS Tool(s):** `control_editor`

**Actions (1):**

| Action | Status |
|--------|--------|
| `start_session` | Implemented |

---

## Test

**File:** `McpAutomationBridge_TestHandlers.cpp`

**Lines:** 39

**Maps to TS Tool(s):** `manage_build`

**Actions (1):**

| Action | Status |
|--------|--------|
| `run_tests` | Implemented |

---

## Alphabetical Action Index

| Action | Handler(s) |
|--------|------------|
| `Add` | MaterialAuthoring |
| `Append` | MaterialAuthoring |
| `Clamp` | MaterialAuthoring |
| `Divide` | MaterialAuthoring |
| `Frac` | MaterialAuthoring |
| `Lerp` | MaterialAuthoring |
| `Multiply` | MaterialAuthoring |
| `OneMinus` | MaterialAuthoring |
| `Power` | MaterialAuthoring |
| `Subtract` | MaterialAuthoring |
| `accept_tool_result` | UtilityPlugins |
| `activate_modeling_tool` | UtilityPlugins |
| `activate_ragdoll` | Animation |
| `activate_variant` | UtilityPlugins |
| `actor` | BlueprintCreation |
| `add` | GAS, MaterialAuthoring, MetaSound |
| `add_ability_system_component` | GAS |
| `add_ability_task` | GAS |
| `add_acceleration_module` | NiagaraAuthoring |
| `add_actor` | SequencerConsolidated |
| `add_actor_binding` | UtilityPlugins |
| `add_actor_data_node` | PCG |
| `add_actor_spawner` | PCG |
| `add_actor_state` | GameplayPrimitives |
| `add_actor_state_transition` | GameplayPrimitives |
| `add_actors` | SequencerConsolidated |
| `add_ai_perception_component` | AI |
| `add_aim_offset_sample` | AnimationAuthoring |
| `add_attribute` | GAS |
| `add_audio_spectrum_data_interface` | NiagaraAuthoring |
| `add_blackboard_key` | AI |
| `add_blend_node` | AnimationAuthoring |
| `add_blend_sample` | AnimationAuthoring |
| `add_bone` | Skeleton |
| `add_bone_track` | AnimationAuthoring |
| `add_bounds_filter` | PCG |
| `add_bounds_modifier` | PCG |
| `add_burn_in` | MovieRender |
| `add_cached_pose` | AnimationAuthoring |
| `add_camera` | SequencerConsolidated |
| `add_camera_cut` | SequencerConsolidated |
| `add_camera_cut_track` | SequencerConsolidated |
| `add_camera_offset_module` | NiagaraAuthoring |
| `add_chaos_integration` | NiagaraAdvanced |
| `add_cluster_node` | VirtualProduction |
| `add_collision_module` | NiagaraAuthoring |
| `add_collision_query_data_interface` | NiagaraAuthoring |
| `add_color_module` | NiagaraAuthoring |
| `add_colorspace_transform` | VirtualProduction |
| `add_component` | Control |
| `add_composite_node` | AI |
| `add_composure_layer` | VirtualProduction |
| `add_condition_listener` | GameplayPrimitives |
| `add_console_variable` | MovieRender |
| `add_construction_field` | PhysicsDestruction |
| `add_control` | ControlRig |
| `add_copy_points` | PCG |
| `add_crafting_component` | Inventory |
| `add_cue_node` | AudioAuthoring |
| `add_curve_row` | Data |
| `add_custom_expression` | MaterialAuthoring |
| `add_custom_movement_mode` | Character |
| `add_data_interface` | NiagaraAdvanced |
| `add_data_table_row` | Data |
| `add_decorator` | AI |
| `add_density_filter` | PCG |
| `add_destruction_component` | Interaction |
| `add_dialogue_node` | GameplaySystems |
| `add_directional_indicators` | Accessibility |
| `add_distance_filter` | PCG |
| `add_dmx_component` | VirtualProduction |
| `add_effect_cue` | GAS |
| `add_effect_execution_calculation` | GAS |
| `add_effect_modifier` | GAS |
| `add_effector` | MotionDesign |
| `add_emitter_to_system` | NiagaraAuthoring |
| `add_eqs_context` | AI |
| `add_eqs_generator` | AI |
| `add_eqs_test` | AI |
| `add_equipment_functions` | Inventory |
| `add_event_generator` | NiagaraAuthoring |
| `add_event_receiver` | NiagaraAuthoring |
| `add_field_noise` | PhysicsDestruction |
| `add_field_radial_falloff` | PhysicsDestruction |
| `add_field_radial_vector` | PhysicsDestruction |
| `add_field_strain` | PhysicsDestruction |
| `add_field_uniform_vector` | PhysicsDestruction |
| `add_fixture_function` | VirtualProduction |
| `add_fixture_mode` | VirtualProduction |
| `add_flipbook_keyframe` | UtilityPlugins |
| `add_foliage` | Environment |
| `add_foliage_instances` | Environment |
| `add_force_module` | NiagaraAuthoring |
| `add_fresnel` | MaterialAuthoring |
| `add_function_input` | MaterialAuthoring |
| `add_function_output` | MaterialAuthoring |
| `add_height_filter` | PCG |
| `add_icvfx_camera` | VirtualProduction |
| `add_if` | MaterialAuthoring |
| `add_ik_chain` | AnimationAuthoring, ControlRig |
| `add_ik_goal` | ControlRig |
| `add_initialize_particle_module` | NiagaraAuthoring |
| `add_input_pass` | VirtualProduction |
| `add_instance` | GameplaySystems |
| `add_interactable_component` | GameplayPrimitives |
| `add_interaction_events` | Interaction |
| `add_inventory_functions` | Inventory |
| `add_job` | MovieRender |
| `add_keyframe` | SequencerConsolidated |
| `add_kill_particles_module` | NiagaraAuthoring |
| `add_landscape_data_node` | PCG |
| `add_landscape_layer` | MaterialAuthoring |
| `add_layered_blend_per_bone` | AnimationAuthoring |
| `add_level_blueprint_node` | LevelStructure |
| `add_light_renderer_module` | NiagaraAuthoring |
| `add_livelink_controller` | LiveLink |
| `add_livelink_source` | LiveLink |
| `add_local_player` | Sessions |
| `add_loot_entry` | Inventory |
| `add_mapping` | Control, Input |
| `add_mass_spawner` | AI |
| `add_material_node` | AssetWorkflow, MaterialAuthoring |
| `add_material_parameter` | AssetWorkflow, MaterialAuthoring |
| `add_math_node` | MaterialAuthoring |
| `add_menu_entry` | UtilityPlugins |
| `add_merge_points` | PCG |
| `add_mesh_renderer_module` | NiagaraAuthoring |
| `add_mesh_sampler` | PCG |
| `add_messagebus_source` | LiveLink |
| `add_metasound_input` | AudioAuthoring |
| `add_metasound_node` | AudioAuthoring, MetaSound |
| `add_metasound_output` | AudioAuthoring |
| `add_midi_device_component` | VirtualProduction |
| `add_mix_modifier` | AudioAuthoring |
| `add_module` | NiagaraGraph |
| `add_montage_notify` | AnimationAuthoring |
| `add_montage_section` | AnimationAuthoring |
| `add_montage_slot` | AnimationAuthoring |
| `add_native_gameplay_tag` | Data |
| `add_network_prediction_data` | Networking |
| `add_niagara_module` | NiagaraAdvanced |
| `add_niagara_script` | NiagaraAdvanced |
| `add_node` | BehaviorTree, MaterialGraph |
| `add_noise` | MaterialAuthoring |
| `add_noise_effector` | MotionDesign |
| `add_notify` | AnimationAuthoring, Animation |
| `add_notify_old_unused` | Animation |
| `add_notify_state` | AnimationAuthoring |
| `add_output_pass` | VirtualProduction |
| `add_panner` | MaterialAuthoring |
| `add_particle_state_module` | NiagaraAuthoring |
| `add_pcg_node` | PCG |
| `add_persistent_field` | PhysicsDestruction |
| `add_physics_body` | Skeleton |
| `add_physics_constraint` | Skeleton |
| `add_pixel_depth` | MaterialAuthoring |
| `add_preset_to_client` | LiveLink |
| `add_procedural_camera_shake` | SequencerConsolidated |
| `add_project_to_surface` | PCG |
| `add_python_path` | UtilityPlugins |
| `add_reference_image` | XRPlugins |
| `add_reflection_vector` | MaterialAuthoring |
| `add_render_pass` | MovieRender |
| `add_reputation_threshold` | GameplayPrimitives |
| `add_ribbon_renderer_module` | NiagaraAuthoring |
| `add_rig_unit` | Animation |
| `add_rotator` | MaterialAuthoring |
| `add_scalar_parameter` | MaterialAuthoring |
| `add_schedule_entry` | GameplayPrimitives |
| `add_section` | SequencerConsolidated |
| `add_self_pruning` | PCG |
| `add_service` | AI |
| `add_shot` | SequencerConsolidated |
| `add_shot_track` | SequencerConsolidated |
| `add_simulation_stage` | NiagaraAuthoring |
| `add_size_module` | NiagaraAuthoring |
| `add_skeletal_mesh_data_interface` | NiagaraAuthoring |
| `add_slope_filter` | PCG |
| `add_slot_node` | AnimationAuthoring |
| `add_smart_object_component` | AI |
| `add_smart_object_slot` | AI |
| `add_source_effect` | AudioAuthoring |
| `add_spawn_burst_module` | NiagaraAuthoring |
| `add_spawn_per_unit_module` | NiagaraAuthoring |
| `add_spawn_rate_module` | NiagaraAuthoring |
| `add_spawnable_from_class` | SequencerConsolidated |
| `add_spline_data_interface` | NiagaraAuthoring |
| `add_spline_data_node` | PCG |
| `add_spline_point` | Spline |
| `add_spline_sampler` | PCG |
| `add_spline_spawner` | PCG |
| `add_sprite_renderer_module` | NiagaraAuthoring |
| `add_state` | AnimationAuthoring |
| `add_state_machine` | AnimationAuthoring |
| `add_state_tree_state` | AI |
| `add_state_tree_transition` | AI |
| `add_static_mesh_data_interface` | NiagaraAuthoring |
| `add_static_mesh_spawner` | PCG |
| `add_static_switch_parameter` | MaterialAuthoring |
| `add_string_entry` | GameplaySystems |
| `add_sublevel` | Level |
| `add_subsequence` | SequencerConsolidated |
| `add_surface_sampler` | PCG |
| `add_switch` | MaterialAuthoring |
| `add_sync_marker` | AnimationAuthoring |
| `add_tag` | Control |
| `add_tag_to_asset` | GAS |
| `add_tag_to_container` | Data |
| `add_task_node` | AI |
| `add_texture_coordinate` | MaterialAuthoring |
| `add_texture_data_node` | PCG |
| `add_texture_sample` | MaterialAuthoring |
| `add_time_event` | GameplayPrimitives |
| `add_timecode_source` | VirtualProduction |
| `add_to_collection` | EditorUtilities |
| `add_to_playlist` | Media |
| `add_toolbar_button` | UtilityPlugins |
| `add_track` | SequencerConsolidated |
| `add_trajectory_prediction` | Animation |
| `add_transform_pass` | VirtualProduction |
| `add_transform_points` | PCG |
| `add_transient_field` | PhysicsDestruction |
| `add_transition` | AnimationAuthoring |
| `add_usd_payload` | AssetPlugins |
| `add_usd_reference` | AssetPlugins |
| `add_user_parameter` | NiagaraAuthoring |
| `add_value_threshold` | GameplayPrimitives |
| `add_variant` | UtilityPlugins |
| `add_vector_parameter` | MaterialAuthoring |
| `add_vehicle_wheel` | PhysicsDestruction |
| `add_velocity_module` | NiagaraAuthoring |
| `add_vertex_normal` | MaterialAuthoring |
| `add_viewport` | VirtualProduction |
| `add_virtual_subject` | LiveLink |
| `add_volume_data_node` | PCG |
| `add_volume_sampler` | PCG |
| `add_voronoi` | MaterialAuthoring |
| `add_widget_child` | Control, Ui |
| `add_world_position` | MaterialAuthoring |
| `add_xr_action` | XRPlugins |
| `add_zone_enter_event` | GameplayPrimitives |
| `add_zone_exit_event` | GameplayPrimitives |
| `additive` | GAS |
| `adjust_curves` | Texture |
| `adjust_levels` | Texture |
| `adsr` | MetaSound |
| `ai_assistant_explain_feature` | AI |
| `ai_assistant_query` | AI |
| `ai_assistant_suggest_fix` | AI |
| `analyze_graph` | AssetWorkflow |
| `animate_effector` | MotionDesign |
| `append` | MaterialAuthoring |
| `appendvector` | MaterialAuthoring |
| `apply_accessibility_preset` | Accessibility |
| `apply_animation_modifier` | ControlRig |
| `apply_avatar_to_character` | CharacterAvatar |
| `apply_baseline_settings` | Performance |
| `apply_cache_to_collection` | PhysicsDestruction |
| `apply_cloth_to_skeletal_mesh` | PhysicsDestruction |
| `apply_damage_with_effects` | Combat |
| `apply_face_to_skeletal_mesh` | LiveLink |
| `apply_fmod_snapshot` | AudioMiddleware |
| `apply_force` | Control |
| `apply_force_to_actor` | Control |
| `apply_livelink_preset` | LiveLink |
| `apply_mesh_operation` | UtilityPlugins |
| `apply_mocap_to_character` | LiveLink |
| `apply_ocio_look` | VirtualProduction |
| `apply_pose_asset` | Animation |
| `apply_preset` | CharacterAvatar |
| `apply_status_effect` | Character |
| `apply_substance_to_material` | AssetPlugins |
| `array_linear` | Geometry |
| `array_radial` | Geometry |
| `assign_actor_to_data_layer` | LevelStructure |
| `assign_actor_to_hlod` | GameplaySystems |
| `assign_behavior_tree` | AI |
| `assign_blackboard` | AI |
| `assign_cloth_asset_to_mesh` | Skeleton |
| `assign_fixture_to_universe` | VirtualProduction |
| `assign_item_category` | Inventory |
| `assign_to_faction` | GameplayPrimitives |
| `attach` | Control |
| `attach_child_layer` | VirtualProduction |
| `attach_groom_to_skeletal_mesh` | CharacterAvatar |
| `attach_render_target_to_volume` | Render |
| `attach_to_socket` | GameplayPrimitives |
| `audio` | AssetWorkflow |
| `audio_clear_sound_mix_class_override` | Audio |
| `audio_create_ambient_sound` | Audio |
| `audio_create_component` | Audio |
| `audio_create_reverb_zone` | Audio |
| `audio_create_sound_class` | Audio |
| `audio_create_sound_cue` | Audio |
| `audio_create_sound_mix` | Audio |
| `audio_enable_audio_analysis` | Audio |
| `audio_fade_sound` | Audio |
| `audio_fade_sound_in` | Audio |
| `audio_fade_sound_out` | Audio |
| `audio_get_metasound_inputs` | Audio |
| `audio_list_metasound_assets` | Audio |
| `audio_play_sound_2d` | Audio |
| `audio_play_sound_at_location` | Audio |
| `audio_play_sound_attached` | Audio |
| `audio_pop_sound_mix` | Audio |
| `audio_push_sound_mix` | Audio |
| `audio_set_audio_occlusion` | Audio |
| `audio_set_doppler_effect` | Audio |
| `audio_set_sound_attenuation` | Audio |
| `audio_set_sound_mix_class_override` | Audio |
| `audio_spawn_sound_at_location` | Audio |
| `audio_trigger_metasound` | Audio |
| `audioinput` | MetaSound |
| `audiooutput` | MetaSound |
| `audit_assets` | Build |
| `auto_skin_weights` | Skeleton |
| `auto_uv` | Geometry |
| `bake_customizable_instance` | CharacterAvatar |
| `bake_hda_to_actors` | AssetPlugins |
| `bake_hda_to_blueprint` | AssetPlugins |
| `bake_lighting_preview` | Lighting |
| `bake_lightmap` | Environment, Level |
| `bandpass` | MetaSound |
| `bandpassfilter` | MetaSound |
| `batch_add_inventory_items` | Character |
| `batch_compile_niagara` | NiagaraAdvanced |
| `batch_convert_to_substrate` | MaterialAuthoring |
| `batch_execute` | Control |
| `batch_execute_pcg_with_gpu` | PCG |
| `batch_nanite_convert` | AssetWorkflow |
| `batch_paint_foliage` | Environment |
| `batch_render_sequences` | MovieRender |
| `batch_render_substances` | AssetPlugins |
| `batch_set_component_properties` | Control |
| `batch_substrate_migration` | Control |
| `batch_transform` | Control |
| `batch_transform_actors` | Control |
| `begin_transaction` | EditorUtilities |
| `bend` | Geometry |
| `bevel` | Geometry |
| `bind_actor` | SequencerConsolidated |
| `bind_cloth_to_skeletal_mesh` | Skeleton |
| `bind_controller` | VirtualProduction |
| `bind_flesh_to_skeleton` | PhysicsDestruction |
| `bind_midi_to_property` | VirtualProduction |
| `bind_osc_address` | VirtualProduction |
| `bind_osc_to_property` | VirtualProduction |
| `bind_parameter_to_source` | NiagaraAuthoring |
| `bind_render_target` | VirtualProduction |
| `bind_statetree` | AI |
| `bind_to_event` | EditorUtilities |
| `bind_to_texture` | Media |
| `bind_xr_action` | XRPlugins |
| `blend_biomes` | PCG |
| `blend_ragdoll_to_animation` | Animation |
| `blueprint` | AssetWorkflow |
| `blueprint_list_node_types` | AssetWorkflow |
| `blueprints` | AssetWorkflow |
| `blur` | Texture |
| `bool` | Blueprint, MaterialAuthoring |
| `boolean` | Blueprint |
| `boolean_intersection` | Geometry |
| `boolean_mesh_operation` | Geometry |
| `boolean_subtract` | Geometry |
| `boolean_trim` | Geometry |
| `boolean_union` | Geometry |
| `boolparam` | MaterialAuthoring |
| `bp_list_node_types` | AssetWorkflow |
| `bpf` | MetaSound |
| `break_connections` | BehaviorTree, MaterialGraph |
| `break_material_connections` | AssetWorkflow |
| `break_pin_links` | BlueprintGraph |
| `bridge` | Geometry |
| `broadcast_event` | EditorUtilities |
| `build_hlod` | GameplaySystems |
| `build_hlod_for_level` | LevelStructure |
| `build_lighting` | Level, Lighting |
| `build_lighting_quality` | Lighting, PostProcess |
| `build_preset_from_client` | LiveLink |
| `byte` | Blueprint |
| `cache_avatar` | CharacterAvatar |
| `calibrate_varjo_eye_tracking` | XRPlugins |
| `cancel_interchange_import` | AssetPlugins |
| `cancel_job` | Control |
| `cancel_tool` | UtilityPlugins |
| `cancel_transaction` | EditorUtilities |
| `capture_property` | UtilityPlugins |
| `capture_scene` | Lighting, PostProcess |
| `capture_viewport` | Control |
| `capture_viewport_sequence` | Control |
| `chamfer` | Geometry |
| `channel_extract` | Texture |
| `channel_pack` | Texture |
| `chaos_create_cloth_config` | Animation |
| `chaos_create_cloth_shared_sim_config` | Animation |
| `chaos_get_plugin_status` | Animation |
| `character` | BlueprintCreation |
| `check_faction_relationship` | GameplayPrimitives |
| `check_has_authority` | Networking |
| `check_is_locally_controlled` | Networking |
| `check_map_errors` | Testing |
| `check_mod_compatibility` | Modding |
| `check_tag_match` | Data |
| `chorus` | MetaSound |
| `claim_smart_object` | AI |
| `clamp` | MaterialAuthoring, MetaSound |
| `class` | Blueprint |
| `cleanup` | Animation |
| `cleanup_invalid_datalayers` | WorldPartition |
| `clear_all_mesh_sections` | UtilityPlugins |
| `clear_all_timers` | EditorUtilities |
| `clear_avatar_cache` | CharacterAvatar |
| `clear_ddc` | Build |
| `clear_event_subscriptions` | Control |
| `clear_mesh_section` | UtilityPlugins |
| `clear_mesh_selection` | UtilityPlugins |
| `clear_python_output` | UtilityPlugins |
| `clear_queue` | MovieRender |
| `clear_sound_mix_class_override` | Audio |
| `clear_subject_frames` | LiveLink |
| `clear_timer` | EditorUtilities |
| `clone_component_hierarchy` | Control |
| `clone_components` | Control |
| `close` | Media |
| `close_midi_input` | VirtualProduction |
| `close_midi_output` | VirtualProduction |
| `close_usd_stage` | AssetPlugins |
| `color` | MaterialAuthoring |
| `colorparam` | MaterialAuthoring |
| `combine_textures` | Texture |
| `compile_blueprint_batch` | AssetWorkflow |
| `compile_customizable_object` | CharacterAvatar |
| `compile_material` | MaterialAuthoring |
| `compile_shaders` | Build |
| `compressor` | MetaSound |
| `configure_ace_emotions` | AINPC |
| `configure_aim_assist` | GameplaySystems |
| `configure_aim_down_sights` | Combat |
| `configure_aja_timecode` | VirtualProduction |
| `configure_alembic_import_settings` | AssetPlugins |
| `configure_analog_cursor` | UtilityPlugins |
| `configure_animation_settings` | AssetPlugins |
| `configure_anti_aliasing` | MovieRender |
| `configure_arcore_session` | XRPlugins |
| `configure_arkit_mapping` | LiveLink |
| `configure_arkit_session` | XRPlugins |
| `configure_asc` | GAS |
| `configure_asset_override_paths` | Modding |
| `configure_attachment_rules` | GameplayPrimitives |
| `configure_audio2face` | AINPC |
| `configure_audio_modulation` | AssetWorkflow, MetaSound |
| `configure_audio_track` | SequencerConsolidated |
| `configure_audio_visualization` | Accessibility |
| `configure_auto_aim_strength` | Accessibility |
| `configure_bink_buffer_mode` | AudioMiddleware |
| `configure_bink_draw_style` | AudioMiddleware |
| `configure_bink_sound_track` | AudioMiddleware |
| `configure_bink_texture` | AudioMiddleware |
| `configure_blackmagic_timecode` | VirtualProduction |
| `configure_blendshape_mapping` | AINPC |
| `configure_blendshape_remap` | LiveLink |
| `configure_block_parry` | Combat |
| `configure_bloom` | Lighting, PostProcess |
| `configure_bone_mapping` | LiveLink |
| `configure_brake_setup` | PhysicsDestruction |
| `configure_bt_node` | AI |
| `configure_build_settings` | Build |
| `configure_burn_in` | MovieRender |
| `configure_button_holds` | Accessibility |
| `configure_camera_component` | Character |
| `configure_camera_settings` | SequencerConsolidated |
| `configure_capsule_component` | Character |
| `configure_channel_responses` | EditorUtilities |
| `configure_chaperone_bounds` | XRPlugins |
| `configure_character_backstory` | AINPC |
| `configure_character_voice` | AINPC |
| `configure_checkpoint_data` | GameplaySystems |
| `configure_chest_properties` | Interaction |
| `configure_chroma_keyer` | VirtualProduction |
| `configure_chromatic_aberration` | Lighting, PostProcess |
| `configure_chunking` | Build |
| `configure_client_prediction` | Networking |
| `configure_cloner_pattern` | MotionDesign |
| `configure_color_grading` | Lighting, PostProcess |
| `configure_colorblind_mode` | Accessibility |
| `configure_combo_system` | Combat |
| `configure_constraint_limits` | Skeleton |
| `configure_control_remapping` | Accessibility |
| `configure_convai_actions` | AINPC |
| `configure_convai_lipsync` | AINPC |
| `configure_cue_trigger` | GAS |
| `configure_curve_mapping` | LiveLink |
| `configure_damage_execution` | Combat |
| `configure_damage_sense_config` | AI |
| `configure_datasmith_import_options` | AssetPlugins |
| `configure_datasmith_lightmap` | AssetPlugins |
| `configure_datasmith_materials` | AssetPlugins |
| `configure_ddc` | Build |
| `configure_destruction_damage` | Character |
| `configure_destruction_effects` | Character |
| `configure_destruction_levels` | Character |
| `configure_differential_setup` | PhysicsDestruction |
| `configure_difficulty_presets` | Accessibility |
| `configure_distance_attenuation` | AudioAuthoring |
| `configure_dmx_component` | VirtualProduction |
| `configure_dmx_port` | VirtualProduction |
| `configure_dof` | Lighting, PostProcess |
| `configure_door_properties` | Interaction |
| `configure_dormancy` | Networking |
| `configure_editor_preferences` | EditorUtilities |
| `configure_encryption` | Build |
| `configure_engine_setup` | PhysicsDestruction |
| `configure_equipment_effects` | Inventory |
| `configure_equipment_socket` | Character |
| `configure_equipment_visuals` | Inventory |
| `configure_event_channel` | Control |
| `configure_event_payload` | NiagaraAuthoring |
| `configure_exponential_height_fog` | Environment |
| `configure_exposed_classes` | Modding |
| `configure_exposure` | MaterialAuthoring |
| `configure_face_retargeting` | LiveLink |
| `configure_face_rig` | CharacterAvatar |
| `configure_face_source` | LiveLink |
| `configure_film_grain` | Lighting, PostProcess |
| `configure_fmod_attenuation` | AudioMiddleware |
| `configure_fmod_component` | AudioMiddleware |
| `configure_fmod_init` | AudioMiddleware |
| `configure_fmod_occlusion` | AudioMiddleware |
| `configure_foliage_density` | Environment |
| `configure_footstep_fx` | Character |
| `configure_footstep_system` | Character |
| `configure_foveated_rendering` | XRPlugins |
| `configure_frame_interpolation` | LiveLink |
| `configure_game_rules` | GameFramework |
| `configure_gamepad_navigation` | UtilityPlugins |
| `configure_gas_effect` | Combat |
| `configure_genlock` | VirtualProduction |
| `configure_genlock_source` | VirtualProduction |
| `configure_gi_settings` | Lighting |
| `configure_gltf_export_options` | AssetPlugins |
| `configure_gltf_material_baking` | AssetPlugins |
| `configure_gpu_simulation` | NiagaraAdvanced |
| `configure_grid_size` | LevelStructure |
| `configure_guardian_bounds` | XRPlugins |
| `configure_hair_physics` | CharacterAvatar |
| `configure_hair_rendering` | CharacterAvatar |
| `configure_hair_simulation` | CharacterAvatar |
| `configure_hda_input` | AssetPlugins |
| `configure_hearing_config` | AI |
| `configure_high_contrast_mode` | Accessibility |
| `configure_high_res_settings` | MovieRender |
| `configure_hit_reaction` | Combat |
| `configure_hitscan` | Combat |
| `configure_hlod_layer` | LevelStructure |
| `configure_hlod_settings` | GameplaySystems, LevelStructure |
| `configure_hold_vs_toggle` | Accessibility |
| `configure_hololens_settings` | XRPlugins |
| `configure_icvfx_camera` | VirtualProduction |
| `configure_impact_effects` | Combat |
| `configure_import_asset_type` | AssetPlugins |
| `configure_indirect_lighting_cache` | Lighting, PostProcess |
| `configure_inner_frustum` | VirtualProduction |
| `configure_interaction` | GameplayPrimitives |
| `configure_interaction_trace` | Interaction |
| `configure_interaction_widget` | Interaction |
| `configure_interchange_pipeline` | AssetPlugins |
| `configure_inventory_events` | Inventory |
| `configure_inventory_slot` | Character |
| `configure_inventory_slots` | Inventory |
| `configure_inworld_scene` | AINPC |
| `configure_inworld_settings` | AINPC |
| `configure_job` | MovieRender |
| `configure_jump` | Character |
| `configure_lan_play` | Sessions |
| `configure_landscape_material_layer` | MaterialAuthoring |
| `configure_large_world_coordinates` | LevelStructure |
| `configure_layer_blend` | MaterialAuthoring |
| `configure_layer_blend_mode` | Animation |
| `configure_led_wall_size` | VirtualProduction |
| `configure_lens_flares` | Lighting, PostProcess |
| `configure_level_bounds` | LevelStructure |
| `configure_level_streaming` | LevelStructure |
| `configure_light_cards` | VirtualProduction |
| `configure_lightmass_settings` | Lighting, PostProcess |
| `configure_lightning` | Weather |
| `configure_livelink_controller` | LiveLink |
| `configure_livelink_timecode` | LiveLink |
| `configure_local_session_settings` | Sessions |
| `configure_localization_entry` | GameplaySystems |
| `configure_lock_on_target` | GameplaySystems |
| `configure_locomotion_state` | Character |
| `configure_lod` | Performance |
| `configure_loot_drop` | Inventory |
| `configure_ltc_timecode` | VirtualProduction |
| `configure_lumen_gi` | Lighting |
| `configure_mantle_vault` | Character |
| `configure_marker_widget` | GameplaySystems |
| `configure_mass_ai_fragment` | AI |
| `configure_mass_entity` | AI |
| `configure_material_lod` | MaterialAuthoring |
| `configure_material_settings` | AssetPlugins |
| `configure_megalights` | Control |
| `configure_megalights_scene` | Lighting |
| `configure_megascan_import_settings` | AssetPlugins |
| `configure_melee_trace` | Combat |
| `configure_mesh_component` | Character |
| `configure_mesh_randomization` | Spline |
| `configure_mesh_spacing` | Spline |
| `configure_metahuman_lod` | CharacterAvatar |
| `configure_midi_learn` | VirtualProduction |
| `configure_minimap_icon` | GameplaySystems |
| `configure_mix_eq` | AudioAuthoring |
| `configure_ml_deformer_training` | Animation |
| `configure_mod_load_order` | Modding |
| `configure_mod_loading_paths` | Modding |
| `configure_mod_sandbox` | Modding |
| `configure_mono_audio` | Accessibility |
| `configure_motion_blur` | Lighting, PostProcess |
| `configure_motion_matching` | ControlRig |
| `configure_motion_sickness_options` | Accessibility |
| `configure_movement_prediction` | Networking |
| `configure_movement_speeds` | Character |
| `configure_mrq_settings` | MovieRender |
| `configure_muzzle_flash` | Combat |
| `configure_nanite` | Performance |
| `configure_nanite_settings` | AssetWorkflow, Geometry |
| `configure_nav_area_cost` | Navigation |
| `configure_nav_link` | Navigation |
| `configure_nav_mesh_settings` | Navigation |
| `configure_nav_movement` | Character |
| `configure_navigation_assistance` | Accessibility |
| `configure_navigation_rules` | UtilityPlugins |
| `configure_net_cull_distance` | Networking |
| `configure_net_driver` | Networking |
| `configure_net_priority` | Networking |
| `configure_net_relevancy` | Networking |
| `configure_net_serialization` | Networking |
| `configure_net_update_frequency` | Networking |
| `configure_network_settings` | VirtualProduction |
| `configure_niagara_determinism` | NiagaraAdvanced |
| `configure_niagara_lod` | NiagaraAdvanced |
| `configure_objective_markers` | GameplaySystems |
| `configure_objective_reminders` | Accessibility |
| `configure_occlusion` | AudioAuthoring, AudioMiddleware |
| `configure_occlusion_culling` | Performance |
| `configure_ocean_waves` | Water |
| `configure_one_handed_mode` | Accessibility |
| `configure_openxr_settings` | XRPlugins |
| `configure_osc_dispatcher` | VirtualProduction |
| `configure_outer_viewport` | VirtualProduction |
| `configure_output` | MovieRender |
| `configure_passthrough_style` | XRPlugins |
| `configure_path_tracing` | Lighting, PostProcess |
| `configure_pcg_mode_brush` | PCG |
| `configure_photo_mode_camera` | GameplaySystems |
| `configure_physics_body` | Skeleton |
| `configure_pickup_effects` | Inventory |
| `configure_pickup_interaction` | Inventory |
| `configure_pickup_respawn` | Inventory |
| `configure_platform` | Build |
| `configure_player_start` | GameFramework |
| `configure_portal` | AudioMiddleware |
| `configure_pp_blend` | Lighting, PostProcess |
| `configure_pp_priority` | Lighting, PostProcess |
| `configure_prediction_settings` | Networking |
| `configure_projectile` | Combat |
| `configure_projectile_collision` | Combat |
| `configure_projectile_homing` | Combat |
| `configure_projectile_movement` | Combat |
| `configure_push_model` | Networking |
| `configure_push_to_talk` | Sessions |
| `configure_python_paths` | UtilityPlugins |
| `configure_quest_settings` | XRPlugins |
| `configure_quick_time_events` | Accessibility |
| `configure_ragdoll_profile` | Animation |
| `configure_rain_particles` | Weather |
| `configure_ray_traced_ao` | Lighting, PostProcess |
| `configure_ray_traced_gi` | Lighting, PostProcess |
| `configure_ray_traced_reflections` | Lighting, PostProcess |
| `configure_ray_traced_shadows` | Lighting, PostProcess |
| `configure_recipe_requirements` | Inventory |
| `configure_recoil_pattern` | Combat |
| `configure_render_pass` | MovieRender |
| `configure_replicated_movement` | Networking |
| `configure_replication_graph` | Networking |
| `configure_reverb_send` | AudioAuthoring |
| `configure_room` | AudioMiddleware |
| `configure_rotation` | Character |
| `configure_round_system` | GameFramework |
| `configure_rpc_validation` | Networking |
| `configure_rpm_materials` | CharacterAvatar |
| `configure_runtime_loading` | LevelStructure |
| `configure_save_system` | GameplaySystems |
| `configure_scalability_group` | GameplaySystems |
| `configure_scoring_system` | GameFramework |
| `configure_screen_narrator` | Accessibility |
| `configure_screen_reader` | Accessibility |
| `configure_sculpt_brush` | UtilityPlugins |
| `configure_sequence_lod` | SequencerConsolidated |
| `configure_sequence_streaming` | SequencerConsolidated |
| `configure_server_correction` | Networking |
| `configure_session_interface` | Sessions |
| `configure_shadow_settings` | Lighting |
| `configure_shadows` | Lighting |
| `configure_shell_ejection` | Combat |
| `configure_sight_config` | AI |
| `configure_skeletal_mesh_settings` | AssetPlugins |
| `configure_skeleton_mapping` | LiveLink |
| `configure_sky_atmosphere` | Environment |
| `configure_slot_behavior` | AI |
| `configure_smart_link_behavior` | Navigation |
| `configure_smart_object` | AI |
| `configure_snow_particles` | Weather |
| `configure_socket` | Skeleton |
| `configure_source_settings` | LiveLink |
| `configure_spatial_audio` | AudioMiddleware |
| `configure_spatial_mapping_quality` | XRPlugins |
| `configure_spatialization` | AudioAuthoring |
| `configure_spawn_conditions` | GameplayPrimitives |
| `configure_spawn_system` | GameFramework |
| `configure_spawner` | GameplayPrimitives |
| `configure_speaker_identification` | Accessibility |
| `configure_spectating` | GameFramework |
| `configure_speedtree_collision` | AssetPlugins |
| `configure_speedtree_lod` | AssetPlugins |
| `configure_speedtree_wind` | AssetPlugins |
| `configure_spline_mesh_axis` | Spline |
| `configure_split_screen` | Sessions |
| `configure_spread_pattern` | Combat |
| `configure_sprite_collision` | UtilityPlugins |
| `configure_sprite_material` | UtilityPlugins |
| `configure_squash_stretch` | Animation |
| `configure_sss_profile` | MaterialAuthoring |
| `configure_state_timer` | GameplayPrimitives |
| `configure_state_tree_node` | AI |
| `configure_state_tree_task` | AI |
| `configure_static_mesh_settings` | AssetPlugins |
| `configure_steamvr_render` | XRPlugins |
| `configure_steamvr_settings` | XRPlugins |
| `configure_steering_setup` | PhysicsDestruction |
| `configure_step_effector` | MotionDesign |
| `configure_subject_settings` | LiveLink |
| `configure_substance_output_size` | AssetPlugins |
| `configure_subtitle_background` | Accessibility |
| `configure_subtitle_style` | Accessibility |
| `configure_subtitle_timing` | Accessibility |
| `configure_sun_atmosphere` | Environment |
| `configure_sun_color` | Environment |
| `configure_sun_position` | Environment |
| `configure_surface_type` | EditorUtilities |
| `configure_suspension_setup` | PhysicsDestruction |
| `configure_switch_properties` | Interaction |
| `configure_system_time_timecode` | VirtualProduction |
| `configure_targeting_priority` | GameplaySystems |
| `configure_team_settings` | Networking |
| `configure_team_system` | GameFramework |
| `configure_test_scoring` | AI |
| `configure_text_to_speech` | Accessibility |
| `configure_texture_streaming` | Performance |
| `configure_time_sync` | LiveLink |
| `configure_tracer` | Combat |
| `configure_transmission_setup` | PhysicsDestruction |
| `configure_trigger_events` | Interaction |
| `configure_trigger_filter` | Character |
| `configure_trigger_response` | Character |
| `configure_tutorial_options` | Accessibility |
| `configure_ui_input_config` | UtilityPlugins |
| `configure_ui_simplification` | Accessibility |
| `configure_usd_asset_cache` | AssetPlugins |
| `configure_value_decay` | GameplayPrimitives |
| `configure_value_regen` | GameplayPrimitives |
| `configure_variant_dependency` | UtilityPlugins |
| `configure_varjo_chroma_key` | XRPlugins |
| `configure_varjo_depth_test` | XRPlugins |
| `configure_varjo_markers` | XRPlugins |
| `configure_varjo_settings` | XRPlugins |
| `configure_vehicle` | Animation |
| `configure_viewport_ocio` | VirtualProduction |
| `configure_viewport_region` | VirtualProduction |
| `configure_vignette` | Lighting, PostProcess |
| `configure_virtual_texture` | Texture |
| `configure_visual_sound_cues` | Accessibility |
| `configure_voice_settings` | Sessions |
| `configure_volumetric_cloud` | Environment |
| `configure_volumetric_fog` | Lighting |
| `configure_volumetric_lightmap` | Lighting, PostProcess |
| `configure_warp_blend` | VirtualProduction |
| `configure_water_body` | Water |
| `configure_water_waves` | Water |
| `configure_weapon_mesh` | Combat |
| `configure_weapon_sockets` | Combat |
| `configure_weapon_trace` | Combat |
| `configure_weapon_trails` | Combat |
| `configure_weather_preset` | Weather |
| `configure_white_balance` | Lighting, PostProcess |
| `configure_wind` | Weather |
| `configure_wind_directional` | Weather |
| `configure_world_partition` | LevelStructure, Performance |
| `configure_world_settings` | LevelStructure |
| `configure_world_tracking` | XRPlugins |
| `configure_wwise_component` | AudioMiddleware |
| `configure_wwise_init` | AudioMiddleware |
| `configure_wwise_occlusion` | Audio |
| `configure_xr_spectator` | XRPlugins |
| `connect_cue_nodes` | AudioAuthoring |
| `connect_fmod_project` | AudioMiddleware |
| `connect_level_blueprint_nodes` | LevelStructure |
| `connect_material_pins` | AssetWorkflow, MaterialAuthoring |
| `connect_metasound_nodes` | AudioAuthoring, MetaSound |
| `connect_niagara_pins` | NiagaraAdvanced |
| `connect_nodes` | BehaviorTree, MaterialAuthoring, MaterialGraph |
| `connect_pcg_pins` | PCG |
| `connect_pins` | BlueprintGraph, MaterialGraph, NiagaraGraph |
| `connect_rig_elements` | Animation |
| `connect_to_bridge` | AssetPlugins |
| `connect_to_houdini_session` | AssetPlugins |
| `connect_wwise_project` | AudioMiddleware |
| `console_command` | Control |
| `constant` | MaterialAuthoring |
| `constant2vector` | MaterialAuthoring |
| `constant3vector` | MaterialAuthoring |
| `constant4vector` | MaterialAuthoring |
| `convert_material_to_substrate` | MaterialAuthoring |
| `convert_procedural_to_static_mesh` | UtilityPlugins |
| `convert_to_nanite` | AssetWorkflow, Geometry |
| `convert_to_static_mesh` | Geometry |
| `convert_to_substrate` | Control |
| `cook_content` | Build |
| `cook_hda` | AssetPlugins |
| `copy_weights` | Skeleton |
| `create` | BehaviorTree, Sequence, SequencerConsolidated |
| `create_accessibility_preset` | Accessibility |
| `create_actor_state_machine` | GameplayPrimitives |
| `create_ai_controller` | AI |
| `create_aim_offset` | AnimationAuthoring |
| `create_ambient_sound` | Audio |
| `create_anchor_field` | PhysicsDestruction |
| `create_anim_blueprint` | AnimationAuthoring |
| `create_anim_layer` | Animation |
| `create_animation_asset` | Animation |
| `create_animation_bp` | Animation |
| `create_animation_modifier` | ControlRig |
| `create_animation_sequence` | AnimationAuthoring |
| `create_ao_from_mesh` | Texture |
| `create_arch` | Geometry |
| `create_arcore_anchor` | XRPlugins |
| `create_arkit_anchor` | XRPlugins |
| `create_artnet_port` | VirtualProduction |
| `create_attenuation_settings` | AudioAuthoring |
| `create_attribute_set` | GAS |
| `create_audio_component` | Audio |
| `create_audio_volume` | Volume |
| `create_behavior_tree` | AI |
| `create_bink_media_player` | AudioMiddleware |
| `create_bink_texture` | AudioMiddleware |
| `create_biome_rules` | PCG |
| `create_blackboard_asset` | AI |
| `create_blend_space` | Animation |
| `create_blend_space_1d` | AnimationAuthoring |
| `create_blend_space_2d` | AnimationAuthoring |
| `create_blend_tree` | Animation |
| `create_blocking_volume` | Volume |
| `create_blueprint_interface` | EditorUtilities |
| `create_blutility_action` | UtilityPlugins |
| `create_bookmark` | Control |
| `create_box` | Geometry |
| `create_box_reflection_capture` | Lighting, PostProcess |
| `create_cable_spline` | Spline, Volume |
| `create_camera_blocking_volume` | Volume |
| `create_camera_cut_track` | SequencerConsolidated |
| `create_capsule` | Geometry |
| `create_chaos_cloth_config` | Animation, PhysicsDestruction |
| `create_chaos_cloth_shared_sim_config` | Animation, PhysicsDestruction |
| `create_character_blueprint` | Character |
| `create_checkpoint_actor` | GameplaySystems |
| `create_chest_actor` | Interaction |
| `create_cine_camera_actor` | SequencerConsolidated |
| `create_cloner` | MotionDesign |
| `create_collection` | EditorUtilities |
| `create_collision_channel` | EditorUtilities |
| `create_collision_profile` | EditorUtilities |
| `create_colorblind_filter` | Accessibility |
| `create_combo_sequence` | Combat |
| `create_common_activatable_widget` | UtilityPlugins |
| `create_composure_element` | VirtualProduction |
| `create_compound_condition` | GameplayPrimitives |
| `create_condition` | GameplayPrimitives |
| `create_cone` | Geometry |
| `create_control_remapping_ui` | Accessibility |
| `create_control_rig` | ControlRig |
| `create_control_rig_physics` | Animation |
| `create_controller` | VirtualProduction |
| `create_convai_character` | AINPC |
| `create_crafting_recipe` | Inventory |
| `create_crafting_station` | Inventory |
| `create_cull_distance_volume` | Volume |
| `create_curve_table` | Data |
| `create_customizable_instance` | CharacterAvatar |
| `create_customizable_object` | CharacterAvatar |
| `create_cylinder` | Geometry |
| `create_damage_type` | Combat |
| `create_data_asset` | Data |
| `create_data_layer` | LevelStructure |
| `create_data_table` | Data |
| `create_datalayer` | WorldPartition |
| `create_datasmith_runtime_actor` | AssetPlugins |
| `create_decal_material` | MaterialAuthoring |
| `create_device_profile` | GameplaySystems |
| `create_dialogue_node` | GameplaySystems |
| `create_dialogue_tree` | GameplaySystems |
| `create_dialogue_voice` | AudioAuthoring |
| `create_dialogue_wave` | AudioAuthoring |
| `create_disc` | Geometry |
| `create_dmx_library` | VirtualProduction |
| `create_dmx_sequencer_track` | VirtualProduction |
| `create_door_actor` | Interaction |
| `create_dynamic_light` | Lighting |
| `create_editor_utility_blueprint` | UtilityPlugins |
| `create_editor_utility_widget` | UtilityPlugins |
| `create_envelope` | AssetWorkflow, MetaSound |
| `create_eqs_query` | AI |
| `create_equipment_component` | Inventory |
| `create_event_dispatcher` | EditorUtilities |
| `create_event_trigger_track` | SequencerConsolidated |
| `create_exponential_height_fog` | Environment |
| `create_faction` | GameplayPrimitives |
| `create_fence_spline` | Spline, Volume |
| `create_field_system_actor` | PhysicsDestruction |
| `create_file_media_source` | Media |
| `create_filter` | AssetWorkflow, MetaSound |
| `create_fixture_patch` | VirtualProduction |
| `create_fixture_type` | VirtualProduction |
| `create_flesh_asset` | PhysicsDestruction |
| `create_flesh_cache` | PhysicsDestruction |
| `create_flesh_component` | PhysicsDestruction |
| `create_flipbook` | UtilityPlugins |
| `create_fluid_simulation` | NiagaraAdvanced |
| `create_fmod_component` | AudioMiddleware |
| `create_fog_volume` | Environment |
| `create_folder` | AssetWorkflow |
| `create_game_instance` | GameFramework |
| `create_game_instance_subsystem` | EditorUtilities |
| `create_game_mode` | GameFramework |
| `create_game_state` | GameFramework |
| `create_gameplay_ability` | GAS |
| `create_gameplay_cue_notify` | GAS |
| `create_gameplay_effect` | GAS |
| `create_gameplay_tag` | Data |
| `create_geometry_cache_track` | AssetPlugins |
| `create_geometry_collection` | PhysicsDestruction |
| `create_geometry_collection_cache` | PhysicsDestruction |
| `create_geospatial_anchor` | XRPlugins |
| `create_gradient_texture` | Texture |
| `create_groom_asset` | CharacterAvatar |
| `create_groom_binding` | CharacterAvatar |
| `create_hierarchical_instanced_static_mesh` | GameplaySystems |
| `create_hit_pause` | Combat |
| `create_hlod_layer` | GameplaySystems |
| `create_hud` | Ui |
| `create_hud_class` | GameFramework |
| `create_ik_retargeter` | AnimationAuthoring, ControlRig |
| `create_ik_rig` | AnimationAuthoring, ControlRig |
| `create_input_action` | Control, Input |
| `create_input_mapping_context` | Control, Input |
| `create_instanced_static_mesh_component` | GameplaySystems |
| `create_interactable_interface` | Interaction |
| `create_interaction_component` | Interaction |
| `create_interchange_pipeline` | AssetPlugins |
| `create_interchange_source_data` | AssetPlugins |
| `create_inventory_component` | Inventory |
| `create_inworld_character` | AINPC |
| `create_item_category` | Inventory |
| `create_item_data_asset` | Inventory |
| `create_kill_z_volume` | Volume |
| `create_landscape_grass_type` | Environment |
| `create_landscape_material` | MaterialAuthoring |
| `create_landscape_spline` | Environment |
| `create_layout_group` | VirtualProduction |
| `create_led_wall` | VirtualProduction |
| `create_level` | Level, LevelStructure |
| `create_level_instance` | LevelStructure |
| `create_level_variant_sets` | UtilityPlugins |
| `create_lever_actor` | Interaction |
| `create_light` | Level, Lighting |
| `create_light_batch` | Lighting |
| `create_lighting_enabled_level` | Lighting |
| `create_lightmass_importance_volume` | Volume |
| `create_lightmass_volume` | Lighting |
| `create_livelink_preset` | LiveLink |
| `create_local_player_subsystem` | EditorUtilities |
| `create_loot_table` | Inventory |
| `create_lumen_volume` | Lighting |
| `create_mass_entity_config` | AI |
| `create_master_sequence` | SequencerConsolidated |
| `create_material` | AssetWorkflow, MaterialAuthoring |
| `create_material_expression_template` | MaterialAuthoring |
| `create_material_from_substance` | AssetPlugins |
| `create_material_function` | MaterialAuthoring |
| `create_material_instance` | AssetWorkflow, MaterialAuthoring |
| `create_material_instance_batch` | MaterialAuthoring |
| `create_media_player` | Media |
| `create_media_playlist` | Media |
| `create_media_sound_wave` | Media |
| `create_media_texture` | Media |
| `create_media_track` | SequencerConsolidated |
| `create_melee_trace` | Combat |
| `create_mesh_from_spline` | Geometry |
| `create_mesh_section` | UtilityPlugins |
| `create_metasound` | AudioAuthoring, MetaSound |
| `create_minimap_volume` | LevelStructure |
| `create_mod_template_project` | Modding |
| `create_mograph_sequence` | MotionDesign |
| `create_montage` | AnimationAuthoring |
| `create_morph_target` | Skeleton |
| `create_nav_link_proxy` | Navigation |
| `create_nav_mesh_bounds_volume` | Volume |
| `create_nav_modifier_component` | Navigation |
| `create_nav_modifier_volume` | Volume |
| `create_ndisplay_config` | VirtualProduction |
| `create_new_level` | Level |
| `create_niagara_data_interface` | NiagaraAdvanced |
| `create_niagara_emitter` | NiagaraAuthoring |
| `create_niagara_module` | NiagaraAdvanced |
| `create_niagara_sim_cache` | NiagaraAdvanced |
| `create_niagara_system` | Effect, NiagaraAuthoring |
| `create_node` | BlueprintGraph |
| `create_noise_texture` | Texture |
| `create_normal_from_height` | Texture |
| `create_objective` | GameplaySystems |
| `create_objective_chain` | GameplaySystems |
| `create_ocio_config` | VirtualProduction |
| `create_osc_client` | VirtualProduction |
| `create_osc_server` | VirtualProduction |
| `create_oscillator` | AssetWorkflow, MetaSound |
| `create_packed_level_actor` | LevelStructure |
| `create_pain_causing_volume` | Volume |
| `create_pak_file` | Build |
| `create_pattern_texture` | Texture |
| `create_pcg_graph` | PCG |
| `create_pcg_hlsl_node` | PCG |
| `create_pcg_subgraph` | PCG |
| `create_physical_material` | EditorUtilities |
| `create_physics_asset` | Skeleton |
| `create_physics_volume` | Volume |
| `create_pickup_actor` | Inventory |
| `create_ping_system` | GameplaySystems |
| `create_pipe` | Geometry |
| `create_pipe_spline` | Spline, Volume |
| `create_planar_reflection` | Lighting, PostProcess |
| `create_plane` | Geometry |
| `create_player_controller` | GameFramework |
| `create_player_state` | GameFramework |
| `create_pose_library` | Animation |
| `create_pose_search_database` | ControlRig |
| `create_post_process_material` | MaterialAuthoring |
| `create_post_process_volume` | Lighting, PostProcess |
| `create_precomputed_visibility_volume` | Volume |
| `create_primary_data_asset` | Data |
| `create_procedural_anim` | Animation |
| `create_procedural_box` | Geometry |
| `create_procedural_foliage` | Environment |
| `create_procedural_mesh_component` | UtilityPlugins |
| `create_procedural_music` | AssetWorkflow, MetaSound |
| `create_procedural_terrain` | Environment |
| `create_projectile_blueprint` | Combat |
| `create_projectile_pool` | Combat |
| `create_python_editor_utility` | UtilityPlugins |
| `create_quest_data_asset` | GameplaySystems |
| `create_quest_stage` | GameplaySystems |
| `create_queue` | MovieRender |
| `create_radial_cloner` | MotionDesign |
| `create_ramp` | Geometry |
| `create_remote_control_preset` | VirtualProduction |
| `create_render_target` | AssetWorkflow, Render |
| `create_reroute_node` | BlueprintGraph |
| `create_retarget_asset` | LiveLink |
| `create_reverb_effect` | AudioAuthoring |
| `create_reverb_volume` | Volume |
| `create_reverb_zone` | Audio |
| `create_rigging_layer` | Animation |
| `create_ring` | Geometry |
| `create_river_spline` | Spline, Volume |
| `create_road_spline` | Spline, Volume |
| `create_rpc_function` | Networking |
| `create_rpm_actor` | CharacterAvatar |
| `create_rpm_animation_blueprint` | CharacterAvatar |
| `create_sacn_port` | VirtualProduction |
| `create_save_game_blueprint` | Data |
| `create_scene_capture_2d` | Lighting, PostProcess |
| `create_scene_capture_cube` | Lighting, PostProcess |
| `create_schedule` | GameplayPrimitives |
| `create_sequencer_node` | AssetWorkflow, MetaSound |
| `create_skeleton` | Skeleton |
| `create_sky_atmosphere` | Environment |
| `create_sky_light` | Lighting |
| `create_sky_sphere` | Environment |
| `create_smart_link` | Navigation |
| `create_smart_object` | AI |
| `create_smart_object_definition` | AI |
| `create_snapshot` | Control |
| `create_socket` | Skeleton |
| `create_sound_class` | AudioAuthoring, Audio |
| `create_sound_cue` | AudioAuthoring, Audio |
| `create_sound_indicator_widget` | Accessibility |
| `create_sound_mix` | AudioAuthoring, Audio |
| `create_source_effect_chain` | AudioAuthoring |
| `create_spatial_anchor` | XRPlugins |
| `create_spawner` | GameplayPrimitives |
| `create_speedtree_material` | AssetPlugins |
| `create_sphere` | Geometry |
| `create_sphere_reflection_capture` | Lighting, PostProcess |
| `create_spiral_stairs` | Geometry |
| `create_spline_actor` | Spline |
| `create_spline_cloner` | MotionDesign |
| `create_spline_mesh_component` | Spline |
| `create_sprite` | UtilityPlugins |
| `create_stairs` | Geometry |
| `create_state_machine` | Animation |
| `create_state_tree` | AI |
| `create_steamvr_overlay` | XRPlugins |
| `create_stream_media_source` | Media |
| `create_streaming_volume` | LevelStructure |
| `create_string_table` | GameplaySystems |
| `create_sublevel` | LevelStructure |
| `create_submix_effect` | AudioAuthoring |
| `create_substance_instance` | AssetPlugins |
| `create_substrate_material` | MaterialAuthoring |
| `create_subtitle_widget` | Accessibility |
| `create_switch_actor` | Interaction |
| `create_tag_container` | Data |
| `create_targeting_component` | GameplaySystems |
| `create_thumbnail` | AssetWorkflow |
| `create_tile_map` | UtilityPlugins |
| `create_tile_set` | UtilityPlugins |
| `create_time_of_day_controller` | Environment |
| `create_timecode_provider` | VirtualProduction |
| `create_timecode_synchronizer` | VirtualProduction |
| `create_torus` | Geometry |
| `create_trigger_actor` | Interaction |
| `create_trigger_box` | Volume |
| `create_trigger_capsule` | Volume |
| `create_trigger_sphere` | Volume |
| `create_trigger_volume` | Volume |
| `create_usd_layer` | AssetPlugins |
| `create_usd_prim` | AssetPlugins |
| `create_usd_stage` | AssetPlugins |
| `create_value_tracker` | GameplayPrimitives |
| `create_variant_set` | UtilityPlugins |
| `create_vehicle_animation_instance` | PhysicsDestruction |
| `create_virtual_bone` | Skeleton |
| `create_volumetric_cloud` | Environment |
| `create_wall_spline` | Spline, Volume |
| `create_water_body_lake` | Water |
| `create_water_body_ocean` | Water |
| `create_water_body_river` | Water |
| `create_weapon_blueprint` | Combat |
| `create_weather_system` | Weather |
| `create_wheeled_vehicle_bp` | PhysicsDestruction |
| `create_widget` | Control, Ui |
| `create_world_anchor` | XRPlugins |
| `create_world_marker` | GameplaySystems |
| `create_world_partition_cell` | LevelStructure |
| `create_world_subsystem` | EditorUtilities |
| `create_world_time` | GameplayPrimitives |
| `create_wwise_component` | AudioMiddleware |
| `create_wwise_trigger` | AudioMiddleware |
| `create_xr_action_set` | XRPlugins |
| `create_zone` | GameplayPrimitives |
| `custom` | MaterialAuthoring |
| `customexpression` | MaterialAuthoring |
| `cylindrify` | Geometry |
| `deactivate_modeling_tool` | UtilityPlugins |
| `deactivate_variant` | UtilityPlugins |
| `debug_behavior_tree` | AI |
| `debug_pcg_execution` | PCG |
| `debug_replication_graph` | Networking |
| `debug_shape` | Effect |
| `decay` | MetaSound |
| `define_equipment_slots` | Inventory |
| `delay` | MetaSound |
| `delete` | AssetWorkflow, Control, Environment, Level, SequencerConsolidated |
| `delete_asset` | AssetWorkflow |
| `delete_assets` | AssetWorkflow |
| `delete_by_tag` | Control |
| `delete_composure_element` | VirtualProduction |
| `delete_media_asset` | Media |
| `delete_node` | BlueprintGraph |
| `delete_object` | Control |
| `delete_save_slot` | Data |
| `delete_sequence` | SequencerConsolidated |
| `delete_spatial_anchor` | XRPlugins |
| `delete_variant_set` | UtilityPlugins |
| `depth` | MaterialAuthoring |
| `desaturate` | Texture |
| `deselect_all` | EditorUtilities |
| `deserialize_actor_state` | Control |
| `despawn_managed_actors` | GameplayPrimitives |
| `destroy_mass_entity` | AI |
| `destroy_overlay` | XRPlugins |
| `detach` | Control |
| `detach_child_layer` | VirtualProduction |
| `detach_from_parent` | GameplayPrimitives |
| `detaillighting` | Control |
| `disable_controller_evaluation` | LiveLink |
| `disable_hand_tracking` | XRPlugins |
| `disable_mod` | Modding |
| `disable_passthrough` | XRPlugins |
| `disable_people_occlusion` | XRPlugins |
| `disable_plugin` | Build |
| `disable_spatial_mapping` | XRPlugins |
| `disable_subject` | LiveLink |
| `disable_timecode_genlock` | VirtualProduction |
| `disable_varjo_passthrough` | XRPlugins |
| `disable_visual_logger` | Testing |
| `disconnect_bridge` | AssetPlugins |
| `disconnect_nodes` | MaterialAuthoring |
| `discover_messagebus_sources` | LiveLink |
| `div` | MaterialAuthoring |
| `divide` | GAS, MaterialAuthoring |
| `division` | GAS |
| `does_save_exist` | Data |
| `double` | Blueprint |
| `download_fab_asset` | AssetPlugins |
| `draw_bink_to_texture` | AudioMiddleware |
| `duplicate` | AssetWorkflow, Control, SequencerConsolidated |
| `duplicate_along_spline` | Geometry |
| `duplicate_sequence` | SequencerConsolidated |
| `duplicate_variant` | UtilityPlugins |
| `edge_split` | Geometry |
| `eject` | Control |
| `empty_data_table` | Data |
| `enable_arcore_augmented_images` | XRPlugins |
| `enable_arkit_face_tracking` | XRPlugins |
| `enable_audio_analysis` | Audio |
| `enable_body_correctives` | CharacterAvatar |
| `enable_body_tracking` | XRPlugins |
| `enable_clustering` | PhysicsDestruction |
| `enable_controller_evaluation` | LiveLink |
| `enable_depth_api` | XRPlugins |
| `enable_eye_tracking` | XRPlugins |
| `enable_foveated_rendering` | XRPlugins |
| `enable_geospatial` | XRPlugins |
| `enable_gpu_simulation` | NiagaraAuthoring |
| `enable_gpu_timing` | Performance |
| `enable_hair_simulation` | CharacterAvatar |
| `enable_hand_tracking` | XRPlugins |
| `enable_hololens_eye_tracking` | XRPlugins |
| `enable_hololens_hand_tracking` | XRPlugins |
| `enable_mod` | Modding |
| `enable_nanite` | AssetWorkflow |
| `enable_nanite_mesh` | AssetWorkflow, Geometry |
| `enable_neck_correctives` | CharacterAvatar |
| `enable_passthrough` | XRPlugins |
| `enable_pcg_gpu_processing` | PCG |
| `enable_people_occlusion` | XRPlugins |
| `enable_photo_mode` | GameplaySystems |
| `enable_plugin` | Build |
| `enable_qr_tracking` | XRPlugins |
| `enable_quest_body_tracking` | XRPlugins |
| `enable_quest_eye_tracking` | XRPlugins |
| `enable_quest_face_tracking` | XRPlugins |
| `enable_quest_hand_tracking` | XRPlugins |
| `enable_scene_capture` | XRPlugins |
| `enable_scene_reconstruction` | XRPlugins |
| `enable_scene_understanding` | XRPlugins |
| `enable_spatial_mapping` | XRPlugins |
| `enable_steamvr_skeletal_input` | XRPlugins |
| `enable_subject` | LiveLink |
| `enable_timecode_genlock` | VirtualProduction |
| `enable_usd_live_edit` | AssetPlugins |
| `enable_varjo_depth_estimation` | XRPlugins |
| `enable_varjo_eye_tracking` | XRPlugins |
| `enable_varjo_mixed_reality` | XRPlugins |
| `enable_varjo_passthrough` | XRPlugins |
| `enable_visual_logger` | Testing |
| `enable_voice_chat` | Sessions |
| `enable_world_partition` | LevelStructure |
| `end_transaction` | EditorUtilities |
| `engine_quit` | Environment |
| `ensure_single_sky_light` | Lighting |
| `enter_modeling_mode` | UtilityPlugins |
| `envelope` | MetaSound |
| `evaluate_condition` | GameplayPrimitives |
| `execute_command` | Control |
| `execute_editor_command` | UtilityPlugins |
| `execute_interaction` | GameplayPrimitives |
| `execute_pcg_graph` | PCG |
| `execute_python_command` | UtilityPlugins |
| `execute_python_file` | UtilityPlugins |
| `execute_python_script` | UtilityPlugins |
| `execute_sculpt_stroke` | UtilityPlugins |
| `exists` | AssetWorkflow |
| `explain_action_parameters` | Control |
| `export` | Control |
| `export_accessibility_settings` | Accessibility |
| `export_actor_to_gltf` | AssetPlugins |
| `export_actor_to_usd` | AssetPlugins |
| `export_animation_to_gltf` | AssetPlugins |
| `export_animation_to_usd` | AssetPlugins |
| `export_curve_table_csv` | Data |
| `export_data_table_csv` | Data |
| `export_datasmith_scene` | AssetPlugins |
| `export_geometry_to_file` | Geometry |
| `export_level` | Level |
| `export_level_to_gltf` | AssetPlugins |
| `export_level_to_usd` | AssetPlugins |
| `export_localization` | GameplaySystems |
| `export_material_template` | MaterialAuthoring |
| `export_material_to_gltf` | AssetPlugins |
| `export_material_to_usd` | AssetPlugins |
| `export_metahuman_settings` | CharacterAvatar |
| `export_metasound_preset` | MetaSound |
| `export_moddable_headers` | Modding |
| `export_mograph_to_sequence` | MotionDesign |
| `export_niagara_system` | NiagaraAdvanced |
| `export_pcg_hlsl_template` | PCG |
| `export_pcg_to_static` | PCG |
| `export_sequence` | SequencerConsolidated |
| `export_skeletal_mesh_to_usd` | AssetPlugins |
| `export_snapshot` | Environment |
| `export_static_mesh_to_usd` | AssetPlugins |
| `export_substance_textures` | AssetPlugins |
| `export_to_alembic` | AssetPlugins |
| `export_to_glb` | AssetPlugins |
| `export_to_gltf` | AssetPlugins |
| `export_variant_configuration` | UtilityPlugins |
| `export_with_interchange` | AssetPlugins |
| `expose_function` | VirtualProduction |
| `expose_property` | VirtualProduction |
| `extrude` | Geometry |
| `extrude_along_spline` | Geometry |
| `fade_sound` | Audio |
| `fade_sound_in` | Audio |
| `fade_sound_out` | Audio |
| `fall` | Character |
| `falling` | Character |
| `fill_holes` | Geometry |
| `filter` | MetaSound |
| `find_by_class` | Control, Environment |
| `find_by_name` | Control |
| `find_by_tag` | AssetQuery, AssetWorkflow, Control |
| `fix_redirectors` | Testing |
| `fixup_redirectors` | AssetWorkflow |
| `flanger` | MetaSound |
| `flatten_fracture` | PhysicsDestruction |
| `flip_normals` | Geometry |
| `float` | Blueprint, MaterialAuthoring |
| `float2` | MaterialAuthoring |
| `float3` | MaterialAuthoring |
| `float4` | MaterialAuthoring |
| `floatinput` | MetaSound |
| `floatparam` | MaterialAuthoring |
| `flush_config` | Data |
| `flush_operation_queue` | Control |
| `fly` | Character |
| `flying` | Character |
| `focus_actor` | Control |
| `focus_interaction` | GameplayPrimitives |
| `force_livelink_tick` | LiveLink |
| `frac` | MaterialAuthoring |
| `fraction` | MaterialAuthoring |
| `fracture_brick` | PhysicsDestruction |
| `fracture_clustered` | PhysicsDestruction |
| `fracture_radial` | PhysicsDestruction |
| `fracture_slice` | PhysicsDestruction |
| `fracture_uniform` | PhysicsDestruction |
| `fresnel` | MaterialAuthoring |
| `functioncall` | MaterialAuthoring |
| `gain` | MetaSound |
| `generate_collision` | Geometry |
| `generate_complex_collision` | Geometry |
| `generate_lods` | AssetWorkflow, Environment, Geometry |
| `generate_memory_report` | Performance |
| `generate_mesh_uvs` | Geometry |
| `generate_project_files` | Build |
| `generate_report` | AssetWorkflow |
| `generate_thumbnail` | AssetWorkflow |
| `get` | Control |
| `get_accessibility_info` | Accessibility |
| `get_ace_info` | AINPC |
| `get_action_statistics` | Control |
| `get_active_jobs` | Control |
| `get_active_timers` | EditorUtilities |
| `get_active_tool` | UtilityPlugins |
| `get_active_variant` | UtilityPlugins |
| `get_actor` | Control |
| `get_actor_bounds` | Control |
| `get_actor_by_name` | Control |
| `get_actor_references` | Control |
| `get_actor_state` | GameplayPrimitives |
| `get_actor_transform` | Control |
| `get_actor_zone` | GameplayPrimitives |
| `get_ai_info` | AI |
| `get_ai_npc_info` | AINPC |
| `get_ai_perception_data` | AI |
| `get_alembic_info` | AssetPlugins |
| `get_all_component_properties` | Control |
| `get_all_gameplay_tags` | Data |
| `get_all_widgets` | Ui |
| `get_animation_info` | AnimationAuthoring |
| `get_arcore_info` | XRPlugins |
| `get_arcore_light_estimate` | XRPlugins |
| `get_arcore_planes` | XRPlugins |
| `get_arcore_points` | XRPlugins |
| `get_arkit_face_blendshapes` | XRPlugins |
| `get_arkit_face_geometry` | XRPlugins |
| `get_arkit_info` | XRPlugins |
| `get_asset_graph` | AssetWorkflow |
| `get_asset_plugins_info` | AssetPlugins |
| `get_asset_references` | Build |
| `get_asset_size_info` | Build |
| `get_attached_actors` | GameplayPrimitives |
| `get_attachment_parent` | GameplayPrimitives |
| `get_audio2face_status` | AINPC |
| `get_audio_info` | AudioAuthoring |
| `get_audio_middleware_info` | AudioMiddleware |
| `get_available_actions` | Control |
| `get_available_cultures` | GameplaySystems |
| `get_avatar_metadata` | CharacterAvatar |
| `get_bindings` | SequencerConsolidated |
| `get_bink_dimensions` | AudioMiddleware |
| `get_bink_duration` | AudioMiddleware |
| `get_bink_status` | AudioMiddleware |
| `get_bink_time` | AudioMiddleware |
| `get_blueprint_dependencies` | AssetWorkflow |
| `get_body_skeleton` | XRPlugins |
| `get_bone_transforms` | Animation |
| `get_bounding_box` | Control, Environment |
| `get_bridge_health` | Control |
| `get_bridge_status` | AssetPlugins |
| `get_build_info` | Build |
| `get_camera_intrinsics` | XRPlugins |
| `get_chaos_plugin_status` | Animation, PhysicsDestruction |
| `get_chaperone_geometry` | XRPlugins |
| `get_character_emotion` | AINPC |
| `get_character_goals` | AINPC |
| `get_character_info` | Character |
| `get_character_response` | AINPC |
| `get_character_stats_snapshot` | Character |
| `get_class_hierarchy` | Control |
| `get_cloth_config` | PhysicsDestruction |
| `get_cloth_stats` | PhysicsDestruction |
| `get_collision_info` | EditorUtilities |
| `get_combat_info` | Combat |
| `get_combat_stats` | Combat |
| `get_common_ui_info` | UtilityPlugins |
| `get_component_property` | Control, Environment |
| `get_components` | Control, Environment |
| `get_composure_info` | VirtualProduction |
| `get_config_section` | Data |
| `get_control_rig_controls` | Animation |
| `get_controller_info` | LiveLink |
| `get_controller_pose` | XRPlugins |
| `get_convai_info` | AINPC |
| `get_current_schedule_entry` | GameplayPrimitives |
| `get_current_timecode` | VirtualProduction |
| `get_curve_value` | Data |
| `get_data_asset_info` | Data |
| `get_data_table_row` | Data |
| `get_data_table_rows` | Data |
| `get_datasmith_scene_info` | AssetPlugins |
| `get_ddc_stats` | Build |
| `get_dependencies` | AssetQuery, AssetWorkflow |
| `get_depth_image` | XRPlugins |
| `get_dmx_info` | VirtualProduction |
| `get_duration` | Media |
| `get_editor_scripting_info` | UtilityPlugins |
| `get_editor_utilities_info` | EditorUtilities |
| `get_engine_version` | Environment |
| `get_event_history` | Control |
| `get_exposed_properties` | VirtualProduction |
| `get_exposed_property_value` | VirtualProduction |
| `get_eye_tracking_data` | XRPlugins |
| `get_face_blendshapes` | LiveLink |
| `get_face_tracking_status` | LiveLink |
| `get_faction` | GameplayPrimitives |
| `get_feature_flags` | Environment |
| `get_fixture_channel_value` | VirtualProduction |
| `get_flesh_asset_info` | PhysicsDestruction |
| `get_fmod_event_info` | AudioMiddleware |
| `get_fmod_loaded_banks` | AudioMiddleware |
| `get_fmod_memory_usage` | AudioMiddleware |
| `get_fmod_parameter` | AudioMiddleware |
| `get_fmod_status` | AudioMiddleware |
| `get_foliage_instances` | Environment |
| `get_functional_test_results` | Testing |
| `get_game_framework_info` | GameFramework |
| `get_gameplay_systems_info` | GameplaySystems |
| `get_gas_info` | GAS |
| `get_geometry_collection_stats` | PhysicsDestruction |
| `get_geospatial_pose` | XRPlugins |
| `get_gltf_export_messages` | AssetPlugins |
| `get_graph_details` | BlueprintGraph |
| `get_groom_info` | CharacterAvatar |
| `get_guardian_geometry` | XRPlugins |
| `get_hand_tracking_data` | XRPlugins |
| `get_hda_cook_status` | AssetPlugins |
| `get_hda_outputs` | AssetPlugins |
| `get_hda_parameters` | AssetPlugins |
| `get_hmd_pose` | XRPlugins |
| `get_hololens_gaze_ray` | XRPlugins |
| `get_hololens_hand_mesh` | XRPlugins |
| `get_hololens_info` | XRPlugins |
| `get_instance_count` | GameplaySystems |
| `get_instance_info` | CharacterAvatar |
| `get_interaction_info` | Interaction |
| `get_interchange_import_result` | AssetPlugins |
| `get_interchange_translators` | AssetPlugins |
| `get_inventory_info` | Inventory |
| `get_inworld_info` | AINPC |
| `get_job_status` | Control |
| `get_keyframes` | SequencerConsolidated |
| `get_last_error_details` | Control |
| `get_level_structure_info` | LevelStructure |
| `get_light_budget_stats` | Control |
| `get_light_complexity` | Lighting |
| `get_light_estimation` | XRPlugins |
| `get_lighthouse_info` | XRPlugins |
| `get_livelink_info` | LiveLink |
| `get_livelink_timecode` | LiveLink |
| `get_loaded_banks` | AudioMiddleware |
| `get_material_dependencies` | MaterialAuthoring |
| `get_material_info` | MaterialAuthoring |
| `get_material_node_details` | AssetWorkflow |
| `get_material_stats` | AssetWorkflow, MaterialAuthoring |
| `get_media_info` | Media |
| `get_megalights_budget` | Lighting |
| `get_memory_report` | Testing |
| `get_mesh_info` | Geometry |
| `get_mesh_selection` | UtilityPlugins |
| `get_metadata` | AssetWorkflow, Control, SequencerConsolidated |
| `get_metahuman_component` | CharacterAvatar |
| `get_metahuman_info` | CharacterAvatar |
| `get_metasound_inputs` | Audio |
| `get_midi_info` | VirtualProduction |
| `get_mod_info` | Modding |
| `get_modding_info` | Modding |
| `get_modeling_tools_info` | UtilityPlugins |
| `get_motion_matching_state` | Animation |
| `get_navigation_info` | Navigation |
| `get_ndisplay_info` | VirtualProduction |
| `get_nearby_interactables` | GameplayPrimitives |
| `get_net_role_info` | Networking |
| `get_networking_info` | Networking |
| `get_niagara_info` | NiagaraAuthoring |
| `get_node_details` | BlueprintGraph, MaterialGraph |
| `get_nodes` | BlueprintGraph |
| `get_ocio_colorspaces` | VirtualProduction |
| `get_ocio_displays` | VirtualProduction |
| `get_ocio_info` | VirtualProduction |
| `get_openxr_info` | XRPlugins |
| `get_operation_history` | Control |
| `get_osc_info` | VirtualProduction |
| `get_paper2d_info` | UtilityPlugins |
| `get_parameter_info` | CharacterAvatar |
| `get_pcg_info` | PCG |
| `get_performance_stats` | Testing |
| `get_physical_material_info` | EditorUtilities |
| `get_physics_destruction_info` | PhysicsDestruction |
| `get_pin_details` | BlueprintGraph |
| `get_platform_settings` | Build |
| `get_playback_range` | SequencerConsolidated |
| `get_playlist` | Media |
| `get_plugin_info` | Build |
| `get_plugin_status` | UtilityPlugins |
| `get_post_process_settings` | Lighting, PostProcess |
| `get_preset_sources` | LiveLink |
| `get_preset_subjects` | LiveLink |
| `get_procedural_mesh_info` | UtilityPlugins |
| `get_project_settings` | Control, Environment |
| `get_properties` | SequencerConsolidated |
| `get_property` | Control, Environment |
| `get_python_info` | UtilityPlugins |
| `get_python_output` | UtilityPlugins |
| `get_python_paths` | UtilityPlugins |
| `get_python_version` | UtilityPlugins |
| `get_quest_body_state` | XRPlugins |
| `get_quest_eye_gaze` | XRPlugins |
| `get_quest_face_state` | XRPlugins |
| `get_quest_hand_pose` | XRPlugins |
| `get_quest_info` | XRPlugins |
| `get_queue` | MovieRender |
| `get_redirectors` | Testing |
| `get_registered_voice_commands` | XRPlugins |
| `get_remote_control_info` | VirtualProduction |
| `get_render_passes` | MovieRender |
| `get_render_progress` | MovieRender |
| `get_render_status` | MovieRender |
| `get_reputation` | GameplayPrimitives |
| `get_room_layout` | XRPlugins |
| `get_rpc_statistics` | Networking |
| `get_rpm_info` | CharacterAvatar |
| `get_rtpc_value` | AudioMiddleware |
| `get_save_slot_names` | Data |
| `get_scalability_settings` | GameplaySystems |
| `get_scene_anchors` | XRPlugins |
| `get_scene_mesh` | XRPlugins |
| `get_scene_objects` | XRPlugins |
| `get_sdk_config` | Modding |
| `get_security_config` | Modding |
| `get_selected_actors` | EditorUtilities |
| `get_selection_info` | Control |
| `get_sequence_bindings` | SequencerConsolidated |
| `get_sequence_info` | SequencerConsolidated |
| `get_session_players` | Networking |
| `get_sessions_info` | Sessions |
| `get_shots` | SequencerConsolidated |
| `get_skeletal_bone_data` | XRPlugins |
| `get_skeleton_info` | Skeleton |
| `get_skeleton_mapping_info` | LiveLink |
| `get_source_control_state` | AssetQuery, AssetWorkflow |
| `get_source_status` | LiveLink |
| `get_source_type` | LiveLink |
| `get_spatial_mesh` | XRPlugins |
| `get_spawned_count` | GameplayPrimitives |
| `get_speedtree_info` | AssetPlugins |
| `get_splines_info` | Spline, Volume |
| `get_sprite_info` | UtilityPlugins |
| `get_state` | Media |
| `get_statetree_state` | AI |
| `get_steamvr_action_manifest` | XRPlugins |
| `get_steamvr_info` | XRPlugins |
| `get_streaming_levels_status` | LevelStructure |
| `get_string_entry` | GameplaySystems |
| `get_subject_frame_data` | LiveLink |
| `get_subject_frame_times` | LiveLink |
| `get_subject_role` | LiveLink |
| `get_subject_state` | LiveLink |
| `get_subject_static_data` | LiveLink |
| `get_subjects_by_role` | LiveLink |
| `get_subscribed_events` | Control |
| `get_subsequences` | SequencerConsolidated |
| `get_substance_graph_info` | AssetPlugins |
| `get_substance_outputs` | AssetPlugins |
| `get_substance_parameters` | AssetPlugins |
| `get_subsystem_info` | EditorUtilities |
| `get_summary` | LevelStructure |
| `get_supported_extensions` | XRPlugins |
| `get_target_platforms` | Build |
| `get_test_info` | Testing |
| `get_test_results` | Testing |
| `get_texture_info` | Texture |
| `get_time` | Media |
| `get_time_period` | GameplayPrimitives |
| `get_timecode_info` | VirtualProduction |
| `get_timecode_provider_status` | VirtualProduction |
| `get_tool_properties` | UtilityPlugins |
| `get_trace_status` | Testing |
| `get_tracked_device_count` | XRPlugins |
| `get_tracked_device_info` | XRPlugins |
| `get_tracked_images` | XRPlugins |
| `get_tracked_planes` | XRPlugins |
| `get_tracked_qr_codes` | XRPlugins |
| `get_tracking_origin` | XRPlugins |
| `get_tracks` | SequencerConsolidated |
| `get_transaction_history` | EditorUtilities |
| `get_transform` | Control |
| `get_ui_input_config` | UtilityPlugins |
| `get_usd_prim` | AssetPlugins |
| `get_usd_prim_attribute` | AssetPlugins |
| `get_usd_prim_children` | AssetPlugins |
| `get_usd_stage_info` | AssetPlugins |
| `get_utility_plugins_info` | UtilityPlugins |
| `get_value` | GameplayPrimitives |
| `get_variant_manager_info` | UtilityPlugins |
| `get_varjo_camera_intrinsics` | XRPlugins |
| `get_varjo_environment_cubemap` | XRPlugins |
| `get_varjo_gaze_data` | XRPlugins |
| `get_varjo_info` | XRPlugins |
| `get_vehicle_config` | PhysicsDestruction |
| `get_view_configuration` | XRPlugins |
| `get_virtual_production_info` | VirtualProduction |
| `get_visual_logger_status` | Testing |
| `get_volumes_info` | Volume |
| `get_water_body_info` | Water |
| `get_water_depth_info` | Water |
| `get_water_surface_info` | Water |
| `get_wave_info` | Water |
| `get_web_server_status` | VirtualProduction |
| `get_widget_hierarchy` | Ui |
| `get_world_partition_cells` | LevelStructure |
| `get_world_time` | GameplayPrimitives |
| `get_wwise_event_duration` | AudioMiddleware |
| `get_wwise_status` | AudioMiddleware |
| `get_xr_action_state` | XRPlugins |
| `get_xr_runtime_name` | XRPlugins |
| `get_xr_system_info` | XRPlugins |
| `get_zone_property` | GameplayPrimitives |
| `grant_gas_ability` | Combat |
| `group_actors` | EditorUtilities |
| `has_tag` | Data |
| `hide` | LevelStructure |
| `hide_overlay` | XRPlugins |
| `high` | EditorFunction |
| `highpass` | MetaSound |
| `highpassfilter` | MetaSound |
| `hlsl` | MaterialAuthoring |
| `host_cloud_anchor` | XRPlugins |
| `host_lan_server` | Sessions |
| `hpf` | MetaSound |
| `if` | MaterialAuthoring |
| `import` | AssetWorkflow |
| `import_accessibility_settings` | Accessibility |
| `import_alembic_file` | AssetPlugins |
| `import_alembic_geometry_cache` | AssetPlugins |
| `import_alembic_groom` | AssetPlugins |
| `import_alembic_skeletal_mesh` | AssetPlugins |
| `import_alembic_static_mesh` | AssetPlugins |
| `import_audio_to_metasound` | MetaSound |
| `import_curve_table_csv` | Data |
| `import_data_table_csv` | Data |
| `import_datasmith_3dsmax` | AssetPlugins |
| `import_datasmith_archicad` | AssetPlugins |
| `import_datasmith_cad` | AssetPlugins |
| `import_datasmith_file` | AssetPlugins |
| `import_datasmith_revit` | AssetPlugins |
| `import_datasmith_rhino` | AssetPlugins |
| `import_datasmith_sketchup` | AssetPlugins |
| `import_datasmith_solidworks` | AssetPlugins |
| `import_fbx_with_interchange` | AssetPlugins |
| `import_gdtf` | VirtualProduction |
| `import_glb` | AssetPlugins |
| `import_gltf` | AssetPlugins |
| `import_gltf_skeletal_mesh` | AssetPlugins |
| `import_gltf_static_mesh` | AssetPlugins |
| `import_groom` | CharacterAvatar |
| `import_hda` | AssetPlugins |
| `import_level` | Level |
| `import_localization` | GameplaySystems |
| `import_megascan_3d_asset` | AssetPlugins |
| `import_megascan_3d_plant` | AssetPlugins |
| `import_megascan_atlas` | AssetPlugins |
| `import_megascan_brush` | AssetPlugins |
| `import_megascan_decal` | AssetPlugins |
| `import_megascan_surface` | AssetPlugins |
| `import_metahuman` | CharacterAvatar |
| `import_morph_targets` | Skeleton |
| `import_niagara_module` | NiagaraAdvanced |
| `import_obj_with_interchange` | AssetPlugins |
| `import_pcg_preset` | PCG |
| `import_sbsar_file` | AssetPlugins |
| `import_snapshot` | Environment |
| `import_speedtree_9` | AssetPlugins |
| `import_speedtree_atlas` | AssetPlugins |
| `import_speedtree_model` | AssetPlugins |
| `import_with_interchange` | AssetPlugins |
| `input` | MetaSound |
| `inset` | Geometry |
| `inspect_class` | Control, Environment |
| `inspect_object` | Control, Environment |
| `instantiate_hda` | AssetPlugins |
| `int` | Blueprint |
| `int64` | Blueprint |
| `integer` | Blueprint |
| `invert` | MaterialAuthoring, Texture |
| `is_python_available` | UtilityPlugins |
| `join_lan_server` | Sessions |
| `jump_to_bookmark` | Control |
| `lerp` | MaterialAuthoring |
| `lightcomplexity` | Control |
| `lightingonly` | Control |
| `lightmapdensity` | Control |
| `limiter` | MetaSound |
| `linearinterpolate` | MaterialAuthoring |
| `link_sections` | AnimationAuthoring |
| `list` | AssetWorkflow, Control, Level, SequencerConsolidated |
| `list_active_vp_sessions` | VirtualProduction |
| `list_actors` | Control |
| `list_asset_overrides` | Modding |
| `list_available_ai_backends` | AINPC |
| `list_available_presets` | CharacterAvatar |
| `list_available_roles` | LiveLink |
| `list_available_tools` | UtilityPlugins |
| `list_bones` | Skeleton |
| `list_chaos_vehicles` | PhysicsDestruction |
| `list_cluster_nodes` | VirtualProduction |
| `list_dmx_fixtures` | VirtualProduction |
| `list_dmx_universes` | VirtualProduction |
| `list_functional_tests` | Testing |
| `list_geometry_collections` | PhysicsDestruction |
| `list_installed_mods` | Modding |
| `list_instances` | AssetWorkflow |
| `list_levels` | Level |
| `list_light_types` | Lighting |
| `list_livelink_sources` | LiveLink |
| `list_livelink_subjects` | LiveLink |
| `list_metasound_assets` | Audio |
| `list_midi_devices` | VirtualProduction |
| `list_node_types` | BlueprintGraph |
| `list_objects` | Control |
| `list_osc_clients` | VirtualProduction |
| `list_osc_servers` | VirtualProduction |
| `list_physics_bodies` | Skeleton |
| `list_plugins` | Build |
| `list_pose_search_databases` | Animation |
| `list_sequences` | SequencerConsolidated |
| `list_sockets` | Skeleton |
| `list_source_factories` | LiveLink |
| `list_statetree_states` | AI |
| `list_tests` | Testing |
| `list_timecode_providers` | VirtualProduction |
| `list_track_types` | SequencerConsolidated |
| `list_tracks` | SequencerConsolidated |
| `list_utility_plugins` | UtilityPlugins |
| `list_water_bodies` | Water |
| `list_xr_devices` | XRPlugins |
| `lit` | Control |
| `load` | Level, LevelStructure |
| `load_avatar_from_glb` | CharacterAvatar |
| `load_avatar_from_url` | CharacterAvatar |
| `load_cells` | WorldPartition |
| `load_checkpoint` | GameplaySystems |
| `load_fmod_bank` | AudioMiddleware |
| `load_game_from_slot` | Data |
| `load_level` | Level |
| `load_livelink_preset` | LiveLink |
| `load_mod_pak` | Modding |
| `load_ocio_config` | VirtualProduction |
| `load_remote_control_preset` | VirtualProduction |
| `load_spatial_anchors` | XRPlugins |
| `load_world_anchors` | XRPlugins |
| `load_wwise_bank` | AudioMiddleware |
| `loft` | Geometry |
| `loop_cut` | Geometry |
| `lowpass` | MetaSound |
| `lowpassfilter` | MetaSound |
| `lpf` | MetaSound |
| `lumen_update_scene` | Control, Render |
| `manage_asset` | MetaSound |
| `manage_audio` | MetaSound |
| `manage_level` | Level |
| `manage_material_authoring` | MaterialAuthoring |
| `map_surface_to_sound` | Character |
| `material` | AssetWorkflow |
| `materialfunctioncall` | MaterialAuthoring |
| `materials` | AssetWorkflow |
| `medium` | EditorFunction |
| `merge_actors` | Control, Performance |
| `merge_vertices` | Geometry |
| `mesh` | AssetWorkflow |
| `meshes` | AssetWorkflow |
| `mirror` | Geometry |
| `mirror_weights` | Skeleton |
| `mixer` | MetaSound |
| `modify_heightmap` | Environment |
| `modify_reputation` | GameplayPrimitives |
| `modify_value` | GameplayPrimitives |
| `move` | AssetWorkflow |
| `mul` | MaterialAuthoring |
| `multiplicative` | GAS |
| `multiply` | GAS, MaterialAuthoring, MetaSound |
| `mute_player` | Sessions |
| `mw_configure_wwise_occlusion` | Audio |
| `name` | Blueprint |
| `nanite_rebuild_mesh` | AssetWorkflow, Render |
| `navigate_to_path` | EditorUtilities |
| `niagara` | Effect |
| `noise` | MaterialAuthoring, MetaSound |
| `noise_deform` | Geometry |
| `noisegenerator` | MetaSound |
| `none` | Character |
| `normalize_weights` | Skeleton |
| `object` | Blueprint |
| `offset_faces` | Geometry |
| `oneminus` | MaterialAuthoring |
| `open` | SequencerConsolidated |
| `open_asset` | Control |
| `open_bink_video` | AudioMiddleware |
| `open_level_blueprint` | LevelStructure |
| `open_midi_input` | VirtualProduction |
| `open_midi_output` | VirtualProduction |
| `open_source` | Media |
| `open_url` | Media |
| `open_usd_stage` | AssetPlugins |
| `optimize_draw_calls` | Performance |
| `optimize_lights_for_megalights` | Lighting |
| `optimize_shaders` | Performance |
| `oscillator` | MetaSound |
| `output` | MetaSound |
| `outset` | Geometry |
| `override` | GAS |
| `pack_uv_islands` | Geometry |
| `package_project` | Build |
| `paint_landscape` | Environment |
| `paint_landscape_layer` | Environment |
| `panner` | MaterialAuthoring |
| `parallel_execute` | Control |
| `parameter` | MetaSound |
| `particle` | Effect |
| `pause` | Control, Media, SequencerConsolidated |
| `pause_all_fmod_events` | AudioMiddleware |
| `pause_arcore_session` | XRPlugins |
| `pause_arkit_session` | XRPlugins |
| `pause_bink` | AudioMiddleware |
| `pause_sequence` | SequencerConsolidated |
| `pause_subject` | LiveLink |
| `pause_value_changes` | GameplayPrimitives |
| `pause_world_time` | GameplayPrimitives |
| `pawn` | BlueprintCreation |
| `perform_arcore_raycast` | XRPlugins |
| `perform_raycast` | XRPlugins |
| `phaser` | MetaSound |
| `pixeldepth` | MaterialAuthoring |
| `play` | Control, Media, SequencerConsolidated |
| `play_anim_montage` | Animation |
| `play_bink` | AudioMiddleware |
| `play_dialogue` | GameplaySystems |
| `play_fmod_event` | AudioMiddleware |
| `play_fmod_event_at_location` | AudioMiddleware |
| `play_geometry_cache` | AssetPlugins |
| `play_in_editor` | Ui |
| `play_montage` | Animation |
| `play_sequence` | SequencerConsolidated |
| `play_sound` | Control |
| `play_sound_2d` | Audio |
| `play_sound_at_location` | Audio |
| `play_sound_attached` | Audio |
| `playback_input_session` | Control |
| `poke` | Geometry |
| `pop_sound_mix` | Audio |
| `possess` | Control |
| `post_wwise_event` | AudioMiddleware |
| `post_wwise_event_at_location` | AudioMiddleware |
| `post_wwise_trigger` | AudioMiddleware |
| `pow` | MaterialAuthoring |
| `power` | MaterialAuthoring |
| `preview` | EditorFunction |
| `prime_sound` | Audio |
| `process_audio_to_blendshapes` | AINPC |
| `profile` | Control, Environment |
| `project_uv` | Geometry |
| `prune_weights` | Skeleton |
| `push_sound_mix` | Audio |
| `quadrangulate` | Geometry |
| `query_actors_by_predicate` | Control |
| `query_assets_by_predicate` | AssetWorkflow |
| `query_eqs_results` | AI |
| `query_interaction_targets` | Character |
| `query_mass_entities` | AI |
| `query_smart_objects` | AI |
| `query_water_bodies` | Water |
| `queue_operations` | Control |
| `randomize_substance_seed` | AssetPlugins |
| `read_config_value` | Data |
| `rebuild_material` | AssetWorkflow |
| `rebuild_navigation` | Navigation |
| `recalculate_normals` | Geometry |
| `recapture_scene` | Lighting, PostProcess |
| `receive_dmx` | VirtualProduction |
| `recompute_tangents` | Geometry |
| `record_flesh_simulation` | PhysicsDestruction |
| `record_geometry_collection_cache` | PhysicsDestruction |
| `record_input_session` | Control |
| `redo` | EditorUtilities |
| `reflectionoverride` | Control |
| `reflectionvector` | MaterialAuthoring |
| `reflectionvectorws` | MaterialAuthoring |
| `register_common_input_metadata` | UtilityPlugins |
| `register_editor_command` | UtilityPlugins |
| `register_mod_asset_redirect` | Modding |
| `register_voice_command` | XRPlugins |
| `reimport_alembic_asset` | AssetPlugins |
| `reimport_datasmith_scene` | AssetPlugins |
| `reimport_sbsar` | AssetPlugins |
| `relax` | Geometry |
| `release_fmod_snapshot` | AudioMiddleware |
| `release_smart_object` | AI |
| `reload_config` | Data |
| `reload_python_module` | UtilityPlugins |
| `remesh_uniform` | Geometry |
| `remesh_voxel` | Geometry |
| `remove` | Control |
| `remove_actor_binding` | UtilityPlugins |
| `remove_actors` | SequencerConsolidated |
| `remove_all_sources` | LiveLink |
| `remove_arcore_anchor` | XRPlugins |
| `remove_arkit_anchor` | XRPlugins |
| `remove_bone` | Skeleton |
| `remove_burn_in` | MovieRender |
| `remove_cloth_from_skeletal_mesh` | PhysicsDestruction |
| `remove_cluster_node` | VirtualProduction |
| `remove_composure_layer` | VirtualProduction |
| `remove_console_variable` | MovieRender |
| `remove_data_table_row` | Data |
| `remove_degenerates` | Geometry |
| `remove_foliage` | Environment |
| `remove_from_playlist` | Media |
| `remove_geometry_collection_cache` | PhysicsDestruction |
| `remove_icvfx_camera` | VirtualProduction |
| `remove_instance` | GameplaySystems |
| `remove_job` | MovieRender |
| `remove_keyframe` | SequencerConsolidated |
| `remove_livelink_source` | LiveLink |
| `remove_local_player` | Sessions |
| `remove_mapping` | Control, Input |
| `remove_material_node` | AssetWorkflow, MaterialAuthoring |
| `remove_menu_entry` | UtilityPlugins |
| `remove_metasound_node` | MetaSound |
| `remove_niagara_node` | NiagaraAdvanced |
| `remove_node` | BehaviorTree, MaterialGraph, NiagaraGraph |
| `remove_python_path` | UtilityPlugins |
| `remove_render_pass` | MovieRender |
| `remove_section` | SequencerConsolidated |
| `remove_shot` | SequencerConsolidated |
| `remove_spline_point` | Spline |
| `remove_subsequence` | SequencerConsolidated |
| `remove_tag` | Control |
| `remove_tag_from_container` | Data |
| `remove_toolbar_button` | UtilityPlugins |
| `remove_track` | SequencerConsolidated |
| `remove_variant` | UtilityPlugins |
| `remove_viewport` | VirtualProduction |
| `remove_virtual_subject` | LiveLink |
| `remove_wheel_from_vehicle` | PhysicsDestruction |
| `remove_widget_from_viewport` | Ui |
| `rename` | AssetWorkflow, SequencerConsolidated |
| `rename_bone` | Skeleton |
| `render_substance_textures` | AssetPlugins |
| `replace_actor_class` | Control |
| `request_gameplay_tag` | Data |
| `reset_accessibility_defaults` | Accessibility |
| `reset_control_rig` | Animation |
| `reset_instance_parameters` | AssetWorkflow |
| `reset_mod_system` | Modding |
| `reset_vp_state` | VirtualProduction |
| `reset_xr_orientation` | XRPlugins |
| `resize_texture` | Texture |
| `resolve_cloud_anchor` | XRPlugins |
| `restart_fmod_engine` | AudioMiddleware |
| `restart_wwise_engine` | AudioMiddleware |
| `restore_original_asset` | Modding |
| `restore_snapshot` | Control |
| `restore_state` | Control |
| `resume` | Control |
| `resume_all_fmod_events` | AudioMiddleware |
| `retarget_rpm_animation` | CharacterAvatar |
| `reverb` | MetaSound |
| `revolve` | Geometry |
| `rgb` | MaterialAuthoring |
| `rgba` | MaterialAuthoring |
| `rotator` | Blueprint, MaterialAuthoring |
| `run_benchmark` | Performance |
| `run_editor_utility` | UtilityPlugins |
| `run_functional_test` | Testing |
| `run_startup_scripts` | UtilityPlugins |
| `run_test` | Testing |
| `run_tests` | Control, Test, Testing |
| `run_ubt` | Build, Control |
| `save` | AssetWorkflow, Level |
| `save_all` | Ui |
| `save_as` | Level |
| `save_asset` | AssetWorkflow |
| `save_checkpoint` | GameplaySystems |
| `save_current_level` | Level |
| `save_game_to_slot` | Data |
| `save_level_as` | Level |
| `save_livelink_preset` | LiveLink |
| `save_spatial_anchor` | XRPlugins |
| `save_usd_stage` | AssetPlugins |
| `save_world_anchor` | XRPlugins |
| `saw` | MetaSound |
| `sawtooth` | MetaSound |
| `sawtoothoscillator` | MetaSound |
| `scalar` | MaterialAuthoring |
| `scalarparameter` | MaterialAuthoring |
| `scan_for_mod_paks` | Modding |
| `scatter_meshes_along_spline` | Spline |
| `screenshot` | Control, Environment, Ui |
| `scrub_to_time` | SequencerConsolidated |
| `sculpt` | Environment |
| `sculpt_landscape` | Environment |
| `search` | AssetWorkflow |
| `search_assets` | AssetQuery, AssetWorkflow |
| `search_fab_assets` | AssetPlugins |
| `seek` | Media |
| `seek_bink` | AudioMiddleware |
| `select_actor` | EditorUtilities |
| `select_actors_by_class` | EditorUtilities |
| `select_actors_by_tag` | EditorUtilities |
| `select_mesh_elements` | UtilityPlugins |
| `self_union` | Geometry |
| `send_dmx` | VirtualProduction |
| `send_message_to_character` | AINPC |
| `send_midi_cc` | VirtualProduction |
| `send_midi_note_off` | VirtualProduction |
| `send_midi_note_on` | VirtualProduction |
| `send_midi_pitch_bend` | VirtualProduction |
| `send_midi_program_change` | VirtualProduction |
| `send_osc_bundle` | VirtualProduction |
| `send_osc_message` | VirtualProduction |
| `send_server_rpc` | Networking |
| `send_text_to_character` | AINPC |
| `sequence_add_actor` | Sequence |
| `sequence_add_actors` | Sequence |
| `sequence_add_camera` | Sequence |
| `sequence_add_keyframe` | Sequence |
| `sequence_add_section` | Sequence |
| `sequence_add_spawnable_from_class` | Sequence |
| `sequence_add_track` | Sequence |
| `sequence_create` | Sequence |
| `sequence_delete` | Sequence |
| `sequence_duplicate` | Sequence |
| `sequence_get_bindings` | Sequence |
| `sequence_get_metadata` | Sequence |
| `sequence_get_properties` | Sequence |
| `sequence_list` | Sequence |
| `sequence_list_track_types` | Sequence |
| `sequence_list_tracks` | Sequence |
| `sequence_open` | Sequence |
| `sequence_pause` | Sequence |
| `sequence_play` | Sequence |
| `sequence_remove_actors` | Sequence |
| `sequence_remove_track` | Sequence |
| `sequence_rename` | Sequence |
| `sequence_set_display_rate` | Sequence |
| `sequence_set_playback_speed` | Sequence |
| `sequence_set_properties` | Sequence |
| `sequence_set_tick_resolution` | Sequence |
| `sequence_set_track_locked` | Sequence |
| `sequence_set_track_muted` | Sequence |
| `sequence_set_track_solo` | Sequence |
| `sequence_set_view_range` | Sequence |
| `sequence_set_work_range` | Sequence |
| `sequence_stop` | Sequence |
| `serialize_actor_state` | Control |
| `set_ability_cooldown` | GAS |
| `set_ability_costs` | GAS |
| `set_ability_tags` | GAS |
| `set_ability_targeting` | GAS |
| `set_activation_policy` | GAS |
| `set_actor_light_channel` | Lighting, PostProcess |
| `set_actor_state` | GameplayPrimitives |
| `set_actor_transform` | Control |
| `set_actor_visibility` | Control |
| `set_additive_settings` | AnimationAuthoring |
| `set_alembic_compression_type` | AssetPlugins |
| `set_alembic_normal_generation` | AssetPlugins |
| `set_alembic_sampling_settings` | AssetPlugins |
| `set_allowed_mod_operations` | Modding |
| `set_always_relevant` | Networking |
| `set_ambient_occlusion` | Lighting |
| `set_anim_graph_node_value` | AnimationAuthoring |
| `set_attribute_base_value` | GAS |
| `set_attribute_clamping` | GAS |
| `set_audio_accessibility_preset` | Accessibility |
| `set_audio_balance` | Accessibility |
| `set_audio_ducking` | Accessibility |
| `set_audio_occlusion` | Audio |
| `set_autonomous_proxy` | Networking |
| `set_aux_send` | AudioMiddleware |
| `set_axis_settings` | AnimationAuthoring |
| `set_base_sound_mix` | Audio |
| `set_bink_looping` | AudioMiddleware |
| `set_bink_rate` | AudioMiddleware |
| `set_bink_texture_player` | AudioMiddleware |
| `set_bink_volume` | AudioMiddleware |
| `set_blend_in` | AnimationAuthoring |
| `set_blend_mode` | MaterialAuthoring |
| `set_blend_out` | AnimationAuthoring |
| `set_blueprint_variables` | Control |
| `set_body_part` | CharacterAvatar |
| `set_body_type` | CharacterAvatar |
| `set_bone_key` | AnimationAuthoring |
| `set_bone_parent` | Skeleton |
| `set_bone_transform` | Skeleton |
| `set_bool_parameter` | CharacterAvatar |
| `set_buffer_settings` | LiveLink |
| `set_camera` | Control |
| `set_camera_fov` | Control |
| `set_camera_position` | Control |
| `set_center_of_mass` | PhysicsDestruction |
| `set_chromakey_settings` | VirtualProduction |
| `set_class_parent` | AudioAuthoring |
| `set_class_properties` | AudioAuthoring |
| `set_cloth_aerodynamics` | PhysicsDestruction |
| `set_cloth_anim_drive` | PhysicsDestruction |
| `set_cloth_collision_properties` | PhysicsDestruction |
| `set_cloth_damping` | PhysicsDestruction |
| `set_cloth_gravity` | PhysicsDestruction |
| `set_cloth_long_range_attachment` | PhysicsDestruction |
| `set_cloth_mass_properties` | PhysicsDestruction |
| `set_cloth_stiffness` | PhysicsDestruction |
| `set_cloth_tether_stiffness` | PhysicsDestruction |
| `set_cluster_connection_type` | PhysicsDestruction |
| `set_cognitive_accessibility_preset` | Accessibility |
| `set_collision_particles_fraction` | PhysicsDestruction |
| `set_color_parameter` | CharacterAvatar |
| `set_colorblind_severity` | Accessibility |
| `set_component_properties` | Control |
| `set_component_property` | Control, Environment |
| `set_compression_settings` | Texture |
| `set_control_value` | Animation |
| `set_controlled_component` | LiveLink |
| `set_controller_role` | LiveLink |
| `set_controller_subject` | LiveLink |
| `set_cue_attenuation` | AudioAuthoring |
| `set_cue_concurrency` | AudioAuthoring |
| `set_cue_effects` | GAS |
| `set_culture` | GameplaySystems |
| `set_cursor_size` | Accessibility |
| `set_curve_key` | AnimationAuthoring |
| `set_custom_timestep` | VirtualProduction |
| `set_cvar` | Control |
| `set_damage_thresholds` | PhysicsDestruction |
| `set_data_asset_property` | Data |
| `set_datalayer` | WorldPartition |
| `set_datasmith_tessellation_quality` | AssetPlugins |
| `set_default_focus_widget` | UtilityPlugins |
| `set_default_pawn_class` | GameFramework |
| `set_dialogue_context` | AudioAuthoring |
| `set_display_rate` | SequencerConsolidated |
| `set_display_view` | VirtualProduction |
| `set_doppler_effect` | Audio |
| `set_draco_compression` | AssetPlugins |
| `set_drag_coefficient` | PhysicsDestruction |
| `set_dynamic_state` | PhysicsDestruction |
| `set_edit_target_layer` | AssetPlugins |
| `set_editor_mode` | Control, EditorUtilities |
| `set_effect_duration` | GAS |
| `set_effect_stacking` | GAS |
| `set_effect_tags` | GAS |
| `set_emitter_properties` | NiagaraAuthoring |
| `set_exposed_property_value` | VirtualProduction |
| `set_exposure` | Lighting |
| `set_eye_color` | CharacterAvatar |
| `set_face_neutral_pose` | LiveLink |
| `set_face_parameter` | CharacterAvatar |
| `set_faction_relationship` | GameplayPrimitives |
| `set_file_name_format` | MovieRender |
| `set_fixture_channel_value` | VirtualProduction |
| `set_flesh_damping` | PhysicsDestruction |
| `set_flesh_incompressibility` | PhysicsDestruction |
| `set_flesh_inflation` | PhysicsDestruction |
| `set_flesh_rest_state` | PhysicsDestruction |
| `set_flesh_simulation_properties` | PhysicsDestruction |
| `set_flesh_solver_iterations` | PhysicsDestruction |
| `set_flesh_stiffness` | PhysicsDestruction |
| `set_float_parameter` | CharacterAvatar |
| `set_fmod_3d_attributes` | AudioMiddleware |
| `set_fmod_bus_mute` | AudioMiddleware |
| `set_fmod_bus_paused` | AudioMiddleware |
| `set_fmod_bus_volume` | AudioMiddleware |
| `set_fmod_global_parameter` | AudioMiddleware |
| `set_fmod_listener_attributes` | AudioMiddleware |
| `set_fmod_parameter` | AudioMiddleware |
| `set_fmod_studio_path` | AudioMiddleware |
| `set_fmod_vca_volume` | AudioMiddleware |
| `set_font_size` | Accessibility |
| `set_frame_rate` | MovieRender, VirtualProduction |
| `set_frame_rate_limit` | Performance |
| `set_friction` | EditorUtilities |
| `set_fullscreen` | Control |
| `set_game_speed` | Accessibility, Control |
| `set_game_state` | GameplaySystems |
| `set_game_state_class` | GameFramework |
| `set_geometry_cache_time` | AssetPlugins |
| `set_geometry_collection_materials` | PhysicsDestruction |
| `set_gltf_export_scale` | AssetPlugins |
| `set_gltf_texture_format` | AssetPlugins |
| `set_grid_settings` | EditorUtilities |
| `set_hair_color` | CharacterAvatar |
| `set_hair_root_scale` | CharacterAvatar |
| `set_hair_style` | CharacterAvatar |
| `set_hair_tip_scale` | CharacterAvatar |
| `set_hair_width` | CharacterAvatar |
| `set_hda_bool_parameter` | AssetPlugins |
| `set_hda_color_parameter` | AssetPlugins |
| `set_hda_curve_input` | AssetPlugins |
| `set_hda_float_parameter` | AssetPlugins |
| `set_hda_geometry_input` | AssetPlugins |
| `set_hda_int_parameter` | AssetPlugins |
| `set_hda_multi_parameter` | AssetPlugins |
| `set_hda_ramp_parameter` | AssetPlugins |
| `set_hda_string_parameter` | AssetPlugins |
| `set_hda_vector_parameter` | AssetPlugins |
| `set_hda_world_input` | AssetPlugins |
| `set_high_contrast_colors` | Accessibility |
| `set_input_action_data` | UtilityPlugins |
| `set_input_mode` | Ui |
| `set_input_timing_tolerance` | Accessibility |
| `set_instancing_policy` | GAS |
| `set_int_parameter` | CharacterAvatar |
| `set_interaction_enabled` | GameplayPrimitives |
| `set_interchange_pipeline_stack` | AssetPlugins |
| `set_interchange_result_container` | AssetPlugins |
| `set_interchange_translator` | AssetPlugins |
| `set_interpolation_settings` | AnimationAuthoring |
| `set_inventory_replication` | Inventory |
| `set_item_properties` | Inventory |
| `set_key_instance_synced` | AI |
| `set_landscape_material` | Environment |
| `set_light_channel` | Lighting, PostProcess |
| `set_listener_position` | AudioMiddleware |
| `set_lod_bias` | Texture |
| `set_lod_screen_sizes` | Geometry |
| `set_lod_settings` | Geometry |
| `set_looping` | Media |
| `set_loot_quality_tiers` | Inventory |
| `set_lumen_reflections` | Lighting |
| `set_map` | MovieRender |
| `set_mass_entity_fragment` | AI |
| `set_material_domain` | MaterialAuthoring |
| `set_mesh_collision` | UtilityPlugins |
| `set_mesh_colors` | UtilityPlugins |
| `set_mesh_normals` | UtilityPlugins |
| `set_mesh_section_visible` | UtilityPlugins |
| `set_mesh_tangents` | UtilityPlugins |
| `set_mesh_triangles` | UtilityPlugins |
| `set_mesh_uvs` | UtilityPlugins |
| `set_mesh_vertices` | UtilityPlugins |
| `set_metadata` | AssetWorkflow, LevelStructure, SequencerConsolidated |
| `set_metasound_default` | AudioAuthoring |
| `set_metasound_variable` | MetaSound |
| `set_modifier_magnitude` | GAS |
| `set_morph_target_deltas` | Skeleton |
| `set_motion_matching_goal` | Animation |
| `set_motor_accessibility_preset` | Accessibility |
| `set_movement_mode` | Character |
| `set_nanite_settings` | AssetWorkflow, Geometry |
| `set_nav_agent_properties` | Navigation |
| `set_nav_area_class` | Navigation |
| `set_nav_link_type` | Navigation |
| `set_net_dormancy` | Networking |
| `set_net_role` | Networking |
| `set_node_properties` | BehaviorTree |
| `set_node_property` | BlueprintGraph |
| `set_objective_state` | GameplaySystems |
| `set_ocean_extent` | Water |
| `set_ocio_working_colorspace` | VirtualProduction |
| `set_only_relevant_to_owner` | Networking |
| `set_output_directory` | MovieRender |
| `set_overlay_texture` | XRPlugins |
| `set_owner` | Networking |
| `set_parameter` | NiagaraGraph |
| `set_parameter_value` | NiagaraAuthoring |
| `set_pcg_node_settings` | PCG |
| `set_pcg_partition_grid_size` | PCG |
| `set_perception_team` | AI |
| `set_pin_default_value` | BlueprintGraph |
| `set_playback_range` | SequencerConsolidated |
| `set_playback_speed` | SequencerConsolidated |
| `set_player_controller_class` | GameFramework |
| `set_player_state_class` | GameFramework |
| `set_preferences` | Control |
| `set_primary_node` | VirtualProduction |
| `set_project_setting` | Control, Environment |
| `set_projection_policy` | VirtualProduction |
| `set_projector_parameter` | CharacterAvatar |
| `set_properties` | SequencerConsolidated |
| `set_property` | Control, Environment |
| `set_property_replicated` | Networking |
| `set_quality` | Control, Environment |
| `set_quality_level` | CharacterAvatar, GameplaySystems |
| `set_rate` | Media |
| `set_remove_on_break` | PhysicsDestruction |
| `set_render_scale` | XRPlugins |
| `set_replicated_using` | Networking |
| `set_replication_condition` | Networking |
| `set_resolution` | Control, MovieRender |
| `set_resolution_scale` | GameplaySystems, Performance |
| `set_respawn_rules` | GameFramework |
| `set_restitution` | EditorUtilities |
| `set_retarget_chain_mapping` | AnimationAuthoring, ControlRig |
| `set_river_depth` | Water |
| `set_river_transitions` | Water |
| `set_root_motion_settings` | AnimationAuthoring |
| `set_rpc_reliability` | Networking |
| `set_rpm_outfit` | CharacterAvatar |
| `set_rtpc_value` | AudioMiddleware |
| `set_rtpc_value_on_actor` | AudioMiddleware |
| `set_scalability` | Performance |
| `set_scalar_parameter_value` | MaterialAuthoring |
| `set_schedule_active` | GameplayPrimitives |
| `set_sculpt_brush` | UtilityPlugins |
| `set_section_timing` | AnimationAuthoring |
| `set_sequence` | MovieRender |
| `set_sequence_length` | AnimationAuthoring |
| `set_shading_model` | MaterialAuthoring |
| `set_skin_tone` | CharacterAvatar |
| `set_skylight_intensity` | Environment |
| `set_snap_settings` | EditorUtilities |
| `set_sound_attenuation` | Audio |
| `set_sound_mix_class_override` | Audio |
| `set_spatial_sample_count` | MovieRender |
| `set_spawner_enabled` | GameplayPrimitives |
| `set_speedtree_lod_distances` | AssetPlugins |
| `set_speedtree_lod_transition` | AssetPlugins |
| `set_speedtree_wind_speed` | AssetPlugins |
| `set_speedtree_wind_type` | AssetPlugins |
| `set_spline_mesh_asset` | Spline |
| `set_spline_mesh_material` | Spline |
| `set_spline_point_position` | Spline |
| `set_spline_point_rotation` | Spline |
| `set_spline_point_scale` | Spline |
| `set_spline_point_tangents` | Spline |
| `set_spline_type` | Spline |
| `set_split_screen_type` | Sessions |
| `set_stage_settings` | VirtualProduction |
| `set_steamvr_action_manifest` | XRPlugins |
| `set_streaming_distance` | LevelStructure |
| `set_streaming_priority` | Texture |
| `set_substance_bool_parameter` | AssetPlugins |
| `set_substance_color_parameter` | AssetPlugins |
| `set_substance_float_parameter` | AssetPlugins |
| `set_substance_image_input` | AssetPlugins |
| `set_substance_int_parameter` | AssetPlugins |
| `set_substance_output_format` | AssetPlugins |
| `set_substance_string_parameter` | AssetPlugins |
| `set_substrate_properties` | MaterialAuthoring |
| `set_subtitle_font_size` | Accessibility |
| `set_subtitle_preset` | Accessibility |
| `set_sun_intensity` | Environment |
| `set_sync_policy` | VirtualProduction |
| `set_tags` | AssetWorkflow |
| `set_temporal_sample_count` | MovieRender |
| `set_texture_group` | Texture |
| `set_texture_parameter` | CharacterAvatar |
| `set_texture_parameter_value` | MaterialAuthoring |
| `set_tick_resolution` | SequencerConsolidated |
| `set_tile_count` | MovieRender |
| `set_tile_map_layer` | UtilityPlugins |
| `set_time_of_day` | Environment |
| `set_time_scale` | GameplayPrimitives |
| `set_timecode_provider` | LiveLink, VirtualProduction |
| `set_timer` | EditorUtilities |
| `set_tool_property` | UtilityPlugins |
| `set_track_locked` | SequencerConsolidated |
| `set_track_muted` | SequencerConsolidated |
| `set_track_solo` | SequencerConsolidated |
| `set_tracking_origin` | XRPlugins |
| `set_transform` | Control |
| `set_transform_parameter` | CharacterAvatar |
| `set_transition_rules` | AnimationAuthoring |
| `set_ui_scale` | Accessibility |
| `set_usd_prim_attribute` | AssetPlugins |
| `set_usd_variant` | AssetPlugins |
| `set_value` | GameplayPrimitives |
| `set_vector_parameter` | CharacterAvatar |
| `set_vector_parameter_value` | MaterialAuthoring |
| `set_vehicle_animation_bp` | PhysicsDestruction |
| `set_vehicle_mass` | PhysicsDestruction |
| `set_vehicle_mesh` | PhysicsDestruction |
| `set_vertex_weights` | Skeleton |
| `set_view_mode` | Control |
| `set_view_range` | SequencerConsolidated |
| `set_viewport_camera` | Control, VirtualProduction |
| `set_viewport_realtime` | Control |
| `set_viewport_resolution` | Control |
| `set_virtual_shadow_maps` | Lighting |
| `set_visibility` | Control |
| `set_visual_accessibility_preset` | Accessibility |
| `set_voice_attenuation` | Sessions |
| `set_voice_channel` | Sessions |
| `set_volume_extent` | Volume |
| `set_volume_properties` | Volume |
| `set_vsync` | Performance |
| `set_water_static_mesh` | Water |
| `set_water_zone` | Water |
| `set_weapon_stats` | Combat |
| `set_wheel_class` | PhysicsDestruction |
| `set_wheel_offset` | PhysicsDestruction |
| `set_wheel_radius` | PhysicsDestruction |
| `set_widget_image` | Ui |
| `set_widget_text` | Ui |
| `set_widget_visibility` | Ui |
| `set_work_range` | SequencerConsolidated |
| `set_world_time` | GameplayPrimitives |
| `set_wwise_game_object` | AudioMiddleware |
| `set_wwise_project_path` | AudioMiddleware |
| `set_wwise_state` | AudioMiddleware |
| `set_wwise_switch` | AudioMiddleware |
| `set_wwise_switch_on_actor` | AudioMiddleware |
| `set_xr_device_priority` | XRPlugins |
| `set_zone_property` | GameplayPrimitives |
| `setup_ammo_system` | Combat |
| `setup_attachment_system` | Combat |
| `setup_climbing` | Character |
| `setup_destructible_mesh` | Interaction |
| `setup_footstep_system` | Character |
| `setup_global_illumination` | Lighting |
| `setup_grappling` | Character |
| `setup_hitbox_component` | Combat |
| `setup_ik` | Animation |
| `setup_mantling` | Character |
| `setup_match_states` | GameFramework |
| `setup_ml_deformer` | ControlRig |
| `setup_niagara_fluids` | NiagaraAdvanced |
| `setup_parry_block_system` | Combat |
| `setup_physics_simulation` | Animation |
| `setup_reload_system` | Combat |
| `setup_retargeting` | Animation |
| `setup_sliding` | Character |
| `setup_vaulting` | Character |
| `setup_volumetric_fog` | Lighting |
| `setup_wall_running` | Character |
| `setup_weapon_switching` | Combat |
| `shadercomplexity` | Control |
| `sharpen` | Texture |
| `shell` | Geometry |
| `show` | LevelStructure |
| `show_fps` | Control, Environment, Performance |
| `show_in_explorer` | EditorUtilities |
| `show_mouse_cursor` | Ui |
| `show_overlay` | XRPlugins |
| `show_stats` | Performance |
| `show_widget` | Control |
| `simplify_collision` | Geometry |
| `simplify_mesh` | Geometry |
| `simulate_input` | Control, Ui |
| `simulate_network_conditions` | Networking |
| `sine` | MetaSound |
| `sineoscillator` | MetaSound |
| `skip_to_schedule_entry` | GameplayPrimitives |
| `smooth` | Geometry |
| `sound` | AssetWorkflow |
| `sounds` | AssetWorkflow |
| `spawn` | Control |
| `spawn_blueprint` | Control |
| `spawn_category` | Control, Debug |
| `spawn_customizable_actor` | CharacterAvatar |
| `spawn_groom_actor` | CharacterAvatar |
| `spawn_hda_actor` | AssetPlugins |
| `spawn_light` | Level, Lighting |
| `spawn_mass_ai_entities` | AI |
| `spawn_mass_entity` | AI |
| `spawn_metahuman_actor` | CharacterAvatar |
| `spawn_niagara` | Effect |
| `spawn_paper_flipbook_actor` | UtilityPlugins |
| `spawn_paper_sprite_actor` | UtilityPlugins |
| `spawn_sky_light` | Lighting |
| `spawn_sound_at_location` | Audio |
| `spawn_usd_stage_actor` | AssetPlugins |
| `spherify` | Geometry |
| `split_normals` | Geometry |
| `square` | MetaSound |
| `squareoscillator` | MetaSound |
| `stack_anim_layers` | Animation |
| `start_arcore_session` | XRPlugins |
| `start_arkit_session` | XRPlugins |
| `start_audio2face_stream` | AINPC |
| `start_background_job` | Control |
| `start_convai_session` | AINPC |
| `start_inworld_session` | AINPC |
| `start_profiling` | Performance |
| `start_recording` | Control |
| `start_render` | MovieRender |
| `start_session` | Control, Insights |
| `start_stats_capture` | Testing |
| `start_trace` | Testing |
| `start_web_server` | VirtualProduction |
| `staticmesh` | AssetWorkflow |
| `staticswitch` | MaterialAuthoring |
| `staticswitchparameter` | MaterialAuthoring |
| `stationarylightoverlap` | Control |
| `step_frame` | Control |
| `stop` | Control, Media, SequencerConsolidated |
| `stop_audio2face_stream` | AINPC |
| `stop_bink` | AudioMiddleware |
| `stop_convai_session` | AINPC |
| `stop_fmod_event` | AudioMiddleware |
| `stop_haptic_feedback` | XRPlugins |
| `stop_inworld_session` | AINPC |
| `stop_osc_server` | VirtualProduction |
| `stop_pie` | Control |
| `stop_play` | Ui |
| `stop_profiling` | Performance |
| `stop_recording` | Control |
| `stop_render` | MovieRender |
| `stop_sequence` | SequencerConsolidated |
| `stop_stats_capture` | Testing |
| `stop_trace` | Testing |
| `stop_web_server` | VirtualProduction |
| `stop_wwise_event` | AudioMiddleware |
| `stream` | Level |
| `stream_level` | Level |
| `stream_level_async` | LevelStructure |
| `stretch` | Geometry |
| `string` | Blueprint |
| `sub` | MaterialAuthoring |
| `subdivide` | Geometry |
| `subscribe` | Control, Log |
| `subscribe_to_event` | Control |
| `subtract` | MaterialAuthoring, MetaSound |
| `suggest_fix_for_error` | Control |
| `sweep` | Geometry |
| `swim` | Character |
| `swimming` | Character |
| `switch` | MaterialAuthoring |
| `sync_datasmith_changes` | AssetPlugins |
| `sync_to_asset` | EditorUtilities |
| `synchronize_timecode` | VirtualProduction |
| `take_photo_mode_screenshot` | GameplaySystems |
| `taper` | Geometry |
| `test_activate_ability` | GAS |
| `test_apply_effect` | GAS |
| `test_get_attribute` | GAS |
| `test_get_gameplay_tags` | GAS |
| `texcoord` | MaterialAuthoring |
| `text` | Blueprint |
| `texture` | AssetWorkflow, MaterialAuthoring |
| `texture2d` | MaterialAuthoring |
| `texturecoordinate` | MaterialAuthoring |
| `textureparameter` | MaterialAuthoring |
| `textures` | AssetWorkflow |
| `texturesample` | MaterialAuthoring |
| `texturesampleparameter2d` | MaterialAuthoring |
| `toggle_realtime_rendering` | Control |
| `transfer_control` | GameplayPrimitives |
| `transform` | Blueprint |
| `transform_uvs` | Geometry |
| `triangle` | MetaSound |
| `triangleoscillator` | MetaSound |
| `triangulate` | Geometry |
| `trigger_haptic_feedback` | XRPlugins |
| `trigger_inworld_event` | AINPC |
| `trigger_metasound` | Audio |
| `trigger_statetree_transition` | AI |
| `trigger_steamvr_haptic` | XRPlugins |
| `tune_lumen_performance` | Lighting |
| `twist` | Geometry |
| `unbind_actor` | SequencerConsolidated |
| `unbind_from_event` | EditorUtilities |
| `unbind_from_texture` | Media |
| `unbind_midi` | VirtualProduction |
| `unbind_osc_address` | VirtualProduction |
| `undo` | EditorUtilities |
| `undo_mesh_operation` | UtilityPlugins |
| `unexpose_property` | VirtualProduction |
| `ungroup_actors` | EditorUtilities |
| `unlit` | Control |
| `unload` | LevelStructure |
| `unload_fmod_bank` | AudioMiddleware |
| `unload_mod_pak` | Modding |
| `unload_wwise_bank` | AudioMiddleware |
| `unpause_subject` | LiveLink |
| `unregister_editor_command` | UtilityPlugins |
| `unregister_voice_command` | XRPlugins |
| `unset_wwise_game_object` | AudioMiddleware |
| `unsubscribe` | Control, Log |
| `unsubscribe_from_event` | Control |
| `unwrap_uv` | Geometry |
| `update_datasmith_scene` | AssetPlugins |
| `update_mesh_section` | UtilityPlugins |
| `update_skeletal_mesh` | CharacterAvatar |
| `use_material_function` | MaterialAuthoring |
| `uv` | MaterialAuthoring |
| `validate` | AssetWorkflow |
| `validate_action_input` | Control |
| `validate_asset` | AssetWorkflow, Testing |
| `validate_assets` | Build, Control, Environment |
| `validate_assets_in_path` | Testing |
| `validate_blueprint` | AssetWorkflow, Testing |
| `validate_level` | LevelStructure |
| `validate_lighting_setup` | Lighting |
| `validate_material` | MaterialAuthoring |
| `validate_mod_content` | Modding |
| `validate_mod_pak` | Modding |
| `validate_niagara_system` | NiagaraAuthoring |
| `validate_operation_preconditions` | Control |
| `vector` | Blueprint, MaterialAuthoring |
| `vectorparameter` | MaterialAuthoring |
| `vertexnormal` | MaterialAuthoring |
| `vertexnormalws` | MaterialAuthoring |
| `walk` | Character |
| `walking` | Character |
| `weld_vertices` | Geometry |
| `whitenoise` | MetaSound |
| `wireframe` | Control |
| `worldposition` | MaterialAuthoring |
| `write_config_value` | Data |
