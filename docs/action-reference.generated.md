<!-- GENERATED FILE - DO NOT EDIT.
     Regenerate with `npm run registry:generate`; `npm run registry:check` gates drift.
     Source of truth: src/tools/catalog/capabilities/records/** -->

# Action reference

Catalog revision: `036a1612ed43ddf0`

Both transports expose exactly ONE public MCP tool, `unreal`, with the four
operations `search` / `describe` / `execute` / `configure`. The parent tools
named in these tables are an INTERNAL routing boundary: they are never listed
by `tools/list` and a direct `tools/call` on one returns a
`DIRECT_TOOL_CALL_REMOVED` receipt rather than executing
(`src/server/gateway/direct-call-migration.ts`).

The catalog declares 1335 capabilities across
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
| `animation_physics` | 87 | 6 | 79 | 2 | animation physics |
| `build_environment` | 150 | 5 | 141 | 4 | environment |
| `control_actor` | 46 | 14 | 29 | 3 | actor |
| `control_editor` | 42 | 19 | 23 | 0 | editor |
| `inspect` | 36 | 30 | 5 | 1 | inspect |
| `manage_ai` | 62 | 4 | 58 | 0 | manage ai |
| `manage_asset` | 158 | 34 | 114 | 10 | asset, datatable, enum, material, struct, texture |
| `manage_audio` | 50 | 0 | 50 | 0 | audio |
| `manage_blueprint` | 104 | 9 | 89 | 6 | blueprint, widget |
| `manage_character` | 27 | 1 | 26 | 0 | manage character |
| `manage_combat` | 39 | 2 | 37 | 0 | manage combat |
| `manage_effect` | 59 | 3 | 55 | 1 | manage effect |
| `manage_gas` | 27 | 1 | 26 | 0 | manage gas |
| `manage_geometry` | 76 | 1 | 75 | 0 | world |
| `manage_interaction` | 22 | 1 | 21 | 0 | manage interaction |
| `manage_inventory` | 33 | 1 | 32 | 0 | manage inventory |
| `manage_level` | 24 | 5 | 17 | 2 | level |
| `manage_level_structure` | 45 | 2 | 42 | 1 | world |
| `manage_networking` | 77 | 6 | 67 | 4 | networking |
| `manage_pcg` | 30 | 0 | 30 | 0 | world |
| `manage_sequence` | 81 | 7 | 72 | 2 | cinematics, media, movie_render, replay, sequence, take_recorder |
| `manage_tools` | 8 | 3 | 5 | 0 | tools |
| `system_control` | 52 | 6 | 46 | 0 | audio, build, console, insights, logs, performance, project, python, render, viewport, widget |

## Capabilities requiring consent

150 of 1335 capabilities require consent.

| Capability | Tool | Action | Effect | Consent |
| --- | --- | --- | --- | --- |
| `animation_physics.cleanup` | `animation_physics` | `cleanup` | destructive | explicit |
| `animation_physics.remove_bone` | `animation_physics` | `remove_bone` | destructive | explicit |
| `asset.add_material_parameter` | `manage_asset` | `add_material_parameter` | write | explicit |
| `asset.bulk_delete` | `manage_asset` | `bulk_delete` | destructive | elevated |
| `asset.bulk_rename` | `manage_asset` | `bulk_rename` | write | explicit |
| `asset.create_folder` | `manage_asset` | `create_folder` | write | explicit |
| `asset.create_render_target` | `manage_asset` | `manage_texture` | write | explicit |
| `asset.create_thumbnail` | `manage_asset` | `generate_thumbnail` | write | explicit |
| `asset.delete` | `manage_asset` | `delete` | destructive | elevated |
| `asset.delete_asset` | `manage_asset` | `delete_asset` | destructive | elevated |
| `asset.delete_assets` | `manage_asset` | `delete_assets` | destructive | elevated |
| `asset.duplicate` | `manage_asset` | `duplicate` | write | explicit |
| `asset.duplicate_asset` | `manage_asset` | `duplicate_asset` | write | explicit |
| `asset.fixup_redirectors` | `manage_asset` | `fixup_redirectors` | write | explicit |
| `asset.generate_lods` | `manage_asset` | `generate_lods` | write | explicit |
| `asset.import` | `manage_asset` | `import` | write | explicit |
| `asset.move` | `manage_asset` | `move` | write | explicit |
| `asset.move_asset` | `manage_asset` | `move_asset` | write | explicit |
| `asset.nanite_rebuild_mesh` | `manage_asset` | `manage_render` | write | explicit |
| `asset.rename` | `manage_asset` | `rename` | write | explicit |
| `asset.rename_asset` | `manage_asset` | `rename_asset` | write | explicit |
| `asset.reset_instance_parameters` | `manage_asset` | `reset_instance_parameters` | write | explicit |
| `asset.set_metadata` | `manage_asset` | `set_metadata` | write | explicit |
| `asset.set_tags` | `manage_asset` | `set_tags` | write | explicit |
| `asset.source_control_checkout` | `manage_asset` | `source_control_checkout` | write | explicit |
| `asset.source_control_submit` | `manage_asset` | `source_control_submit` | write | explicit |
| `blueprint.break_pin_links` | `manage_blueprint` | `break_pin_links` | destructive | explicit |
| `blueprint.delete_node` | `manage_blueprint` | `delete_node` | destructive | explicit |
| `blueprint.remove_event` | `manage_blueprint` | `remove_event` | destructive | explicit |
| `blueprint.remove_function` | `manage_blueprint` | `remove_function` | destructive | explicit |
| `blueprint.remove_scs_component` | `manage_blueprint` | `remove_scs_component` | destructive | explicit |
| `blueprint.remove_variable` | `manage_blueprint` | `remove_variable` | destructive | explicit |
| `build_environment.delete` | `build_environment` | `delete` | destructive | explicit |
| `build_environment.remove_foliage` | `build_environment` | `remove_foliage` | destructive | explicit |
| `build_environment.remove_foliage_instances` | `build_environment` | `remove_foliage_instances` | destructive | explicit |
| `build_environment.remove_spline_point` | `build_environment` | `remove_spline_point` | destructive | explicit |
| `control_actor.delete` | `control_actor` | `delete` | destructive | explicit |
| `control_actor.delete_by_tag` | `control_actor` | `delete_by_tag` | destructive | explicit |
| `control_actor.destroy_actor` | `control_actor` | `destroy_actor` | destructive | explicit |
| `datatable.add_data_table_row` | `manage_asset` | `add_data_table_row` | write | explicit |
| `datatable.clear_data_table_rows` | `manage_asset` | `clear_data_table_rows` | destructive | elevated |
| `datatable.create_data_table` | `manage_asset` | `create_data_table` | write | explicit |
| `datatable.create_row_struct` | `manage_asset` | `create_row_struct` | write | explicit |
| `datatable.delete_data_table_row` | `manage_asset` | `delete_data_table_row` | destructive | elevated |
| `datatable.import_data_table_rows` | `manage_asset` | `import_data_table_rows` | write | explicit |
| `datatable.set_data_table_row_struct` | `manage_asset` | `set_data_table_row_struct` | write | explicit |
| `datatable.set_struct_as_row_struct` | `manage_asset` | `set_struct_as_row_struct` | write | explicit |
| `datatable.update_data_table_row` | `manage_asset` | `update_data_table_row` | write | explicit |
| `enum.add_enum_value` | `manage_asset` | `add_enum_value` | write | explicit |
| `enum.create_enum` | `manage_asset` | `create_enum` | write | explicit |
| `enum.delete_enum` | `manage_asset` | `delete_enum` | destructive | elevated |
| `enum.remove_enum_value` | `manage_asset` | `remove_enum_value` | destructive | elevated |
| `enum.rename_enum_value` | `manage_asset` | `rename_enum_value` | write | explicit |
| `enum.reorder_enum_values` | `manage_asset` | `reorder_enum_values` | write | explicit |
| `enum.set_enum_value_metadata` | `manage_asset` | `set_enum_value_metadata` | write | explicit |
| `enum.split_enum` | `manage_asset` | `split_enum` | write | explicit |
| `inspect.delete_object` | `inspect` | `control_actor` | destructive | explicit |
| `manage_effect.remove_niagara_node` | `manage_effect` | `remove_niagara_node` | destructive | explicit |
| `manage_level.delete` | `manage_level` | `delete_level` | destructive | explicit |
| `manage_level.delete_level` | `manage_level` | `delete_level` | destructive | explicit |
| `manage_level_structure.remove_volume` | `manage_level_structure` | `remove_volume` | destructive | explicit |
| `manage_networking.remove_legacy_action_mapping` | `manage_networking` | `manage_input` | destructive | explicit |
| `manage_networking.remove_legacy_axis_mapping` | `manage_networking` | `manage_input` | destructive | explicit |
| `manage_networking.remove_local_player` | `manage_networking` | `manage_sessions` | destructive | explicit |
| `manage_networking.remove_mapping` | `manage_networking` | `manage_input` | destructive | explicit |
| `material.add_custom_expression` | `manage_asset` | `add_custom_expression` | write | explicit |
| `material.add_fresnel` | `manage_asset` | `add_fresnel` | write | explicit |
| `material.add_function_input` | `manage_asset` | `add_function_input` | write | explicit |
| `material.add_function_output` | `manage_asset` | `add_function_output` | write | explicit |
| `material.add_if` | `manage_asset` | `add_if` | write | explicit |
| `material.add_landscape_layer` | `manage_asset` | `add_landscape_layer` | write | explicit |
| `material.add_material_node` | `manage_asset` | `add_material_node` | write | explicit |
| `material.add_math_node` | `manage_asset` | `add_math_node` | write | explicit |
| `material.add_noise` | `manage_asset` | `add_noise` | write | explicit |
| `material.add_panner` | `manage_asset` | `add_panner` | write | explicit |
| `material.add_pixel_depth` | `manage_asset` | `add_pixel_depth` | write | explicit |
| `material.add_reflection_vector` | `manage_asset` | `add_reflection_vector` | write | explicit |
| `material.add_rotator` | `manage_asset` | `add_rotator` | write | explicit |
| `material.add_scalar_parameter` | `manage_asset` | `add_scalar_parameter` | write | explicit |
| `material.add_static_switch_parameter` | `manage_asset` | `add_static_switch_parameter` | write | explicit |
| `material.add_switch` | `manage_asset` | `add_switch` | write | explicit |
| `material.add_texture_coordinate` | `manage_asset` | `add_texture_coordinate` | write | explicit |
| `material.add_texture_sample` | `manage_asset` | `add_texture_sample` | write | explicit |
| `material.add_vector_parameter` | `manage_asset` | `add_vector_parameter` | write | explicit |
| `material.add_vertex_normal` | `manage_asset` | `add_vertex_normal` | write | explicit |
| `material.add_voronoi` | `manage_asset` | `add_voronoi` | write | explicit |
| `material.add_world_position` | `manage_asset` | `add_world_position` | write | explicit |
| `material.break_material_connections` | `manage_asset` | `break_material_connections` | write | explicit |
| `material.compile_material` | `manage_asset` | `compile_material` | write | explicit |
| `material.configure_layer_blend` | `manage_asset` | `configure_layer_blend` | write | explicit |
| `material.connect_material_pins` | `manage_asset` | `connect_material_pins` | write | explicit |
| `material.connect_nodes` | `manage_asset` | `connect_nodes` | write | explicit |
| `material.create_decal_material` | `manage_asset` | `create_decal_material` | write | explicit |
| `material.create_landscape_material` | `manage_asset` | `create_landscape_material` | write | explicit |
| `material.create_material` | `manage_asset` | `create_material` | write | explicit |
| `material.create_material_function` | `manage_asset` | `create_material_function` | write | explicit |
| `material.create_material_instance` | `manage_asset` | `create_material_instance` | write | explicit |
| `material.create_post_process_material` | `manage_asset` | `create_post_process_material` | write | explicit |
| `material.delete_node` | `manage_asset` | `delete_node` | write | explicit |
| `material.disconnect_nodes` | `manage_asset` | `disconnect_nodes` | write | explicit |
| `material.rebuild_material` | `manage_asset` | `rebuild_material` | write | explicit |
| `material.remove_material_node` | `manage_asset` | `remove_material_node` | write | explicit |
| `material.set_blend_mode` | `manage_asset` | `set_blend_mode` | write | explicit |
| `material.set_material_domain` | `manage_asset` | `set_material_domain` | write | explicit |
| `material.set_material_parameter` | `manage_asset` | `set_material_parameter` | write | explicit |
| `material.set_scalar_parameter_value` | `manage_asset` | `set_scalar_parameter_value` | write | explicit |
| `material.set_shading_model` | `manage_asset` | `set_shading_model` | write | explicit |
| `material.set_static_switch_parameter_value` | `manage_asset` | `set_static_switch_parameter_value` | write | explicit |
| `material.set_texture_parameter_value` | `manage_asset` | `set_texture_parameter_value` | write | explicit |
| `material.set_two_sided` | `manage_asset` | `set_two_sided` | write | explicit |
| `material.set_vector_parameter_value` | `manage_asset` | `set_vector_parameter_value` | write | explicit |
| `material.update_custom_expression` | `manage_asset` | `update_custom_expression` | write | explicit |
| `material.use_material_function` | `manage_asset` | `use_material_function` | write | explicit |
| `sequence.delete` | `manage_sequence` | `delete` | destructive | explicit |
| `sequence.remove_track` | `manage_sequence` | `remove_track` | destructive | explicit |
| `struct.add_struct_member` | `manage_asset` | `add_struct_member` | write | explicit |
| `struct.create_struct` | `manage_asset` | `create_struct` | write | explicit |
| `struct.delete_struct` | `manage_asset` | `delete_struct` | destructive | elevated |
| `struct.duplicate_struct` | `manage_asset` | `duplicate_struct` | write | explicit |
| `struct.import_struct` | `manage_asset` | `import_struct` | write | explicit |
| `struct.recompile_struct` | `manage_asset` | `recompile_struct` | write | explicit |
| `struct.refresh_struct_dependencies` | `manage_asset` | `refresh_struct_dependencies` | write | explicit |
| `struct.remove_struct_member` | `manage_asset` | `remove_struct_member` | destructive | elevated |
| `struct.rename_struct` | `manage_asset` | `rename_struct` | write | explicit |
| `struct.rename_struct_member` | `manage_asset` | `rename_struct_member` | write | explicit |
| `struct.reorder_struct_members` | `manage_asset` | `reorder_struct_members` | write | explicit |
| `struct.set_instanced_struct_property` | `manage_asset` | `set_instanced_struct_property` | write | explicit |
| `struct.set_struct_member_default` | `manage_asset` | `set_struct_member_default` | write | explicit |
| `struct.set_struct_member_metadata` | `manage_asset` | `set_struct_member_metadata` | write | explicit |
| `struct.set_struct_member_type` | `manage_asset` | `set_struct_member_type` | write | explicit |
| `texture.adjust_curves` | `manage_asset` | `adjust_curves` | write | explicit |
| `texture.adjust_levels` | `manage_asset` | `adjust_levels` | write | explicit |
| `texture.blur` | `manage_asset` | `blur` | write | explicit |
| `texture.channel_extract` | `manage_asset` | `channel_extract` | write | explicit |
| `texture.channel_pack` | `manage_asset` | `channel_pack` | write | explicit |
| `texture.combine_textures` | `manage_asset` | `combine_textures` | write | explicit |
| `texture.configure_virtual_texture` | `manage_asset` | `configure_virtual_texture` | write | explicit |
| `texture.create_ao_from_mesh` | `manage_asset` | `create_ao_from_mesh` | write | explicit |
| `texture.create_gradient_texture` | `manage_asset` | `create_gradient_texture` | write | explicit |
| `texture.create_noise_texture` | `manage_asset` | `create_noise_texture` | write | explicit |
| `texture.create_normal_from_height` | `manage_asset` | `create_normal_from_height` | write | explicit |
| `texture.create_pattern_texture` | `manage_asset` | `create_pattern_texture` | write | explicit |
| `texture.desaturate` | `manage_asset` | `desaturate` | write | explicit |
| `texture.invert` | `manage_asset` | `invert` | write | explicit |
| `texture.resize_texture` | `manage_asset` | `resize_texture` | write | explicit |
| `texture.set_compression_settings` | `manage_asset` | `set_compression_settings` | write | explicit |
| `texture.set_lod_bias` | `manage_asset` | `set_lod_bias` | write | explicit |
| `texture.set_streaming_priority` | `manage_asset` | `set_streaming_priority` | write | explicit |
| `texture.set_texture_group` | `manage_asset` | `set_texture_group` | write | explicit |
| `texture.sharpen` | `manage_asset` | `sharpen` | write | explicit |

## Deprecated and removed capabilities

1 capability record is no longer `active`.

| Capability | Status | Since | Guidance | Replacement |
| --- | --- | --- | --- | --- |
| `animation_physics.set_retarget_chain_mapping` | deprecated | 5.0 | Documented no-op: the route performs no mutation. Prefer setup_retargeting for real retarget configuration. | — |

## Full action reference

| Capability | Tool | Action | Effect | Scope | Consent | Legacy pairs |
| --- | --- | --- | --- | --- | --- | --- |
| `animation_physics.activate_ragdoll` | `animation_physics` | `activate_ragdoll` | write | write | none | `animation_physics.activate_ragdoll` |
| `animation_physics.add_aim_offset_sample` | `animation_physics` | `add_aim_offset_sample` | write | write | none | `animation_physics.add_aim_offset_sample` |
| `animation_physics.add_blend_node` | `animation_physics` | `add_blend_node` | write | write | none | `animation_physics.add_blend_node` |
| `animation_physics.add_blend_sample` | `animation_physics` | `add_blend_sample` | write | write | none | `animation_physics.add_blend_sample` |
| `animation_physics.add_bone` | `animation_physics` | `add_bone` | write | write | none | `animation_physics.add_bone` |
| `animation_physics.add_bone_track` | `animation_physics` | `add_bone_track` | write | write | none | `animation_physics.add_bone_track` |
| `animation_physics.add_cached_pose` | `animation_physics` | `add_cached_pose` | write | write | none | `animation_physics.add_cached_pose` |
| `animation_physics.add_layered_blend_per_bone` | `animation_physics` | `add_layered_blend_per_bone` | write | write | none | `animation_physics.add_layered_blend_per_bone` |
| `animation_physics.add_montage_notify` | `animation_physics` | `add_montage_notify` | write | write | none | `animation_physics.add_montage_notify` |
| `animation_physics.add_montage_section` | `animation_physics` | `add_montage_section` | write | write | none | `animation_physics.add_montage_section` |
| `animation_physics.add_montage_slot` | `animation_physics` | `add_montage_slot` | write | write | none | `animation_physics.add_montage_slot` |
| `animation_physics.add_notify` | `animation_physics` | `add_notify` | write | write | none | `animation_physics.add_notify` |
| `animation_physics.add_notify_state` | `animation_physics` | `add_notify_state` | write | write | none | `animation_physics.add_notify_state` |
| `animation_physics.add_physics_body` | `animation_physics` | `add_physics_body` | write | write | none | `animation_physics.add_physics_body` |
| `animation_physics.add_physics_constraint` | `animation_physics` | `add_physics_constraint` | write | write | none | `animation_physics.add_physics_constraint` |
| `animation_physics.add_slot_node` | `animation_physics` | `add_slot_node` | write | write | none | `animation_physics.add_slot_node` |
| `animation_physics.add_state` | `animation_physics` | `add_state` | write | write | none | `animation_physics.add_state` |
| `animation_physics.add_state_machine` | `animation_physics` | `add_state_machine` | write | write | none | `animation_physics.add_state_machine` |
| `animation_physics.add_sync_marker` | `animation_physics` | `add_sync_marker` | write | write | none | `animation_physics.add_sync_marker` |
| `animation_physics.add_transition` | `animation_physics` | `add_transition` | write | write | none | `animation_physics.add_transition` |
| `animation_physics.assign_cloth_asset_to_mesh` | `animation_physics` | `assign_cloth_asset_to_mesh` | write | write | none | `animation_physics.assign_cloth_asset_to_mesh` |
| `animation_physics.auto_skin_weights` | `animation_physics` | `auto_skin_weights` | write | write | none | `animation_physics.auto_skin_weights` |
| `animation_physics.bind_cloth_to_skeletal_mesh` | `animation_physics` | `bind_cloth_to_skeletal_mesh` | write | write | none | `animation_physics.bind_cloth_to_skeletal_mesh` |
| `animation_physics.cleanup` | `animation_physics` | `cleanup` | destructive | destructive | explicit | `animation_physics.cleanup` |
| `animation_physics.configure_constraint_limits` | `animation_physics` | `configure_constraint_limits` | write | write | none | `animation_physics.configure_constraint_limits` |
| `animation_physics.configure_physics_body` | `animation_physics` | `configure_physics_body` | write | write | none | `animation_physics.configure_physics_body` |
| `animation_physics.configure_socket` | `animation_physics` | `configure_socket` | write | write | none | `animation_physics.configure_socket` |
| `animation_physics.configure_vehicle` | `animation_physics` | `configure_vehicle` | write | write | none | `animation_physics.configure_vehicle` |
| `animation_physics.copy_weights` | `animation_physics` | `copy_weights` | write | write | none | `animation_physics.copy_weights` |
| `animation_physics.create_aim_offset` | `animation_physics` | `create_aim_offset` | write | write | none | `animation_physics.create_aim_offset` |
| `animation_physics.create_anim_blueprint` | `animation_physics` | `create_anim_blueprint` | write | write | none | `animation_physics.create_anim_blueprint` |
| `animation_physics.create_animation_asset` | `animation_physics` | `create_animation_asset` | write | write | none | `animation_physics.create_animation_asset` |
| `animation_physics.create_animation_blueprint` | `animation_physics` | `create_animation_blueprint` | write | write | none | `animation_physics.create_animation_blueprint` |
| `animation_physics.create_animation_bp` | `animation_physics` | `create_animation_bp` | write | write | none | `animation_physics.create_animation_bp` |
| `animation_physics.create_animation_sequence` | `animation_physics` | `create_animation_sequence` | write | write | none | `animation_physics.create_animation_sequence` |
| `animation_physics.create_blend_space` | `animation_physics` | `create_blend_space` | write | write | none | `animation_physics.create_blend_space` |
| `animation_physics.create_blend_space_1d` | `animation_physics` | `create_blend_space_1d` | write | write | none | `animation_physics.create_blend_space_1d` |
| `animation_physics.create_blend_space_2d` | `animation_physics` | `create_blend_space_2d` | write | write | none | `animation_physics.create_blend_space_2d` |
| `animation_physics.create_blend_tree` | `animation_physics` | `create_blend_tree` | write | write | none | `animation_physics.create_blend_tree` |
| `animation_physics.create_control_rig` | `animation_physics` | `create_control_rig` | write | write | none | `animation_physics.create_control_rig` |
| `animation_physics.create_ik_retargeter` | `animation_physics` | `create_ik_retargeter` | write | write | none | `animation_physics.create_ik_retargeter` |
| `animation_physics.create_ik_rig` | `animation_physics` | `create_ik_rig` | write | write | none | `animation_physics.create_ik_rig` |
| `animation_physics.create_montage` | `animation_physics` | `create_montage` | write | write | none | `animation_physics.create_montage` |
| `animation_physics.create_morph_target` | `animation_physics` | `create_morph_target` | write | write | none | `animation_physics.create_morph_target` |
| `animation_physics.create_physics_asset` | `animation_physics` | `create_physics_asset` | write | write | none | `animation_physics.create_physics_asset` |
| `animation_physics.create_pose_library` | `animation_physics` | `create_pose_library` | write | write | none | `animation_physics.create_pose_library` |
| `animation_physics.create_procedural_anim` | `animation_physics` | `create_procedural_anim` | write | write | none | `animation_physics.create_procedural_anim` |
| `animation_physics.create_skeleton` | `animation_physics` | `create_skeleton` | write | write | none | `animation_physics.create_skeleton` |
| `animation_physics.create_socket` | `animation_physics` | `create_socket` | write | write | none | `animation_physics.create_socket` |
| `animation_physics.create_state_machine` | `animation_physics` | `create_state_machine` | write | write | none | `animation_physics.create_state_machine` |
| `animation_physics.create_virtual_bone` | `animation_physics` | `create_virtual_bone` | write | write | none | `animation_physics.create_virtual_bone` |
| `animation_physics.force_rebuild_blend_space` | `animation_physics` | `force_rebuild_blend_space` | write | write | none | `animation_physics.force_rebuild_blend_space` |
| `animation_physics.get_animation_info` | `animation_physics` | `get_animation_info` | read | read | none | `animation_physics.get_animation_info` |
| `animation_physics.get_skeleton_info` | `animation_physics` | `get_skeleton_info` | read | read | none | `animation_physics.get_skeleton_info` |
| `animation_physics.import_morph_targets` | `animation_physics` | `import_morph_targets` | write | write | none | `animation_physics.import_morph_targets` |
| `animation_physics.link_sections` | `animation_physics` | `link_sections` | write | write | none | `animation_physics.link_sections` |
| `animation_physics.list_bones` | `animation_physics` | `list_bones` | read | read | none | `animation_physics.list_bones` |
| `animation_physics.list_physics_bodies` | `animation_physics` | `list_physics_bodies` | read | read | none | `animation_physics.list_physics_bodies` |
| `animation_physics.list_sockets` | `animation_physics` | `list_sockets` | read | read | none | `animation_physics.list_sockets` |
| `animation_physics.mirror_weights` | `animation_physics` | `mirror_weights` | write | write | none | `animation_physics.mirror_weights` |
| `animation_physics.normalize_weights` | `animation_physics` | `normalize_weights` | write | write | none | `animation_physics.normalize_weights` |
| `animation_physics.play_anim_montage` | `animation_physics` | `play_anim_montage` | write | write | none | `animation_physics.play_anim_montage` |
| `animation_physics.play_montage` | `animation_physics` | `play_montage` | write | write | none | `animation_physics.play_montage` |
| `animation_physics.prune_weights` | `animation_physics` | `prune_weights` | write | write | none | `animation_physics.prune_weights` |
| `animation_physics.remove_bone` | `animation_physics` | `remove_bone` | destructive | destructive | explicit | `animation_physics.remove_bone` |
| `animation_physics.rename_bone` | `animation_physics` | `rename_bone` | write | write | none | `animation_physics.rename_bone` |
| `animation_physics.set_additive_settings` | `animation_physics` | `set_additive_settings` | write | write | none | `animation_physics.set_additive_settings` |
| `animation_physics.set_anim_graph_node_value` | `animation_physics` | `set_anim_graph_node_value` | write | write | none | `animation_physics.set_anim_graph_node_value` |
| `animation_physics.set_axis_settings` | `animation_physics` | `set_axis_settings` | write | write | none | `animation_physics.set_axis_settings` |
| `animation_physics.set_blend_in` | `animation_physics` | `set_blend_in` | write | write | none | `animation_physics.set_blend_in` |
| `animation_physics.set_blend_out` | `animation_physics` | `set_blend_out` | write | write | none | `animation_physics.set_blend_out` |
| `animation_physics.set_bone_key` | `animation_physics` | `set_bone_key` | write | write | none | `animation_physics.set_bone_key` |
| `animation_physics.set_bone_parent` | `animation_physics` | `set_bone_parent` | write | write | none | `animation_physics.set_bone_parent` |
| `animation_physics.set_bone_transform` | `animation_physics` | `set_bone_transform` | write | write | none | `animation_physics.set_bone_transform` |
| `animation_physics.set_curve_key` | `animation_physics` | `set_curve_key` | write | write | none | `animation_physics.set_curve_key` |
| `animation_physics.set_interpolation_settings` | `animation_physics` | `set_interpolation_settings` | write | write | none | `animation_physics.set_interpolation_settings` |
| `animation_physics.set_morph_target_deltas` | `animation_physics` | `set_morph_target_deltas` | write | write | none | `animation_physics.set_morph_target_deltas` |
| `animation_physics.set_retarget_chain_mapping` | `animation_physics` | `set_retarget_chain_mapping` | read | read | none | `animation_physics.set_retarget_chain_mapping` |
| `animation_physics.set_root_motion_settings` | `animation_physics` | `set_root_motion_settings` | write | write | none | `animation_physics.set_root_motion_settings` |
| `animation_physics.set_section_timing` | `animation_physics` | `set_section_timing` | write | write | none | `animation_physics.set_section_timing` |
| `animation_physics.set_sequence_length` | `animation_physics` | `set_sequence_length` | write | write | none | `animation_physics.set_sequence_length` |
| `animation_physics.set_transition_rules` | `animation_physics` | `set_transition_rules` | write | write | none | `animation_physics.set_transition_rules` |
| `animation_physics.set_vertex_weights` | `animation_physics` | `set_vertex_weights` | write | write | none | `animation_physics.set_vertex_weights` |
| `animation_physics.setup_ik` | `animation_physics` | `setup_ik` | write | write | none | `animation_physics.setup_ik` |
| `animation_physics.setup_physics_simulation` | `animation_physics` | `setup_physics_simulation` | write | write | none | `animation_physics.setup_physics_simulation` |
| `animation_physics.setup_ragdoll` | `animation_physics` | `setup_ragdoll` | write | write | none | `animation_physics.setup_ragdoll` |
| `animation_physics.setup_retargeting` | `animation_physics` | `setup_retargeting` | write | write | none | `animation_physics.setup_retargeting` |
| `asset.add_material_parameter` | `manage_asset` | `add_material_parameter` | write | write | explicit | `manage_asset.add_material_parameter` |
| `asset.analyze_graph` | `manage_asset` | `get_asset_graph` | read | read | none | `manage_asset.analyze_graph` |
| `asset.bulk_delete` | `manage_asset` | `bulk_delete` | destructive | destructive | elevated | `manage_asset.bulk_delete` |
| `asset.bulk_rename` | `manage_asset` | `bulk_rename` | write | write | explicit | `manage_asset.bulk_rename` |
| `asset.create_folder` | `manage_asset` | `create_folder` | write | write | explicit | `manage_asset.create_folder` |
| `asset.create_render_target` | `manage_asset` | `manage_texture` | write | write | explicit | `manage_asset.create_render_target` |
| `asset.create_thumbnail` | `manage_asset` | `generate_thumbnail` | write | write | explicit | `manage_asset.create_thumbnail` |
| `asset.delete` | `manage_asset` | `delete` | destructive | destructive | elevated | `manage_asset.delete` |
| `asset.delete_asset` | `manage_asset` | `delete_asset` | destructive | destructive | elevated | `manage_asset.delete_asset` |
| `asset.delete_assets` | `manage_asset` | `delete_assets` | destructive | destructive | elevated | `manage_asset.delete_assets` |
| `asset.duplicate` | `manage_asset` | `duplicate` | write | write | explicit | `manage_asset.duplicate` |
| `asset.duplicate_asset` | `manage_asset` | `duplicate_asset` | write | write | explicit | `manage_asset.duplicate_asset` |
| `asset.exists` | `manage_asset` | `exists` | read | read | none | `manage_asset.exists` |
| `asset.find_by_tag` | `manage_asset` | `asset_query` | read | read | none | `manage_asset.find_by_tag` |
| `asset.fixup_redirectors` | `manage_asset` | `fixup_redirectors` | write | write | explicit | `manage_asset.fixup_redirectors` |
| `asset.generate_lods` | `manage_asset` | `generate_lods` | write | write | explicit | `manage_asset.generate_lods` |
| `asset.generate_report` | `manage_asset` | `generate_report` | read | read | none | `manage_asset.generate_report` |
| `asset.get_asset_graph` | `manage_asset` | `get_asset_graph` | read | read | none | `manage_asset.get_asset_graph` |
| `asset.get_dependencies` | `manage_asset` | `get_dependencies` | read | read | none | `manage_asset.get_dependencies` |
| `asset.get_material_stats` | `manage_asset` | `get_material_stats` | read | read | none | `manage_asset.get_material_stats` |
| `asset.get_metadata` | `manage_asset` | `get_metadata` | read | read | none | `manage_asset.get_metadata` |
| `asset.get_source_control_state` | `manage_asset` | `asset_query` | read | read | none | `manage_asset.get_source_control_state` |
| `asset.import` | `manage_asset` | `import` | write | write | explicit | `manage_asset.import` |
| `asset.list` | `manage_asset` | `list` | read | read | none | `manage_asset.list` |
| `asset.list_instances` | `manage_asset` | `list_instances` | read | read | none | `manage_asset.list_instances` |
| `asset.move` | `manage_asset` | `move` | write | write | explicit | `manage_asset.move` |
| `asset.move_asset` | `manage_asset` | `move_asset` | write | write | explicit | `manage_asset.move_asset` |
| `asset.nanite_rebuild_mesh` | `manage_asset` | `manage_render` | write | write | explicit | `manage_asset.nanite_rebuild_mesh` |
| `asset.rename` | `manage_asset` | `rename` | write | write | explicit | `manage_asset.rename` |
| `asset.rename_asset` | `manage_asset` | `rename_asset` | write | write | explicit | `manage_asset.rename_asset` |
| `asset.reset_instance_parameters` | `manage_asset` | `reset_instance_parameters` | write | write | explicit | `manage_asset.reset_instance_parameters` |
| `asset.search_assets` | `manage_asset` | `asset_query` | read | read | none | `manage_asset.search_assets` |
| `asset.set_metadata` | `manage_asset` | `set_metadata` | write | write | explicit | `manage_asset.set_metadata` |
| `asset.set_tags` | `manage_asset` | `set_tags` | write | write | explicit | `manage_asset.set_tags` |
| `asset.source_control_checkout` | `manage_asset` | `source_control_checkout` | write | write | explicit | `manage_asset.source_control_checkout` |
| `asset.source_control_submit` | `manage_asset` | `source_control_submit` | write | write | explicit | `manage_asset.source_control_submit` |
| `asset.validate` | `manage_asset` | `validate` | read | read | none | `manage_asset.validate` |
| `blueprint.add_ammo_counter` | `manage_blueprint` | `add_ammo_counter` | write | write | none | `manage_blueprint.add_ammo_counter` |
| `blueprint.add_animation_keyframe` | `manage_blueprint` | `add_animation_keyframe` | write | write | none | `manage_blueprint.add_animation_keyframe` |
| `blueprint.add_animation_track` | `manage_blueprint` | `add_animation_track` | write | write | none | `manage_blueprint.add_animation_track` |
| `blueprint.add_border` | `manage_blueprint` | `add_border` | write | write | none | `manage_blueprint.add_border` |
| `blueprint.add_button` | `manage_blueprint` | `add_button` | write | write | none | `manage_blueprint.add_button` |
| `blueprint.add_canvas_panel` | `manage_blueprint` | `add_canvas_panel` | write | write | none | `manage_blueprint.add_canvas_panel` |
| `blueprint.add_check_box` | `manage_blueprint` | `add_check_box` | write | write | none | `manage_blueprint.add_check_box` |
| `blueprint.add_combo_box` | `manage_blueprint` | `add_combo_box` | write | write | none | `manage_blueprint.add_combo_box` |
| `blueprint.add_compass` | `manage_blueprint` | `add_compass` | write | write | none | `manage_blueprint.add_compass` |
| `blueprint.add_component` | `manage_blueprint` | `add_component` | write | write | none | `manage_blueprint.add_component` |
| `blueprint.add_construction_script` | `manage_blueprint` | `add_construction_script` | write | write | none | `manage_blueprint.add_construction_script` |
| `blueprint.add_crosshair` | `manage_blueprint` | `add_crosshair` | write | write | none | `manage_blueprint.add_crosshair` |
| `blueprint.add_damage_indicator` | `manage_blueprint` | `add_damage_indicator` | write | write | none | `manage_blueprint.add_damage_indicator` |
| `blueprint.add_event` | `manage_blueprint` | `add_event` | write | write | none | `manage_blueprint.add_event` |
| `blueprint.add_function` | `manage_blueprint` | `add_function` | write | write | none | `manage_blueprint.add_function` |
| `blueprint.add_grid_panel` | `manage_blueprint` | `add_grid_panel` | write | write | none | `manage_blueprint.add_grid_panel` |
| `blueprint.add_health_bar` | `manage_blueprint` | `add_health_bar` | write | write | none | `manage_blueprint.add_health_bar` |
| `blueprint.add_horizontal_box` | `manage_blueprint` | `add_horizontal_box` | write | write | none | `manage_blueprint.add_horizontal_box` |
| `blueprint.add_image` | `manage_blueprint` | `add_image` | write | write | none | `manage_blueprint.add_image` |
| `blueprint.add_interaction_prompt` | `manage_blueprint` | `add_interaction_prompt` | write | write | none | `manage_blueprint.add_interaction_prompt` |
| `blueprint.add_list_view` | `manage_blueprint` | `add_list_view` | write | write | none | `manage_blueprint.add_list_view` |
| `blueprint.add_minimap` | `manage_blueprint` | `add_minimap` | write | write | none | `manage_blueprint.add_minimap` |
| `blueprint.add_node` | `manage_blueprint` | `add_node` | write | write | none | `manage_blueprint.add_node` |
| `blueprint.add_objective_tracker` | `manage_blueprint` | `add_objective_tracker` | write | write | none | `manage_blueprint.add_objective_tracker` |
| `blueprint.add_overlay` | `manage_blueprint` | `add_overlay` | write | write | none | `manage_blueprint.add_overlay` |
| `blueprint.add_progress_bar` | `manage_blueprint` | `add_progress_bar` | write | write | none | `manage_blueprint.add_progress_bar` |
| `blueprint.add_rich_text_block` | `manage_blueprint` | `add_rich_text_block` | write | write | none | `manage_blueprint.add_rich_text_block` |
| `blueprint.add_scale_box` | `manage_blueprint` | `add_scale_box` | write | write | none | `manage_blueprint.add_scale_box` |
| `blueprint.add_scroll_box` | `manage_blueprint` | `add_scroll_box` | write | write | none | `manage_blueprint.add_scroll_box` |
| `blueprint.add_scs_component` | `manage_blueprint` | `add_scs_component` | write | write | none | `manage_blueprint.add_scs_component` |
| `blueprint.add_size_box` | `manage_blueprint` | `add_size_box` | write | write | none | `manage_blueprint.add_size_box` |
| `blueprint.add_slider` | `manage_blueprint` | `add_slider` | write | write | none | `manage_blueprint.add_slider` |
| `blueprint.add_spin_box` | `manage_blueprint` | `add_spin_box` | write | write | none | `manage_blueprint.add_spin_box` |
| `blueprint.add_text_block` | `manage_blueprint` | `add_text_block` | write | write | none | `manage_blueprint.add_text_block` |
| `blueprint.add_text_input` | `manage_blueprint` | `add_text_input` | write | write | none | `manage_blueprint.add_text_input` |
| `blueprint.add_tree_view` | `manage_blueprint` | `add_tree_view` | write | write | none | `manage_blueprint.add_tree_view` |
| `blueprint.add_uniform_grid` | `manage_blueprint` | `add_uniform_grid` | write | write | none | `manage_blueprint.add_uniform_grid` |
| `blueprint.add_variable` | `manage_blueprint` | `add_variable` | write | write | none | `manage_blueprint.add_variable` |
| `blueprint.add_vertical_box` | `manage_blueprint` | `add_vertical_box` | write | write | none | `manage_blueprint.add_vertical_box` |
| `blueprint.add_wrap_box` | `manage_blueprint` | `add_wrap_box` | write | write | none | `manage_blueprint.add_wrap_box` |
| `blueprint.bind_color` | `manage_blueprint` | `bind_color` | write | write | none | `manage_blueprint.bind_color` |
| `blueprint.bind_enabled` | `manage_blueprint` | `bind_enabled` | write | write | none | `manage_blueprint.bind_enabled` |
| `blueprint.bind_on_clicked` | `manage_blueprint` | `bind_on_clicked` | write | write | none | `manage_blueprint.bind_on_clicked` |
| `blueprint.bind_on_hovered` | `manage_blueprint` | `bind_on_hovered` | write | write | none | `manage_blueprint.bind_on_hovered` |
| `blueprint.bind_on_value_changed` | `manage_blueprint` | `bind_on_value_changed` | write | write | none | `manage_blueprint.bind_on_value_changed` |
| `blueprint.bind_text` | `manage_blueprint` | `bind_text` | write | write | none | `manage_blueprint.bind_text` |
| `blueprint.bind_visibility` | `manage_blueprint` | `bind_visibility` | write | write | none | `manage_blueprint.bind_visibility` |
| `blueprint.break_pin_links` | `manage_blueprint` | `break_pin_links` | destructive | destructive | explicit | `manage_blueprint.break_pin_links` |
| `blueprint.compile` | `manage_blueprint` | `compile` | write | write | none | `manage_blueprint.compile` |
| `blueprint.connect_pins` | `manage_blueprint` | `connect_pins` | write | write | none | `manage_blueprint.connect_pins` |
| `blueprint.create` | `manage_blueprint` | `create` | write | write | none | `manage_blueprint.create` |
| `blueprint.create_blueprint` | `manage_blueprint` | `create_blueprint` | write | write | none | `manage_blueprint.create_blueprint` |
| `blueprint.create_dialog_widget` | `manage_blueprint` | `create_dialog_widget` | write | write | none | `manage_blueprint.create_dialog_widget` |
| `blueprint.create_hud_widget` | `manage_blueprint` | `create_hud_widget` | write | write | none | `manage_blueprint.create_hud_widget` |
| `blueprint.create_inventory_ui` | `manage_blueprint` | `create_inventory_ui` | write | write | none | `manage_blueprint.create_inventory_ui` |
| `blueprint.create_loading_screen` | `manage_blueprint` | `create_loading_screen` | write | write | none | `manage_blueprint.create_loading_screen` |
| `blueprint.create_main_menu` | `manage_blueprint` | `create_main_menu` | write | write | none | `manage_blueprint.create_main_menu` |
| `blueprint.create_node` | `manage_blueprint` | `create_node` | write | write | none | `manage_blueprint.create_node` |
| `blueprint.create_pause_menu` | `manage_blueprint` | `create_pause_menu` | write | write | none | `manage_blueprint.create_pause_menu` |
| `blueprint.create_property_binding` | `manage_blueprint` | `create_property_binding` | write | write | none | `manage_blueprint.create_property_binding` |
| `blueprint.create_radial_menu` | `manage_blueprint` | `create_radial_menu` | write | write | none | `manage_blueprint.create_radial_menu` |
| `blueprint.create_reroute_node` | `manage_blueprint` | `create_reroute_node` | write | write | none | `manage_blueprint.create_reroute_node` |
| `blueprint.create_settings_menu` | `manage_blueprint` | `create_settings_menu` | write | write | none | `manage_blueprint.create_settings_menu` |
| `blueprint.create_struct_make_break_nodes` | `manage_blueprint` | `create_struct_make_break_nodes` | write | write | none | `manage_blueprint.create_struct_make_break_nodes` |
| `blueprint.create_widget_animation` | `manage_blueprint` | `create_widget_animation` | write | write | none | `manage_blueprint.create_widget_animation` |
| `blueprint.create_widget_blueprint` | `manage_blueprint` | `create_widget_blueprint` | write | write | none | `manage_blueprint.create_widget_blueprint` |
| `blueprint.delete_node` | `manage_blueprint` | `delete_node` | destructive | destructive | explicit | `manage_blueprint.delete_node` |
| `blueprint.ensure_exists` | `manage_blueprint` | `ensure_exists` | write | write | none | `manage_blueprint.ensure_exists` |
| `blueprint.get` | `manage_blueprint` | `get` | read | read | none | `manage_blueprint.get` |
| `blueprint.get_blueprint` | `manage_blueprint` | `get_blueprint` | read | read | none | `manage_blueprint.get_blueprint` |
| `blueprint.get_graph_details` | `manage_blueprint` | `get_graph_details` | read | read | none | `manage_blueprint.get_graph_details` |
| `blueprint.get_node_details` | `manage_blueprint` | `get_node_details` | read | read | none | `manage_blueprint.get_node_details` |
| `blueprint.get_pin_details` | `manage_blueprint` | `get_pin_details` | read | read | none | `manage_blueprint.get_pin_details` |
| `blueprint.get_scs` | `manage_blueprint` | `get_scs` | read | read | none | `manage_blueprint.get_scs` |
| `blueprint.get_widget_info` | `manage_blueprint` | `get_widget_info` | read | read | none | `manage_blueprint.get_widget_info` |
| `blueprint.list_node_types` | `manage_blueprint` | `list_node_types` | read | read | none | `manage_blueprint.list_node_types` |
| `blueprint.modify_scs` | `manage_blueprint` | `modify_scs` | write | write | none | `manage_blueprint.modify_scs` |
| `blueprint.preview_widget` | `manage_blueprint` | `preview_widget` | write | write | none | `manage_blueprint.preview_widget` |
| `blueprint.probe_handle` | `manage_blueprint` | `probe_handle` | read | read | none | `manage_blueprint.probe_handle` |
| `blueprint.remove_event` | `manage_blueprint` | `remove_event` | destructive | destructive | explicit | `manage_blueprint.remove_event` |
| `blueprint.remove_function` | `manage_blueprint` | `remove_function` | destructive | destructive | explicit | `manage_blueprint.remove_function` |
| `blueprint.remove_scs_component` | `manage_blueprint` | `remove_scs_component` | destructive | destructive | explicit | `manage_blueprint.remove_scs_component` |
| `blueprint.remove_variable` | `manage_blueprint` | `remove_variable` | destructive | destructive | explicit | `manage_blueprint.remove_variable` |
| `blueprint.rename_variable` | `manage_blueprint` | `rename_variable` | write | write | none | `manage_blueprint.rename_variable` |
| `blueprint.reparent_scs_component` | `manage_blueprint` | `reparent_scs_component` | write | write | none | `manage_blueprint.reparent_scs_component` |
| `blueprint.set_alignment` | `manage_blueprint` | `set_alignment` | write | write | none | `manage_blueprint.set_alignment` |
| `blueprint.set_anchor` | `manage_blueprint` | `set_anchor` | write | write | none | `manage_blueprint.set_anchor` |
| `blueprint.set_animation_loop` | `manage_blueprint` | `set_animation_loop` | write | write | none | `manage_blueprint.set_animation_loop` |
| `blueprint.set_clipping` | `manage_blueprint` | `set_clipping` | write | write | none | `manage_blueprint.set_clipping` |
| `blueprint.set_default` | `manage_blueprint` | `set_default` | write | write | none | `manage_blueprint.set_default` |
| `blueprint.set_metadata` | `manage_blueprint` | `set_metadata` | write | write | none | `manage_blueprint.set_metadata` |
| `blueprint.set_node_property` | `manage_blueprint` | `set_node_property` | write | write | none | `manage_blueprint.set_node_property` |
| `blueprint.set_padding` | `manage_blueprint` | `set_padding` | write | write | none | `manage_blueprint.set_padding` |
| `blueprint.set_pin_default_value` | `manage_blueprint` | `set_pin_default_value` | write | write | none | `manage_blueprint.set_pin_default_value` |
| `blueprint.set_position` | `manage_blueprint` | `set_position` | write | write | none | `manage_blueprint.set_position` |
| `blueprint.set_render_transform` | `manage_blueprint` | `set_render_transform` | write | write | none | `manage_blueprint.set_render_transform` |
| `blueprint.set_scs_property` | `manage_blueprint` | `set_scs_property` | write | write | none | `manage_blueprint.set_scs_property` |
| `blueprint.set_scs_transform` | `manage_blueprint` | `set_scs_transform` | write | write | none | `manage_blueprint.set_scs_transform` |
| `blueprint.set_size` | `manage_blueprint` | `set_size` | write | write | none | `manage_blueprint.set_size` |
| `blueprint.set_style` | `manage_blueprint` | `set_style` | write | write | none | `manage_blueprint.set_style` |
| `blueprint.set_variable_metadata` | `manage_blueprint` | `set_variable_metadata` | write | write | none | `manage_blueprint.set_variable_metadata` |
| `blueprint.set_visibility` | `manage_blueprint` | `set_visibility` | write | write | none | `manage_blueprint.set_visibility` |
| `blueprint.set_widget_parent_class` | `manage_blueprint` | `set_widget_parent_class` | write | write | none | `manage_blueprint.set_widget_parent_class` |
| `blueprint.set_z_order` | `manage_blueprint` | `set_z_order` | write | write | none | `manage_blueprint.set_z_order` |
| `build_environment.add_foliage` | `build_environment` | `add_foliage` | write | write | none | `build_environment.add_foliage` |
| `build_environment.add_foliage_instances` | `build_environment` | `add_foliage_instances` | write | write | none | `build_environment.add_foliage_instances` |
| `build_environment.add_spline_point` | `build_environment` | `add_spline_point` | write | write | none | `build_environment.add_spline_point` |
| `build_environment.assign_render_target` | `build_environment` | `assign_render_target` | write | write | none | `build_environment.assign_render_target` |
| `build_environment.bake_lightmap` | `build_environment` | `bake_lightmap` | write | write | none | `build_environment.bake_lightmap` |
| `build_environment.build_lighting` | `build_environment` | `build_lighting` | write | write | none | `build_environment.build_lighting` |
| `build_environment.build_lighting_quality` | `build_environment` | `build_lighting_quality` | write | write | none | `build_environment.build_lighting_quality` |
| `build_environment.capture_scene` | `build_environment` | `capture_scene` | write | write | none | `build_environment.capture_scene` |
| `build_environment.configure_bloom` | `build_environment` | `configure_bloom` | write | write | none | `build_environment.configure_bloom` |
| `build_environment.configure_bokeh` | `build_environment` | `configure_bokeh` | write | write | none | `build_environment.configure_bokeh` |
| `build_environment.configure_capture_offset` | `build_environment` | `configure_capture_offset` | write | write | none | `build_environment.configure_capture_offset` |
| `build_environment.configure_capture_resolution` | `build_environment` | `configure_capture_resolution` | write | write | none | `build_environment.configure_capture_resolution` |
| `build_environment.configure_capture_source` | `build_environment` | `configure_capture_source` | write | write | none | `build_environment.configure_capture_source` |
| `build_environment.configure_chromatic_aberration` | `build_environment` | `configure_chromatic_aberration` | write | write | none | `build_environment.configure_chromatic_aberration` |
| `build_environment.configure_directional_light_atmosphere` | `build_environment` | `configure_directional_light_atmosphere` | write | write | none | `build_environment.configure_directional_light_atmosphere` |
| `build_environment.configure_dof` | `build_environment` | `configure_dof` | write | write | none | `build_environment.configure_dof` |
| `build_environment.configure_exponential_height_fog` | `build_environment` | `configure_exponential_height_fog` | write | write | none | `build_environment.configure_exponential_height_fog` |
| `build_environment.configure_exposure` | `build_environment` | `configure_exposure` | write | write | none | `build_environment.configure_exposure` |
| `build_environment.configure_foliage_collision` | `build_environment` | `configure_foliage_collision` | write | write | none | `build_environment.configure_foliage_collision` |
| `build_environment.configure_foliage_culling` | `build_environment` | `configure_foliage_culling` | write | write | none | `build_environment.configure_foliage_culling` |
| `build_environment.configure_foliage_lod` | `build_environment` | `configure_foliage_lod` | write | write | none | `build_environment.configure_foliage_lod` |
| `build_environment.configure_foliage_mesh` | `build_environment` | `configure_foliage_mesh` | write | write | none | `build_environment.configure_foliage_mesh` |
| `build_environment.configure_foliage_placement` | `build_environment` | `configure_foliage_placement` | write | write | none | `build_environment.configure_foliage_placement` |
| `build_environment.configure_grain` | `build_environment` | `configure_grain` | write | write | none | `build_environment.configure_grain` |
| `build_environment.configure_gtao` | `build_environment` | `configure_gtao` | write | write | none | `build_environment.configure_gtao` |
| `build_environment.configure_indirect_lighting_cache` | `build_environment` | `configure_indirect_lighting_cache` | write | write | none | `build_environment.configure_indirect_lighting_cache` |
| `build_environment.configure_landscape_lod` | `build_environment` | `configure_landscape_lod` | write | write | none | `build_environment.configure_landscape_lod` |
| `build_environment.configure_landscape_material` | `build_environment` | `configure_landscape_material` | write | write | none | `build_environment.configure_landscape_material` |
| `build_environment.configure_landscape_splines` | `build_environment` | `configure_landscape_splines` | write | write | none | `build_environment.configure_landscape_splines` |
| `build_environment.configure_lens_flare` | `build_environment` | `configure_lens_flare` | write | write | none | `build_environment.configure_lens_flare` |
| `build_environment.configure_light_color_curve` | `build_environment` | `configure_light_color_curve` | write | write | none | `build_environment.configure_light_color_curve` |
| `build_environment.configure_lightmass_settings` | `build_environment` | `configure_lightmass_settings` | write | write | none | `build_environment.configure_lightmass_settings` |
| `build_environment.configure_lightning` | `build_environment` | `configure_lightning` | write | write | none | `build_environment.configure_lightning` |
| `build_environment.configure_lumen_reflection_settings` | `build_environment` | `configure_lumen_reflection_settings` | write | write | none | `build_environment.configure_lumen_reflection_settings` |
| `build_environment.configure_mesh_randomization` | `build_environment` | `configure_mesh_randomization` | write | write | none | `build_environment.configure_mesh_randomization` |
| `build_environment.configure_mesh_spacing` | `build_environment` | `configure_mesh_spacing` | write | write | none | `build_environment.configure_mesh_spacing` |
| `build_environment.configure_motion_blur` | `build_environment` | `configure_motion_blur` | write | write | none | `build_environment.configure_motion_blur` |
| `build_environment.configure_path_tracing` | `build_environment` | `configure_path_tracing` | write | write | none | `build_environment.configure_path_tracing` |
| `build_environment.configure_planar_reflection` | `build_environment` | `configure_planar_reflection` | write | write | none | `build_environment.configure_planar_reflection` |
| `build_environment.configure_pp_blend` | `build_environment` | `configure_pp_blend` | write | write | none | `build_environment.configure_pp_blend` |
| `build_environment.configure_rain_particles` | `build_environment` | `configure_rain_particles` | write | write | none | `build_environment.configure_rain_particles` |
| `build_environment.configure_ray_traced_ao` | `build_environment` | `configure_ray_traced_ao` | write | write | none | `build_environment.configure_ray_traced_ao` |
| `build_environment.configure_ray_traced_gi` | `build_environment` | `configure_ray_traced_gi` | write | write | none | `build_environment.configure_ray_traced_gi` |
| `build_environment.configure_ray_traced_reflections` | `build_environment` | `configure_ray_traced_reflections` | write | write | none | `build_environment.configure_ray_traced_reflections` |
| `build_environment.configure_ray_traced_shadows` | `build_environment` | `configure_ray_traced_shadows` | write | write | none | `build_environment.configure_ray_traced_shadows` |
| `build_environment.configure_reflection_capture_resolution` | `build_environment` | `configure_reflection_capture_resolution` | write | write | none | `build_environment.configure_reflection_capture_resolution` |
| `build_environment.configure_screen_percentage` | `build_environment` | `configure_screen_percentage` | write | write | none | `build_environment.configure_screen_percentage` |
| `build_environment.configure_shadows` | `build_environment` | `configure_shadows` | write | write | none | `build_environment.configure_shadows` |
| `build_environment.configure_sky_atmosphere` | `build_environment` | `configure_sky_atmosphere` | write | write | none | `build_environment.configure_sky_atmosphere` |
| `build_environment.configure_sky_color_curve` | `build_environment` | `configure_sky_color_curve` | write | write | none | `build_environment.configure_sky_color_curve` |
| `build_environment.configure_sky_light` | `build_environment` | `configure_sky_light` | write | write | none | `build_environment.configure_sky_light` |
| `build_environment.configure_snow_particles` | `build_environment` | `configure_snow_particles` | write | write | none | `build_environment.configure_snow_particles` |
| `build_environment.configure_spline_mesh_axis` | `build_environment` | `configure_spline_mesh_axis` | write | write | none | `build_environment.configure_spline_mesh_axis` |
| `build_environment.configure_ssao` | `build_environment` | `configure_ssao` | write | write | none | `build_environment.configure_ssao` |
| `build_environment.configure_ssr_settings` | `build_environment` | `configure_ssr_settings` | write | write | none | `build_environment.configure_ssr_settings` |
| `build_environment.configure_sun_position` | `build_environment` | `configure_sun_position` | write | write | none | `build_environment.configure_sun_position` |
| `build_environment.configure_tonemapper` | `build_environment` | `configure_tonemapper` | write | write | none | `build_environment.configure_tonemapper` |
| `build_environment.configure_vignette` | `build_environment` | `configure_vignette` | write | write | none | `build_environment.configure_vignette` |
| `build_environment.configure_volumetric_cloud` | `build_environment` | `configure_volumetric_cloud` | write | write | none | `build_environment.configure_volumetric_cloud` |
| `build_environment.configure_water_collision` | `build_environment` | `configure_water_collision` | write | write | none | `build_environment.configure_water_collision` |
| `build_environment.configure_water_material` | `build_environment` | `configure_water_material` | write | write | none | `build_environment.configure_water_material` |
| `build_environment.configure_water_waves` | `build_environment` | `configure_water_waves` | write | write | none | `build_environment.configure_water_waves` |
| `build_environment.configure_wind` | `build_environment` | `configure_wind` | write | write | none | `build_environment.configure_wind` |
| `build_environment.create_box_reflection_capture` | `build_environment` | `create_box_reflection_capture` | write | write | none | `build_environment.create_box_reflection_capture` |
| `build_environment.create_buoyancy_component` | `build_environment` | `create_buoyancy_component` | write | write | none | `build_environment.create_buoyancy_component` |
| `build_environment.create_cable_spline` | `build_environment` | `create_cable_spline` | write | write | none | `build_environment.create_cable_spline` |
| `build_environment.create_dynamic_light` | `build_environment` | `create_dynamic_light` | write | write | none | `build_environment.create_dynamic_light` |
| `build_environment.create_fence_spline` | `build_environment` | `create_fence_spline` | write | write | none | `build_environment.create_fence_spline` |
| `build_environment.create_fog_volume` | `build_environment` | `create_fog_volume` | write | write | none | `build_environment.create_fog_volume` |
| `build_environment.create_foliage_type` | `build_environment` | `add_foliage_type` | write | write | none | `build_environment.create_foliage_type` |
| `build_environment.create_landscape` | `build_environment` | `create_landscape` | write | write | none | `build_environment.create_landscape` |
| `build_environment.create_landscape_grass_type` | `build_environment` | `create_landscape_grass_type` | write | write | none | `build_environment.create_landscape_grass_type` |
| `build_environment.create_landscape_layer_info` | `build_environment` | `create_landscape_layer_info` | write | write | none | `build_environment.create_landscape_layer_info` |
| `build_environment.create_landscape_streaming_proxy` | `build_environment` | `create_landscape_streaming_proxy` | write | write | none | `build_environment.create_landscape_streaming_proxy` |
| `build_environment.create_light` | `build_environment` | `create_light` | write | write | none | `build_environment.create_light` |
| `build_environment.create_lighting_enabled_level` | `build_environment` | `create_lighting_enabled_level` | write | write | none | `build_environment.create_lighting_enabled_level` |
| `build_environment.create_lightmass_volume` | `build_environment` | `create_lightmass_volume` | write | write | none | `build_environment.create_lightmass_volume` |
| `build_environment.create_pipe_spline` | `build_environment` | `create_pipe_spline` | write | write | none | `build_environment.create_pipe_spline` |
| `build_environment.create_planar_reflection` | `build_environment` | `create_planar_reflection` | write | write | none | `build_environment.create_planar_reflection` |
| `build_environment.create_procedural_foliage` | `build_environment` | `create_procedural_foliage` | write | write | none | `build_environment.create_procedural_foliage` |
| `build_environment.create_procedural_terrain` | `build_environment` | `create_procedural_terrain` | write | write | none | `build_environment.create_procedural_terrain` |
| `build_environment.create_river_spline` | `build_environment` | `create_river_spline` | write | write | none | `build_environment.create_river_spline` |
| `build_environment.create_road_spline` | `build_environment` | `create_road_spline` | write | write | none | `build_environment.create_road_spline` |
| `build_environment.create_scene_capture_2d` | `build_environment` | `create_scene_capture_2d` | write | write | none | `build_environment.create_scene_capture_2d` |
| `build_environment.create_scene_capture_cube` | `build_environment` | `create_scene_capture_cube` | write | write | none | `build_environment.create_scene_capture_cube` |
| `build_environment.create_sky_light` | `build_environment` | `create_sky_light` | write | write | none | `build_environment.create_sky_light` |
| `build_environment.create_sky_sphere` | `build_environment` | `create_sky_sphere` | write | write | none | `build_environment.create_sky_sphere` |
| `build_environment.create_sphere_reflection_capture` | `build_environment` | `create_sphere_reflection_capture` | write | write | none | `build_environment.create_sphere_reflection_capture` |
| `build_environment.create_spline_actor` | `build_environment` | `create_spline_actor` | write | write | none | `build_environment.create_spline_actor` |
| `build_environment.create_spline_mesh_component` | `build_environment` | `create_spline_mesh_component` | write | write | none | `build_environment.create_spline_mesh_component` |
| `build_environment.create_time_of_day_system` | `build_environment` | `create_time_of_day_system` | write | write | none | `build_environment.create_time_of_day_system` |
| `build_environment.create_wall_spline` | `build_environment` | `create_wall_spline` | write | write | none | `build_environment.create_wall_spline` |
| `build_environment.create_water_body_custom` | `build_environment` | `create_water_body_custom` | write | write | none | `build_environment.create_water_body_custom` |
| `build_environment.create_water_body_lake` | `build_environment` | `create_water_body_lake` | write | write | none | `build_environment.create_water_body_lake` |
| `build_environment.create_water_body_ocean` | `build_environment` | `create_water_body_ocean` | write | write | none | `build_environment.create_water_body_ocean` |
| `build_environment.create_water_body_river` | `build_environment` | `create_water_body_river` | write | write | none | `build_environment.create_water_body_river` |
| `build_environment.create_weather_system` | `build_environment` | `create_weather_system` | write | write | none | `build_environment.create_weather_system` |
| `build_environment.delete` | `build_environment` | `delete` | destructive | destructive | explicit | `build_environment.delete` |
| `build_environment.ensure_single_sky_light` | `build_environment` | `ensure_single_sky_light` | write | write | none | `build_environment.ensure_single_sky_light` |
| `build_environment.export_heightmap` | `build_environment` | `export_heightmap` | read | read | none | `build_environment.export_heightmap` |
| `build_environment.export_snapshot` | `build_environment` | `export_snapshot` | read | read | none | `build_environment.export_snapshot` |
| `build_environment.generate_lods` | `build_environment` | `generate_lods` | write | write | none | `build_environment.generate_lods` |
| `build_environment.get_foliage_instances` | `build_environment` | `get_foliage_instances` | read | read | none | `build_environment.get_foliage_instances` |
| `build_environment.get_splines_info` | `build_environment` | `get_splines_info` | read | read | none | `build_environment.get_splines_info` |
| `build_environment.import_heightmap` | `build_environment` | `import_heightmap` | write | write | none | `build_environment.import_heightmap` |
| `build_environment.import_snapshot` | `build_environment` | `import_snapshot` | write | write | none | `build_environment.import_snapshot` |
| `build_environment.list_light_types` | `build_environment` | `list_light_types` | read | read | none | `build_environment.list_light_types` |
| `build_environment.modify_heightmap` | `build_environment` | `modify_heightmap` | write | write | none | `build_environment.modify_heightmap` |
| `build_environment.paint_foliage` | `build_environment` | `paint_foliage` | write | write | none | `build_environment.paint_foliage` |
| `build_environment.paint_foliage_instances` | `build_environment` | `paint_foliage_instances` | write | write | none | `build_environment.paint_foliage_instances` |
| `build_environment.paint_landscape` | `build_environment` | `paint_landscape` | write | write | none | `build_environment.paint_landscape` |
| `build_environment.paint_landscape_layer` | `build_environment` | `paint_landscape_layer` | write | write | none | `build_environment.paint_landscape_layer` |
| `build_environment.recapture_scene` | `build_environment` | `recapture_scene` | write | write | none | `build_environment.recapture_scene` |
| `build_environment.remove_foliage` | `build_environment` | `remove_foliage` | destructive | destructive | explicit | `build_environment.remove_foliage` |
| `build_environment.remove_foliage_instances` | `build_environment` | `remove_foliage_instances` | destructive | destructive | explicit | `build_environment.remove_foliage_instances` |
| `build_environment.remove_spline_point` | `build_environment` | `remove_spline_point` | destructive | destructive | explicit | `build_environment.remove_spline_point` |
| `build_environment.scatter_meshes_along_spline` | `build_environment` | `scatter_meshes_along_spline` | write | write | none | `build_environment.scatter_meshes_along_spline` |
| `build_environment.sculpt` | `build_environment` | `sculpt_landscape` | write | write | none | `build_environment.sculpt` |
| `build_environment.sculpt_landscape` | `build_environment` | `sculpt_landscape` | write | write | none | `build_environment.sculpt_landscape` |
| `build_environment.set_actor_light_channel` | `build_environment` | `set_actor_light_channel` | write | write | none | `build_environment.set_actor_light_channel` |
| `build_environment.set_ambient_occlusion` | `build_environment` | `set_ambient_occlusion` | write | write | none | `build_environment.set_ambient_occlusion` |
| `build_environment.set_aperture` | `build_environment` | `set_aperture` | write | write | none | `build_environment.set_aperture` |
| `build_environment.set_bloom_intensity` | `build_environment` | `set_bloom_intensity` | write | write | none | `build_environment.set_bloom_intensity` |
| `build_environment.set_bloom_threshold` | `build_environment` | `set_bloom_threshold` | write | write | none | `build_environment.set_bloom_threshold` |
| `build_environment.set_dof_method` | `build_environment` | `set_dof_method` | write | write | none | `build_environment.set_dof_method` |
| `build_environment.set_exposure` | `build_environment` | `set_exposure` | write | write | none | `build_environment.set_exposure` |
| `build_environment.set_exposure_compensation` | `build_environment` | `set_exposure_compensation` | write | write | none | `build_environment.set_exposure_compensation` |
| `build_environment.set_exposure_method` | `build_environment` | `set_exposure_method` | write | write | none | `build_environment.set_exposure_method` |
| `build_environment.set_exposure_min_max` | `build_environment` | `set_exposure_min_max` | write | write | none | `build_environment.set_exposure_min_max` |
| `build_environment.set_focal_distance` | `build_environment` | `set_focal_distance` | write | write | none | `build_environment.set_focal_distance` |
| `build_environment.set_landscape_material` | `build_environment` | `set_landscape_material` | write | write | none | `build_environment.set_landscape_material` |
| `build_environment.set_light_channel` | `build_environment` | `set_light_channel` | write | write | none | `build_environment.set_light_channel` |
| `build_environment.set_motion_blur_amount` | `build_environment` | `set_motion_blur_amount` | write | write | none | `build_environment.set_motion_blur_amount` |
| `build_environment.set_motion_blur_max` | `build_environment` | `set_motion_blur_max` | write | write | none | `build_environment.set_motion_blur_max` |
| `build_environment.set_pp_color_grading` | `build_environment` | `set_pp_color_grading` | write | write | none | `build_environment.set_pp_color_grading` |
| `build_environment.set_pp_lut` | `build_environment` | `set_pp_lut` | write | write | none | `build_environment.set_pp_lut` |
| `build_environment.set_pp_white_balance` | `build_environment` | `set_pp_white_balance` | write | write | none | `build_environment.set_pp_white_balance` |
| `build_environment.set_spline_mesh_asset` | `build_environment` | `set_spline_mesh_asset` | write | write | none | `build_environment.set_spline_mesh_asset` |
| `build_environment.set_spline_mesh_material` | `build_environment` | `set_spline_mesh_material` | write | write | none | `build_environment.set_spline_mesh_material` |
| `build_environment.set_spline_point_position` | `build_environment` | `set_spline_point_position` | write | write | none | `build_environment.set_spline_point_position` |
| `build_environment.set_spline_point_rotation` | `build_environment` | `set_spline_point_rotation` | write | write | none | `build_environment.set_spline_point_rotation` |
| `build_environment.set_spline_point_scale` | `build_environment` | `set_spline_point_scale` | write | write | none | `build_environment.set_spline_point_scale` |
| `build_environment.set_spline_point_tangents` | `build_environment` | `set_spline_point_tangents` | write | write | none | `build_environment.set_spline_point_tangents` |
| `build_environment.set_spline_type` | `build_environment` | `set_spline_type` | write | write | none | `build_environment.set_spline_type` |
| `build_environment.set_time_of_day` | `build_environment` | `set_time_of_day` | write | write | none | `build_environment.set_time_of_day` |
| `build_environment.set_tonemapper_type` | `build_environment` | `set_tonemapper_type` | write | write | none | `build_environment.set_tonemapper_type` |
| `build_environment.setup_global_illumination` | `build_environment` | `setup_global_illumination` | write | write | none | `build_environment.setup_global_illumination` |
| `build_environment.setup_volumetric_fog` | `build_environment` | `setup_volumetric_fog` | write | write | none | `build_environment.setup_volumetric_fog` |
| `build_environment.spawn_light` | `build_environment` | `spawn_light` | write | write | none | `build_environment.spawn_light` |
| `build_environment.spawn_sky_light` | `build_environment` | `spawn_sky_light` | write | write | none | `build_environment.spawn_sky_light` |
| `control_actor.add_component` | `control_actor` | `add_component` | write | write | none | `control_actor.add_component` |
| `control_actor.add_tag` | `control_actor` | `add_tag` | write | write | none | `control_actor.add_tag` |
| `control_actor.apply_force` | `control_actor` | `apply_force` | write | write | none | `control_actor.apply_force` |
| `control_actor.apply_material` | `control_actor` | `apply_material` | write | write | none | `control_actor.apply_material` |
| `control_actor.attach` | `control_actor` | `attach` | write | write | none | `control_actor.attach` |
| `control_actor.attach_actor` | `control_actor` | `attach_actor` | write | write | none | `control_actor.attach_actor` |
| `control_actor.call_actor_function` | `control_actor` | `call_actor_function` | write | write | none | `control_actor.call_actor_function` |
| `control_actor.create_snapshot` | `control_actor` | `create_snapshot` | read | read | none | `control_actor.create_snapshot` |
| `control_actor.delete` | `control_actor` | `delete` | destructive | destructive | explicit | `control_actor.delete` |
| `control_actor.delete_by_tag` | `control_actor` | `delete_by_tag` | destructive | destructive | explicit | `control_actor.delete_by_tag` |
| `control_actor.destroy_actor` | `control_actor` | `destroy_actor` | destructive | destructive | explicit | `control_actor.destroy_actor` |
| `control_actor.detach` | `control_actor` | `detach` | write | write | none | `control_actor.detach` |
| `control_actor.detach_actor` | `control_actor` | `detach_actor` | write | write | none | `control_actor.detach_actor` |
| `control_actor.duplicate` | `control_actor` | `duplicate` | write | write | none | `control_actor.duplicate` |
| `control_actor.find_actors_by_class` | `control_actor` | `find_actors_by_class` | read | read | none | `control_actor.find_actors_by_class` |
| `control_actor.find_actors_by_name` | `control_actor` | `find_actors_by_name` | read | read | none | `control_actor.find_actors_by_name` |
| `control_actor.find_actors_by_tag` | `control_actor` | `find_actors_by_tag` | read | read | none | `control_actor.find_actors_by_tag` |
| `control_actor.find_by_class` | `control_actor` | `find_by_class` | read | read | none | `control_actor.find_by_class` |
| `control_actor.find_by_name` | `control_actor` | `find_by_name` | read | read | none | `control_actor.find_by_name` |
| `control_actor.find_by_tag` | `control_actor` | `find_by_tag` | read | read | none | `control_actor.find_by_tag` |
| `control_actor.get_actor_bounds` | `control_actor` | `get_actor_bounds` | read | read | none | `control_actor.get_actor_bounds` |
| `control_actor.get_actor_components` | `control_actor` | `get_actor_components` | read | read | none | `control_actor.get_actor_components` |
| `control_actor.get_actor_transform` | `control_actor` | `get_actor_transform` | read | read | none | `control_actor.get_actor_transform` |
| `control_actor.get_component_property` | `control_actor` | `get_component_property` | read | read | none | `control_actor.get_component_property` |
| `control_actor.get_components` | `control_actor` | `get_components` | read | read | none | `control_actor.get_components` |
| `control_actor.get_transform` | `control_actor` | `get_transform` | read | read | none | `control_actor.get_transform` |
| `control_actor.list` | `control_actor` | `list` | read | read | none | `control_actor.list` |
| `control_actor.remove_component` | `control_actor` | `remove_component` | write | write | none | `control_actor.remove_component` |
| `control_actor.remove_tag` | `control_actor` | `remove_tag` | write | write | none | `control_actor.remove_tag` |
| `control_actor.set_actor_collision` | `control_actor` | `set_actor_collision` | write | write | none | `control_actor.set_actor_collision` |
| `control_actor.set_actor_location` | `control_actor` | `set_actor_location` | write | write | none | `control_actor.set_actor_location` |
| `control_actor.set_actor_material` | `control_actor` | `set_actor_material` | write | write | none | `control_actor.set_actor_material` |
| `control_actor.set_actor_rotation` | `control_actor` | `set_actor_rotation` | write | write | none | `control_actor.set_actor_rotation` |
| `control_actor.set_actor_scale` | `control_actor` | `set_actor_scale` | write | write | none | `control_actor.set_actor_scale` |
| `control_actor.set_actor_transform` | `control_actor` | `set_actor_transform` | write | write | none | `control_actor.set_actor_transform` |
| `control_actor.set_actor_visible` | `control_actor` | `set_actor_visible` | write | write | none | `control_actor.set_actor_visible` |
| `control_actor.set_blueprint_variables` | `control_actor` | `set_blueprint_variables` | write | write | none | `control_actor.set_blueprint_variables` |
| `control_actor.set_component_properties` | `control_actor` | `set_component_properties` | write | write | none | `control_actor.set_component_properties` |
| `control_actor.set_component_property` | `control_actor` | `set_component_property` | write | write | none | `control_actor.set_component_property` |
| `control_actor.set_material` | `control_actor` | `set_material` | write | write | none | `control_actor.set_material` |
| `control_actor.set_transform` | `control_actor` | `set_transform` | write | write | none | `control_actor.set_transform` |
| `control_actor.set_visibility` | `control_actor` | `set_visibility` | write | write | none | `control_actor.set_visibility` |
| `control_actor.spawn` | `control_actor` | `spawn` | write | write | none | `control_actor.spawn` |
| `control_actor.spawn_actor` | `control_actor` | `spawn_actor` | write | write | none | `control_actor.spawn_actor` |
| `control_actor.spawn_blueprint` | `control_actor` | `spawn_blueprint` | write | write | none | `control_actor.spawn_blueprint` |
| `control_actor.teleport_actor` | `control_actor` | `teleport_actor` | write | write | none | `control_actor.teleport_actor` |
| `control_editor.close_asset` | `control_editor` | `close_asset` | write | write | none | `control_editor.close_asset` |
| `control_editor.console_command` | `control_editor` | `console_command` | write | write | none | `control_editor.console_command` |
| `control_editor.create_bookmark` | `control_editor` | `create_bookmark` | write | write | none | `control_editor.create_bookmark` |
| `control_editor.eject` | `control_editor` | `eject` | write | write | none | `control_editor.eject` |
| `control_editor.execute_command` | `control_editor` | `console_command` | write | write | none | `control_editor.execute_command` |
| `control_editor.focus_actor` | `control_editor` | `focus_actor` | read | read | none | `control_editor.focus_actor` |
| `control_editor.hide_stats` | `control_editor` | `hide_stats` | read | read | none | `control_editor.hide_stats` |
| `control_editor.jump_to_bookmark` | `control_editor` | `jump_to_bookmark` | read | read | none | `control_editor.jump_to_bookmark` |
| `control_editor.open_asset` | `control_editor` | `open_asset` | read | read | none | `control_editor.open_asset` |
| `control_editor.open_level` | `control_editor` | `open_level` | write | write | none | `control_editor.open_level` |
| `control_editor.pause` | `control_editor` | `pause` | write | write | none | `control_editor.pause` |
| `control_editor.play` | `control_editor` | `play` | write | write | none | `control_editor.play` |
| `control_editor.possess` | `control_editor` | `possess` | write | write | none | `control_editor.possess` |
| `control_editor.redo` | `control_editor` | `redo` | write | write | none | `control_editor.redo` |
| `control_editor.resume` | `control_editor` | `resume` | write | write | none | `control_editor.resume` |
| `control_editor.save_all` | `control_editor` | `save_all` | write | write | none | `control_editor.save_all` |
| `control_editor.screenshot` | `control_editor` | `screenshot` | read | read | none | `control_editor.screenshot` |
| `control_editor.set_camera` | `control_editor` | `set_camera` | read | read | none | `control_editor.set_camera` |
| `control_editor.set_camera_fov` | `control_editor` | `set_camera_fov` | read | read | none | `control_editor.set_camera_fov` |
| `control_editor.set_camera_position` | `control_editor` | `set_camera` | read | read | none | `control_editor.set_camera_position` |
| `control_editor.set_editor_mode` | `control_editor` | `set_editor_mode` | read | read | none | `control_editor.set_editor_mode` |
| `control_editor.set_fixed_delta_time` | `control_editor` | `set_fixed_delta_time` | write | write | none | `control_editor.set_fixed_delta_time` |
| `control_editor.set_game_speed` | `control_editor` | `set_game_speed` | write | write | none | `control_editor.set_game_speed` |
| `control_editor.set_game_view` | `control_editor` | `set_game_view` | read | read | none | `control_editor.set_game_view` |
| `control_editor.set_game_view_target` | `control_editor` | `set_view_target` | read | read | none | `control_editor.set_game_view_target` |
| `control_editor.set_immersive_mode` | `control_editor` | `set_immersive_mode` | read | read | none | `control_editor.set_immersive_mode` |
| `control_editor.set_preferences` | `control_editor` | `set_preferences` | write | write | none | `control_editor.set_preferences` |
| `control_editor.set_view_mode` | `control_editor` | `set_view_mode` | read | read | none | `control_editor.set_view_mode` |
| `control_editor.set_view_target` | `control_editor` | `set_view_target` | read | read | none | `control_editor.set_view_target` |
| `control_editor.set_viewport_camera` | `control_editor` | `set_camera` | read | read | none | `control_editor.set_viewport_camera` |
| `control_editor.set_viewport_realtime` | `control_editor` | `set_viewport_realtime` | read | read | none | `control_editor.set_viewport_realtime` |
| `control_editor.set_viewport_resolution` | `control_editor` | `console_command` | read | read | none | `control_editor.set_viewport_resolution` |
| `control_editor.show_stats` | `control_editor` | `show_stats` | read | read | none | `control_editor.show_stats` |
| `control_editor.simulate_input` | `control_editor` | `simulate_input` | write | write | none | `control_editor.simulate_input` |
| `control_editor.single_frame_step` | `control_editor` | `step_frame` | write | write | none | `control_editor.single_frame_step` |
| `control_editor.start_recording` | `control_editor` | `start_recording` | write | write | none | `control_editor.start_recording` |
| `control_editor.step_frame` | `control_editor` | `step_frame` | write | write | none | `control_editor.step_frame` |
| `control_editor.stop` | `control_editor` | `stop` | write | write | none | `control_editor.stop` |
| `control_editor.stop_pie` | `control_editor` | `stop` | write | write | none | `control_editor.stop_pie` |
| `control_editor.stop_recording` | `control_editor` | `stop_recording` | write | write | none | `control_editor.stop_recording` |
| `control_editor.take_screenshot` | `control_editor` | `screenshot` | read | read | none | `control_editor.take_screenshot` |
| `control_editor.undo` | `control_editor` | `undo` | write | write | none | `control_editor.undo` |
| `datatable.add_data_table_row` | `manage_asset` | `add_data_table_row` | write | write | explicit | `manage_asset.add_data_table_row` |
| `datatable.clear_data_table_rows` | `manage_asset` | `clear_data_table_rows` | destructive | destructive | elevated | `manage_asset.clear_data_table_rows` |
| `datatable.create_data_table` | `manage_asset` | `create_data_table` | write | write | explicit | `manage_asset.create_data_table` |
| `datatable.create_row_struct` | `manage_asset` | `create_row_struct` | write | write | explicit | `manage_asset.create_row_struct` |
| `datatable.delete_data_table_row` | `manage_asset` | `delete_data_table_row` | destructive | destructive | elevated | `manage_asset.delete_data_table_row` |
| `datatable.get_data_table_row` | `manage_asset` | `get_data_table_row` | read | read | none | `manage_asset.get_data_table_row` |
| `datatable.get_row_struct` | `manage_asset` | `get_row_struct` | read | read | none | `manage_asset.get_row_struct` |
| `datatable.import_data_table_rows` | `manage_asset` | `import_data_table_rows` | write | write | explicit | `manage_asset.import_data_table_rows` |
| `datatable.list_data_table_rows` | `manage_asset` | `list_data_table_rows` | read | read | none | `manage_asset.list_data_table_rows` |
| `datatable.set_data_table_row_struct` | `manage_asset` | `set_data_table_row_struct` | write | write | explicit | `manage_asset.set_data_table_row_struct` |
| `datatable.set_struct_as_row_struct` | `manage_asset` | `set_struct_as_row_struct` | write | write | explicit | `manage_asset.set_struct_as_row_struct` |
| `datatable.update_data_table_row` | `manage_asset` | `update_data_table_row` | write | write | explicit | `manage_asset.update_data_table_row` |
| `enum.add_enum_value` | `manage_asset` | `add_enum_value` | write | write | explicit | `manage_asset.add_enum_value` |
| `enum.create_enum` | `manage_asset` | `create_enum` | write | write | explicit | `manage_asset.create_enum` |
| `enum.delete_enum` | `manage_asset` | `delete_enum` | destructive | destructive | elevated | `manage_asset.delete_enum` |
| `enum.get_enum` | `manage_asset` | `get_enum` | read | read | none | `manage_asset.get_enum` |
| `enum.remove_enum_value` | `manage_asset` | `remove_enum_value` | destructive | destructive | elevated | `manage_asset.remove_enum_value` |
| `enum.rename_enum_value` | `manage_asset` | `rename_enum_value` | write | write | explicit | `manage_asset.rename_enum_value` |
| `enum.reorder_enum_values` | `manage_asset` | `reorder_enum_values` | write | write | explicit | `manage_asset.reorder_enum_values` |
| `enum.set_enum_value_metadata` | `manage_asset` | `set_enum_value_metadata` | write | write | explicit | `manage_asset.set_enum_value_metadata` |
| `enum.split_enum` | `manage_asset` | `split_enum` | write | write | explicit | `manage_asset.split_enum` |
| `inspect.add_tag` | `inspect` | `control_actor` | write | write | none | `inspect.add_tag` |
| `inspect.create_snapshot` | `inspect` | `control_actor` | write | write | none | `inspect.create_snapshot` |
| `inspect.delete_object` | `inspect` | `control_actor` | destructive | destructive | explicit | `inspect.delete_object` |
| `inspect.export` | `inspect` | `control_actor` | read | read | none | `inspect.export` |
| `inspect.find_by_class` | `inspect` | `find_by_class` | read | read | none | `inspect.find_by_class` |
| `inspect.find_by_tag` | `inspect` | `control_actor` | read | read | none | `inspect.find_by_tag` |
| `inspect.get_actor_details` | `inspect` | `inspect_object` | read | read | none | `inspect.get_actor_details` |
| `inspect.get_blueprint_details` | `inspect` | `blueprint_get` | read | read | none | `inspect.get_blueprint_details` |
| `inspect.get_bounding_box` | `inspect` | `control_actor` | read | read | none | `inspect.get_bounding_box` |
| `inspect.get_component_details` | `inspect` | `control_actor` | read | read | none | `inspect.get_component_details` |
| `inspect.get_component_property` | `inspect` | `control_actor` | read | read | none | `inspect.get_component_property` |
| `inspect.get_components` | `inspect` | `get_components` | read | read | none | `inspect.get_components` |
| `inspect.get_editor_settings` | `inspect` | `get_editor_settings` | read | read | none | `inspect.get_editor_settings` |
| `inspect.get_level_details` | `inspect` | `get_world_settings` | read | read | none | `inspect.get_level_details` |
| `inspect.get_material_details` | `inspect` | `inspect_object` | read | read | none | `inspect.get_material_details` |
| `inspect.get_memory_stats` | `inspect` | `get_memory_stats` | read | read | none | `inspect.get_memory_stats` |
| `inspect.get_mesh_details` | `inspect` | `inspect_object` | read | read | none | `inspect.get_mesh_details` |
| `inspect.get_metadata` | `inspect` | `control_actor` | read | read | none | `inspect.get_metadata` |
| `inspect.get_performance_stats` | `inspect` | `get_performance_stats` | read | read | none | `inspect.get_performance_stats` |
| `inspect.get_project_settings` | `inspect` | `get_project_settings` | read | read | none | `inspect.get_project_settings` |
| `inspect.get_property` | `inspect` | `get_property` | read | read | none | `inspect.get_property` |
| `inspect.get_scene_stats` | `inspect` | `get_scene_stats` | read | read | none | `inspect.get_scene_stats` |
| `inspect.get_selected_actors` | `inspect` | `get_selected_actors` | read | read | none | `inspect.get_selected_actors` |
| `inspect.get_texture_details` | `inspect` | `inspect_object` | read | read | none | `inspect.get_texture_details` |
| `inspect.get_viewport_info` | `inspect` | `get_viewport_info` | read | read | none | `inspect.get_viewport_info` |
| `inspect.get_world_settings` | `inspect` | `get_world_settings` | read | read | none | `inspect.get_world_settings` |
| `inspect.inspect_cdo` | `inspect` | `inspect_cdo` | read | read | none | `inspect.inspect_cdo` |
| `inspect.inspect_class` | `inspect` | `inspect_class` | read | read | none | `inspect.inspect_class` |
| `inspect.inspect_object` | `inspect` | `inspect_object` | read | read | none | `inspect.inspect_object` |
| `inspect.inspect_struct` | `inspect` | `inspect_struct` | read | read | none | `inspect.inspect_struct` |
| `inspect.list_objects` | `inspect` | `control_actor` | read | read | none | `inspect.list_objects` |
| `inspect.pie_report` | `inspect` | `pie_report` | read | read | none | `inspect.pie_report` |
| `inspect.restore_snapshot` | `inspect` | `control_actor` | write | write | none | `inspect.restore_snapshot` |
| `inspect.runtime_report` | `inspect` | `runtime_report` | read | read | none | `inspect.runtime_report` |
| `inspect.set_component_property` | `inspect` | `control_actor` | write | write | none | `inspect.set_component_property` |
| `inspect.set_property` | `inspect` | `set_property` | write | write | none | `inspect.set_property` |
| `manage_ai.add_ai_perception_component` | `manage_ai` | `add_ai_perception_component` | write | write | none | `manage_ai.add_ai_perception_component` |
| `manage_ai.add_blackboard_key` | `manage_ai` | `add_blackboard_key` | write | write | none | `manage_ai.add_blackboard_key` |
| `manage_ai.add_composite_node` | `manage_ai` | `add_composite_node` | write | write | none | `manage_ai.add_composite_node` |
| `manage_ai.add_decorator` | `manage_ai` | `add_decorator` | write | write | none | `manage_ai.add_decorator` |
| `manage_ai.add_eqs_context` | `manage_ai` | `add_eqs_context` | write | write | none | `manage_ai.add_eqs_context` |
| `manage_ai.add_eqs_generator` | `manage_ai` | `add_eqs_generator` | write | write | none | `manage_ai.add_eqs_generator` |
| `manage_ai.add_eqs_test` | `manage_ai` | `add_eqs_test` | write | write | none | `manage_ai.add_eqs_test` |
| `manage_ai.add_mass_spawner` | `manage_ai` | `add_mass_spawner` | write | write | none | `manage_ai.add_mass_spawner` |
| `manage_ai.add_node` | `manage_ai` | `add_node` | write | write | none | `manage_ai.add_node` |
| `manage_ai.add_service` | `manage_ai` | `add_service` | write | write | none | `manage_ai.add_service` |
| `manage_ai.add_smart_object_component` | `manage_ai` | `add_smart_object_component` | write | write | none | `manage_ai.add_smart_object_component` |
| `manage_ai.add_smart_object_slot` | `manage_ai` | `add_smart_object_slot` | write | write | none | `manage_ai.add_smart_object_slot` |
| `manage_ai.add_state_tree_state` | `manage_ai` | `add_state_tree_state` | write | write | none | `manage_ai.add_state_tree_state` |
| `manage_ai.add_state_tree_transition` | `manage_ai` | `add_state_tree_transition` | write | write | none | `manage_ai.add_state_tree_transition` |
| `manage_ai.add_subnode` | `manage_ai` | `add_subnode` | write | write | none | `manage_ai.add_subnode` |
| `manage_ai.add_task_node` | `manage_ai` | `add_task_node` | write | write | none | `manage_ai.add_task_node` |
| `manage_ai.assign_behavior_tree` | `manage_ai` | `assign_behavior_tree` | write | write | none | `manage_ai.assign_behavior_tree` |
| `manage_ai.assign_blackboard` | `manage_ai` | `assign_blackboard` | write | write | none | `manage_ai.assign_blackboard` |
| `manage_ai.break_connections` | `manage_ai` | `break_connections` | write | write | none | `manage_ai.break_connections` |
| `manage_ai.clear_focus` | `manage_ai` | `clear_focus` | write | write | none | `manage_ai.clear_focus` |
| `manage_ai.configure_bt_node` | `manage_ai` | `configure_bt_node` | write | write | none | `manage_ai.configure_bt_node` |
| `manage_ai.configure_damage_sense_config` | `manage_ai` | `configure_damage_sense_config` | write | write | none | `manage_ai.configure_damage_sense_config` |
| `manage_ai.configure_hearing_config` | `manage_ai` | `configure_hearing_config` | write | write | none | `manage_ai.configure_hearing_config` |
| `manage_ai.configure_mass_entity` | `manage_ai` | `configure_mass_entity` | write | write | none | `manage_ai.configure_mass_entity` |
| `manage_ai.configure_nav_area_cost` | `manage_ai` | `configure_nav_area_cost` | write | write | none | `manage_ai.configure_nav_area_cost` |
| `manage_ai.configure_nav_link` | `manage_ai` | `configure_nav_link` | write | write | none | `manage_ai.configure_nav_link` |
| `manage_ai.configure_nav_mesh_settings` | `manage_ai` | `configure_nav_mesh_settings` | write | write | none | `manage_ai.configure_nav_mesh_settings` |
| `manage_ai.configure_sight_config` | `manage_ai` | `configure_sight_config` | write | write | none | `manage_ai.configure_sight_config` |
| `manage_ai.configure_slot_behavior` | `manage_ai` | `configure_slot_behavior` | write | write | none | `manage_ai.configure_slot_behavior` |
| `manage_ai.configure_smart_link_behavior` | `manage_ai` | `configure_smart_link_behavior` | write | write | none | `manage_ai.configure_smart_link_behavior` |
| `manage_ai.configure_state_tree_task` | `manage_ai` | `configure_state_tree_task` | write | write | none | `manage_ai.configure_state_tree_task` |
| `manage_ai.configure_test_scoring` | `manage_ai` | `configure_test_scoring` | write | write | none | `manage_ai.configure_test_scoring` |
| `manage_ai.connect_nodes` | `manage_ai` | `connect_nodes` | write | write | none | `manage_ai.connect_nodes` |
| `manage_ai.create` | `manage_ai` | `create` | write | write | none | `manage_ai.create` |
| `manage_ai.create_ai_controller` | `manage_ai` | `create_ai_controller` | write | write | none | `manage_ai.create_ai_controller` |
| `manage_ai.create_behavior_tree` | `manage_ai` | `create_behavior_tree` | write | write | none | `manage_ai.create_behavior_tree` |
| `manage_ai.create_blackboard` | `manage_ai` | `create_blackboard` | write | write | none | `manage_ai.create_blackboard` |
| `manage_ai.create_blackboard_asset` | `manage_ai` | `create_blackboard_asset` | write | write | none | `manage_ai.create_blackboard_asset` |
| `manage_ai.create_eqs_query` | `manage_ai` | `create_eqs_query` | write | write | none | `manage_ai.create_eqs_query` |
| `manage_ai.create_mass_entity_config` | `manage_ai` | `create_mass_entity_config` | write | write | none | `manage_ai.create_mass_entity_config` |
| `manage_ai.create_nav_link_proxy` | `manage_ai` | `create_nav_link_proxy` | write | write | none | `manage_ai.create_nav_link_proxy` |
| `manage_ai.create_nav_modifier_component` | `manage_ai` | `create_nav_modifier_component` | write | write | none | `manage_ai.create_nav_modifier_component` |
| `manage_ai.create_smart_link` | `manage_ai` | `create_smart_link` | write | write | none | `manage_ai.create_smart_link` |
| `manage_ai.create_smart_object_definition` | `manage_ai` | `create_smart_object_definition` | write | write | none | `manage_ai.create_smart_object_definition` |
| `manage_ai.create_state_tree` | `manage_ai` | `create_state_tree` | write | write | none | `manage_ai.create_state_tree` |
| `manage_ai.get_ai_info` | `manage_ai` | `get_ai_info` | read | read | none | `manage_ai.get_ai_info` |
| `manage_ai.get_blackboard_value` | `manage_ai` | `get_blackboard_value` | read | read | none | `manage_ai.get_blackboard_value` |
| `manage_ai.get_navigation_info` | `manage_ai` | `get_navigation_info` | read | read | none | `manage_ai.get_navigation_info` |
| `manage_ai.get_tree` | `manage_ai` | `get_tree` | read | read | none | `manage_ai.get_tree` |
| `manage_ai.rebuild_navigation` | `manage_ai` | `rebuild_navigation` | write | write | none | `manage_ai.rebuild_navigation` |
| `manage_ai.remove_node` | `manage_ai` | `remove_node` | write | write | none | `manage_ai.remove_node` |
| `manage_ai.run_behavior_tree` | `manage_ai` | `run_behavior_tree` | write | write | none | `manage_ai.run_behavior_tree` |
| `manage_ai.set_blackboard_value` | `manage_ai` | `set_blackboard_value` | write | write | none | `manage_ai.set_blackboard_value` |
| `manage_ai.set_focus` | `manage_ai` | `set_focus` | write | write | none | `manage_ai.set_focus` |
| `manage_ai.set_key_instance_synced` | `manage_ai` | `set_key_instance_synced` | write | write | none | `manage_ai.set_key_instance_synced` |
| `manage_ai.set_nav_agent_properties` | `manage_ai` | `set_nav_agent_properties` | write | write | none | `manage_ai.set_nav_agent_properties` |
| `manage_ai.set_nav_area_class` | `manage_ai` | `set_nav_area_class` | write | write | none | `manage_ai.set_nav_area_class` |
| `manage_ai.set_nav_link_type` | `manage_ai` | `set_nav_link_type` | write | write | none | `manage_ai.set_nav_link_type` |
| `manage_ai.set_node_properties` | `manage_ai` | `set_node_properties` | write | write | none | `manage_ai.set_node_properties` |
| `manage_ai.set_perception_team` | `manage_ai` | `set_perception_team` | write | write | none | `manage_ai.set_perception_team` |
| `manage_ai.setup_perception` | `manage_ai` | `setup_perception` | write | write | none | `manage_ai.setup_perception` |
| `manage_ai.stop_behavior_tree` | `manage_ai` | `stop_behavior_tree` | write | write | none | `manage_ai.stop_behavior_tree` |
| `manage_audio.add_cue_node` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.add_cue_node` |
| `manage_audio.add_metasound_input` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.add_metasound_input` |
| `manage_audio.add_metasound_node` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.add_metasound_node` |
| `manage_audio.add_metasound_output` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.add_metasound_output` |
| `manage_audio.add_mix_modifier` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.add_mix_modifier` |
| `manage_audio.add_source_effect` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.add_source_effect` |
| `manage_audio.clear_sound_mix_class_override` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.clear_sound_mix_class_override` |
| `manage_audio.configure_distance_attenuation` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.configure_distance_attenuation` |
| `manage_audio.configure_mix_eq` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.configure_mix_eq` |
| `manage_audio.configure_occlusion` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.configure_occlusion` |
| `manage_audio.configure_reverb_send` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.configure_reverb_send` |
| `manage_audio.configure_spatialization` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.configure_spatialization` |
| `manage_audio.connect_cue_nodes` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.connect_cue_nodes` |
| `manage_audio.connect_metasound_nodes` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.connect_metasound_nodes` |
| `manage_audio.create_ambient_sound` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_ambient_sound` |
| `manage_audio.create_attenuation_settings` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_attenuation_settings` |
| `manage_audio.create_audio_component` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_audio_component` |
| `manage_audio.create_dialogue_voice` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_dialogue_voice` |
| `manage_audio.create_dialogue_wave` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_dialogue_wave` |
| `manage_audio.create_metasound` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_metasound` |
| `manage_audio.create_reverb_effect` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_reverb_effect` |
| `manage_audio.create_reverb_zone` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_reverb_zone` |
| `manage_audio.create_sound_class` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_sound_class` |
| `manage_audio.create_sound_cue` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_sound_cue` |
| `manage_audio.create_sound_mix` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_sound_mix` |
| `manage_audio.create_source_effect_chain` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_source_effect_chain` |
| `manage_audio.create_submix_effect` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.create_submix_effect` |
| `manage_audio.enable_audio_analysis` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.enable_audio_analysis` |
| `manage_audio.fade_sound` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.fade_sound` |
| `manage_audio.fade_sound_in` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.fade_sound_in` |
| `manage_audio.fade_sound_out` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.fade_sound_out` |
| `manage_audio.get_audio_info` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.get_audio_info` |
| `manage_audio.play_sound_2d` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.play_sound_2d` |
| `manage_audio.play_sound_at_location` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.play_sound_at_location` |
| `manage_audio.play_sound_attached` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.play_sound_attached` |
| `manage_audio.pop_sound_mix` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.pop_sound_mix` |
| `manage_audio.prime_sound` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.prime_sound` |
| `manage_audio.push_sound_mix` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.push_sound_mix` |
| `manage_audio.set_audio_occlusion` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.set_audio_occlusion` |
| `manage_audio.set_base_sound_mix` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.set_base_sound_mix` |
| `manage_audio.set_class_parent` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.set_class_parent` |
| `manage_audio.set_class_properties` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.set_class_properties` |
| `manage_audio.set_cue_attenuation` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.set_cue_attenuation` |
| `manage_audio.set_cue_concurrency` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.set_cue_concurrency` |
| `manage_audio.set_dialogue_context` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.set_dialogue_context` |
| `manage_audio.set_doppler_effect` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.set_doppler_effect` |
| `manage_audio.set_metasound_default` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.set_metasound_default` |
| `manage_audio.set_sound_attenuation` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.set_sound_attenuation` |
| `manage_audio.set_sound_mix_class_override` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.set_sound_mix_class_override` |
| `manage_audio.spawn_sound_at_location` | `manage_audio` | `manage_audio` | write | write | none | `manage_audio.spawn_sound_at_location` |
| `manage_character.add_custom_movement_mode` | `manage_character` | `add_custom_movement_mode` | write | write | none | `manage_character.add_custom_movement_mode` |
| `manage_character.configure_camera_component` | `manage_character` | `configure_camera_component` | write | write | none | `manage_character.configure_camera_component` |
| `manage_character.configure_capsule_component` | `manage_character` | `configure_capsule_component` | write | write | none | `manage_character.configure_capsule_component` |
| `manage_character.configure_crouch` | `manage_character` | `configure_crouch` | write | write | none | `manage_character.configure_crouch` |
| `manage_character.configure_footstep_fx` | `manage_character` | `configure_footstep_fx` | write | write | none | `manage_character.configure_footstep_fx` |
| `manage_character.configure_jump` | `manage_character` | `configure_jump` | write | write | none | `manage_character.configure_jump` |
| `manage_character.configure_mesh_component` | `manage_character` | `configure_mesh_component` | write | write | none | `manage_character.configure_mesh_component` |
| `manage_character.configure_movement_speeds` | `manage_character` | `configure_movement_speeds` | write | write | none | `manage_character.configure_movement_speeds` |
| `manage_character.configure_nav_movement` | `manage_character` | `configure_nav_movement` | write | write | none | `manage_character.configure_nav_movement` |
| `manage_character.configure_rotation` | `manage_character` | `configure_rotation` | write | write | none | `manage_character.configure_rotation` |
| `manage_character.configure_sprint` | `manage_character` | `configure_sprint` | write | write | none | `manage_character.configure_sprint` |
| `manage_character.create_character_blueprint` | `manage_character` | `create_character_blueprint` | write | write | none | `manage_character.create_character_blueprint` |
| `manage_character.get_character_info` | `manage_character` | `get_character_info` | read | read | none | `manage_character.get_character_info` |
| `manage_character.map_surface_to_sound` | `manage_character` | `map_surface_to_sound` | write | write | none | `manage_character.map_surface_to_sound` |
| `manage_character.set_braking_deceleration` | `manage_character` | `set_braking_deceleration` | write | write | none | `manage_character.set_braking_deceleration` |
| `manage_character.set_gravity_scale` | `manage_character` | `set_gravity_scale` | write | write | none | `manage_character.set_gravity_scale` |
| `manage_character.set_ground_friction` | `manage_character` | `set_ground_friction` | write | write | none | `manage_character.set_ground_friction` |
| `manage_character.set_jump_height` | `manage_character` | `set_jump_height` | write | write | none | `manage_character.set_jump_height` |
| `manage_character.set_walk_speed` | `manage_character` | `set_walk_speed` | write | write | none | `manage_character.set_walk_speed` |
| `manage_character.setup_climbing` | `manage_character` | `setup_climbing` | write | write | none | `manage_character.setup_climbing` |
| `manage_character.setup_footstep_system` | `manage_character` | `setup_footstep_system` | write | write | none | `manage_character.setup_footstep_system` |
| `manage_character.setup_grappling` | `manage_character` | `setup_grappling` | write | write | none | `manage_character.setup_grappling` |
| `manage_character.setup_mantling` | `manage_character` | `setup_mantling` | write | write | none | `manage_character.setup_mantling` |
| `manage_character.setup_movement` | `manage_character` | `setup_movement` | write | write | none | `manage_character.setup_movement` |
| `manage_character.setup_sliding` | `manage_character` | `setup_sliding` | write | write | none | `manage_character.setup_sliding` |
| `manage_character.setup_vaulting` | `manage_character` | `setup_vaulting` | write | write | none | `manage_character.setup_vaulting` |
| `manage_character.setup_wall_running` | `manage_character` | `setup_wall_running` | write | write | none | `manage_character.setup_wall_running` |
| `manage_combat.apply_damage` | `manage_combat` | `apply_damage` | write | write | none | `manage_combat.apply_damage` |
| `manage_combat.configure_aim_down_sights` | `manage_combat` | `configure_aim_down_sights` | write | write | none | `manage_combat.configure_aim_down_sights` |
| `manage_combat.configure_combo_system` | `manage_combat` | `configure_combo_system` | write | write | none | `manage_combat.configure_combo_system` |
| `manage_combat.configure_damage_execution` | `manage_combat` | `configure_damage_execution` | write | write | none | `manage_combat.configure_damage_execution` |
| `manage_combat.configure_hit_detection` | `manage_combat` | `configure_hit_detection` | write | write | none | `manage_combat.configure_hit_detection` |
| `manage_combat.configure_hit_reaction` | `manage_combat` | `configure_hit_reaction` | write | write | none | `manage_combat.configure_hit_reaction` |
| `manage_combat.configure_hitscan` | `manage_combat` | `configure_hitscan` | write | write | none | `manage_combat.configure_hitscan` |
| `manage_combat.configure_impact_effects` | `manage_combat` | `configure_impact_effects` | write | write | none | `manage_combat.configure_impact_effects` |
| `manage_combat.configure_muzzle_flash` | `manage_combat` | `configure_muzzle_flash` | write | write | none | `manage_combat.configure_muzzle_flash` |
| `manage_combat.configure_projectile` | `manage_combat` | `configure_projectile` | write | write | none | `manage_combat.configure_projectile` |
| `manage_combat.configure_projectile_collision` | `manage_combat` | `configure_projectile_collision` | write | write | none | `manage_combat.configure_projectile_collision` |
| `manage_combat.configure_projectile_homing` | `manage_combat` | `configure_projectile_homing` | write | write | none | `manage_combat.configure_projectile_homing` |
| `manage_combat.configure_projectile_movement` | `manage_combat` | `configure_projectile_movement` | write | write | none | `manage_combat.configure_projectile_movement` |
| `manage_combat.configure_recoil_pattern` | `manage_combat` | `configure_recoil_pattern` | write | write | none | `manage_combat.configure_recoil_pattern` |
| `manage_combat.configure_shell_ejection` | `manage_combat` | `configure_shell_ejection` | write | write | none | `manage_combat.configure_shell_ejection` |
| `manage_combat.configure_spread_pattern` | `manage_combat` | `configure_spread_pattern` | write | write | none | `manage_combat.configure_spread_pattern` |
| `manage_combat.configure_tracer` | `manage_combat` | `configure_tracer` | write | write | none | `manage_combat.configure_tracer` |
| `manage_combat.configure_weapon_mesh` | `manage_combat` | `configure_weapon_mesh` | write | write | none | `manage_combat.configure_weapon_mesh` |
| `manage_combat.configure_weapon_sockets` | `manage_combat` | `configure_weapon_sockets` | write | write | none | `manage_combat.configure_weapon_sockets` |
| `manage_combat.configure_weapon_trails` | `manage_combat` | `configure_weapon_trails` | write | write | none | `manage_combat.configure_weapon_trails` |
| `manage_combat.create_damage_effect` | `manage_combat` | `create_damage_effect` | write | write | none | `manage_combat.create_damage_effect` |
| `manage_combat.create_damage_type` | `manage_combat` | `create_damage_type` | write | write | none | `manage_combat.create_damage_type` |
| `manage_combat.create_hit_pause` | `manage_combat` | `create_hit_pause` | write | write | none | `manage_combat.create_hit_pause` |
| `manage_combat.create_melee_trace` | `manage_combat` | `create_melee_trace` | write | write | none | `manage_combat.create_melee_trace` |
| `manage_combat.create_projectile_blueprint` | `manage_combat` | `create_projectile_blueprint` | write | write | none | `manage_combat.create_projectile_blueprint` |
| `manage_combat.create_shield` | `manage_combat` | `create_shield` | write | write | none | `manage_combat.create_shield` |
| `manage_combat.create_weapon_blueprint` | `manage_combat` | `create_weapon_blueprint` | write | write | none | `manage_combat.create_weapon_blueprint` |
| `manage_combat.get_combat_info` | `manage_combat` | `get_combat_info` | read | read | none | `manage_combat.get_combat_info` |
| `manage_combat.get_combat_stats` | `manage_combat` | `get_combat_stats` | read | read | none | `manage_combat.get_combat_stats` |
| `manage_combat.heal` | `manage_combat` | `heal` | write | write | none | `manage_combat.heal` |
| `manage_combat.modify_armor` | `manage_combat` | `modify_armor` | write | write | none | `manage_combat.modify_armor` |
| `manage_combat.set_weapon_stats` | `manage_combat` | `set_weapon_stats` | write | write | none | `manage_combat.set_weapon_stats` |
| `manage_combat.setup_ammo_system` | `manage_combat` | `setup_ammo_system` | write | write | none | `manage_combat.setup_ammo_system` |
| `manage_combat.setup_attachment_system` | `manage_combat` | `setup_attachment_system` | write | write | none | `manage_combat.setup_attachment_system` |
| `manage_combat.setup_damage_type` | `manage_combat` | `setup_damage_type` | write | write | none | `manage_combat.setup_damage_type` |
| `manage_combat.setup_hitbox_component` | `manage_combat` | `setup_hitbox_component` | write | write | none | `manage_combat.setup_hitbox_component` |
| `manage_combat.setup_parry_block_system` | `manage_combat` | `setup_parry_block_system` | write | write | none | `manage_combat.setup_parry_block_system` |
| `manage_combat.setup_reload_system` | `manage_combat` | `setup_reload_system` | write | write | none | `manage_combat.setup_reload_system` |
| `manage_combat.setup_weapon_switching` | `manage_combat` | `setup_weapon_switching` | write | write | none | `manage_combat.setup_weapon_switching` |
| `manage_effect.activate` | `manage_effect` | `activate` | write | write | none | `manage_effect.activate` |
| `manage_effect.activate_effect` | `manage_effect` | `activate_effect` | write | write | none | `manage_effect.activate_effect` |
| `manage_effect.add_acceleration_module` | `manage_effect` | `add_acceleration_module` | write | write | none | `manage_effect.add_acceleration_module` |
| `manage_effect.add_audio_spectrum_data_interface` | `manage_effect` | `add_audio_spectrum_data_interface` | write | write | none | `manage_effect.add_audio_spectrum_data_interface` |
| `manage_effect.add_camera_offset_module` | `manage_effect` | `add_camera_offset_module` | write | write | none | `manage_effect.add_camera_offset_module` |
| `manage_effect.add_collision_module` | `manage_effect` | `add_collision_module` | write | write | none | `manage_effect.add_collision_module` |
| `manage_effect.add_collision_query_data_interface` | `manage_effect` | `add_collision_query_data_interface` | write | write | none | `manage_effect.add_collision_query_data_interface` |
| `manage_effect.add_color_module` | `manage_effect` | `add_color_module` | write | write | none | `manage_effect.add_color_module` |
| `manage_effect.add_emitter_to_system` | `manage_effect` | `add_emitter_to_system` | write | write | none | `manage_effect.add_emitter_to_system` |
| `manage_effect.add_event_generator` | `manage_effect` | `add_event_generator` | write | write | none | `manage_effect.add_event_generator` |
| `manage_effect.add_event_receiver` | `manage_effect` | `add_event_receiver` | write | write | none | `manage_effect.add_event_receiver` |
| `manage_effect.add_force_module` | `manage_effect` | `add_force_module` | write | write | none | `manage_effect.add_force_module` |
| `manage_effect.add_initialize_particle_module` | `manage_effect` | `add_initialize_particle_module` | write | write | none | `manage_effect.add_initialize_particle_module` |
| `manage_effect.add_kill_particles_module` | `manage_effect` | `add_kill_particles_module` | write | write | none | `manage_effect.add_kill_particles_module` |
| `manage_effect.add_light_renderer_module` | `manage_effect` | `add_light_renderer_module` | write | write | none | `manage_effect.add_light_renderer_module` |
| `manage_effect.add_mesh_renderer_module` | `manage_effect` | `add_mesh_renderer_module` | write | write | none | `manage_effect.add_mesh_renderer_module` |
| `manage_effect.add_niagara_module` | `manage_effect` | `add_niagara_module` | write | write | none | `manage_effect.add_niagara_module` |
| `manage_effect.add_particle_state_module` | `manage_effect` | `add_particle_state_module` | write | write | none | `manage_effect.add_particle_state_module` |
| `manage_effect.add_ribbon_renderer_module` | `manage_effect` | `add_ribbon_renderer_module` | write | write | none | `manage_effect.add_ribbon_renderer_module` |
| `manage_effect.add_simulation_stage` | `manage_effect` | `add_simulation_stage` | write | write | none | `manage_effect.add_simulation_stage` |
| `manage_effect.add_size_module` | `manage_effect` | `add_size_module` | write | write | none | `manage_effect.add_size_module` |
| `manage_effect.add_skeletal_mesh_data_interface` | `manage_effect` | `add_skeletal_mesh_data_interface` | write | write | none | `manage_effect.add_skeletal_mesh_data_interface` |
| `manage_effect.add_spawn_burst_module` | `manage_effect` | `add_spawn_burst_module` | write | write | none | `manage_effect.add_spawn_burst_module` |
| `manage_effect.add_spawn_per_unit_module` | `manage_effect` | `add_spawn_per_unit_module` | write | write | none | `manage_effect.add_spawn_per_unit_module` |
| `manage_effect.add_spawn_rate_module` | `manage_effect` | `add_spawn_rate_module` | write | write | none | `manage_effect.add_spawn_rate_module` |
| `manage_effect.add_spline_data_interface` | `manage_effect` | `add_spline_data_interface` | write | write | none | `manage_effect.add_spline_data_interface` |
| `manage_effect.add_sprite_renderer_module` | `manage_effect` | `add_sprite_renderer_module` | write | write | none | `manage_effect.add_sprite_renderer_module` |
| `manage_effect.add_static_mesh_data_interface` | `manage_effect` | `add_static_mesh_data_interface` | write | write | none | `manage_effect.add_static_mesh_data_interface` |
| `manage_effect.add_user_parameter` | `manage_effect` | `add_user_parameter` | write | write | none | `manage_effect.add_user_parameter` |
| `manage_effect.add_velocity_module` | `manage_effect` | `add_velocity_module` | write | write | none | `manage_effect.add_velocity_module` |
| `manage_effect.advance_simulation` | `manage_effect` | `advance_simulation` | write | write | none | `manage_effect.advance_simulation` |
| `manage_effect.bind_parameter_to_source` | `manage_effect` | `bind_parameter_to_source` | write | write | none | `manage_effect.bind_parameter_to_source` |
| `manage_effect.cleanup` | `manage_effect` | `cleanup` | write | write | none | `manage_effect.cleanup` |
| `manage_effect.clear_debug_shapes` | `manage_effect` | `clear_debug_shapes` | write | write | none | `manage_effect.clear_debug_shapes` |
| `manage_effect.configure_event_payload` | `manage_effect` | `configure_event_payload` | write | write | none | `manage_effect.configure_event_payload` |
| `manage_effect.connect_niagara_pins` | `manage_effect` | `connect_niagara_pins` | write | write | none | `manage_effect.connect_niagara_pins` |
| `manage_effect.create_dynamic_light` | `manage_effect` | `create_dynamic_light` | write | write | none | `manage_effect.create_dynamic_light` |
| `manage_effect.create_environment_effect` | `manage_effect` | `create_environment_effect` | write | write | none | `manage_effect.create_environment_effect` |
| `manage_effect.create_impact_effect` | `manage_effect` | `create_impact_effect` | write | write | none | `manage_effect.create_impact_effect` |
| `manage_effect.create_niagara_emitter` | `manage_effect` | `create_niagara_emitter` | write | write | none | `manage_effect.create_niagara_emitter` |
| `manage_effect.create_niagara_ribbon` | `manage_effect` | `create_niagara_ribbon` | write | write | none | `manage_effect.create_niagara_ribbon` |
| `manage_effect.create_niagara_system` | `manage_effect` | `create_niagara_system` | write | write | none | `manage_effect.create_niagara_system` |
| `manage_effect.create_particle_trail` | `manage_effect` | `create_particle_trail` | write | write | none | `manage_effect.create_particle_trail` |
| `manage_effect.create_volumetric_fog` | `manage_effect` | `create_volumetric_fog` | write | write | none | `manage_effect.create_volumetric_fog` |
| `manage_effect.deactivate` | `manage_effect` | `deactivate` | write | write | none | `manage_effect.deactivate` |
| `manage_effect.debug_shape` | `manage_effect` | `debug_shape` | write | write | none | `manage_effect.debug_shape` |
| `manage_effect.enable_gpu_simulation` | `manage_effect` | `enable_gpu_simulation` | write | write | none | `manage_effect.enable_gpu_simulation` |
| `manage_effect.get_niagara_info` | `manage_effect` | `get_niagara_info` | read | read | none | `manage_effect.get_niagara_info` |
| `manage_effect.list_debug_shapes` | `manage_effect` | `list_debug_shapes` | read | read | none | `manage_effect.list_debug_shapes` |
| `manage_effect.niagara` | `manage_effect` | `niagara` | write | write | none | `manage_effect.niagara` |
| `manage_effect.particle` | `manage_effect` | `particle` | write | write | none | `manage_effect.particle` |
| `manage_effect.remove_niagara_node` | `manage_effect` | `remove_niagara_node` | destructive | destructive | explicit | `manage_effect.remove_niagara_node` |
| `manage_effect.reset` | `manage_effect` | `reset` | write | write | none | `manage_effect.reset` |
| `manage_effect.set_emitter_properties` | `manage_effect` | `set_emitter_properties` | write | write | none | `manage_effect.set_emitter_properties` |
| `manage_effect.set_niagara_dynamic_input` | `manage_effect` | `set_niagara_dynamic_input` | write | write | none | `manage_effect.set_niagara_dynamic_input` |
| `manage_effect.set_niagara_parameter` | `manage_effect` | `set_niagara_parameter` | write | write | none | `manage_effect.set_niagara_parameter` |
| `manage_effect.set_parameter_value` | `manage_effect` | `set_parameter_value` | write | write | none | `manage_effect.set_parameter_value` |
| `manage_effect.spawn_niagara` | `manage_effect` | `spawn_niagara` | write | write | none | `manage_effect.spawn_niagara` |
| `manage_effect.validate_niagara_system` | `manage_effect` | `validate_niagara_system` | read | read | none | `manage_effect.validate_niagara_system` |
| `manage_gas.add_ability_system_component` | `manage_gas` | `add_ability_system_component` | write | write | none | `manage_gas.add_ability_system_component` |
| `manage_gas.add_ability_task` | `manage_gas` | `add_ability_task` | write | write | none | `manage_gas.add_ability_task` |
| `manage_gas.add_attribute` | `manage_gas` | `add_attribute` | write | write | none | `manage_gas.add_attribute` |
| `manage_gas.add_effect_cue` | `manage_gas` | `add_effect_cue` | write | write | none | `manage_gas.add_effect_cue` |
| `manage_gas.add_effect_execution_calculation` | `manage_gas` | `add_effect_execution_calculation` | write | write | none | `manage_gas.add_effect_execution_calculation` |
| `manage_gas.add_effect_modifier` | `manage_gas` | `add_effect_modifier` | write | write | none | `manage_gas.add_effect_modifier` |
| `manage_gas.add_tag_to_asset` | `manage_gas` | `add_tag_to_asset` | write | write | none | `manage_gas.add_tag_to_asset` |
| `manage_gas.configure_asc` | `manage_gas` | `configure_asc` | write | write | none | `manage_gas.configure_asc` |
| `manage_gas.configure_cue_trigger` | `manage_gas` | `configure_cue_trigger` | write | write | none | `manage_gas.configure_cue_trigger` |
| `manage_gas.create_attribute_set` | `manage_gas` | `create_attribute_set` | write | write | none | `manage_gas.create_attribute_set` |
| `manage_gas.create_gameplay_ability` | `manage_gas` | `create_gameplay_ability` | write | write | none | `manage_gas.create_gameplay_ability` |
| `manage_gas.create_gameplay_cue_notify` | `manage_gas` | `create_gameplay_cue_notify` | write | write | none | `manage_gas.create_gameplay_cue_notify` |
| `manage_gas.create_gameplay_effect` | `manage_gas` | `create_gameplay_effect` | write | write | none | `manage_gas.create_gameplay_effect` |
| `manage_gas.get_gas_info` | `manage_gas` | `get_gas_info` | read | read | none | `manage_gas.get_gas_info` |
| `manage_gas.set_ability_cooldown` | `manage_gas` | `set_ability_cooldown` | write | write | none | `manage_gas.set_ability_cooldown` |
| `manage_gas.set_ability_costs` | `manage_gas` | `set_ability_costs` | write | write | none | `manage_gas.set_ability_costs` |
| `manage_gas.set_ability_tags` | `manage_gas` | `set_ability_tags` | write | write | none | `manage_gas.set_ability_tags` |
| `manage_gas.set_ability_targeting` | `manage_gas` | `set_ability_targeting` | write | write | none | `manage_gas.set_ability_targeting` |
| `manage_gas.set_activation_policy` | `manage_gas` | `set_activation_policy` | write | write | none | `manage_gas.set_activation_policy` |
| `manage_gas.set_attribute_base_value` | `manage_gas` | `set_attribute_base_value` | write | write | none | `manage_gas.set_attribute_base_value` |
| `manage_gas.set_attribute_clamping` | `manage_gas` | `set_attribute_clamping` | write | write | none | `manage_gas.set_attribute_clamping` |
| `manage_gas.set_cue_effects` | `manage_gas` | `set_cue_effects` | write | write | none | `manage_gas.set_cue_effects` |
| `manage_gas.set_effect_duration` | `manage_gas` | `set_effect_duration` | write | write | none | `manage_gas.set_effect_duration` |
| `manage_gas.set_effect_stacking` | `manage_gas` | `set_effect_stacking` | write | write | none | `manage_gas.set_effect_stacking` |
| `manage_gas.set_effect_tags` | `manage_gas` | `set_effect_tags` | write | write | none | `manage_gas.set_effect_tags` |
| `manage_gas.set_instancing_policy` | `manage_gas` | `set_instancing_policy` | write | write | none | `manage_gas.set_instancing_policy` |
| `manage_gas.set_modifier_magnitude` | `manage_gas` | `set_modifier_magnitude` | write | write | none | `manage_gas.set_modifier_magnitude` |
| `manage_geometry.array_linear` | `manage_geometry` | `array_linear` | write | write | none | `manage_geometry.array_linear` |
| `manage_geometry.array_radial` | `manage_geometry` | `array_radial` | write | write | none | `manage_geometry.array_radial` |
| `manage_geometry.auto_uv` | `manage_geometry` | `auto_uv` | write | write | none | `manage_geometry.auto_uv` |
| `manage_geometry.bend` | `manage_geometry` | `bend` | write | write | none | `manage_geometry.bend` |
| `manage_geometry.bevel` | `manage_geometry` | `bevel` | write | write | none | `manage_geometry.bevel` |
| `manage_geometry.boolean_intersection` | `manage_geometry` | `boolean_intersection` | write | write | none | `manage_geometry.boolean_intersection` |
| `manage_geometry.boolean_subtract` | `manage_geometry` | `boolean_subtract` | write | write | none | `manage_geometry.boolean_subtract` |
| `manage_geometry.boolean_trim` | `manage_geometry` | `boolean_trim` | write | write | none | `manage_geometry.boolean_trim` |
| `manage_geometry.boolean_union` | `manage_geometry` | `boolean_union` | write | write | none | `manage_geometry.boolean_union` |
| `manage_geometry.bridge` | `manage_geometry` | `bridge` | write | write | none | `manage_geometry.bridge` |
| `manage_geometry.chamfer` | `manage_geometry` | `chamfer` | write | write | none | `manage_geometry.chamfer` |
| `manage_geometry.convert_to_nanite` | `manage_geometry` | `convert_to_nanite` | write | write | none | `manage_geometry.convert_to_nanite` |
| `manage_geometry.convert_to_static_mesh` | `manage_geometry` | `convert_to_static_mesh` | write | write | none | `manage_geometry.convert_to_static_mesh` |
| `manage_geometry.create_arch` | `manage_geometry` | `create_arch` | write | write | none | `manage_geometry.create_arch` |
| `manage_geometry.create_box` | `manage_geometry` | `create_box` | write | write | none | `manage_geometry.create_box` |
| `manage_geometry.create_capsule` | `manage_geometry` | `create_capsule` | write | write | none | `manage_geometry.create_capsule` |
| `manage_geometry.create_cone` | `manage_geometry` | `create_cone` | write | write | none | `manage_geometry.create_cone` |
| `manage_geometry.create_cylinder` | `manage_geometry` | `create_cylinder` | write | write | none | `manage_geometry.create_cylinder` |
| `manage_geometry.create_disc` | `manage_geometry` | `create_disc` | write | write | none | `manage_geometry.create_disc` |
| `manage_geometry.create_pipe` | `manage_geometry` | `create_pipe` | write | write | none | `manage_geometry.create_pipe` |
| `manage_geometry.create_plane` | `manage_geometry` | `create_plane` | write | write | none | `manage_geometry.create_plane` |
| `manage_geometry.create_ramp` | `manage_geometry` | `create_ramp` | write | write | none | `manage_geometry.create_ramp` |
| `manage_geometry.create_ring` | `manage_geometry` | `create_ring` | write | write | none | `manage_geometry.create_ring` |
| `manage_geometry.create_sphere` | `manage_geometry` | `create_sphere` | write | write | none | `manage_geometry.create_sphere` |
| `manage_geometry.create_spiral_stairs` | `manage_geometry` | `create_spiral_stairs` | write | write | none | `manage_geometry.create_spiral_stairs` |
| `manage_geometry.create_stairs` | `manage_geometry` | `create_stairs` | write | write | none | `manage_geometry.create_stairs` |
| `manage_geometry.create_torus` | `manage_geometry` | `create_torus` | write | write | none | `manage_geometry.create_torus` |
| `manage_geometry.cylindrify` | `manage_geometry` | `cylindrify` | write | write | none | `manage_geometry.cylindrify` |
| `manage_geometry.displace_by_texture` | `manage_geometry` | `displace_by_texture` | write | write | none | `manage_geometry.displace_by_texture` |
| `manage_geometry.duplicate_along_spline` | `manage_geometry` | `duplicate_along_spline` | write | write | none | `manage_geometry.duplicate_along_spline` |
| `manage_geometry.edge_split` | `manage_geometry` | `edge_split` | write | write | none | `manage_geometry.edge_split` |
| `manage_geometry.extrude` | `manage_geometry` | `extrude` | write | write | none | `manage_geometry.extrude` |
| `manage_geometry.extrude_along_spline` | `manage_geometry` | `extrude_along_spline` | write | write | none | `manage_geometry.extrude_along_spline` |
| `manage_geometry.fill_holes` | `manage_geometry` | `fill_holes` | write | write | none | `manage_geometry.fill_holes` |
| `manage_geometry.flip_normals` | `manage_geometry` | `flip_normals` | write | write | none | `manage_geometry.flip_normals` |
| `manage_geometry.generate_collision` | `manage_geometry` | `generate_collision` | write | write | none | `manage_geometry.generate_collision` |
| `manage_geometry.generate_complex_collision` | `manage_geometry` | `generate_complex_collision` | write | write | none | `manage_geometry.generate_complex_collision` |
| `manage_geometry.generate_lods` | `manage_geometry` | `generate_lods` | write | write | none | `manage_geometry.generate_lods` |
| `manage_geometry.get_mesh_info` | `manage_geometry` | `get_mesh_info` | read | read | none | `manage_geometry.get_mesh_info` |
| `manage_geometry.inset` | `manage_geometry` | `inset` | write | write | none | `manage_geometry.inset` |
| `manage_geometry.lattice_deform` | `manage_geometry` | `lattice_deform` | write | write | none | `manage_geometry.lattice_deform` |
| `manage_geometry.loft` | `manage_geometry` | `loft` | write | write | none | `manage_geometry.loft` |
| `manage_geometry.loop_cut` | `manage_geometry` | `loop_cut` | write | write | none | `manage_geometry.loop_cut` |
| `manage_geometry.merge_vertices` | `manage_geometry` | `merge_vertices` | write | write | none | `manage_geometry.merge_vertices` |
| `manage_geometry.mirror` | `manage_geometry` | `mirror` | write | write | none | `manage_geometry.mirror` |
| `manage_geometry.noise_deform` | `manage_geometry` | `noise_deform` | write | write | none | `manage_geometry.noise_deform` |
| `manage_geometry.offset_faces` | `manage_geometry` | `offset_faces` | write | write | none | `manage_geometry.offset_faces` |
| `manage_geometry.outset` | `manage_geometry` | `outset` | write | write | none | `manage_geometry.outset` |
| `manage_geometry.pack_uv_islands` | `manage_geometry` | `pack_uv_islands` | write | write | none | `manage_geometry.pack_uv_islands` |
| `manage_geometry.poke` | `manage_geometry` | `poke` | write | write | none | `manage_geometry.poke` |
| `manage_geometry.project_uv` | `manage_geometry` | `project_uv` | write | write | none | `manage_geometry.project_uv` |
| `manage_geometry.quadrangulate` | `manage_geometry` | `quadrangulate` | write | write | none | `manage_geometry.quadrangulate` |
| `manage_geometry.recalculate_normals` | `manage_geometry` | `recalculate_normals` | write | write | none | `manage_geometry.recalculate_normals` |
| `manage_geometry.recompute_tangents` | `manage_geometry` | `recompute_tangents` | write | write | none | `manage_geometry.recompute_tangents` |
| `manage_geometry.relax` | `manage_geometry` | `relax` | write | write | none | `manage_geometry.relax` |
| `manage_geometry.remesh_uniform` | `manage_geometry` | `remesh_uniform` | write | write | none | `manage_geometry.remesh_uniform` |
| `manage_geometry.remesh_voxel` | `manage_geometry` | `remesh_voxel` | write | write | none | `manage_geometry.remesh_voxel` |
| `manage_geometry.remove_degenerates` | `manage_geometry` | `remove_degenerates` | write | write | none | `manage_geometry.remove_degenerates` |
| `manage_geometry.revolve` | `manage_geometry` | `revolve` | write | write | none | `manage_geometry.revolve` |
| `manage_geometry.self_union` | `manage_geometry` | `self_union` | write | write | none | `manage_geometry.self_union` |
| `manage_geometry.set_lod_screen_sizes` | `manage_geometry` | `set_lod_screen_sizes` | write | write | none | `manage_geometry.set_lod_screen_sizes` |
| `manage_geometry.set_lod_settings` | `manage_geometry` | `set_lod_settings` | write | write | none | `manage_geometry.set_lod_settings` |
| `manage_geometry.shell` | `manage_geometry` | `shell` | write | write | none | `manage_geometry.shell` |
| `manage_geometry.simplify_collision` | `manage_geometry` | `simplify_collision` | write | write | none | `manage_geometry.simplify_collision` |
| `manage_geometry.simplify_mesh` | `manage_geometry` | `simplify_mesh` | write | write | none | `manage_geometry.simplify_mesh` |
| `manage_geometry.smooth` | `manage_geometry` | `smooth` | write | write | none | `manage_geometry.smooth` |
| `manage_geometry.spherify` | `manage_geometry` | `spherify` | write | write | none | `manage_geometry.spherify` |
| `manage_geometry.stretch` | `manage_geometry` | `stretch` | write | write | none | `manage_geometry.stretch` |
| `manage_geometry.subdivide` | `manage_geometry` | `subdivide` | write | write | none | `manage_geometry.subdivide` |
| `manage_geometry.sweep` | `manage_geometry` | `sweep` | write | write | none | `manage_geometry.sweep` |
| `manage_geometry.taper` | `manage_geometry` | `taper` | write | write | none | `manage_geometry.taper` |
| `manage_geometry.transform_uvs` | `manage_geometry` | `transform_uvs` | write | write | none | `manage_geometry.transform_uvs` |
| `manage_geometry.triangulate` | `manage_geometry` | `triangulate` | write | write | none | `manage_geometry.triangulate` |
| `manage_geometry.twist` | `manage_geometry` | `twist` | write | write | none | `manage_geometry.twist` |
| `manage_geometry.unwrap_uv` | `manage_geometry` | `unwrap_uv` | write | write | none | `manage_geometry.unwrap_uv` |
| `manage_geometry.weld_vertices` | `manage_geometry` | `weld_vertices` | write | write | none | `manage_geometry.weld_vertices` |
| `manage_interaction.add_destruction_component` | `manage_interaction` | `add_destruction_component` | write | write | none | `manage_interaction.add_destruction_component` |
| `manage_interaction.add_interaction_events` | `manage_interaction` | `add_interaction_events` | write | write | none | `manage_interaction.add_interaction_events` |
| `manage_interaction.configure_chest_properties` | `manage_interaction` | `configure_chest_properties` | write | write | none | `manage_interaction.configure_chest_properties` |
| `manage_interaction.configure_destruction_damage` | `manage_interaction` | `configure_destruction_damage` | write | write | none | `manage_interaction.configure_destruction_damage` |
| `manage_interaction.configure_destruction_effects` | `manage_interaction` | `configure_destruction_effects` | write | write | none | `manage_interaction.configure_destruction_effects` |
| `manage_interaction.configure_destruction_levels` | `manage_interaction` | `configure_destruction_levels` | write | write | none | `manage_interaction.configure_destruction_levels` |
| `manage_interaction.configure_door_properties` | `manage_interaction` | `configure_door_properties` | write | write | none | `manage_interaction.configure_door_properties` |
| `manage_interaction.configure_interaction_trace` | `manage_interaction` | `configure_interaction_trace` | write | write | none | `manage_interaction.configure_interaction_trace` |
| `manage_interaction.configure_interaction_widget` | `manage_interaction` | `configure_interaction_widget` | write | write | none | `manage_interaction.configure_interaction_widget` |
| `manage_interaction.configure_switch_properties` | `manage_interaction` | `configure_switch_properties` | write | write | none | `manage_interaction.configure_switch_properties` |
| `manage_interaction.configure_trigger_events` | `manage_interaction` | `configure_trigger_events` | write | write | none | `manage_interaction.configure_trigger_events` |
| `manage_interaction.configure_trigger_filter` | `manage_interaction` | `configure_trigger_filter` | write | write | none | `manage_interaction.configure_trigger_filter` |
| `manage_interaction.configure_trigger_response` | `manage_interaction` | `configure_trigger_response` | write | write | none | `manage_interaction.configure_trigger_response` |
| `manage_interaction.create_chest_actor` | `manage_interaction` | `create_chest_actor` | write | write | none | `manage_interaction.create_chest_actor` |
| `manage_interaction.create_door_actor` | `manage_interaction` | `create_door_actor` | write | write | none | `manage_interaction.create_door_actor` |
| `manage_interaction.create_interactable_interface` | `manage_interaction` | `create_interactable_interface` | write | write | none | `manage_interaction.create_interactable_interface` |
| `manage_interaction.create_interaction_component` | `manage_interaction` | `create_interaction_component` | write | write | none | `manage_interaction.create_interaction_component` |
| `manage_interaction.create_lever_actor` | `manage_interaction` | `create_lever_actor` | write | write | none | `manage_interaction.create_lever_actor` |
| `manage_interaction.create_switch_actor` | `manage_interaction` | `create_switch_actor` | write | write | none | `manage_interaction.create_switch_actor` |
| `manage_interaction.create_trigger_actor` | `manage_interaction` | `create_trigger_actor` | write | write | none | `manage_interaction.create_trigger_actor` |
| `manage_interaction.get_interaction_info` | `manage_interaction` | `get_interaction_info` | read | read | none | `manage_interaction.get_interaction_info` |
| `manage_interaction.setup_destructible_mesh` | `manage_interaction` | `setup_destructible_mesh` | write | write | none | `manage_interaction.setup_destructible_mesh` |
| `manage_inventory.add_crafting_component` | `manage_inventory` | `add_crafting_component` | write | write | none | `manage_inventory.add_crafting_component` |
| `manage_inventory.add_equipment_functions` | `manage_inventory` | `add_equipment_functions` | write | write | none | `manage_inventory.add_equipment_functions` |
| `manage_inventory.add_inventory_functions` | `manage_inventory` | `add_inventory_functions` | write | write | none | `manage_inventory.add_inventory_functions` |
| `manage_inventory.add_loot_entry` | `manage_inventory` | `add_loot_entry` | write | write | none | `manage_inventory.add_loot_entry` |
| `manage_inventory.add_recipe_ingredient` | `manage_inventory` | `add_recipe_ingredient` | write | write | none | `manage_inventory.add_recipe_ingredient` |
| `manage_inventory.assign_item_category` | `manage_inventory` | `assign_item_category` | write | write | none | `manage_inventory.assign_item_category` |
| `manage_inventory.configure_equipment_effects` | `manage_inventory` | `configure_equipment_effects` | write | write | none | `manage_inventory.configure_equipment_effects` |
| `manage_inventory.configure_equipment_visuals` | `manage_inventory` | `configure_equipment_visuals` | write | write | none | `manage_inventory.configure_equipment_visuals` |
| `manage_inventory.configure_inventory_events` | `manage_inventory` | `configure_inventory_events` | write | write | none | `manage_inventory.configure_inventory_events` |
| `manage_inventory.configure_inventory_slots` | `manage_inventory` | `configure_inventory_slots` | write | write | none | `manage_inventory.configure_inventory_slots` |
| `manage_inventory.configure_inventory_weight` | `manage_inventory` | `configure_inventory_weight` | write | write | none | `manage_inventory.configure_inventory_weight` |
| `manage_inventory.configure_item_stacking` | `manage_inventory` | `configure_item_stacking` | write | write | none | `manage_inventory.configure_item_stacking` |
| `manage_inventory.configure_loot_drop` | `manage_inventory` | `configure_loot_drop` | write | write | none | `manage_inventory.configure_loot_drop` |
| `manage_inventory.configure_pickup_effects` | `manage_inventory` | `configure_pickup_effects` | write | write | none | `manage_inventory.configure_pickup_effects` |
| `manage_inventory.configure_pickup_interaction` | `manage_inventory` | `configure_pickup_interaction` | write | write | none | `manage_inventory.configure_pickup_interaction` |
| `manage_inventory.configure_pickup_respawn` | `manage_inventory` | `configure_pickup_respawn` | write | write | none | `manage_inventory.configure_pickup_respawn` |
| `manage_inventory.configure_recipe_requirements` | `manage_inventory` | `configure_recipe_requirements` | write | write | none | `manage_inventory.configure_recipe_requirements` |
| `manage_inventory.configure_station_recipes` | `manage_inventory` | `configure_station_recipes` | write | write | none | `manage_inventory.configure_station_recipes` |
| `manage_inventory.create_crafting_recipe` | `manage_inventory` | `create_crafting_recipe` | write | write | none | `manage_inventory.create_crafting_recipe` |
| `manage_inventory.create_crafting_station` | `manage_inventory` | `create_crafting_station` | write | write | none | `manage_inventory.create_crafting_station` |
| `manage_inventory.create_equipment_component` | `manage_inventory` | `create_equipment_component` | write | write | none | `manage_inventory.create_equipment_component` |
| `manage_inventory.create_inventory_component` | `manage_inventory` | `create_inventory_component` | write | write | none | `manage_inventory.create_inventory_component` |
| `manage_inventory.create_item_category` | `manage_inventory` | `create_item_category` | write | write | none | `manage_inventory.create_item_category` |
| `manage_inventory.create_item_data_asset` | `manage_inventory` | `create_item_data_asset` | write | write | none | `manage_inventory.create_item_data_asset` |
| `manage_inventory.create_loot_table` | `manage_inventory` | `create_loot_table` | write | write | none | `manage_inventory.create_loot_table` |
| `manage_inventory.create_pickup_actor` | `manage_inventory` | `create_pickup_actor` | write | write | none | `manage_inventory.create_pickup_actor` |
| `manage_inventory.define_equipment_slots` | `manage_inventory` | `define_equipment_slots` | write | write | none | `manage_inventory.define_equipment_slots` |
| `manage_inventory.get_inventory_info` | `manage_inventory` | `get_inventory_info` | read | read | none | `manage_inventory.get_inventory_info` |
| `manage_inventory.remove_loot_entry` | `manage_inventory` | `remove_loot_entry` | write | write | none | `manage_inventory.remove_loot_entry` |
| `manage_inventory.set_inventory_replication` | `manage_inventory` | `set_inventory_replication` | write | write | none | `manage_inventory.set_inventory_replication` |
| `manage_inventory.set_item_icon` | `manage_inventory` | `set_item_icon` | write | write | none | `manage_inventory.set_item_icon` |
| `manage_inventory.set_item_properties` | `manage_inventory` | `set_item_properties` | write | write | none | `manage_inventory.set_item_properties` |
| `manage_inventory.set_loot_quality_tiers` | `manage_inventory` | `set_loot_quality_tiers` | write | write | none | `manage_inventory.set_loot_quality_tiers` |
| `manage_level.add_sublevel` | `manage_level` | `add_sublevel` | write | write | none | `manage_level.add_sublevel` |
| `manage_level.build_lighting` | `manage_level` | `manage_lighting` | write | write | none | `manage_level.build_lighting` |
| `manage_level.create_level` | `manage_level` | `manage_level_structure` | write | write | none | `manage_level.create_level` |
| `manage_level.create_light` | `manage_level` | `manage_lighting` | write | write | none | `manage_level.create_light` |
| `manage_level.delete` | `manage_level` | `delete_level` | destructive | destructive | explicit | `manage_level.delete` |
| `manage_level.delete_level` | `manage_level` | `delete_level` | destructive | destructive | explicit | `manage_level.delete_level` |
| `manage_level.duplicate_level` | `manage_level` | `duplicate` | write | write | none | `manage_level.duplicate_level` |
| `manage_level.export_level` | `manage_level` | `export_level` | read | read | none | `manage_level.export_level` |
| `manage_level.get_current_level` | `manage_level` | `get_current_level` | read | read | none | `manage_level.get_current_level` |
| `manage_level.get_summary` | `manage_level` | `get_summary` | read | read | none | `manage_level.get_summary` |
| `manage_level.import_level` | `manage_level` | `import_level` | write | write | none | `manage_level.import_level` |
| `manage_level.list_levels` | `manage_level` | `list_levels` | read | read | none | `manage_level.list_levels` |
| `manage_level.load` | `manage_level` | `load` | write | write | none | `manage_level.load` |
| `manage_level.load_level` | `manage_level` | `load` | write | write | none | `manage_level.load_level` |
| `manage_level.rename_level` | `manage_level` | `rename` | write | write | none | `manage_level.rename_level` |
| `manage_level.save` | `manage_level` | `save` | write | write | none | `manage_level.save` |
| `manage_level.save_as` | `manage_level` | `save_level_as` | write | write | none | `manage_level.save_as` |
| `manage_level.save_level` | `manage_level` | `save` | write | write | none | `manage_level.save_level` |
| `manage_level.save_level_as` | `manage_level` | `save_level_as` | write | write | none | `manage_level.save_level_as` |
| `manage_level.set_metadata` | `manage_level` | `set_metadata` | write | write | none | `manage_level.set_metadata` |
| `manage_level.stream` | `manage_level` | `stream_level` | write | write | none | `manage_level.stream` |
| `manage_level.unload` | `manage_level` | `stream_level` | write | write | none | `manage_level.unload` |
| `manage_level.unload_level` | `manage_level` | `unload_level` | write | write | none | `manage_level.unload_level` |
| `manage_level.validate_level` | `manage_level` | `execute_editor_function` | read | read | none | `manage_level.validate_level` |
| `manage_level_structure.add_blocking_volume` | `manage_level_structure` | `add_blocking_volume` | write | write | none | `manage_level_structure.add_blocking_volume` |
| `manage_level_structure.add_cull_distance_volume` | `manage_level_structure` | `add_cull_distance_volume` | write | write | none | `manage_level_structure.add_cull_distance_volume` |
| `manage_level_structure.add_kill_z_volume` | `manage_level_structure` | `add_kill_z_volume` | write | write | none | `manage_level_structure.add_kill_z_volume` |
| `manage_level_structure.add_level_blueprint_node` | `manage_level_structure` | `add_level_blueprint_node` | write | write | none | `manage_level_structure.add_level_blueprint_node` |
| `manage_level_structure.add_physics_volume` | `manage_level_structure` | `add_physics_volume` | write | write | none | `manage_level_structure.add_physics_volume` |
| `manage_level_structure.add_post_process_volume` | `manage_level_structure` | `add_post_process_volume` | write | write | none | `manage_level_structure.add_post_process_volume` |
| `manage_level_structure.add_trigger_volume` | `manage_level_structure` | `add_trigger_volume` | write | write | none | `manage_level_structure.add_trigger_volume` |
| `manage_level_structure.assign_actor_to_data_layer` | `manage_level_structure` | `assign_actor_to_data_layer` | write | write | none | `manage_level_structure.assign_actor_to_data_layer` |
| `manage_level_structure.configure_grid_size` | `manage_level_structure` | `configure_grid_size` | write | write | none | `manage_level_structure.configure_grid_size` |
| `manage_level_structure.configure_hlod_layer` | `manage_level_structure` | `configure_hlod_layer` | write | write | none | `manage_level_structure.configure_hlod_layer` |
| `manage_level_structure.configure_level_bounds` | `manage_level_structure` | `configure_level_bounds` | write | write | none | `manage_level_structure.configure_level_bounds` |
| `manage_level_structure.configure_level_streaming` | `manage_level_structure` | `configure_level_streaming` | write | write | none | `manage_level_structure.configure_level_streaming` |
| `manage_level_structure.connect_level_blueprint_nodes` | `manage_level_structure` | `connect_level_blueprint_nodes` | write | write | none | `manage_level_structure.connect_level_blueprint_nodes` |
| `manage_level_structure.create_audio_volume` | `manage_level_structure` | `create_audio_volume` | write | write | none | `manage_level_structure.create_audio_volume` |
| `manage_level_structure.create_blocking_volume` | `manage_level_structure` | `create_blocking_volume` | write | write | none | `manage_level_structure.create_blocking_volume` |
| `manage_level_structure.create_camera_blocking_volume` | `manage_level_structure` | `create_camera_blocking_volume` | write | write | none | `manage_level_structure.create_camera_blocking_volume` |
| `manage_level_structure.create_cull_distance_volume` | `manage_level_structure` | `create_cull_distance_volume` | write | write | none | `manage_level_structure.create_cull_distance_volume` |
| `manage_level_structure.create_data_layer` | `manage_level_structure` | `create_data_layer` | write | write | none | `manage_level_structure.create_data_layer` |
| `manage_level_structure.create_kill_z_volume` | `manage_level_structure` | `create_kill_z_volume` | write | write | none | `manage_level_structure.create_kill_z_volume` |
| `manage_level_structure.create_level` | `manage_level_structure` | `create_level` | write | write | none | `manage_level_structure.create_level` |
| `manage_level_structure.create_level_instance` | `manage_level_structure` | `create_level_instance` | write | write | none | `manage_level_structure.create_level_instance` |
| `manage_level_structure.create_lightmass_importance_volume` | `manage_level_structure` | `create_lightmass_importance_volume` | write | write | none | `manage_level_structure.create_lightmass_importance_volume` |
| `manage_level_structure.create_minimap_volume` | `manage_level_structure` | `create_minimap_volume` | write | write | none | `manage_level_structure.create_minimap_volume` |
| `manage_level_structure.create_nav_mesh_bounds_volume` | `manage_level_structure` | `create_nav_mesh_bounds_volume` | write | write | none | `manage_level_structure.create_nav_mesh_bounds_volume` |
| `manage_level_structure.create_nav_modifier_volume` | `manage_level_structure` | `create_nav_modifier_volume` | write | write | none | `manage_level_structure.create_nav_modifier_volume` |
| `manage_level_structure.create_packed_level_actor` | `manage_level_structure` | `create_packed_level_actor` | write | write | none | `manage_level_structure.create_packed_level_actor` |
| `manage_level_structure.create_pain_causing_volume` | `manage_level_structure` | `create_pain_causing_volume` | write | write | none | `manage_level_structure.create_pain_causing_volume` |
| `manage_level_structure.create_physics_volume` | `manage_level_structure` | `create_physics_volume` | write | write | none | `manage_level_structure.create_physics_volume` |
| `manage_level_structure.create_post_process_volume` | `manage_level_structure` | `create_post_process_volume` | write | write | none | `manage_level_structure.create_post_process_volume` |
| `manage_level_structure.create_precomputed_visibility_volume` | `manage_level_structure` | `create_precomputed_visibility_volume` | write | write | none | `manage_level_structure.create_precomputed_visibility_volume` |
| `manage_level_structure.create_reverb_volume` | `manage_level_structure` | `create_reverb_volume` | write | write | none | `manage_level_structure.create_reverb_volume` |
| `manage_level_structure.create_sublevel` | `manage_level_structure` | `create_sublevel` | write | write | none | `manage_level_structure.create_sublevel` |
| `manage_level_structure.create_trigger_box` | `manage_level_structure` | `create_trigger_box` | write | write | none | `manage_level_structure.create_trigger_box` |
| `manage_level_structure.create_trigger_capsule` | `manage_level_structure` | `create_trigger_capsule` | write | write | none | `manage_level_structure.create_trigger_capsule` |
| `manage_level_structure.create_trigger_sphere` | `manage_level_structure` | `create_trigger_sphere` | write | write | none | `manage_level_structure.create_trigger_sphere` |
| `manage_level_structure.create_trigger_volume` | `manage_level_structure` | `create_trigger_volume` | write | write | none | `manage_level_structure.create_trigger_volume` |
| `manage_level_structure.enable_world_partition` | `manage_level_structure` | `enable_world_partition` | write | write | none | `manage_level_structure.enable_world_partition` |
| `manage_level_structure.get_level_structure_info` | `manage_level_structure` | `get_level_structure_info` | read | read | none | `manage_level_structure.get_level_structure_info` |
| `manage_level_structure.get_volumes_info` | `manage_level_structure` | `get_volumes_info` | read | read | none | `manage_level_structure.get_volumes_info` |
| `manage_level_structure.open_level_blueprint` | `manage_level_structure` | `open_level_blueprint` | write | write | none | `manage_level_structure.open_level_blueprint` |
| `manage_level_structure.remove_volume` | `manage_level_structure` | `remove_volume` | destructive | destructive | explicit | `manage_level_structure.remove_volume` |
| `manage_level_structure.set_streaming_distance` | `manage_level_structure` | `set_streaming_distance` | write | write | none | `manage_level_structure.set_streaming_distance` |
| `manage_level_structure.set_volume_bounds` | `manage_level_structure` | `set_volume_bounds` | write | write | none | `manage_level_structure.set_volume_bounds` |
| `manage_level_structure.set_volume_extent` | `manage_level_structure` | `set_volume_extent` | write | write | none | `manage_level_structure.set_volume_extent` |
| `manage_level_structure.set_volume_properties` | `manage_level_structure` | `set_volume_properties` | write | write | none | `manage_level_structure.set_volume_properties` |
| `manage_networking.add_legacy_action_mapping` | `manage_networking` | `manage_input` | write | write | none | `manage_networking.add_legacy_action_mapping` |
| `manage_networking.add_legacy_axis_mapping` | `manage_networking` | `manage_input` | write | write | none | `manage_networking.add_legacy_axis_mapping` |
| `manage_networking.add_local_player` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.add_local_player` |
| `manage_networking.add_mapping` | `manage_networking` | `manage_input` | write | write | none | `manage_networking.add_mapping` |
| `manage_networking.add_network_prediction_data` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.add_network_prediction_data` |
| `manage_networking.check_has_authority` | `manage_networking` | `manage_networking` | read | read | none | `manage_networking.check_has_authority` |
| `manage_networking.check_is_locally_controlled` | `manage_networking` | `manage_networking` | read | read | none | `manage_networking.check_is_locally_controlled` |
| `manage_networking.configure_client_prediction` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_client_prediction` |
| `manage_networking.configure_game_rules` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.configure_game_rules` |
| `manage_networking.configure_lan_play` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.configure_lan_play` |
| `manage_networking.configure_local_session_settings` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.configure_local_session_settings` |
| `manage_networking.configure_movement_prediction` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_movement_prediction` |
| `manage_networking.configure_net_cull_distance` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_net_cull_distance` |
| `manage_networking.configure_net_driver` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_net_driver` |
| `manage_networking.configure_net_priority` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_net_priority` |
| `manage_networking.configure_net_serialization` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_net_serialization` |
| `manage_networking.configure_net_update_frequency` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_net_update_frequency` |
| `manage_networking.configure_player_start` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.configure_player_start` |
| `manage_networking.configure_push_model` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_push_model` |
| `manage_networking.configure_push_to_talk` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.configure_push_to_talk` |
| `manage_networking.configure_replicated_movement` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_replicated_movement` |
| `manage_networking.configure_replication_graph` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_replication_graph` |
| `manage_networking.configure_round_system` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.configure_round_system` |
| `manage_networking.configure_rpc_validation` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_rpc_validation` |
| `manage_networking.configure_scoring_system` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.configure_scoring_system` |
| `manage_networking.configure_server_correction` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.configure_server_correction` |
| `manage_networking.configure_session_interface` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.configure_session_interface` |
| `manage_networking.configure_spawn_system` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.configure_spawn_system` |
| `manage_networking.configure_spectating` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.configure_spectating` |
| `manage_networking.configure_split_screen` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.configure_split_screen` |
| `manage_networking.configure_team_system` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.configure_team_system` |
| `manage_networking.configure_voice_settings` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.configure_voice_settings` |
| `manage_networking.create_game_instance` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.create_game_instance` |
| `manage_networking.create_game_mode` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.create_game_mode` |
| `manage_networking.create_game_state` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.create_game_state` |
| `manage_networking.create_hud_class` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.create_hud_class` |
| `manage_networking.create_input_action` | `manage_networking` | `manage_input` | write | write | none | `manage_networking.create_input_action` |
| `manage_networking.create_input_mapping_context` | `manage_networking` | `manage_input` | write | write | none | `manage_networking.create_input_mapping_context` |
| `manage_networking.create_player_controller` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.create_player_controller` |
| `manage_networking.create_player_state` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.create_player_state` |
| `manage_networking.create_rpc_function` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.create_rpc_function` |
| `manage_networking.disable_input_action` | `manage_networking` | `manage_input` | write | write | none | `manage_networking.disable_input_action` |
| `manage_networking.enable_input_mapping` | `manage_networking` | `manage_input` | write | write | none | `manage_networking.enable_input_mapping` |
| `manage_networking.enable_voice_chat` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.enable_voice_chat` |
| `manage_networking.get_game_framework_info` | `manage_networking` | `manage_game_framework` | read | read | none | `manage_networking.get_game_framework_info` |
| `manage_networking.get_input_info` | `manage_networking` | `manage_input` | read | read | none | `manage_networking.get_input_info` |
| `manage_networking.get_networking_info` | `manage_networking` | `manage_networking` | read | read | none | `manage_networking.get_networking_info` |
| `manage_networking.get_sessions_info` | `manage_networking` | `manage_sessions` | read | read | none | `manage_networking.get_sessions_info` |
| `manage_networking.host_lan_server` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.host_lan_server` |
| `manage_networking.join_lan_server` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.join_lan_server` |
| `manage_networking.map_input_action` | `manage_networking` | `manage_input` | write | write | none | `manage_networking.map_input_action` |
| `manage_networking.mute_player` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.mute_player` |
| `manage_networking.remove_legacy_action_mapping` | `manage_networking` | `manage_input` | destructive | destructive | explicit | `manage_networking.remove_legacy_action_mapping` |
| `manage_networking.remove_legacy_axis_mapping` | `manage_networking` | `manage_input` | destructive | destructive | explicit | `manage_networking.remove_legacy_axis_mapping` |
| `manage_networking.remove_local_player` | `manage_networking` | `manage_sessions` | destructive | destructive | explicit | `manage_networking.remove_local_player` |
| `manage_networking.remove_mapping` | `manage_networking` | `manage_input` | destructive | destructive | explicit | `manage_networking.remove_mapping` |
| `manage_networking.set_always_relevant` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.set_always_relevant` |
| `manage_networking.set_autonomous_proxy` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.set_autonomous_proxy` |
| `manage_networking.set_default_pawn_class` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.set_default_pawn_class` |
| `manage_networking.set_game_state_class` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.set_game_state_class` |
| `manage_networking.set_input_modifier` | `manage_networking` | `manage_input` | write | write | none | `manage_networking.set_input_modifier` |
| `manage_networking.set_input_trigger` | `manage_networking` | `manage_input` | write | write | none | `manage_networking.set_input_trigger` |
| `manage_networking.set_net_dormancy` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.set_net_dormancy` |
| `manage_networking.set_net_role` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.set_net_role` |
| `manage_networking.set_only_relevant_to_owner` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.set_only_relevant_to_owner` |
| `manage_networking.set_owner` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.set_owner` |
| `manage_networking.set_player_controller_class` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.set_player_controller_class` |
| `manage_networking.set_player_state_class` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.set_player_state_class` |
| `manage_networking.set_property_replicated` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.set_property_replicated` |
| `manage_networking.set_replicated_using` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.set_replicated_using` |
| `manage_networking.set_replication_condition` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.set_replication_condition` |
| `manage_networking.set_respawn_rules` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.set_respawn_rules` |
| `manage_networking.set_rpc_reliability` | `manage_networking` | `manage_networking` | write | write | none | `manage_networking.set_rpc_reliability` |
| `manage_networking.set_split_screen_type` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.set_split_screen_type` |
| `manage_networking.set_voice_attenuation` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.set_voice_attenuation` |
| `manage_networking.set_voice_channel` | `manage_networking` | `manage_sessions` | write | write | none | `manage_networking.set_voice_channel` |
| `manage_networking.setup_match_states` | `manage_networking` | `manage_game_framework` | write | write | none | `manage_networking.setup_match_states` |
| `manage_pcg.add_actor_data_node` | `manage_pcg` | `add_actor_data_node` | write | write | none | `manage_pcg.add_actor_data_node` |
| `manage_pcg.add_actor_spawner` | `manage_pcg` | `add_actor_spawner` | write | write | none | `manage_pcg.add_actor_spawner` |
| `manage_pcg.add_bounds_filter` | `manage_pcg` | `add_bounds_filter` | write | write | none | `manage_pcg.add_bounds_filter` |
| `manage_pcg.add_bounds_modifier` | `manage_pcg` | `add_bounds_modifier` | write | write | none | `manage_pcg.add_bounds_modifier` |
| `manage_pcg.add_copy_points` | `manage_pcg` | `add_copy_points` | write | write | none | `manage_pcg.add_copy_points` |
| `manage_pcg.add_density_filter` | `manage_pcg` | `add_density_filter` | write | write | none | `manage_pcg.add_density_filter` |
| `manage_pcg.add_distance_filter` | `manage_pcg` | `add_distance_filter` | write | write | none | `manage_pcg.add_distance_filter` |
| `manage_pcg.add_height_filter` | `manage_pcg` | `add_height_filter` | write | write | none | `manage_pcg.add_height_filter` |
| `manage_pcg.add_landscape_data_node` | `manage_pcg` | `add_landscape_data_node` | write | write | none | `manage_pcg.add_landscape_data_node` |
| `manage_pcg.add_merge_points` | `manage_pcg` | `add_merge_points` | write | write | none | `manage_pcg.add_merge_points` |
| `manage_pcg.add_mesh_sampler` | `manage_pcg` | `add_mesh_sampler` | write | write | none | `manage_pcg.add_mesh_sampler` |
| `manage_pcg.add_pcg_node` | `manage_pcg` | `add_pcg_node` | write | write | none | `manage_pcg.add_pcg_node` |
| `manage_pcg.add_project_to_surface` | `manage_pcg` | `add_project_to_surface` | write | write | none | `manage_pcg.add_project_to_surface` |
| `manage_pcg.add_self_pruning` | `manage_pcg` | `add_self_pruning` | write | write | none | `manage_pcg.add_self_pruning` |
| `manage_pcg.add_slope_filter` | `manage_pcg` | `add_slope_filter` | write | write | none | `manage_pcg.add_slope_filter` |
| `manage_pcg.add_spline_data_node` | `manage_pcg` | `add_spline_data_node` | write | write | none | `manage_pcg.add_spline_data_node` |
| `manage_pcg.add_spline_sampler` | `manage_pcg` | `add_spline_sampler` | write | write | none | `manage_pcg.add_spline_sampler` |
| `manage_pcg.add_spline_spawner` | `manage_pcg` | `add_spline_spawner` | write | write | none | `manage_pcg.add_spline_spawner` |
| `manage_pcg.add_static_mesh_spawner` | `manage_pcg` | `add_static_mesh_spawner` | write | write | none | `manage_pcg.add_static_mesh_spawner` |
| `manage_pcg.add_surface_sampler` | `manage_pcg` | `add_surface_sampler` | write | write | none | `manage_pcg.add_surface_sampler` |
| `manage_pcg.add_texture_data_node` | `manage_pcg` | `add_texture_data_node` | write | write | none | `manage_pcg.add_texture_data_node` |
| `manage_pcg.add_transform_points` | `manage_pcg` | `add_transform_points` | write | write | none | `manage_pcg.add_transform_points` |
| `manage_pcg.add_volume_data_node` | `manage_pcg` | `add_volume_data_node` | write | write | none | `manage_pcg.add_volume_data_node` |
| `manage_pcg.add_volume_sampler` | `manage_pcg` | `add_volume_sampler` | write | write | none | `manage_pcg.add_volume_sampler` |
| `manage_pcg.connect_pcg_pins` | `manage_pcg` | `connect_pcg_pins` | write | write | none | `manage_pcg.connect_pcg_pins` |
| `manage_pcg.create_pcg_graph` | `manage_pcg` | `create_pcg_graph` | write | write | none | `manage_pcg.create_pcg_graph` |
| `manage_pcg.create_pcg_subgraph` | `manage_pcg` | `create_pcg_subgraph` | write | write | none | `manage_pcg.create_pcg_subgraph` |
| `manage_pcg.execute_pcg_graph` | `manage_pcg` | `execute_pcg_graph` | write | write | none | `manage_pcg.execute_pcg_graph` |
| `manage_pcg.set_pcg_node_settings` | `manage_pcg` | `set_pcg_node_settings` | write | write | none | `manage_pcg.set_pcg_node_settings` |
| `manage_pcg.set_pcg_partition_grid_size` | `manage_pcg` | `set_pcg_partition_grid_size` | write | write | none | `manage_pcg.set_pcg_partition_grid_size` |
| `manage_tools.disable_category` | `manage_tools` | `disable_category` | write | write | none | `manage_tools.disable_category` |
| `manage_tools.disable_tools` | `manage_tools` | `disable_tools` | write | write | none | `manage_tools.disable_tools` |
| `manage_tools.enable_category` | `manage_tools` | `enable_category` | write | write | none | `manage_tools.enable_category` |
| `manage_tools.enable_tools` | `manage_tools` | `enable_tools` | write | write | none | `manage_tools.enable_tools` |
| `manage_tools.get_status` | `manage_tools` | `get_status` | read | read | none | `manage_tools.get_status` |
| `manage_tools.list_categories` | `manage_tools` | `list_categories` | read | read | none | `manage_tools.list_categories` |
| `manage_tools.list_tools` | `manage_tools` | `list_tools` | read | read | none | `manage_tools.list_tools` |
| `manage_tools.reset` | `manage_tools` | `reset` | write | write | none | `manage_tools.reset` |
| `material.add_custom_expression` | `manage_asset` | `add_custom_expression` | write | write | explicit | `manage_asset.add_custom_expression` |
| `material.add_fresnel` | `manage_asset` | `add_fresnel` | write | write | explicit | `manage_asset.add_fresnel` |
| `material.add_function_input` | `manage_asset` | `add_function_input` | write | write | explicit | `manage_asset.add_function_input` |
| `material.add_function_output` | `manage_asset` | `add_function_output` | write | write | explicit | `manage_asset.add_function_output` |
| `material.add_if` | `manage_asset` | `add_if` | write | write | explicit | `manage_asset.add_if` |
| `material.add_landscape_layer` | `manage_asset` | `add_landscape_layer` | write | write | explicit | `manage_asset.add_landscape_layer` |
| `material.add_material_node` | `manage_asset` | `add_material_node` | write | write | explicit | `manage_asset.add_material_node` |
| `material.add_math_node` | `manage_asset` | `add_math_node` | write | write | explicit | `manage_asset.add_math_node` |
| `material.add_noise` | `manage_asset` | `add_noise` | write | write | explicit | `manage_asset.add_noise` |
| `material.add_panner` | `manage_asset` | `add_panner` | write | write | explicit | `manage_asset.add_panner` |
| `material.add_pixel_depth` | `manage_asset` | `add_pixel_depth` | write | write | explicit | `manage_asset.add_pixel_depth` |
| `material.add_reflection_vector` | `manage_asset` | `add_reflection_vector` | write | write | explicit | `manage_asset.add_reflection_vector` |
| `material.add_rotator` | `manage_asset` | `add_rotator` | write | write | explicit | `manage_asset.add_rotator` |
| `material.add_scalar_parameter` | `manage_asset` | `add_scalar_parameter` | write | write | explicit | `manage_asset.add_scalar_parameter` |
| `material.add_static_switch_parameter` | `manage_asset` | `add_static_switch_parameter` | write | write | explicit | `manage_asset.add_static_switch_parameter` |
| `material.add_switch` | `manage_asset` | `add_switch` | write | write | explicit | `manage_asset.add_switch` |
| `material.add_texture_coordinate` | `manage_asset` | `add_texture_coordinate` | write | write | explicit | `manage_asset.add_texture_coordinate` |
| `material.add_texture_sample` | `manage_asset` | `add_texture_sample` | write | write | explicit | `manage_asset.add_texture_sample` |
| `material.add_vector_parameter` | `manage_asset` | `add_vector_parameter` | write | write | explicit | `manage_asset.add_vector_parameter` |
| `material.add_vertex_normal` | `manage_asset` | `add_vertex_normal` | write | write | explicit | `manage_asset.add_vertex_normal` |
| `material.add_voronoi` | `manage_asset` | `add_voronoi` | write | write | explicit | `manage_asset.add_voronoi` |
| `material.add_world_position` | `manage_asset` | `add_world_position` | write | write | explicit | `manage_asset.add_world_position` |
| `material.break_material_connections` | `manage_asset` | `break_material_connections` | write | write | explicit | `manage_asset.break_material_connections` |
| `material.compile_material` | `manage_asset` | `compile_material` | write | write | explicit | `manage_asset.compile_material` |
| `material.configure_layer_blend` | `manage_asset` | `configure_layer_blend` | write | write | explicit | `manage_asset.configure_layer_blend` |
| `material.connect_material_pins` | `manage_asset` | `connect_material_pins` | write | write | explicit | `manage_asset.connect_material_pins` |
| `material.connect_nodes` | `manage_asset` | `connect_nodes` | write | write | explicit | `manage_asset.connect_nodes` |
| `material.create_decal_material` | `manage_asset` | `create_decal_material` | write | write | explicit | `manage_asset.create_decal_material` |
| `material.create_landscape_material` | `manage_asset` | `create_landscape_material` | write | write | explicit | `manage_asset.create_landscape_material` |
| `material.create_material` | `manage_asset` | `create_material` | write | write | explicit | `manage_asset.create_material` |
| `material.create_material_function` | `manage_asset` | `create_material_function` | write | write | explicit | `manage_asset.create_material_function` |
| `material.create_material_instance` | `manage_asset` | `create_material_instance` | write | write | explicit | `manage_asset.create_material_instance` |
| `material.create_post_process_material` | `manage_asset` | `create_post_process_material` | write | write | explicit | `manage_asset.create_post_process_material` |
| `material.delete_node` | `manage_asset` | `delete_node` | write | write | explicit | `manage_asset.delete_node` |
| `material.disconnect_nodes` | `manage_asset` | `disconnect_nodes` | write | write | explicit | `manage_asset.disconnect_nodes` |
| `material.find_node` | `manage_asset` | `find_node` | read | read | none | `manage_asset.find_node` |
| `material.get_connected_subgraph` | `manage_asset` | `get_connected_subgraph` | read | read | none | `manage_asset.get_connected_subgraph` |
| `material.get_material_function_info` | `manage_asset` | `get_material_function_info` | read | read | none | `manage_asset.get_material_function_info` |
| `material.get_material_info` | `manage_asset` | `get_material_info` | read | read | none | `manage_asset.get_material_info` |
| `material.get_material_node_details` | `manage_asset` | `get_material_node_details` | read | read | none | `manage_asset.get_material_node_details` |
| `material.get_node_chain` | `manage_asset` | `get_node_chain` | read | read | none | `manage_asset.get_node_chain` |
| `material.get_node_connections` | `manage_asset` | `get_node_connections` | read | read | none | `manage_asset.get_node_connections` |
| `material.get_node_properties` | `manage_asset` | `get_node_properties` | read | read | none | `manage_asset.get_node_properties` |
| `material.rebuild_material` | `manage_asset` | `rebuild_material` | write | write | explicit | `manage_asset.rebuild_material` |
| `material.remove_material_node` | `manage_asset` | `remove_material_node` | write | write | explicit | `manage_asset.remove_material_node` |
| `material.set_blend_mode` | `manage_asset` | `set_blend_mode` | write | write | explicit | `manage_asset.set_blend_mode` |
| `material.set_material_domain` | `manage_asset` | `set_material_domain` | write | write | explicit | `manage_asset.set_material_domain` |
| `material.set_material_parameter` | `manage_asset` | `set_material_parameter` | write | write | explicit | `manage_asset.set_material_parameter` |
| `material.set_scalar_parameter_value` | `manage_asset` | `set_scalar_parameter_value` | write | write | explicit | `manage_asset.set_scalar_parameter_value` |
| `material.set_shading_model` | `manage_asset` | `set_shading_model` | write | write | explicit | `manage_asset.set_shading_model` |
| `material.set_static_switch_parameter_value` | `manage_asset` | `set_static_switch_parameter_value` | write | write | explicit | `manage_asset.set_static_switch_parameter_value` |
| `material.set_texture_parameter_value` | `manage_asset` | `set_texture_parameter_value` | write | write | explicit | `manage_asset.set_texture_parameter_value` |
| `material.set_two_sided` | `manage_asset` | `set_two_sided` | write | write | explicit | `manage_asset.set_two_sided` |
| `material.set_vector_parameter_value` | `manage_asset` | `set_vector_parameter_value` | write | write | explicit | `manage_asset.set_vector_parameter_value` |
| `material.update_custom_expression` | `manage_asset` | `update_custom_expression` | write | write | explicit | `manage_asset.update_custom_expression` |
| `material.use_material_function` | `manage_asset` | `use_material_function` | write | write | explicit | `manage_asset.use_material_function` |
| `sequence.add_actor` | `manage_sequence` | `add_actor` | write | write | none | `manage_sequence.add_actor` |
| `sequence.add_actors` | `manage_sequence` | `add_actors` | write | write | none | `manage_sequence.add_actors` |
| `sequence.add_camera` | `manage_sequence` | `add_camera` | write | write | none | `manage_sequence.add_camera` |
| `sequence.add_keyframe` | `manage_sequence` | `add_keyframe` | write | write | none | `manage_sequence.add_keyframe` |
| `sequence.add_section` | `manage_sequence` | `add_section` | write | write | none | `manage_sequence.add_section` |
| `sequence.add_spawnable_from_class` | `manage_sequence` | `add_spawnable_from_class` | write | write | none | `manage_sequence.add_spawnable_from_class` |
| `sequence.add_track` | `manage_sequence` | `add_track` | write | write | none | `manage_sequence.add_track` |
| `sequence.cinematic.add_camera_cut_track` | `manage_sequence` | `add_camera_cut_track` | write | write | none | `manage_sequence.add_camera_cut_track` |
| `sequence.cinematic.add_camera_shake_track` | `manage_sequence` | `add_camera_shake_track` | write | write | none | `manage_sequence.add_camera_shake_track` |
| `sequence.cinematic.add_event_track` | `manage_sequence` | `add_event_track` | write | write | none | `manage_sequence.add_event_track` |
| `sequence.cinematic.add_fade_track` | `manage_sequence` | `add_fade_track` | write | write | none | `manage_sequence.add_fade_track` |
| `sequence.cinematic.add_level_visibility_track` | `manage_sequence` | `add_level_visibility_track` | write | write | none | `manage_sequence.add_level_visibility_track` |
| `sequence.cinematic.add_material_parameter_track` | `manage_sequence` | `add_material_parameter_track` | write | write | none | `manage_sequence.add_material_parameter_track` |
| `sequence.cinematic.add_particle_track` | `manage_sequence` | `add_particle_track` | write | write | none | `manage_sequence.add_particle_track` |
| `sequence.cinematic.add_property_track` | `manage_sequence` | `add_property_track` | write | write | none | `manage_sequence.add_property_track` |
| `sequence.cinematic.add_shot_track` | `manage_sequence` | `add_shot_track` | write | write | none | `manage_sequence.add_shot_track` |
| `sequence.cinematic.add_skeletal_animation_track` | `manage_sequence` | `add_skeletal_animation_track` | write | write | none | `manage_sequence.add_skeletal_animation_track` |
| `sequence.cinematic.add_subsequence` | `manage_sequence` | `add_subsequence` | write | write | none | `manage_sequence.add_subsequence` |
| `sequence.cinematic.add_transform_track` | `manage_sequence` | `add_transform_track` | write | write | none | `manage_sequence.add_transform_track` |
| `sequence.cinematic.configure_camera_rig_crane` | `manage_sequence` | `configure_camera_rig_crane` | write | write | none | `manage_sequence.configure_camera_rig_crane` |
| `sequence.cinematic.configure_camera_rig_rail` | `manage_sequence` | `configure_camera_rig_rail` | write | write | none | `manage_sequence.configure_camera_rig_rail` |
| `sequence.cinematic.configure_camera_settings` | `manage_sequence` | `configure_camera_settings` | write | write | none | `manage_sequence.configure_camera_settings` |
| `sequence.cinematic.configure_shot_settings` | `manage_sequence` | `configure_shot_settings` | write | write | none | `manage_sequence.configure_shot_settings` |
| `sequence.cinematic.create_cine_camera_actor` | `manage_sequence` | `create_cine_camera_actor` | write | write | none | `manage_sequence.create_cine_camera_actor` |
| `sequence.cinematic.create_master_sequence` | `manage_sequence` | `create_master_sequence` | write | write | none | `manage_sequence.create_master_sequence` |
| `sequence.create` | `manage_sequence` | `create` | write | write | none | `manage_sequence.create` |
| `sequence.delete` | `manage_sequence` | `delete` | destructive | destructive | explicit | `manage_sequence.delete` |
| `sequence.duplicate` | `manage_sequence` | `duplicate` | write | write | none | `manage_sequence.duplicate` |
| `sequence.get_bindings` | `manage_sequence` | `get_bindings` | read | read | none | `manage_sequence.get_bindings` |
| `sequence.get_metadata` | `manage_sequence` | `get_metadata` | read | read | none | `manage_sequence.get_metadata` |
| `sequence.get_properties` | `manage_sequence` | `get_properties` | read | read | none | `manage_sequence.get_properties` |
| `sequence.list` | `manage_sequence` | `list` | read | read | none | `manage_sequence.list` |
| `sequence.list_track_types` | `manage_sequence` | `list_track_types` | read | read | none | `manage_sequence.list_track_types` |
| `sequence.list_tracks` | `manage_sequence` | `list_tracks` | read | read | none | `manage_sequence.list_tracks` |
| `sequence.media.create_media_player` | `manage_sequence` | `create_media_player` | write | write | none | `manage_sequence.create_media_player` |
| `sequence.media.create_media_playlist` | `manage_sequence` | `create_media_playlist` | write | write | none | `manage_sequence.create_media_playlist` |
| `sequence.media.create_media_sound_component` | `manage_sequence` | `create_media_sound_component` | write | write | none | `manage_sequence.create_media_sound_component` |
| `sequence.media.create_media_source` | `manage_sequence` | `create_media_source` | write | write | none | `manage_sequence.create_media_source` |
| `sequence.media.create_media_texture` | `manage_sequence` | `create_media_texture` | write | write | none | `manage_sequence.create_media_texture` |
| `sequence.media.pause_media` | `manage_sequence` | `pause_media` | write | write | none | `manage_sequence.pause_media` |
| `sequence.media.play_media` | `manage_sequence` | `play_media` | write | write | none | `manage_sequence.play_media` |
| `sequence.media.seek_media` | `manage_sequence` | `seek_media` | write | write | none | `manage_sequence.seek_media` |
| `sequence.mrq.add_render_pass` | `manage_sequence` | `add_render_pass` | write | write | none | `manage_sequence.add_render_pass` |
| `sequence.mrq.configure_anti_aliasing` | `manage_sequence` | `configure_anti_aliasing` | write | write | none | `manage_sequence.configure_anti_aliasing` |
| `sequence.mrq.configure_burn_ins` | `manage_sequence` | `configure_burn_ins` | write | write | none | `manage_sequence.configure_burn_ins` |
| `sequence.mrq.configure_console_variables` | `manage_sequence` | `configure_console_variables` | write | write | none | `manage_sequence.configure_console_variables` |
| `sequence.mrq.configure_output_settings` | `manage_sequence` | `configure_output_settings` | write | write | none | `manage_sequence.configure_output_settings` |
| `sequence.mrq.create_render_job` | `manage_sequence` | `create_render_job` | write | write | none | `manage_sequence.create_render_job` |
| `sequence.mrq.queue_render` | `manage_sequence` | `queue_render` | write | write | none | `manage_sequence.queue_render` |
| `sequence.mrq.start_render` | `manage_sequence` | `start_render` | write | write | none | `manage_sequence.start_render` |
| `sequence.open` | `manage_sequence` | `open` | read | read | none | `manage_sequence.open` |
| `sequence.pause` | `manage_sequence` | `pause` | write | write | none | `manage_sequence.pause` |
| `sequence.play` | `manage_sequence` | `play` | write | write | none | `manage_sequence.play` |
| `sequence.remove_actors` | `manage_sequence` | `remove_actors` | write | write | none | `manage_sequence.remove_actors` |
| `sequence.remove_track` | `manage_sequence` | `remove_track` | destructive | destructive | explicit | `manage_sequence.remove_track` |
| `sequence.rename` | `manage_sequence` | `rename` | write | write | none | `manage_sequence.rename` |
| `sequence.replay.configure_demo_settings` | `manage_sequence` | `configure_demo_settings` | write | write | none | `manage_sequence.configure_demo_settings` |
| `sequence.replay.configure_killcam_duration` | `manage_sequence` | `configure_killcam_duration` | write | write | none | `manage_sequence.configure_killcam_duration` |
| `sequence.replay.pause_demo` | `manage_sequence` | `pause_demo` | write | write | none | `manage_sequence.pause_demo` |
| `sequence.replay.play_demo` | `manage_sequence` | `play_demo` | write | write | none | `manage_sequence.play_demo` |
| `sequence.replay.seek_demo` | `manage_sequence` | `seek_demo` | write | write | none | `manage_sequence.seek_demo` |
| `sequence.replay.set_demo_playback_speed` | `manage_sequence` | `set_demo_playback_speed` | write | write | none | `manage_sequence.set_demo_playback_speed` |
| `sequence.replay.start_demo_recording` | `manage_sequence` | `start_demo_recording` | write | write | none | `manage_sequence.start_demo_recording` |
| `sequence.replay.start_killcam` | `manage_sequence` | `start_killcam` | write | write | none | `manage_sequence.start_killcam` |
| `sequence.replay.stop_demo_recording` | `manage_sequence` | `stop_demo_recording` | write | write | none | `manage_sequence.stop_demo_recording` |
| `sequence.set_display_rate` | `manage_sequence` | `set_display_rate` | write | write | none | `manage_sequence.set_display_rate` |
| `sequence.set_metadata` | `manage_sequence` | `set_metadata` | write | write | none | `manage_sequence.set_metadata` |
| `sequence.set_playback_speed` | `manage_sequence` | `set_playback_speed` | write | write | none | `manage_sequence.set_playback_speed` |
| `sequence.set_properties` | `manage_sequence` | `set_properties` | write | write | none | `manage_sequence.set_properties` |
| `sequence.set_tick_resolution` | `manage_sequence` | `set_tick_resolution` | write | write | none | `manage_sequence.set_tick_resolution` |
| `sequence.set_track_locked` | `manage_sequence` | `set_track_locked` | write | write | none | `manage_sequence.set_track_locked` |
| `sequence.set_track_muted` | `manage_sequence` | `set_track_muted` | write | write | none | `manage_sequence.set_track_muted` |
| `sequence.set_track_solo` | `manage_sequence` | `set_track_solo` | write | write | none | `manage_sequence.set_track_solo` |
| `sequence.set_view_range` | `manage_sequence` | `set_view_range` | write | write | none | `manage_sequence.set_view_range` |
| `sequence.set_work_range` | `manage_sequence` | `set_work_range` | write | write | none | `manage_sequence.set_work_range` |
| `sequence.stop` | `manage_sequence` | `stop` | write | write | none | `manage_sequence.stop` |
| `sequence.take.configure_recorded_tracks` | `manage_sequence` | `configure_recorded_tracks` | write | write | none | `manage_sequence.configure_recorded_tracks` |
| `sequence.take.configure_take_sources` | `manage_sequence` | `configure_take_sources` | write | write | none | `manage_sequence.configure_take_sources` |
| `sequence.take.create_take_recorder_panel` | `manage_sequence` | `create_take_recorder_panel` | write | write | none | `manage_sequence.create_take_recorder_panel` |
| `sequence.take.start_recording` | `manage_sequence` | `start_recording` | write | write | none | `manage_sequence.start_recording` |
| `sequence.take.stop_recording` | `manage_sequence` | `stop_recording` | write | write | none | `manage_sequence.stop_recording` |
| `struct.add_struct_member` | `manage_asset` | `add_struct_member` | write | write | explicit | `manage_asset.add_struct_member` |
| `struct.compare_structs` | `manage_asset` | `compare_structs` | read | read | none | `manage_asset.compare_structs` |
| `struct.create_struct` | `manage_asset` | `create_struct` | write | write | explicit | `manage_asset.create_struct` |
| `struct.delete_struct` | `manage_asset` | `delete_struct` | destructive | destructive | elevated | `manage_asset.delete_struct` |
| `struct.duplicate_struct` | `manage_asset` | `duplicate_struct` | write | write | explicit | `manage_asset.duplicate_struct` |
| `struct.export_struct` | `manage_asset` | `export_struct` | read | read | none | `manage_asset.export_struct` |
| `struct.get_instanced_struct_property` | `manage_asset` | `get_instanced_struct_property` | read | read | none | `manage_asset.get_instanced_struct_property` |
| `struct.get_struct` | `manage_asset` | `get_struct` | read | read | none | `manage_asset.get_struct` |
| `struct.import_struct` | `manage_asset` | `import_struct` | write | write | explicit | `manage_asset.import_struct` |
| `struct.list_struct_members` | `manage_asset` | `list_struct_members` | read | read | none | `manage_asset.list_struct_members` |
| `struct.list_structs` | `manage_asset` | `list_structs` | read | read | none | `manage_asset.list_structs` |
| `struct.read_struct` | `manage_asset` | `read_struct` | read | read | none | `manage_asset.read_struct` |
| `struct.recompile_struct` | `manage_asset` | `recompile_struct` | write | write | explicit | `manage_asset.recompile_struct` |
| `struct.refresh_struct_dependencies` | `manage_asset` | `refresh_struct_dependencies` | write | write | explicit | `manage_asset.refresh_struct_dependencies` |
| `struct.remove_struct_member` | `manage_asset` | `remove_struct_member` | destructive | destructive | elevated | `manage_asset.remove_struct_member` |
| `struct.rename_struct` | `manage_asset` | `rename_struct` | write | write | explicit | `manage_asset.rename_struct` |
| `struct.rename_struct_member` | `manage_asset` | `rename_struct_member` | write | write | explicit | `manage_asset.rename_struct_member` |
| `struct.reorder_struct_members` | `manage_asset` | `reorder_struct_members` | write | write | explicit | `manage_asset.reorder_struct_members` |
| `struct.search_struct_usage` | `manage_asset` | `search_struct_usage` | read | read | none | `manage_asset.search_struct_usage` |
| `struct.set_instanced_struct_property` | `manage_asset` | `set_instanced_struct_property` | write | write | explicit | `manage_asset.set_instanced_struct_property` |
| `struct.set_struct_member_default` | `manage_asset` | `set_struct_member_default` | write | write | explicit | `manage_asset.set_struct_member_default` |
| `struct.set_struct_member_metadata` | `manage_asset` | `set_struct_member_metadata` | write | write | explicit | `manage_asset.set_struct_member_metadata` |
| `struct.set_struct_member_type` | `manage_asset` | `set_struct_member_type` | write | write | explicit | `manage_asset.set_struct_member_type` |
| `system_control.add_widget_child` | `system_control` | `manage_widget_authoring` | write | write | none | `system_control.add_widget_child` |
| `system_control.analyze_trace` | `system_control` | `manage_insights` | read | read | none | `system_control.analyze_trace` |
| `system_control.apply_baseline_settings` | `system_control` | `apply_baseline_settings` | write | write | none | `system_control.apply_baseline_settings` |
| `system_control.capture_insights_trace` | `system_control` | `manage_insights` | write | write | none | `system_control.capture_insights_trace` |
| `system_control.configure_lod` | `system_control` | `configure_lod` | write | write | none | `system_control.configure_lod` |
| `system_control.configure_nanite` | `system_control` | `configure_nanite` | write | write | none | `system_control.configure_nanite` |
| `system_control.configure_occlusion_culling` | `system_control` | `configure_occlusion_culling` | write | write | none | `system_control.configure_occlusion_culling` |
| `system_control.configure_texture_streaming` | `system_control` | `configure_texture_streaming` | write | write | none | `system_control.configure_texture_streaming` |
| `system_control.configure_world_partition` | `system_control` | `configure_world_partition` | write | write | none | `system_control.configure_world_partition` |
| `system_control.console_command` | `system_control` | `console_command` | write | write | none | `system_control.console_command` |
| `system_control.create_widget` | `system_control` | `manage_widget_authoring` | write | write | none | `system_control.create_widget` |
| `system_control.enable_gpu_timing` | `system_control` | `manage_performance` | write | write | none | `system_control.enable_gpu_timing` |
| `system_control.execute_command` | `system_control` | `console_command` | write | write | none | `system_control.execute_command` |
| `system_control.execute_python` | `system_control` | `system_control` | write | write | none | `system_control.execute_python` |
| `system_control.generate_memory_report` | `system_control` | `generate_memory_report` | read | read | none | `system_control.generate_memory_report` |
| `system_control.get_project_settings` | `system_control` | `system_control` | read | read | none | `system_control.get_project_settings` |
| `system_control.get_trace_status` | `system_control` | `manage_insights` | read | read | none | `system_control.get_trace_status` |
| `system_control.lumen_update_scene` | `system_control` | `manage_render` | write | write | none | `system_control.lumen_update_scene` |
| `system_control.merge_actors` | `system_control` | `merge_actors` | write | write | none | `system_control.merge_actors` |
| `system_control.optimize_draw_calls` | `system_control` | `optimize_draw_calls` | write | write | none | `system_control.optimize_draw_calls` |
| `system_control.optimize_shaders` | `system_control` | `optimize_shaders` | write | write | none | `system_control.optimize_shaders` |
| `system_control.pause_session` | `system_control` | `manage_insights` | write | write | none | `system_control.pause_session` |
| `system_control.play_sound` | `system_control` | `play_sound_2d` | write | write | none | `system_control.play_sound` |
| `system_control.profile` | `system_control` | `console_command` | write | write | none | `system_control.profile` |
| `system_control.resume_session` | `system_control` | `manage_insights` | write | write | none | `system_control.resume_session` |
| `system_control.run_benchmark` | `system_control` | `run_benchmark` | write | write | none | `system_control.run_benchmark` |
| `system_control.run_tests` | `system_control` | `manage_tests` | write | write | none | `system_control.run_tests` |
| `system_control.run_ubt` | `system_control` | `manage_pipeline` | write | write | none | `system_control.run_ubt` |
| `system_control.screenshot` | `system_control` | `control_editor` | read | read | none | `system_control.screenshot` |
| `system_control.send_snapshot` | `system_control` | `manage_insights` | write | write | none | `system_control.send_snapshot` |
| `system_control.set_cvar` | `system_control` | `console_command` | write | write | none | `system_control.set_cvar` |
| `system_control.set_frame_rate_limit` | `system_control` | `set_frame_rate_limit` | write | write | none | `system_control.set_frame_rate_limit` |
| `system_control.set_fullscreen` | `system_control` | `console_command` | write | write | none | `system_control.set_fullscreen` |
| `system_control.set_project_setting` | `system_control` | `system_control` | write | write | none | `system_control.set_project_setting` |
| `system_control.set_quality` | `system_control` | `console_command` | write | write | none | `system_control.set_quality` |
| `system_control.set_resolution` | `system_control` | `console_command` | write | write | none | `system_control.set_resolution` |
| `system_control.set_resolution_scale` | `system_control` | `set_resolution_scale` | write | write | none | `system_control.set_resolution_scale` |
| `system_control.set_scalability` | `system_control` | `set_scalability` | write | write | none | `system_control.set_scalability` |
| `system_control.set_vsync` | `system_control` | `set_vsync` | write | write | none | `system_control.set_vsync` |
| `system_control.show_fps` | `system_control` | `console_command` | write | write | none | `system_control.show_fps` |
| `system_control.show_stats` | `system_control` | `show_stats` | write | write | none | `system_control.show_stats` |
| `system_control.show_widget` | `system_control` | `manage_widget_authoring` | write | write | none | `system_control.show_widget` |
| `system_control.spawn_category` | `system_control` | `manage_debug` | write | write | none | `system_control.spawn_category` |
| `system_control.start_profiling` | `system_control` | `start_profiling` | write | write | none | `system_control.start_profiling` |
| `system_control.start_session` | `system_control` | `manage_insights` | write | write | none | `system_control.start_session` |
| `system_control.start_unreal_insights` | `system_control` | `manage_insights` | write | write | none | `system_control.start_unreal_insights` |
| `system_control.stop_profiling` | `system_control` | `stop_profiling` | write | write | none | `system_control.stop_profiling` |
| `system_control.stop_session` | `system_control` | `manage_insights` | write | write | none | `system_control.stop_session` |
| `system_control.subscribe` | `system_control` | `manage_logs` | write | write | none | `system_control.subscribe` |
| `system_control.unsubscribe` | `system_control` | `manage_logs` | write | write | none | `system_control.unsubscribe` |
| `system_control.validate_assets` | `system_control` | `system_control` | read | read | none | `system_control.validate_assets` |
| `system_control.write_snapshot` | `system_control` | `manage_insights` | write | write | none | `system_control.write_snapshot` |
| `texture.adjust_curves` | `manage_asset` | `adjust_curves` | write | write | explicit | `manage_asset.adjust_curves` |
| `texture.adjust_levels` | `manage_asset` | `adjust_levels` | write | write | explicit | `manage_asset.adjust_levels` |
| `texture.blur` | `manage_asset` | `blur` | write | write | explicit | `manage_asset.blur` |
| `texture.channel_extract` | `manage_asset` | `channel_extract` | write | write | explicit | `manage_asset.channel_extract` |
| `texture.channel_pack` | `manage_asset` | `channel_pack` | write | write | explicit | `manage_asset.channel_pack` |
| `texture.combine_textures` | `manage_asset` | `combine_textures` | write | write | explicit | `manage_asset.combine_textures` |
| `texture.configure_virtual_texture` | `manage_asset` | `configure_virtual_texture` | write | write | explicit | `manage_asset.configure_virtual_texture` |
| `texture.create_ao_from_mesh` | `manage_asset` | `create_ao_from_mesh` | write | write | explicit | `manage_asset.create_ao_from_mesh` |
| `texture.create_gradient_texture` | `manage_asset` | `create_gradient_texture` | write | write | explicit | `manage_asset.create_gradient_texture` |
| `texture.create_noise_texture` | `manage_asset` | `create_noise_texture` | write | write | explicit | `manage_asset.create_noise_texture` |
| `texture.create_normal_from_height` | `manage_asset` | `create_normal_from_height` | write | write | explicit | `manage_asset.create_normal_from_height` |
| `texture.create_pattern_texture` | `manage_asset` | `create_pattern_texture` | write | write | explicit | `manage_asset.create_pattern_texture` |
| `texture.desaturate` | `manage_asset` | `desaturate` | write | write | explicit | `manage_asset.desaturate` |
| `texture.get_texture_info` | `manage_asset` | `get_texture_info` | read | read | none | `manage_asset.get_texture_info` |
| `texture.invert` | `manage_asset` | `invert` | write | write | explicit | `manage_asset.invert` |
| `texture.resize_texture` | `manage_asset` | `resize_texture` | write | write | explicit | `manage_asset.resize_texture` |
| `texture.set_compression_settings` | `manage_asset` | `set_compression_settings` | write | write | explicit | `manage_asset.set_compression_settings` |
| `texture.set_lod_bias` | `manage_asset` | `set_lod_bias` | write | write | explicit | `manage_asset.set_lod_bias` |
| `texture.set_streaming_priority` | `manage_asset` | `set_streaming_priority` | write | write | explicit | `manage_asset.set_streaming_priority` |
| `texture.set_texture_group` | `manage_asset` | `set_texture_group` | write | write | explicit | `manage_asset.set_texture_group` |
| `texture.sharpen` | `manage_asset` | `sharpen` | write | write | explicit | `manage_asset.sharpen` |
