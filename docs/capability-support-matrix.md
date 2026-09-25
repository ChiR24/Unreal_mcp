<!-- GENERATED FILE - DO NOT EDIT.
     Regenerate with `npm run registry:generate`; `npm run registry:check` gates drift.
     Source of truth: src/tools/catalog/capabilities/records/**
     Claims are elevated only by src/tools/catalog/capabilities/records/semantics/evidence-ledger.ts -->

# Capability preview / undo / compensation support matrix

Catalog revision: `d1e25aa33a361186`

Every one of the 389 capabilities declares all three semantics. The
default on each axis is the pessimistic one (no preview, not undoable, no
compensation); a capability carries a stronger claim only where the ledger
cites the implementation that proves it. A mostly-pessimistic matrix is the
truthful result, not a gap in coverage.

## Coverage

| Axis | All capabilities | Mutations only |
| --- | --- | --- |
| Previewable | 0 / 389 | 0 / 304 |
| Undoable | 0 / 389 | 0 / 304 |
| Compensatable | 8 / 389 | 7 / 304 |
| Fully pessimistic | 381 / 389 | 297 / 304 |

## By parent tool

| Parent | Capabilities | Mutations | Preview | Undo | Compensation |
| --- | --- | --- | --- | --- | --- |
| `animation_physics` | 28 | 25 | 0 | 0 | 0 |
| `build_environment` | 40 | 35 | 0 | 0 | 1 |
| `control_actor` | 22 | 14 | 0 | 0 | 2 |
| `control_editor` | 21 | 13 | 0 | 0 | 2 |
| `inspect` | 16 | 5 | 0 | 0 | 0 |
| `manage_ai` | 16 | 14 | 0 | 0 | 0 |
| `manage_asset` | 46 | 37 | 0 | 0 | 0 |
| `manage_audio` | 12 | 11 | 0 | 0 | 0 |
| `manage_blueprint` | 27 | 22 | 0 | 0 | 0 |
| `manage_character` | 10 | 8 | 0 | 0 | 0 |
| `manage_combat` | 5 | 4 | 0 | 0 | 0 |
| `manage_effect` | 13 | 11 | 0 | 0 | 0 |
| `manage_gas` | 8 | 7 | 0 | 0 | 0 |
| `manage_geometry` | 15 | 13 | 0 | 0 | 0 |
| `manage_interaction` | 4 | 3 | 0 | 0 | 0 |
| `manage_inventory` | 8 | 7 | 0 | 0 | 0 |
| `manage_level` | 17 | 13 | 0 | 0 | 0 |
| `manage_level_structure` | 8 | 6 | 0 | 0 | 0 |
| `manage_networking` | 20 | 15 | 0 | 0 | 0 |
| `manage_pcg` | 3 | 3 | 0 | 0 | 0 |
| `manage_sequence` | 19 | 17 | 0 | 0 | 0 |
| `manage_tools` | 8 | 5 | 0 | 0 | 2 |
| `system_control` | 23 | 16 | 0 | 0 | 1 |

## Capabilities with an earned (non-pessimistic) declaration

| Capability | Axis | Declaration | Evidence grade | Evidence |
| --- | --- | --- | --- | --- |
| `build_environment.create_landscape` | compensation | inverse: build_environment.delete | contract-derived | catalogue-verified inverse capability |
| `control_actor.duplicate` | compensation | inverse: control_actor.delete | contract-derived | catalogue-verified inverse capability |
| `control_actor.spawn` | compensation | inverse: control_actor.delete | contract-derived | catalogue-verified inverse capability |
| `control_editor.open_asset` | compensation | inverse: control_editor.close_asset | contract-derived | catalogue-verified inverse capability |
| `control_editor.start_recording` | compensation | inverse: control_editor.stop_recording | contract-derived | catalogue-verified inverse capability |
| `manage_tools.enable_category` | compensation | inverse: manage_tools.disable_category | contract-derived | catalogue-verified inverse capability |
| `manage_tools.enable_tools` | compensation | inverse: manage_tools.disable_tools | contract-derived | catalogue-verified inverse capability |
| `system_control.start_session` | compensation | inverse: system_control.stop_session | contract-derived | catalogue-verified inverse capability |

## Full matrix

| Capability | Effect | Preview | Undo | Compensation |
| --- | --- | --- | --- | --- |
| `animation_physics.bind_cloth_to_skeletal_mesh` | write | none | none | none |
| `animation_physics.cleanup` | destructive | none | none | none |
| `animation_physics.configure_anim_graph_node` | write | none | none | none |
| `animation_physics.configure_socket` | write | none | none | none |
| `animation_physics.configure_vehicle` | write | none | none | none |
| `animation_physics.create_animation_asset` | write | none | none | none |
| `animation_physics.create_animation_blueprint` | write | none | none | none |
| `animation_physics.create_control_rig` | write | none | none | none |
| `animation_physics.create_skeleton` | write | none | none | none |
| `animation_physics.edit_anim_graph` | write | none | none | none |
| `animation_physics.edit_animation` | write | none | none | none |
| `animation_physics.edit_blend_space` | write | none | none | none |
| `animation_physics.edit_montage` | write | none | none | none |
| `animation_physics.edit_morph_target` | write | none | none | none |
| `animation_physics.edit_physics_asset` | write | none | none | none |
| `animation_physics.edit_skeleton` | write | none | none | none |
| `animation_physics.edit_skin_weights` | write | none | none | none |
| `animation_physics.get_animation_info` | read | none | none | none |
| `animation_physics.get_skeleton_info` | read | none | none | none |
| `animation_physics.import_morph_targets` | write | none | none | none |
| `animation_physics.play_montage` | write | none | none | none |
| `animation_physics.remove_skeleton_element` | destructive | none | none | none |
| `animation_physics.set_retarget_chain_mapping` | read | none | none | none |
| `animation_physics.setup_ik` | write | none | none | none |
| `animation_physics.setup_physics_simulation` | write | none | none | none |
| `animation_physics.setup_ragdoll` | write | none | none | none |
| `animation_physics.setup_retargeting` | write | none | none | none |
| `animation_physics.skin_mesh_to_skeleton` | write | none | none | none |
| `asset.bulk_delete` | destructive | none | none | none |
| `asset.create_folder` | write | none | none | none |
| `asset.create_render_target` | write | none | none | none |
| `asset.delete` | destructive | none | none | none |
| `asset.duplicate` | write | none | none | none |
| `asset.edit_material_instance` | write | none | none | none |
| `asset.import` | write | none | none | none |
| `asset.import_marketplace_asset` | write | none | none | none |
| `asset.inspect_asset` | read | none | none | none |
| `asset.list` | read | none | none | none |
| `asset.maintain_content` | write | none | none | none |
| `asset.move` | write | none | none | none |
| `asset.nanite_rebuild_mesh` | write | none | none | none |
| `asset.process_asset` | write | none | none | none |
| `asset.query_asset` | read | none | none | none |
| `asset.query_marketplace` | read | none | none | none |
| `asset.rename` | write | none | none | none |
| `asset.set_metadata` | write | none | none | none |
| `asset.source_control` | write | none | none | none |
| `blueprint.add_content_widget` | write | none | none | none |
| `blueprint.add_function` | write | none | none | none |
| `blueprint.add_game_widget` | write | none | none | none |
| `blueprint.add_panel_widget` | write | none | none | none |
| `blueprint.bind_widget` | write | none | none | none |
| `blueprint.compile` | write | none | none | none |
| `blueprint.create` | write | none | none | none |
| `blueprint.create_game_screen` | write | none | none | none |
| `blueprint.create_widget_template` | write | none | none | none |
| `blueprint.delete_animation` | destructive | none | none | none |
| `blueprint.delete_node` | destructive | none | none | none |
| `blueprint.edit_graph` | write | none | none | none |
| `blueprint.edit_scs` | write | none | none | none |
| `blueprint.edit_variable` | write | none | none | none |
| `blueprint.edit_widget_animation` | write | none | none | none |
| `blueprint.edit_widget_blueprint` | write | none | none | none |
| `blueprint.get_blueprint` | read | none | none | none |
| `blueprint.get_scs` | read | none | none | none |
| `blueprint.get_widget_info` | read | none | none | none |
| `blueprint.inspect_graph` | read | none | none | none |
| `blueprint.probe_handle` | read | none | none | none |
| `blueprint.remove_function` | destructive | none | none | none |
| `blueprint.remove_scs_component` | destructive | none | none | none |
| `blueprint.remove_variable` | destructive | none | none | none |
| `blueprint.remove_widget` | destructive | none | none | none |
| `blueprint.set_font` | write | none | none | none |
| `blueprint.set_widget_layout` | write | none | none | none |
| `build_environment.add_foliage` | write | none | none | none |
| `build_environment.bake_lightmap` | write | none | none | none |
| `build_environment.build_lighting` | write | none | none | none |
| `build_environment.configure_atmosphere` | write | none | none | none |
| `build_environment.configure_foliage` | write | none | none | none |
| `build_environment.configure_lighting` | write | none | none | none |
| `build_environment.configure_lightmass` | write | none | none | none |
| `build_environment.configure_post_process` | write | none | none | none |
| `build_environment.configure_ray_tracing` | write | none | none | none |
| `build_environment.configure_scene_capture` | write | none | none | none |
| `build_environment.configure_spline_meshes` | write | none | none | none |
| `build_environment.configure_water` | write | none | none | none |
| `build_environment.configure_weather` | write | none | none | none |
| `build_environment.create_atmosphere_actor` | write | none | none | none |
| `build_environment.create_buoyancy_component` | write | none | none | none |
| `build_environment.create_capture_actor` | write | none | none | none |
| `build_environment.create_foliage_type` | write | none | none | none |
| `build_environment.create_landscape` | write | none | none | inverse: build_environment.delete |
| `build_environment.create_landscape_asset` | write | none | none | none |
| `build_environment.create_light` | write | none | none | none |
| `build_environment.create_lighting_setup` | write | none | none | none |
| `build_environment.create_procedural_terrain` | write | none | none | none |
| `build_environment.create_sky_light` | write | none | none | none |
| `build_environment.create_spline` | write | none | none | none |
| `build_environment.create_water_body` | write | none | none | none |
| `build_environment.create_weather_system` | write | none | none | none |
| `build_environment.delete` | destructive | none | none | none |
| `build_environment.edit_landscape` | write | none | none | none |
| `build_environment.edit_spline` | write | none | none | none |
| `build_environment.export_heightmap` | read | none | none | none |
| `build_environment.export_snapshot` | read | none | none | none |
| `build_environment.get_foliage_instances` | read | none | none | none |
| `build_environment.get_splines_info` | read | none | none | none |
| `build_environment.import_snapshot` | write | none | none | none |
| `build_environment.list_light_types` | read | none | none | none |
| `build_environment.paint_foliage_instances` | write | none | none | none |
| `build_environment.remove_foliage` | destructive | none | none | none |
| `build_environment.remove_spline_point` | destructive | none | none | none |
| `build_environment.sculpt` | write | none | none | none |
| `build_environment.set_light_channel` | write | none | none | none |
| `control_actor.add_tag` | write | none | none | none |
| `control_actor.apply_force` | write | none | none | none |
| `control_actor.attach` | write | none | none | none |
| `control_actor.audit_placement` | read | none | none | none |
| `control_actor.call_actor_function` | destructive | none | none | none |
| `control_actor.create_snapshot` | read | none | none | none |
| `control_actor.delete` | destructive | none | none | none |
| `control_actor.detach` | write | none | none | none |
| `control_actor.duplicate` | write | none | none | inverse: control_actor.delete |
| `control_actor.edit_component` | write | none | none | none |
| `control_actor.find` | read | none | none | none |
| `control_actor.find_by_tag` | read | none | none | none |
| `control_actor.get_component_property` | read | none | none | none |
| `control_actor.get_components` | read | none | none | none |
| `control_actor.get_transform` | read | none | none | none |
| `control_actor.list` | read | none | none | none |
| `control_actor.set_actor_collision` | write | none | none | none |
| `control_actor.set_blueprint_variables` | write | none | none | none |
| `control_actor.set_material` | write | none | none | none |
| `control_actor.set_transform` | write | none | none | none |
| `control_actor.set_visibility` | write | none | none | none |
| `control_actor.spawn` | write | none | none | inverse: control_actor.delete |
| `control_editor.close_asset` | write | none | none | none |
| `control_editor.configure_editor` | write | none | none | none |
| `control_editor.configure_viewport` | read | none | none | none |
| `control_editor.console_command` | write | none | none | none |
| `control_editor.create_bookmark` | write | none | none | none |
| `control_editor.describe_reflected_api` | read | none | none | none |
| `control_editor.focus_actor` | read | none | none | none |
| `control_editor.invoke_reflected_function` | destructive | none | none | none |
| `control_editor.jump_to_bookmark` | read | none | none | none |
| `control_editor.open_asset` | read | none | none | inverse: control_editor.close_asset |
| `control_editor.open_level` | write | none | none | none |
| `control_editor.play` | write | none | none | none |
| `control_editor.restart_editor` | destructive | none | none | none |
| `control_editor.save_all` | write | none | none | none |
| `control_editor.screenshot` | read | none | none | none |
| `control_editor.set_camera` | read | none | none | none |
| `control_editor.set_game_speed` | write | none | none | none |
| `control_editor.set_viewport_resolution` | read | none | none | none |
| `control_editor.simulate_input` | write | none | none | none |
| `control_editor.start_recording` | write | none | none | inverse: control_editor.stop_recording |
| `control_editor.undo` | write | none | none | none |
| `datatable.delete_data_table_row` | destructive | none | none | none |
| `datatable.edit_data_table` | write | none | none | none |
| `datatable.inspect_data_table` | read | none | none | none |
| `enum.delete_enum` | destructive | none | none | none |
| `enum.edit_enum` | write | none | none | none |
| `enum.get_enum` | read | none | none | none |
| `inspect.add_tag` | write | none | none | none |
| `inspect.create_snapshot` | write | none | none | none |
| `inspect.delete_object` | destructive | none | none | none |
| `inspect.find_by_class` | read | none | none | none |
| `inspect.get_blueprint_details` | read | none | none | none |
| `inspect.get_component_details` | read | none | none | none |
| `inspect.get_components` | read | none | none | none |
| `inspect.get_editor_state` | read | none | none | none |
| `inspect.get_property` | read | none | none | none |
| `inspect.get_stats` | read | none | none | none |
| `inspect.inspect_class` | read | none | none | none |
| `inspect.inspect_object` | read | none | none | none |
| `inspect.query_object` | read | none | none | none |
| `inspect.runtime_report` | read | none | none | none |
| `inspect.set_component_property` | write | none | none | none |
| `inspect.set_property` | write | none | none | none |
| `manage_ai.configure_navigation` | write | none | none | none |
| `manage_ai.create_ai_controller` | write | none | none | none |
| `manage_ai.create_behavior_tree` | write | none | none | none |
| `manage_ai.create_nav_actor` | write | none | none | none |
| `manage_ai.edit_behavior_tree` | write | none | none | none |
| `manage_ai.edit_blackboard` | write | none | none | none |
| `manage_ai.edit_eqs_query` | write | none | none | none |
| `manage_ai.edit_mass_entity` | write | none | none | none |
| `manage_ai.edit_smart_object` | write | none | none | none |
| `manage_ai.edit_state_tree` | write | none | none | none |
| `manage_ai.get_ai_info` | read | none | none | none |
| `manage_ai.get_tree` | read | none | none | none |
| `manage_ai.run_behavior_tree` | write | none | none | none |
| `manage_ai.set_ai_movement` | write | none | none | none |
| `manage_ai.set_focus` | write | none | none | none |
| `manage_ai.setup_perception` | write | none | none | none |
| `manage_audio.configure_sound_attenuation` | write | none | none | none |
| `manage_audio.configure_sound_class` | write | none | none | none |
| `manage_audio.control_sound_mix` | write | none | none | none |
| `manage_audio.create_audio_actor` | write | none | none | none |
| `manage_audio.create_audio_asset` | write | none | none | none |
| `manage_audio.edit_metasound` | write | none | none | none |
| `manage_audio.edit_sound_cue` | write | none | none | none |
| `manage_audio.enable_audio_analysis` | write | none | none | none |
| `manage_audio.fade_sound` | write | none | none | none |
| `manage_audio.get_audio_info` | read | none | none | none |
| `manage_audio.play_sound` | write | none | none | none |
| `manage_audio.set_dialogue_context` | write | none | none | none |
| `manage_character.build_metahuman` | write | none | none | none |
| `manage_character.configure_character` | write | none | none | none |
| `manage_character.create_character_blueprint` | write | none | none | none |
| `manage_character.create_metahuman` | write | none | none | none |
| `manage_character.export_metahuman` | write | none | none | none |
| `manage_character.get_character_info` | read | none | none | none |
| `manage_character.metahuman_status` | read | none | none | none |
| `manage_character.rig_metahuman` | write | none | none | none |
| `manage_character.set_movement_property` | write | none | none | none |
| `manage_character.setup_character_ability` | write | none | none | none |
| `manage_combat.configure_damage` | write | none | none | none |
| `manage_combat.configure_projectile` | write | none | none | none |
| `manage_combat.configure_weapon` | write | none | none | none |
| `manage_combat.create_combat_asset` | write | none | none | none |
| `manage_combat.get_combat_info` | read | none | none | none |
| `manage_effect.activate` | write | none | none | none |
| `manage_effect.add_niagara_data_interface` | write | none | none | none |
| `manage_effect.add_niagara_module` | write | none | none | none |
| `manage_effect.advance_simulation` | write | none | none | none |
| `manage_effect.cleanup` | write | none | none | none |
| `manage_effect.create_dynamic_light` | write | none | none | none |
| `manage_effect.create_effect` | write | none | none | none |
| `manage_effect.debug_shape` | write | none | none | none |
| `manage_effect.edit_niagara_system` | write | none | none | none |
| `manage_effect.get_niagara_info` | read | none | none | none |
| `manage_effect.list_debug_shapes` | read | none | none | none |
| `manage_effect.remove_niagara_node` | destructive | none | none | none |
| `manage_effect.spawn_niagara` | write | none | none | none |
| `manage_gas.add_tag_to_asset` | write | none | none | none |
| `manage_gas.configure_ability` | write | none | none | none |
| `manage_gas.configure_asc` | write | none | none | none |
| `manage_gas.configure_attribute_set` | write | none | none | none |
| `manage_gas.configure_gameplay_cue` | write | none | none | none |
| `manage_gas.configure_gameplay_effect` | write | none | none | none |
| `manage_gas.create_gas_asset` | write | none | none | none |
| `manage_gas.get_gas_info` | read | none | none | none |
| `manage_geometry.array_mesh` | write | none | none | none |
| `manage_geometry.boolean_mesh` | write | none | none | none |
| `manage_geometry.configure_mesh_collision` | write | none | none | none |
| `manage_geometry.configure_mesh_lods` | write | none | none | none |
| `manage_geometry.convert_to_nanite` | write | none | none | none |
| `manage_geometry.convert_to_static_mesh` | write | none | none | none |
| `manage_geometry.create_primitive` | write | none | none | none |
| `manage_geometry.deform_mesh` | write | none | none | none |
| `manage_geometry.edit_dynamic_mesh` | write | none | none | none |
| `manage_geometry.edit_uvs` | write | none | none | none |
| `manage_geometry.get_mesh_info` | read | none | none | none |
| `manage_geometry.get_vertex_position` | read | none | none | none |
| `manage_geometry.mirror` | write | none | none | none |
| `manage_geometry.model_mesh` | write | none | none | none |
| `manage_geometry.optimize_mesh` | write | none | none | none |
| `manage_interaction.configure_destruction` | write | none | none | none |
| `manage_interaction.configure_interactable` | write | none | none | none |
| `manage_interaction.create_interactable` | write | none | none | none |
| `manage_interaction.get_interaction_info` | read | none | none | none |
| `manage_inventory.configure_crafting` | write | none | none | none |
| `manage_inventory.configure_equipment` | write | none | none | none |
| `manage_inventory.configure_inventory` | write | none | none | none |
| `manage_inventory.configure_item` | write | none | none | none |
| `manage_inventory.configure_loot` | write | none | none | none |
| `manage_inventory.configure_pickup` | write | none | none | none |
| `manage_inventory.create_inventory_asset` | write | none | none | none |
| `manage_inventory.get_inventory_info` | read | none | none | none |
| `manage_level.add_sublevel` | write | none | none | none |
| `manage_level.build_lighting` | write | none | none | none |
| `manage_level.create_level` | write | none | none | none |
| `manage_level.create_light` | write | none | none | none |
| `manage_level.delete` | destructive | none | none | none |
| `manage_level.duplicate_level` | write | none | none | none |
| `manage_level.export_level` | read | none | none | none |
| `manage_level.get_summary` | read | none | none | none |
| `manage_level.import_level` | write | none | none | none |
| `manage_level.list_levels` | read | none | none | none |
| `manage_level.load` | write | none | none | none |
| `manage_level.save` | write | none | none | none |
| `manage_level.set_metadata` | write | none | none | none |
| `manage_level.set_world_settings` | write | none | none | none |
| `manage_level.stream` | write | none | none | none |
| `manage_level.unload_level` | write | none | none | none |
| `manage_level.validate_level` | read | none | none | none |
| `manage_level_structure.configure_level_streaming` | write | none | none | none |
| `manage_level_structure.create_level_structure` | write | none | none | none |
| `manage_level_structure.create_volume` | write | none | none | none |
| `manage_level_structure.edit_level_blueprint` | write | none | none | none |
| `manage_level_structure.get_level_structure_info` | read | none | none | none |
| `manage_level_structure.get_volumes_info` | read | none | none | none |
| `manage_level_structure.remove_volume` | destructive | none | none | none |
| `manage_level_structure.set_volume_properties` | write | none | none | none |
| `manage_networking.add_legacy_mapping` | write | none | none | none |
| `manage_networking.add_local_player` | write | none | none | none |
| `manage_networking.check_authority` | read | none | none | none |
| `manage_networking.configure_game_mode` | write | none | none | none |
| `manage_networking.configure_input` | write | none | none | none |
| `manage_networking.configure_prediction` | write | none | none | none |
| `manage_networking.configure_replication` | write | none | none | none |
| `manage_networking.configure_rpc` | write | none | none | none |
| `manage_networking.configure_session` | write | none | none | none |
| `manage_networking.configure_voice` | write | none | none | none |
| `manage_networking.create_framework_class` | write | none | none | none |
| `manage_networking.get_game_framework_info` | read | none | none | none |
| `manage_networking.get_input_info` | read | none | none | none |
| `manage_networking.get_networking_info` | read | none | none | none |
| `manage_networking.get_sessions_info` | read | none | none | none |
| `manage_networking.host_lan_server` | write | none | none | none |
| `manage_networking.remove_legacy_mapping` | destructive | none | none | none |
| `manage_networking.remove_local_player` | destructive | none | none | none |
| `manage_networking.remove_mapping` | destructive | none | none | none |
| `manage_networking.set_owner` | write | none | none | none |
| `manage_pcg.add_pcg_node` | write | none | none | none |
| `manage_pcg.edit_pcg_graph` | write | none | none | none |
| `manage_pcg.execute_pcg_graph` | write | none | none | none |
| `manage_tools.disable_category` | write | none | none | none |
| `manage_tools.disable_tools` | write | none | none | none |
| `manage_tools.enable_category` | write | none | none | inverse: manage_tools.disable_category |
| `manage_tools.enable_tools` | write | none | none | inverse: manage_tools.disable_tools |
| `manage_tools.get_status` | read | none | none | none |
| `manage_tools.list_categories` | read | none | none | none |
| `manage_tools.list_tools` | read | none | none | none |
| `manage_tools.reset` | write | none | none | none |
| `material.add_function_io` | write | none | none | none |
| `material.add_material_node` | write | none | none | none |
| `material.compile_material` | write | none | none | none |
| `material.configure_layer_blend` | write | none | none | none |
| `material.connect_nodes` | write | none | none | none |
| `material.create_material` | write | none | none | none |
| `material.create_material_instance` | write | none | none | none |
| `material.delete_node` | write | none | none | none |
| `material.disconnect_nodes` | write | none | none | none |
| `material.get_material_info` | read | none | none | none |
| `material.set_material_parameter` | write | none | none | none |
| `material.set_material_property` | write | none | none | none |
| `material.set_node_position` | write | none | none | none |
| `material.update_custom_expression` | write | none | none | none |
| `sequence.cinematic.add_cinematic_track` | write | none | none | none |
| `sequence.cinematic.configure_cinematic` | write | none | none | none |
| `sequence.cinematic.create_cinematic_asset` | write | none | none | none |
| `sequence.create` | write | none | none | none |
| `sequence.delete` | destructive | none | none | none |
| `sequence.edit_sequence_bindings` | write | none | none | none |
| `sequence.edit_sequence_tracks` | write | none | none | none |
| `sequence.get_metadata` | read | none | none | none |
| `sequence.get_properties` | read | none | none | none |
| `sequence.media.create_media_asset` | write | none | none | none |
| `sequence.media.play_media` | write | none | none | none |
| `sequence.mrq.configure_render_job` | write | none | none | none |
| `sequence.mrq.create_render_job` | write | none | none | none |
| `sequence.play` | write | none | none | none |
| `sequence.replay.configure_demo_settings` | write | none | none | none |
| `sequence.replay.play_demo` | write | none | none | none |
| `sequence.set_metadata` | write | none | none | none |
| `sequence.set_properties` | write | none | none | none |
| `sequence.take.configure_take_recorder` | write | none | none | none |
| `struct.delete_struct` | destructive | none | none | none |
| `struct.edit_struct` | write | none | none | none |
| `struct.get_struct` | read | none | none | none |
| `system_control.configure_display` | write | none | none | none |
| `system_control.configure_performance` | write | none | none | none |
| `system_control.console_command` | write | none | none | none |
| `system_control.create_widget` | write | none | none | none |
| `system_control.enable_plugin` | write | none | none | none |
| `system_control.execute_python` | write | none | none | none |
| `system_control.get_project_settings` | read | none | none | none |
| `system_control.get_trace_status` | read | none | none | none |
| `system_control.launch_build` | write | none | none | none |
| `system_control.list_plugins` | read | none | none | none |
| `system_control.lumen_update_scene` | write | none | none | none |
| `system_control.merge_actors` | write | none | none | none |
| `system_control.package_project` | write | none | none | none |
| `system_control.package_status` | read | none | none | none |
| `system_control.play_sound` | write | none | none | none |
| `system_control.profile_performance` | write | none | none | none |
| `system_control.read_log` | read | none | none | none |
| `system_control.run_build` | write | none | none | none |
| `system_control.screenshot` | read | none | none | none |
| `system_control.set_project_setting` | write | none | none | none |
| `system_control.start_session` | write | none | none | inverse: system_control.stop_session |
| `system_control.subscribe` | write | none | none | none |
| `system_control.validate_assets` | read | none | none | none |
| `texture.adjust_texture` | write | none | none | none |
| `texture.configure_texture` | write | none | none | none |
| `texture.create_texture` | write | none | none | none |
| `texture.get_texture_info` | read | none | none | none |
