<!-- GENERATED FILE - DO NOT EDIT.
     Regenerate with `npm run registry:generate`; `npm run registry:check` gates drift.
     Source of truth: src/tools/catalog/capabilities/records/** -->

# Action reference

Catalog revision: `a5308260c019ebae`

Both transports expose exactly ONE public MCP tool, `unreal`, with the four
operations `search` / `describe` / `execute` / `configure`. The parent tools
named in these tables are an INTERNAL routing boundary: they are never listed
by `tools/list` and a direct `tools/call` on one returns a
`DIRECT_TOOL_CALL_REMOVED` receipt rather than executing
(`src/server/gateway/direct-call-migration.ts`).

The catalog declares 389 capabilities across
23 internal parent tools.
Every row is derived from the capability record that the gateway actually
validates against, so `execute` cannot accept an action this table omits.

## Reading a row

- **Capability** — the canonical id. Pass it to `describe`/`execute` as the
  `tool` + `action` pair shown in the same row.
- **Effect** — `read` | `write` | `destructive` (`behavior.effect`).
- **Scope** — the capability scope the caller must hold
  (`policy.requiredScope`). Scope membership is EXACT-SET with an `admin`
  wildcard: holding `write` does NOT imply `read`.
- **Consent** — `none` | `explicit` | `elevated` (`policy.consent`). A
  non-`none` value must be satisfied by an execute-envelope `consent`
  sibling naming THAT capability; it is never a handler parameter.
- **Legacy pairs** — the pre-gateway `{tool, action}` spellings that still
  resolve to this capability. See the migration reference for the full map.

## Per-parent totals

| Parent tool | Capabilities | read | write | destructive | Domains |
| --- | --- | --- | --- | --- | --- |
| `animation_physics` | 28 | 3 | 23 | 2 | animation physics |
| `build_environment` | 40 | 5 | 32 | 3 | environment |
| `control_actor` | 22 | 8 | 12 | 2 | actor |
| `control_editor` | 21 | 8 | 11 | 2 | editor |
| `inspect` | 16 | 11 | 4 | 1 | inspect |
| `manage_ai` | 16 | 2 | 14 | 0 | manage ai |
| `manage_asset` | 46 | 9 | 32 | 5 | asset, datatable, enum, material, struct, texture |
| `manage_audio` | 12 | 1 | 11 | 0 | audio |
| `manage_blueprint` | 27 | 5 | 16 | 6 | blueprint, widget |
| `manage_character` | 10 | 2 | 8 | 0 | manage character |
| `manage_combat` | 5 | 1 | 4 | 0 | manage combat |
| `manage_effect` | 13 | 2 | 10 | 1 | manage effect |
| `manage_gas` | 8 | 1 | 7 | 0 | manage gas |
| `manage_geometry` | 15 | 2 | 13 | 0 | world |
| `manage_interaction` | 4 | 1 | 3 | 0 | manage interaction |
| `manage_inventory` | 8 | 1 | 7 | 0 | manage inventory |
| `manage_level` | 17 | 4 | 12 | 1 | level |
| `manage_level_structure` | 8 | 2 | 5 | 1 | world |
| `manage_networking` | 20 | 5 | 12 | 3 | networking |
| `manage_pcg` | 3 | 0 | 3 | 0 | world |
| `manage_sequence` | 19 | 2 | 16 | 1 | cinematics, media, movie_render, replay, sequence, take_recorder |
| `manage_tools` | 8 | 3 | 5 | 0 | tools |
| `system_control` | 23 | 7 | 16 | 0 | audio, build, console, insights, logs, performance, project, python, render, viewport, widget |

## Capabilities requiring consent

61 of 389 capabilities require consent.

| Capability | Tool | Action | Effect | Consent |
| --- | --- | --- | --- | --- |
| `animation_physics.cleanup` | `animation_physics` | `cleanup` | destructive | explicit |
| `animation_physics.remove_skeleton_element` | `animation_physics` | `remove_bone` | destructive | explicit |
| `asset.bulk_delete` | `manage_asset` | `bulk_delete` | destructive | elevated |
| `asset.create_folder` | `manage_asset` | `create_folder` | write | explicit |
| `asset.create_render_target` | `manage_asset` | `manage_texture` | write | explicit |
| `asset.delete` | `manage_asset` | `delete` | destructive | elevated |
| `asset.duplicate` | `manage_asset` | `duplicate` | write | explicit |
| `asset.edit_material_instance` | `manage_asset` | `add_material_parameter` | write | explicit |
| `asset.import` | `manage_asset` | `import` | write | explicit |
| `asset.import_marketplace_asset` | `manage_asset` | `add_fab_asset_to_project` | write | explicit |
| `asset.maintain_content` | `manage_asset` | `bulk_rename` | write | explicit |
| `asset.move` | `manage_asset` | `move` | write | explicit |
| `asset.nanite_rebuild_mesh` | `manage_asset` | `manage_render` | write | explicit |
| `asset.process_asset` | `manage_asset` | `generate_thumbnail` | write | explicit |
| `asset.rename` | `manage_asset` | `rename` | write | explicit |
| `asset.set_metadata` | `manage_asset` | `set_metadata` | write | explicit |
| `asset.source_control` | `manage_asset` | `source_control_checkout` | write | explicit |
| `blueprint.delete_animation` | `manage_blueprint` | `delete_animation` | destructive | explicit |
| `blueprint.delete_node` | `manage_blueprint` | `delete_node` | destructive | explicit |
| `blueprint.remove_function` | `manage_blueprint` | `remove_function` | destructive | explicit |
| `blueprint.remove_scs_component` | `manage_blueprint` | `remove_scs_component` | destructive | explicit |
| `blueprint.remove_variable` | `manage_blueprint` | `remove_variable` | destructive | explicit |
| `blueprint.remove_widget` | `manage_blueprint` | `remove_widget` | destructive | explicit |
| `build_environment.delete` | `build_environment` | `delete` | destructive | explicit |
| `build_environment.remove_foliage` | `build_environment` | `remove_foliage` | destructive | explicit |
| `build_environment.remove_spline_point` | `build_environment` | `remove_spline_point` | destructive | explicit |
| `control_actor.call_actor_function` | `control_actor` | `call_actor_function` | destructive | elevated |
| `control_actor.delete` | `control_actor` | `delete` | destructive | explicit |
| `control_editor.invoke_reflected_function` | `control_editor` | `control_editor` | destructive | elevated |
| `control_editor.restart_editor` | `control_editor` | `restart_editor` | destructive | explicit |
| `datatable.delete_data_table_row` | `manage_asset` | `delete_data_table_row` | destructive | elevated |
| `datatable.edit_data_table` | `manage_asset` | `create_data_table` | write | explicit |
| `enum.delete_enum` | `manage_asset` | `delete_enum` | destructive | elevated |
| `enum.edit_enum` | `manage_asset` | `create_enum` | write | explicit |
| `inspect.delete_object` | `inspect` | `control_actor` | destructive | explicit |
| `manage_effect.remove_niagara_node` | `manage_effect` | `remove_niagara_node` | destructive | explicit |
| `manage_level.delete` | `manage_level` | `delete_level` | destructive | explicit |
| `manage_level_structure.remove_volume` | `manage_level_structure` | `remove_volume` | destructive | explicit |
| `manage_networking.remove_legacy_mapping` | `manage_networking` | `manage_input` | destructive | explicit |
| `manage_networking.remove_local_player` | `manage_networking` | `manage_sessions` | destructive | explicit |
| `manage_networking.remove_mapping` | `manage_networking` | `manage_input` | destructive | explicit |
| `material.add_function_io` | `manage_asset` | `add_function_input` | write | explicit |
| `material.add_material_node` | `manage_asset` | `add_material_node` | write | explicit |
| `material.compile_material` | `manage_asset` | `compile_material` | write | explicit |
| `material.configure_layer_blend` | `manage_asset` | `configure_layer_blend` | write | explicit |
| `material.connect_nodes` | `manage_asset` | `connect_nodes` | write | explicit |
| `material.create_material` | `manage_asset` | `create_material` | write | explicit |
| `material.create_material_instance` | `manage_asset` | `create_material_instance` | write | explicit |
| `material.delete_node` | `manage_asset` | `delete_node` | write | explicit |
| `material.disconnect_nodes` | `manage_asset` | `disconnect_nodes` | write | explicit |
| `material.set_material_parameter` | `manage_asset` | `set_material_parameter` | write | explicit |
| `material.set_material_property` | `manage_asset` | `set_blend_mode` | write | explicit |
| `material.set_node_position` | `manage_asset` | `set_node_position` | write | explicit |
| `material.update_custom_expression` | `manage_asset` | `update_custom_expression` | write | explicit |
| `sequence.delete` | `manage_sequence` | `delete` | destructive | explicit |
| `struct.delete_struct` | `manage_asset` | `delete_struct` | destructive | elevated |
| `struct.edit_struct` | `manage_asset` | `create_struct` | write | explicit |
| `system_control.execute_python` | `system_control` | `system_control` | write | explicit |
| `texture.adjust_texture` | `manage_asset` | `adjust_curves` | write | explicit |
| `texture.configure_texture` | `manage_asset` | `set_compression_settings` | write | explicit |
| `texture.create_texture` | `manage_asset` | `create_gradient_texture` | write | explicit |

## Deprecated and removed capabilities

1 capability record is no longer `active`.

| Capability | Status | Since | Guidance | Replacement |
| --- | --- | --- | --- | --- |
| `animation_physics.set_retarget_chain_mapping` | deprecated | 5.0 | Documented no-op: the route performs no mutation. Prefer setup_retargeting for real retarget configuration. | — |

## Full action reference

| Capability | Tool | Action | Effect | Scope | Consent | Legacy pairs |
| --- | --- | --- | --- | --- | --- | --- |
| `animation_physics.bind_cloth_to_skeletal_mesh` | `animation_physics` | `bind_cloth_to_skeletal_mesh` | write | write | none | `animation_physics.bind_cloth_to_skeletal_mesh` `animation_physics.assign_cloth_asset_to_mesh` |
| `animation_physics.cleanup` | `animation_physics` | `cleanup` | destructive | destructive | explicit | `animation_physics.cleanup` |
| `animation_physics.configure_anim_graph_node` | `animation_physics` | `add_layered_blend_per_bone` | write | write | none | `animation_physics.configure_anim_graph_node` `animation_physics.add_layered_blend_per_bone` `animation_physics.set_anim_graph_node_value` |
| `animation_physics.configure_socket` | `animation_physics` | `configure_socket` | write | write | none | `animation_physics.configure_socket` `animation_physics.add_socket` `animation_physics.create_socket` `animation_physics.modify_socket` |
| `animation_physics.configure_vehicle` | `animation_physics` | `configure_vehicle` | write | write | none | `animation_physics.configure_vehicle` |
| `animation_physics.create_animation_asset` | `animation_physics` | `create_animation_asset` | write | write | none | `animation_physics.create_animation_asset` `animation_physics.create_animation_sequence` `animation_physics.create_montage` `animation_physics.create_blend_space` `animation_physics.create_blend_space_1d` `animation_physics.create_blend_space_2d` `animation_physics.create_aim_offset` `animation_physics.create_pose_library` `animation_physics.create_procedural_anim` |
| `animation_physics.create_animation_blueprint` | `animation_physics` | `create_animation_blueprint` | write | write | none | `animation_physics.create_animation_blueprint` `animation_physics.create_anim_blueprint` `animation_physics.create_animation_bp` |
| `animation_physics.create_control_rig` | `animation_physics` | `create_control_rig` | write | write | none | `animation_physics.create_control_rig` |
| `animation_physics.create_skeleton` | `animation_physics` | `create_skeleton` | write | write | none | `animation_physics.create_skeleton` |
| `animation_physics.edit_anim_graph` | `animation_physics` | `add_blend_node` | write | write | none | `animation_physics.edit_anim_graph` `animation_physics.add_blend_node` `animation_physics.add_cached_pose` `animation_physics.add_slot_node` `animation_physics.create_state_machine` `animation_physics.add_state_machine` `animation_physics.add_state` `animation_physics.add_transition` `animation_physics.set_transition_rules` `animation_physics.delete_transition` `animation_physics.create_blend_tree` |
| `animation_physics.edit_animation` | `animation_physics` | `add_bone_track` | write | write | none | `animation_physics.edit_animation` `animation_physics.add_bone_track` `animation_physics.set_bone_key` `animation_physics.set_curve_key` `animation_physics.add_notify` `animation_physics.add_notify_state` `animation_physics.add_sync_marker` `animation_physics.set_additive_settings` `animation_physics.set_root_motion_settings` `animation_physics.set_sequence_length` `animation_physics.add_aim_offset_sample` |
| `animation_physics.edit_blend_space` | `animation_physics` | `add_blend_sample` | write | write | none | `animation_physics.edit_blend_space` `animation_physics.add_blend_sample` `animation_physics.set_axis_settings` `animation_physics.set_interpolation_settings` `animation_physics.force_rebuild_blend_space` |
| `animation_physics.edit_montage` | `animation_physics` | `add_montage_notify` | write | write | none | `animation_physics.edit_montage` `animation_physics.add_montage_notify` `animation_physics.add_montage_section` `animation_physics.add_montage_slot` `animation_physics.link_sections` `animation_physics.set_blend_in` `animation_physics.set_blend_out` `animation_physics.set_section_timing` |
| `animation_physics.edit_morph_target` | `animation_physics` | `create_morph_target` | write | write | none | `animation_physics.edit_morph_target` `animation_physics.create_morph_target` `animation_physics.set_morph_target_deltas` `animation_physics.set_morph_target_value` |
| `animation_physics.edit_physics_asset` | `animation_physics` | `create_physics_asset` | write | write | none | `animation_physics.edit_physics_asset` `animation_physics.create_physics_asset` `animation_physics.add_physics_body` `animation_physics.configure_physics_body` `animation_physics.modify_physics_body` `animation_physics.add_physics_constraint` `animation_physics.set_physics_constraint` `animation_physics.configure_constraint_limits` `animation_physics.set_physics_asset` |
| `animation_physics.edit_skeleton` | `animation_physics` | `add_bone` | write | write | none | `animation_physics.edit_skeleton` `animation_physics.add_bone` `animation_physics.rename_bone` `animation_physics.set_bone_parent` `animation_physics.set_bone_transform` `animation_physics.create_virtual_bone` |
| `animation_physics.edit_skin_weights` | `animation_physics` | `auto_skin_weights` | write | write | none | `animation_physics.edit_skin_weights` `animation_physics.auto_skin_weights` `animation_physics.copy_weights` `animation_physics.mirror_weights` `animation_physics.normalize_weights` `animation_physics.prune_weights` `animation_physics.set_vertex_weights` |
| `animation_physics.get_animation_info` | `animation_physics` | `get_animation_info` | read | read | none | `animation_physics.get_animation_info` |
| `animation_physics.get_skeleton_info` | `animation_physics` | `get_skeleton_info` | read | read | none | `animation_physics.get_skeleton_info` `animation_physics.list_bones` `animation_physics.list_sockets` `animation_physics.list_virtual_bones` `animation_physics.get_bone_transform` `animation_physics.list_morph_targets` `animation_physics.get_physics_asset_info` `animation_physics.list_physics_bodies` |
| `animation_physics.import_morph_targets` | `animation_physics` | `import_morph_targets` | write | write | none | `animation_physics.import_morph_targets` |
| `animation_physics.play_montage` | `animation_physics` | `play_montage` | write | write | none | `animation_physics.play_montage` `animation_physics.play_anim_montage` |
| `animation_physics.remove_skeleton_element` | `animation_physics` | `remove_bone` | destructive | destructive | explicit | `animation_physics.remove_skeleton_element` `animation_physics.remove_bone` `animation_physics.remove_socket` `animation_physics.remove_physics_body` |
| `animation_physics.set_retarget_chain_mapping` | `animation_physics` | `set_retarget_chain_mapping` | read | read | none | `animation_physics.set_retarget_chain_mapping` |
| `animation_physics.setup_ik` | `animation_physics` | `setup_ik` | write | write | none | `animation_physics.setup_ik` `animation_physics.create_ik_rig` `animation_physics.create_ik_retargeter` |
| `animation_physics.setup_physics_simulation` | `animation_physics` | `setup_physics_simulation` | write | write | none | `animation_physics.setup_physics_simulation` |
| `animation_physics.setup_ragdoll` | `animation_physics` | `setup_ragdoll` | write | write | none | `animation_physics.setup_ragdoll` `animation_physics.activate_ragdoll` |
| `animation_physics.setup_retargeting` | `animation_physics` | `setup_retargeting` | write | write | none | `animation_physics.setup_retargeting` |
| `animation_physics.skin_mesh_to_skeleton` | `animation_physics` | `skin_mesh_to_skeleton` | write | write | none | `animation_physics.skin_mesh_to_skeleton` |
| `asset.bulk_delete` | `manage_asset` | `bulk_delete` | destructive | destructive | elevated | `manage_asset.bulk_delete` |
| `asset.create_folder` | `manage_asset` | `create_folder` | write | write | explicit | `manage_asset.create_folder` |
| `asset.create_render_target` | `manage_asset` | `manage_texture` | write | write | explicit | `manage_asset.create_render_target` |
| `asset.delete` | `manage_asset` | `delete` | destructive | destructive | elevated | `manage_asset.delete` `manage_asset.delete_asset` `manage_asset.delete_assets` |
| `asset.duplicate` | `manage_asset` | `duplicate` | write | write | explicit | `manage_asset.duplicate` `manage_asset.duplicate_asset` |
| `asset.edit_material_instance` | `manage_asset` | `add_material_parameter` | write | write | explicit | `manage_asset.edit_material_instance` `manage_asset.add_material_parameter` `manage_asset.reset_instance_parameters` |
| `asset.import` | `manage_asset` | `import` | write | write | explicit | `manage_asset.import` |
| `asset.import_marketplace_asset` | `manage_asset` | `add_fab_asset_to_project` | write | write | explicit | `manage_asset.import_marketplace_asset` `manage_asset.add_fab_asset_to_project` `manage_asset.download_fab_asset` `manage_asset.import_megascans_asset` |
| `asset.inspect_asset` | `manage_asset` | `get_metadata` | read | read | none | `manage_asset.inspect_asset` `manage_asset.get_metadata` `manage_asset.get_dependencies` `manage_asset.get_asset_graph` `manage_asset.validate` `manage_asset.generate_report` |
| `asset.list` | `manage_asset` | `list` | read | read | none | `manage_asset.list` `manage_asset.list_content_sources` `manage_asset.list_instances` |
| `asset.maintain_content` | `manage_asset` | `bulk_rename` | write | write | explicit | `manage_asset.maintain_content` `manage_asset.bulk_rename` `manage_asset.fixup_redirectors` `manage_asset.migrate_assets` |
| `asset.move` | `manage_asset` | `move` | write | write | explicit | `manage_asset.move` `manage_asset.move_asset` |
| `asset.nanite_rebuild_mesh` | `manage_asset` | `manage_render` | write | write | explicit | `manage_asset.nanite_rebuild_mesh` |
| `asset.process_asset` | `manage_asset` | `generate_thumbnail` | write | write | explicit | `manage_asset.process_asset` `manage_asset.create_thumbnail` `manage_asset.generate_lods` |
| `asset.query_asset` | `manage_asset` | `exists` | read | read | none | `manage_asset.query_asset` `manage_asset.exists` `manage_asset.search_assets` `manage_asset.find_by_tag` `manage_asset.analyze_graph` `manage_asset.get_material_stats` `manage_asset.get_source_control_state` |
| `asset.query_marketplace` | `manage_asset` | `get_fab_listing_details` | read | read | none | `manage_asset.query_marketplace` `manage_asset.get_fab_listing_details` `manage_asset.list_fab_downloads` `manage_asset.list_fab_library` `manage_asset.search_fab_listings` `manage_asset.list_megascans_library` |
| `asset.rename` | `manage_asset` | `rename` | write | write | explicit | `manage_asset.rename` `manage_asset.rename_asset` |
| `asset.set_metadata` | `manage_asset` | `set_metadata` | write | write | explicit | `manage_asset.set_metadata` `manage_asset.set_tags` |
| `asset.source_control` | `manage_asset` | `source_control_checkout` | write | write | explicit | `manage_asset.source_control` `manage_asset.source_control_checkout` `manage_asset.source_control_submit` `manage_asset.source_control_enable` `manage_asset.source_control_init` `manage_asset.source_control_commit_all` |
| `blueprint.add_content_widget` | `manage_blueprint` | `add_text_block` | write | write | none | `manage_blueprint.add_content_widget` `manage_blueprint.add_text_block` `manage_blueprint.add_rich_text_block` `manage_blueprint.add_image` `manage_blueprint.add_button` `manage_blueprint.add_check_box` `manage_blueprint.add_combo_box` `manage_blueprint.add_slider` `manage_blueprint.add_spin_box` `manage_blueprint.add_progress_bar` `manage_blueprint.add_text_input` `manage_blueprint.add_list_view` `manage_blueprint.add_tree_view` `manage_blueprint.add_widget_component` |
| `blueprint.add_function` | `manage_blueprint` | `add_function` | write | write | none | `manage_blueprint.add_function` `manage_blueprint.add_event` |
| `blueprint.add_game_widget` | `manage_blueprint` | `add_health_bar` | write | write | none | `manage_blueprint.add_game_widget` `manage_blueprint.add_health_bar` `manage_blueprint.add_ammo_counter` `manage_blueprint.add_crosshair` `manage_blueprint.add_minimap` `manage_blueprint.add_compass` `manage_blueprint.add_damage_indicator` `manage_blueprint.add_interaction_prompt` `manage_blueprint.add_objective_tracker` `manage_blueprint.add_quest_tracker` |
| `blueprint.add_panel_widget` | `manage_blueprint` | `add_canvas_panel` | write | write | none | `manage_blueprint.add_panel_widget` `manage_blueprint.add_canvas_panel` `manage_blueprint.add_overlay` `manage_blueprint.add_vertical_box` `manage_blueprint.add_horizontal_box` `manage_blueprint.add_grid_panel` `manage_blueprint.add_uniform_grid` `manage_blueprint.add_wrap_box` `manage_blueprint.add_border` `manage_blueprint.add_scroll_box` `manage_blueprint.add_size_box` `manage_blueprint.add_scale_box` `manage_blueprint.add_spacer` `manage_blueprint.add_safe_zone` `manage_blueprint.add_widget_switcher` |
| `blueprint.bind_widget` | `manage_blueprint` | `bind_text` | write | write | none | `manage_blueprint.bind_widget` `manage_blueprint.bind_text` `manage_blueprint.bind_color` `manage_blueprint.bind_enabled` `manage_blueprint.bind_visibility` `manage_blueprint.bind_on_clicked` `manage_blueprint.bind_on_hovered` `manage_blueprint.bind_on_value_changed` `manage_blueprint.bind_localized_text` `manage_blueprint.create_property_binding` `manage_blueprint.set_localization_key` `manage_blueprint.set_widget_binding` |
| `blueprint.compile` | `manage_blueprint` | `compile` | write | write | none | `manage_blueprint.compile` |
| `blueprint.create` | `manage_blueprint` | `create` | write | write | none | `manage_blueprint.create` `manage_blueprint.create_blueprint` `manage_blueprint.ensure_exists` |
| `blueprint.create_game_screen` | `manage_blueprint` | `create_credits_screen` | write | write | none | `manage_blueprint.create_game_screen` `manage_blueprint.create_credits_screen` `manage_blueprint.create_shop_ui` |
| `blueprint.create_widget_template` | `manage_blueprint` | `create_main_menu` | write | write | none | `manage_blueprint.create_widget_template` `manage_blueprint.create_main_menu` `manage_blueprint.create_pause_menu` `manage_blueprint.create_settings_menu` `manage_blueprint.create_hud_widget` `manage_blueprint.create_dialog_widget` `manage_blueprint.create_inventory_ui` `manage_blueprint.create_loading_screen` `manage_blueprint.create_radial_menu` |
| `blueprint.delete_animation` | `manage_blueprint` | `delete_animation` | destructive | destructive | explicit | `manage_blueprint.delete_animation` |
| `blueprint.delete_node` | `manage_blueprint` | `delete_node` | destructive | destructive | explicit | `manage_blueprint.delete_node` `manage_blueprint.break_pin_links` |
| `blueprint.edit_graph` | `manage_blueprint` | `add_node` | write | write | none | `manage_blueprint.edit_graph` `manage_blueprint.add_node` `manage_blueprint.create_node` `manage_blueprint.create_reroute_node` `manage_blueprint.create_struct_make_break_nodes` `manage_blueprint.connect_pins` `manage_blueprint.set_node_property` `manage_blueprint.set_pin_default_value` `manage_blueprint.add_construction_script` `manage_blueprint.build_graph` |
| `blueprint.edit_scs` | `manage_blueprint` | `add_scs_component` | write | write | none | `manage_blueprint.edit_scs` `manage_blueprint.add_scs_component` `manage_blueprint.add_component` `manage_blueprint.modify_scs` `manage_blueprint.reparent_scs_component` `manage_blueprint.set_scs_property` `manage_blueprint.set_scs_transform` |
| `blueprint.edit_variable` | `manage_blueprint` | `add_variable` | write | write | none | `manage_blueprint.edit_variable` `manage_blueprint.add_variable` `manage_blueprint.rename_variable` `manage_blueprint.set_variable_metadata` `manage_blueprint.set_metadata` `manage_blueprint.set_default` |
| `blueprint.edit_widget_animation` | `manage_blueprint` | `create_widget_animation` | write | write | none | `manage_blueprint.edit_widget_animation` `manage_blueprint.create_widget_animation` `manage_blueprint.add_animation_track` `manage_blueprint.add_animation_keyframe` `manage_blueprint.set_animation_loop` |
| `blueprint.edit_widget_blueprint` | `manage_blueprint` | `create_widget_blueprint` | write | write | none | `manage_blueprint.edit_widget_blueprint` `manage_blueprint.create_widget_blueprint` `manage_blueprint.set_widget_parent_class` `manage_blueprint.preview_widget` `manage_blueprint.rename_widget` `manage_blueprint.reparent_widget` |
| `blueprint.get_blueprint` | `manage_blueprint` | `get_blueprint` | read | read | none | `manage_blueprint.get_blueprint` `manage_blueprint.get` |
| `blueprint.get_scs` | `manage_blueprint` | `get_scs` | read | read | none | `manage_blueprint.get_scs` |
| `blueprint.get_widget_info` | `manage_blueprint` | `get_widget_info` | read | read | none | `manage_blueprint.get_widget_info` `manage_blueprint.get_widget_slot_info` |
| `blueprint.inspect_graph` | `manage_blueprint` | `get_graph_details` | read | read | none | `manage_blueprint.inspect_graph` `manage_blueprint.get_graph_details` `manage_blueprint.get_node_details` `manage_blueprint.get_pin_details` `manage_blueprint.list_node_types` |
| `blueprint.probe_handle` | `manage_blueprint` | `probe_handle` | read | read | none | `manage_blueprint.probe_handle` |
| `blueprint.remove_function` | `manage_blueprint` | `remove_function` | destructive | destructive | explicit | `manage_blueprint.remove_function` `manage_blueprint.remove_event` |
| `blueprint.remove_scs_component` | `manage_blueprint` | `remove_scs_component` | destructive | destructive | explicit | `manage_blueprint.remove_scs_component` |
| `blueprint.remove_variable` | `manage_blueprint` | `remove_variable` | destructive | destructive | explicit | `manage_blueprint.remove_variable` |
| `blueprint.remove_widget` | `manage_blueprint` | `remove_widget` | destructive | destructive | explicit | `manage_blueprint.remove_widget` |
| `blueprint.set_font` | `manage_blueprint` | `set_font` | write | write | none | `manage_blueprint.set_font` `manage_blueprint.set_margin` |
| `blueprint.set_widget_layout` | `manage_blueprint` | `set_anchor` | write | write | none | `manage_blueprint.set_widget_layout` `manage_blueprint.set_anchor` `manage_blueprint.set_position` `manage_blueprint.set_size` `manage_blueprint.set_alignment` `manage_blueprint.set_padding` `manage_blueprint.set_z_order` `manage_blueprint.set_visibility` `manage_blueprint.set_clipping` `manage_blueprint.set_render_transform` `manage_blueprint.set_style` |
| `build_environment.add_foliage` | `build_environment` | `add_foliage` | write | write | none | `build_environment.add_foliage` `build_environment.add_foliage_instances` `build_environment.paint_foliage` |
| `build_environment.bake_lightmap` | `build_environment` | `bake_lightmap` | write | write | none | `build_environment.bake_lightmap` |
| `build_environment.build_lighting` | `build_environment` | `build_lighting` | write | write | none | `build_environment.build_lighting` |
| `build_environment.configure_atmosphere` | `build_environment` | `configure_sky_atmosphere` | write | write | none | `build_environment.configure_atmosphere` `build_environment.configure_sky_atmosphere` `build_environment.configure_sky_light` `build_environment.configure_sun_position` `build_environment.configure_directional_light_atmosphere` `build_environment.configure_exponential_height_fog` `build_environment.configure_volumetric_cloud` `build_environment.set_time_of_day` `build_environment.configure_sky_color_curve` `build_environment.configure_light_color_curve` |
| `build_environment.configure_foliage` | `build_environment` | `configure_foliage_mesh` | write | write | none | `build_environment.configure_foliage` `build_environment.configure_foliage_mesh` `build_environment.configure_foliage_placement` `build_environment.configure_foliage_collision` `build_environment.configure_foliage_culling` `build_environment.configure_foliage_lod` |
| `build_environment.configure_lighting` | `build_environment` | `configure_shadows` | write | write | none | `build_environment.configure_lighting` `build_environment.configure_shadows` `build_environment.set_ambient_occlusion` `build_environment.set_exposure` `build_environment.setup_global_illumination` `build_environment.setup_volumetric_fog` |
| `build_environment.configure_lightmass` | `build_environment` | `configure_lightmass_settings` | write | write | none | `build_environment.configure_lightmass` `build_environment.configure_lightmass_settings` `build_environment.build_lighting_quality` `build_environment.configure_indirect_lighting_cache` |
| `build_environment.configure_post_process` | `build_environment` | `configure_bloom` | write | write | none | `build_environment.configure_post_process` `build_environment.configure_bloom` `build_environment.set_bloom_intensity` `build_environment.set_bloom_threshold` `build_environment.configure_exposure` `build_environment.set_exposure_compensation` `build_environment.set_exposure_method` `build_environment.set_exposure_min_max` `build_environment.configure_dof` `build_environment.set_dof_method` `build_environment.set_focal_distance` `build_environment.set_aperture` `build_environment.configure_bokeh` `build_environment.configure_motion_blur` `build_environment.set_motion_blur_amount` `build_environment.set_motion_blur_max` `build_environment.configure_tonemapper` `build_environment.set_tonemapper_type` `build_environment.set_pp_color_grading` `build_environment.set_pp_lut` `build_environment.set_pp_white_balance` `build_environment.configure_vignette` `build_environment.configure_grain` `build_environment.configure_chromatic_aberration` `build_environment.configure_lens_flare` `build_environment.configure_ssao` `build_environment.configure_gtao` `build_environment.configure_ssr_settings` `build_environment.configure_lumen_reflection_settings` `build_environment.configure_screen_percentage` `build_environment.configure_pp_blend` |
| `build_environment.configure_ray_tracing` | `build_environment` | `configure_ray_traced_ao` | write | write | none | `build_environment.configure_ray_tracing` `build_environment.configure_ray_traced_ao` `build_environment.configure_ray_traced_gi` `build_environment.configure_ray_traced_reflections` `build_environment.configure_ray_traced_shadows` `build_environment.configure_path_tracing` |
| `build_environment.configure_scene_capture` | `build_environment` | `assign_render_target` | write | write | none | `build_environment.configure_scene_capture` `build_environment.assign_render_target` `build_environment.configure_capture_source` `build_environment.configure_capture_resolution` `build_environment.configure_capture_offset` `build_environment.configure_planar_reflection` `build_environment.configure_reflection_capture_resolution` `build_environment.capture_scene` `build_environment.recapture_scene` |
| `build_environment.configure_spline_meshes` | `build_environment` | `set_spline_mesh_asset` | write | write | none | `build_environment.configure_spline_meshes` `build_environment.set_spline_mesh_asset` `build_environment.set_spline_mesh_material` `build_environment.configure_spline_mesh_axis` `build_environment.configure_mesh_spacing` `build_environment.configure_mesh_randomization` `build_environment.scatter_meshes_along_spline` |
| `build_environment.configure_water` | `build_environment` | `configure_water_waves` | write | write | none | `build_environment.configure_water` `build_environment.configure_water_waves` `build_environment.configure_water_material` `build_environment.configure_water_collision` |
| `build_environment.configure_weather` | `build_environment` | `configure_rain_particles` | write | write | none | `build_environment.configure_weather` `build_environment.configure_rain_particles` `build_environment.configure_snow_particles` `build_environment.configure_lightning` `build_environment.configure_wind` |
| `build_environment.create_atmosphere_actor` | `build_environment` | `create_sky_sphere` | write | write | none | `build_environment.create_atmosphere_actor` `build_environment.create_sky_sphere` `build_environment.create_fog_volume` `build_environment.create_time_of_day_system` |
| `build_environment.create_buoyancy_component` | `build_environment` | `create_buoyancy_component` | write | write | none | `build_environment.create_buoyancy_component` |
| `build_environment.create_capture_actor` | `build_environment` | `create_scene_capture_2d` | write | write | none | `build_environment.create_capture_actor` `build_environment.create_scene_capture_2d` `build_environment.create_scene_capture_cube` `build_environment.create_sphere_reflection_capture` `build_environment.create_box_reflection_capture` `build_environment.create_planar_reflection` |
| `build_environment.create_foliage_type` | `build_environment` | `add_foliage_type` | write | write | none | `build_environment.create_foliage_type` `build_environment.create_procedural_foliage` |
| `build_environment.create_landscape` | `build_environment` | `create_landscape` | write | write | none | `build_environment.create_landscape` `build_environment.create_landscape_grass_type` |
| `build_environment.create_landscape_asset` | `build_environment` | `create_landscape_layer_info` | write | write | none | `build_environment.create_landscape_asset` `build_environment.create_landscape_layer_info` `build_environment.create_landscape_streaming_proxy` |
| `build_environment.create_light` | `build_environment` | `create_light` | write | write | none | `build_environment.create_light` `build_environment.create_dynamic_light` `build_environment.spawn_light` |
| `build_environment.create_lighting_setup` | `build_environment` | `create_lighting_enabled_level` | write | write | none | `build_environment.create_lighting_setup` `build_environment.create_lighting_enabled_level` `build_environment.create_lightmass_volume` |
| `build_environment.create_procedural_terrain` | `build_environment` | `create_procedural_terrain` | write | write | none | `build_environment.create_procedural_terrain` |
| `build_environment.create_sky_light` | `build_environment` | `create_sky_light` | write | write | none | `build_environment.create_sky_light` `build_environment.ensure_single_sky_light` `build_environment.spawn_sky_light` |
| `build_environment.create_spline` | `build_environment` | `create_spline_actor` | write | write | none | `build_environment.create_spline` `build_environment.create_spline_actor` `build_environment.create_spline_mesh_component` `build_environment.create_road_spline` `build_environment.create_wall_spline` `build_environment.create_fence_spline` `build_environment.create_pipe_spline` `build_environment.create_cable_spline` `build_environment.create_river_spline` |
| `build_environment.create_water_body` | `build_environment` | `create_water_body_ocean` | write | write | none | `build_environment.create_water_body` `build_environment.create_water_body_ocean` `build_environment.create_water_body_lake` `build_environment.create_water_body_river` `build_environment.create_water_body_custom` |
| `build_environment.create_weather_system` | `build_environment` | `create_weather_system` | write | write | none | `build_environment.create_weather_system` |
| `build_environment.delete` | `build_environment` | `delete` | destructive | destructive | explicit | `build_environment.delete` |
| `build_environment.edit_landscape` | `build_environment` | `paint_landscape` | write | write | none | `build_environment.edit_landscape` `build_environment.paint_landscape` `build_environment.paint_landscape_layer` `build_environment.import_heightmap` `build_environment.configure_landscape_lod` `build_environment.configure_landscape_material` `build_environment.configure_landscape_splines` `build_environment.generate_lods` |
| `build_environment.edit_spline` | `build_environment` | `add_spline_point` | write | write | none | `build_environment.edit_spline` `build_environment.add_spline_point` `build_environment.set_spline_point_position` `build_environment.set_spline_point_rotation` `build_environment.set_spline_point_scale` `build_environment.set_spline_point_tangents` `build_environment.set_spline_type` |
| `build_environment.export_heightmap` | `build_environment` | `export_heightmap` | read | read | none | `build_environment.export_heightmap` |
| `build_environment.export_snapshot` | `build_environment` | `export_snapshot` | read | read | none | `build_environment.export_snapshot` |
| `build_environment.get_foliage_instances` | `build_environment` | `get_foliage_instances` | read | read | none | `build_environment.get_foliage_instances` |
| `build_environment.get_splines_info` | `build_environment` | `get_splines_info` | read | read | none | `build_environment.get_splines_info` |
| `build_environment.import_snapshot` | `build_environment` | `import_snapshot` | write | write | none | `build_environment.import_snapshot` |
| `build_environment.list_light_types` | `build_environment` | `list_light_types` | read | read | none | `build_environment.list_light_types` |
| `build_environment.paint_foliage_instances` | `build_environment` | `paint_foliage_instances` | write | write | none | `build_environment.paint_foliage_instances` |
| `build_environment.remove_foliage` | `build_environment` | `remove_foliage` | destructive | destructive | explicit | `build_environment.remove_foliage` `build_environment.remove_foliage_instances` |
| `build_environment.remove_spline_point` | `build_environment` | `remove_spline_point` | destructive | destructive | explicit | `build_environment.remove_spline_point` |
| `build_environment.sculpt` | `build_environment` | `sculpt_landscape` | write | write | none | `build_environment.sculpt` `build_environment.modify_heightmap` `build_environment.set_landscape_material` `build_environment.sculpt_landscape` |
| `build_environment.set_light_channel` | `build_environment` | `set_light_channel` | write | write | none | `build_environment.set_light_channel` `build_environment.set_actor_light_channel` |
| `control_actor.add_tag` | `control_actor` | `add_tag` | write | write | none | `control_actor.add_tag` `control_actor.remove_tag` |
| `control_actor.apply_force` | `control_actor` | `apply_force` | write | write | none | `control_actor.apply_force` |
| `control_actor.attach` | `control_actor` | `attach` | write | write | none | `control_actor.attach` `control_actor.attach_actor` |
| `control_actor.audit_placement` | `control_actor` | `audit_placement` | read | read | none | `control_actor.audit_placement` |
| `control_actor.call_actor_function` | `control_actor` | `call_actor_function` | destructive | destructive | elevated | `control_actor.call_actor_function` |
| `control_actor.create_snapshot` | `control_actor` | `create_snapshot` | read | read | none | `control_actor.create_snapshot` |
| `control_actor.delete` | `control_actor` | `delete` | destructive | destructive | explicit | `control_actor.delete` `control_actor.delete_by_tag` `control_actor.destroy_actor` |
| `control_actor.detach` | `control_actor` | `detach` | write | write | none | `control_actor.detach` `control_actor.detach_actor` |
| `control_actor.duplicate` | `control_actor` | `duplicate` | write | write | none | `control_actor.duplicate` |
| `control_actor.edit_component` | `control_actor` | `add_component` | write | write | none | `control_actor.edit_component` `control_actor.add_component` `control_actor.remove_component` `control_actor.set_component_property` `control_actor.set_component_properties` |
| `control_actor.find` | `control_actor` | `find_by_class` | read | read | none | `control_actor.find` `control_actor.find_by_class` `control_actor.find_by_name` `control_actor.find_actors_by_class` `control_actor.find_actors_by_name` |
| `control_actor.find_by_tag` | `control_actor` | `find_by_tag` | read | read | none | `control_actor.find_by_tag` `control_actor.find_actors_by_tag` |
| `control_actor.get_component_property` | `control_actor` | `get_component_property` | read | read | none | `control_actor.get_component_property` |
| `control_actor.get_components` | `control_actor` | `get_components` | read | read | none | `control_actor.get_components` `control_actor.get_actor_bounds` `control_actor.get_actor_components` |
| `control_actor.get_transform` | `control_actor` | `get_transform` | read | read | none | `control_actor.get_transform` `control_actor.get_actor_transform` |
| `control_actor.list` | `control_actor` | `list` | read | read | none | `control_actor.list` |
| `control_actor.set_actor_collision` | `control_actor` | `set_actor_collision` | write | write | none | `control_actor.set_actor_collision` |
| `control_actor.set_blueprint_variables` | `control_actor` | `set_blueprint_variables` | write | write | none | `control_actor.set_blueprint_variables` |
| `control_actor.set_material` | `control_actor` | `set_material` | write | write | none | `control_actor.set_material` `control_actor.apply_material` `control_actor.set_actor_material` |
| `control_actor.set_transform` | `control_actor` | `set_transform` | write | write | none | `control_actor.set_transform` `control_actor.set_actor_location` `control_actor.set_actor_rotation` `control_actor.set_actor_scale` `control_actor.teleport_actor` `control_actor.set_actor_transform` |
| `control_actor.set_visibility` | `control_actor` | `set_visibility` | write | write | none | `control_actor.set_visibility` `control_actor.set_actor_visible` |
| `control_actor.spawn` | `control_actor` | `spawn` | write | write | none | `control_actor.spawn` `control_actor.spawn_blueprint` `control_actor.spawn_batch` `control_actor.spawn_actor` |
| `control_editor.close_asset` | `control_editor` | `close_asset` | write | write | none | `control_editor.close_asset` |
| `control_editor.configure_editor` | `control_editor` | `control_editor` | write | write | none | `control_editor.configure_editor` `control_editor.open_editor_tab` `control_editor.set_preferences` |
| `control_editor.configure_viewport` | `control_editor` | `set_view_mode` | read | read | none | `control_editor.configure_viewport` `control_editor.set_view_mode` `control_editor.set_editor_mode` `control_editor.set_game_view` `control_editor.set_immersive_mode` `control_editor.set_viewport_realtime` `control_editor.show_stats` `control_editor.hide_stats` |
| `control_editor.console_command` | `control_editor` | `console_command` | write | write | none | `control_editor.console_command` `control_editor.execute_command` |
| `control_editor.create_bookmark` | `control_editor` | `create_bookmark` | write | write | none | `control_editor.create_bookmark` |
| `control_editor.describe_reflected_api` | `control_editor` | `control_editor` | read | read | none | `control_editor.describe_reflected_api` |
| `control_editor.focus_actor` | `control_editor` | `focus_actor` | read | read | none | `control_editor.focus_actor` |
| `control_editor.invoke_reflected_function` | `control_editor` | `control_editor` | destructive | destructive | elevated | `control_editor.invoke_reflected_function` |
| `control_editor.jump_to_bookmark` | `control_editor` | `jump_to_bookmark` | read | read | none | `control_editor.jump_to_bookmark` |
| `control_editor.open_asset` | `control_editor` | `open_asset` | read | read | none | `control_editor.open_asset` |
| `control_editor.open_level` | `control_editor` | `open_level` | write | write | none | `control_editor.open_level` |
| `control_editor.play` | `control_editor` | `play` | write | write | none | `control_editor.play` `control_editor.pause` `control_editor.resume` `control_editor.stop` `control_editor.eject` `control_editor.possess` `control_editor.stop_pie` |
| `control_editor.restart_editor` | `control_editor` | `restart_editor` | destructive | destructive | explicit | `control_editor.restart_editor` |
| `control_editor.save_all` | `control_editor` | `save_all` | write | write | none | `control_editor.save_all` |
| `control_editor.screenshot` | `control_editor` | `screenshot` | read | read | none | `control_editor.screenshot` `control_editor.take_screenshot` |
| `control_editor.set_camera` | `control_editor` | `set_camera` | read | read | none | `control_editor.set_camera` `control_editor.set_camera_fov` `control_editor.set_view_target` `control_editor.set_camera_position` `control_editor.set_viewport_camera` `control_editor.set_game_view_target` |
| `control_editor.set_game_speed` | `control_editor` | `set_game_speed` | write | write | none | `control_editor.set_game_speed` `control_editor.set_fixed_delta_time` `control_editor.step_frame` `control_editor.single_frame_step` |
| `control_editor.set_viewport_resolution` | `control_editor` | `console_command` | read | read | none | `control_editor.set_viewport_resolution` |
| `control_editor.simulate_input` | `control_editor` | `simulate_input` | write | write | none | `control_editor.simulate_input` |
| `control_editor.start_recording` | `control_editor` | `start_recording` | write | write | none | `control_editor.start_recording` `control_editor.stop_recording` |
| `control_editor.undo` | `control_editor` | `undo` | write | write | none | `control_editor.undo` `control_editor.redo` |
| `datatable.delete_data_table_row` | `manage_asset` | `delete_data_table_row` | destructive | destructive | elevated | `manage_asset.delete_data_table_row` `manage_asset.clear_data_table_rows` |
| `datatable.edit_data_table` | `manage_asset` | `create_data_table` | write | write | explicit | `manage_asset.edit_data_table` `manage_asset.create_data_table` `manage_asset.create_row_struct` `manage_asset.add_data_table_row` `manage_asset.update_data_table_row` `manage_asset.import_data_table_rows` `manage_asset.set_data_table_row_struct` `manage_asset.set_struct_as_row_struct` |
| `datatable.inspect_data_table` | `manage_asset` | `get_data_table_row` | read | read | none | `manage_asset.inspect_data_table` `manage_asset.get_data_table_row` `manage_asset.list_data_table_rows` `manage_asset.get_row_struct` |
| `enum.delete_enum` | `manage_asset` | `delete_enum` | destructive | destructive | elevated | `manage_asset.delete_enum` `manage_asset.remove_enum_value` |
| `enum.edit_enum` | `manage_asset` | `create_enum` | write | write | explicit | `manage_asset.edit_enum` `manage_asset.create_enum` `manage_asset.add_enum_value` `manage_asset.rename_enum_value` `manage_asset.reorder_enum_values` `manage_asset.set_enum_value_metadata` `manage_asset.split_enum` |
| `enum.get_enum` | `manage_asset` | `get_enum` | read | read | none | `manage_asset.get_enum` |
| `inspect.add_tag` | `inspect` | `control_actor` | write | write | none | `inspect.add_tag` |
| `inspect.create_snapshot` | `inspect` | `control_actor` | write | write | none | `inspect.create_snapshot` `inspect.restore_snapshot` |
| `inspect.delete_object` | `inspect` | `control_actor` | destructive | destructive | explicit | `inspect.delete_object` |
| `inspect.find_by_class` | `inspect` | `find_by_class` | read | read | none | `inspect.find_by_class` |
| `inspect.get_blueprint_details` | `inspect` | `blueprint_get` | read | read | none | `inspect.get_blueprint_details` |
| `inspect.get_component_details` | `inspect` | `control_actor` | read | read | none | `inspect.get_component_details` `inspect.get_component_property` |
| `inspect.get_components` | `inspect` | `get_components` | read | read | none | `inspect.get_components` |
| `inspect.get_editor_state` | `inspect` | `get_selected_actors` | read | read | none | `inspect.get_editor_state` `inspect.get_selected_actors` `inspect.get_viewport_info` `inspect.get_world_settings` `inspect.get_project_settings` `inspect.get_editor_settings` |
| `inspect.get_property` | `inspect` | `get_property` | read | read | none | `inspect.get_property` |
| `inspect.get_stats` | `inspect` | `get_performance_stats` | read | read | none | `inspect.get_stats` `inspect.get_performance_stats` `inspect.get_memory_stats` `inspect.get_scene_stats` |
| `inspect.inspect_class` | `inspect` | `inspect_class` | read | read | none | `inspect.inspect_class` `inspect.inspect_struct` `inspect.inspect_cdo` |
| `inspect.inspect_object` | `inspect` | `inspect_object` | read | read | none | `inspect.inspect_object` `inspect.get_actor_details` `inspect.get_level_details` `inspect.get_material_details` `inspect.get_mesh_details` `inspect.get_texture_details` |
| `inspect.query_object` | `inspect` | `control_actor` | read | read | none | `inspect.query_object` `inspect.list_objects` `inspect.find_by_tag` `inspect.get_metadata` `inspect.get_bounding_box` `inspect.export` |
| `inspect.runtime_report` | `inspect` | `runtime_report` | read | read | none | `inspect.runtime_report` `inspect.pie_report` |
| `inspect.set_component_property` | `inspect` | `control_actor` | write | write | none | `inspect.set_component_property` |
| `inspect.set_property` | `inspect` | `set_property` | write | write | none | `inspect.set_property` |
| `manage_ai.configure_navigation` | `manage_ai` | `configure_nav_mesh_settings` | write | write | none | `manage_ai.configure_navigation` `manage_ai.configure_nav_mesh_settings` `manage_ai.set_nav_agent_properties` `manage_ai.configure_nav_area_cost` `manage_ai.set_nav_area_class` `manage_ai.configure_nav_link` `manage_ai.set_nav_link_type` `manage_ai.configure_smart_link_behavior` `manage_ai.rebuild_navigation` |
| `manage_ai.create_ai_controller` | `manage_ai` | `create_ai_controller` | write | write | none | `manage_ai.create_ai_controller` |
| `manage_ai.create_behavior_tree` | `manage_ai` | `create_behavior_tree` | write | write | none | `manage_ai.create_behavior_tree` `manage_ai.create` `manage_ai.create_blackboard` `manage_ai.create_blackboard_asset` |
| `manage_ai.create_nav_actor` | `manage_ai` | `create_nav_link_proxy` | write | write | none | `manage_ai.create_nav_actor` `manage_ai.create_nav_link_proxy` `manage_ai.create_smart_link` `manage_ai.create_nav_modifier` `manage_ai.create_nav_modifier_component` |
| `manage_ai.edit_behavior_tree` | `manage_ai` | `add_composite_node` | write | write | none | `manage_ai.edit_behavior_tree` `manage_ai.add_composite_node` `manage_ai.add_task_node` `manage_ai.add_decorator` `manage_ai.add_service` `manage_ai.add_node` `manage_ai.add_subnode` `manage_ai.connect_nodes` `manage_ai.break_connections` `manage_ai.configure_bt_node` `manage_ai.set_node_properties` `manage_ai.remove_node` |
| `manage_ai.edit_blackboard` | `manage_ai` | `add_blackboard_key` | write | write | none | `manage_ai.edit_blackboard` `manage_ai.add_blackboard_key` `manage_ai.set_blackboard_value` `manage_ai.set_key_instance_synced` |
| `manage_ai.edit_eqs_query` | `manage_ai` | `create_eqs_query` | write | write | none | `manage_ai.edit_eqs_query` `manage_ai.create_eqs_query` `manage_ai.add_eqs_generator` `manage_ai.add_eqs_test` `manage_ai.add_eqs_context` `manage_ai.configure_test_scoring` |
| `manage_ai.edit_mass_entity` | `manage_ai` | `create_mass_entity_config` | write | write | none | `manage_ai.edit_mass_entity` `manage_ai.create_mass_entity_config` `manage_ai.configure_mass_entity` `manage_ai.add_mass_spawner` |
| `manage_ai.edit_smart_object` | `manage_ai` | `create_smart_object_definition` | write | write | none | `manage_ai.edit_smart_object` `manage_ai.create_smart_object_definition` `manage_ai.add_smart_object_slot` `manage_ai.configure_slot_behavior` `manage_ai.add_smart_object_component` |
| `manage_ai.edit_state_tree` | `manage_ai` | `create_state_tree` | write | write | none | `manage_ai.edit_state_tree` `manage_ai.create_state_tree` `manage_ai.add_state_tree_state` `manage_ai.add_state_tree_transition` `manage_ai.configure_state_tree_task` |
| `manage_ai.get_ai_info` | `manage_ai` | `get_ai_info` | read | read | none | `manage_ai.get_ai_info` `manage_ai.get_navigation_info` |
| `manage_ai.get_tree` | `manage_ai` | `get_tree` | read | read | none | `manage_ai.get_tree` `manage_ai.get_blackboard_value` |
| `manage_ai.run_behavior_tree` | `manage_ai` | `run_behavior_tree` | write | write | none | `manage_ai.run_behavior_tree` `manage_ai.stop_behavior_tree` `manage_ai.assign_behavior_tree` `manage_ai.assign_blackboard` |
| `manage_ai.set_ai_movement` | `manage_ai` | `set_ai_movement` | write | write | none | `manage_ai.set_ai_movement` |
| `manage_ai.set_focus` | `manage_ai` | `set_focus` | write | write | none | `manage_ai.set_focus` `manage_ai.clear_focus` |
| `manage_ai.setup_perception` | `manage_ai` | `setup_perception` | write | write | none | `manage_ai.setup_perception` `manage_ai.add_ai_perception_component` `manage_ai.configure_sight_config` `manage_ai.configure_hearing_config` `manage_ai.configure_damage_sense_config` `manage_ai.set_perception_team` `manage_ai.set_ai_perception` |
| `manage_audio.configure_sound_attenuation` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.configure_sound_attenuation` `manage_audio.configure_distance_attenuation` `manage_audio.configure_spatialization` `manage_audio.configure_occlusion` `manage_audio.configure_reverb_send` `manage_audio.set_audio_occlusion` `manage_audio.set_doppler_effect` `manage_audio.set_sound_attenuation` |
| `manage_audio.configure_sound_class` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.configure_sound_class` `manage_audio.set_class_parent` `manage_audio.set_class_properties` `manage_audio.add_mix_modifier` `manage_audio.configure_mix_eq` |
| `manage_audio.control_sound_mix` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.control_sound_mix` `manage_audio.push_sound_mix` `manage_audio.pop_sound_mix` `manage_audio.set_base_sound_mix` `manage_audio.set_sound_mix_class_override` `manage_audio.clear_sound_mix_class_override` |
| `manage_audio.create_audio_actor` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_audio_actor` `manage_audio.create_ambient_sound` `manage_audio.create_audio_component` `manage_audio.create_reverb_zone` |
| `manage_audio.create_audio_asset` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_audio_asset` `manage_audio.create_sound_cue` `manage_audio.create_sound_class` `manage_audio.create_sound_mix` `manage_audio.create_attenuation_settings` `manage_audio.create_reverb_effect` `manage_audio.create_dialogue_voice` `manage_audio.create_dialogue_wave` `manage_audio.create_source_effect_chain` `manage_audio.create_submix_effect` |
| `manage_audio.edit_metasound` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.edit_metasound` `manage_audio.create_metasound` `manage_audio.add_metasound_input` `manage_audio.add_metasound_output` `manage_audio.add_metasound_node` `manage_audio.connect_metasound_nodes` `manage_audio.set_metasound_default` `manage_audio.build_metasound` |
| `manage_audio.edit_sound_cue` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.edit_sound_cue` `manage_audio.add_cue_node` `manage_audio.connect_cue_nodes` `manage_audio.set_cue_attenuation` `manage_audio.set_cue_concurrency` `manage_audio.add_source_effect` |
| `manage_audio.enable_audio_analysis` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.enable_audio_analysis` |
| `manage_audio.fade_sound` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.fade_sound` `manage_audio.fade_sound_in` `manage_audio.fade_sound_out` |
| `manage_audio.get_audio_info` | `manage_audio` | `manage_audio` | read | read | none | `manage_audio.get_audio_info` |
| `manage_audio.play_sound` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.play_sound` `manage_audio.play_sound_2d` `manage_audio.play_sound_at_location` `manage_audio.spawn_sound_at_location` `manage_audio.play_sound_attached` `manage_audio.prime_sound` |
| `manage_audio.set_dialogue_context` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.set_dialogue_context` |
| `manage_character.build_metahuman` | `manage_character` | `build_metahuman` | write | write | none | `manage_character.build_metahuman` |
| `manage_character.configure_character` | `manage_character` | `configure_movement_speeds` | write | write | none | `manage_character.configure_character` `manage_character.configure_movement_speeds` `manage_character.configure_jump` `manage_character.configure_crouch` `manage_character.configure_sprint` `manage_character.configure_rotation` `manage_character.configure_capsule_component` `manage_character.configure_mesh_component` `manage_character.configure_camera_component` `manage_character.configure_nav_movement` `manage_character.configure_footstep_fx` `manage_character.add_custom_movement_mode` `manage_character.map_surface_to_sound` |
| `manage_character.create_character_blueprint` | `manage_character` | `create_character_blueprint` | write | write | none | `manage_character.create_character_blueprint` |
| `manage_character.create_metahuman` | `manage_character` | `create_metahuman` | write | write | none | `manage_character.create_metahuman` |
| `manage_character.export_metahuman` | `manage_character` | `export_metahuman` | write | write | none | `manage_character.export_metahuman` |
| `manage_character.get_character_info` | `manage_character` | `get_character_info` | read | read | none | `manage_character.get_character_info` |
| `manage_character.metahuman_status` | `manage_character` | `metahuman_status` | read | read | none | `manage_character.metahuman_status` |
| `manage_character.rig_metahuman` | `manage_character` | `rig_metahuman` | write | write | none | `manage_character.rig_metahuman` |
| `manage_character.set_movement_property` | `manage_character` | `set_walk_speed` | write | write | none | `manage_character.set_movement_property` `manage_character.set_walk_speed` `manage_character.set_jump_height` `manage_character.set_gravity_scale` `manage_character.set_ground_friction` `manage_character.set_braking_deceleration` |
| `manage_character.setup_character_ability` | `manage_character` | `setup_movement` | write | write | none | `manage_character.setup_character_ability` `manage_character.setup_movement` `manage_character.setup_climbing` `manage_character.setup_mantling` `manage_character.setup_vaulting` `manage_character.setup_sliding` `manage_character.setup_wall_running` `manage_character.setup_grappling` `manage_character.setup_footstep_system` |
| `manage_combat.configure_damage` | `manage_combat` | `configure_damage_execution` | write | write | none | `manage_combat.configure_damage` `manage_combat.configure_damage_execution` `manage_combat.configure_hit_detection` `manage_combat.setup_hitbox_component` `manage_combat.configure_hit_reaction` `manage_combat.configure_impact_effects` `manage_combat.configure_combo_system` `manage_combat.create_hit_pause` `manage_combat.create_melee_trace` `manage_combat.setup_parry_block_system` `manage_combat.create_shield` `manage_combat.modify_armor` `manage_combat.apply_damage` `manage_combat.heal` |
| `manage_combat.configure_projectile` | `manage_combat` | `configure_projectile` | write | write | none | `manage_combat.configure_projectile` `manage_combat.configure_projectile_movement` `manage_combat.configure_projectile_collision` `manage_combat.configure_projectile_homing` |
| `manage_combat.configure_weapon` | `manage_combat` | `configure_weapon_mesh` | write | write | none | `manage_combat.configure_weapon` `manage_combat.configure_weapon_mesh` `manage_combat.configure_weapon_sockets` `manage_combat.configure_hitscan` `manage_combat.configure_spread_pattern` `manage_combat.configure_recoil_pattern` `manage_combat.configure_aim_down_sights` `manage_combat.configure_muzzle_flash` `manage_combat.configure_tracer` `manage_combat.configure_shell_ejection` `manage_combat.configure_weapon_trails` `manage_combat.set_weapon_stats` `manage_combat.setup_ammo_system` `manage_combat.setup_reload_system` `manage_combat.setup_attachment_system` `manage_combat.setup_weapon_switching` |
| `manage_combat.create_combat_asset` | `manage_combat` | `create_weapon_blueprint` | write | write | none | `manage_combat.create_combat_asset` `manage_combat.create_weapon_blueprint` `manage_combat.create_projectile_blueprint` `manage_combat.create_damage_type` `manage_combat.setup_damage_type` `manage_combat.create_damage_effect` |
| `manage_combat.get_combat_info` | `manage_combat` | `get_combat_info` | read | read | none | `manage_combat.get_combat_info` `manage_combat.get_combat_stats` |
| `manage_effect.activate` | `manage_effect` | `activate` | write | write | none | `manage_effect.activate` `manage_effect.deactivate` `manage_effect.reset` `manage_effect.activate_effect` |
| `manage_effect.add_niagara_data_interface` | `manage_effect` | `add_static_mesh_data_interface` | write | write | none | `manage_effect.add_niagara_data_interface` `manage_effect.add_static_mesh_data_interface` `manage_effect.add_skeletal_mesh_data_interface` `manage_effect.add_spline_data_interface` `manage_effect.add_collision_query_data_interface` `manage_effect.add_audio_spectrum_data_interface` |
| `manage_effect.add_niagara_module` | `manage_effect` | `add_niagara_module` | write | write | none | `manage_effect.add_niagara_module` `manage_effect.add_spawn_rate_module` `manage_effect.add_spawn_burst_module` `manage_effect.add_spawn_per_unit_module` `manage_effect.add_initialize_particle_module` `manage_effect.add_velocity_module` `manage_effect.add_acceleration_module` `manage_effect.add_force_module` `manage_effect.add_color_module` `manage_effect.add_size_module` `manage_effect.add_collision_module` `manage_effect.add_kill_particles_module` `manage_effect.add_camera_offset_module` `manage_effect.add_particle_state_module` `manage_effect.add_sprite_renderer_module` `manage_effect.add_mesh_renderer_module` `manage_effect.add_ribbon_renderer_module` `manage_effect.add_light_renderer_module` `manage_effect.add_simulation_stage` `manage_effect.add_event_generator` `manage_effect.add_event_receiver` |
| `manage_effect.advance_simulation` | `manage_effect` | `advance_simulation` | write | write | none | `manage_effect.advance_simulation` |
| `manage_effect.cleanup` | `manage_effect` | `cleanup` | write | write | none | `manage_effect.cleanup` `manage_effect.clear_debug_shapes` |
| `manage_effect.create_dynamic_light` | `manage_effect` | `create_dynamic_light` | write | write | none | `manage_effect.create_dynamic_light` |
| `manage_effect.create_effect` | `manage_effect` | `create_niagara_system` | write | write | none | `manage_effect.create_effect` `manage_effect.create_niagara_system` `manage_effect.create_niagara_emitter` `manage_effect.create_niagara_ribbon` `manage_effect.create_particle_trail` `manage_effect.create_impact_effect` `manage_effect.create_environment_effect` `manage_effect.create_volumetric_fog` `manage_effect.niagara` `manage_effect.particle` |
| `manage_effect.debug_shape` | `manage_effect` | `debug_shape` | write | write | none | `manage_effect.debug_shape` |
| `manage_effect.edit_niagara_system` | `manage_effect` | `add_emitter_to_system` | write | write | none | `manage_effect.edit_niagara_system` `manage_effect.add_emitter_to_system` `manage_effect.add_user_parameter` `manage_effect.bind_parameter_to_source` `manage_effect.set_parameter_value` `manage_effect.set_niagara_parameter` `manage_effect.set_niagara_dynamic_input` `manage_effect.set_emitter_properties` `manage_effect.configure_event_payload` `manage_effect.enable_gpu_simulation` `manage_effect.connect_niagara_pins` |
| `manage_effect.get_niagara_info` | `manage_effect` | `get_niagara_info` | read | read | none | `manage_effect.get_niagara_info` `manage_effect.validate_niagara_system` |
| `manage_effect.list_debug_shapes` | `manage_effect` | `list_debug_shapes` | read | read | none | `manage_effect.list_debug_shapes` |
| `manage_effect.remove_niagara_node` | `manage_effect` | `remove_niagara_node` | destructive | destructive | explicit | `manage_effect.remove_niagara_node` |
| `manage_effect.spawn_niagara` | `manage_effect` | `spawn_niagara` | write | write | none | `manage_effect.spawn_niagara` |
| `manage_gas.add_tag_to_asset` | `manage_gas` | `add_tag_to_asset` | write | write | none | `manage_gas.add_tag_to_asset` |
| `manage_gas.configure_ability` | `manage_gas` | `set_ability_tags` | write | write | none | `manage_gas.configure_ability` `manage_gas.set_ability_tags` `manage_gas.set_ability_cooldown` `manage_gas.set_ability_costs` `manage_gas.set_ability_targeting` `manage_gas.set_activation_policy` `manage_gas.set_instancing_policy` `manage_gas.add_ability_task` `manage_gas.add_ability` `manage_gas.grant_ability` |
| `manage_gas.configure_asc` | `manage_gas` | `configure_asc` | write | write | none | `manage_gas.configure_asc` `manage_gas.add_ability_system_component` |
| `manage_gas.configure_attribute_set` | `manage_gas` | `add_attribute` | write | write | none | `manage_gas.configure_attribute_set` `manage_gas.add_attribute` `manage_gas.set_attribute_base_value` `manage_gas.set_attribute_clamping` |
| `manage_gas.configure_gameplay_cue` | `manage_gas` | `configure_cue_trigger` | write | write | none | `manage_gas.configure_gameplay_cue` `manage_gas.configure_cue_trigger` `manage_gas.set_cue_effects` |
| `manage_gas.configure_gameplay_effect` | `manage_gas` | `set_effect_duration` | write | write | none | `manage_gas.configure_gameplay_effect` `manage_gas.set_effect_duration` `manage_gas.set_effect_stacking` `manage_gas.set_effect_tags` `manage_gas.add_effect_modifier` `manage_gas.set_modifier_magnitude` `manage_gas.add_effect_execution_calculation` `manage_gas.add_effect_cue` |
| `manage_gas.create_gas_asset` | `manage_gas` | `create_gameplay_ability` | write | write | none | `manage_gas.create_gas_asset` `manage_gas.create_gameplay_ability` `manage_gas.create_gameplay_effect` `manage_gas.create_attribute_set` `manage_gas.create_ability_set` `manage_gas.create_gameplay_cue_notify` `manage_gas.create_execution_calculation` |
| `manage_gas.get_gas_info` | `manage_gas` | `get_gas_info` | read | read | none | `manage_gas.get_gas_info` |
| `manage_geometry.array_mesh` | `manage_geometry` | `array_linear` | write | write | none | `manage_geometry.array_mesh` `manage_geometry.array_linear` `manage_geometry.array_radial` |
| `manage_geometry.boolean_mesh` | `manage_geometry` | `boolean_union` | write | write | none | `manage_geometry.boolean_mesh` `manage_geometry.boolean_union` `manage_geometry.boolean_subtract` `manage_geometry.boolean_intersection` `manage_geometry.boolean_trim` `manage_geometry.self_union` |
| `manage_geometry.configure_mesh_collision` | `manage_geometry` | `generate_collision` | write | write | none | `manage_geometry.configure_mesh_collision` `manage_geometry.generate_collision` `manage_geometry.generate_complex_collision` `manage_geometry.simplify_collision` |
| `manage_geometry.configure_mesh_lods` | `manage_geometry` | `generate_lods` | write | write | none | `manage_geometry.configure_mesh_lods` `manage_geometry.generate_lods` `manage_geometry.set_lod_settings` `manage_geometry.set_lod_screen_sizes` |
| `manage_geometry.convert_to_nanite` | `manage_geometry` | `convert_to_nanite` | write | write | none | `manage_geometry.convert_to_nanite` |
| `manage_geometry.convert_to_static_mesh` | `manage_geometry` | `convert_to_static_mesh` | write | write | none | `manage_geometry.convert_to_static_mesh` |
| `manage_geometry.create_primitive` | `manage_geometry` | `create_box` | write | write | none | `manage_geometry.create_primitive` `manage_geometry.create_box` `manage_geometry.create_sphere` `manage_geometry.create_cylinder` `manage_geometry.create_cone` `manage_geometry.create_capsule` `manage_geometry.create_plane` `manage_geometry.create_disc` `manage_geometry.create_ring` `manage_geometry.create_torus` `manage_geometry.create_pipe` `manage_geometry.create_arch` `manage_geometry.create_ramp` `manage_geometry.create_stairs` `manage_geometry.create_spiral_stairs` |
| `manage_geometry.deform_mesh` | `manage_geometry` | `bend` | write | write | none | `manage_geometry.deform_mesh` `manage_geometry.bend` `manage_geometry.twist` `manage_geometry.taper` `manage_geometry.stretch` `manage_geometry.spherify` `manage_geometry.cylindrify` `manage_geometry.smooth` `manage_geometry.relax` `manage_geometry.noise_deform` `manage_geometry.lattice_deform` `manage_geometry.displace_by_texture` `manage_geometry.poke` `manage_geometry.triangulate` |
| `manage_geometry.edit_dynamic_mesh` | `manage_geometry` | `create_procedural_mesh` | write | write | none | `manage_geometry.edit_dynamic_mesh` `manage_geometry.create_procedural_mesh` `manage_geometry.append_vertex` `manage_geometry.append_triangle` `manage_geometry.set_vertex_position` `manage_geometry.set_vertex_color` `manage_geometry.set_uvs` `manage_geometry.split_normals` `manage_geometry.translate_mesh` `manage_geometry.difference` |
| `manage_geometry.edit_uvs` | `manage_geometry` | `auto_uv` | write | write | none | `manage_geometry.edit_uvs` `manage_geometry.auto_uv` `manage_geometry.unwrap_uv` `manage_geometry.project_uv` `manage_geometry.pack_uv_islands` `manage_geometry.transform_uvs` |
| `manage_geometry.get_mesh_info` | `manage_geometry` | `get_mesh_info` | read | read | none | `manage_geometry.get_mesh_info` |
| `manage_geometry.get_vertex_position` | `manage_geometry` | `get_vertex_position` | read | read | none | `manage_geometry.get_vertex_position` |
| `manage_geometry.mirror` | `manage_geometry` | `mirror` | write | write | none | `manage_geometry.mirror` |
| `manage_geometry.model_mesh` | `manage_geometry` | `extrude` | write | write | none | `manage_geometry.model_mesh` `manage_geometry.extrude` `manage_geometry.inset` `manage_geometry.outset` `manage_geometry.offset_faces` `manage_geometry.bevel` `manage_geometry.chamfer` `manage_geometry.bridge` `manage_geometry.loft` `manage_geometry.sweep` `manage_geometry.revolve` `manage_geometry.shell` `manage_geometry.loop_cut` `manage_geometry.edge_split` `manage_geometry.quadrangulate` `manage_geometry.extrude_along_spline` `manage_geometry.duplicate_along_spline` |
| `manage_geometry.optimize_mesh` | `manage_geometry` | `simplify_mesh` | write | write | none | `manage_geometry.optimize_mesh` `manage_geometry.simplify_mesh` `manage_geometry.remesh_uniform` `manage_geometry.remesh_voxel` `manage_geometry.subdivide` `manage_geometry.merge_vertices` `manage_geometry.weld_vertices` `manage_geometry.remove_degenerates` `manage_geometry.fill_holes` `manage_geometry.flip_normals` `manage_geometry.recalculate_normals` `manage_geometry.recompute_tangents` |
| `manage_interaction.configure_destruction` | `manage_interaction` | `add_destruction_component` | write | write | none | `manage_interaction.configure_destruction` `manage_interaction.add_destruction_component` `manage_interaction.setup_destructible_mesh` `manage_interaction.configure_destruction_damage` `manage_interaction.configure_destruction_effects` `manage_interaction.configure_destruction_levels` |
| `manage_interaction.configure_interactable` | `manage_interaction` | `configure_door_properties` | write | write | none | `manage_interaction.configure_interactable` `manage_interaction.configure_door_properties` `manage_interaction.configure_chest_properties` `manage_interaction.configure_switch_properties` `manage_interaction.configure_trigger_events` `manage_interaction.configure_trigger_filter` `manage_interaction.configure_trigger_response` `manage_interaction.configure_interaction_trace` `manage_interaction.configure_interaction_widget` `manage_interaction.add_interaction_events` |
| `manage_interaction.create_interactable` | `manage_interaction` | `create_door_actor` | write | write | none | `manage_interaction.create_interactable` `manage_interaction.create_door_actor` `manage_interaction.create_chest_actor` `manage_interaction.create_switch_actor` `manage_interaction.create_lever_actor` `manage_interaction.create_trigger_actor` `manage_interaction.create_interactable_interface` `manage_interaction.create_interaction_component` |
| `manage_interaction.get_interaction_info` | `manage_interaction` | `get_interaction_info` | read | read | none | `manage_interaction.get_interaction_info` |
| `manage_inventory.configure_crafting` | `manage_inventory` | `add_crafting_component` | write | write | none | `manage_inventory.configure_crafting` `manage_inventory.add_crafting_component` `manage_inventory.add_recipe_ingredient` `manage_inventory.configure_recipe_requirements` `manage_inventory.configure_station_recipes` |
| `manage_inventory.configure_equipment` | `manage_inventory` | `create_equipment_component` | write | write | none | `manage_inventory.configure_equipment` `manage_inventory.create_equipment_component` `manage_inventory.add_equipment_functions` `manage_inventory.define_equipment_slots` `manage_inventory.configure_equipment_visuals` `manage_inventory.configure_equipment_effects` |
| `manage_inventory.configure_inventory` | `manage_inventory` | `create_inventory_component` | write | write | none | `manage_inventory.configure_inventory` `manage_inventory.create_inventory_component` `manage_inventory.add_inventory_functions` `manage_inventory.configure_inventory_slots` `manage_inventory.configure_inventory_weight` `manage_inventory.configure_inventory_events` `manage_inventory.set_inventory_replication` |
| `manage_inventory.configure_item` | `manage_inventory` | `set_item_properties` | write | write | none | `manage_inventory.configure_item` `manage_inventory.set_item_properties` `manage_inventory.set_item_icon` `manage_inventory.configure_item_stacking` `manage_inventory.assign_item_category` |
| `manage_inventory.configure_loot` | `manage_inventory` | `add_loot_entry` | write | write | none | `manage_inventory.configure_loot` `manage_inventory.add_loot_entry` `manage_inventory.remove_loot_entry` `manage_inventory.set_loot_quality_tiers` `manage_inventory.configure_loot_drop` |
| `manage_inventory.configure_pickup` | `manage_inventory` | `configure_pickup_interaction` | write | write | none | `manage_inventory.configure_pickup` `manage_inventory.configure_pickup_interaction` `manage_inventory.configure_pickup_effects` `manage_inventory.configure_pickup_respawn` |
| `manage_inventory.create_inventory_asset` | `manage_inventory` | `create_item_data_asset` | write | write | none | `manage_inventory.create_inventory_asset` `manage_inventory.create_item_data_asset` `manage_inventory.create_item_category` `manage_inventory.create_loot_table` `manage_inventory.create_crafting_recipe` `manage_inventory.create_crafting_station` `manage_inventory.create_pickup_actor` |
| `manage_inventory.get_inventory_info` | `manage_inventory` | `get_inventory_info` | read | read | none | `manage_inventory.get_inventory_info` |
| `manage_level.add_sublevel` | `manage_level` | `add_sublevel` | write | write | none | `manage_level.add_sublevel` |
| `manage_level.build_lighting` | `manage_level` | `manage_lighting` | write | write | none | `manage_level.build_lighting` |
| `manage_level.create_level` | `manage_level` | `manage_level_structure` | write | write | none | `manage_level.create_level` |
| `manage_level.create_light` | `manage_level` | `manage_lighting` | write | write | none | `manage_level.create_light` |
| `manage_level.delete` | `manage_level` | `delete_level` | destructive | destructive | explicit | `manage_level.delete` `manage_level.delete_level` |
| `manage_level.duplicate_level` | `manage_level` | `duplicate` | write | write | none | `manage_level.duplicate_level` `manage_level.rename_level` |
| `manage_level.export_level` | `manage_level` | `export_level` | read | read | none | `manage_level.export_level` |
| `manage_level.get_summary` | `manage_level` | `get_summary` | read | read | none | `manage_level.get_summary` `manage_level.get_current_level` |
| `manage_level.import_level` | `manage_level` | `import_level` | write | write | none | `manage_level.import_level` |
| `manage_level.list_levels` | `manage_level` | `list_levels` | read | read | none | `manage_level.list_levels` |
| `manage_level.load` | `manage_level` | `load` | write | write | none | `manage_level.load` `manage_level.load_level` |
| `manage_level.save` | `manage_level` | `save` | write | write | none | `manage_level.save` `manage_level.save_as` `manage_level.save_level` `manage_level.save_level_as` |
| `manage_level.set_metadata` | `manage_level` | `set_metadata` | write | write | none | `manage_level.set_metadata` |
| `manage_level.set_world_settings` | `manage_level` | `set_level_world_settings` | write | write | none | `manage_level.set_world_settings` |
| `manage_level.stream` | `manage_level` | `stream_level` | write | write | none | `manage_level.stream` `manage_level.unload` |
| `manage_level.unload_level` | `manage_level` | `unload_level` | write | write | none | `manage_level.unload_level` |
| `manage_level.validate_level` | `manage_level` | `execute_editor_function` | read | read | none | `manage_level.validate_level` |
| `manage_level_structure.configure_level_streaming` | `manage_level_structure` | `configure_level_streaming` | write | write | none | `manage_level_structure.configure_level_streaming` `manage_level_structure.set_streaming_distance` `manage_level_structure.configure_level_bounds` `manage_level_structure.enable_world_partition` `manage_level_structure.configure_grid_size` `manage_level_structure.configure_hlod_layer` `manage_level_structure.assign_actor_to_data_layer` |
| `manage_level_structure.create_level_structure` | `manage_level_structure` | `create_level` | write | write | none | `manage_level_structure.create_level_structure` `manage_level_structure.create_level` `manage_level_structure.create_sublevel` `manage_level_structure.create_level_instance` `manage_level_structure.create_packed_level_actor` `manage_level_structure.create_data_layer` `manage_level_structure.create_minimap_volume` |
| `manage_level_structure.create_volume` | `manage_level_structure` | `create_volume` | write | write | none | `manage_level_structure.create_volume` `manage_level_structure.create_trigger_volume` `manage_level_structure.add_trigger_volume` `manage_level_structure.create_trigger_box` `manage_level_structure.create_trigger_sphere` `manage_level_structure.create_trigger_capsule` `manage_level_structure.create_blocking_volume` `manage_level_structure.add_blocking_volume` `manage_level_structure.create_kill_z_volume` `manage_level_structure.add_kill_z_volume` `manage_level_structure.create_pain_causing_volume` `manage_level_structure.create_physics_volume` `manage_level_structure.add_physics_volume` `manage_level_structure.create_audio_volume` `manage_level_structure.create_reverb_volume` `manage_level_structure.create_cull_distance_volume` `manage_level_structure.add_cull_distance_volume` `manage_level_structure.create_precomputed_visibility_volume` `manage_level_structure.create_lightmass_importance_volume` `manage_level_structure.create_nav_mesh_bounds_volume` `manage_level_structure.create_nav_modifier_volume` `manage_level_structure.create_camera_blocking_volume` `manage_level_structure.create_post_process_volume` `manage_level_structure.add_post_process_volume` |
| `manage_level_structure.edit_level_blueprint` | `manage_level_structure` | `open_level_blueprint` | write | write | none | `manage_level_structure.edit_level_blueprint` `manage_level_structure.open_level_blueprint` `manage_level_structure.add_level_blueprint_node` `manage_level_structure.connect_level_blueprint_nodes` `manage_level_structure.remove_level_blueprint_node` |
| `manage_level_structure.get_level_structure_info` | `manage_level_structure` | `get_level_structure_info` | read | read | none | `manage_level_structure.get_level_structure_info` |
| `manage_level_structure.get_volumes_info` | `manage_level_structure` | `get_volumes_info` | read | read | none | `manage_level_structure.get_volumes_info` |
| `manage_level_structure.remove_volume` | `manage_level_structure` | `remove_volume` | destructive | destructive | explicit | `manage_level_structure.remove_volume` |
| `manage_level_structure.set_volume_properties` | `manage_level_structure` | `set_volume_properties` | write | write | none | `manage_level_structure.set_volume_properties` `manage_level_structure.set_volume_extent` `manage_level_structure.set_volume_bounds` |
| `manage_networking.add_legacy_mapping` | `manage_networking` | `manage_input` | write | write | none | `manage_networking.add_legacy_mapping` `manage_networking.add_legacy_action_mapping` `manage_networking.add_legacy_axis_mapping` |
| `manage_networking.add_local_player` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.add_local_player` |
| `manage_networking.check_authority` | `manage_networking` | `manage_networking` | read | read | none | `manage_networking.check_authority` `manage_networking.check_has_authority` `manage_networking.check_is_locally_controlled` |
| `manage_networking.configure_game_mode` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.configure_game_mode` `manage_networking.set_default_pawn_class` `manage_networking.set_player_controller_class` `manage_networking.set_game_state_class` `manage_networking.set_player_state_class` `manage_networking.set_hud_class` `manage_networking.configure_game_rules` `manage_networking.setup_match_states` `manage_networking.configure_round_system` `manage_networking.configure_scoring_system` `manage_networking.configure_team_system` `manage_networking.configure_spawn_system` `manage_networking.set_respawn_rules` `manage_networking.configure_spectating` `manage_networking.configure_player_start` |
| `manage_networking.configure_input` | `manage_networking` | `manage_input` | write | write | none | `manage_networking.configure_input` `manage_networking.create_input_action` `manage_networking.create_input_mapping_context` `manage_networking.add_mapping` `manage_networking.map_input_action` `manage_networking.set_input_trigger` `manage_networking.set_input_modifier` `manage_networking.enable_input_mapping` `manage_networking.disable_input_action` |
| `manage_networking.configure_prediction` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_prediction` `manage_networking.configure_client_prediction` `manage_networking.configure_movement_prediction` `manage_networking.configure_server_correction` `manage_networking.add_network_prediction_data` |
| `manage_networking.configure_replication` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_replication` `manage_networking.set_property_replicated` `manage_networking.set_replication_condition` `manage_networking.set_replicated_using` `manage_networking.set_net_role` `manage_networking.set_net_dormancy` `manage_networking.set_always_relevant` `manage_networking.set_only_relevant_to_owner` `manage_networking.set_autonomous_proxy` `manage_networking.configure_net_priority` `manage_networking.configure_net_update_frequency` `manage_networking.configure_net_cull_distance` `manage_networking.configure_push_model` `manage_networking.configure_replicated_movement` `manage_networking.configure_replication_graph` `manage_networking.configure_net_serialization` `manage_networking.configure_net_driver` |
| `manage_networking.configure_rpc` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_rpc` `manage_networking.create_rpc_function` `manage_networking.configure_rpc_validation` `manage_networking.set_rpc_reliability` |
| `manage_networking.configure_session` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.configure_session` `manage_networking.configure_lan_play` `manage_networking.configure_local_session_settings` `manage_networking.configure_session_interface` `manage_networking.configure_split_screen` `manage_networking.set_split_screen_type` |
| `manage_networking.configure_voice` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.configure_voice` `manage_networking.enable_voice_chat` `manage_networking.configure_voice_settings` `manage_networking.configure_push_to_talk` `manage_networking.set_voice_attenuation` `manage_networking.set_voice_channel` `manage_networking.mute_player` |
| `manage_networking.create_framework_class` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.create_framework_class` `manage_networking.create_game_mode` `manage_networking.create_game_state` `manage_networking.create_game_instance` `manage_networking.create_player_controller` `manage_networking.create_player_state` `manage_networking.create_hud_class` |
| `manage_networking.get_game_framework_info` | `manage_networking` | `manage_game_framework` | read | read | none | `manage_networking.get_game_framework_info` |
| `manage_networking.get_input_info` | `manage_networking` | `manage_input` | read | read | none | `manage_networking.get_input_info` |
| `manage_networking.get_networking_info` | `manage_networking` | `manage_networking` | read | read | none | `manage_networking.get_networking_info` |
| `manage_networking.get_sessions_info` | `manage_networking` | `manage_sessions` | read | read | none | `manage_networking.get_sessions_info` |
| `manage_networking.host_lan_server` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.host_lan_server` `manage_networking.join_lan_server` |
| `manage_networking.remove_legacy_mapping` | `manage_networking` | `manage_input` | destructive | destructive | explicit | `manage_networking.remove_legacy_mapping` `manage_networking.remove_legacy_action_mapping` `manage_networking.remove_legacy_axis_mapping` |
| `manage_networking.remove_local_player` | `manage_networking` | `manage_sessions` | destructive | destructive | explicit | `manage_networking.remove_local_player` |
| `manage_networking.remove_mapping` | `manage_networking` | `manage_input` | destructive | destructive | explicit | `manage_networking.remove_mapping` |
| `manage_networking.set_owner` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.set_owner` |
| `manage_pcg.add_pcg_node` | `manage_pcg` | `add_pcg_node` | write | write | none | `manage_pcg.add_pcg_node` `manage_pcg.add_surface_sampler` `manage_pcg.add_spline_sampler` `manage_pcg.add_mesh_sampler` `manage_pcg.add_volume_sampler` `manage_pcg.add_static_mesh_spawner` `manage_pcg.add_actor_spawner` `manage_pcg.add_spline_spawner` `manage_pcg.add_density_filter` `manage_pcg.add_distance_filter` `manage_pcg.add_height_filter` `manage_pcg.add_slope_filter` `manage_pcg.add_bounds_filter` `manage_pcg.add_bounds_modifier` `manage_pcg.add_landscape_data_node` `manage_pcg.add_spline_data_node` `manage_pcg.add_actor_data_node` `manage_pcg.add_texture_data_node` `manage_pcg.add_volume_data_node` `manage_pcg.add_transform_points` `manage_pcg.add_copy_points` `manage_pcg.add_merge_points` `manage_pcg.add_project_to_surface` `manage_pcg.add_self_pruning` |
| `manage_pcg.edit_pcg_graph` | `manage_pcg` | `create_pcg_graph` | write | write | none | `manage_pcg.edit_pcg_graph` `manage_pcg.create_pcg_graph` `manage_pcg.create_pcg_subgraph` `manage_pcg.connect_pcg_pins` `manage_pcg.set_pcg_node_settings` `manage_pcg.set_pcg_partition_grid_size` |
| `manage_pcg.execute_pcg_graph` | `manage_pcg` | `execute_pcg_graph` | write | write | none | `manage_pcg.execute_pcg_graph` |
| `manage_tools.disable_category` | `manage_tools` | `disable_category` | write | write | none | `manage_tools.disable_category` |
| `manage_tools.disable_tools` | `manage_tools` | `disable_tools` | write | write | none | `manage_tools.disable_tools` |
| `manage_tools.enable_category` | `manage_tools` | `enable_category` | write | write | none | `manage_tools.enable_category` |
| `manage_tools.enable_tools` | `manage_tools` | `enable_tools` | write | write | none | `manage_tools.enable_tools` |
| `manage_tools.get_status` | `manage_tools` | `get_status` | read | read | none | `manage_tools.get_status` |
| `manage_tools.list_categories` | `manage_tools` | `list_categories` | read | read | none | `manage_tools.list_categories` |
| `manage_tools.list_tools` | `manage_tools` | `list_tools` | read | read | none | `manage_tools.list_tools` |
| `manage_tools.reset` | `manage_tools` | `reset` | write | write | none | `manage_tools.reset` |
| `material.add_function_io` | `manage_asset` | `add_function_input` | write | write | explicit | `manage_asset.add_function_io` `manage_asset.add_function_input` `manage_asset.add_function_output` |
| `material.add_material_node` | `manage_asset` | `add_material_node` | write | write | explicit | `manage_asset.add_material_node` `manage_asset.add_custom_expression` `manage_asset.add_fresnel` `manage_asset.add_if` `manage_asset.add_math_node` `manage_asset.add_noise` `manage_asset.add_panner` `manage_asset.add_pixel_depth` `manage_asset.add_reflection_vector` `manage_asset.add_rotator` `manage_asset.add_scalar_parameter` `manage_asset.add_static_switch_parameter` `manage_asset.add_switch` `manage_asset.add_texture_coordinate` `manage_asset.add_texture_sample` `manage_asset.add_vector_parameter` `manage_asset.add_vertex_normal` `manage_asset.add_voronoi` `manage_asset.add_world_position` `manage_asset.use_material_function` `manage_asset.add_landscape_layer` `manage_asset.build_material_graph` |
| `material.compile_material` | `manage_asset` | `compile_material` | write | write | explicit | `manage_asset.compile_material` `manage_asset.rebuild_material` |
| `material.configure_layer_blend` | `manage_asset` | `configure_layer_blend` | write | write | explicit | `manage_asset.configure_layer_blend` |
| `material.connect_nodes` | `manage_asset` | `connect_nodes` | write | write | explicit | `manage_asset.connect_nodes` `manage_asset.connect_material_pins` |
| `material.create_material` | `manage_asset` | `create_material` | write | write | explicit | `manage_asset.create_material` `manage_asset.create_decal_material` `manage_asset.create_landscape_material` `manage_asset.create_post_process_material` `manage_asset.create_material_function` |
| `material.create_material_instance` | `manage_asset` | `create_material_instance` | write | write | explicit | `manage_asset.create_material_instance` |
| `material.delete_node` | `manage_asset` | `delete_node` | write | write | explicit | `manage_asset.delete_node` `manage_asset.remove_material_node` |
| `material.disconnect_nodes` | `manage_asset` | `disconnect_nodes` | write | write | explicit | `manage_asset.disconnect_nodes` `manage_asset.break_material_connections` |
| `material.get_material_info` | `manage_asset` | `get_material_info` | read | read | none | `manage_asset.get_material_info` `manage_asset.get_material_function_info` `manage_asset.find_node` `manage_asset.get_material_node_details` `manage_asset.get_node_properties` `manage_asset.get_node_connections` `manage_asset.get_node_chain` `manage_asset.get_connected_subgraph` |
| `material.set_material_parameter` | `manage_asset` | `set_material_parameter` | write | write | explicit | `manage_asset.set_material_parameter` `manage_asset.set_scalar_parameter_value` `manage_asset.set_vector_parameter_value` `manage_asset.set_texture_parameter_value` `manage_asset.set_static_switch_parameter_value` |
| `material.set_material_property` | `manage_asset` | `set_blend_mode` | write | write | explicit | `manage_asset.set_material_property` `manage_asset.set_blend_mode` `manage_asset.set_material_domain` `manage_asset.set_shading_model` `manage_asset.set_two_sided` |
| `material.set_node_position` | `manage_asset` | `set_node_position` | write | write | explicit | `manage_asset.set_node_position` |
| `material.update_custom_expression` | `manage_asset` | `update_custom_expression` | write | write | explicit | `manage_asset.update_custom_expression` |
| `sequence.cinematic.add_cinematic_track` | `manage_sequence` | `add_camera_cut_track` | write | write | none | `manage_sequence.add_cinematic_track` `manage_sequence.add_camera_cut_track` `manage_sequence.add_camera_shake_track` `manage_sequence.add_transform_track` `manage_sequence.add_property_track` `manage_sequence.add_skeletal_animation_track` `manage_sequence.add_material_parameter_track` `manage_sequence.add_particle_track` `manage_sequence.add_event_track` `manage_sequence.add_fade_track` `manage_sequence.add_level_visibility_track` `manage_sequence.add_shot_track` `manage_sequence.add_subsequence` |
| `sequence.cinematic.configure_cinematic` | `manage_sequence` | `configure_camera_settings` | write | write | none | `manage_sequence.configure_cinematic` `manage_sequence.configure_camera_settings` `manage_sequence.configure_camera_rig_crane` `manage_sequence.configure_camera_rig_rail` `manage_sequence.configure_shot_settings` |
| `sequence.cinematic.create_cinematic_asset` | `manage_sequence` | `create_cine_camera_actor` | write | write | none | `manage_sequence.create_cinematic_asset` `manage_sequence.create_cine_camera_actor` `manage_sequence.create_master_sequence` |
| `sequence.create` | `manage_sequence` | `create` | write | write | none | `manage_sequence.create` `manage_sequence.duplicate` `manage_sequence.rename` |
| `sequence.delete` | `manage_sequence` | `delete` | destructive | destructive | explicit | `manage_sequence.delete` `manage_sequence.remove_track` `manage_sequence.remove_keyframe` |
| `sequence.edit_sequence_bindings` | `manage_sequence` | `add_actor` | write | write | none | `manage_sequence.edit_sequence_bindings` `manage_sequence.add_actor` `manage_sequence.add_actors` `manage_sequence.add_camera` `manage_sequence.add_spawnable_from_class` `manage_sequence.remove_actors` |
| `sequence.edit_sequence_tracks` | `manage_sequence` | `add_track` | write | write | none | `manage_sequence.edit_sequence_tracks` `manage_sequence.add_track` `manage_sequence.add_section` `manage_sequence.add_keyframe` `manage_sequence.set_track_locked` `manage_sequence.set_track_muted` `manage_sequence.set_track_solo` |
| `sequence.get_metadata` | `manage_sequence` | `get_metadata` | read | read | none | `manage_sequence.get_metadata` |
| `sequence.get_properties` | `manage_sequence` | `get_properties` | read | read | none | `manage_sequence.get_properties` `manage_sequence.get_bindings` `manage_sequence.list_tracks` `manage_sequence.list_track_keys` `manage_sequence.list_track_types` `manage_sequence.list` `manage_sequence.open` |
| `sequence.media.create_media_asset` | `manage_sequence` | `create_media_source` | write | write | none | `manage_sequence.create_media_asset` `manage_sequence.create_media_source` `manage_sequence.create_media_player` `manage_sequence.create_media_playlist` `manage_sequence.create_media_texture` `manage_sequence.create_media_sound_component` |
| `sequence.media.play_media` | `manage_sequence` | `play_media` | write | write | none | `manage_sequence.play_media` `manage_sequence.pause_media` `manage_sequence.seek_media` |
| `sequence.mrq.configure_render_job` | `manage_sequence` | `configure_output_settings` | write | write | none | `manage_sequence.configure_render_job` `manage_sequence.configure_output_settings` `manage_sequence.configure_anti_aliasing` `manage_sequence.add_render_pass` `manage_sequence.configure_burn_ins` `manage_sequence.configure_console_variables` |
| `sequence.mrq.create_render_job` | `manage_sequence` | `create_render_job` | write | write | none | `manage_sequence.create_render_job` `manage_sequence.queue_render` `manage_sequence.start_render` |
| `sequence.play` | `manage_sequence` | `play` | write | write | none | `manage_sequence.play` `manage_sequence.pause` `manage_sequence.stop` |
| `sequence.replay.configure_demo_settings` | `manage_sequence` | `configure_demo_settings` | write | write | none | `manage_sequence.configure_demo_settings` `manage_sequence.configure_killcam_duration` |
| `sequence.replay.play_demo` | `manage_sequence` | `play_demo` | write | write | none | `manage_sequence.play_demo` `manage_sequence.pause_demo` `manage_sequence.seek_demo` `manage_sequence.set_demo_playback_speed` `manage_sequence.start_demo_recording` `manage_sequence.stop_demo_recording` `manage_sequence.start_killcam` |
| `sequence.set_metadata` | `manage_sequence` | `set_metadata` | write | write | none | `manage_sequence.set_metadata` |
| `sequence.set_properties` | `manage_sequence` | `set_properties` | write | write | none | `manage_sequence.set_properties` `manage_sequence.set_display_rate` `manage_sequence.set_tick_resolution` `manage_sequence.set_playback_speed` `manage_sequence.set_view_range` `manage_sequence.set_work_range` |
| `sequence.take.configure_take_recorder` | `manage_sequence` | `configure_take_sources` | write | write | none | `manage_sequence.configure_take_recorder` `manage_sequence.configure_take_sources` `manage_sequence.configure_recorded_tracks` `manage_sequence.create_take_recorder_panel` `manage_sequence.start_recording` `manage_sequence.stop_recording` |
| `struct.delete_struct` | `manage_asset` | `delete_struct` | destructive | destructive | elevated | `manage_asset.delete_struct` `manage_asset.remove_struct_member` |
| `struct.edit_struct` | `manage_asset` | `create_struct` | write | write | explicit | `manage_asset.edit_struct` `manage_asset.create_struct` `manage_asset.add_struct_member` `manage_asset.rename_struct_member` `manage_asset.reorder_struct_members` `manage_asset.set_struct_member_default` `manage_asset.set_struct_member_metadata` `manage_asset.set_struct_member_type` `manage_asset.rename_struct` `manage_asset.duplicate_struct` `manage_asset.import_struct` `manage_asset.recompile_struct` `manage_asset.refresh_struct_dependencies` `manage_asset.set_instanced_struct_property` |
| `struct.get_struct` | `manage_asset` | `get_struct` | read | read | none | `manage_asset.get_struct` `manage_asset.read_struct` `manage_asset.list_struct_members` `manage_asset.list_structs` `manage_asset.search_struct_usage` `manage_asset.compare_structs` `manage_asset.export_struct` `manage_asset.get_instanced_struct_property` |
| `system_control.configure_display` | `system_control` | `console_command` | write | write | none | `system_control.configure_display` `system_control.set_resolution` `system_control.set_fullscreen` `system_control.set_quality` `system_control.set_cvar` `system_control.show_fps` `system_control.profile` |
| `system_control.configure_performance` | `system_control` | `set_scalability` | write | write | none | `system_control.configure_performance` `system_control.set_scalability` `system_control.set_frame_rate_limit` `system_control.set_resolution_scale` `system_control.set_vsync` `system_control.configure_lod` `system_control.configure_nanite` `system_control.configure_occlusion_culling` `system_control.configure_texture_streaming` `system_control.configure_world_partition` `system_control.enable_gpu_timing` `system_control.optimize_draw_calls` `system_control.optimize_shaders` `system_control.apply_baseline_settings` |
| `system_control.console_command` | `system_control` | `console_command` | write | write | none | `system_control.console_command` `system_control.execute_command` |
| `system_control.create_widget` | `system_control` | `manage_widget_authoring` | write | write | none | `system_control.create_widget` `system_control.add_widget_child` `system_control.show_widget` |
| `system_control.enable_plugin` | `system_control` | `system_control` | write | write | none | `system_control.enable_plugin` `system_control.disable_plugin` |
| `system_control.execute_python` | `system_control` | `system_control` | write | write | explicit | `system_control.execute_python` |
| `system_control.get_project_settings` | `system_control` | `system_control` | read | read | none | `system_control.get_project_settings` |
| `system_control.get_trace_status` | `system_control` | `manage_insights` | read | read | none | `system_control.get_trace_status` `system_control.analyze_trace` |
| `system_control.launch_build` | `system_control` | `system_control` | write | write | none | `system_control.launch_build` |
| `system_control.list_plugins` | `system_control` | `system_control` | read | read | none | `system_control.list_plugins` |
| `system_control.lumen_update_scene` | `system_control` | `manage_render` | write | write | none | `system_control.lumen_update_scene` |
| `system_control.merge_actors` | `system_control` | `merge_actors` | write | write | none | `system_control.merge_actors` |
| `system_control.package_project` | `system_control` | `system_control` | write | write | none | `system_control.package_project` |
| `system_control.package_status` | `system_control` | `system_control` | read | read | none | `system_control.package_status` |
| `system_control.play_sound` | `system_control` | `play_sound_2d` | write | write | none | `system_control.play_sound` |
| `system_control.profile_performance` | `system_control` | `start_profiling` | write | write | none | `system_control.profile_performance` `system_control.start_profiling` `system_control.stop_profiling` `system_control.run_benchmark` `system_control.generate_memory_report` `system_control.show_stats` |
| `system_control.read_log` | `system_control` | `manage_logs` | read | read | none | `system_control.read_log` |
| `system_control.run_build` | `system_control` | `manage_tests` | write | write | none | `system_control.run_build` `system_control.run_tests` `system_control.run_ubt` |
| `system_control.screenshot` | `system_control` | `control_editor` | read | read | none | `system_control.screenshot` |
| `system_control.set_project_setting` | `system_control` | `system_control` | write | write | none | `system_control.set_project_setting` |
| `system_control.start_session` | `system_control` | `manage_insights` | write | write | none | `system_control.start_session` `system_control.stop_session` `system_control.pause_session` `system_control.resume_session` `system_control.capture_insights_trace` `system_control.send_snapshot` `system_control.write_snapshot` `system_control.start_unreal_insights` |
| `system_control.subscribe` | `system_control` | `manage_logs` | write | write | none | `system_control.subscribe` `system_control.unsubscribe` `system_control.spawn_category` |
| `system_control.validate_assets` | `system_control` | `system_control` | read | read | none | `system_control.validate_assets` |
| `texture.adjust_texture` | `manage_asset` | `adjust_curves` | write | write | explicit | `manage_asset.adjust_texture` `manage_asset.adjust_curves` `manage_asset.adjust_levels` `manage_asset.blur` `manage_asset.sharpen` `manage_asset.desaturate` `manage_asset.invert` |
| `texture.configure_texture` | `manage_asset` | `set_compression_settings` | write | write | explicit | `manage_asset.configure_texture` `manage_asset.set_compression_settings` `manage_asset.set_lod_bias` `manage_asset.set_streaming_priority` `manage_asset.set_texture_group` `manage_asset.configure_virtual_texture` |
| `texture.create_texture` | `manage_asset` | `create_gradient_texture` | write | write | explicit | `manage_asset.create_texture` `manage_asset.create_gradient_texture` `manage_asset.create_noise_texture` `manage_asset.create_pattern_texture` `manage_asset.create_ao_from_mesh` `manage_asset.create_normal_from_height` `manage_asset.resize_texture` `manage_asset.channel_pack` `manage_asset.channel_extract` `manage_asset.combine_textures` |
| `texture.get_texture_info` | `manage_asset` | `get_texture_info` | read | read | none | `manage_asset.get_texture_info` |
