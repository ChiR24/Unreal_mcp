# Live Action Reference

> **Generated from LIVE MCP server** - This documents exactly what an LLM client sees at runtime.
> Unlike the static action reference, this is fetched via the actual MCP protocol.

Generated: 2026-01-21T16:53:16.490Z

## Summary

| Metric | Value |
|--------|-------|
| Total Tools | 36 |
| Total Actions | 2575 |
| Server Command | `node X:\Newfolder(2)\MCP\Unreal 5.6\Unreal_mcp\dist\cli.js` |

## Table of Contents

- [manage_asset](#manage-asset) (99 actions)
- [control_actor](#control-actor) (45 actions)
- [control_editor](#control-editor) (84 actions)
- [manage_level](#manage-level) (87 actions)
- [manage_motion_design](#manage-motion-design) (10 actions)
- [animation_physics](#animation-physics) (162 actions)
- [manage_effect](#manage-effect) (74 actions)
- [build_environment](#build-environment) (58 actions)
- [manage_sequence](#manage-sequence) (100 actions)
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
- [manage_ui](#manage-ui) (7 actions)
- [manage_gameplay_abilities](#manage-gameplay-abilities) (18 actions)
- [manage_attribute_sets](#manage-attribute-sets) (6 actions)
- [manage_gameplay_cues](#manage-gameplay-cues) (3 actions)
- [test_gameplay_abilities](#test-gameplay-abilities) (4 actions)

---

## manage_asset

Assets, Materials, dependencies; Blueprints (SCS, graph nodes).

### Actions

*99 actions available*

| # | Action |
|---|--------|
| 1 | `list` |
| 2 | `import` |
| 3 | `duplicate` |
| 4 | `rename` |
| 5 | `move` |
| 6 | `delete` |
| 7 | `delete_asset` |
| 8 | `delete_assets` |
| 9 | `create_folder` |
| 10 | `search_assets` |
| 11 | `get_dependencies` |
| 12 | `get_source_control_state` |
| 13 | `analyze_graph` |
| 14 | `get_asset_graph` |
| 15 | `create_thumbnail` |
| 16 | `set_tags` |
| 17 | `get_metadata` |
| 18 | `set_metadata` |
| 19 | `validate` |
| 20 | `fixup_redirectors` |
| 21 | `find_by_tag` |
| 22 | `generate_report` |
| 23 | `create_material` |
| 24 | `create_material_instance` |
| 25 | `create_render_target` |
| 26 | `generate_lods` |
| 27 | `add_material_parameter` |
| 28 | `list_instances` |
| 29 | `reset_instance_parameters` |
| 30 | `exists` |
| 31 | `get_material_stats` |
| 32 | `nanite_rebuild_mesh` |
| 33 | `enable_nanite_mesh` |
| 34 | `set_nanite_settings` |
| 35 | `batch_nanite_convert` |
| 36 | `add_material_node` |
| 37 | `connect_material_pins` |
| 38 | `remove_material_node` |
| 39 | `break_material_connections` |
| 40 | `get_material_node_details` |
| 41 | `rebuild_material` |
| 42 | `create_metasound` |
| 43 | `add_metasound_node` |
| 44 | `connect_metasound_nodes` |
| 45 | `remove_metasound_node` |
| 46 | `set_metasound_variable` |
| 47 | `create_oscillator` |
| 48 | `create_envelope` |
| 49 | `create_filter` |
| 50 | `create_sequencer_node` |
| 51 | `create_procedural_music` |
| 52 | `import_audio_to_metasound` |
| 53 | `export_metasound_preset` |
| 54 | `configure_audio_modulation` |
| 55 | `bp_create` |
| 56 | `bp_get` |
| 57 | `bp_compile` |
| 58 | `bp_add_component` |
| 59 | `bp_set_default` |
| 60 | `bp_modify_scs` |
| 61 | `bp_get_scs` |
| 62 | `bp_add_scs_component` |
| 63 | `bp_remove_scs_component` |
| 64 | `bp_reparent_scs_component` |
| 65 | `bp_set_scs_transform` |
| 66 | `bp_set_scs_property` |
| 67 | `bp_ensure_exists` |
| 68 | `bp_probe_handle` |
| 69 | `bp_add_variable` |
| 70 | `bp_remove_variable` |
| 71 | `bp_rename_variable` |
| 72 | `bp_add_function` |
| 73 | `bp_add_event` |
| 74 | `bp_remove_event` |
| 75 | `bp_add_construction_script` |
| 76 | `bp_set_variable_metadata` |
| 77 | `bp_create_node` |
| 78 | `bp_add_node` |
| 79 | `bp_delete_node` |
| 80 | `bp_connect_pins` |
| 81 | `bp_break_pin_links` |
| 82 | `bp_set_node_property` |
| 83 | `bp_create_reroute_node` |
| 84 | `bp_get_node_details` |
| 85 | `bp_get_graph_details` |
| 86 | `bp_get_pin_details` |
| 87 | `bp_list_node_types` |
| 88 | `bp_set_pin_default_value` |
| 89 | `query_assets_by_predicate` |
| 90 | `bp_implement_interface` |
| 91 | `bp_add_macro` |
| 92 | `bp_create_widget_binding` |
| 93 | `bp_add_custom_event` |
| 94 | `bp_set_replication_settings` |
| 95 | `bp_add_event_dispatcher` |
| 96 | `bp_bind_event` |
| 97 | `get_blueprint_dependencies` |
| 98 | `validate_blueprint` |
| 99 | `compile_blueprint_batch` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `assetPath` | unknown | No |
| `directory` | unknown | No |
| `classNames` | array | No |
| `packagePaths` | array | No |
| `recursivePaths` | unknown | No |
| `recursiveClasses` | unknown | No |
| `limit` | unknown | No |
| `sourcePath` | unknown | No |
| `destinationPath` | unknown | No |
| `assetPaths` | array | No |
| `lodCount` | unknown | No |
| `reductionSettings` | object | No |
| `nodeName` | unknown | No |
| `eventName` | unknown | No |
| `memberClass` | unknown | No |
| `posX` | unknown | No |
| `newName` | unknown | No |
| `overwrite` | unknown | No |
| `save` | unknown | No |
| `fixupRedirectors` | unknown | No |
| `directoryPath` | unknown | No |
| `name` | unknown | No |
| `path` | unknown | No |
| `parentMaterial` | unknown | No |
| `parameters` | object | No |
| `width` | unknown | No |
| `height` | unknown | No |
| `meshPath` | unknown | No |
| `tag` | unknown | No |
| `metadata` | object | No |
| `graphName` | unknown | No |
| `nodeType` | unknown | No |
| `nodeId` | unknown | No |
| `sourceNodeId` | unknown | No |
| `targetNodeId` | unknown | No |
| `inputName` | unknown | No |
| `fromNodeId` | unknown | No |
| `fromPin` | unknown | No |
| `toNodeId` | unknown | No |
| `toPin` | unknown | No |
| `parameterName` | unknown | No |
| `value` | unknown | No |
| `x` | unknown | No |
| `y` | unknown | No |
| `comment` | unknown | No |
| `parentNodeId` | unknown | No |
| `childNodeId` | unknown | No |
| `maxDepth` | unknown | No |
| `metaSoundName` | unknown | No |
| `metaSoundPath` | unknown | No |
| `enableNanite` | unknown | No |
| `nanitePositionPrecision` | unknown | No |
| `nanitePercentTriangles` | unknown | No |
| `naniteFallbackRelativeError` | unknown | No |
| `refresh` | unknown | No |

---

## control_actor

Spawn actors, transforms, physics, components, tags, attachments.

### Actions

*45 actions available*

| # | Action |
|---|--------|
| 1 | `spawn` |
| 2 | `spawn_blueprint` |
| 3 | `delete` |
| 4 | `delete_by_tag` |
| 5 | `duplicate` |
| 6 | `apply_force` |
| 7 | `set_transform` |
| 8 | `get_transform` |
| 9 | `set_visibility` |
| 10 | `add_component` |
| 11 | `set_component_properties` |
| 12 | `get_components` |
| 13 | `add_tag` |
| 14 | `remove_tag` |
| 15 | `find_by_tag` |
| 16 | `find_by_name` |
| 17 | `list` |
| 18 | `set_blueprint_variables` |
| 19 | `create_snapshot` |
| 20 | `attach` |
| 21 | `detach` |
| 22 | `inspect_object` |
| 23 | `set_property` |
| 24 | `get_property` |
| 25 | `inspect_class` |
| 26 | `list_objects` |
| 27 | `get_component_property` |
| 28 | `set_component_property` |
| 29 | `get_metadata` |
| 30 | `restore_snapshot` |
| 31 | `export` |
| 32 | `delete_object` |
| 33 | `find_by_class` |
| 34 | `get_bounding_box` |
| 35 | `query_actors_by_predicate` |
| 36 | `get_all_component_properties` |
| 37 | `batch_set_component_properties` |
| 38 | `clone_component_hierarchy` |
| 39 | `serialize_actor_state` |
| 40 | `deserialize_actor_state` |
| 41 | `get_actor_bounds` |
| 42 | `batch_transform_actors` |
| 43 | `get_actor_references` |
| 44 | `replace_actor_class` |
| 45 | `merge_actors` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `actorName` | unknown | No |
| `childActor` | unknown | No |
| `parentActor` | unknown | No |
| `classPath` | unknown | No |
| `meshPath` | unknown | No |
| `blueprintPath` | unknown | No |
| `location` | object | No |
| `rotation` | object | No |
| `scale` | object | No |
| `force` | object | No |
| `componentType` | unknown | No |
| `componentName` | unknown | No |
| `properties` | object | No |
| `visible` | unknown | No |
| `newName` | unknown | No |
| `tag` | unknown | No |
| `variables` | object | No |
| `snapshotName` | unknown | No |
| `objectPath` | unknown | No |
| `propertyName` | unknown | No |
| `propertyPath` | unknown | No |
| `value` | unknown | No |
| `name` | unknown | No |
| `className` | unknown | No |
| `filter` | unknown | No |
| `destinationPath` | unknown | No |
| `outputPath` | unknown | No |
| `refresh` | unknown | No |

---

## control_editor

PIE, viewport, console, screenshots, profiling, CVars, UBT, widgets.

### Actions

*84 actions available*

| # | Action |
|---|--------|
| 1 | `play` |
| 2 | `stop` |
| 3 | `stop_pie` |
| 4 | `pause` |
| 5 | `resume` |
| 6 | `set_game_speed` |
| 7 | `eject` |
| 8 | `possess` |
| 9 | `set_camera` |
| 10 | `set_camera_position` |
| 11 | `set_camera_fov` |
| 12 | `set_view_mode` |
| 13 | `set_viewport_resolution` |
| 14 | `console_command` |
| 15 | `execute_command` |
| 16 | `screenshot` |
| 17 | `step_frame` |
| 18 | `start_recording` |
| 19 | `stop_recording` |
| 20 | `create_bookmark` |
| 21 | `jump_to_bookmark` |
| 22 | `set_preferences` |
| 23 | `set_viewport_realtime` |
| 24 | `open_asset` |
| 25 | `simulate_input` |
| 26 | `create_input_action` |
| 27 | `create_input_mapping_context` |
| 28 | `add_mapping` |
| 29 | `remove_mapping` |
| 30 | `profile` |
| 31 | `show_fps` |
| 32 | `set_quality` |
| 33 | `set_resolution` |
| 34 | `set_fullscreen` |
| 35 | `run_ubt` |
| 36 | `run_tests` |
| 37 | `subscribe` |
| 38 | `unsubscribe` |
| 39 | `spawn_category` |
| 40 | `start_session` |
| 41 | `lumen_update_scene` |
| 42 | `subscribe_to_event` |
| 43 | `unsubscribe_from_event` |
| 44 | `get_subscribed_events` |
| 45 | `configure_event_channel` |
| 46 | `get_event_history` |
| 47 | `clear_event_subscriptions` |
| 48 | `start_background_job` |
| 49 | `get_job_status` |
| 50 | `cancel_job` |
| 51 | `get_active_jobs` |
| 52 | `play_sound` |
| 53 | `create_widget` |
| 54 | `show_widget` |
| 55 | `add_widget_child` |
| 56 | `set_cvar` |
| 57 | `get_project_settings` |
| 58 | `validate_assets` |
| 59 | `set_project_setting` |
| 60 | `batch_execute` |
| 61 | `parallel_execute` |
| 62 | `queue_operations` |
| 63 | `flush_operation_queue` |
| 64 | `capture_viewport` |
| 65 | `get_last_error_details` |
| 66 | `suggest_fix_for_error` |
| 67 | `validate_operation_preconditions` |
| 68 | `get_operation_history` |
| 69 | `get_available_actions` |
| 70 | `explain_action_parameters` |
| 71 | `get_class_hierarchy` |
| 72 | `validate_action_input` |
| 73 | `get_action_statistics` |
| 74 | `get_bridge_health` |
| 75 | `configure_megalights` |
| 76 | `get_light_budget_stats` |
| 77 | `convert_to_substrate` |
| 78 | `batch_substrate_migration` |
| 79 | `record_input_session` |
| 80 | `playback_input_session` |
| 81 | `capture_viewport_sequence` |
| 82 | `set_editor_mode` |
| 83 | `get_selection_info` |
| 84 | `toggle_realtime_rendering` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `location` | object | No |
| `rotation` | object | No |
| `viewMode` | unknown | No |
| `enabled` | unknown | No |
| `speed` | unknown | No |
| `filename` | unknown | No |
| `fov` | unknown | No |
| `width` | unknown | No |
| `height` | unknown | No |
| `command` | unknown | No |
| `steps` | unknown | No |
| `bookmarkName` | unknown | No |
| `assetPath` | unknown | No |
| `keyName` | unknown | No |
| `eventType` | unknown | No |
| `contextPath` | unknown | No |
| `actionPath` | unknown | No |
| `key` | unknown | No |
| `profileType` | unknown | No |
| `level` | unknown | No |
| `resolution` | unknown | No |
| `target` | unknown | No |
| `platform` | unknown | No |
| `configuration` | unknown | No |
| `arguments` | unknown | No |
| `filter` | unknown | No |
| `channels` | unknown | No |
| `widgetPath` | unknown | No |
| `childClass` | unknown | No |
| `parentName` | unknown | No |
| `section` | unknown | No |
| `value` | unknown | No |
| `configName` | unknown | No |
| `outputPath` | unknown | No |
| `returnBase64` | unknown | No |
| `captureHDR` | unknown | No |
| `showUI` | unknown | No |
| `requests` | array | No |
| `operations` | array | No |
| `stopOnError` | unknown | No |
| `maxConcurrency` | unknown | No |
| `queueId` | unknown | No |

---

## manage_level

Levels, streaming, World Partition, data layers, HLOD; PCG graphs.

### Actions

*87 actions available*

| # | Action |
|---|--------|
| 1 | `load` |
| 2 | `save` |
| 3 | `save_as` |
| 4 | `save_level_as` |
| 5 | `stream` |
| 6 | `create_level` |
| 7 | `create_light` |
| 8 | `build_lighting` |
| 9 | `set_metadata` |
| 10 | `load_cells` |
| 11 | `set_datalayer` |
| 12 | `export_level` |
| 13 | `import_level` |
| 14 | `list_levels` |
| 15 | `get_summary` |
| 16 | `delete` |
| 17 | `validate_level` |
| 18 | `cleanup_invalid_datalayers` |
| 19 | `add_sublevel` |
| 20 | `create_sublevel` |
| 21 | `configure_level_streaming` |
| 22 | `set_streaming_distance` |
| 23 | `configure_level_bounds` |
| 24 | `enable_world_partition` |
| 25 | `configure_grid_size` |
| 26 | `create_data_layer` |
| 27 | `assign_actor_to_data_layer` |
| 28 | `configure_hlod_layer` |
| 29 | `create_minimap_volume` |
| 30 | `open_level_blueprint` |
| 31 | `add_level_blueprint_node` |
| 32 | `connect_level_blueprint_nodes` |
| 33 | `create_level_instance` |
| 34 | `create_packed_level_actor` |
| 35 | `get_level_structure_info` |
| 36 | `configure_world_partition` |
| 37 | `create_streaming_volume` |
| 38 | `configure_large_world_coordinates` |
| 39 | `create_world_partition_cell` |
| 40 | `configure_runtime_loading` |
| 41 | `configure_world_settings` |
| 42 | `create_pcg_graph` |
| 43 | `create_pcg_subgraph` |
| 44 | `add_pcg_node` |
| 45 | `connect_pcg_pins` |
| 46 | `set_pcg_node_settings` |
| 47 | `add_landscape_data_node` |
| 48 | `add_spline_data_node` |
| 49 | `add_volume_data_node` |
| 50 | `add_actor_data_node` |
| 51 | `add_texture_data_node` |
| 52 | `add_surface_sampler` |
| 53 | `add_mesh_sampler` |
| 54 | `add_spline_sampler` |
| 55 | `add_volume_sampler` |
| 56 | `add_bounds_modifier` |
| 57 | `add_density_filter` |
| 58 | `add_height_filter` |
| 59 | `add_slope_filter` |
| 60 | `add_distance_filter` |
| 61 | `add_bounds_filter` |
| 62 | `add_self_pruning` |
| 63 | `add_transform_points` |
| 64 | `add_project_to_surface` |
| 65 | `add_copy_points` |
| 66 | `add_merge_points` |
| 67 | `add_static_mesh_spawner` |
| 68 | `add_actor_spawner` |
| 69 | `add_spline_spawner` |
| 70 | `execute_pcg_graph` |
| 71 | `set_pcg_partition_grid_size` |
| 72 | `get_pcg_info` |
| 73 | `create_biome_rules` |
| 74 | `blend_biomes` |
| 75 | `export_pcg_to_static` |
| 76 | `import_pcg_preset` |
| 77 | `debug_pcg_execution` |
| 78 | `create_pcg_hlsl_node` |
| 79 | `enable_pcg_gpu_processing` |
| 80 | `configure_pcg_mode_brush` |
| 81 | `export_pcg_hlsl_template` |
| 82 | `batch_execute_pcg_with_gpu` |
| 83 | `get_world_partition_cells` |
| 84 | `stream_level_async` |
| 85 | `get_streaming_levels_status` |
| 86 | `configure_hlod_settings` |
| 87 | `build_hlod_for_level` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `graphPath` | unknown | No |
| `levelPath` | unknown | No |
| `levelName` | unknown | No |
| `streaming` | unknown | No |
| `shouldBeLoaded` | unknown | No |
| `shouldBeVisible` | unknown | No |
| `lightType` | unknown | No |
| `location` | object | No |
| `intensity` | unknown | No |
| `quality` | unknown | No |
| `min` | array | No |
| `max` | array | No |
| `dataLayerLabel` | unknown | No |
| `dataLayerState` | unknown | No |
| `recursive` | unknown | No |
| `exportPath` | unknown | No |
| `packagePath` | unknown | No |
| `destinationPath` | unknown | No |
| `note` | unknown | No |
| `levelPaths` | array | No |
| `subLevelPath` | unknown | No |
| `parentLevel` | unknown | No |
| `streamingMethod` | unknown | No |
| `templateLevel` | unknown | No |
| `bCreateWorldPartition` | unknown | No |
| `sublevelName` | unknown | No |
| `sublevelPath` | unknown | No |
| `bShouldBeVisible` | unknown | No |
| `bShouldBlockOnLoad` | unknown | No |
| `bDisableDistanceStreaming` | unknown | No |
| `streamingDistance` | unknown | No |
| `streamingUsage` | unknown | No |
| `createVolume` | unknown | No |
| `boundsOrigin` | object | No |
| `boundsExtent` | object | No |
| `bAutoCalculateBounds` | unknown | No |
| `bEnableWorldPartition` | unknown | No |
| `gridCellSize` | unknown | No |
| `loadingRange` | unknown | No |
| `dataLayerName` | unknown | No |
| `bIsInitiallyVisible` | unknown | No |
| `bIsInitiallyLoaded` | unknown | No |
| `dataLayerType` | unknown | No |
| `actorName` | unknown | No |
| `actorPath` | unknown | No |
| `hlodLayerName` | unknown | No |
| `hlodLayerPath` | unknown | No |
| `bIsSpatiallyLoaded` | unknown | No |
| `cellSize` | unknown | No |
| `loadingDistance` | unknown | No |
| `volumeName` | unknown | No |
| `volumeLocation` | object | No |
| `volumeExtent` | object | No |
| `nodeClass` | unknown | No |
| `nodePosition` | object | No |
| `nodeName` | unknown | No |
| `sourceNodeName` | unknown | No |
| `sourcePinName` | unknown | No |
| `targetNodeName` | unknown | No |
| `targetPinName` | unknown | No |
| `levelInstanceName` | unknown | No |
| `levelAssetPath` | unknown | No |
| `instanceLocation` | object | No |
| `instanceRotation` | object | No |
| `instanceScale` | object | No |
| `packedLevelName` | unknown | No |
| `bPackBlueprints` | unknown | No |
| `bPackStaticMeshes` | unknown | No |
| `enableLargeWorlds` | unknown | No |
| `defaultGameMode` | unknown | No |
| `killZ` | unknown | No |
| `worldGravityZ` | unknown | No |
| `runtimeCellSize` | unknown | No |
| `streamingVolumeExtent` | object | No |
| `save` | unknown | No |

---

## manage_motion_design

Motion Design (Avalanche) tools: Cloners, Effectors, Mograph.

### Actions

*10 actions available*

| # | Action |
|---|--------|
| 1 | `create_cloner` |
| 2 | `configure_cloner_pattern` |
| 3 | `add_effector` |
| 4 | `animate_effector` |
| 5 | `create_mograph_sequence` |
| 6 | `create_radial_cloner` |
| 7 | `create_spline_cloner` |
| 8 | `add_noise_effector` |
| 9 | `configure_step_effector` |
| 10 | `export_mograph_to_sequence` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `clonerName` | unknown | No |
| `clonerType` | unknown | No |
| `sourceActor` | unknown | No |
| `location` | object | No |
| `clonerActor` | unknown | No |
| `countX` | unknown | No |
| `countY` | unknown | No |
| `countZ` | unknown | No |
| `offset` | object | No |
| `rotation` | object | No |
| `scale` | object | No |
| `radius` | unknown | No |
| `count` | unknown | No |
| `axis` | unknown | No |
| `align` | unknown | No |
| `splineActor` | unknown | No |
| `effectorType` | unknown | No |
| `effectorName` | unknown | No |
| `effectorActor` | unknown | No |
| `strength` | unknown | No |
| `frequency` | unknown | No |
| `stepCount` | unknown | No |
| `operation` | unknown | No |
| `propertyName` | unknown | No |
| `startValue` | unknown | No |
| `endValue` | unknown | No |
| `duration` | unknown | No |
| `sequencePath` | unknown | No |

---

## animation_physics

Animation BPs, Montages, IK, retargeting + Chaos destruction/vehicles.

### Actions

*162 actions available*

| # | Action |
|---|--------|
| 1 | `create_animation_bp` |
| 2 | `play_montage` |
| 3 | `setup_ragdoll` |
| 4 | `activate_ragdoll` |
| 5 | `configure_vehicle` |
| 6 | `create_blend_space` |
| 7 | `create_state_machine` |
| 8 | `setup_ik` |
| 9 | `create_procedural_anim` |
| 10 | `create_blend_tree` |
| 11 | `setup_retargeting` |
| 12 | `setup_physics_simulation` |
| 13 | `cleanup` |
| 14 | `create_animation_asset` |
| 15 | `add_notify` |
| 16 | `create_animation_sequence` |
| 17 | `set_sequence_length` |
| 18 | `add_bone_track` |
| 19 | `set_bone_key` |
| 20 | `set_curve_key` |
| 21 | `add_notify_state` |
| 22 | `add_sync_marker` |
| 23 | `set_root_motion_settings` |
| 24 | `set_additive_settings` |
| 25 | `create_montage` |
| 26 | `add_montage_section` |
| 27 | `add_montage_slot` |
| 28 | `set_section_timing` |
| 29 | `add_montage_notify` |
| 30 | `set_blend_in` |
| 31 | `set_blend_out` |
| 32 | `link_sections` |
| 33 | `create_blend_space_1d` |
| 34 | `create_blend_space_2d` |
| 35 | `add_blend_sample` |
| 36 | `set_axis_settings` |
| 37 | `set_interpolation_settings` |
| 38 | `create_aim_offset` |
| 39 | `add_aim_offset_sample` |
| 40 | `create_anim_blueprint` |
| 41 | `add_state_machine` |
| 42 | `add_state` |
| 43 | `add_transition` |
| 44 | `set_transition_rules` |
| 45 | `add_blend_node` |
| 46 | `add_cached_pose` |
| 47 | `add_slot_node` |
| 48 | `add_layered_blend_per_bone` |
| 49 | `set_anim_graph_node_value` |
| 50 | `create_control_rig` |
| 51 | `add_control` |
| 52 | `add_rig_unit` |
| 53 | `connect_rig_elements` |
| 54 | `create_pose_library` |
| 55 | `create_ik_rig` |
| 56 | `add_ik_chain` |
| 57 | `add_ik_goal` |
| 58 | `create_ik_retargeter` |
| 59 | `set_retarget_chain_mapping` |
| 60 | `get_animation_info` |
| 61 | `create_pose_search_database` |
| 62 | `configure_motion_matching` |
| 63 | `add_trajectory_prediction` |
| 64 | `create_animation_modifier` |
| 65 | `setup_ml_deformer` |
| 66 | `configure_ml_deformer_training` |
| 67 | `get_motion_matching_state` |
| 68 | `set_motion_matching_goal` |
| 69 | `list_pose_search_databases` |
| 70 | `get_control_rig_controls` |
| 71 | `set_control_value` |
| 72 | `reset_control_rig` |
| 73 | `chaos_create_geometry_collection` |
| 74 | `chaos_fracture_uniform` |
| 75 | `chaos_fracture_clustered` |
| 76 | `chaos_fracture_radial` |
| 77 | `chaos_fracture_slice` |
| 78 | `chaos_fracture_brick` |
| 79 | `chaos_flatten_fracture` |
| 80 | `chaos_set_geometry_collection_materials` |
| 81 | `chaos_set_damage_thresholds` |
| 82 | `chaos_set_cluster_connection_type` |
| 83 | `chaos_set_collision_particles_fraction` |
| 84 | `chaos_set_remove_on_break` |
| 85 | `chaos_create_field_system_actor` |
| 86 | `chaos_add_transient_field` |
| 87 | `chaos_add_persistent_field` |
| 88 | `chaos_add_construction_field` |
| 89 | `chaos_add_field_radial_falloff` |
| 90 | `chaos_add_field_radial_vector` |
| 91 | `chaos_add_field_uniform_vector` |
| 92 | `chaos_add_field_noise` |
| 93 | `chaos_add_field_strain` |
| 94 | `chaos_create_anchor_field` |
| 95 | `chaos_set_dynamic_state` |
| 96 | `chaos_enable_clustering` |
| 97 | `chaos_get_geometry_collection_stats` |
| 98 | `chaos_create_geometry_collection_cache` |
| 99 | `chaos_record_geometry_collection_cache` |
| 100 | `chaos_apply_cache_to_collection` |
| 101 | `chaos_remove_geometry_collection_cache` |
| 102 | `chaos_create_wheeled_vehicle_bp` |
| 103 | `chaos_add_vehicle_wheel` |
| 104 | `chaos_remove_wheel_from_vehicle` |
| 105 | `chaos_configure_engine_setup` |
| 106 | `chaos_configure_transmission_setup` |
| 107 | `chaos_configure_steering_setup` |
| 108 | `chaos_configure_differential_setup` |
| 109 | `chaos_configure_suspension_setup` |
| 110 | `chaos_configure_brake_setup` |
| 111 | `chaos_set_vehicle_mesh` |
| 112 | `chaos_set_wheel_class` |
| 113 | `chaos_set_wheel_offset` |
| 114 | `chaos_set_wheel_radius` |
| 115 | `chaos_set_vehicle_mass` |
| 116 | `chaos_set_drag_coefficient` |
| 117 | `chaos_set_center_of_mass` |
| 118 | `chaos_create_vehicle_animation_instance` |
| 119 | `chaos_set_vehicle_animation_bp` |
| 120 | `chaos_get_vehicle_config` |
| 121 | `chaos_create_cloth_config` |
| 122 | `chaos_create_cloth_shared_sim_config` |
| 123 | `chaos_apply_cloth_to_skeletal_mesh` |
| 124 | `chaos_remove_cloth_from_skeletal_mesh` |
| 125 | `chaos_set_cloth_mass_properties` |
| 126 | `chaos_set_cloth_gravity` |
| 127 | `chaos_set_cloth_damping` |
| 128 | `chaos_set_cloth_collision_properties` |
| 129 | `chaos_set_cloth_stiffness` |
| 130 | `chaos_set_cloth_tether_stiffness` |
| 131 | `chaos_set_cloth_aerodynamics` |
| 132 | `chaos_set_cloth_anim_drive` |
| 133 | `chaos_set_cloth_long_range_attachment` |
| 134 | `chaos_get_cloth_config` |
| 135 | `chaos_get_cloth_stats` |
| 136 | `chaos_create_flesh_asset` |
| 137 | `chaos_create_flesh_component` |
| 138 | `chaos_set_flesh_simulation_properties` |
| 139 | `chaos_set_flesh_stiffness` |
| 140 | `chaos_set_flesh_damping` |
| 141 | `chaos_set_flesh_incompressibility` |
| 142 | `chaos_set_flesh_inflation` |
| 143 | `chaos_set_flesh_solver_iterations` |
| 144 | `chaos_bind_flesh_to_skeleton` |
| 145 | `chaos_set_flesh_rest_state` |
| 146 | `chaos_create_flesh_cache` |
| 147 | `chaos_record_flesh_simulation` |
| 148 | `chaos_get_flesh_asset_info` |
| 149 | `chaos_get_physics_destruction_info` |
| 150 | `chaos_list_geometry_collections` |
| 151 | `chaos_list_chaos_vehicles` |
| 152 | `chaos_get_plugin_status` |
| 153 | `create_anim_layer` |
| 154 | `stack_anim_layers` |
| 155 | `configure_squash_stretch` |
| 156 | `create_rigging_layer` |
| 157 | `configure_layer_blend_mode` |
| 158 | `create_control_rig_physics` |
| 159 | `configure_ragdoll_profile` |
| 160 | `blend_ragdoll_to_animation` |
| 161 | `get_bone_transforms` |
| 162 | `apply_pose_asset` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `name` | unknown | No |
| `actorName` | unknown | No |
| `skeletonPath` | unknown | No |
| `montagePath` | unknown | No |
| `animationPath` | unknown | No |
| `playRate` | unknown | No |
| `physicsAssetName` | unknown | No |
| `meshPath` | unknown | No |
| `vehicleName` | unknown | No |
| `vehicleType` | unknown | No |
| `savePath` | unknown | No |
| `path` | unknown | No |
| `assetPath` | unknown | No |
| `skeletalMeshPath` | unknown | No |
| `blueprintPath` | unknown | No |
| `save` | unknown | No |
| `numFrames` | unknown | No |
| `frameRate` | unknown | No |
| `boneName` | unknown | No |
| `frame` | unknown | No |
| `location` | object | No |
| `rotation` | object | No |
| `scale` | object | No |
| `curveName` | unknown | No |
| `value` | unknown | No |
| `createIfMissing` | unknown | No |
| `notifyClass` | unknown | No |
| `notifyName` | unknown | No |
| `trackIndex` | unknown | No |
| `startFrame` | unknown | No |
| `endFrame` | unknown | No |
| `markerName` | unknown | No |
| `enableRootMotion` | unknown | No |
| `rootMotionRootLock` | unknown | No |
| `forceRootLock` | unknown | No |
| `additiveAnimType` | unknown | No |
| `basePoseType` | unknown | No |
| `basePoseAnimation` | unknown | No |
| `basePoseFrame` | unknown | No |
| `slotName` | unknown | No |
| `sectionName` | unknown | No |
| `startTime` | unknown | No |
| `length` | unknown | No |
| `time` | unknown | No |
| `blendTime` | unknown | No |
| `blendOption` | unknown | No |
| `fromSection` | unknown | No |
| `toSection` | unknown | No |
| `axisName` | unknown | No |
| `axisMin` | unknown | No |
| `axisMax` | unknown | No |
| `horizontalAxisName` | unknown | No |
| `horizontalMin` | unknown | No |
| `horizontalMax` | unknown | No |
| `verticalAxisName` | unknown | No |
| `verticalMin` | unknown | No |
| `verticalMax` | unknown | No |
| `sampleValue` | unknown | No |
| `axis` | unknown | No |
| `minValue` | unknown | No |
| `maxValue` | unknown | No |
| `gridDivisions` | unknown | No |
| `interpolationType` | unknown | No |
| `targetWeightInterpolationSpeed` | unknown | No |
| `yaw` | unknown | No |
| `pitch` | unknown | No |
| `parentClass` | unknown | No |
| `stateMachineName` | unknown | No |
| `stateName` | unknown | No |
| `isEntryState` | unknown | No |
| `fromState` | unknown | No |
| `toState` | unknown | No |
| `blendLogicType` | unknown | No |
| `automaticTriggerRule` | unknown | No |
| `automaticTriggerTime` | unknown | No |
| `blendType` | unknown | No |
| `nodeName` | unknown | No |
| `x` | unknown | No |
| `y` | unknown | No |
| `cacheName` | unknown | No |
| `layerSetup` | array | No |
| `propertyName` | unknown | No |
| `controlName` | unknown | No |
| `controlType` | unknown | No |
| `parentBone` | unknown | No |
| `parentControl` | unknown | No |
| `unitType` | unknown | No |
| `unitName` | unknown | No |
| `settings` | object | No |
| `sourceElement` | unknown | No |
| `sourcePin` | unknown | No |
| `targetElement` | unknown | No |
| `targetPin` | unknown | No |
| `chainName` | unknown | No |
| `startBone` | unknown | No |
| `endBone` | unknown | No |
| `goal` | unknown | No |
| `sourceIKRigPath` | unknown | No |
| `targetIKRigPath` | unknown | No |
| `sourceChain` | unknown | No |
| `targetChain` | unknown | No |

---

## manage_effect

Niagara/Cascade particles, debug shapes, VFX graph authoring.

### Actions

*74 actions available*

| # | Action |
|---|--------|
| 1 | `particle` |
| 2 | `niagara` |
| 3 | `debug_shape` |
| 4 | `spawn_niagara` |
| 5 | `create_dynamic_light` |
| 6 | `create_niagara_system` |
| 7 | `create_niagara_emitter` |
| 8 | `create_volumetric_fog` |
| 9 | `create_particle_trail` |
| 10 | `create_environment_effect` |
| 11 | `create_impact_effect` |
| 12 | `create_niagara_ribbon` |
| 13 | `activate` |
| 14 | `activate_effect` |
| 15 | `deactivate` |
| 16 | `reset` |
| 17 | `advance_simulation` |
| 18 | `add_niagara_module` |
| 19 | `connect_niagara_pins` |
| 20 | `remove_niagara_node` |
| 21 | `set_niagara_parameter` |
| 22 | `clear_debug_shapes` |
| 23 | `cleanup` |
| 24 | `list_debug_shapes` |
| 25 | `add_emitter_to_system` |
| 26 | `set_emitter_properties` |
| 27 | `add_spawn_rate_module` |
| 28 | `add_spawn_burst_module` |
| 29 | `add_spawn_per_unit_module` |
| 30 | `add_initialize_particle_module` |
| 31 | `add_particle_state_module` |
| 32 | `add_force_module` |
| 33 | `add_velocity_module` |
| 34 | `add_acceleration_module` |
| 35 | `add_size_module` |
| 36 | `add_color_module` |
| 37 | `add_sprite_renderer_module` |
| 38 | `add_mesh_renderer_module` |
| 39 | `add_ribbon_renderer_module` |
| 40 | `add_light_renderer_module` |
| 41 | `add_collision_module` |
| 42 | `add_kill_particles_module` |
| 43 | `add_camera_offset_module` |
| 44 | `add_user_parameter` |
| 45 | `set_parameter_value` |
| 46 | `bind_parameter_to_source` |
| 47 | `add_skeletal_mesh_data_interface` |
| 48 | `add_static_mesh_data_interface` |
| 49 | `add_spline_data_interface` |
| 50 | `add_audio_spectrum_data_interface` |
| 51 | `add_collision_query_data_interface` |
| 52 | `add_event_generator` |
| 53 | `add_event_receiver` |
| 54 | `configure_event_payload` |
| 55 | `enable_gpu_simulation` |
| 56 | `add_simulation_stage` |
| 57 | `get_niagara_info` |
| 58 | `validate_niagara_system` |
| 59 | `create_niagara_module` |
| 60 | `add_niagara_script` |
| 61 | `add_data_interface` |
| 62 | `setup_niagara_fluids` |
| 63 | `create_fluid_simulation` |
| 64 | `add_chaos_integration` |
| 65 | `create_niagara_sim_cache` |
| 66 | `configure_niagara_lod` |
| 67 | `export_niagara_system` |
| 68 | `import_niagara_module` |
| 69 | `configure_niagara_determinism` |
| 70 | `create_niagara_data_interface` |
| 71 | `configure_gpu_simulation` |
| 72 | `batch_compile_niagara` |
| 73 | `get_niagara_parameters` |
| 74 | `set_niagara_variable` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `name` | unknown | No |
| `systemName` | unknown | No |
| `systemPath` | unknown | No |
| `preset` | unknown | No |
| `location` | object | No |
| `scale` | unknown | No |
| `shape` | unknown | No |
| `size` | unknown | No |
| `color` | array | No |
| `modulePath` | unknown | No |
| `emitterName` | unknown | No |
| `pinName` | unknown | No |
| `linkedTo` | unknown | No |
| `parameterName` | unknown | No |
| `parameterType` | unknown | No |
| `type` | unknown | No |
| `value` | unknown | No |
| `filter` | unknown | No |
| `fluidType` | unknown | No |
| `stage` | unknown | No |
| `className` | unknown | No |
| `path` | unknown | No |
| `assetPath` | unknown | No |
| `emitterPath` | unknown | No |
| `save` | unknown | No |
| `emitterProperties` | object | No |
| `spawnRate` | unknown | No |
| `burstCount` | unknown | No |
| `burstTime` | unknown | No |
| `burstInterval` | unknown | No |
| `spawnPerUnit` | unknown | No |
| `lifetime` | unknown | No |
| `lifetimeMin` | unknown | No |
| `lifetimeMax` | unknown | No |
| `mass` | unknown | No |
| `spriteSize` | object | No |
| `meshScale` | object | No |
| `forceType` | unknown | No |
| `forceStrength` | unknown | No |
| `forceVector` | object | No |
| `dragCoefficient` | unknown | No |
| `velocity` | object | No |
| `velocityMin` | object | No |
| `velocityMax` | object | No |
| `acceleration` | object | No |
| `velocityMode` | unknown | No |
| `sizeMode` | unknown | No |
| `uniformSize` | unknown | No |
| `sizeScale` | object | No |
| `sizeCurve` | array | No |
| `colorMin` | object | No |
| `colorMax` | object | No |
| `colorMode` | unknown | No |
| `colorCurve` | array | No |
| `materialPath` | unknown | No |
| `meshPath` | unknown | No |
| `sortMode` | unknown | No |
| `alignment` | unknown | No |
| `facingMode` | unknown | No |
| `ribbonWidth` | unknown | No |
| `ribbonTwist` | unknown | No |
| `ribbonFacingMode` | unknown | No |
| `tessellationFactor` | unknown | No |
| `lightRadius` | unknown | No |
| `lightIntensity` | unknown | No |
| `lightColor` | object | No |
| `volumetricScattering` | unknown | No |
| `lightExponent` | unknown | No |
| `affectsTranslucency` | unknown | No |
| `collisionMode` | unknown | No |
| `restitution` | unknown | No |
| `friction` | unknown | No |
| `radiusScale` | unknown | No |
| `dieOnCollision` | unknown | No |
| `killCondition` | unknown | No |
| `killBox` | object | No |
| `invertKillZone` | unknown | No |
| `cameraOffset` | unknown | No |
| `cameraOffsetMode` | unknown | No |
| `parameterValue` | unknown | No |
| `sourceBinding` | unknown | No |
| `skeletalMeshPath` | unknown | No |
| `staticMeshPath` | unknown | No |
| `useWholeSkeletonOrBones` | unknown | No |
| `specificBones` | array | No |
| `samplingMode` | unknown | No |
| `eventName` | unknown | No |
| `eventPayload` | array | No |
| `spawnOnEvent` | unknown | No |
| `eventSpawnCount` | unknown | No |
| `gpuEnabled` | unknown | No |
| `fixedBoundsEnabled` | unknown | No |
| `deterministicEnabled` | unknown | No |
| `stageName` | unknown | No |
| `stageIterationSource` | unknown | No |

---

## build_environment

Landscapes, foliage, procedural terrain, sky/fog, water, weather.

### Actions

*58 actions available*

| # | Action |
|---|--------|
| 1 | `create_landscape` |
| 2 | `sculpt` |
| 3 | `sculpt_landscape` |
| 4 | `add_foliage` |
| 5 | `paint_foliage` |
| 6 | `create_procedural_terrain` |
| 7 | `create_procedural_foliage` |
| 8 | `add_foliage_instances` |
| 9 | `get_foliage_instances` |
| 10 | `remove_foliage` |
| 11 | `paint_landscape` |
| 12 | `paint_landscape_layer` |
| 13 | `modify_heightmap` |
| 14 | `set_landscape_material` |
| 15 | `create_landscape_grass_type` |
| 16 | `generate_lods` |
| 17 | `bake_lightmap` |
| 18 | `export_snapshot` |
| 19 | `import_snapshot` |
| 20 | `delete` |
| 21 | `create_sky_sphere` |
| 22 | `create_sky_atmosphere` |
| 23 | `configure_sky_atmosphere` |
| 24 | `create_fog_volume` |
| 25 | `create_exponential_height_fog` |
| 26 | `configure_exponential_height_fog` |
| 27 | `create_volumetric_cloud` |
| 28 | `configure_volumetric_cloud` |
| 29 | `set_time_of_day` |
| 30 | `create_water_body_ocean` |
| 31 | `create_water_body_lake` |
| 32 | `create_water_body_river` |
| 33 | `configure_water_body` |
| 34 | `configure_water_waves` |
| 35 | `get_water_body_info` |
| 36 | `list_water_bodies` |
| 37 | `set_river_depth` |
| 38 | `set_ocean_extent` |
| 39 | `set_water_static_mesh` |
| 40 | `set_river_transitions` |
| 41 | `set_water_zone` |
| 42 | `get_water_surface_info` |
| 43 | `get_wave_info` |
| 44 | `configure_wind` |
| 45 | `create_weather_system` |
| 46 | `configure_rain_particles` |
| 47 | `configure_snow_particles` |
| 48 | `configure_lightning` |
| 49 | `configure_weather_preset` |
| 50 | `query_water_bodies` |
| 51 | `configure_ocean_waves` |
| 52 | `create_landscape_spline` |
| 53 | `configure_foliage_density` |
| 54 | `batch_paint_foliage` |
| 55 | `configure_sky_atmosphere` |
| 56 | `create_volumetric_cloud` |
| 57 | `configure_wind_directional` |
| 58 | `get_terrain_height_at` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `name` | unknown | No |
| `landscapeName` | unknown | No |
| `heightData` | array | No |
| `minX` | unknown | No |
| `minY` | unknown | No |
| `maxX` | unknown | No |
| `maxY` | unknown | No |
| `updateNormals` | unknown | No |
| `location` | object | No |
| `rotation` | object | No |
| `scale` | object | No |
| `sizeX` | unknown | No |
| `sizeY` | unknown | No |
| `sectionSize` | unknown | No |
| `sectionsPerComponent` | unknown | No |
| `componentCount` | object | No |
| `materialPath` | unknown | No |
| `tool` | unknown | No |
| `radius` | unknown | No |
| `strength` | unknown | No |
| `falloff` | unknown | No |
| `brushSize` | unknown | No |
| `layerName` | unknown | No |
| `eraseMode` | unknown | No |
| `foliageType` | unknown | No |
| `foliageTypePath` | unknown | No |
| `meshPath` | unknown | No |
| `density` | unknown | No |
| `minScale` | unknown | No |
| `maxScale` | unknown | No |
| `cullDistance` | unknown | No |
| `alignToNormal` | unknown | No |
| `randomYaw` | unknown | No |
| `locations` | array | No |
| `transforms` | array | No |
| `position` | object | No |
| `bounds` | object | No |
| `volumeName` | unknown | No |
| `seed` | unknown | No |
| `foliageTypes` | array | No |
| `path` | unknown | No |
| `filename` | unknown | No |
| `assetPaths` | array | No |
| `bottomRadius` | unknown | No |
| `atmosphereHeight` | unknown | No |
| `mieAnisotropy` | unknown | No |
| `mieScatteringScale` | unknown | No |
| `rayleighScatteringScale` | unknown | No |
| `multiScatteringFactor` | unknown | No |
| `rayleighExponentialDistribution` | unknown | No |
| `mieExponentialDistribution` | unknown | No |
| `mieAbsorptionScale` | unknown | No |
| `otherAbsorptionScale` | unknown | No |
| `heightFogContribution` | unknown | No |
| `aerialPerspectiveViewDistanceScale` | unknown | No |
| `transmittanceMinLightElevationAngle` | unknown | No |
| `aerialPerspectiveStartDepth` | unknown | No |
| `rayleighScattering` | object | No |
| `mieScattering` | object | No |
| `mieAbsorption` | object | No |
| `skyLuminanceFactor` | object | No |
| `fogDensity` | unknown | No |
| `fogHeightFalloff` | unknown | No |
| `fogMaxOpacity` | unknown | No |
| `startDistance` | unknown | No |
| `endDistance` | unknown | No |
| `fogCutoffDistance` | unknown | No |
| `volumetricFog` | unknown | No |
| `volumetricFogScatteringDistribution` | unknown | No |
| `volumetricFogExtinctionScale` | unknown | No |
| `volumetricFogDistance` | unknown | No |
| `volumetricFogStartDistance` | unknown | No |
| `volumetricFogNearFadeInDistance` | unknown | No |
| `fogInscatteringColor` | object | No |
| `directionalInscatteringColor` | object | No |
| `volumetricFogAlbedo` | object | No |
| `volumetricFogEmissive` | object | No |
| `directionalInscatteringExponent` | unknown | No |
| `directionalInscatteringStartDistance` | unknown | No |
| `secondFogDensity` | unknown | No |
| `secondFogHeightFalloff` | unknown | No |
| `secondFogHeightOffset` | unknown | No |
| `inscatteringColorCubemapAngle` | unknown | No |
| `fullyDirectionalInscatteringColorDistance` | unknown | No |
| `nonDirectionalInscatteringColorDistance` | unknown | No |
| `inscatteringTextureTint` | object | No |
| `layerBottomAltitude` | unknown | No |
| `layerHeight` | unknown | No |
| `tracingStartMaxDistance` | unknown | No |
| `tracingStartDistanceFromCamera` | unknown | No |
| `tracingMaxDistance` | unknown | No |
| `planetRadius` | unknown | No |
| `groundAlbedo` | object | No |
| `usePerSampleAtmosphericLightTransmittance` | unknown | No |
| `skyLightCloudBottomOcclusion` | unknown | No |
| `viewSampleCountScale` | unknown | No |
| `reflectionViewSampleCountScale` | unknown | No |
| `shadowViewSampleCountScale` | unknown | No |
| `shadowReflectionViewSampleCountScale` | unknown | No |
| `shadowTracingDistance` | unknown | No |
| `stopTracingTransmittanceThreshold` | unknown | No |
| `holdout` | unknown | No |
| `renderInMainPass` | unknown | No |
| `visibleInRealTimeSkyCaptures` | unknown | No |
| `time` | unknown | No |
| `hour` | unknown | No |

---

## manage_sequence

Sequencer cinematics, Level Sequences, keyframes, MRQ renders.

### Actions

*100 actions available*

| # | Action |
|---|--------|
| 1 | `create` |
| 2 | `open` |
| 3 | `duplicate` |
| 4 | `rename` |
| 5 | `delete` |
| 6 | `list` |
| 7 | `get_metadata` |
| 8 | `set_metadata` |
| 9 | `create_master_sequence` |
| 10 | `add_subsequence` |
| 11 | `remove_subsequence` |
| 12 | `get_subsequences` |
| 13 | `export_sequence` |
| 14 | `add_actor` |
| 15 | `add_actors` |
| 16 | `remove_actors` |
| 17 | `bind_actor` |
| 18 | `unbind_actor` |
| 19 | `get_bindings` |
| 20 | `add_spawnable_from_class` |
| 21 | `add_track` |
| 22 | `remove_track` |
| 23 | `list_tracks` |
| 24 | `list_track_types` |
| 25 | `add_section` |
| 26 | `remove_section` |
| 27 | `get_tracks` |
| 28 | `set_track_muted` |
| 29 | `set_track_solo` |
| 30 | `set_track_locked` |
| 31 | `add_shot_track` |
| 32 | `add_shot` |
| 33 | `remove_shot` |
| 34 | `get_shots` |
| 35 | `add_camera` |
| 36 | `create_cine_camera_actor` |
| 37 | `configure_camera_settings` |
| 38 | `add_camera_cut_track` |
| 39 | `add_camera_cut` |
| 40 | `add_keyframe` |
| 41 | `remove_keyframe` |
| 42 | `get_keyframes` |
| 43 | `get_properties` |
| 44 | `set_properties` |
| 45 | `set_display_rate` |
| 46 | `set_tick_resolution` |
| 47 | `set_work_range` |
| 48 | `set_view_range` |
| 49 | `set_playback_range` |
| 50 | `get_playback_range` |
| 51 | `get_sequence_info` |
| 52 | `play` |
| 53 | `pause` |
| 54 | `stop` |
| 55 | `set_playback_speed` |
| 56 | `play_sequence` |
| 57 | `pause_sequence` |
| 58 | `stop_sequence` |
| 59 | `scrub_to_time` |
| 60 | `create_queue` |
| 61 | `add_job` |
| 62 | `remove_job` |
| 63 | `clear_queue` |
| 64 | `get_queue` |
| 65 | `configure_job` |
| 66 | `set_sequence` |
| 67 | `set_map` |
| 68 | `configure_output` |
| 69 | `set_resolution` |
| 70 | `set_frame_rate` |
| 71 | `set_output_directory` |
| 72 | `set_file_name_format` |
| 73 | `add_render_pass` |
| 74 | `remove_render_pass` |
| 75 | `get_render_passes` |
| 76 | `configure_render_pass` |
| 77 | `configure_anti_aliasing` |
| 78 | `set_spatial_sample_count` |
| 79 | `set_temporal_sample_count` |
| 80 | `add_burn_in` |
| 81 | `remove_burn_in` |
| 82 | `configure_burn_in` |
| 83 | `start_render` |
| 84 | `stop_render` |
| 85 | `get_render_status` |
| 86 | `get_render_progress` |
| 87 | `add_console_variable` |
| 88 | `remove_console_variable` |
| 89 | `configure_high_res_settings` |
| 90 | `set_tile_count` |
| 91 | `create_media_track` |
| 92 | `configure_sequence_streaming` |
| 93 | `create_event_trigger_track` |
| 94 | `add_procedural_camera_shake` |
| 95 | `configure_sequence_lod` |
| 96 | `create_camera_cut_track` |
| 97 | `configure_mrq_settings` |
| 98 | `batch_render_sequences` |
| 99 | `get_sequence_bindings` |
| 100 | `configure_audio_track` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `name` | unknown | No |
| `path` | unknown | No |
| `sequencePath` | unknown | No |
| `sequenceName` | unknown | No |
| `savePath` | unknown | No |
| `assetPath` | unknown | No |
| `destinationPath` | unknown | No |
| `newName` | unknown | No |
| `actorName` | unknown | No |
| `actorNames` | array | No |
| `bindingName` | unknown | No |
| `bindingId` | unknown | No |
| `spawnable` | unknown | No |
| `className` | unknown | No |
| `trackType` | unknown | No |
| `trackName` | unknown | No |
| `propertyPath` | unknown | No |
| `muted` | unknown | No |
| `solo` | unknown | No |
| `locked` | unknown | No |
| `subsequencePath` | unknown | No |
| `shotName` | unknown | No |
| `shotNumber` | unknown | No |
| `cameraActorName` | unknown | No |
| `filmbackPreset` | unknown | No |
| `sensorWidth` | unknown | No |
| `sensorHeight` | unknown | No |
| `focalLength` | unknown | No |
| `aperture` | unknown | No |
| `focusDistance` | unknown | No |
| `autoFocus` | unknown | No |
| `focusMethod` | unknown | No |
| `focusTarget` | unknown | No |
| `frame` | unknown | No |
| `time` | unknown | No |
| `value` | unknown | No |
| `property` | unknown | No |
| `interpolation` | unknown | No |
| `startFrame` | unknown | No |
| `endFrame` | unknown | No |
| `startTime` | unknown | No |
| `endTime` | unknown | No |
| `start` | unknown | No |
| `end` | unknown | No |
| `displayRate` | unknown | No |
| `tickResolution` | unknown | No |
| `lengthInFrames` | unknown | No |
| `playbackStart` | unknown | No |
| `playbackEnd` | unknown | No |
| `speed` | unknown | No |
| `loopMode` | unknown | No |
| `jobName` | unknown | No |
| `jobIndex` | unknown | No |
| `mapPath` | unknown | No |
| `outputDirectory` | unknown | No |
| `fileNameFormat` | unknown | No |
| `outputFormat` | unknown | No |
| `resolutionX` | unknown | No |
| `resolutionY` | unknown | No |
| `frameRate` | unknown | No |
| `resolution` | unknown | No |
| `passType` | unknown | No |
| `passName` | unknown | No |
| `passEnabled` | unknown | No |
| `spatialSampleCount` | unknown | No |
| `temporalSampleCount` | unknown | No |
| `overrideAntiAliasing` | unknown | No |
| `antiAliasingMethod` | unknown | No |
| `burnInClass` | unknown | No |
| `burnInText` | unknown | No |
| `burnInPosition` | unknown | No |
| `cvarName` | unknown | No |
| `cvarValue` | unknown | No |
| `tileCountX` | unknown | No |
| `tileCountY` | unknown | No |
| `overlapRatio` | unknown | No |
| `location` | object | No |
| `rotation` | object | No |
| `overwrite` | unknown | No |
| `metadata` | object | No |
| `exportPath` | unknown | No |
| `exportFormat` | unknown | No |
| `save` | unknown | No |
| `mediaPath` | unknown | No |
| `mediaSourceName` | unknown | No |
| `streamingSettings` | object | No |
| `eventTriggerName` | unknown | No |
| `eventPayload` | object | No |
| `shakeIntensity` | unknown | No |
| `shakeFrequency` | unknown | No |
| `shakeBlendIn` | unknown | No |
| `shakeBlendOut` | unknown | No |
| `shakeDuration` | unknown | No |
| `lodBias` | unknown | No |
| `forceLod` | unknown | No |
| `cameraCutName` | unknown | No |
| `targetCamera` | unknown | No |
| `blendTime` | unknown | No |
| `mrqPreset` | unknown | No |
| `mrqSettings` | object | No |
| `sequencePaths` | array | No |
| `outputSettings` | object | No |
| `parallelRenders` | unknown | No |
| `audioTrackName` | unknown | No |
| `audioAssetPath` | unknown | No |
| `audioStartOffset` | unknown | No |
| `audioVolumeMultiplier` | unknown | No |

---

## manage_audio

Audio playback, mixes, MetaSounds + Wwise/FMOD/Bink middleware.

### Actions

*134 actions available*

| # | Action |
|---|--------|
| 1 | `create_sound_cue` |
| 2 | `play_sound_at_location` |
| 3 | `play_sound_2d` |
| 4 | `create_audio_component` |
| 5 | `create_sound_mix` |
| 6 | `push_sound_mix` |
| 7 | `pop_sound_mix` |
| 8 | `set_sound_mix_class_override` |
| 9 | `clear_sound_mix_class_override` |
| 10 | `set_base_sound_mix` |
| 11 | `prime_sound` |
| 12 | `play_sound_attached` |
| 13 | `spawn_sound_at_location` |
| 14 | `fade_sound_in` |
| 15 | `fade_sound_out` |
| 16 | `create_ambient_sound` |
| 17 | `create_sound_class` |
| 18 | `set_sound_attenuation` |
| 19 | `create_reverb_zone` |
| 20 | `enable_audio_analysis` |
| 21 | `fade_sound` |
| 22 | `set_doppler_effect` |
| 23 | `set_audio_occlusion` |
| 24 | `add_cue_node` |
| 25 | `connect_cue_nodes` |
| 26 | `set_cue_attenuation` |
| 27 | `set_cue_concurrency` |
| 28 | `create_metasound` |
| 29 | `add_metasound_node` |
| 30 | `connect_metasound_nodes` |
| 31 | `add_metasound_input` |
| 32 | `add_metasound_output` |
| 33 | `set_metasound_default` |
| 34 | `set_class_properties` |
| 35 | `set_class_parent` |
| 36 | `add_mix_modifier` |
| 37 | `configure_mix_eq` |
| 38 | `create_attenuation_settings` |
| 39 | `configure_distance_attenuation` |
| 40 | `configure_spatialization` |
| 41 | `configure_occlusion` |
| 42 | `configure_reverb_send` |
| 43 | `create_dialogue_voice` |
| 44 | `create_dialogue_wave` |
| 45 | `set_dialogue_context` |
| 46 | `create_reverb_effect` |
| 47 | `create_source_effect_chain` |
| 48 | `add_source_effect` |
| 49 | `create_submix_effect` |
| 50 | `get_audio_info` |
| 51 | `mw_connect_wwise_project` |
| 52 | `mw_post_wwise_event` |
| 53 | `mw_post_wwise_event_at_location` |
| 54 | `mw_stop_wwise_event` |
| 55 | `mw_set_rtpc_value` |
| 56 | `mw_set_rtpc_value_on_actor` |
| 57 | `mw_get_rtpc_value` |
| 58 | `mw_set_wwise_switch` |
| 59 | `mw_set_wwise_switch_on_actor` |
| 60 | `mw_set_wwise_state` |
| 61 | `mw_load_wwise_bank` |
| 62 | `mw_unload_wwise_bank` |
| 63 | `mw_get_loaded_banks` |
| 64 | `mw_create_wwise_component` |
| 65 | `mw_configure_wwise_component` |
| 66 | `mw_configure_spatial_audio` |
| 67 | `mw_configure_room` |
| 68 | `mw_configure_portal` |
| 69 | `mw_set_listener_position` |
| 70 | `mw_get_wwise_event_duration` |
| 71 | `mw_create_wwise_trigger` |
| 72 | `mw_set_wwise_game_object` |
| 73 | `mw_unset_wwise_game_object` |
| 74 | `mw_post_wwise_trigger` |
| 75 | `mw_set_aux_send` |
| 76 | `mw_configure_wwise_occlusion` |
| 77 | `mw_set_wwise_project_path` |
| 78 | `mw_get_wwise_status` |
| 79 | `mw_configure_wwise_init` |
| 80 | `mw_restart_wwise_engine` |
| 81 | `mw_connect_fmod_project` |
| 82 | `mw_play_fmod_event` |
| 83 | `mw_play_fmod_event_at_location` |
| 84 | `mw_stop_fmod_event` |
| 85 | `mw_set_fmod_parameter` |
| 86 | `mw_set_fmod_global_parameter` |
| 87 | `mw_get_fmod_parameter` |
| 88 | `mw_load_fmod_bank` |
| 89 | `mw_unload_fmod_bank` |
| 90 | `mw_get_fmod_loaded_banks` |
| 91 | `mw_create_fmod_component` |
| 92 | `mw_configure_fmod_component` |
| 93 | `mw_set_fmod_bus_volume` |
| 94 | `mw_set_fmod_bus_paused` |
| 95 | `mw_set_fmod_bus_mute` |
| 96 | `mw_set_fmod_vca_volume` |
| 97 | `mw_apply_fmod_snapshot` |
| 98 | `mw_release_fmod_snapshot` |
| 99 | `mw_set_fmod_listener_attributes` |
| 100 | `mw_get_fmod_event_info` |
| 101 | `mw_configure_fmod_occlusion` |
| 102 | `mw_configure_fmod_attenuation` |
| 103 | `mw_set_fmod_studio_path` |
| 104 | `mw_get_fmod_status` |
| 105 | `mw_configure_fmod_init` |
| 106 | `mw_restart_fmod_engine` |
| 107 | `mw_set_fmod_3d_attributes` |
| 108 | `mw_get_fmod_memory_usage` |
| 109 | `mw_pause_all_fmod_events` |
| 110 | `mw_resume_all_fmod_events` |
| 111 | `mw_create_bink_media_player` |
| 112 | `mw_open_bink_video` |
| 113 | `mw_play_bink` |
| 114 | `mw_pause_bink` |
| 115 | `mw_stop_bink` |
| 116 | `mw_seek_bink` |
| 117 | `mw_set_bink_looping` |
| 118 | `mw_set_bink_rate` |
| 119 | `mw_set_bink_volume` |
| 120 | `mw_get_bink_duration` |
| 121 | `mw_get_bink_time` |
| 122 | `mw_get_bink_status` |
| 123 | `mw_create_bink_texture` |
| 124 | `mw_configure_bink_texture` |
| 125 | `mw_set_bink_texture_player` |
| 126 | `mw_draw_bink_to_texture` |
| 127 | `mw_configure_bink_buffer_mode` |
| 128 | `mw_configure_bink_sound_track` |
| 129 | `mw_configure_bink_draw_style` |
| 130 | `mw_get_bink_dimensions` |
| 131 | `mw_get_audio_middleware_info` |
| 132 | `list_metasound_assets` |
| 133 | `get_metasound_inputs` |
| 134 | `trigger_metasound` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `name` | unknown | No |
| `soundPath` | unknown | No |
| `location` | object | No |
| `rotation` | object | No |
| `volume` | unknown | No |
| `pitch` | unknown | No |
| `startTime` | unknown | No |
| `attenuationPath` | unknown | No |
| `concurrencyPath` | unknown | No |
| `mixName` | unknown | No |
| `soundClassName` | unknown | No |
| `fadeInTime` | unknown | No |
| `fadeOutTime` | unknown | No |
| `fadeTime` | unknown | No |
| `targetVolume` | unknown | No |
| `attachPointName` | unknown | No |
| `actorName` | unknown | No |
| `componentName` | unknown | No |
| `parentClass` | unknown | No |
| `properties` | object | No |
| `innerRadius` | unknown | No |
| `falloffDistance` | unknown | No |
| `attenuationShape` | unknown | No |
| `falloffMode` | unknown | No |
| `reverbEffect` | unknown | No |
| `size` | object | No |
| `fftSize` | unknown | No |
| `outputType` | unknown | No |
| `soundName` | unknown | No |
| `fadeType` | unknown | No |
| `scale` | unknown | No |
| `lowPassFilterFrequency` | unknown | No |
| `volumeAttenuation` | unknown | No |
| `enabled` | unknown | No |
| `path` | unknown | No |
| `assetPath` | unknown | No |
| `save` | unknown | No |
| `wavePath` | unknown | No |
| `nodeType` | unknown | No |
| `nodeId` | unknown | No |
| `sourceNodeId` | unknown | No |
| `targetNodeId` | unknown | No |
| `outputPin` | unknown | No |
| `inputPin` | unknown | No |
| `looping` | unknown | No |
| `x` | unknown | No |
| `y` | unknown | No |
| `metasoundType` | unknown | No |
| `inputName` | unknown | No |
| `inputType` | unknown | No |
| `outputName` | unknown | No |
| `sourceNode` | unknown | No |
| `sourcePin` | unknown | No |
| `targetNode` | unknown | No |
| `targetPin` | unknown | No |
| `defaultValue` | unknown | No |
| `metasoundNodeType` | unknown | No |
| `soundClassPath` | unknown | No |
| `parentClassPath` | unknown | No |

---

## manage_lighting

Lights (point, spot, rect, sky), GI, shadows, volumetric fog.

### Actions

*61 actions available*

| # | Action |
|---|--------|
| 1 | `spawn_light` |
| 2 | `create_light` |
| 3 | `spawn_sky_light` |
| 4 | `create_sky_light` |
| 5 | `ensure_single_sky_light` |
| 6 | `create_lightmass_volume` |
| 7 | `create_lighting_enabled_level` |
| 8 | `create_dynamic_light` |
| 9 | `setup_global_illumination` |
| 10 | `configure_shadows` |
| 11 | `set_exposure` |
| 12 | `set_ambient_occlusion` |
| 13 | `setup_volumetric_fog` |
| 14 | `build_lighting` |
| 15 | `list_light_types` |
| 16 | `create_post_process_volume` |
| 17 | `configure_pp_blend` |
| 18 | `configure_pp_priority` |
| 19 | `get_post_process_settings` |
| 20 | `configure_bloom` |
| 21 | `configure_dof` |
| 22 | `configure_motion_blur` |
| 23 | `configure_color_grading` |
| 24 | `configure_white_balance` |
| 25 | `configure_vignette` |
| 26 | `configure_chromatic_aberration` |
| 27 | `configure_film_grain` |
| 28 | `configure_lens_flares` |
| 29 | `create_sphere_reflection_capture` |
| 30 | `create_box_reflection_capture` |
| 31 | `create_planar_reflection` |
| 32 | `recapture_scene` |
| 33 | `configure_ray_traced_shadows` |
| 34 | `configure_ray_traced_gi` |
| 35 | `configure_ray_traced_reflections` |
| 36 | `configure_ray_traced_ao` |
| 37 | `configure_path_tracing` |
| 38 | `create_scene_capture_2d` |
| 39 | `create_scene_capture_cube` |
| 40 | `capture_scene` |
| 41 | `set_light_channel` |
| 42 | `set_actor_light_channel` |
| 43 | `configure_lightmass_settings` |
| 44 | `build_lighting_quality` |
| 45 | `configure_indirect_lighting_cache` |
| 46 | `configure_volumetric_lightmap` |
| 47 | `configure_lumen_gi` |
| 48 | `set_lumen_reflections` |
| 49 | `tune_lumen_performance` |
| 50 | `create_lumen_volume` |
| 51 | `set_virtual_shadow_maps` |
| 52 | `configure_megalights_scene` |
| 53 | `get_megalights_budget` |
| 54 | `optimize_lights_for_megalights` |
| 55 | `configure_gi_settings` |
| 56 | `bake_lighting_preview` |
| 57 | `get_light_complexity` |
| 58 | `configure_volumetric_fog` |
| 59 | `create_light_batch` |
| 60 | `configure_shadow_settings` |
| 61 | `validate_lighting_setup` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `name` | unknown | No |
| `location` | object | No |
| `rotation` | object | No |
| `lightType` | unknown | No |
| `intensity` | unknown | No |
| `color` | array | No |
| `castShadows` | unknown | No |
| `useAsAtmosphereSunLight` | unknown | No |
| `temperature` | unknown | No |
| `radius` | unknown | No |
| `falloffExponent` | unknown | No |
| `innerCone` | unknown | No |
| `outerCone` | unknown | No |
| `width` | unknown | No |
| `height` | unknown | No |
| `sourceType` | unknown | No |
| `cubemapPath` | unknown | No |
| `recapture` | unknown | No |
| `method` | unknown | No |
| `quality` | unknown | No |
| `indirectLightingIntensity` | unknown | No |
| `bounces` | unknown | No |
| `shadowQuality` | unknown | No |
| `cascadedShadows` | unknown | No |
| `shadowDistance` | unknown | No |
| `contactShadows` | unknown | No |
| `rayTracedShadows` | unknown | No |
| `compensationValue` | unknown | No |
| `minBrightness` | unknown | No |
| `maxBrightness` | unknown | No |
| `enabled` | unknown | No |
| `density` | unknown | No |
| `scatteringIntensity` | unknown | No |
| `fogHeight` | unknown | No |
| `buildOnlySelected` | unknown | No |
| `buildReflectionCaptures` | unknown | No |
| `levelName` | unknown | No |
| `copyActors` | unknown | No |
| `useTemplate` | unknown | No |
| `size` | object | No |
| `volumeName` | unknown | No |
| `actorName` | unknown | No |
| `infinite` | unknown | No |
| `blendRadius` | unknown | No |
| `blendWeight` | unknown | No |
| `priority` | unknown | No |
| `extent` | object | No |
| `bloomIntensity` | unknown | No |
| `bloomThreshold` | unknown | No |
| `focalDistance` | unknown | No |
| `focalRegion` | unknown | No |
| `motionBlurAmount` | unknown | No |
| `globalSaturation` | object | No |
| `whiteTemp` | unknown | No |
| `vignetteIntensity` | unknown | No |
| `chromaticAberrationIntensity` | unknown | No |
| `filmGrainIntensity` | unknown | No |
| `influenceRadius` | unknown | No |
| `boxExtent` | object | No |
| `captureOffset` | object | No |
| `brightness` | unknown | No |
| `screenPercentage` | unknown | No |
| `rayTracedShadowsEnabled` | unknown | No |
| `rayTracedGIEnabled` | unknown | No |
| `rayTracedReflectionsEnabled` | unknown | No |
| `rayTracedAOEnabled` | unknown | No |
| `pathTracingEnabled` | unknown | No |
| `fov` | unknown | No |
| `captureResolution` | unknown | No |
| `captureSource` | unknown | No |
| `textureTargetPath` | unknown | No |
| `savePath` | unknown | No |
| `lightActorName` | unknown | No |
| `channel0` | unknown | No |
| `channel1` | unknown | No |
| `channel2` | unknown | No |
| `numIndirectBounces` | unknown | No |
| `indirectLightingQuality` | unknown | No |
| `volumetricLightmapEnabled` | unknown | No |
| `lumenQuality` | unknown | No |
| `lumenDetailTrace` | unknown | No |
| `lumenReflectionQuality` | unknown | No |
| `lumenUpdateSpeed` | unknown | No |
| `lumenFinalGatherQuality` | unknown | No |
| `virtualShadowMapResolution` | unknown | No |
| `virtualShadowMapQuality` | unknown | No |
| `megalightsEnabled` | unknown | No |
| `megalightsBudget` | unknown | No |
| `megalightsQuality` | unknown | No |
| `giMethod` | unknown | No |
| `lightQuality` | unknown | No |
| `previewBake` | unknown | No |
| `fogInscatteringColor` | array | No |
| `fogExtinctionScale` | unknown | No |
| `fogViewDistance` | unknown | No |
| `fogStartDistance` | unknown | No |
| `lights` | array | No |
| `shadowBias` | unknown | No |
| `shadowSlopeBias` | unknown | No |
| `shadowResolution` | unknown | No |
| `dynamicShadowCascades` | unknown | No |
| `insetShadows` | unknown | No |
| `validatePerformance` | unknown | No |
| `validateOverlap` | unknown | No |
| `validateShadows` | unknown | No |

---

## manage_performance

Profiling, benchmarks, scalability, LOD, Nanite optimization.

### Actions

*20 actions available*

| # | Action |
|---|--------|
| 1 | `start_profiling` |
| 2 | `stop_profiling` |
| 3 | `run_benchmark` |
| 4 | `show_fps` |
| 5 | `show_stats` |
| 6 | `generate_memory_report` |
| 7 | `set_scalability` |
| 8 | `set_resolution_scale` |
| 9 | `set_vsync` |
| 10 | `set_frame_rate_limit` |
| 11 | `enable_gpu_timing` |
| 12 | `configure_texture_streaming` |
| 13 | `configure_lod` |
| 14 | `apply_baseline_settings` |
| 15 | `optimize_draw_calls` |
| 16 | `merge_actors` |
| 17 | `configure_occlusion_culling` |
| 18 | `optimize_shaders` |
| 19 | `configure_nanite` |
| 20 | `configure_world_partition` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `type` | unknown | No |
| `duration` | unknown | No |
| `outputPath` | unknown | No |
| `detailed` | unknown | No |
| `level` | unknown | No |
| `scale` | unknown | No |
| `enabled` | unknown | No |
| `maxFPS` | unknown | No |
| `verbose` | unknown | No |
| `poolSize` | unknown | No |
| `boostPlayerLocation` | unknown | No |
| `forceLOD` | unknown | No |
| `lodBias` | unknown | No |
| `distanceScale` | unknown | No |
| `skeletalBias` | unknown | No |
| `hzb` | unknown | No |
| `enableInstancing` | unknown | No |
| `enableBatching` | unknown | No |
| `mergeActors` | unknown | No |
| `actors` | array | No |
| `freezeRendering` | unknown | No |
| `compileOnDemand` | unknown | No |
| `cacheShaders` | unknown | No |
| `reducePermutations` | unknown | No |
| `maxPixelsPerEdge` | unknown | No |
| `streamingPoolSize` | unknown | No |
| `streamingDistance` | unknown | No |
| `cellSize` | unknown | No |

---

## manage_geometry

Procedural meshes via Geometry Script: booleans, UVs, collision.

### Actions

*80 actions available*

| # | Action |
|---|--------|
| 1 | `create_box` |
| 2 | `create_sphere` |
| 3 | `create_cylinder` |
| 4 | `create_cone` |
| 5 | `create_capsule` |
| 6 | `create_torus` |
| 7 | `create_plane` |
| 8 | `create_disc` |
| 9 | `create_stairs` |
| 10 | `create_spiral_stairs` |
| 11 | `create_ring` |
| 12 | `create_arch` |
| 13 | `create_pipe` |
| 14 | `create_ramp` |
| 15 | `boolean_union` |
| 16 | `boolean_subtract` |
| 17 | `boolean_intersection` |
| 18 | `boolean_trim` |
| 19 | `self_union` |
| 20 | `extrude` |
| 21 | `inset` |
| 22 | `outset` |
| 23 | `bevel` |
| 24 | `offset_faces` |
| 25 | `shell` |
| 26 | `revolve` |
| 27 | `chamfer` |
| 28 | `extrude_along_spline` |
| 29 | `bridge` |
| 30 | `loft` |
| 31 | `sweep` |
| 32 | `duplicate_along_spline` |
| 33 | `loop_cut` |
| 34 | `edge_split` |
| 35 | `quadrangulate` |
| 36 | `bend` |
| 37 | `twist` |
| 38 | `taper` |
| 39 | `noise_deform` |
| 40 | `smooth` |
| 41 | `relax` |
| 42 | `stretch` |
| 43 | `spherify` |
| 44 | `cylindrify` |
| 45 | `triangulate` |
| 46 | `poke` |
| 47 | `mirror` |
| 48 | `array_linear` |
| 49 | `array_radial` |
| 50 | `simplify_mesh` |
| 51 | `subdivide` |
| 52 | `remesh_uniform` |
| 53 | `merge_vertices` |
| 54 | `remesh_voxel` |
| 55 | `weld_vertices` |
| 56 | `fill_holes` |
| 57 | `remove_degenerates` |
| 58 | `auto_uv` |
| 59 | `project_uv` |
| 60 | `transform_uvs` |
| 61 | `unwrap_uv` |
| 62 | `pack_uv_islands` |
| 63 | `recalculate_normals` |
| 64 | `flip_normals` |
| 65 | `recompute_tangents` |
| 66 | `generate_collision` |
| 67 | `generate_complex_collision` |
| 68 | `simplify_collision` |
| 69 | `generate_lods` |
| 70 | `set_lod_settings` |
| 71 | `set_lod_screen_sizes` |
| 72 | `convert_to_nanite` |
| 73 | `convert_to_static_mesh` |
| 74 | `get_mesh_info` |
| 75 | `create_procedural_box` |
| 76 | `boolean_mesh_operation` |
| 77 | `generate_mesh_uvs` |
| 78 | `create_mesh_from_spline` |
| 79 | `configure_nanite_settings` |
| 80 | `export_geometry_to_file` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `meshPath` | unknown | No |
| `targetMeshPath` | unknown | No |
| `outputPath` | unknown | No |
| `actorName` | unknown | No |
| `width` | unknown | No |
| `height` | unknown | No |
| `depth` | unknown | No |
| `radius` | unknown | No |
| `innerRadius` | unknown | No |
| `numSides` | unknown | No |
| `numRings` | unknown | No |
| `numSteps` | unknown | No |
| `stepWidth` | unknown | No |
| `stepHeight` | unknown | No |
| `stepDepth` | unknown | No |
| `numTurns` | unknown | No |
| `widthSegments` | unknown | No |
| `heightSegments` | unknown | No |
| `depthSegments` | unknown | No |
| `radialSegments` | unknown | No |
| `location` | object | No |
| `rotation` | object | No |
| `scale` | object | No |
| `distance` | unknown | No |
| `amount` | unknown | No |
| `segments` | unknown | No |
| `angle` | unknown | No |
| `axis` | unknown | No |
| `strength` | unknown | No |
| `iterations` | unknown | No |
| `targetTriangleCount` | unknown | No |
| `targetEdgeLength` | unknown | No |
| `weldDistance` | unknown | No |
| `faceIndices` | array | No |
| `edgeIndices` | array | No |
| `vertexIndices` | array | No |
| `selectionBox` | object | No |
| `uvChannel` | unknown | No |
| `uvScale` | object | No |
| `uvOffset` | object | No |
| `projectionDirection` | unknown | No |
| `hardEdgeAngle` | unknown | No |
| `computeWeightedNormals` | unknown | No |
| `smoothingGroupId` | unknown | No |
| `collisionType` | unknown | No |
| `hullCount` | unknown | No |
| `hullPrecision` | unknown | No |
| `maxVerticesPerHull` | unknown | No |
| `lodCount` | unknown | No |
| `lodIndex` | unknown | No |
| `reductionPercent` | unknown | No |
| `screenSize` | unknown | No |
| `screenSizes` | array | No |
| `preserveBorders` | unknown | No |
| `preserveUVs` | unknown | No |
| `exportFormat` | unknown | No |
| `exportPath` | unknown | No |
| `includeNormals` | unknown | No |
| `includeUVs` | unknown | No |
| `includeTangents` | unknown | No |
| `createAsset` | unknown | No |
| `overwrite` | unknown | No |
| `save` | unknown | No |
| `enableNanite` | unknown | No |
| `booleanOperation` | unknown | No |
| `uvMethod` | unknown | No |
| `splinePath` | unknown | No |
| `profileShape` | unknown | No |
| `profileSize` | unknown | No |
| `nanitePositionPrecision` | unknown | No |
| `nanitePercentTriangles` | unknown | No |
| `naniteFallbackRelativeError` | unknown | No |
| `naniteTrimRelativeError` | unknown | No |
| `filePath` | unknown | No |

---

## manage_skeleton

Skeletal meshes, sockets, physics assets; media players/sources.

### Actions

*54 actions available*

| # | Action |
|---|--------|
| 1 | `create_skeleton` |
| 2 | `add_bone` |
| 3 | `remove_bone` |
| 4 | `rename_bone` |
| 5 | `set_bone_transform` |
| 6 | `set_bone_parent` |
| 7 | `create_virtual_bone` |
| 8 | `create_socket` |
| 9 | `configure_socket` |
| 10 | `auto_skin_weights` |
| 11 | `set_vertex_weights` |
| 12 | `normalize_weights` |
| 13 | `prune_weights` |
| 14 | `copy_weights` |
| 15 | `mirror_weights` |
| 16 | `create_physics_asset` |
| 17 | `add_physics_body` |
| 18 | `configure_physics_body` |
| 19 | `add_physics_constraint` |
| 20 | `configure_constraint_limits` |
| 21 | `bind_cloth_to_skeletal_mesh` |
| 22 | `assign_cloth_asset_to_mesh` |
| 23 | `create_morph_target` |
| 24 | `set_morph_target_deltas` |
| 25 | `import_morph_targets` |
| 26 | `get_skeleton_info` |
| 27 | `list_bones` |
| 28 | `list_sockets` |
| 29 | `list_physics_bodies` |
| 30 | `create_media_player` |
| 31 | `create_file_media_source` |
| 32 | `create_stream_media_source` |
| 33 | `create_media_texture` |
| 34 | `create_media_playlist` |
| 35 | `create_media_sound_wave` |
| 36 | `delete_media_asset` |
| 37 | `get_media_info` |
| 38 | `add_to_playlist` |
| 39 | `remove_from_playlist` |
| 40 | `get_playlist` |
| 41 | `open_source` |
| 42 | `open_url` |
| 43 | `close` |
| 44 | `play` |
| 45 | `pause` |
| 46 | `stop` |
| 47 | `seek` |
| 48 | `set_rate` |
| 49 | `set_looping` |
| 50 | `get_duration` |
| 51 | `get_time` |
| 52 | `get_state` |
| 53 | `bind_to_texture` |
| 54 | `unbind_from_texture` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `skeletonPath` | unknown | No |
| `skeletalMeshPath` | unknown | No |
| `physicsAssetPath` | unknown | No |
| `morphTargetPath` | unknown | No |
| `clothAssetPath` | unknown | No |
| `outputPath` | unknown | No |
| `boneName` | unknown | No |
| `newBoneName` | unknown | No |
| `parentBoneName` | unknown | No |
| `sourceBoneName` | unknown | No |
| `targetBoneName` | unknown | No |
| `boneIndex` | unknown | No |
| `location` | object | No |
| `rotation` | object | No |
| `scale` | object | No |
| `socketName` | unknown | No |
| `attachBoneName` | unknown | No |
| `relativeLocation` | object | No |
| `relativeRotation` | object | No |
| `relativeScale` | object | No |
| `vertexIndex` | unknown | No |
| `vertexIndices` | array | No |
| `weights` | array | No |
| `threshold` | unknown | No |
| `mirrorAxis` | unknown | No |
| `mirrorTable` | object | No |
| `bodyType` | unknown | No |
| `bodyName` | unknown | No |
| `mass` | unknown | No |
| `linearDamping` | unknown | No |
| `angularDamping` | unknown | No |
| `collisionEnabled` | unknown | No |
| `simulatePhysics` | unknown | No |
| `constraintName` | unknown | No |
| `bodyA` | unknown | No |
| `bodyB` | unknown | No |
| `limits` | object | No |
| `morphTargetName` | unknown | No |
| `deltas` | array | No |
| `paintValue` | unknown | No |
| `save` | unknown | No |
| `overwrite` | unknown | No |
| `mediaPlayerPath` | unknown | No |
| `mediaSourcePath` | unknown | No |
| `mediaTexturePath` | unknown | No |
| `playlistPath` | unknown | No |
| `filePath` | unknown | No |
| `url` | unknown | No |
| `time` | unknown | No |
| `rate` | unknown | No |
| `looping` | unknown | No |
| `autoPlay` | unknown | No |

---

## manage_material_authoring

Materials, expressions, parameters, landscape layers, textures.

### Actions

*73 actions available*

| # | Action |
|---|--------|
| 1 | `create_material` |
| 2 | `set_blend_mode` |
| 3 | `set_shading_model` |
| 4 | `set_material_domain` |
| 5 | `add_texture_sample` |
| 6 | `add_texture_coordinate` |
| 7 | `add_scalar_parameter` |
| 8 | `add_vector_parameter` |
| 9 | `add_static_switch_parameter` |
| 10 | `add_math_node` |
| 11 | `add_world_position` |
| 12 | `add_vertex_normal` |
| 13 | `add_pixel_depth` |
| 14 | `add_fresnel` |
| 15 | `add_reflection_vector` |
| 16 | `add_panner` |
| 17 | `add_rotator` |
| 18 | `add_noise` |
| 19 | `add_voronoi` |
| 20 | `add_if` |
| 21 | `add_switch` |
| 22 | `add_custom_expression` |
| 23 | `connect_nodes` |
| 24 | `disconnect_nodes` |
| 25 | `create_material_function` |
| 26 | `add_function_input` |
| 27 | `add_function_output` |
| 28 | `use_material_function` |
| 29 | `create_material_instance` |
| 30 | `set_scalar_parameter_value` |
| 31 | `set_vector_parameter_value` |
| 32 | `set_texture_parameter_value` |
| 33 | `create_landscape_material` |
| 34 | `create_decal_material` |
| 35 | `create_post_process_material` |
| 36 | `add_landscape_layer` |
| 37 | `configure_layer_blend` |
| 38 | `compile_material` |
| 39 | `get_material_info` |
| 40 | `create_noise_texture` |
| 41 | `create_gradient_texture` |
| 42 | `create_pattern_texture` |
| 43 | `create_normal_from_height` |
| 44 | `create_ao_from_mesh` |
| 45 | `resize_texture` |
| 46 | `adjust_levels` |
| 47 | `adjust_curves` |
| 48 | `blur` |
| 49 | `sharpen` |
| 50 | `invert` |
| 51 | `desaturate` |
| 52 | `channel_pack` |
| 53 | `channel_extract` |
| 54 | `combine_textures` |
| 55 | `set_compression_settings` |
| 56 | `set_texture_group` |
| 57 | `set_lod_bias` |
| 58 | `configure_virtual_texture` |
| 59 | `set_streaming_priority` |
| 60 | `get_texture_info` |
| 61 | `create_substrate_material` |
| 62 | `set_substrate_properties` |
| 63 | `configure_sss_profile` |
| 64 | `configure_exposure` |
| 65 | `convert_material_to_substrate` |
| 66 | `batch_convert_to_substrate` |
| 67 | `create_material_expression_template` |
| 68 | `configure_landscape_material_layer` |
| 69 | `create_material_instance_batch` |
| 70 | `get_material_dependencies` |
| 71 | `validate_material` |
| 72 | `configure_material_lod` |
| 73 | `export_material_template` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `assetPath` | unknown | No |
| `name` | unknown | No |
| `path` | unknown | No |
| `materialDomain` | unknown | No |
| `blendMode` | unknown | No |
| `shadingModel` | unknown | No |
| `twoSided` | unknown | No |
| `x` | unknown | No |
| `y` | unknown | No |
| `texturePath` | unknown | No |
| `samplerType` | unknown | No |
| `coordinateIndex` | unknown | No |
| `uTiling` | unknown | No |
| `vTiling` | unknown | No |
| `parameterName` | unknown | No |
| `defaultValue` | unknown | No |
| `group` | unknown | No |
| `value` | unknown | No |
| `operation` | unknown | No |
| `constA` | unknown | No |
| `constB` | unknown | No |
| `code` | unknown | No |
| `outputType` | unknown | No |
| `sourceNodeId` | unknown | No |
| `sourcePin` | unknown | No |
| `targetNodeId` | unknown | No |
| `targetPin` | unknown | No |
| `nodeId` | unknown | No |
| `pinName` | unknown | No |
| `functionPath` | unknown | No |
| `exposeToLibrary` | unknown | No |
| `inputName` | unknown | No |
| `inputType` | unknown | No |
| `parentMaterial` | unknown | No |
| `layerName` | unknown | No |
| `blendType` | unknown | No |
| `layers` | array | No |
| `save` | unknown | No |
| `width` | unknown | No |
| `height` | unknown | No |
| `newWidth` | unknown | No |
| `newHeight` | unknown | No |
| `noiseType` | unknown | No |
| `scale` | unknown | No |
| `octaves` | unknown | No |
| `seed` | unknown | No |
| `seamless` | unknown | No |
| `gradientType` | unknown | No |
| `startColor` | object | No |
| `endColor` | object | No |
| `patternType` | unknown | No |
| `primaryColor` | object | No |
| `secondaryColor` | object | No |
| `sourceTexture` | unknown | No |
| `strength` | unknown | No |
| `filterMethod` | unknown | No |
| `outputPath` | unknown | No |
| `channel` | unknown | No |
| `compressionSettings` | unknown | No |
| `textureGroup` | unknown | No |
| `lodBias` | unknown | No |
| `virtualTextureStreaming` | unknown | No |
| `neverStream` | unknown | No |
| `slabType` | unknown | No |
| `thickness` | unknown | No |
| `fuzzAmount` | unknown | No |
| `scatterRadius` | unknown | No |
| `postProcessVolumeName` | unknown | No |
| `minBrightness` | unknown | No |
| `maxBrightness` | unknown | No |
| `speedUp` | unknown | No |
| `speedDown` | unknown | No |

---

## manage_character

Characters, movement, locomotion + Inventory (items, equipment).

### Actions

*78 actions available*

| # | Action |
|---|--------|
| 1 | `create_character_blueprint` |
| 2 | `configure_capsule_component` |
| 3 | `configure_mesh_component` |
| 4 | `configure_camera_component` |
| 5 | `configure_movement_speeds` |
| 6 | `configure_jump` |
| 7 | `configure_rotation` |
| 8 | `add_custom_movement_mode` |
| 9 | `configure_nav_movement` |
| 10 | `setup_mantling` |
| 11 | `setup_vaulting` |
| 12 | `setup_climbing` |
| 13 | `setup_sliding` |
| 14 | `setup_wall_running` |
| 15 | `setup_grappling` |
| 16 | `setup_footstep_system` |
| 17 | `map_surface_to_sound` |
| 18 | `configure_footstep_fx` |
| 19 | `get_character_info` |
| 20 | `create_interaction_component` |
| 21 | `configure_interaction_trace` |
| 22 | `configure_interaction_widget` |
| 23 | `add_interaction_events` |
| 24 | `create_interactable_interface` |
| 25 | `create_door_actor` |
| 26 | `configure_door_properties` |
| 27 | `create_switch_actor` |
| 28 | `configure_switch_properties` |
| 29 | `create_chest_actor` |
| 30 | `configure_chest_properties` |
| 31 | `create_lever_actor` |
| 32 | `setup_destructible_mesh` |
| 33 | `configure_destruction_levels` |
| 34 | `configure_destruction_effects` |
| 35 | `configure_destruction_damage` |
| 36 | `add_destruction_component` |
| 37 | `create_trigger_actor` |
| 38 | `configure_trigger_events` |
| 39 | `configure_trigger_filter` |
| 40 | `configure_trigger_response` |
| 41 | `get_interaction_info` |
| 42 | `inv_create_item_data_asset` |
| 43 | `inv_set_item_properties` |
| 44 | `inv_create_item_category` |
| 45 | `inv_assign_item_category` |
| 46 | `inv_create_inventory_component` |
| 47 | `inv_configure_inventory_slots` |
| 48 | `inv_add_inventory_functions` |
| 49 | `inv_configure_inventory_events` |
| 50 | `inv_set_inventory_replication` |
| 51 | `inv_create_pickup_actor` |
| 52 | `inv_configure_pickup_interaction` |
| 53 | `inv_configure_pickup_respawn` |
| 54 | `inv_configure_pickup_effects` |
| 55 | `inv_create_equipment_component` |
| 56 | `inv_define_equipment_slots` |
| 57 | `inv_configure_equipment_effects` |
| 58 | `inv_add_equipment_functions` |
| 59 | `inv_configure_equipment_visuals` |
| 60 | `inv_create_loot_table` |
| 61 | `inv_add_loot_entry` |
| 62 | `inv_configure_loot_drop` |
| 63 | `inv_set_loot_quality_tiers` |
| 64 | `inv_create_crafting_recipe` |
| 65 | `inv_configure_recipe_requirements` |
| 66 | `inv_create_crafting_station` |
| 67 | `inv_add_crafting_component` |
| 68 | `inv_get_inventory_info` |
| 69 | `configure_locomotion_state` |
| 70 | `query_interaction_targets` |
| 71 | `configure_inventory_slot` |
| 72 | `batch_add_inventory_items` |
| 73 | `configure_equipment_socket` |
| 74 | `get_character_stats_snapshot` |
| 75 | `apply_status_effect` |
| 76 | `configure_footstep_system` |
| 77 | `set_movement_mode` |
| 78 | `configure_mantle_vault` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `name` | unknown | No |
| `path` | unknown | No |
| `blueprintPath` | unknown | No |
| `save` | unknown | No |
| `parentClass` | unknown | No |
| `skeletalMeshPath` | unknown | No |
| `animBlueprintPath` | unknown | No |
| `capsuleRadius` | unknown | No |
| `capsuleHalfHeight` | unknown | No |
| `meshOffset` | object | No |
| `meshRotation` | object | No |
| `cameraSocketName` | unknown | No |
| `cameraOffset` | object | No |
| `cameraUsePawnControlRotation` | unknown | No |
| `springArmLength` | unknown | No |
| `springArmLagEnabled` | unknown | No |
| `springArmLagSpeed` | unknown | No |
| `walkSpeed` | unknown | No |
| `runSpeed` | unknown | No |
| `sprintSpeed` | unknown | No |
| `crouchSpeed` | unknown | No |
| `swimSpeed` | unknown | No |
| `flySpeed` | unknown | No |
| `acceleration` | unknown | No |
| `deceleration` | unknown | No |
| `groundFriction` | unknown | No |
| `jumpHeight` | unknown | No |
| `airControl` | unknown | No |
| `doubleJumpEnabled` | unknown | No |
| `maxJumpCount` | unknown | No |
| `jumpHoldTime` | unknown | No |
| `gravityScale` | unknown | No |
| `fallingLateralFriction` | unknown | No |
| `orientToMovement` | unknown | No |
| `useControllerRotationYaw` | unknown | No |
| `useControllerRotationPitch` | unknown | No |
| `useControllerRotationRoll` | unknown | No |
| `rotationRate` | unknown | No |
| `modeName` | unknown | No |
| `modeId` | unknown | No |
| `navAgentRadius` | unknown | No |
| `navAgentHeight` | unknown | No |
| `avoidanceEnabled` | unknown | No |
| `pathFollowingEnabled` | unknown | No |
| `mantleHeight` | unknown | No |
| `mantleReachDistance` | unknown | No |
| `mantleAnimationPath` | unknown | No |
| `vaultHeight` | unknown | No |
| `vaultDepth` | unknown | No |
| `vaultAnimationPath` | unknown | No |
| `climbSpeed` | unknown | No |
| `climbableTag` | unknown | No |
| `climbAnimationPath` | unknown | No |
| `slideSpeed` | unknown | No |
| `slideDuration` | unknown | No |
| `slideCooldown` | unknown | No |
| `slideAnimationPath` | unknown | No |
| `wallRunSpeed` | unknown | No |
| `wallRunDuration` | unknown | No |
| `wallRunGravityScale` | unknown | No |
| `wallRunAnimationPath` | unknown | No |
| `grappleRange` | unknown | No |
| `grappleSpeed` | unknown | No |
| `grappleTargetTag` | unknown | No |
| `grappleCablePath` | unknown | No |
| `footstepEnabled` | unknown | No |
| `footstepSocketLeft` | unknown | No |
| `footstepSocketRight` | unknown | No |
| `footstepTraceDistance` | unknown | No |
| `surfaceType` | unknown | No |
| `footstepSoundPath` | unknown | No |
| `footstepParticlePath` | unknown | No |
| `footstepDecalPath` | unknown | No |
| `locomotionState` | unknown | No |
| `stateMachineBlueprint` | unknown | No |
| `stateTransitions` | array | No |
| `blendTime` | unknown | No |
| `interactionRange` | unknown | No |
| `interactionTypes` | array | No |
| `slotIndex` | unknown | No |
| `slotType` | unknown | No |
| `slotCapacity` | unknown | No |
| `slotFilter` | array | No |
| `itemDataAssets` | array | No |
| `targetSlot` | unknown | No |
| `autoStack` | unknown | No |
| `socketName` | unknown | No |
| `socketBone` | unknown | No |
| `socketOffset` | object | No |
| `socketRotation` | object | No |
| `socketScale` | object | No |
| `includeStats` | array | No |
| `statusEffectId` | unknown | No |
| `effectDuration` | unknown | No |
| `effectMagnitude` | unknown | No |
| `effectStacks` | unknown | No |
| `effectSource` | unknown | No |
| `footstepMaterialMap` | object | No |
| `footstepVolumeScale` | unknown | No |
| `footstepInterval` | unknown | No |
| `movementMode` | unknown | No |
| `customModeIndex` | unknown | No |
| `mantleEnabled` | unknown | No |
| `vaultEnabled` | unknown | No |
| `mantleMinHeight` | unknown | No |
| `mantleMaxHeight` | unknown | No |
| `vaultMaxWidth` | unknown | No |
| `mantleSpeed` | unknown | No |
| `vaultSpeed` | unknown | No |
| `actorName` | unknown | No |

---

## manage_combat

Weapons, projectiles, damage, melee; GAS abilities and effects.

### Actions

*67 actions available*

| # | Action |
|---|--------|
| 1 | `create_weapon_blueprint` |
| 2 | `configure_weapon_mesh` |
| 3 | `configure_weapon_sockets` |
| 4 | `set_weapon_stats` |
| 5 | `configure_hitscan` |
| 6 | `configure_projectile` |
| 7 | `configure_spread_pattern` |
| 8 | `configure_recoil_pattern` |
| 9 | `configure_aim_down_sights` |
| 10 | `create_projectile_blueprint` |
| 11 | `configure_projectile_movement` |
| 12 | `configure_projectile_collision` |
| 13 | `configure_projectile_homing` |
| 14 | `create_damage_type` |
| 15 | `configure_damage_execution` |
| 16 | `setup_hitbox_component` |
| 17 | `setup_reload_system` |
| 18 | `setup_ammo_system` |
| 19 | `setup_attachment_system` |
| 20 | `setup_weapon_switching` |
| 21 | `configure_muzzle_flash` |
| 22 | `configure_tracer` |
| 23 | `configure_impact_effects` |
| 24 | `configure_shell_ejection` |
| 25 | `create_melee_trace` |
| 26 | `configure_combo_system` |
| 27 | `create_hit_pause` |
| 28 | `configure_hit_reaction` |
| 29 | `setup_parry_block_system` |
| 30 | `configure_weapon_trails` |
| 31 | `get_combat_info` |
| 32 | `create_combo_sequence` |
| 33 | `apply_damage_with_effects` |
| 34 | `configure_weapon_trace` |
| 35 | `create_projectile_pool` |
| 36 | `configure_gas_effect` |
| 37 | `grant_gas_ability` |
| 38 | `configure_melee_trace` |
| 39 | `get_combat_stats` |
| 40 | `configure_block_parry` |
| 41 | `add_ability_system_component` |
| 42 | `configure_asc` |
| 43 | `create_attribute_set` |
| 44 | `add_attribute` |
| 45 | `set_attribute_base_value` |
| 46 | `set_attribute_clamping` |
| 47 | `create_gameplay_ability` |
| 48 | `set_ability_tags` |
| 49 | `set_ability_costs` |
| 50 | `set_ability_cooldown` |
| 51 | `set_ability_targeting` |
| 52 | `add_ability_task` |
| 53 | `set_activation_policy` |
| 54 | `set_instancing_policy` |
| 55 | `create_gameplay_effect` |
| 56 | `set_effect_duration` |
| 57 | `add_effect_modifier` |
| 58 | `set_modifier_magnitude` |
| 59 | `add_effect_execution_calculation` |
| 60 | `add_effect_cue` |
| 61 | `set_effect_stacking` |
| 62 | `set_effect_tags` |
| 63 | `create_gameplay_cue_notify` |
| 64 | `configure_cue_trigger` |
| 65 | `set_cue_effects` |
| 66 | `add_tag_to_asset` |
| 67 | `get_gas_info` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `blueprintPath` | unknown | No |
| `name` | unknown | No |
| `path` | unknown | No |
| `weaponMeshPath` | unknown | No |
| `muzzleSocketName` | unknown | No |
| `ejectionSocketName` | unknown | No |
| `attachmentSocketNames` | array | No |
| `baseDamage` | unknown | No |
| `fireRate` | unknown | No |
| `range` | unknown | No |
| `spread` | unknown | No |
| `hitscanEnabled` | unknown | No |
| `traceChannel` | unknown | No |
| `projectileClass` | unknown | No |
| `spreadPattern` | unknown | No |
| `spreadIncrease` | unknown | No |
| `spreadRecovery` | unknown | No |
| `recoilPitch` | unknown | No |
| `recoilYaw` | unknown | No |
| `recoilRecovery` | unknown | No |
| `adsEnabled` | unknown | No |
| `adsFov` | unknown | No |
| `adsSpeed` | unknown | No |
| `adsSpreadMultiplier` | unknown | No |
| `projectileSpeed` | unknown | No |
| `projectileGravityScale` | unknown | No |
| `projectileLifespan` | unknown | No |
| `projectileMeshPath` | unknown | No |
| `collisionRadius` | unknown | No |
| `bounceEnabled` | unknown | No |
| `bounceVelocityRatio` | unknown | No |
| `homingEnabled` | unknown | No |
| `homingAcceleration` | unknown | No |
| `homingTargetTag` | unknown | No |
| `damageTypeName` | unknown | No |
| `damageCategory` | unknown | No |
| `damageImpulse` | unknown | No |
| `criticalMultiplier` | unknown | No |
| `headshotMultiplier` | unknown | No |
| `hitboxBoneName` | unknown | No |
| `hitboxType` | unknown | No |
| `hitboxSize` | object | No |
| `isDamageZoneHead` | unknown | No |
| `damageMultiplier` | unknown | No |
| `magazineSize` | unknown | No |
| `reloadTime` | unknown | No |
| `reloadAnimationPath` | unknown | No |
| `ammoType` | unknown | No |
| `maxAmmo` | unknown | No |
| `startingAmmo` | unknown | No |
| `attachmentSlots` | array | No |
| `switchInTime` | unknown | No |
| `switchOutTime` | unknown | No |
| `switchInAnimationPath` | unknown | No |
| `switchOutAnimationPath` | unknown | No |
| `muzzleFlashParticlePath` | unknown | No |
| `muzzleFlashScale` | unknown | No |
| `muzzleSoundPath` | unknown | No |
| `tracerParticlePath` | unknown | No |
| `tracerSpeed` | unknown | No |
| `impactParticlePath` | unknown | No |
| `impactSoundPath` | unknown | No |
| `impactDecalPath` | unknown | No |
| `shellMeshPath` | unknown | No |
| `shellEjectionForce` | unknown | No |
| `shellLifespan` | unknown | No |
| `meleeTraceStartSocket` | unknown | No |
| `meleeTraceEndSocket` | unknown | No |
| `meleeTraceRadius` | unknown | No |
| `meleeTraceChannel` | unknown | No |
| `comboWindowTime` | unknown | No |
| `maxComboCount` | unknown | No |
| `comboAnimations` | array | No |
| `hitPauseDuration` | unknown | No |
| `hitPauseTimeDilation` | unknown | No |
| `hitReactionMontage` | unknown | No |
| `hitReactionStunTime` | unknown | No |
| `parryWindowStart` | unknown | No |
| `parryWindowEnd` | unknown | No |
| `parryAnimationPath` | unknown | No |
| `blockDamageReduction` | unknown | No |
| `blockStaminaCost` | unknown | No |
| `weaponTrailParticlePath` | unknown | No |
| `weaponTrailStartSocket` | unknown | No |
| `weaponTrailEndSocket` | unknown | No |

---

## manage_ai

AI Controllers, BT, EQS, perception, State Trees, MassAI, NPCs.

### Actions

*103 actions available*

| # | Action |
|---|--------|
| 1 | `create_ai_controller` |
| 2 | `assign_behavior_tree` |
| 3 | `assign_blackboard` |
| 4 | `create_blackboard_asset` |
| 5 | `add_blackboard_key` |
| 6 | `set_key_instance_synced` |
| 7 | `create_behavior_tree` |
| 8 | `add_composite_node` |
| 9 | `add_task_node` |
| 10 | `add_decorator` |
| 11 | `add_service` |
| 12 | `configure_bt_node` |
| 13 | `create_eqs_query` |
| 14 | `add_eqs_generator` |
| 15 | `add_eqs_context` |
| 16 | `add_eqs_test` |
| 17 | `configure_test_scoring` |
| 18 | `add_ai_perception_component` |
| 19 | `configure_sight_config` |
| 20 | `configure_hearing_config` |
| 21 | `configure_damage_sense_config` |
| 22 | `set_perception_team` |
| 23 | `create_state_tree` |
| 24 | `add_state_tree_state` |
| 25 | `add_state_tree_transition` |
| 26 | `configure_state_tree_task` |
| 27 | `create_smart_object_definition` |
| 28 | `add_smart_object_slot` |
| 29 | `configure_slot_behavior` |
| 30 | `add_smart_object_component` |
| 31 | `bind_statetree` |
| 32 | `create_mass_entity_config` |
| 33 | `configure_mass_entity` |
| 34 | `add_mass_spawner` |
| 35 | `spawn_mass_entity` |
| 36 | `destroy_mass_entity` |
| 37 | `query_mass_entities` |
| 38 | `set_mass_entity_fragment` |
| 39 | `get_statetree_state` |
| 40 | `trigger_statetree_transition` |
| 41 | `list_statetree_states` |
| 42 | `create_smart_object` |
| 43 | `query_smart_objects` |
| 44 | `claim_smart_object` |
| 45 | `release_smart_object` |
| 46 | `get_ai_info` |
| 47 | `bt_add_node` |
| 48 | `bt_connect_nodes` |
| 49 | `bt_remove_node` |
| 50 | `bt_break_connections` |
| 51 | `bt_set_node_properties` |
| 52 | `configure_nav_mesh_settings` |
| 53 | `set_nav_agent_properties` |
| 54 | `rebuild_navigation` |
| 55 | `create_nav_modifier_component` |
| 56 | `set_nav_area_class` |
| 57 | `configure_nav_area_cost` |
| 58 | `create_nav_link_proxy` |
| 59 | `configure_nav_link` |
| 60 | `set_nav_link_type` |
| 61 | `create_smart_link` |
| 62 | `configure_smart_link_behavior` |
| 63 | `get_navigation_info` |
| 64 | `create_convai_character` |
| 65 | `configure_character_backstory` |
| 66 | `configure_character_voice` |
| 67 | `configure_convai_lipsync` |
| 68 | `start_convai_session` |
| 69 | `stop_convai_session` |
| 70 | `send_text_to_character` |
| 71 | `get_character_response` |
| 72 | `configure_convai_actions` |
| 73 | `get_convai_info` |
| 74 | `create_inworld_character` |
| 75 | `configure_inworld_settings` |
| 76 | `configure_inworld_scene` |
| 77 | `start_inworld_session` |
| 78 | `stop_inworld_session` |
| 79 | `send_message_to_character` |
| 80 | `get_character_emotion` |
| 81 | `get_character_goals` |
| 82 | `trigger_inworld_event` |
| 83 | `get_inworld_info` |
| 84 | `configure_audio2face` |
| 85 | `process_audio_to_blendshapes` |
| 86 | `configure_blendshape_mapping` |
| 87 | `start_audio2face_stream` |
| 88 | `stop_audio2face_stream` |
| 89 | `get_audio2face_status` |
| 90 | `configure_ace_emotions` |
| 91 | `get_ace_info` |
| 92 | `get_ai_npc_info` |
| 93 | `list_available_ai_backends` |
| 94 | `ai_assistant_query` |
| 95 | `ai_assistant_explain_feature` |
| 96 | `ai_assistant_suggest_fix` |
| 97 | `configure_state_tree_node` |
| 98 | `debug_behavior_tree` |
| 99 | `query_eqs_results` |
| 100 | `configure_mass_ai_fragment` |
| 101 | `spawn_mass_ai_entities` |
| 102 | `get_ai_perception_data` |
| 103 | `configure_smart_object` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `name` | unknown | No |
| `path` | unknown | No |
| `blueprintPath` | unknown | No |
| `controllerPath` | unknown | No |
| `behaviorTreePath` | unknown | No |
| `blackboardPath` | unknown | No |
| `parentClass` | unknown | No |
| `autoRunBehaviorTree` | unknown | No |
| `keyName` | unknown | No |
| `keyType` | unknown | No |
| `isInstanceSynced` | unknown | No |
| `baseObjectClass` | unknown | No |
| `enumClass` | unknown | No |
| `compositeType` | unknown | No |
| `taskType` | unknown | No |
| `decoratorType` | unknown | No |
| `serviceType` | unknown | No |
| `parentNodeId` | unknown | No |
| `nodeId` | unknown | No |
| `nodeProperties` | object | No |
| `customTaskClass` | unknown | No |
| `customDecoratorClass` | unknown | No |
| `customServiceClass` | unknown | No |
| `queryPath` | unknown | No |
| `generatorType` | unknown | No |
| `contextType` | unknown | No |
| `testType` | unknown | No |
| `generatorSettings` | object | No |
| `testSettings` | object | No |
| `testIndex` | unknown | No |
| `sightConfig` | object | No |
| `hearingConfig` | object | No |
| `damageConfig` | object | No |
| `teamId` | unknown | No |
| `dominantSense` | unknown | No |
| `stateTreePath` | unknown | No |
| `stateName` | unknown | No |
| `fromState` | unknown | No |
| `toState` | unknown | No |
| `transitionCondition` | unknown | No |
| `stateTaskClass` | unknown | No |
| `stateEvaluatorClass` | unknown | No |
| `definitionPath` | unknown | No |
| `slotIndex` | unknown | No |
| `slotOffset` | object | No |
| `slotRotation` | object | No |
| `slotBehaviorDefinition` | unknown | No |
| `slotActivityTags` | array | No |
| `slotUserTags` | array | No |
| `slotEnabled` | unknown | No |
| `configPath` | unknown | No |
| `target` | unknown | No |
| `count` | unknown | No |
| `massTraits` | array | No |
| `massProcessors` | array | No |
| `spawnerSettings` | object | No |
| `savePath` | unknown | No |
| `x` | unknown | No |
| `y` | unknown | No |
| `comment` | unknown | No |
| `btNodeProperties` | object | No |
| `navMeshPath` | unknown | No |
| `actorPath` | unknown | No |
| `agentRadius` | unknown | No |
| `agentHeight` | unknown | No |
| `agentStepHeight` | unknown | No |
| `agentMaxSlope` | unknown | No |
| `cellSize` | unknown | No |
| `cellHeight` | unknown | No |
| `areaClass` | unknown | No |
| `areaCost` | unknown | No |
| `linkName` | unknown | No |
| `startPoint` | object | No |
| `endPoint` | object | No |
| `direction` | unknown | No |
| `linkEnabled` | unknown | No |
| `linkType` | unknown | No |

---

## manage_widget_authoring

UMG widgets: buttons, text, sliders. Layouts, bindings, HUDs.

### Actions

*73 actions available*

| # | Action |
|---|--------|
| 1 | `create_widget_blueprint` |
| 2 | `set_widget_parent_class` |
| 3 | `add_canvas_panel` |
| 4 | `add_horizontal_box` |
| 5 | `add_vertical_box` |
| 6 | `add_overlay` |
| 7 | `add_grid_panel` |
| 8 | `add_uniform_grid` |
| 9 | `add_wrap_box` |
| 10 | `add_scroll_box` |
| 11 | `add_size_box` |
| 12 | `add_scale_box` |
| 13 | `add_border` |
| 14 | `add_text_block` |
| 15 | `add_rich_text_block` |
| 16 | `add_image` |
| 17 | `add_button` |
| 18 | `add_check_box` |
| 19 | `add_slider` |
| 20 | `add_progress_bar` |
| 21 | `add_text_input` |
| 22 | `add_combo_box` |
| 23 | `add_spin_box` |
| 24 | `add_list_view` |
| 25 | `add_tree_view` |
| 26 | `set_anchor` |
| 27 | `set_alignment` |
| 28 | `set_position` |
| 29 | `set_size` |
| 30 | `set_padding` |
| 31 | `set_z_order` |
| 32 | `set_render_transform` |
| 33 | `set_visibility` |
| 34 | `set_style` |
| 35 | `set_clipping` |
| 36 | `create_property_binding` |
| 37 | `bind_text` |
| 38 | `bind_visibility` |
| 39 | `bind_color` |
| 40 | `bind_enabled` |
| 41 | `bind_on_clicked` |
| 42 | `bind_on_hovered` |
| 43 | `bind_on_value_changed` |
| 44 | `create_widget_animation` |
| 45 | `add_animation_track` |
| 46 | `add_animation_keyframe` |
| 47 | `set_animation_loop` |
| 48 | `create_main_menu` |
| 49 | `create_pause_menu` |
| 50 | `create_settings_menu` |
| 51 | `create_loading_screen` |
| 52 | `create_hud_widget` |
| 53 | `add_health_bar` |
| 54 | `add_ammo_counter` |
| 55 | `add_minimap` |
| 56 | `add_crosshair` |
| 57 | `add_compass` |
| 58 | `add_interaction_prompt` |
| 59 | `add_objective_tracker` |
| 60 | `add_damage_indicator` |
| 61 | `create_inventory_ui` |
| 62 | `create_dialog_widget` |
| 63 | `create_radial_menu` |
| 64 | `get_widget_info` |
| 65 | `preview_widget` |
| 66 | `create_widget_template` |
| 67 | `configure_widget_binding_batch` |
| 68 | `create_widget_animation_advanced` |
| 69 | `configure_widget_navigation` |
| 70 | `validate_widget_accessibility` |
| 71 | `create_hud_layout` |
| 72 | `configure_safe_zone` |
| 73 | `batch_localize_widgets` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `name` | unknown | No |
| `folder` | unknown | No |
| `widgetPath` | unknown | No |
| `slotName` | unknown | No |
| `parentSlot` | unknown | No |
| `parentClass` | unknown | No |
| `anchorMin` | object | No |
| `anchorMax` | object | No |
| `alignment` | object | No |
| `alignmentX` | unknown | No |
| `alignmentY` | unknown | No |
| `positionX` | unknown | No |
| `positionY` | unknown | No |
| `sizeX` | unknown | No |
| `sizeY` | unknown | No |
| `sizeToContent` | unknown | No |
| `left` | unknown | No |
| `top` | unknown | No |
| `right` | unknown | No |
| `bottom` | unknown | No |
| `zOrder` | unknown | No |
| `translation` | object | No |
| `scale` | object | No |
| `shear` | object | No |
| `angle` | unknown | No |
| `pivot` | object | No |
| `visibility` | unknown | No |
| `clipping` | unknown | No |
| `text` | unknown | No |
| `font` | unknown | No |
| `fontSize` | unknown | No |
| `colorAndOpacity` | object | No |
| `justification` | unknown | No |
| `autoWrap` | unknown | No |
| `texturePath` | unknown | No |
| `brushSize` | object | No |
| `brushTiling` | unknown | No |
| `isEnabled` | unknown | No |
| `isChecked` | unknown | No |
| `value` | unknown | No |
| `minValue` | unknown | No |
| `maxValue` | unknown | No |
| `stepSize` | unknown | No |
| `delta` | unknown | No |
| `percent` | unknown | No |
| `fillColorAndOpacity` | object | No |
| `barFillType` | unknown | No |
| `isMarquee` | unknown | No |
| `inputType` | unknown | No |
| `hintText` | unknown | No |
| `isPassword` | unknown | No |
| `options` | array | No |
| `selectedOption` | unknown | No |
| `entryWidgetClass` | unknown | No |
| `orientation` | unknown | No |
| `selectionMode` | unknown | No |
| `scrollBarVisibility` | unknown | No |
| `alwaysShowScrollbar` | unknown | No |
| `columnCount` | unknown | No |
| `rowCount` | unknown | No |
| `slotPadding` | unknown | No |
| `minDesiredSlotWidth` | unknown | No |
| `minDesiredSlotHeight` | unknown | No |
| `innerSlotPadding` | unknown | No |
| `wrapWidth` | unknown | No |
| `explicitWrapWidth` | unknown | No |
| `widthOverride` | unknown | No |
| `heightOverride` | unknown | No |
| `minDesiredWidth` | unknown | No |
| `minDesiredHeight` | unknown | No |
| `stretch` | unknown | No |
| `stretchDirection` | unknown | No |
| `userSpecifiedScale` | unknown | No |
| `brushColor` | object | No |
| `padding` | unknown | No |
| `horizontalAlignment` | unknown | No |
| `verticalAlignment` | unknown | No |
| `color` | object | No |
| `opacity` | unknown | No |
| `brush` | unknown | No |
| `backgroundImage` | unknown | No |
| `style` | unknown | No |
| `propertyName` | unknown | No |
| `bindingType` | unknown | No |
| `bindingSource` | unknown | No |
| `functionName` | unknown | No |
| `onHoveredFunction` | unknown | No |
| `onUnhoveredFunction` | unknown | No |
| `animationName` | unknown | No |
| `length` | unknown | No |
| `trackType` | unknown | No |
| `time` | unknown | No |
| `interpolation` | unknown | No |
| `loopCount` | unknown | No |
| `playMode` | unknown | No |
| `includePlayButton` | unknown | No |
| `includeSettingsButton` | unknown | No |
| `includeQuitButton` | unknown | No |
| `includeResumeButton` | unknown | No |
| `includeQuitToMenuButton` | unknown | No |
| `settingsType` | unknown | No |
| `includeApplyButton` | unknown | No |
| `includeResetButton` | unknown | No |
| `includeProgressBar` | unknown | No |
| `includeTipText` | unknown | No |
| `includeBackgroundImage` | unknown | No |
| `titleText` | unknown | No |
| `elements` | array | No |
| `barStyle` | unknown | No |
| `showNumbers` | unknown | No |
| `barColor` | object | No |
| `ammoStyle` | unknown | No |
| `showReserve` | unknown | No |
| `ammoIcon` | unknown | No |
| `minimapSize` | unknown | No |
| `minimapShape` | unknown | No |
| `rotateWithPlayer` | unknown | No |
| `showObjectives` | unknown | No |
| `crosshairStyle` | unknown | No |
| `crosshairSize` | unknown | No |
| `spreadMultiplier` | unknown | No |
| `showDegrees` | unknown | No |
| `showCardinals` | unknown | No |
| `promptFormat` | unknown | No |
| `showKeyIcon` | unknown | No |
| `keyIconStyle` | unknown | No |
| `maxVisibleObjectives` | unknown | No |
| `showProgress` | unknown | No |
| `animateUpdates` | unknown | No |
| `indicatorStyle` | unknown | No |
| `fadeTime` | unknown | No |
| `gridSize` | object | No |
| `slotSize` | unknown | No |
| `showEquipment` | unknown | No |
| `showDetails` | unknown | No |
| `showPortrait` | unknown | No |
| `showSpeakerName` | unknown | No |
| `choiceLayout` | unknown | No |
| `segmentCount` | unknown | No |
| `innerRadius` | unknown | No |
| `outerRadius` | unknown | No |
| `showIcons` | unknown | No |
| `showLabels` | unknown | No |
| `previewSize` | unknown | No |
| `customWidth` | unknown | No |
| `customHeight` | unknown | No |
| `templateName` | unknown | No |
| `templateType` | unknown | No |
| `templateConfig` | object | No |
| `bindings` | array | No |
| `animationDuration` | unknown | No |
| `easing` | unknown | No |
| `animationProperties` | array | No |
| `animationKeyframes` | array | No |
| `navigationMap` | object | No |
| `navigationMode` | unknown | No |
| `defaultFocus` | unknown | No |
| `wrapNavigation` | unknown | No |
| `accessibilityRules` | array | No |
| `wcagLevel` | unknown | No |
| `checkContrast` | unknown | No |
| `checkFocusOrder` | unknown | No |
| `checkTextSize` | unknown | No |
| `checkScreenReader` | unknown | No |
| `hudLayoutName` | unknown | No |
| `hudZones` | array | No |
| `safeZoneType` | unknown | No |
| `safeZonePadding` | object | No |
| `scalingRule` | unknown | No |
| `targetWidgets` | array | No |
| `localizationNamespace` | unknown | No |
| `localizationKey` | unknown | No |
| `extractStrings` | unknown | No |
| `updateBindings` | unknown | No |

---

## manage_networking

Replication, RPCs, prediction, sessions; GameModes, teams.

### Actions

*73 actions available*

| # | Action |
|---|--------|
| 1 | `set_property_replicated` |
| 2 | `set_replication_condition` |
| 3 | `configure_net_update_frequency` |
| 4 | `configure_net_priority` |
| 5 | `set_net_dormancy` |
| 6 | `configure_replication_graph` |
| 7 | `create_rpc_function` |
| 8 | `configure_rpc_validation` |
| 9 | `set_rpc_reliability` |
| 10 | `set_owner` |
| 11 | `set_autonomous_proxy` |
| 12 | `check_has_authority` |
| 13 | `check_is_locally_controlled` |
| 14 | `configure_net_cull_distance` |
| 15 | `set_always_relevant` |
| 16 | `set_only_relevant_to_owner` |
| 17 | `configure_net_serialization` |
| 18 | `set_replicated_using` |
| 19 | `configure_push_model` |
| 20 | `configure_client_prediction` |
| 21 | `configure_server_correction` |
| 22 | `add_network_prediction_data` |
| 23 | `configure_movement_prediction` |
| 24 | `configure_net_driver` |
| 25 | `set_net_role` |
| 26 | `configure_replicated_movement` |
| 27 | `get_networking_info` |
| 28 | `debug_replication_graph` |
| 29 | `configure_net_relevancy` |
| 30 | `get_rpc_statistics` |
| 31 | `configure_prediction_settings` |
| 32 | `simulate_network_conditions` |
| 33 | `get_session_players` |
| 34 | `configure_team_settings` |
| 35 | `send_server_rpc` |
| 36 | `get_net_role_info` |
| 37 | `configure_dormancy` |
| 38 | `configure_local_session_settings` |
| 39 | `configure_session_interface` |
| 40 | `configure_split_screen` |
| 41 | `set_split_screen_type` |
| 42 | `add_local_player` |
| 43 | `remove_local_player` |
| 44 | `configure_lan_play` |
| 45 | `host_lan_server` |
| 46 | `join_lan_server` |
| 47 | `enable_voice_chat` |
| 48 | `configure_voice_settings` |
| 49 | `set_voice_channel` |
| 50 | `mute_player` |
| 51 | `set_voice_attenuation` |
| 52 | `configure_push_to_talk` |
| 53 | `get_sessions_info` |
| 54 | `create_game_mode` |
| 55 | `create_game_state` |
| 56 | `create_player_controller` |
| 57 | `create_player_state` |
| 58 | `create_game_instance` |
| 59 | `create_hud_class` |
| 60 | `set_default_pawn_class` |
| 61 | `set_player_controller_class` |
| 62 | `set_game_state_class` |
| 63 | `set_player_state_class` |
| 64 | `configure_game_rules` |
| 65 | `setup_match_states` |
| 66 | `configure_round_system` |
| 67 | `configure_team_system` |
| 68 | `configure_scoring_system` |
| 69 | `configure_spawn_system` |
| 70 | `configure_player_start` |
| 71 | `set_respawn_rules` |
| 72 | `configure_spectating` |
| 73 | `get_game_framework_info` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `blueprintPath` | unknown | No |
| `actorName` | unknown | No |
| `propertyName` | unknown | No |
| `replicated` | unknown | No |
| `condition` | unknown | No |
| `repNotifyFunc` | unknown | No |
| `netUpdateFrequency` | unknown | No |
| `minNetUpdateFrequency` | unknown | No |
| `netPriority` | unknown | No |
| `dormancy` | unknown | No |
| `nodeClass` | unknown | No |
| `spatialBias` | unknown | No |
| `defaultSettingsClass` | unknown | No |
| `functionName` | unknown | No |
| `rpcType` | unknown | No |
| `reliable` | unknown | No |
| `parameters` | array | No |
| `returnType` | unknown | No |
| `validationFunctionName` | unknown | No |
| `withValidation` | unknown | No |
| `ownerActorName` | unknown | No |
| `isAutonomousProxy` | unknown | No |
| `netCullDistanceSquared` | unknown | No |
| `useOwnerNetRelevancy` | unknown | No |
| `alwaysRelevant` | unknown | No |
| `onlyRelevantToOwner` | unknown | No |
| `structName` | unknown | No |
| `useNetSerialize` | unknown | No |
| `usePushModel` | unknown | No |
| `enablePrediction` | unknown | No |
| `predictionKey` | unknown | No |
| `correctionThreshold` | unknown | No |
| `smoothingRate` | unknown | No |
| `dataType` | unknown | No |
| `properties` | array | No |
| `networkSmoothingMode` | unknown | No |
| `networkMaxSmoothUpdateDistance` | unknown | No |
| `networkNoSmoothUpdateDistance` | unknown | No |
| `maxClientRate` | unknown | No |
| `maxInternetClientRate` | unknown | No |
| `netServerMaxTickRate` | unknown | No |
| `role` | unknown | No |
| `replicateMovement` | unknown | No |
| `replicatedMovementMode` | unknown | No |
| `locationQuantizationLevel` | unknown | No |
| `save` | unknown | No |
| `sessionName` | unknown | No |
| `sessionId` | unknown | No |
| `maxPlayers` | unknown | No |
| `bIsLANMatch` | unknown | No |
| `bAllowJoinInProgress` | unknown | No |
| `splitScreenType` | unknown | No |
| `playerIndex` | unknown | No |
| `controllerId` | unknown | No |
| `serverAddress` | unknown | No |
| `serverPort` | unknown | No |
| `serverPassword` | unknown | No |
| `serverName` | unknown | No |
| `mapName` | unknown | No |
| `voiceEnabled` | unknown | No |
| `voiceSettings` | object | No |
| `channelName` | unknown | No |
| `channelType` | unknown | No |
| `targetPlayerId` | unknown | No |
| `muted` | unknown | No |
| `attenuationRadius` | unknown | No |
| `pushToTalkEnabled` | unknown | No |
| `pushToTalkKey` | unknown | No |
| `showConnections` | unknown | No |
| `showActorList` | unknown | No |
| `relevancyRadius` | unknown | No |
| `relevancyMode` | unknown | No |
| `includeRpcDetails` | unknown | No |
| `resetStats` | unknown | No |
| `predictionEnabled` | unknown | No |
| `predictionAmount` | unknown | No |
| `interpolationEnabled` | unknown | No |
| `latencyMs` | unknown | No |
| `packetLoss` | unknown | No |
| `jitterMs` | unknown | No |
| `bandwidthLimit` | unknown | No |
| `includeInactive` | unknown | No |
| `teamId` | unknown | No |
| `teamName` | unknown | No |
| `autoBalance` | unknown | No |
| `rpcName` | unknown | No |
| `rpcParameters` | object | No |
| `targetActor` | unknown | No |
| `dormancyMode` | unknown | No |
| `flushDormancy` | unknown | No |

---

## manage_volumes

Volumes (trigger, physics, audio, nav) and splines (meshes).

### Actions

*41 actions available*

| # | Action |
|---|--------|
| 1 | `create_trigger_volume` |
| 2 | `create_trigger_box` |
| 3 | `create_trigger_sphere` |
| 4 | `create_trigger_capsule` |
| 5 | `create_blocking_volume` |
| 6 | `create_kill_z_volume` |
| 7 | `create_pain_causing_volume` |
| 8 | `create_physics_volume` |
| 9 | `create_audio_volume` |
| 10 | `create_reverb_volume` |
| 11 | `create_cull_distance_volume` |
| 12 | `create_precomputed_visibility_volume` |
| 13 | `create_lightmass_importance_volume` |
| 14 | `create_nav_mesh_bounds_volume` |
| 15 | `create_nav_modifier_volume` |
| 16 | `create_camera_blocking_volume` |
| 17 | `set_volume_extent` |
| 18 | `set_volume_properties` |
| 19 | `get_volumes_info` |
| 20 | `create_spline_actor` |
| 21 | `add_spline_point` |
| 22 | `remove_spline_point` |
| 23 | `set_spline_point_position` |
| 24 | `set_spline_point_tangents` |
| 25 | `set_spline_point_rotation` |
| 26 | `set_spline_point_scale` |
| 27 | `set_spline_type` |
| 28 | `create_spline_mesh_component` |
| 29 | `set_spline_mesh_asset` |
| 30 | `configure_spline_mesh_axis` |
| 31 | `set_spline_mesh_material` |
| 32 | `scatter_meshes_along_spline` |
| 33 | `configure_mesh_spacing` |
| 34 | `configure_mesh_randomization` |
| 35 | `create_road_spline` |
| 36 | `create_river_spline` |
| 37 | `create_fence_spline` |
| 38 | `create_wall_spline` |
| 39 | `create_cable_spline` |
| 40 | `create_pipe_spline` |
| 41 | `get_splines_info` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `volumeName` | unknown | No |
| `volumePath` | unknown | No |
| `location` | object | No |
| `rotation` | object | No |
| `extent` | object | No |
| `sphereRadius` | unknown | No |
| `capsuleRadius` | unknown | No |
| `capsuleHalfHeight` | unknown | No |
| `boxExtent` | object | No |
| `bPainCausing` | unknown | No |
| `damagePerSec` | unknown | No |
| `damageType` | unknown | No |
| `bWaterVolume` | unknown | No |
| `fluidFriction` | unknown | No |
| `terminalVelocity` | unknown | No |
| `priority` | unknown | No |
| `bEnabled` | unknown | No |
| `reverbEffect` | unknown | No |
| `reverbVolume` | unknown | No |
| `fadeTime` | unknown | No |
| `cullDistances` | array | No |
| `areaClass` | unknown | No |
| `bDynamicModifier` | unknown | No |
| `bUnbound` | unknown | No |
| `blendRadius` | unknown | No |
| `blendWeight` | unknown | No |
| `properties` | object | No |
| `filter` | unknown | No |
| `volumeType` | unknown | No |
| `save` | unknown | No |
| `actorName` | unknown | No |
| `actorPath` | unknown | No |
| `splineName` | unknown | No |
| `componentName` | unknown | No |
| `blueprintPath` | unknown | No |
| `pointIndex` | unknown | No |
| `position` | object | No |
| `arriveTangent` | object | No |
| `leaveTangent` | object | No |
| `tangent` | object | No |
| `coordinateSpace` | unknown | No |
| `splineType` | unknown | No |
| `bClosedLoop` | unknown | No |
| `meshPath` | unknown | No |
| `materialPath` | unknown | No |
| `forwardAxis` | unknown | No |
| `spacing` | unknown | No |
| `templateType` | unknown | No |
| `width` | unknown | No |
| `points` | array | No |

---

## manage_data

Data assets, tables, save games, tags, config; modding/PAK/UGC.

### Actions

*62 actions available*

| # | Action |
|---|--------|
| 1 | `create_data_asset` |
| 2 | `create_primary_data_asset` |
| 3 | `get_data_asset_info` |
| 4 | `set_data_asset_property` |
| 5 | `create_data_table` |
| 6 | `add_data_table_row` |
| 7 | `remove_data_table_row` |
| 8 | `get_data_table_row` |
| 9 | `get_data_table_rows` |
| 10 | `import_data_table_csv` |
| 11 | `export_data_table_csv` |
| 12 | `empty_data_table` |
| 13 | `create_curve_table` |
| 14 | `add_curve_row` |
| 15 | `get_curve_value` |
| 16 | `import_curve_table_csv` |
| 17 | `export_curve_table_csv` |
| 18 | `create_save_game_blueprint` |
| 19 | `save_game_to_slot` |
| 20 | `load_game_from_slot` |
| 21 | `delete_save_slot` |
| 22 | `does_save_exist` |
| 23 | `get_save_slot_names` |
| 24 | `create_gameplay_tag` |
| 25 | `add_native_gameplay_tag` |
| 26 | `request_gameplay_tag` |
| 27 | `check_tag_match` |
| 28 | `create_tag_container` |
| 29 | `add_tag_to_container` |
| 30 | `remove_tag_from_container` |
| 31 | `has_tag` |
| 32 | `get_all_gameplay_tags` |
| 33 | `read_config_value` |
| 34 | `write_config_value` |
| 35 | `get_config_section` |
| 36 | `flush_config` |
| 37 | `reload_config` |
| 38 | `configure_mod_loading_paths` |
| 39 | `scan_for_mod_paks` |
| 40 | `load_mod_pak` |
| 41 | `unload_mod_pak` |
| 42 | `validate_mod_pak` |
| 43 | `configure_mod_load_order` |
| 44 | `list_installed_mods` |
| 45 | `enable_mod` |
| 46 | `disable_mod` |
| 47 | `check_mod_compatibility` |
| 48 | `get_mod_info` |
| 49 | `configure_asset_override_paths` |
| 50 | `register_mod_asset_redirect` |
| 51 | `restore_original_asset` |
| 52 | `list_asset_overrides` |
| 53 | `export_moddable_headers` |
| 54 | `create_mod_template_project` |
| 55 | `configure_exposed_classes` |
| 56 | `get_sdk_config` |
| 57 | `configure_mod_sandbox` |
| 58 | `set_allowed_mod_operations` |
| 59 | `validate_mod_content` |
| 60 | `get_security_config` |
| 61 | `get_modding_info` |
| 62 | `reset_mod_system` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `assetPath` | unknown | No |
| `assetName` | unknown | No |
| `savePath` | unknown | No |
| `dataAssetClass` | unknown | No |
| `primaryAssetType` | unknown | No |
| `primaryAssetName` | unknown | No |
| `properties` | object | No |
| `propertyName` | unknown | No |
| `propertyValue` | unknown | No |
| `dataTablePath` | unknown | No |
| `rowStructPath` | unknown | No |
| `rowName` | unknown | No |
| `rowData` | object | No |
| `csvString` | unknown | No |
| `csvFilePath` | unknown | No |
| `curveTablePath` | unknown | No |
| `curveName` | unknown | No |
| `curveType` | unknown | No |
| `keyTime` | unknown | No |
| `keyValue` | unknown | No |
| `interpMode` | unknown | No |
| `slotName` | unknown | No |
| `userIndex` | unknown | No |
| `saveGameClass` | unknown | No |
| `saveData` | object | No |
| `tagName` | unknown | No |
| `tagString` | unknown | No |
| `tagDevComment` | unknown | No |
| `tagToCheck` | unknown | No |
| `containerName` | unknown | No |
| `exactMatch` | unknown | No |
| `tags` | array | No |
| `configName` | unknown | No |
| `configSection` | unknown | No |
| `configKey` | unknown | No |
| `configValue` | unknown | No |
| `save` | unknown | No |

---

## manage_build

UBT, cook/package, plugins, DDC; tests, profiling, validation.

### Actions

*48 actions available*

| # | Action |
|---|--------|
| 1 | `run_ubt` |
| 2 | `generate_project_files` |
| 3 | `compile_shaders` |
| 4 | `cook_content` |
| 5 | `package_project` |
| 6 | `configure_build_settings` |
| 7 | `get_build_info` |
| 8 | `configure_platform` |
| 9 | `get_platform_settings` |
| 10 | `get_target_platforms` |
| 11 | `validate_assets` |
| 12 | `audit_assets` |
| 13 | `get_asset_size_info` |
| 14 | `get_asset_references` |
| 15 | `configure_chunking` |
| 16 | `create_pak_file` |
| 17 | `configure_encryption` |
| 18 | `list_plugins` |
| 19 | `enable_plugin` |
| 20 | `disable_plugin` |
| 21 | `get_plugin_info` |
| 22 | `clear_ddc` |
| 23 | `get_ddc_stats` |
| 24 | `configure_ddc` |
| 25 | `list_tests` |
| 26 | `run_tests` |
| 27 | `run_test` |
| 28 | `get_test_results` |
| 29 | `get_test_info` |
| 30 | `list_functional_tests` |
| 31 | `run_functional_test` |
| 32 | `get_functional_test_results` |
| 33 | `start_trace` |
| 34 | `stop_trace` |
| 35 | `get_trace_status` |
| 36 | `enable_visual_logger` |
| 37 | `disable_visual_logger` |
| 38 | `get_visual_logger_status` |
| 39 | `start_stats_capture` |
| 40 | `stop_stats_capture` |
| 41 | `get_memory_report` |
| 42 | `get_performance_stats` |
| 43 | `validate_asset` |
| 44 | `validate_assets_in_path` |
| 45 | `validate_blueprint` |
| 46 | `check_map_errors` |
| 47 | `fix_redirectors` |
| 48 | `get_redirectors` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `target` | unknown | No |
| `platform` | unknown | No |
| `configuration` | unknown | No |
| `ubtArgs` | unknown | No |
| `clean` | unknown | No |
| `cookFlavor` | unknown | No |
| `iterativeCook` | unknown | No |
| `compressContent` | unknown | No |
| `outputDirectory` | unknown | No |
| `stagingDirectory` | unknown | No |
| `maps` | array | No |
| `settingName` | unknown | No |
| `settingValue` | unknown | No |
| `assetPaths` | array | No |
| `assetPath` | unknown | No |
| `validateOnSave` | unknown | No |
| `validationRules` | array | No |
| `chunkId` | unknown | No |
| `chunkAssets` | array | No |
| `pakFilePath` | unknown | No |
| `pakContentPaths` | array | No |
| `signPak` | unknown | No |
| `encryptPak` | unknown | No |
| `encryptionKey` | unknown | No |
| `pluginName` | unknown | No |
| `pluginCategory` | unknown | No |
| `includeEngine` | unknown | No |
| `includeProject` | unknown | No |
| `ddcBackend` | unknown | No |
| `ddcPath` | unknown | No |
| `clearLocal` | unknown | No |
| `clearShared` | unknown | No |
| `save` | unknown | No |

---

## manage_editor_utilities

Editor modes, content browser, selection, collision, subsystems.

### Actions

*45 actions available*

| # | Action |
|---|--------|
| 1 | `set_editor_mode` |
| 2 | `configure_editor_preferences` |
| 3 | `set_grid_settings` |
| 4 | `set_snap_settings` |
| 5 | `navigate_to_path` |
| 6 | `sync_to_asset` |
| 7 | `create_collection` |
| 8 | `add_to_collection` |
| 9 | `show_in_explorer` |
| 10 | `select_actor` |
| 11 | `select_actors_by_class` |
| 12 | `select_actors_by_tag` |
| 13 | `deselect_all` |
| 14 | `group_actors` |
| 15 | `ungroup_actors` |
| 16 | `get_selected_actors` |
| 17 | `create_collision_channel` |
| 18 | `create_collision_profile` |
| 19 | `configure_channel_responses` |
| 20 | `get_collision_info` |
| 21 | `create_physical_material` |
| 22 | `set_friction` |
| 23 | `set_restitution` |
| 24 | `configure_surface_type` |
| 25 | `get_physical_material_info` |
| 26 | `create_game_instance_subsystem` |
| 27 | `create_world_subsystem` |
| 28 | `create_local_player_subsystem` |
| 29 | `get_subsystem_info` |
| 30 | `set_timer` |
| 31 | `clear_timer` |
| 32 | `clear_all_timers` |
| 33 | `get_active_timers` |
| 34 | `create_event_dispatcher` |
| 35 | `bind_to_event` |
| 36 | `unbind_from_event` |
| 37 | `broadcast_event` |
| 38 | `create_blueprint_interface` |
| 39 | `begin_transaction` |
| 40 | `end_transaction` |
| 41 | `cancel_transaction` |
| 42 | `undo` |
| 43 | `redo` |
| 44 | `get_transaction_history` |
| 45 | `get_editor_utilities_info` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `modeName` | unknown | No |
| `preferences` | object | No |
| `gridSize` | unknown | No |
| `rotationSnap` | unknown | No |
| `scaleSnap` | unknown | No |
| `path` | unknown | No |
| `assetPath` | unknown | No |
| `collectionName` | unknown | No |
| `collectionType` | unknown | No |
| `assetPaths` | array | No |
| `actorName` | unknown | No |
| `className` | unknown | No |
| `tag` | unknown | No |
| `addToSelection` | unknown | No |
| `groupName` | unknown | No |
| `channelName` | unknown | No |
| `channelType` | unknown | No |
| `profileName` | unknown | No |
| `collisionEnabled` | unknown | No |
| `objectType` | unknown | No |
| `responses` | object | No |
| `materialName` | unknown | No |
| `friction` | unknown | No |
| `staticFriction` | unknown | No |
| `restitution` | unknown | No |
| `density` | unknown | No |
| `surfaceType` | unknown | No |
| `subsystemClass` | unknown | No |
| `parentClass` | unknown | No |
| `timerHandle` | unknown | No |
| `duration` | unknown | No |
| `looping` | unknown | No |
| `functionName` | unknown | No |
| `targetActor` | unknown | No |
| `dispatcherName` | unknown | No |
| `eventName` | unknown | No |
| `blueprintPath` | unknown | No |
| `interfaceName` | unknown | No |
| `functions` | array | No |
| `transactionName` | unknown | No |
| `transactionId` | unknown | No |
| `save` | unknown | No |

---

## manage_gameplay_systems

Targeting, checkpoints, objectives, photo mode, dialogue, HLOD.

### Actions

*50 actions available*

| # | Action |
|---|--------|
| 1 | `create_targeting_component` |
| 2 | `configure_lock_on_target` |
| 3 | `configure_aim_assist` |
| 4 | `create_checkpoint_actor` |
| 5 | `save_checkpoint` |
| 6 | `load_checkpoint` |
| 7 | `create_objective` |
| 8 | `set_objective_state` |
| 9 | `configure_objective_markers` |
| 10 | `create_world_marker` |
| 11 | `create_ping_system` |
| 12 | `configure_marker_widget` |
| 13 | `enable_photo_mode` |
| 14 | `configure_photo_mode_camera` |
| 15 | `take_photo_mode_screenshot` |
| 16 | `create_quest_data_asset` |
| 17 | `create_dialogue_tree` |
| 18 | `add_dialogue_node` |
| 19 | `play_dialogue` |
| 20 | `create_instanced_static_mesh_component` |
| 21 | `create_hierarchical_instanced_static_mesh` |
| 22 | `add_instance` |
| 23 | `remove_instance` |
| 24 | `get_instance_count` |
| 25 | `create_hlod_layer` |
| 26 | `configure_hlod_settings` |
| 27 | `build_hlod` |
| 28 | `assign_actor_to_hlod` |
| 29 | `create_string_table` |
| 30 | `add_string_entry` |
| 31 | `get_string_entry` |
| 32 | `import_localization` |
| 33 | `export_localization` |
| 34 | `set_culture` |
| 35 | `get_available_cultures` |
| 36 | `create_device_profile` |
| 37 | `configure_scalability_group` |
| 38 | `set_quality_level` |
| 39 | `get_scalability_settings` |
| 40 | `set_resolution_scale` |
| 41 | `get_gameplay_systems_info` |
| 42 | `create_objective_chain` |
| 43 | `configure_checkpoint_data` |
| 44 | `create_dialogue_node` |
| 45 | `configure_targeting_priority` |
| 46 | `configure_localization_entry` |
| 47 | `create_quest_stage` |
| 48 | `configure_minimap_icon` |
| 49 | `set_game_state` |
| 50 | `configure_save_system` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `actorName` | unknown | No |
| `componentName` | unknown | No |
| `maxTargetingRange` | unknown | No |
| `targetingConeAngle` | unknown | No |
| `autoTargetNearest` | unknown | No |
| `lockOnRange` | unknown | No |
| `lockOnAngle` | unknown | No |
| `breakLockOnDistance` | unknown | No |
| `stickyLockOn` | unknown | No |
| `lockOnSpeed` | unknown | No |
| `aimAssistStrength` | unknown | No |
| `aimAssistRadius` | unknown | No |
| `magnetismStrength` | unknown | No |
| `bulletMagnetism` | unknown | No |
| `frictionZoneScale` | unknown | No |
| `location` | object | No |
| `rotation` | object | No |
| `checkpointId` | unknown | No |
| `autoActivate` | unknown | No |
| `triggerRadius` | unknown | No |
| `slotName` | unknown | No |
| `playerIndex` | unknown | No |
| `saveWorldState` | unknown | No |
| `objectiveId` | unknown | No |
| `objectiveName` | unknown | No |
| `objectiveType` | unknown | No |
| `initialState` | unknown | No |
| `parentObjectiveId` | unknown | No |
| `trackingType` | unknown | No |
| `state` | unknown | No |
| `progress` | unknown | No |
| `notify` | unknown | No |
| `showOnCompass` | unknown | No |
| `showOnMap` | unknown | No |
| `showInWorld` | unknown | No |
| `markerIcon` | unknown | No |
| `markerColor` | object | No |
| `distanceDisplay` | unknown | No |
| `markerId` | unknown | No |
| `markerType` | unknown | No |
| `iconPath` | unknown | No |
| `label` | unknown | No |
| `color` | object | No |
| `lifetime` | unknown | No |
| `visibleRange` | unknown | No |
| `maxPingsPerPlayer` | unknown | No |
| `pingLifetime` | unknown | No |
| `pingCooldown` | unknown | No |
| `replicatedPings` | unknown | No |
| `contextualPings` | unknown | No |
| `widgetClass` | unknown | No |
| `clampToScreen` | unknown | No |
| `fadeWithDistance` | unknown | No |
| `fadeStartDistance` | unknown | No |
| `fadeEndDistance` | unknown | No |
| `scaleWithDistance` | unknown | No |
| `minScale` | unknown | No |
| `maxScale` | unknown | No |
| `enabled` | unknown | No |
| `pauseGame` | unknown | No |
| `hideUI` | unknown | No |
| `hidePlayer` | unknown | No |
| `allowCameraMovement` | unknown | No |
| `maxCameraDistance` | unknown | No |
| `fov` | unknown | No |
| `aperture` | unknown | No |
| `focalDistance` | unknown | No |
| `depthOfField` | unknown | No |
| `exposure` | unknown | No |
| `contrast` | unknown | No |
| `saturation` | unknown | No |
| `vignetteIntensity` | unknown | No |
| `filmGrain` | unknown | No |
| `filename` | unknown | No |
| `resolution` | unknown | No |
| `superSampling` | unknown | No |
| `includeUI` | unknown | No |
| `assetPath` | unknown | No |
| `questId` | unknown | No |
| `questName` | unknown | No |
| `questType` | unknown | No |
| `prerequisites` | array | No |
| `rewards` | array | No |
| `dialogueName` | unknown | No |
| `startNodeId` | unknown | No |
| `nodeId` | unknown | No |
| `speakerId` | unknown | No |
| `text` | unknown | No |
| `audioAsset` | unknown | No |
| `duration` | unknown | No |
| `choices` | array | No |
| `nextNodeId` | unknown | No |
| `events` | array | No |
| `targetActor` | unknown | No |
| `skipable` | unknown | No |
| `meshPath` | unknown | No |
| `materialPath` | unknown | No |
| `cullDistance` | unknown | No |
| `castShadow` | unknown | No |
| `minLOD` | unknown | No |
| `useGpuLodSelection` | unknown | No |
| `transform` | object | No |
| `instances` | array | No |
| `instanceIndex` | unknown | No |
| `instanceIndices` | array | No |
| `layerName` | unknown | No |
| `cellSize` | unknown | No |
| `loadingRange` | unknown | No |
| `parentLayer` | unknown | No |
| `hlodBuildMethod` | unknown | No |
| `minDrawDistance` | unknown | No |
| `spatiallyLoaded` | unknown | No |
| `alwaysLoaded` | unknown | No |
| `buildAll` | unknown | No |
| `forceRebuild` | unknown | No |
| `tableName` | unknown | No |
| `namespace` | unknown | No |
| `key` | unknown | No |
| `sourceString` | unknown | No |
| `comment` | unknown | No |
| `sourcePath` | unknown | No |
| `targetPath` | unknown | No |
| `outputPath` | unknown | No |
| `culture` | unknown | No |
| `saveToConfig` | unknown | No |
| `profileName` | unknown | No |
| `baseProfile` | unknown | No |
| `deviceType` | unknown | No |
| `cvars` | object | No |
| `groupName` | unknown | No |
| `qualityLevel` | unknown | No |
| `overallQuality` | unknown | No |
| `applyImmediately` | unknown | No |
| `scale` | unknown | No |
| `save` | unknown | No |

---

## manage_gameplay_primitives

Universal gameplay building blocks: state machines, values, factions, zones, conditions, spawners.

### Actions

*62 actions available*

| # | Action |
|---|--------|
| 1 | `create_value_tracker` |
| 2 | `modify_value` |
| 3 | `set_value` |
| 4 | `get_value` |
| 5 | `add_value_threshold` |
| 6 | `configure_value_decay` |
| 7 | `configure_value_regen` |
| 8 | `pause_value_changes` |
| 9 | `create_actor_state_machine` |
| 10 | `add_actor_state` |
| 11 | `add_actor_state_transition` |
| 12 | `set_actor_state` |
| 13 | `get_actor_state` |
| 14 | `configure_state_timer` |
| 15 | `create_faction` |
| 16 | `set_faction_relationship` |
| 17 | `assign_to_faction` |
| 18 | `get_faction` |
| 19 | `modify_reputation` |
| 20 | `get_reputation` |
| 21 | `add_reputation_threshold` |
| 22 | `check_faction_relationship` |
| 23 | `attach_to_socket` |
| 24 | `detach_from_parent` |
| 25 | `transfer_control` |
| 26 | `configure_attachment_rules` |
| 27 | `get_attached_actors` |
| 28 | `get_attachment_parent` |
| 29 | `create_schedule` |
| 30 | `add_schedule_entry` |
| 31 | `set_schedule_active` |
| 32 | `get_current_schedule_entry` |
| 33 | `skip_to_schedule_entry` |
| 34 | `create_world_time` |
| 35 | `set_world_time` |
| 36 | `get_world_time` |
| 37 | `set_time_scale` |
| 38 | `pause_world_time` |
| 39 | `add_time_event` |
| 40 | `get_time_period` |
| 41 | `create_zone` |
| 42 | `set_zone_property` |
| 43 | `get_zone_property` |
| 44 | `get_actor_zone` |
| 45 | `add_zone_enter_event` |
| 46 | `add_zone_exit_event` |
| 47 | `create_condition` |
| 48 | `create_compound_condition` |
| 49 | `evaluate_condition` |
| 50 | `add_condition_listener` |
| 51 | `add_interactable_component` |
| 52 | `configure_interaction` |
| 53 | `set_interaction_enabled` |
| 54 | `get_nearby_interactables` |
| 55 | `focus_interaction` |
| 56 | `execute_interaction` |
| 57 | `create_spawner` |
| 58 | `configure_spawner` |
| 59 | `set_spawner_enabled` |
| 60 | `configure_spawn_conditions` |
| 61 | `despawn_managed_actors` |
| 62 | `get_spawned_count` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `actorName` | unknown | No |
| `componentName` | unknown | No |
| `trackerKey` | unknown | No |
| `value` | unknown | No |
| `delta` | unknown | No |
| `initialValue` | unknown | No |
| `minValue` | unknown | No |
| `maxValue` | unknown | No |
| `threshold` | unknown | No |
| `direction` | unknown | No |
| `rate` | unknown | No |
| `interval` | unknown | No |
| `paused` | unknown | No |
| `stateName` | unknown | No |
| `stateData` | object | No |
| `fromState` | unknown | No |
| `toState` | unknown | No |
| `conditions` | object | No |
| `force` | unknown | No |
| `duration` | unknown | No |
| `autoTransition` | unknown | No |
| `targetState` | unknown | No |
| `factionId` | unknown | No |
| `displayName` | unknown | No |
| `factionA` | unknown | No |
| `factionB` | unknown | No |
| `relationship` | unknown | No |
| `bidirectional` | unknown | No |
| `actorA` | unknown | No |
| `actorB` | unknown | No |
| `childActor` | unknown | No |
| `parentActor` | unknown | No |
| `socketName` | unknown | No |
| `attachRules` | object | No |
| `detachRules` | object | No |
| `newController` | unknown | No |
| `locationRule` | unknown | No |
| `rotationRule` | unknown | No |
| `scaleRule` | unknown | No |
| `recursive` | unknown | No |
| `scheduleId` | unknown | No |
| `startTime` | unknown | No |
| `endTime` | unknown | No |
| `scheduleAction` | unknown | No |
| `location` | object | No |
| `active` | unknown | No |
| `entryIndex` | unknown | No |
| `time` | unknown | No |
| `dayLength` | unknown | No |
| `timeScale` | unknown | No |
| `startPaused` | unknown | No |
| `eventId` | unknown | No |
| `triggerTime` | unknown | No |
| `recurring` | unknown | No |
| `zoneId` | unknown | No |
| `zoneName` | unknown | No |
| `volumeActor` | unknown | No |
| `propertyKey` | unknown | No |
| `propertyValue` | string,number,boolean | No |
| `properties` | object | No |
| `conditionId` | unknown | No |
| `predicate` | object | No |
| `operator` | unknown | No |
| `conditionIds` | array | No |
| `context` | object | No |
| `listenerId` | unknown | No |
| `oneShot` | unknown | No |
| `interactionType` | unknown | No |
| `range` | unknown | No |
| `prompt` | unknown | No |
| `enabled` | unknown | No |
| `filterType` | unknown | No |
| `targetActor` | unknown | No |
| `interactionData` | object | No |
| `spawnClass` | unknown | No |
| `spawnRadius` | unknown | No |
| `maxSpawned` | unknown | No |
| `respawnDelay` | unknown | No |
| `filter` | object | No |
| `color` | object | No |

---

## manage_character_avatar

MetaHuman, Groom/Hair, Mutable, Ready Player Me avatar systems.

### Actions

*60 actions available*

| # | Action |
|---|--------|
| 1 | `import_metahuman` |
| 2 | `spawn_metahuman_actor` |
| 3 | `get_metahuman_component` |
| 4 | `set_body_type` |
| 5 | `set_face_parameter` |
| 6 | `set_skin_tone` |
| 7 | `set_hair_style` |
| 8 | `set_eye_color` |
| 9 | `configure_metahuman_lod` |
| 10 | `enable_body_correctives` |
| 11 | `enable_neck_correctives` |
| 12 | `set_quality_level` |
| 13 | `configure_face_rig` |
| 14 | `set_body_part` |
| 15 | `get_metahuman_info` |
| 16 | `list_available_presets` |
| 17 | `apply_preset` |
| 18 | `export_metahuman_settings` |
| 19 | `create_groom_asset` |
| 20 | `import_groom` |
| 21 | `create_groom_binding` |
| 22 | `spawn_groom_actor` |
| 23 | `attach_groom_to_skeletal_mesh` |
| 24 | `configure_hair_simulation` |
| 25 | `set_hair_width` |
| 26 | `set_hair_root_scale` |
| 27 | `set_hair_tip_scale` |
| 28 | `set_hair_color` |
| 29 | `configure_hair_physics` |
| 30 | `configure_hair_rendering` |
| 31 | `enable_hair_simulation` |
| 32 | `get_groom_info` |
| 33 | `create_customizable_object` |
| 34 | `compile_customizable_object` |
| 35 | `create_customizable_instance` |
| 36 | `set_bool_parameter` |
| 37 | `set_int_parameter` |
| 38 | `set_float_parameter` |
| 39 | `set_color_parameter` |
| 40 | `set_vector_parameter` |
| 41 | `set_texture_parameter` |
| 42 | `set_transform_parameter` |
| 43 | `set_projector_parameter` |
| 44 | `update_skeletal_mesh` |
| 45 | `bake_customizable_instance` |
| 46 | `get_parameter_info` |
| 47 | `get_instance_info` |
| 48 | `spawn_customizable_actor` |
| 49 | `load_avatar_from_url` |
| 50 | `load_avatar_from_glb` |
| 51 | `create_rpm_actor` |
| 52 | `apply_avatar_to_character` |
| 53 | `configure_rpm_materials` |
| 54 | `set_rpm_outfit` |
| 55 | `get_avatar_metadata` |
| 56 | `cache_avatar` |
| 57 | `clear_avatar_cache` |
| 58 | `create_rpm_animation_blueprint` |
| 59 | `retarget_rpm_animation` |
| 60 | `get_rpm_info` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `name` | unknown | No |
| `actorName` | unknown | No |
| `sourcePath` | unknown | No |
| `destinationPath` | unknown | No |
| `location` | object | No |
| `rotation` | object | No |
| `scale` | object | No |
| `metahumanPath` | unknown | No |
| `bodyType` | unknown | No |
| `parameterName` | unknown | No |
| `parameterValue` | unknown | No |
| `skinTone` | unknown | No |
| `hairStylePath` | unknown | No |
| `eyeColor` | object | No |
| `lodLevel` | unknown | No |
| `lodScreenSize` | unknown | No |
| `rigLogicLODThreshold` | unknown | No |
| `enableBodyCorrectives` | unknown | No |
| `enableNeckCorrectives` | unknown | No |
| `qualityLevel` | unknown | No |
| `bodyPartType` | unknown | No |
| `bodyPartPath` | unknown | No |
| `presetName` | unknown | No |
| `exportPath` | unknown | No |
| `groomAssetPath` | unknown | No |
| `groomBindingPath` | unknown | No |
| `skeletalMeshPath` | unknown | No |
| `hairWidth` | unknown | No |
| `hairRootScale` | unknown | No |
| `hairTipScale` | unknown | No |
| `hairColor` | object | No |
| `enableSimulation` | unknown | No |
| `simulationSettings` | object | No |
| `physicsSettings` | object | No |
| `renderingSettings` | object | No |
| `objectPath` | unknown | No |
| `instancePath` | unknown | No |
| `boolValue` | unknown | No |
| `intValue` | unknown | No |
| `floatValue` | unknown | No |
| `colorValue` | object | No |
| `vectorValue` | object | No |
| `texturePath` | unknown | No |
| `transformValue` | object | No |
| `projectorValue` | object | No |
| `bakeOutputPath` | unknown | No |
| `componentIndex` | unknown | No |
| `avatarUrl` | unknown | No |
| `glbPath` | unknown | No |
| `avatarAssetPath` | unknown | No |
| `characterPath` | unknown | No |
| `materialSettings` | object | No |
| `outfitId` | unknown | No |
| `cacheKey` | unknown | No |
| `animBlueprintPath` | unknown | No |
| `sourceAnimationPath` | unknown | No |
| `targetSkeletonPath` | unknown | No |
| `save` | unknown | No |
| `overwrite` | unknown | No |

---

## manage_asset_plugins

Import plugins (USD, Alembic, glTF, Datasmith, Houdini, Substance).

### Actions

*248 actions available*

| # | Action |
|---|--------|
| 1 | `create_interchange_pipeline` |
| 2 | `configure_interchange_pipeline` |
| 3 | `import_with_interchange` |
| 4 | `import_fbx_with_interchange` |
| 5 | `import_obj_with_interchange` |
| 6 | `export_with_interchange` |
| 7 | `set_interchange_translator` |
| 8 | `get_interchange_translators` |
| 9 | `configure_import_asset_type` |
| 10 | `set_interchange_result_container` |
| 11 | `get_interchange_import_result` |
| 12 | `cancel_interchange_import` |
| 13 | `create_interchange_source_data` |
| 14 | `set_interchange_pipeline_stack` |
| 15 | `configure_static_mesh_settings` |
| 16 | `configure_skeletal_mesh_settings` |
| 17 | `configure_animation_settings` |
| 18 | `configure_material_settings` |
| 19 | `create_usd_stage` |
| 20 | `open_usd_stage` |
| 21 | `close_usd_stage` |
| 22 | `get_usd_stage_info` |
| 23 | `create_usd_prim` |
| 24 | `get_usd_prim` |
| 25 | `set_usd_prim_attribute` |
| 26 | `get_usd_prim_attribute` |
| 27 | `add_usd_reference` |
| 28 | `add_usd_payload` |
| 29 | `set_usd_variant` |
| 30 | `create_usd_layer` |
| 31 | `set_edit_target_layer` |
| 32 | `save_usd_stage` |
| 33 | `export_actor_to_usd` |
| 34 | `export_level_to_usd` |
| 35 | `export_static_mesh_to_usd` |
| 36 | `export_skeletal_mesh_to_usd` |
| 37 | `export_material_to_usd` |
| 38 | `export_animation_to_usd` |
| 39 | `enable_usd_live_edit` |
| 40 | `spawn_usd_stage_actor` |
| 41 | `configure_usd_asset_cache` |
| 42 | `get_usd_prim_children` |
| 43 | `import_alembic_file` |
| 44 | `import_alembic_static_mesh` |
| 45 | `import_alembic_skeletal_mesh` |
| 46 | `import_alembic_geometry_cache` |
| 47 | `import_alembic_groom` |
| 48 | `configure_alembic_import_settings` |
| 49 | `set_alembic_sampling_settings` |
| 50 | `set_alembic_compression_type` |
| 51 | `set_alembic_normal_generation` |
| 52 | `reimport_alembic_asset` |
| 53 | `get_alembic_info` |
| 54 | `create_geometry_cache_track` |
| 55 | `play_geometry_cache` |
| 56 | `set_geometry_cache_time` |
| 57 | `export_to_alembic` |
| 58 | `import_gltf` |
| 59 | `import_glb` |
| 60 | `import_gltf_static_mesh` |
| 61 | `import_gltf_skeletal_mesh` |
| 62 | `export_to_gltf` |
| 63 | `export_to_glb` |
| 64 | `export_level_to_gltf` |
| 65 | `export_actor_to_gltf` |
| 66 | `configure_gltf_export_options` |
| 67 | `set_gltf_export_scale` |
| 68 | `set_gltf_texture_format` |
| 69 | `set_draco_compression` |
| 70 | `export_material_to_gltf` |
| 71 | `export_animation_to_gltf` |
| 72 | `get_gltf_export_messages` |
| 73 | `configure_gltf_material_baking` |
| 74 | `import_datasmith_file` |
| 75 | `import_datasmith_cad` |
| 76 | `import_datasmith_revit` |
| 77 | `import_datasmith_sketchup` |
| 78 | `import_datasmith_3dsmax` |
| 79 | `import_datasmith_rhino` |
| 80 | `import_datasmith_solidworks` |
| 81 | `import_datasmith_archicad` |
| 82 | `configure_datasmith_import_options` |
| 83 | `set_datasmith_tessellation_quality` |
| 84 | `reimport_datasmith_scene` |
| 85 | `get_datasmith_scene_info` |
| 86 | `update_datasmith_scene` |
| 87 | `configure_datasmith_lightmap` |
| 88 | `create_datasmith_runtime_actor` |
| 89 | `configure_datasmith_materials` |
| 90 | `export_datasmith_scene` |
| 91 | `sync_datasmith_changes` |
| 92 | `import_speedtree_model` |
| 93 | `import_speedtree_9` |
| 94 | `import_speedtree_atlas` |
| 95 | `configure_speedtree_wind` |
| 96 | `set_speedtree_wind_type` |
| 97 | `set_speedtree_wind_speed` |
| 98 | `configure_speedtree_lod` |
| 99 | `set_speedtree_lod_distances` |
| 100 | `set_speedtree_lod_transition` |
| 101 | `create_speedtree_material` |
| 102 | `configure_speedtree_collision` |
| 103 | `get_speedtree_info` |
| 104 | `connect_to_bridge` |
| 105 | `disconnect_bridge` |
| 106 | `get_bridge_status` |
| 107 | `import_megascan_surface` |
| 108 | `import_megascan_3d_asset` |
| 109 | `import_megascan_3d_plant` |
| 110 | `import_megascan_decal` |
| 111 | `import_megascan_atlas` |
| 112 | `import_megascan_brush` |
| 113 | `search_fab_assets` |
| 114 | `download_fab_asset` |
| 115 | `configure_megascan_import_settings` |
| 116 | `import_hda` |
| 117 | `instantiate_hda` |
| 118 | `spawn_hda_actor` |
| 119 | `get_hda_parameters` |
| 120 | `set_hda_float_parameter` |
| 121 | `set_hda_int_parameter` |
| 122 | `set_hda_bool_parameter` |
| 123 | `set_hda_string_parameter` |
| 124 | `set_hda_color_parameter` |
| 125 | `set_hda_vector_parameter` |
| 126 | `set_hda_ramp_parameter` |
| 127 | `set_hda_multi_parameter` |
| 128 | `cook_hda` |
| 129 | `bake_hda_to_actors` |
| 130 | `bake_hda_to_blueprint` |
| 131 | `configure_hda_input` |
| 132 | `set_hda_world_input` |
| 133 | `set_hda_geometry_input` |
| 134 | `set_hda_curve_input` |
| 135 | `get_hda_outputs` |
| 136 | `get_hda_cook_status` |
| 137 | `connect_to_houdini_session` |
| 138 | `import_sbsar_file` |
| 139 | `create_substance_instance` |
| 140 | `get_substance_parameters` |
| 141 | `set_substance_float_parameter` |
| 142 | `set_substance_int_parameter` |
| 143 | `set_substance_bool_parameter` |
| 144 | `set_substance_color_parameter` |
| 145 | `set_substance_string_parameter` |
| 146 | `set_substance_image_input` |
| 147 | `render_substance_textures` |
| 148 | `get_substance_outputs` |
| 149 | `create_material_from_substance` |
| 150 | `apply_substance_to_material` |
| 151 | `configure_substance_output_size` |
| 152 | `randomize_substance_seed` |
| 153 | `export_substance_textures` |
| 154 | `reimport_sbsar` |
| 155 | `get_substance_graph_info` |
| 156 | `set_substance_output_format` |
| 157 | `batch_render_substances` |
| 158 | `get_asset_plugins_info` |
| 159 | `util_execute_python_script` |
| 160 | `util_execute_python_file` |
| 161 | `util_execute_python_command` |
| 162 | `util_configure_python_paths` |
| 163 | `util_add_python_path` |
| 164 | `util_remove_python_path` |
| 165 | `util_get_python_paths` |
| 166 | `util_create_python_editor_utility` |
| 167 | `util_run_startup_scripts` |
| 168 | `util_get_python_output` |
| 169 | `util_clear_python_output` |
| 170 | `util_is_python_available` |
| 171 | `util_get_python_version` |
| 172 | `util_reload_python_module` |
| 173 | `util_get_python_info` |
| 174 | `util_create_editor_utility_widget` |
| 175 | `util_create_editor_utility_blueprint` |
| 176 | `util_add_menu_entry` |
| 177 | `util_remove_menu_entry` |
| 178 | `util_add_toolbar_button` |
| 179 | `util_remove_toolbar_button` |
| 180 | `util_register_editor_command` |
| 181 | `util_unregister_editor_command` |
| 182 | `util_execute_editor_command` |
| 183 | `util_create_blutility_action` |
| 184 | `util_run_editor_utility` |
| 185 | `util_get_editor_scripting_info` |
| 186 | `util_activate_modeling_tool` |
| 187 | `util_deactivate_modeling_tool` |
| 188 | `util_get_active_tool` |
| 189 | `util_select_mesh_elements` |
| 190 | `util_clear_mesh_selection` |
| 191 | `util_get_mesh_selection` |
| 192 | `util_set_sculpt_brush` |
| 193 | `util_configure_sculpt_brush` |
| 194 | `util_execute_sculpt_stroke` |
| 195 | `util_apply_mesh_operation` |
| 196 | `util_undo_mesh_operation` |
| 197 | `util_accept_tool_result` |
| 198 | `util_cancel_tool` |
| 199 | `util_set_tool_property` |
| 200 | `util_get_tool_properties` |
| 201 | `util_list_available_tools` |
| 202 | `util_enter_modeling_mode` |
| 203 | `util_get_modeling_tools_info` |
| 204 | `util_create_sprite` |
| 205 | `util_create_flipbook` |
| 206 | `util_add_flipbook_keyframe` |
| 207 | `util_create_tile_map` |
| 208 | `util_create_tile_set` |
| 209 | `util_set_tile_map_layer` |
| 210 | `util_spawn_paper_sprite_actor` |
| 211 | `util_spawn_paper_flipbook_actor` |
| 212 | `util_configure_sprite_collision` |
| 213 | `util_configure_sprite_material` |
| 214 | `util_get_sprite_info` |
| 215 | `util_get_paper2d_info` |
| 216 | `util_create_procedural_mesh_component` |
| 217 | `util_create_mesh_section` |
| 218 | `util_update_mesh_section` |
| 219 | `util_clear_mesh_section` |
| 220 | `util_clear_all_mesh_sections` |
| 221 | `util_set_mesh_section_visible` |
| 222 | `util_set_mesh_collision` |
| 223 | `util_set_mesh_vertices` |
| 224 | `util_set_mesh_triangles` |
| 225 | `util_set_mesh_normals` |
| 226 | `util_set_mesh_uvs` |
| 227 | `util_set_mesh_colors` |
| 228 | `util_set_mesh_tangents` |
| 229 | `util_convert_procedural_to_static_mesh` |
| 230 | `util_get_procedural_mesh_info` |
| 231 | `util_create_level_variant_sets` |
| 232 | `util_create_variant_set` |
| 233 | `util_delete_variant_set` |
| 234 | `util_add_variant` |
| 235 | `util_remove_variant` |
| 236 | `util_duplicate_variant` |
| 237 | `util_activate_variant` |
| 238 | `util_deactivate_variant` |
| 239 | `util_get_active_variant` |
| 240 | `util_add_actor_binding` |
| 241 | `util_remove_actor_binding` |
| 242 | `util_capture_property` |
| 243 | `util_configure_variant_dependency` |
| 244 | `util_export_variant_configuration` |
| 245 | `util_get_variant_manager_info` |
| 246 | `util_get_utility_plugins_info` |
| 247 | `util_list_utility_plugins` |
| 248 | `util_get_plugin_status` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `filePath` | unknown | No |
| `destinationPath` | unknown | No |
| `assetPath` | unknown | No |
| `actorName` | unknown | No |
| `pipelineName` | unknown | No |
| `pipelineClass` | unknown | No |
| `translatorClass` | unknown | No |
| `importOptions` | object | No |
| `exportOptions` | object | No |
| `assetType` | unknown | No |
| `rootLayerPath` | unknown | No |
| `primPath` | unknown | No |
| `primType` | unknown | No |
| `attributeName` | unknown | No |
| `attributeValue` | string,number,boolean,object | No |
| `referencePath` | unknown | No |
| `payloadPath` | unknown | No |
| `variantSetName` | unknown | No |
| `variantName` | unknown | No |
| `layerIdentifier` | unknown | No |
| `stageState` | unknown | No |
| `enableLiveEdit` | unknown | No |
| `importType` | unknown | No |
| `samplingType` | unknown | No |
| `frameSteps` | unknown | No |
| `timeSteps` | unknown | No |
| `frameStart` | unknown | No |
| `frameEnd` | unknown | No |
| `recomputeNormals` | unknown | No |
| `compressionType` | unknown | No |
| `exportScale` | unknown | No |
| `textureFormat` | unknown | No |
| `textureQuality` | unknown | No |
| `useDracoCompression` | unknown | No |
| `exportVertexColors` | unknown | No |
| `exportMorphTargets` | unknown | No |
| `bakeMaterialInputs` | unknown | No |
| `tessellationQuality` | unknown | No |
| `lightmapResolution` | unknown | No |
| `importHierarchy` | unknown | No |
| `minLightmapResolution` | unknown | No |
| `modelPath` | unknown | No |
| `windType` | unknown | No |
| `windSpeed` | unknown | No |
| `lodDistances` | array | No |
| `enableCollision` | unknown | No |
| `assetId` | unknown | No |
| `searchQuery` | unknown | No |
| `quality` | unknown | No |
| `applyMaterials` | unknown | No |
| `hdaPath` | unknown | No |
| `parameterName` | unknown | No |
| `parameterValue` | string,number,boolean,array,object | No |
| `inputIndex` | unknown | No |
| `inputActors` | array | No |
| `bakeToBlueprint` | unknown | No |
| `bakeToActors` | unknown | No |
| `cookMode` | unknown | No |
| `sbsarPath` | unknown | No |
| `graphName` | unknown | No |
| `outputSize` | unknown | No |
| `seed` | unknown | No |
| `outputFormat` | unknown | No |
| `parameterValues` | object | No |
| `save` | unknown | No |

---

## manage_livelink

Live Link motion capture: sources, subjects, presets, face tracking.

### Actions

*64 actions available*

| # | Action |
|---|--------|
| 1 | `add_livelink_source` |
| 2 | `remove_livelink_source` |
| 3 | `list_livelink_sources` |
| 4 | `get_source_status` |
| 5 | `get_source_type` |
| 6 | `configure_source_settings` |
| 7 | `add_messagebus_source` |
| 8 | `discover_messagebus_sources` |
| 9 | `remove_all_sources` |
| 10 | `list_livelink_subjects` |
| 11 | `get_subject_role` |
| 12 | `get_subject_state` |
| 13 | `enable_subject` |
| 14 | `disable_subject` |
| 15 | `pause_subject` |
| 16 | `unpause_subject` |
| 17 | `clear_subject_frames` |
| 18 | `get_subject_static_data` |
| 19 | `get_subject_frame_data` |
| 20 | `add_virtual_subject` |
| 21 | `remove_virtual_subject` |
| 22 | `configure_subject_settings` |
| 23 | `get_subject_frame_times` |
| 24 | `get_subjects_by_role` |
| 25 | `create_livelink_preset` |
| 26 | `load_livelink_preset` |
| 27 | `apply_livelink_preset` |
| 28 | `add_preset_to_client` |
| 29 | `build_preset_from_client` |
| 30 | `save_livelink_preset` |
| 31 | `get_preset_sources` |
| 32 | `get_preset_subjects` |
| 33 | `add_livelink_controller` |
| 34 | `configure_livelink_controller` |
| 35 | `set_controller_subject` |
| 36 | `set_controller_role` |
| 37 | `enable_controller_evaluation` |
| 38 | `disable_controller_evaluation` |
| 39 | `set_controlled_component` |
| 40 | `get_controller_info` |
| 41 | `configure_livelink_timecode` |
| 42 | `set_timecode_provider` |
| 43 | `get_livelink_timecode` |
| 44 | `configure_time_sync` |
| 45 | `set_buffer_settings` |
| 46 | `configure_frame_interpolation` |
| 47 | `configure_face_source` |
| 48 | `configure_arkit_mapping` |
| 49 | `set_face_neutral_pose` |
| 50 | `get_face_blendshapes` |
| 51 | `configure_blendshape_remap` |
| 52 | `apply_face_to_skeletal_mesh` |
| 53 | `configure_face_retargeting` |
| 54 | `get_face_tracking_status` |
| 55 | `configure_skeleton_mapping` |
| 56 | `create_retarget_asset` |
| 57 | `configure_bone_mapping` |
| 58 | `configure_curve_mapping` |
| 59 | `apply_mocap_to_character` |
| 60 | `get_skeleton_mapping_info` |
| 61 | `get_livelink_info` |
| 62 | `list_available_roles` |
| 63 | `list_source_factories` |
| 64 | `force_livelink_tick` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `sourceGuid` | unknown | No |
| `sourceType` | unknown | No |
| `sourceName` | unknown | No |
| `connectionString` | unknown | No |
| `subjectName` | unknown | No |
| `subjectKey` | object | No |
| `roleName` | unknown | No |
| `presetPath` | unknown | No |
| `presetName` | unknown | No |
| `recreateExisting` | unknown | No |
| `actorName` | unknown | No |
| `componentName` | unknown | No |
| `controllerClass` | unknown | No |
| `updateInEditor` | unknown | No |
| `disableWhenSpawnable` | unknown | No |
| `sourceSettings` | object | No |
| `subjectSettings` | object | No |
| `discoveryTimeout` | unknown | No |
| `machineAddress` | unknown | No |
| `virtualSubjectClass` | unknown | No |
| `timecodeSettings` | object | No |
| `bufferSettings` | object | No |
| `interpolationSettings` | object | No |
| `blendshapeMapping` | object | No |
| `neutralPose` | object | No |
| `faceRemapAsset` | unknown | No |
| `skeletalMeshPath` | unknown | No |
| `retargetAssetPath` | unknown | No |
| `boneMapping` | object | No |
| `curveMapping` | object | No |
| `targetSkeleton` | unknown | No |
| `sourceSkeleton` | unknown | No |
| `characterActor` | unknown | No |
| `includeDisabledSubjects` | unknown | No |
| `includeVirtualSubjects` | unknown | No |
| `save` | unknown | No |

---

## manage_xr

XR (VR/AR/MR) + Virtual Production (nDisplay, Composure, DMX).

### Actions

*272 actions available*

| # | Action |
|---|--------|
| 1 | `get_openxr_info` |
| 2 | `configure_openxr_settings` |
| 3 | `set_tracking_origin` |
| 4 | `get_tracking_origin` |
| 5 | `create_xr_action_set` |
| 6 | `add_xr_action` |
| 7 | `bind_xr_action` |
| 8 | `get_xr_action_state` |
| 9 | `trigger_haptic_feedback` |
| 10 | `stop_haptic_feedback` |
| 11 | `get_hmd_pose` |
| 12 | `get_controller_pose` |
| 13 | `get_hand_tracking_data` |
| 14 | `enable_hand_tracking` |
| 15 | `disable_hand_tracking` |
| 16 | `get_eye_tracking_data` |
| 17 | `enable_eye_tracking` |
| 18 | `get_view_configuration` |
| 19 | `set_render_scale` |
| 20 | `get_supported_extensions` |
| 21 | `get_quest_info` |
| 22 | `configure_quest_settings` |
| 23 | `enable_passthrough` |
| 24 | `disable_passthrough` |
| 25 | `configure_passthrough_style` |
| 26 | `enable_scene_capture` |
| 27 | `get_scene_anchors` |
| 28 | `get_room_layout` |
| 29 | `enable_quest_hand_tracking` |
| 30 | `get_quest_hand_pose` |
| 31 | `enable_quest_face_tracking` |
| 32 | `get_quest_face_state` |
| 33 | `enable_quest_eye_tracking` |
| 34 | `get_quest_eye_gaze` |
| 35 | `enable_quest_body_tracking` |
| 36 | `get_quest_body_state` |
| 37 | `create_spatial_anchor` |
| 38 | `save_spatial_anchor` |
| 39 | `load_spatial_anchors` |
| 40 | `delete_spatial_anchor` |
| 41 | `configure_guardian_bounds` |
| 42 | `get_guardian_geometry` |
| 43 | `get_steamvr_info` |
| 44 | `configure_steamvr_settings` |
| 45 | `configure_chaperone_bounds` |
| 46 | `get_chaperone_geometry` |
| 47 | `create_steamvr_overlay` |
| 48 | `set_overlay_texture` |
| 49 | `show_overlay` |
| 50 | `hide_overlay` |
| 51 | `destroy_overlay` |
| 52 | `get_tracked_device_count` |
| 53 | `get_tracked_device_info` |
| 54 | `get_lighthouse_info` |
| 55 | `trigger_steamvr_haptic` |
| 56 | `get_steamvr_action_manifest` |
| 57 | `set_steamvr_action_manifest` |
| 58 | `enable_steamvr_skeletal_input` |
| 59 | `get_skeletal_bone_data` |
| 60 | `configure_steamvr_render` |
| 61 | `get_arkit_info` |
| 62 | `configure_arkit_session` |
| 63 | `start_arkit_session` |
| 64 | `pause_arkit_session` |
| 65 | `configure_world_tracking` |
| 66 | `get_tracked_planes` |
| 67 | `get_tracked_images` |
| 68 | `add_reference_image` |
| 69 | `enable_people_occlusion` |
| 70 | `disable_people_occlusion` |
| 71 | `enable_arkit_face_tracking` |
| 72 | `get_arkit_face_blendshapes` |
| 73 | `get_arkit_face_geometry` |
| 74 | `enable_body_tracking` |
| 75 | `get_body_skeleton` |
| 76 | `create_arkit_anchor` |
| 77 | `remove_arkit_anchor` |
| 78 | `get_light_estimation` |
| 79 | `enable_scene_reconstruction` |
| 80 | `get_scene_mesh` |
| 81 | `perform_raycast` |
| 82 | `get_camera_intrinsics` |
| 83 | `get_arcore_info` |
| 84 | `configure_arcore_session` |
| 85 | `start_arcore_session` |
| 86 | `pause_arcore_session` |
| 87 | `get_arcore_planes` |
| 88 | `get_arcore_points` |
| 89 | `create_arcore_anchor` |
| 90 | `remove_arcore_anchor` |
| 91 | `enable_depth_api` |
| 92 | `get_depth_image` |
| 93 | `enable_geospatial` |
| 94 | `get_geospatial_pose` |
| 95 | `create_geospatial_anchor` |
| 96 | `resolve_cloud_anchor` |
| 97 | `host_cloud_anchor` |
| 98 | `enable_arcore_augmented_images` |
| 99 | `get_arcore_light_estimate` |
| 100 | `perform_arcore_raycast` |
| 101 | `get_varjo_info` |
| 102 | `configure_varjo_settings` |
| 103 | `enable_varjo_passthrough` |
| 104 | `disable_varjo_passthrough` |
| 105 | `configure_varjo_depth_test` |
| 106 | `enable_varjo_eye_tracking` |
| 107 | `get_varjo_gaze_data` |
| 108 | `calibrate_varjo_eye_tracking` |
| 109 | `enable_foveated_rendering` |
| 110 | `configure_foveated_rendering` |
| 111 | `enable_varjo_mixed_reality` |
| 112 | `configure_varjo_chroma_key` |
| 113 | `get_varjo_camera_intrinsics` |
| 114 | `enable_varjo_depth_estimation` |
| 115 | `get_varjo_environment_cubemap` |
| 116 | `configure_varjo_markers` |
| 117 | `get_hololens_info` |
| 118 | `configure_hololens_settings` |
| 119 | `enable_spatial_mapping` |
| 120 | `disable_spatial_mapping` |
| 121 | `get_spatial_mesh` |
| 122 | `configure_spatial_mapping_quality` |
| 123 | `enable_scene_understanding` |
| 124 | `get_scene_objects` |
| 125 | `enable_qr_tracking` |
| 126 | `get_tracked_qr_codes` |
| 127 | `create_world_anchor` |
| 128 | `save_world_anchor` |
| 129 | `load_world_anchors` |
| 130 | `enable_hololens_hand_tracking` |
| 131 | `get_hololens_hand_mesh` |
| 132 | `enable_hololens_eye_tracking` |
| 133 | `get_hololens_gaze_ray` |
| 134 | `register_voice_command` |
| 135 | `unregister_voice_command` |
| 136 | `get_registered_voice_commands` |
| 137 | `get_xr_system_info` |
| 138 | `list_xr_devices` |
| 139 | `set_xr_device_priority` |
| 140 | `reset_xr_orientation` |
| 141 | `configure_xr_spectator` |
| 142 | `get_xr_runtime_name` |
| 143 | `vp_create_ndisplay_config` |
| 144 | `vp_add_cluster_node` |
| 145 | `vp_remove_cluster_node` |
| 146 | `vp_add_viewport` |
| 147 | `vp_remove_viewport` |
| 148 | `vp_set_viewport_camera` |
| 149 | `vp_configure_viewport_region` |
| 150 | `vp_set_projection_policy` |
| 151 | `vp_configure_warp_blend` |
| 152 | `vp_list_cluster_nodes` |
| 153 | `vp_create_led_wall` |
| 154 | `vp_configure_led_wall_size` |
| 155 | `vp_configure_icvfx_camera` |
| 156 | `vp_add_icvfx_camera` |
| 157 | `vp_remove_icvfx_camera` |
| 158 | `vp_configure_inner_frustum` |
| 159 | `vp_configure_outer_viewport` |
| 160 | `vp_set_chromakey_settings` |
| 161 | `vp_configure_light_cards` |
| 162 | `vp_set_stage_settings` |
| 163 | `vp_set_sync_policy` |
| 164 | `vp_configure_genlock` |
| 165 | `vp_set_primary_node` |
| 166 | `vp_configure_network_settings` |
| 167 | `vp_get_ndisplay_info` |
| 168 | `vp_create_composure_element` |
| 169 | `vp_delete_composure_element` |
| 170 | `vp_add_composure_layer` |
| 171 | `vp_remove_composure_layer` |
| 172 | `vp_attach_child_layer` |
| 173 | `vp_detach_child_layer` |
| 174 | `vp_add_input_pass` |
| 175 | `vp_add_transform_pass` |
| 176 | `vp_add_output_pass` |
| 177 | `vp_configure_chroma_keyer` |
| 178 | `vp_bind_render_target` |
| 179 | `vp_get_composure_info` |
| 180 | `vp_create_ocio_config` |
| 181 | `vp_load_ocio_config` |
| 182 | `vp_get_ocio_colorspaces` |
| 183 | `vp_get_ocio_displays` |
| 184 | `vp_set_display_view` |
| 185 | `vp_add_colorspace_transform` |
| 186 | `vp_apply_ocio_look` |
| 187 | `vp_configure_viewport_ocio` |
| 188 | `vp_set_ocio_working_colorspace` |
| 189 | `vp_get_ocio_info` |
| 190 | `vp_create_remote_control_preset` |
| 191 | `vp_load_remote_control_preset` |
| 192 | `vp_expose_property` |
| 193 | `vp_unexpose_property` |
| 194 | `vp_expose_function` |
| 195 | `vp_create_controller` |
| 196 | `vp_bind_controller` |
| 197 | `vp_get_exposed_properties` |
| 198 | `vp_set_exposed_property_value` |
| 199 | `vp_get_exposed_property_value` |
| 200 | `vp_start_web_server` |
| 201 | `vp_stop_web_server` |
| 202 | `vp_get_web_server_status` |
| 203 | `vp_create_layout_group` |
| 204 | `vp_get_remote_control_info` |
| 205 | `vp_create_dmx_library` |
| 206 | `vp_import_gdtf` |
| 207 | `vp_create_fixture_type` |
| 208 | `vp_add_fixture_mode` |
| 209 | `vp_add_fixture_function` |
| 210 | `vp_create_fixture_patch` |
| 211 | `vp_assign_fixture_to_universe` |
| 212 | `vp_configure_dmx_port` |
| 213 | `vp_create_artnet_port` |
| 214 | `vp_create_sacn_port` |
| 215 | `vp_send_dmx` |
| 216 | `vp_receive_dmx` |
| 217 | `vp_set_fixture_channel_value` |
| 218 | `vp_get_fixture_channel_value` |
| 219 | `vp_add_dmx_component` |
| 220 | `vp_configure_dmx_component` |
| 221 | `vp_list_dmx_universes` |
| 222 | `vp_list_dmx_fixtures` |
| 223 | `vp_create_dmx_sequencer_track` |
| 224 | `vp_get_dmx_info` |
| 225 | `vp_create_osc_server` |
| 226 | `vp_stop_osc_server` |
| 227 | `vp_create_osc_client` |
| 228 | `send_osc_message` |
| 229 | `send_osc_bundle` |
| 230 | `bind_osc_address` |
| 231 | `unbind_osc_address` |
| 232 | `bind_osc_to_property` |
| 233 | `list_osc_servers` |
| 234 | `list_osc_clients` |
| 235 | `configure_osc_dispatcher` |
| 236 | `get_osc_info` |
| 237 | `list_midi_devices` |
| 238 | `open_midi_input` |
| 239 | `close_midi_input` |
| 240 | `open_midi_output` |
| 241 | `close_midi_output` |
| 242 | `send_midi_note_on` |
| 243 | `send_midi_note_off` |
| 244 | `send_midi_cc` |
| 245 | `send_midi_pitch_bend` |
| 246 | `send_midi_program_change` |
| 247 | `bind_midi_to_property` |
| 248 | `unbind_midi` |
| 249 | `configure_midi_learn` |
| 250 | `add_midi_device_component` |
| 251 | `get_midi_info` |
| 252 | `create_timecode_provider` |
| 253 | `set_timecode_provider` |
| 254 | `get_current_timecode` |
| 255 | `set_frame_rate` |
| 256 | `configure_ltc_timecode` |
| 257 | `configure_aja_timecode` |
| 258 | `configure_blackmagic_timecode` |
| 259 | `configure_system_time_timecode` |
| 260 | `enable_timecode_genlock` |
| 261 | `disable_timecode_genlock` |
| 262 | `set_custom_timestep` |
| 263 | `configure_genlock_source` |
| 264 | `get_timecode_provider_status` |
| 265 | `synchronize_timecode` |
| 266 | `create_timecode_synchronizer` |
| 267 | `add_timecode_source` |
| 268 | `list_timecode_providers` |
| 269 | `get_timecode_info` |
| 270 | `get_virtual_production_info` |
| 271 | `list_active_vp_sessions` |
| 272 | `reset_vp_state` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `name` | unknown | No |
| `actorName` | unknown | No |
| `assetPath` | unknown | No |
| `configPath` | unknown | No |
| `nodeName` | unknown | No |
| `nodeId` | unknown | No |
| `viewportId` | unknown | No |
| `viewportName` | unknown | No |
| `cameraComponent` | unknown | No |
| `projectionPolicy` | unknown | No |
| `projectionMesh` | unknown | No |
| `mpcdiFilePath` | unknown | No |
| `mpcdiBufferId` | unknown | No |
| `mpcdiRegionId` | unknown | No |
| `viewportRegion` | object | No |
| `hostAddress` | unknown | No |
| `hostPort` | unknown | No |
| `isPrimary` | unknown | No |
| `syncPolicy` | unknown | No |
| `swapSyncType` | unknown | No |
| `ledWallSize` | object | No |
| `icvfxCameraName` | unknown | No |
| `innerFrustumSettings` | object | No |
| `chromakeySettings` | object | No |
| `lightCardActor` | unknown | No |
| `elementName` | unknown | No |
| `elementClass` | unknown | No |
| `parentElement` | unknown | No |
| `childElement` | unknown | No |
| `passType` | unknown | No |
| `transformPassClass` | unknown | No |
| `chromaKeyerSettings` | object | No |
| `renderTargetPath` | unknown | No |
| `ocioConfigPath` | unknown | No |
| `sourceColorspace` | unknown | No |
| `destColorspace` | unknown | No |
| `displayName` | unknown | No |
| `viewName` | unknown | No |
| `lookName` | unknown | No |
| `workingColorspace` | unknown | No |
| `presetName` | unknown | No |
| `presetPath` | unknown | No |
| `propertyPath` | unknown | No |
| `propertyLabel` | unknown | No |
| `functionPath` | unknown | No |
| `controllerType` | unknown | No |
| `controllerSettings` | object | No |
| `webServerPort` | unknown | No |
| `layoutGroupName` | unknown | No |
| `libraryPath` | unknown | No |
| `gdtfFilePath` | unknown | No |
| `fixtureTypeName` | unknown | No |
| `fixtureMode` | unknown | No |
| `channelCount` | unknown | No |
| `fixtureFunction` | object | No |
| `patchName` | unknown | No |
| `fixtureId` | unknown | No |
| `universeId` | unknown | No |
| `startingChannel` | unknown | No |
| `portType` | unknown | No |
| `protocol` | unknown | No |
| `networkInterface` | unknown | No |
| `dmxData` | array | No |
| `channelIndex` | unknown | No |
| `channelValue` | unknown | No |
| `sequencePath` | unknown | No |
| `oscServerName` | unknown | No |
| `oscClientName` | unknown | No |
| `ipAddress` | unknown | No |
| `port` | unknown | No |
| `oscAddress` | unknown | No |
| `oscArgs` | array | No |
| `targetProperty` | unknown | No |
| `multicast` | unknown | No |
| `loopback` | unknown | No |
| `midiDeviceId` | unknown | No |
| `midiDeviceName` | unknown | No |
| `midiChannel` | unknown | No |
| `noteNumber` | unknown | No |
| `velocity` | unknown | No |
| `controlNumber` | unknown | No |
| `controlValue` | unknown | No |
| `pitchBendValue` | unknown | No |
| `programNumber` | unknown | No |
| `midiLearnEnabled` | unknown | No |
| `timecodeProviderType` | unknown | No |
| `frameRate` | unknown | No |
| `ltcSettings` | object | No |
| `ajaSettings` | object | No |
| `blackmagicSettings` | object | No |
| `genlockEnabled` | unknown | No |
| `genlockSource` | unknown | No |
| `customTimestepClass` | unknown | No |
| `synchronizerId` | unknown | No |
| `save` | unknown | No |

---

## manage_accessibility

Accessibility: colorblind, subtitles, audio, motor, cognitive.

### Actions

*50 actions available*

| # | Action |
|---|--------|
| 1 | `create_colorblind_filter` |
| 2 | `configure_colorblind_mode` |
| 3 | `set_colorblind_severity` |
| 4 | `configure_high_contrast_mode` |
| 5 | `set_high_contrast_colors` |
| 6 | `set_ui_scale` |
| 7 | `configure_text_to_speech` |
| 8 | `set_font_size` |
| 9 | `configure_screen_reader` |
| 10 | `set_visual_accessibility_preset` |
| 11 | `create_subtitle_widget` |
| 12 | `configure_subtitle_style` |
| 13 | `set_subtitle_font_size` |
| 14 | `configure_subtitle_background` |
| 15 | `configure_speaker_identification` |
| 16 | `add_directional_indicators` |
| 17 | `configure_subtitle_timing` |
| 18 | `set_subtitle_preset` |
| 19 | `configure_mono_audio` |
| 20 | `configure_audio_visualization` |
| 21 | `create_sound_indicator_widget` |
| 22 | `configure_visual_sound_cues` |
| 23 | `set_audio_ducking` |
| 24 | `configure_screen_narrator` |
| 25 | `set_audio_balance` |
| 26 | `set_audio_accessibility_preset` |
| 27 | `configure_control_remapping` |
| 28 | `create_control_remapping_ui` |
| 29 | `configure_hold_vs_toggle` |
| 30 | `configure_auto_aim_strength` |
| 31 | `configure_one_handed_mode` |
| 32 | `set_input_timing_tolerance` |
| 33 | `configure_button_holds` |
| 34 | `configure_quick_time_events` |
| 35 | `set_cursor_size` |
| 36 | `set_motor_accessibility_preset` |
| 37 | `configure_difficulty_presets` |
| 38 | `configure_objective_reminders` |
| 39 | `configure_navigation_assistance` |
| 40 | `configure_motion_sickness_options` |
| 41 | `set_game_speed` |
| 42 | `configure_tutorial_options` |
| 43 | `configure_ui_simplification` |
| 44 | `set_cognitive_accessibility_preset` |
| 45 | `create_accessibility_preset` |
| 46 | `apply_accessibility_preset` |
| 47 | `export_accessibility_settings` |
| 48 | `import_accessibility_settings` |
| 49 | `get_accessibility_info` |
| 50 | `reset_accessibility_defaults` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `actorName` | unknown | No |
| `assetPath` | unknown | No |
| `assetName` | unknown | No |
| `widgetName` | unknown | No |
| `presetName` | unknown | No |
| `save` | unknown | No |
| `colorblindMode` | unknown | No |
| `colorblindSeverity` | unknown | No |
| `highContrastEnabled` | unknown | No |
| `highContrastColors` | object | No |
| `uiScale` | unknown | No |
| `textToSpeechEnabled` | unknown | No |
| `textToSpeechRate` | unknown | No |
| `textToSpeechVolume` | unknown | No |
| `fontSize` | unknown | No |
| `fontSizeMultiplier` | unknown | No |
| `screenReaderEnabled` | unknown | No |
| `subtitleEnabled` | unknown | No |
| `subtitleFontSize` | unknown | No |
| `subtitleFontFamily` | unknown | No |
| `subtitleColor` | unknown | No |
| `subtitleBackgroundEnabled` | unknown | No |
| `subtitleBackgroundColor` | unknown | No |
| `subtitleBackgroundOpacity` | unknown | No |
| `speakerIdentificationEnabled` | unknown | No |
| `speakerColorCodingEnabled` | unknown | No |
| `directionalIndicatorsEnabled` | unknown | No |
| `subtitleDisplayTime` | unknown | No |
| `subtitlePosition` | unknown | No |
| `monoAudioEnabled` | unknown | No |
| `audioVisualizationEnabled` | unknown | No |
| `visualSoundCuesEnabled` | unknown | No |
| `soundIndicatorPosition` | unknown | No |
| `audioDuckingEnabled` | unknown | No |
| `audioDuckingAmount` | unknown | No |
| `screenNarratorEnabled` | unknown | No |
| `audioBalance` | unknown | No |
| `holdToToggleEnabled` | unknown | No |
| `autoAimEnabled` | unknown | No |
| `autoAimStrength` | unknown | No |
| `oneHandedModeEnabled` | unknown | No |
| `oneHandedModeHand` | unknown | No |
| `inputTimingTolerance` | unknown | No |
| `buttonHoldTime` | unknown | No |
| `qteTimeMultiplier` | unknown | No |
| `qteAutoComplete` | unknown | No |
| `cursorSize` | unknown | No |
| `cursorHighContrastEnabled` | unknown | No |
| `difficultyPreset` | unknown | No |
| `objectiveRemindersEnabled` | unknown | No |
| `objectiveReminderInterval` | unknown | No |
| `navigationAssistanceEnabled` | unknown | No |
| `navigationAssistanceType` | unknown | No |
| `motionSicknessReductionEnabled` | unknown | No |
| `cameraShakeEnabled` | unknown | No |
| `headBobEnabled` | unknown | No |
| `motionBlurEnabled` | unknown | No |
| `fovAdjustment` | unknown | No |
| `gameSpeedMultiplier` | unknown | No |
| `tutorialHintsEnabled` | unknown | No |
| `simplifiedUIEnabled` | unknown | No |
| `actionName` | unknown | No |
| `newBinding` | unknown | No |
| `inputMappingContext` | unknown | No |
| `presetPath` | unknown | No |
| `exportPath` | unknown | No |
| `importPath` | unknown | No |
| `exportFormat` | unknown | No |

---

## manage_ui

Runtime UI management: spawn widgets, hierarchy, viewport control.

### Actions

*7 actions available*

| # | Action |
|---|--------|
| 1 | `create_widget` |
| 2 | `remove_widget_from_viewport` |
| 3 | `get_all_widgets` |
| 4 | `get_widget_hierarchy` |
| 5 | `set_input_mode` |
| 6 | `show_mouse_cursor` |
| 7 | `set_widget_visibility` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `widgetPath` | unknown | No |
| `addToViewport` | unknown | No |
| `zOrder` | unknown | No |
| `key` | unknown | No |
| `visible` | unknown | No |
| `showCursor` | unknown | No |
| `inputMode` | unknown | No |

---

## manage_gameplay_abilities

Create and configure Gameplay Abilities, Effects, and Ability Tasks.

### Actions

*18 actions available*

| # | Action |
|---|--------|
| 1 | `create_gameplay_ability` |
| 2 | `set_ability_tags` |
| 3 | `set_ability_costs` |
| 4 | `set_ability_cooldown` |
| 5 | `set_ability_targeting` |
| 6 | `add_ability_task` |
| 7 | `set_activation_policy` |
| 8 | `set_instancing_policy` |
| 9 | `create_gameplay_effect` |
| 10 | `set_effect_duration` |
| 11 | `add_effect_modifier` |
| 12 | `set_modifier_magnitude` |
| 13 | `add_effect_execution_calculation` |
| 14 | `add_effect_cue` |
| 15 | `set_effect_stacking` |
| 16 | `set_effect_tags` |
| 17 | `add_tag_to_asset` |
| 18 | `get_gas_info` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `name` | unknown | No |
| `path` | unknown | No |
| `blueprintPath` | unknown | No |
| `assetPath` | unknown | No |
| `abilityPath` | unknown | No |
| `abilityTags` | array | No |
| `cancelAbilitiesWithTags` | array | No |
| `blockAbilitiesWithTags` | array | No |
| `costEffectPath` | unknown | No |
| `cooldownEffectPath` | unknown | No |
| `targetingType` | unknown | No |
| `targetingRange` | unknown | No |
| `requiresLineOfSight` | unknown | No |
| `targetingAngle` | unknown | No |
| `taskType` | unknown | No |
| `taskClassName` | unknown | No |
| `policy` | unknown | No |
| `effectPath` | unknown | No |
| `durationType` | unknown | No |
| `duration` | unknown | No |
| `operation` | unknown | No |
| `magnitude` | unknown | No |
| `attributeName` | unknown | No |
| `modifierIndex` | unknown | No |
| `magnitudeType` | unknown | No |
| `value` | unknown | No |
| `calculationClass` | unknown | No |
| `cueTag` | unknown | No |
| `stackingType` | unknown | No |
| `stackLimit` | unknown | No |
| `grantedTags` | array | No |
| `tag` | unknown | No |
| `tagName` | unknown | No |

---

## manage_attribute_sets

Create Blueprint AttributeSets and add Ability System Components.

### Actions

*6 actions available*

| # | Action |
|---|--------|
| 1 | `add_ability_system_component` |
| 2 | `configure_asc` |
| 3 | `create_attribute_set` |
| 4 | `add_attribute` |
| 5 | `set_attribute_base_value` |
| 6 | `set_attribute_clamping` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `blueprintPath` | unknown | No |
| `name` | unknown | No |
| `path` | unknown | No |
| `componentName` | unknown | No |
| `replicationMode` | unknown | No |
| `attributeSetPath` | unknown | No |
| `attributeName` | unknown | No |
| `defaultValue` | unknown | No |
| `baseValue` | unknown | No |
| `minValue` | unknown | No |
| `maxValue` | unknown | No |

---

## manage_gameplay_cues

Create and configure Gameplay Cue Notifies (Static/Actor).

### Actions

*3 actions available*

| # | Action |
|---|--------|
| 1 | `create_gameplay_cue_notify` |
| 2 | `configure_cue_trigger` |
| 3 | `set_cue_effects` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `name` | unknown | No |
| `path` | unknown | No |
| `cueType` | unknown | No |
| `cueTag` | unknown | No |
| `cuePath` | unknown | No |
| `triggerType` | unknown | No |
| `particleSystem` | unknown | No |
| `sound` | unknown | No |
| `cameraShake` | unknown | No |
| `blueprintPath` | unknown | No |

---

## test_gameplay_abilities

Runtime testing of GAS: Activate abilities, apply effects, query attributes.

### Actions

*4 actions available*

| # | Action |
|---|--------|
| 1 | `test_activate_ability` |
| 2 | `test_apply_effect` |
| 3 | `test_get_attribute` |
| 4 | `test_get_gameplay_tags` |

### Input Parameters

| Parameter | Type | Required |
|-----------|------|----------|
| `actorName` | unknown | No |
| `actorLabel` | unknown | No |
| `abilityClass` | unknown | No |
| `effectClass` | unknown | No |
| `attributeName` | unknown | No |
| `attributeSetClass` | unknown | No |
| `payload` | object | No |

---

## Generation Metadata

```json
{
  "generatedAt": "2026-01-21T16:53:16.491Z",
  "serverCommand": "node",
  "serverArgs": [
    "X:\\Newfolder(2)\\MCP\\Unreal 5.6\\Unreal_mcp\\dist\\cli.js"
  ],
  "toolCount": 36,
  "actionCount": 2575,
  "mockedConnection": true
}
```
