# Unreal Engine MCP Server - Development Roadmap

A comprehensive development plan for the Unreal Engine Model Context Protocol (MCP) server, enabling AI-driven automation for game development, film production, archviz, VR/AR experiences, and virtual production pipelines.

---

## Quick Reference

| Metric | Value |
|--------|-------|
| **Total Phases** | 59 |
| **Estimated Actions** | ~2,850 |
| **Completed Phases** | 37 |
| **In Progress** | Phase 5 (Infrastructure) |
| **Engine Support** | Unreal Engine 5.0 - 5.7 |

---

## Table of Contents

1. [Foundation (Phases 1-5)](#milestone-1-foundation-phases-1-5)
2. [Content Creation (Phases 6-12)](#milestone-2-content-creation-phases-6-12)
3. [Gameplay Systems (Phases 13-18)](#milestone-3-gameplay-systems-phases-13-18)
4. [UI, Networking & Framework (Phases 19-22)](#milestone-4-ui-networking--framework-phases-19-22)
5. [World Building (Phases 23-27)](#milestone-5-world-building-phases-23-27)
6. [Advanced Systems (Phases 28-35)](#milestone-6-advanced-systems-phases-28-35)
7. [Plugin Integration (Phases 36-46)](#milestone-7-plugin-integration-phases-36-46)
8. [Optimization (Phases 47-52)](#milestone-8-context-optimization-phases-47-52)
9. [Production Tools (Phases 53-59)](#milestone-9-production-tools-phases-53-59)

---

# Milestone 1: Foundation (Phases 1-5)

## Phase 1: Architecture & Foundation ✅

**Status**: Complete | **Actions**: ~25

The core architecture enabling all subsequent development.

| Feature | Description |
|---------|-------------|
| Native C++ Bridge | WebSocket plugin replacing Python-based bridge |
| Consolidated Tools | Unified domains (`manage_asset`, `control_actor`, etc.) |
| Modular Server | Specialized subsystems (`ServerSetup`, `HealthMonitor`) |
| WASM Acceleration | Rust/WASM for high-performance JSON parsing |
| Error Handling | Standardized responses, codes, and logging |
| Offline Mode | File-based fallback for project settings |

---

## Phase 2: Graph & Logic Automation ✅

**Status**: Complete | **Actions**: ~30

Full graph editing capabilities across all visual scripting systems.

- [x] **Blueprint Graphs** - Add/remove nodes, pins, properties
- [x] **Material Graphs** - Edit expressions and connections
- [x] **Niagara Graphs** - Edit emitters, modules, parameters
- [x] **Behavior Trees** - Edit AI tasks and decorators
- [x] **Environment Builder** - Unified landscape/foliage/proc-gen

---

## Phase 3: Cinematic & Visual Automation ✅

**Status**: Complete | **Actions**: ~25

Sequencer, audio, and environment authoring.

- [x] **Sequencer** - Level Sequences (create, play, tracks, keys, bindings)
- [x] **Audio** - SoundCues, spatial audio, mixes
- [x] **Landscape** - Sculpting, layer painting, heightmaps
- [x] **Foliage** - Instance painting, procedural spawning

---

## Phase 4: System & Developer Experience ✅

**Status**: Complete | **Actions**: ~20

Developer tooling and engine compatibility.

- [x] **GraphQL API** - Read/write access to assets and actors
- [x] **Pipeline Integration** - Direct UBT execution with streaming
- [x] **Metrics Dashboard** - `ue://health` backed by metrics
- [x] **UE 5.7 Support** - Full compatibility (Control Rig, Subobject Data)

---

## Phase 5: Infrastructure Improvements 🔄

**Status**: In Progress | **Actions**: ~20

Performance and extensibility enhancements.

| Feature | Status | Description |
|---------|--------|-------------|
| Real-time Streaming | Planned | SSE/chunked responses for logs |
| WASM Expansion | Planned | Move graph handlers to Rust |
| Extensibility Framework | Planned | Dynamic handler registry |
| Remote Profiling | Planned | Unreal Insights integration |

---

# Milestone 2: Content Creation (Phases 6-12)

## Phase 6: Geometry & Mesh Creation ✅

**Status**: Complete | **Tool**: `manage_geometry` | **Actions**: 74

Full procedural mesh creation using Geometry Script.

### Capabilities

| Category | Actions |
|----------|---------|
| Primitives | `create_box`, `create_sphere`, `create_cylinder`, `create_cone`, `create_torus`, `create_capsule`, `create_plane`, `create_disc`, `create_stairs`, `create_spiral_stairs`, `create_ring`, `create_ramp`, `create_arch`, `create_pipe` |
| Boolean/CSG | `boolean_union`, `boolean_subtract`, `boolean_intersection`, `boolean_trim`, `self_union` |
| Modeling | `extrude`, `inset`, `outset`, `bevel`, `offset_faces`, `shell`, `chamfer`, `bridge`, `loft`, `sweep`, `revolve`, `mirror`, `array_linear`, `array_radial`, `loop_cut`, `edge_split`, `poke`, `triangulate`, `quadrangulate` |
| Deformers | `bend`, `twist`, `taper`, `noise_deform`, `smooth`, `stretch`, `spherify`, `cylindrify`, `relax` |
| Mesh Processing | `subdivide`, `simplify_mesh`, `remesh_uniform`, `remesh_voxel`, `weld_vertices`, `merge_vertices`, `remove_degenerates`, `fill_holes`, `flip_normals`, `recalculate_normals`, `recompute_tangents` |
| UV Operations | `auto_uv`, `project_uv`, `unwrap_uv`, `pack_uv_islands`, `transform_uvs` |
| Collision | `generate_collision`, `generate_complex_collision`, `simplify_collision` |
| LOD & Output | `generate_lods`, `set_lod_settings`, `set_lod_screen_sizes`, `convert_to_static_mesh`, `convert_to_nanite` |

> **Cross-Reference**: Geometry Collection for destruction → Phase 44.1

---

## Phase 7: Skeletal Mesh & Rigging ✅

**Status**: Complete | **Tool**: `manage_skeleton` | **Actions**: 29

Character rigging and skeletal mesh editing.

### Capabilities

| Category | Actions |
|----------|---------|
| Skeleton | `create_skeleton`, `add_bone`, `remove_bone`, `set_bone_parent`, `rename_bone`, `set_bone_transform`, `create_virtual_bone`, `get_skeleton_info`, `list_bones` |
| Sockets | `create_socket`, `configure_socket`, `list_sockets` |
| Skin Weights | `auto_skin_weights`, `set_vertex_weights`, `normalize_weights`, `prune_weights`, `copy_weights`, `mirror_weights` |
| Physics Asset | `create_physics_asset`, `add_physics_body`, `configure_physics_body`, `add_physics_constraint`, `configure_constraint_limits`, `list_physics_bodies` |
| Cloth (Basic) | `bind_cloth_to_skeletal_mesh`, `assign_cloth_asset_to_mesh` |
| Morph Targets | `create_morph_target`, `set_morph_target_deltas`, `import_morph_targets` |

> **Cross-Reference**: Full cloth simulation → Phase 44.3 (Chaos Cloth)

---

## Phase 8: Advanced Material Authoring ✅

**Status**: Complete | **Tool**: `manage_material_authoring` | **Actions**: 39

Full material creation and shader graph editing.

### Capabilities

| Category | Actions |
|----------|---------|
| Creation | `create_material`, `set_blend_mode`, `set_shading_model`, `set_material_domain` |
| Expressions | `add_texture_sample`, `add_texture_coordinate`, `add_scalar_parameter`, `add_vector_parameter`, `add_static_switch_parameter`, `add_math_node`, `add_world_position`, `add_vertex_normal`, `add_pixel_depth`, `add_fresnel`, `add_reflection_vector`, `add_panner`, `add_rotator`, `add_noise`, `add_voronoi`, `add_if`, `add_switch`, `add_custom_expression` |
| Connections | `connect_nodes`, `disconnect_nodes` |
| Functions | `create_material_function`, `add_function_input`, `add_function_output`, `use_material_function` |
| Instances | `create_material_instance`, `set_scalar_parameter_value`, `set_vector_parameter_value`, `set_texture_parameter_value` |
| Specialized | `create_landscape_material`, `add_landscape_layer`, `configure_layer_blend`, `create_decal_material`, `create_post_process_material` |
| Utilities | `compile_material`, `get_material_info` |

---

## Phase 9: Texture Generation & Processing ✅

**Status**: Complete | **Tool**: `manage_texture` | **Actions**: 21

Procedural texture creation and image processing.

### Capabilities

| Category | Actions |
|----------|---------|
| Procedural | `create_noise_texture`, `create_gradient_texture`, `create_pattern_texture`, `create_normal_from_height`, `create_ao_from_mesh` |
| Processing | `resize_texture`, `adjust_levels`, `adjust_curves`, `blur`, `sharpen`, `invert`, `desaturate`, `channel_pack`, `channel_extract`, `combine_textures` |
| Settings | `set_compression_settings`, `set_texture_group`, `set_lod_bias`, `configure_virtual_texture`, `set_streaming_priority` |
| Utilities | `get_texture_info` |

---

## Phase 10: Complete Animation System ✅

**Status**: Complete | **Tool**: `animation_physics` | **Actions**: 59

Full animation authoring from keyframes to state machines.

### Capabilities

| Category | Actions |
|----------|---------|
| Sequences | `create_animation_sequence`, `set_sequence_length`, `add_bone_track`, `set_bone_key`, `set_curve_key`, `add_notify`, `add_notify_state`, `add_sync_marker`, `set_root_motion_settings`, `set_additive_settings` |
| Montages | `create_montage`, `add_montage_section`, `add_montage_slot`, `set_section_timing`, `add_montage_notify`, `set_blend_in`, `set_blend_out`, `link_sections` |
| Blend Spaces | `create_blend_space_1d`, `create_blend_space_2d`, `add_blend_sample`, `set_axis_settings`, `set_interpolation_settings`, `create_aim_offset`, `add_aim_offset_sample` |
| Anim Blueprints | `create_anim_blueprint`, `add_state_machine`, `add_state`, `add_transition`, `set_transition_rules`, `add_blend_node`, `add_cached_pose`, `add_slot_node`, `add_layered_blend_per_bone`, `set_anim_graph_node_value` |
| Control Rig | `create_control_rig`, `add_control`, `add_rig_unit`, `connect_rig_elements`, `create_pose_library` |
| Retargeting | `create_ik_rig`, `add_ik_chain`, `create_ik_retargeter`, `set_retarget_chain_mapping` |
| Physics | `setup_ragdoll`, `activate_ragdoll`, `configure_vehicle` |

---

## Phase 11: Complete Audio System ✅

**Status**: Complete | **Tool**: `manage_audio` | **Actions**: 50

Full audio authoring including MetaSounds.

### Capabilities

| Category | Actions |
|----------|---------|
| Sound Cues | `create_sound_cue`, `add_cue_node`, `connect_cue_nodes`, `set_cue_attenuation`, `set_cue_concurrency` |
| MetaSounds | `create_metasound`, `add_metasound_node`, `connect_metasound_nodes`, `add_metasound_input`, `add_metasound_output`, `set_metasound_default` |
| Classes/Mixes | `create_sound_class`, `set_class_properties`, `set_class_parent`, `create_sound_mix`, `add_mix_modifier`, `configure_mix_eq` |
| Attenuation | `create_attenuation_settings`, `configure_distance_attenuation`, `configure_spatialization`, `configure_occlusion`, `configure_reverb_send` |
| Dialogue | `create_dialogue_voice`, `create_dialogue_wave`, `set_dialogue_context` |
| Effects | `create_reverb_effect`, `create_source_effect_chain`, `add_source_effect`, `create_submix_effect` |
| Playback | `play_sound_at_location`, `play_sound_2d`, `create_audio_component`, `spawn_sound_at_location`, `create_ambient_sound` |

---

## Phase 12: Complete Niagara VFX System ✅

**Status**: Complete | **Tool**: `manage_effect` | **Actions**: 58

Full Niagara system authoring and VFX.

### Capabilities

| Category | Actions |
|----------|---------|
| Systems | `create_niagara_system`, `create_niagara_emitter`, `add_emitter_to_system`, `set_emitter_properties` |
| Spawn Modules | `add_spawn_rate_module`, `add_spawn_burst_module`, `add_spawn_per_unit_module` |
| Particle Modules | `add_initialize_particle_module`, `add_particle_state_module`, `add_force_module`, `add_velocity_module`, `add_acceleration_module`, `add_size_module`, `add_color_module`, `add_collision_module`, `add_kill_particles_module`, `add_camera_offset_module` |
| Renderers | `add_sprite_renderer_module`, `add_mesh_renderer_module`, `add_ribbon_renderer_module`, `add_light_renderer_module` |
| Data Interfaces | `add_skeletal_mesh_data_interface`, `add_static_mesh_data_interface`, `add_spline_data_interface`, `add_audio_spectrum_data_interface`, `add_collision_query_data_interface` |
| Parameters | `add_user_parameter`, `set_parameter_value`, `bind_parameter_to_source` |
| Events/GPU | `add_event_generator`, `add_event_receiver`, `configure_event_payload`, `enable_gpu_simulation`, `add_simulation_stage` |
| Debug | `debug_shape` (sphere, box, cylinder, line, cone, capsule, arrow, plane) |

---

# Milestone 3: Gameplay Systems (Phases 13-18)

## Phase 13: Gameplay Ability System (GAS) ✅

**Status**: Complete | **Tool**: `manage_gas` | **Actions**: 27

Complete GAS implementation for abilities, effects, and attributes.

### Capabilities

| Category | Actions |
|----------|---------|
| Components | `add_ability_system_component`, `configure_asc` |
| Attributes | `create_attribute_set`, `add_attribute`, `set_attribute_base_value`, `set_attribute_clamping` |
| Abilities | `create_gameplay_ability`, `set_ability_tags`, `set_ability_costs`, `set_ability_cooldown`, `set_ability_targeting`, `add_ability_task`, `set_activation_policy`, `set_instancing_policy` |
| Effects | `create_gameplay_effect`, `set_effect_duration`, `add_effect_modifier`, `set_modifier_magnitude`, `add_effect_execution_calculation`, `add_effect_cue`, `set_effect_stacking`, `set_effect_tags` |
| Cues | `create_gameplay_cue_notify`, `configure_cue_trigger`, `set_cue_effects`, `add_tag_to_asset` |

> **Cross-Reference**: Gameplay Tags → Phase 31.3

---

## Phase 14: Character & Movement System ✅

**Status**: Complete | **Tool**: `manage_character` | **Actions**: 19

Complete character setup with advanced movement.

### Capabilities

| Category | Actions |
|----------|---------|
| Creation | `create_character_blueprint`, `configure_capsule_component`, `configure_mesh_component`, `configure_camera_component` |
| Movement | `configure_movement_speeds`, `configure_jump`, `configure_rotation`, `add_custom_movement_mode`, `configure_nav_movement` |
| Advanced | `setup_mantling`, `setup_vaulting`, `setup_climbing`, `setup_sliding`, `setup_wall_running`, `setup_grappling` |
| Footsteps | `setup_footstep_system`, `map_surface_to_sound`, `configure_footstep_fx` |

> **Cross-Reference**: Physical Materials → Phase 34.5

---

## Phase 15: Combat & Weapons System ✅

**Status**: Complete | **Tool**: `manage_combat` | **Actions**: 31

Complete combat implementation.

### Capabilities

| Category | Actions |
|----------|---------|
| Weapons | `create_weapon_blueprint`, `configure_weapon_mesh`, `configure_weapon_sockets`, `set_weapon_stats` |
| Firing | `configure_hitscan`, `configure_projectile`, `configure_spread_pattern`, `configure_recoil_pattern`, `configure_aim_down_sights` |
| Projectiles | `create_projectile_blueprint`, `configure_projectile_movement`, `configure_projectile_collision`, `configure_projectile_homing` |
| Damage | `create_damage_type`, `configure_damage_execution`, `setup_hitbox_component` |
| Systems | `setup_reload_system`, `setup_ammo_system`, `setup_attachment_system`, `setup_weapon_switching` |
| Effects | `configure_muzzle_flash`, `configure_tracer`, `configure_impact_effects`, `configure_shell_ejection` |
| Melee | `create_melee_trace`, `configure_combo_system`, `create_hit_pause`, `configure_hit_reaction`, `setup_parry_block_system`, `configure_weapon_trails` |

---

## Phase 16: Complete AI System ✅

**Status**: Complete | **Tool**: `manage_ai` | **Actions**: 34

Full AI pipeline with EQS, perception, and smart objects.

### Capabilities

| Category | Actions |
|----------|---------|
| Controller | `create_ai_controller`, `assign_behavior_tree`, `assign_blackboard` |
| Blackboard | `create_blackboard_asset`, `add_blackboard_key`, `set_key_instance_synced` |
| Behavior Tree | `create_behavior_tree`, `add_composite_node`, `add_task_node`, `add_decorator`, `add_service`, `configure_bt_node` |
| EQS | `create_eqs_query`, `add_eqs_generator`, `add_eqs_context`, `add_eqs_test`, `configure_test_scoring` |
| Perception | `add_ai_perception_component`, `configure_sight_config`, `configure_hearing_config`, `configure_damage_sense_config`, `set_perception_team` |
| State Trees | `create_state_tree`, `add_state_tree_state`, `add_state_tree_transition`, `configure_state_tree_task` |
| Smart Objects | `create_smart_object_definition`, `add_smart_object_slot`, `configure_slot_behavior`, `add_smart_object_component` |
| Mass AI | `create_mass_entity_config`, `configure_mass_entity`, `add_mass_spawner` |

---

## Phase 17: Inventory & Items System ✅

**Status**: Complete | **Tool**: `manage_inventory` | **Actions**: 27

Complete inventory and item management.

### Capabilities

| Category | Actions |
|----------|---------|
| Data Assets | `create_item_data_asset`, `set_item_properties`, `create_item_category`, `assign_item_category` |
| Inventory | `create_inventory_component`, `configure_inventory_slots`, `add_inventory_functions`, `configure_inventory_events`, `set_inventory_replication` |
| Pickups | `create_pickup_actor`, `configure_pickup_interaction`, `configure_pickup_respawn`, `configure_pickup_effects` |
| Equipment | `create_equipment_component`, `define_equipment_slots`, `configure_equipment_effects`, `add_equipment_functions`, `configure_equipment_visuals` |
| Loot | `create_loot_table`, `add_loot_entry`, `configure_loot_drop`, `set_loot_quality_tiers` |
| Crafting | `create_crafting_recipe`, `configure_recipe_requirements`, `create_crafting_station`, `add_crafting_component` |

---

## Phase 18: Interaction System ✅

**Status**: Complete | **Tool**: `manage_interaction` | **Actions**: 22

Complete interaction framework.

### Capabilities

| Category | Actions |
|----------|---------|
| Component | `create_interaction_component`, `configure_interaction_trace`, `configure_interaction_widget`, `add_interaction_events` |
| Interactables | `create_interactable_interface`, `create_door_actor`, `configure_door_properties`, `create_switch_actor`, `configure_switch_properties`, `create_chest_actor`, `configure_chest_properties`, `create_lever_actor` |
| Destructibles | `setup_destructible_mesh`, `configure_destruction_levels`, `configure_destruction_effects`, `configure_destruction_damage`, `add_destruction_component` |
| Triggers | `create_trigger_actor`, `configure_trigger_events`, `configure_trigger_filter`, `configure_trigger_response` |

---

# Milestone 4: UI, Networking & Framework (Phases 19-22)

## Phase 19: Complete UI/UX System ✅

**Status**: Complete | **Tool**: `manage_widget_authoring` | **Actions**: 65

Full UMG widget authoring capabilities.

### Capabilities

| Category | Actions |
|----------|---------|
| Creation | `create_widget_blueprint`, `set_widget_parent_class` |
| Layouts | `add_canvas_panel`, `add_horizontal_box`, `add_vertical_box`, `add_overlay`, `add_grid_panel`, `add_uniform_grid`, `add_wrap_box`, `add_scroll_box`, `add_size_box`, `add_scale_box`, `add_border` |
| Widgets | `add_text_block`, `add_rich_text_block`, `add_image`, `add_button`, `add_check_box`, `add_slider`, `add_progress_bar`, `add_text_input`, `add_combo_box`, `add_spin_box`, `add_list_view`, `add_tree_view` |
| Styling | `set_anchor`, `set_alignment`, `set_position`, `set_size`, `set_padding`, `set_z_order`, `set_render_transform`, `set_clipping`, `set_visibility`, `set_style` |
| Bindings | `create_property_binding`, `bind_text`, `bind_visibility`, `bind_color`, `bind_enabled`, `bind_on_clicked`, `bind_on_hovered`, `bind_on_value_changed` |
| Animations | `create_widget_animation`, `add_animation_track`, `add_animation_keyframe`, `set_animation_loop` |
| Templates | `create_main_menu`, `create_pause_menu`, `create_settings_menu`, `create_loading_screen`, `create_hud_widget`, `add_health_bar`, `add_ammo_counter`, `add_minimap`, `add_crosshair`, `add_compass`, `add_interaction_prompt`, `add_objective_tracker`, `add_damage_indicator`, `create_inventory_ui`, `create_dialog_widget`, `create_radial_menu` |

---

## Phase 20: Networking & Multiplayer ✅

**Status**: Complete | **Tool**: `manage_networking` | **Actions**: 27

Complete networking and replication system.

### Capabilities

| Category | Actions |
|----------|---------|
| Replication | `set_property_replicated`, `set_replication_condition`, `configure_net_update_frequency`, `configure_net_priority`, `set_net_dormancy`, `configure_replication_graph` |
| RPCs | `create_rpc_function`, `configure_rpc_validation`, `set_rpc_reliability` |
| Authority | `set_owner`, `set_autonomous_proxy`, `check_has_authority`, `check_is_locally_controlled` |
| Relevancy | `configure_net_cull_distance`, `set_always_relevant`, `set_only_relevant_to_owner` |
| Serialization | `configure_net_serialization`, `set_replicated_using`, `configure_push_model` |
| Prediction | `configure_client_prediction`, `configure_server_correction`, `add_network_prediction_data`, `configure_movement_prediction` |
| Connection | `configure_net_driver`, `set_net_role`, `configure_replicated_movement` |

---

## Phase 21: Game Framework ✅

**Status**: Complete | **Tool**: `manage_game_framework` | **Actions**: 20

Complete game mode and session management.

### Capabilities

| Category | Actions |
|----------|---------|
| Core Classes | `create_game_mode`, `create_game_state`, `create_player_controller`, `create_player_state`, `create_game_instance`, `create_hud_class` |
| Configuration | `set_default_pawn_class`, `set_player_controller_class`, `set_game_state_class`, `set_player_state_class`, `configure_game_rules` |
| Match Flow | `setup_match_states`, `configure_round_system`, `configure_team_system`, `configure_scoring_system`, `configure_spawn_system` |
| Players | `configure_player_start`, `set_respawn_rules`, `configure_spectating` |

---

## Phase 22: Sessions & Local Multiplayer ✅

**Status**: Complete | **Tool**: `manage_sessions` | **Actions**: 16

Session management and split-screen support.

### Capabilities

| Category | Actions |
|----------|---------|
| Sessions | `configure_local_session_settings`, `configure_session_interface` |
| Split-Screen | `configure_split_screen`, `set_split_screen_type`, `add_local_player`, `remove_local_player` |
| LAN | `configure_lan_play`, `host_lan_server`, `join_lan_server` |
| Voice Chat | `enable_voice_chat`, `configure_voice_settings`, `set_voice_channel`, `mute_player`, `set_voice_attenuation`, `configure_push_to_talk` |

---

# Milestone 5: World Building (Phases 23-27)

## Phase 23: World & Level Structure ✅

**Status**: Complete | **Tool**: `manage_level_structure` | **Actions**: 17

Complete level and world management.

### Capabilities

| Category | Actions |
|----------|---------|
| Levels | `create_level`, `create_sublevel`, `configure_level_streaming`, `set_streaming_distance`, `configure_level_bounds` |
| World Partition | `enable_world_partition`, `configure_grid_size`, `create_data_layer`, `assign_actor_to_data_layer`, `configure_hlod_layer`, `create_minimap_volume` |
| Level Blueprint | `open_level_blueprint`, `add_level_blueprint_node`, `connect_level_blueprint_nodes` |
| Instances | `create_level_instance`, `create_packed_level_actor` |

---

## Phase 24: Volumes & Zones ✅

**Status**: Complete | **Tool**: `manage_volumes` | **Actions**: 19

Complete volume and trigger system.

### Capabilities

| Category | Actions |
|----------|---------|
| Triggers | `create_trigger_volume`, `create_trigger_box`, `create_trigger_sphere`, `create_trigger_capsule` |
| Gameplay | `create_blocking_volume`, `create_kill_z_volume`, `create_pain_causing_volume`, `create_physics_volume`, `create_audio_volume`, `create_reverb_volume`, `create_cull_distance_volume`, `create_precomputed_visibility_volume`, `create_lightmass_importance_volume`, `create_nav_mesh_bounds_volume`, `create_nav_modifier_volume`, `create_camera_blocking_volume` |
| Configuration | `set_volume_extent`, `set_volume_properties` |

> **Cross-Reference**: Post Process Volume → Phase 29.5

---

## Phase 25: Navigation System ✅

**Status**: Complete | **Tool**: `manage_navigation` | **Actions**: 12

Complete navigation mesh and pathfinding.

### Capabilities

| Category | Actions |
|----------|---------|
| NavMesh | `configure_nav_mesh_settings`, `set_nav_agent_properties`, `rebuild_navigation` |
| Modifiers | `create_nav_modifier_component`, `set_nav_area_class`, `configure_nav_area_cost` |
| Links | `create_nav_link_proxy`, `configure_nav_link`, `set_nav_link_type`, `create_smart_link`, `configure_smart_link_behavior` |

---

## Phase 26: Spline System ✅

**Status**: Complete | **Tool**: `manage_splines` | **Actions**: 22

Complete spline-based content creation.

### Capabilities

| Category | Actions |
|----------|---------|
| Creation | `create_spline_actor`, `add_spline_point`, `remove_spline_point`, `set_spline_point_position`, `set_spline_point_tangents`, `set_spline_point_rotation`, `set_spline_point_scale`, `set_spline_type` |
| Spline Mesh | `create_spline_mesh_component`, `set_spline_mesh_asset`, `configure_spline_mesh_axis`, `set_spline_mesh_material` |
| Mesh Array | `scatter_meshes_along_spline`, `configure_mesh_spacing`, `configure_mesh_randomization` |
| Templates | `create_road_spline`, `create_river_spline`, `create_fence_spline`, `create_wall_spline`, `create_cable_spline`, `create_pipe_spline` |

---

## Phase 27: PCG Framework ✅

**Status**: Complete | **Tool**: `manage_pcg` | **Actions**: 31

Complete procedural content generation.

### Capabilities

| Category | Actions |
|----------|---------|
| Graphs | `create_pcg_graph`, `create_pcg_subgraph`, `add_pcg_node`, `connect_pcg_pins`, `set_pcg_node_settings` |
| Input | `add_landscape_data_node`, `add_spline_data_node`, `add_volume_data_node`, `add_actor_data_node`, `add_texture_data_node` |
| Samplers | `add_surface_sampler`, `add_mesh_sampler`, `add_spline_sampler`, `add_volume_sampler` |
| Filters | `add_bounds_modifier`, `add_density_filter`, `add_height_filter`, `add_slope_filter`, `add_distance_filter`, `add_bounds_filter`, `add_self_pruning` |
| Operations | `add_transform_points`, `add_project_to_surface`, `add_copy_points`, `add_merge_points` |
| Spawning | `add_static_mesh_spawner`, `add_actor_spawner`, `add_spline_spawner` |
| Execution | `execute_pcg_graph`, `set_pcg_partition_grid_size` |

---

# Milestone 6: Advanced Systems (Phases 28-35)

## Phase 28: Environment & Water Systems ✅

**Status**: Complete | **Tools**: `build_environment`, `manage_water`, `manage_weather` | **Actions**: 27 implemented

Complete environment (landscape, foliage, sky, fog, clouds, weather, water) systems.

### Implemented Capabilities ✅

| Category | Actions |
|----------|---------|
| Landscape | `create_landscape`, `sculpt_landscape`, `paint_landscape_layer`, `modify_heightmap`, `set_landscape_material`, `create_landscape_grass_type` |
| Foliage | `add_foliage_type`, `add_foliage_instances`, `paint_foliage`, `remove_foliage`, `get_foliage_instances`, `create_procedural_foliage` |
| Sky | `configure_sky_atmosphere` (29 properties), `create_sky_atmosphere` |
| Fog | `configure_exponential_height_fog` (29 properties), `create_exponential_height_fog` |
| Clouds | `configure_volumetric_cloud` (25+ properties), `create_volumetric_cloud` |
| Water Bodies | `create_water_body_ocean`, `create_water_body_lake`, `create_water_body_river` |
| Water Config | `configure_water_body` (5 material setters), `configure_water_waves` (15 Gerstner properties) |
| Water Info | `get_water_body_info`, `list_water_bodies` |
| Water Extended | `set_river_depth`, `set_ocean_extent`, `set_water_static_mesh`, `set_river_transitions`, `set_water_zone`, `get_water_surface_info`, `get_wave_info` |
| Weather | `configure_wind`, `create_weather_system`, `configure_rain_particles`, `configure_snow_particles`, `configure_lightning` |

### Planned Capabilities 🔄

| Category | Actions |
|----------|---------|
| Landscape | `import_heightmap`, `export_heightmap`, `create_landscape_layer_info`, `configure_landscape_splines`, `configure_landscape_lod` |
| Foliage | `configure_foliage_mesh`, `configure_foliage_placement`, `configure_foliage_lod`, `configure_foliage_collision` |
| Time of Day | `create_time_of_day_system`, `configure_sun_position`, `configure_light_color_curve`, `configure_sky_color_curve` |
| Water (Extended) | `create_water_body_custom`, `configure_water_collision`, `create_buoyancy_component` |

### Notes
- Water system requires Water Plugin (Experimental) enabled
- Weather system in `manage_weather` tool (wind, rain, snow, lightning)
- All implemented actions use real UE 5.7 setter APIs (no stubs)
- Landscape/foliage handlers support both World Partition and non-partitioned levels
- Actor search uses class AND name filtering for precise targeting

---

## Phase 29: Advanced Lighting & Rendering ✅

**Status**: Complete | **Tools**: `manage_lighting`, `manage_post_process` | **Actions**: 31

Complete lighting and post-processing system.

### Implemented Capabilities

| Category | Actions |
|----------|---------|
| Post-Process Core | `create_post_process_volume`, `configure_pp_blend`, `configure_pp_priority`, `get_post_process_settings` |
| Visual Effects | `configure_bloom`, `configure_dof`, `configure_motion_blur` |
| Color & Lens | `configure_color_grading`, `configure_white_balance`, `configure_vignette`, `configure_chromatic_aberration`, `configure_film_grain`, `configure_lens_flares` |
| Reflections | `create_sphere_reflection_capture`, `create_box_reflection_capture`, `create_planar_reflection`, `recapture_scene` |
| Ray Tracing | `configure_ray_traced_shadows`, `configure_ray_traced_gi`, `configure_ray_traced_reflections`, `configure_ray_traced_ao`, `configure_path_tracing` |
| Scene Capture | `create_scene_capture_2d`, `create_scene_capture_cube`, `capture_scene` |
| Light Channels | `set_light_channel`, `set_actor_light_channel` |
| Lightmass | `configure_lightmass_settings`, `build_lighting_quality`, `configure_indirect_lighting_cache`, `configure_volumetric_lightmap` |

### Notes
- Post-process settings use proper `bOverride_*` flags before setting values
- Ray tracing actions use console variables for configuration
- Reflection captures support `MarkDirtyForRecapture()` for scene updates
- All actions implemented with real UE 5.6/5.7 APIs

---

## Phase 30: Cinematics & Media ✅

**Status**: Complete | **Tools**: `manage_sequencer`, `manage_movie_render`, `manage_media` | **Actions**: 90

Complete sequencer and media capabilities.

### Capabilities

| Category | Actions |
|----------|---------|
| Sequencer Management | `create_master_sequence`, `add_subsequence`, `remove_subsequence`, `get_subsequences`, `list_sequences`, `duplicate_sequence`, `delete_sequence` |
| Shot Tracks | `add_shot_track`, `add_shot`, `remove_shot`, `get_shots` |
| Camera | `create_cine_camera_actor`, `configure_camera_settings`, `add_camera_cut_track`, `add_camera_cut` |
| Actor Binding | `bind_actor`, `unbind_actor`, `get_bindings` |
| Tracks & Sections | `add_track`, `remove_track`, `get_tracks`, `add_section`, `remove_section` (Transform, Animation, Audio, Event, Fade, LevelVisibility, Property) |
| Keyframes | `add_keyframe`, `remove_keyframe`, `get_keyframes` |
| Playback | `set_playback_range`, `get_playback_range`, `set_display_rate`, `get_sequence_info`, `play_sequence`, `pause_sequence`, `stop_sequence`, `scrub_to_time` |
| Movie Render Queue | `create_queue`, `add_job`, `remove_job`, `clear_queue`, `get_queue`, `configure_job`, `configure_output`, `add_render_pass`, `configure_anti_aliasing`, `configure_high_res_settings`, `add_console_variable` |
| Movie Render Execution | `start_render`, `stop_render`, `get_render_status`, `get_render_progress` |
| Media Assets | `create_media_player`, `create_file_media_source`, `create_stream_media_source`, `create_media_texture`, `create_media_playlist` |
| Media Playback | `open_source`, `open_url`, `play`, `pause`, `stop`, `close`, `seek`, `set_rate`, `set_looping`, `get_duration`, `get_time`, `get_state` |
| Media Playlists | `add_to_playlist`, `get_playlist`, `bind_to_texture`, `unbind_from_texture` |

### Implementation Notes
- Full native C++ handlers for all actions (1339 lines sequencer, 666 lines movie render, 887 lines media)
- CineCamera support with filmback presets and focus settings
- MovieRenderPipeline integration with PIE executor
- MediaPlayer with streaming support and texture binding

---

## Phase 31: Data & Persistence ✅

**Status**: Complete | **Tool**: `manage_data` | **Actions**: 35

Complete data management and save systems.

### Capabilities

| Category | Actions |
|----------|---------|
| Data Assets | `create_data_asset`, `create_primary_data_asset`, `get_data_asset_info`, `set_data_asset_property` |
| Data Tables | `create_data_table`, `add_data_table_row`, `remove_data_table_row`, `get_data_table_row`, `get_data_table_rows`, `import_data_table_csv`, `export_data_table_csv`, `empty_data_table` |
| Curve Tables | `create_curve_table`, `add_curve_row`, `get_curve_value`, `import_curve_table_csv`, `export_curve_table_csv` |
| Save System | `create_save_game_blueprint`, `save_game_to_slot`, `load_game_from_slot`, `delete_save_slot`, `does_save_exist`, `get_save_slot_names` |
| Gameplay Tags | `create_gameplay_tag`, `add_native_gameplay_tag`, `request_gameplay_tag`, `check_tag_match`, `create_tag_container`, `add_tag_to_container`, `remove_tag_from_container`, `has_tag`, `get_all_gameplay_tags` |
| Config | `read_config_value`, `write_config_value`, `get_config_section`, `flush_config`, `reload_config` |

> **Cross-Reference**: Gameplay Tags used by GAS → Phase 13

---

## Phase 32: Build & Deployment ✅

**Status**: Complete | **Tool**: `manage_build` | **Actions**: 24

Complete build pipeline and packaging.

### Implemented Capabilities

| Category | Actions |
|----------|---------|
| Build Pipeline | `run_ubt`, `generate_project_files`, `compile_shaders`, `cook_content`, `package_project`, `configure_build_settings`, `get_build_info` |
| Platform Config | `configure_platform`, `get_platform_settings`, `get_target_platforms` |
| Asset Validation | `validate_assets`, `audit_assets`, `get_asset_size_info`, `get_asset_references` |
| PAK & Chunking | `configure_chunking`, `create_pak_file`, `configure_encryption` |
| Plugin Management | `list_plugins`, `enable_plugin`, `disable_plugin`, `get_plugin_info` |
| DDC Management | `clear_ddc`, `get_ddc_stats`, `configure_ddc` |

### Implementation Notes
- Uses native UE 5.7 APIs (IPluginManager, IProjectManager, DDC interfaces)
- External processes (UAT, UBT) launched via FPlatformProcess::CreateProc()
- Plugin operations modify .uproject file directly
- DDC operations use GetDerivedDataCache() singleton

---

## Phase 33: Testing & Quality ✅

**Status**: Complete | **Tool**: `manage_testing` | **Actions**: 23

Complete testing and profiling infrastructure.

### Implemented Capabilities

| Category | Actions |
|----------|---------|
| Automation Tests | `list_tests`, `run_tests`, `run_test`, `get_test_results`, `get_test_info` |
| Functional Tests | `list_functional_tests`, `run_functional_test`, `get_functional_test_results` |
| Profiling - Trace | `start_trace`, `stop_trace`, `get_trace_status` |
| Profiling - Visual Logger | `enable_visual_logger`, `disable_visual_logger`, `get_visual_logger_status` |
| Profiling - Stats | `start_stats_capture`, `stop_stats_capture`, `get_memory_report`, `get_performance_stats` |
| Validation | `validate_asset`, `validate_assets_in_path`, `validate_blueprint`, `check_map_errors`, `fix_redirectors`, `get_redirectors` |

### Implementation Notes
- Uses IAutomationControllerModule for automation test execution
- Visual Logger via FVisualLogger singleton
- Trace via FTraceAuxiliary for Unreal Insights integration
- Asset validation via UEditorValidatorSubsystem and Data Validation module

---

## Phase 34: Editor Utilities ✅

**Status**: Complete | **Tool**: `manage_editor_utilities` | **Actions**: 46

Complete editor automation.

### Implemented Capabilities

| Category | Actions |
|----------|---------|
| Editor Modes | `set_editor_mode`, `configure_editor_preferences`, `set_grid_settings`, `set_snap_settings` |
| Content Browser | `navigate_to_path`, `sync_to_asset`, `create_collection`, `add_to_collection`, `show_in_explorer` |
| Selection | `select_actor`, `select_actors_by_class`, `select_actors_by_tag`, `deselect_all`, `group_actors`, `ungroup_actors`, `get_selected_actors` |
| Collision | `create_collision_channel`, `create_collision_profile`, `configure_channel_responses`, `get_collision_info` |
| Physical Materials | `create_physical_material`, `set_friction`, `set_restitution`, `configure_surface_type`, `get_physical_material_info` |
| Subsystems | `create_game_instance_subsystem`, `create_world_subsystem`, `create_local_player_subsystem`, `get_subsystem_info` |
| Timers | `set_timer`, `clear_timer`, `clear_all_timers`, `get_active_timers` |
| Delegates | `create_event_dispatcher`, `bind_to_event`, `unbind_from_event`, `broadcast_event`, `create_blueprint_interface` |
| Transactions | `begin_transaction`, `end_transaction`, `cancel_transaction`, `undo`, `redo`, `get_transaction_history` |
| Utility | `get_editor_utilities_info` |

### Implementation Notes
- Uses `FEditorModeTools` for mode switching
- Content Browser operations via `FContentBrowserModule`
- Selection via `GEditor->SelectActor()` and `GEditor->GetSelectedActors()`
- Collision channels require `DefaultEngine.ini` modification (guidance provided)
- Physical materials created via `NewObject<UPhysicalMaterial>()`
- Transactions use `GEditor->BeginTransaction()` / `EndTransaction()` for undo support
- Blueprint interfaces created via `FKismetEditorUtilities::CreateBlueprint()`

---

## Phase 35: Additional Gameplay Systems ✅

**Status**: Complete | **Tool**: `manage_gameplay_systems` | **Actions**: 43

Common gameplay patterns and systems.

### Implemented Capabilities

| Category | Actions |
|----------|---------|
| Targeting | `create_targeting_component`, `configure_lock_on_target`, `configure_aim_assist` |
| Checkpoints | `create_checkpoint_actor`, `save_checkpoint`, `load_checkpoint` |
| Objectives | `create_objective`, `set_objective_state`, `configure_objective_markers` |
| World Markers | `create_world_marker`, `create_ping_system`, `configure_marker_widget` |
| Photo Mode | `enable_photo_mode`, `configure_photo_mode_camera`, `take_photo_mode_screenshot` |
| Quest/Dialogue | `create_quest_data_asset`, `create_dialogue_tree`, `add_dialogue_node`, `play_dialogue` |
| Instancing | `create_instanced_static_mesh_component`, `create_hierarchical_instanced_static_mesh`, `add_instance`, `remove_instance`, `get_instance_count` |
| HLOD | `create_hlod_layer`, `configure_hlod_settings`, `build_hlod`, `assign_actor_to_hlod` |
| Localization | `create_string_table`, `add_string_entry`, `get_string_entry`, `import_localization`, `export_localization`, `set_culture`, `get_available_cultures` |
| Scalability | `create_device_profile`, `configure_scalability_group`, `set_quality_level`, `get_scalability_settings`, `set_resolution_scale` |
| Utility | `get_gameplay_systems_info` |

### Implementation Notes
- Targeting uses component tags for configuration (actual targeting logic is blueprint-based)
- Checkpoints integrate with UE's SaveGame system
- Objectives stored as hidden actors with tag-based metadata
- Instancing uses native UInstancedStaticMeshComponent and UHierarchicalInstancedStaticMeshComponent
- HLOD uses UHLODLayer for World Partition integration
- Localization uses UStringTable for string tables
- Scalability uses Scalability::FQualityLevels API

---

# Milestone 7: Plugin Integration (Phases 36-46)

## Phase 36: Character & Avatar Plugins ✅

**Status**: Complete | **Tool**: `manage_character_avatar` | **Actions**: 60

MetaHuman, Ready Player Me, Mutable, Groom.

### Implemented Capabilities

| Category | Actions |
|----------|---------|
| MetaHuman | `import_metahuman`, `spawn_metahuman_actor`, `get_metahuman_component`, `set_body_type`, `set_face_parameter`, `set_skin_tone`, `set_hair_style`, `set_eye_color`, `configure_metahuman_lod`, `enable_body_correctives`, `enable_neck_correctives`, `set_quality_level`, `configure_face_rig`, `set_body_part`, `get_metahuman_info`, `list_available_presets`, `apply_preset`, `export_metahuman_settings` |
| Groom/Hair | `create_groom_asset`, `import_groom`, `create_groom_binding`, `spawn_groom_actor`, `attach_groom_to_skeletal_mesh`, `configure_hair_simulation`, `set_hair_width`, `set_hair_root_scale`, `set_hair_tip_scale`, `set_hair_color`, `configure_hair_physics`, `configure_hair_rendering`, `enable_hair_simulation`, `get_groom_info` |
| Mutable | `create_customizable_object`, `compile_customizable_object`, `create_customizable_instance`, `set_bool_parameter`, `set_int_parameter`, `set_float_parameter`, `set_color_parameter`, `set_vector_parameter`, `set_texture_parameter`, `set_transform_parameter`, `set_projector_parameter`, `update_skeletal_mesh`, `bake_customizable_instance`, `get_parameter_info`, `get_instance_info`, `spawn_customizable_actor` |
| Ready Player Me | `load_avatar_from_url`, `load_avatar_from_glb`, `create_rpm_actor`, `apply_avatar_to_character`, `configure_rpm_materials`, `set_rpm_outfit`, `get_avatar_metadata`, `cache_avatar`, `clear_avatar_cache`, `create_rpm_animation_blueprint`, `retarget_rpm_animation`, `get_rpm_info` |

### Implementation Notes
- Groom/HairStrands uses conditional compilation (`#if MCP_HAS_GROOM`)
- Mutable uses conditional compilation (`#if MCP_HAS_MUTABLE`)
- MetaHuman SDK has limited public API - provides guidance messages for SDK-dependent features
- Ready Player Me integration provides URL/GLB loading guidance
- All actions return success with appropriate messages even when optional plugins unavailable

---

## Phase 37: Asset & Content Plugins ✅

**Status**: Complete | **Tool**: `manage_asset_plugins` | **Actions**: 158

Interchange, USD, Alembic, glTF, Substance, Houdini Engine, SpeedTree, Datasmith, Quixel/Fab.

> **External Dependencies**: Some plugins require external software/accounts:
> - **Quixel/Fab**: Requires Epic Games account (free with UE license)
> - **Substance**: Requires Adobe Substance license
> - **Houdini Engine**: Requires SideFX Houdini license
> - **SpeedTree**: Requires SpeedTree license for authoring (runtime is free)

### Implemented Capabilities

| Plugin | # Actions | Key Actions |
|--------|-----------|-------------|
| Interchange | 18 | `import_with_interchange`, `export_with_interchange`, `create_interchange_pipeline`, `configure_static_mesh_settings`, `configure_skeletal_mesh_settings`, `configure_texture_settings`, `configure_material_settings`, `configure_animation_settings`, `set_pipeline_source`, `get_available_translators`, `register_factory_node`, `configure_scene_import`, `configure_common_pipelines_settings`, `get_interchange_info` |
| USD | 24 | `create_usd_stage`, `open_usd_stage`, `close_usd_stage`, `save_usd_stage`, `create_usd_prim`, `set_usd_prim_attribute`, `get_usd_prim_info`, `add_reference`, `export_actor_to_usd`, `export_level_to_usd`, `import_usd_layer`, `create_usd_layer`, `mute_usd_layer`, `set_edit_target`, `enable_usd_live_edit`, `create_usd_assets_from_prims`, `configure_usd_stage_options`, `get_usd_info` |
| Alembic | 15 | `import_alembic_file`, `set_alembic_import_settings`, `create_geometry_cache_track`, `import_alembic_groom`, `configure_geometry_cache_playback`, `create_geometry_cache_actor`, `sample_geometry_cache_transform`, `configure_alembic_compression`, `export_alembic`, `get_alembic_info` |
| glTF | 16 | `import_gltf_file`, `import_glb_file`, `export_to_gltf`, `export_to_glb`, `configure_gltf_import_settings`, `configure_gltf_export_settings`, `import_gltf_static_mesh`, `import_gltf_skeletal_mesh`, `import_gltf_scene`, `set_draco_compression`, `configure_material_import`, `get_gltf_info` |
| Datasmith | 18 | `import_datasmith_file`, `configure_datasmith_import`, `import_cad_file`, `import_cad_assembly`, `configure_cad_import_options`, `import_revit_file`, `import_sketchup_file`, `import_3ds_max_file`, `import_rhino_file`, `import_archicad_file`, `configure_tessellation`, `configure_lightmap_settings`, `reimport_datasmith`, `get_datasmith_info` |
| SpeedTree | 12 | `import_speedtree_model`, `import_speedtree_9`, `configure_speedtree_wind`, `configure_speedtree_lod`, `configure_speedtree_materials`, `configure_speedtree_collision`, `apply_speedtree_wind_to_material`, `get_speedtree_info` |
| Quixel/Fab | 12 | `connect_to_bridge`, `disconnect_from_bridge`, `import_megascan_surface`, `import_megascan_3d_asset`, `import_megascan_3d_plant`, `configure_megascan_lods`, `apply_megascan_material`, `search_quixel_library`, `download_fab_asset`, `get_quixel_bridge_status`, `get_quixel_info` |
| Houdini Engine | 22 | `import_hda`, `instantiate_hda`, `rebake_hda`, `set_hda_float_parameter`, `set_hda_int_parameter`, `set_hda_string_parameter`, `set_hda_toggle_parameter`, `get_hda_parameter_info`, `cook_hda`, `recook_hda`, `bake_hda_to_actors`, `bake_hda_to_blueprint`, `bake_hda_to_foliage`, `bake_hda_to_landscape`, `set_hda_input_object`, `set_hda_input_landscape`, `set_hda_input_curve`, `clear_hda_input`, `get_hda_output_info`, `rebuild_hda`, `delete_hda_instance`, `get_houdini_info` |
| Substance | 20 | `import_sbsar_file`, `create_substance_graph_instance`, `set_substance_input_float`, `set_substance_input_int`, `set_substance_input_color`, `set_substance_input_image`, `get_substance_input_info`, `render_substance_textures`, `configure_substance_output_size`, `randomize_substance_seed`, `create_material_from_substance`, `configure_substance_output`, `reimport_substance`, `get_substance_info` |

### Implementation Notes
- All 157 actions fully implemented in both TypeScript and C++ handlers
- Conditional compilation (`#if`) for optional plugins (Houdini, Substance)
- Graceful fallback messages when external plugins not installed
- Uses real UE 5.7 APIs for all Interchange, USD, Alembic, glTF, Datasmith operations

---

## Phase 38: Audio Middleware Plugins 🔄

**Status**: Planned | **Actions**: ~80

Wwise, FMOD, Bink Video integration.

> **External Dependencies**: These are commercial audio middleware solutions:
> - **Wwise**: Requires Audiokinetic Wwise license (free tier available)
> - **FMOD**: Requires Firelight Technologies FMOD license (free tier available)
> - **Bink Video**: Included with UE, no additional license required

### Planned Capabilities

| Plugin | Key Actions |
|--------|-------------|
| Wwise | `connect_wwise_project`, `post_wwise_event`, `set_rtpc_value`, `set_wwise_switch`, `configure_room`, `load_soundbank` |
| FMOD | `connect_fmod_project`, `play_fmod_event`, `set_fmod_global_parameter`, `set_fmod_bus_volume`, `load_fmod_bank` |
| Bink Video | `create_bink_media_player`, `open_bink_video`, `play_bink`, `configure_bink_texture` |

---

## Phase 39: Motion Capture & Live Link 🔄

**Status**: Planned | **Actions**: ~70

Live Link core, Face, OptiTrack, Vicon, Rokoko, Xsens.

> **External Dependencies**: These require motion capture hardware/software:
> - **Live Link Face**: Requires iOS device with TrueDepth camera
> - **OptiTrack**: Requires OptiTrack motion capture system
> - **Vicon**: Requires Vicon motion capture system
> - **Rokoko**: Requires Rokoko motion capture suit/gloves
> - **Xsens**: Requires Xsens MVN motion capture system

### Planned Capabilities

| Plugin | Key Actions |
|--------|-------------|
| Live Link Core | `add_livelink_source`, `list_livelink_subjects`, `create_livelink_preset`, `configure_livelink_timecode` |
| Live Link Face | `configure_livelink_face_source`, `configure_arkit_blendshape_mapping`, `set_neutral_pose` |
| OptiTrack | `connect_optitrack_server`, `configure_skeleton_mapping`, `assign_optitrack_to_actor` |
| Vicon | `connect_vicon_datastream`, `get_vicon_subject_data`, `configure_vicon_retargeting` |
| Rokoko | `connect_rokoko_studio`, `assign_rokoko_to_character`, `map_rokoko_blendshapes` |
| Xsens | `connect_xsens_mvn`, `map_xsens_skeleton`, `assign_xsens_to_character` |

---

## Phase 40: Virtual Production Plugins 🔄

**Status**: Planned | **Actions**: ~150

nDisplay, Composure, OCIO, Remote Control, DMX, OSC, MIDI, Timecode.

> **Hardware/Setup Requirements**: Virtual production features require:
> - **nDisplay**: Multi-display cluster, LED walls, or projection systems
> - **DMX**: DMX512 lighting fixtures and interface hardware
> - **Timecode**: Timecode generator (LTC, MTC, or genlock source)
> - **MIDI/OSC**: MIDI controllers or OSC-capable devices (optional)

### Planned Capabilities

| Plugin | Key Actions |
|--------|-------------|
| nDisplay | `create_ndisplay_config`, `add_cluster_node`, `create_viewport`, `create_icvfx_camera`, `configure_genlock` |
| Composure | `create_composure_actor`, `add_composure_layer`, `add_chroma_keyer`, `configure_composure_output` |
| OCIO | `load_ocio_config`, `set_working_color_space`, `configure_viewport_display_transform` |
| Remote Control | `create_remote_control_preset`, `expose_actor_properties`, `start_web_remote_control_server` |
| DMX | `create_dmx_library`, `add_dmx_fixture_type`, `create_dmx_output_port`, `import_gdtf_fixture` |
| OSC | `create_osc_server`, `send_osc_message`, `bind_osc_to_property` |
| MIDI | `list_midi_devices`, `send_midi_note_on`, `bind_midi_to_property` |
| Timecode | `set_timecode_provider`, `configure_genlock_source`, `configure_ltc_input` |

---

## Phase 41: XR Plugins (VR/AR/MR) 🔄

**Status**: Planned | **Actions**: ~140

OpenXR, Meta Quest, SteamVR, Apple ARKit, Google ARCore, Varjo, HoloLens.

> **Hardware Requirements**: These plugins require XR hardware:
> - **OpenXR**: Any OpenXR-compatible headset
> - **Meta Quest**: Meta Quest 2/3/Pro headset
> - **SteamVR**: SteamVR-compatible headset (Valve Index, HTC Vive, etc.)
> - **ARKit**: iOS device with ARKit support (iPhone 6s+, iPad 5th gen+)
> - **ARCore**: Android device with ARCore support
> - **Varjo**: Varjo XR-3/VR-3 headset
> - **HoloLens**: Microsoft HoloLens 2

### Planned Capabilities

| Plugin | Key Actions |
|--------|-------------|
| OpenXR | `configure_openxr_settings`, `configure_tracking_origin`, `create_openxr_action_set`, `trigger_haptic_feedback` |
| Meta Quest | `configure_quest_settings`, `enable_passthrough`, `enable_scene_capture`, `enable_quest_hand_tracking`, `enable_quest_face_tracking` |
| SteamVR | `configure_steamvr_settings`, `configure_chaperone_bounds`, `create_steamvr_overlay` |
| ARKit | `configure_arkit_session`, `configure_world_tracking`, `get_tracked_planes`, `enable_people_occlusion`, `get_face_blendshapes` |
| ARCore | `configure_arcore_session`, `get_arcore_planes`, `enable_geospatial`, `create_geospatial_anchor` |
| Varjo | `configure_varjo_settings`, `enable_varjo_video_passthrough`, `enable_varjo_eye_tracking` |
| HoloLens | `configure_hololens_settings`, `configure_spatial_mapping`, `enable_qr_tracking`, `register_voice_command` |

---

## Phase 42: AI & NPC Plugins 🔄

**Status**: Planned | **Actions**: ~30

> **Note**: These plugins require external cloud services for full functionality. Actions configure the UE-side integration only.

### Planned Capabilities

| Plugin | Key Actions |
|--------|-------------|
| Convai | `create_convai_character`, `configure_character_backstory`, `configure_convai_lipsync` |
| Inworld AI | `create_inworld_character`, `configure_inworld_settings`, `get_character_emotion` |
| NVIDIA ACE | `configure_audio2face`, `process_audio_to_blendshapes`, `configure_blendshape_mapping` |

---

## Phase 43: Utility Plugins 🔄

**Status**: Planned | **Actions**: ~100

Python Scripting, Editor Scripting, Modeling Tools, Common UI, Paper2D, Procedural Mesh, Variant Manager.

### Planned Capabilities

| Plugin | Key Actions |
|--------|-------------|
| Python | `execute_python_script`, `execute_python_file`, `configure_python_paths`, `create_python_editor_utility` |
| Editor Scripting | `create_editor_utility_widget`, `add_menu_entry`, `add_toolbar_button`, `register_editor_command` |
| Modeling Tools | `activate_modeling_tool`, `select_mesh_elements`, `set_sculpt_brush`, `sculpt_stroke` |
| Common UI | `configure_ui_input_config`, `create_common_activatable_widget`, `configure_navigation_rules` |
| Paper2D | `create_sprite`, `create_flipbook`, `create_tile_map`, `spawn_paper_sprite_actor` |
| Procedural Mesh | `create_procedural_mesh_component`, `create_mesh_section`, `set_mesh_vertices`, `convert_to_static_mesh` |
| Variant Manager | `create_variant_set`, `add_variant`, `activate_variant`, `export_variant_configuration` |

---

## Phase 44: Physics & Destruction Plugins 🔄

**Status**: Planned | **Actions**: ~80

Chaos Destruction, Chaos Vehicles, Chaos Cloth, Chaos Flesh.

### Planned Capabilities

| Plugin | Key Actions |
|--------|-------------|
| Chaos Destruction | `create_geometry_collection`, `apply_uniform_fracture`, `apply_radial_fracture`, `configure_damage_threshold`, `create_field_system` |
| Chaos Vehicles | `create_chaos_wheeled_vehicle`, `add_wheel_setup`, `configure_engine_torque_curve`, `configure_transmission` |
| Chaos Cloth | `create_cloth_asset`, `configure_cloth_config`, `paint_max_distance`, `configure_cloth_lod` |
| Chaos Flesh | `create_flesh_asset`, `configure_flesh_simulation`, `configure_flesh_collision` |

---

## Phase 45: Accessibility System 🔄

**Status**: Planned | **Tool**: `manage_accessibility` | **Actions**: ~40

Complete accessibility features for inclusive game design.

### Planned Capabilities

| Category | Actions |
|----------|---------|
| Visual | `create_colorblind_filter`, `configure_high_contrast_mode`, `set_ui_scale`, `configure_text_to_speech` |
| Subtitles | `create_subtitle_widget`, `configure_subtitle_style`, `configure_speaker_identification`, `add_directional_indicators` |
| Audio | `configure_mono_audio`, `configure_audio_visualization`, `create_sound_indicator_widget` |
| Motor | `configure_control_remapping_ui`, `configure_hold_vs_toggle`, `configure_auto_aim_strength`, `configure_one_handed_mode` |
| Cognitive | `configure_difficulty_presets`, `configure_objective_reminders`, `configure_navigation_assistance`, `configure_motion_sickness_options` |
| Presets | `create_accessibility_preset`, `apply_accessibility_preset`, `export_accessibility_settings` |

---

## Phase 46: Modding & UGC System 🔄

**Status**: Planned | **Tool**: `manage_modding` | **Actions**: ~25

Enable mod support and user-generated content.

### Planned Capabilities

| Category | Actions |
|----------|---------|
| Pak Loading | `configure_mod_loading_paths`, `scan_for_mod_paks`, `load_mod_pak`, `unload_mod_pak`, `validate_mod_pak`, `configure_mod_load_order` |
| Discovery | `create_mod_browser_widget`, `list_installed_mods`, `enable_mod`, `disable_mod`, `check_mod_compatibility` |
| Asset Override | `configure_asset_override_paths`, `register_mod_asset_redirect`, `restore_original_asset` |
| SDK Generation | `export_moddable_headers`, `create_mod_template_project`, `configure_exposed_classes` |
| Security | `configure_mod_sandbox`, `set_allowed_mod_operations`, `validate_mod_content` |

---

# Milestone 8: Context Optimization (Phases 47-52)

## Phase 47: Schema Pruning ✅

**Status**: Complete | **Issue**: [#106](https://github.com/ChiR24/Unreal_mcp/issues/106)

**Results**: ~23,000 token reduction

- [x] Remove "Supported actions:" lists from descriptions
- [x] Remove "Use it when you need to:" bullet lists
- [x] Condense descriptions to 1-2 sentences max
- [x] Remove redundant parameter descriptions

---

## Phase 48: Common Schema Extraction ✅

**Status**: Complete | **Issue**: [#108](https://github.com/ChiR24/Unreal_mcp/issues/108)

**Results**: ~8,000 token reduction

- [x] Move repeated parameters to `commonSchemas`
- [x] Extract: `assetPath`, `actorName`, `location`, `rotation`, `scale`, `save`, `overwrite`
- [x] Define `standardResponse` schema
- [x] Update all tools to reference `commonSchemas`

---

## Phase 49: Dynamic Tool Loading ✅

**Status**: Complete | **Issue**: [#109](https://github.com/ChiR24/Unreal_mcp/issues/109)

**Results**: ~50,000 token reduction (with category filtering)

- [x] Add `listChanged: true` to server capabilities
- [x] Add `manage_pipeline` tool with `set_categories`, `list_categories`, `get_status`
- [x] Define categories: `core`, `world`, `authoring`, `gameplay`, `utility`, `all`
- [x] Filter tools by category in `ListToolsRequestSchema` handler
- [x] Backward compatibility for clients without `listChanged` support

---

## Phase 50: Lazy Schema Loading 🔄

**Status**: Planned | **Issue**: [#110](https://github.com/ChiR24/Unreal_mcp/issues/110)

**Goal**: Load tool schemas on-demand to reduce initial context.

| Feature | Description |
|---------|-------------|
| Deferred Loading | Only load full schemas when tool is first invoked |
| Schema Stubs | Minimal schema placeholders for `tools/list` response |
| Cache Warming | Pre-load frequently used tool schemas |
| Memory Optimization | Unload unused schemas after idle timeout |

---

## Phase 51: Action-Level Filtering 🔄

**Status**: Planned | **Issue**: [#112](https://github.com/ChiR24/Unreal_mcp/issues/112)

**Goal**: Filter actions within tools to further reduce context.

| Feature | Description |
|---------|-------------|
| Action Subsets | Enable specific actions within a tool |
| Workflow Profiles | Predefined action sets for common workflows |
| Context Hints | AI can request additional actions as needed |
| Usage Analytics | Track action usage to optimize defaults |

---

## Phase 52: Strategic Tool Merging ✅

**Status**: Complete | **Issue**: [#111](https://github.com/ChiR24/Unreal_mcp/issues/111)

**Results**: Reduced tool count from 38 to 35 (~10,000 token savings)

### Tool Consolidations

| Merged Tool | Into | Actions Added |
|-------------|------|---------------|
| `manage_blueprint_graph` | `manage_blueprint` | 11 graph actions |
| `manage_audio_authoring` | `manage_audio` | 30 authoring actions |
| `manage_niagara_authoring` | `manage_effect` | 36 authoring actions |
| `manage_animation_authoring` | `animation_physics` | 45 authoring actions |

---

# Milestone 9: Production Tools (Phases 53-59)

These phases add production workflow tools: reliability, validation, source control integration, build automation, testing, and security.

---

## Phase 53: Reliability & Determinism 🔄

**Status**: Planned | **Tool**: `system_control` (expanded) | **Actions**: ~20

Make automation safe and repeatable.

### Planned Capabilities

| Category | Actions |
|----------|---------|
| Transactions | `begin_transaction`, `end_transaction`, `cancel_transaction`, `create_change_set`, `export_change_set`, `apply_change_set` |
| Determinism | `set_execution_seed`, `get_execution_seed`, `set_deterministic_mode`, `snapshot_world_state`, `restore_world_state` |
| Crash Hardening | `set_request_timeout_policy`, `get_request_health`, `enable_watchdog`, `safe_mode_enable`, `safe_mode_disable` |
| Audit Logging | `set_audit_log_enabled`, `set_audit_log_path`, `get_last_audit_entries` |

---

## Phase 54: Source Control & Studio Workflow 🔄

**Status**: Planned | **Tool**: `manage_source_control` | **Actions**: ~20

Enable real studio workflows (Perforce/Git).

### Planned Capabilities

| Category | Actions |
|----------|---------|
| Provider | `get_source_control_provider`, `set_source_control_provider`, `connect_source_control`, `get_workspace_status` |
| Changelists | `create_changelist`, `set_active_changelist`, `list_changelists`, `delete_changelist` |
| Files | `checkout_files`, `add_files`, `revert_files`, `revert_unchanged`, `resolve_conflicts` |
| Submission | `submit_changelist`, `shelve_changelist`, `unshelve_changelist`, `get_file_history` |

---

## Phase 55: Asset Validation & Linting 🔄

**Status**: Planned | **Tool**: `manage_validation` | **Actions**: ~20

Enforce quality standards automatically.

### Planned Capabilities

| Category | Actions |
|----------|---------|
| Naming | `lint_asset_naming`, `lint_folder_structure`, `auto_fix_renames` |
| Asset Rules | `validate_lods`, `validate_collision`, `validate_uvs`, `validate_textures`, `validate_materials`, `validate_nanite_policy` |
| Map Health | `run_map_check`, `run_world_partition_validation`, `validate_streaming` |
| Budgets | `set_budget_profile`, `check_budget`, `generate_budget_report` |

---

## Phase 56: Build, Cook, Package & Release Engineering 🔄

**Status**: Planned | **Tool**: `manage_build` (expanded) | **Actions**: ~15

Turn projects into shippable builds reliably.

### Planned Capabilities

| Category | Actions |
|----------|---------|
| Build Graph | `run_uat`, `run_build_graph`, `configure_build_graph` |
| Cooking | `cook_by_the_book`, `cook_on_the_fly`, `package_build` |
| Patch/DLC | `configure_chunk_manifest`, `build_paks`, `build_patch`, `build_dlc` |
| Verification | `run_smoke_tests_on_build`, `generate_release_manifest` |

---

## Phase 57: Automated Testing 🔄

**Status**: Planned | **Tool**: `manage_testing` | **Actions**: ~15

Testing automation: functional tests, performance, crash detection.

### Planned Capabilities

| Category | Actions |
|----------|---------|
| Authoring | `create_functional_test`, `create_automation_test`, `create_perf_test` |
| Execution | `run_automation_tests`, `run_functional_tests`, `run_gauntlet`, `get_test_results` |
| Soak | `run_soak_test`, `monitor_crashes`, `collect_minidumps` |

---

## Phase 58: Security, Integrity & Compliance 🔄

**Status**: Planned | **Tool**: `manage_security` | **Actions**: ~15

Make automation and editor/runtime operations safer.

### Planned Capabilities

| Category | Actions |
|----------|---------|
| Policy | `set_security_policy`, `set_allowed_actions`, `set_blocked_console_commands`, `set_blocked_asset_paths` |
| Integrity | `enable_asset_integrity_checks`, `validate_build_artifacts`, `verify_project_settings` |
| Runtime | `configure_network_validation`, `configure_replay_recording_policy`, `configure_spectator_policy` |

---

## Phase 59: Pipeline Templates & Project Bootstrapping 🔄

**Status**: Planned | **Tool**: `manage_templates` | **Actions**: ~10

Make new project setup reproducible.

### Planned Capabilities

| Category | Actions |
|----------|---------|
| Templates | `create_project_template`, `apply_project_template`, `validate_template` |
| Presets | `create_preset_pack`, `apply_preset_pack`, `export_preset_pack`, `import_preset_pack` |

---

# Summary

## Statistics by Category

| Category | Phases | Estimated Actions |
|----------|--------|-------------------|
| Foundation (1-4) | 4 | ~100 |
| Infrastructure (5) | 1 | ~20 |
| Content Creation (6-12) | 7 | ~370 |
| Gameplay Systems (13-18) | 6 | ~160 |
| UI/Networking/Framework (19-22) | 4 | ~130 |
| World Building (23-27) | 5 | ~100 |
| Advanced Systems (28-35) | 8 | ~380 |
| Plugin: Character/Avatar (36) | 1 | ~60 |
| Plugin: Asset/Content (37) | 1 | 158 ✅ |
| Plugin: Audio Middleware (38) | 1 | ~80 |
| Plugin: Motion Capture (39) | 1 | ~70 |
| Plugin: Virtual Production (40) | 1 | ~150 |
| Plugin: XR (41) | 1 | ~140 |
| Plugin: AI/NPC (42) | 1 | ~30 |
| Plugin: Utilities (43) | 1 | ~100 |
| Plugin: Physics/Destruction (44) | 1 | ~80 |
| Accessibility (45) | 1 | ~40 |
| Modding & UGC (46) | 1 | ~25 |
| Context Optimization (47-52) | 6 | ~0 |
| Production Tools (53-59) | 7 | ~115 |
| **TOTAL** | **59** | **~2,850** |

---

## What This Enables

With ~2,850 actions covering all Unreal Engine systems and major plugins:

### Game Development
- **Complete Game Development** - Build any genre (FPS, RPG, Racing, Platformer, etc.)
- **Accessible Games** - Full accessibility support (colorblind, subtitles, motor, cognitive)
- **Mod-Friendly Games** - Pak loading, mod browser, asset override

### Film & Virtual Production
- **Animated Films** - Full pipeline from character to final render
- **Virtual Production** - LED walls, motion capture, real-time compositing
- **Live Events** - DMX, OSC, MIDI, timecode sync

### Enterprise & XR
- **VR/AR Experiences** - All major headsets and AR platforms
- **ArchViz** - CAD import, materials, lighting, walkthroughs
- **Mobile Development** - iOS ARKit, Android ARCore

---

## Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Phase Complete |
| 🔄 | In Progress or Planned |
| [x] | Action Implemented |
| [ ] | Action Planned |

---

## Contributing

This roadmap represents a massive undertaking. Contributions are welcome for any phase.

### Adding a New Action

Each new action requires:

1. **TypeScript Handler** in `src/tools/handlers/`
2. **Tool Definition** in `src/tools/consolidated-tool-definitions.ts`
3. **C++ Handler** in `plugins/McpAutomationBridge/Source/.../Private/`
4. **Integration Test** in `tests/`

See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines.

---

## Related Documentation

| Document | Description |
|----------|-------------|
| [Handler Mappings](handler-mapping.md) | TypeScript to C++ routing |
| [Plugin Extension](editor-plugin-extension.md) | C++ plugin architecture |
| [Testing Guide](testing-guide.md) | How to run and write tests |
| [Migration Guide](Migration-Guide-v0.5.0.md) | Upgrade to v0.5.0 |
