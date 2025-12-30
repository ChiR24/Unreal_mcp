# Roadmap for Unreal Engine MCP Server

This roadmap outlines the comprehensive development plan for expanding the Unreal Engine Model Context Protocol (MCP) server into a **complete automation platform** capable of building full production projects (games, films, archviz, VR experiences, virtual production, etc.).

**Target**: ~2,855 actions covering all Unreal Engine subsystems and major plugin integrations.

---

## Phase 1: Architecture & Foundation (Completed)

- [x] **Native C++ Bridge**: Replace Python-based bridge with native C++ WebSocket plugin.
- [x] **Consolidated Tools**: Unify disparate tools into cohesive domains (`manage_asset`, `control_actor`, etc.).
- [x] **Modular Server**: Refactor monolithic `index.ts` into specialized subsystems (`ServerSetup`, `HealthMonitor`, `ResourceHandler`).
- [x] **WASM Acceleration**: Integrate Rust/WASM for high-performance JSON parsing and math.
- [x] **Robust Error Handling**: Standardize error responses, codes, and logging.
- [x] **Offline Capabilities**: Implement file-based fallback for project settings (`DefaultEngine.ini`).

## Phase 2: Graph & Logic Automation (Completed)

- [x] **Blueprint Graphs**: Add/remove nodes, pins, and properties (`manage_blueprint_graph`).
- [x] **Material Graphs**: Edit material expressions and connections (`manage_material_graph`).
- [x] **Niagara Graphs**: Edit emitters, modules, and parameters (`manage_niagara_graph`).
- [x] **Behavior Trees**: Edit AI behavior trees, tasks, and decorators (`manage_behavior_tree`).
- [x] **Environment**: Unified environment builder (`build_environment`) for landscape, foliage, and proc-gen.

## Phase 3: Cinematic & Visual Automation (Completed)

- [x] **Sequencer**: Full control over Level Sequences (create, play, tracks, keys, bindings).
- [x] **Audio**: Create SoundCues, play sounds, set mixes (`create_sound_cue`, `play_sound_at_location`).
- [x] **Landscape**: Sculpting, painting layers, modifying heightmaps (`sculpt_landscape`, `paint_landscape_layer`).
- [x] **Foliage**: Painting foliage instances and procedural spawning (`paint_foliage`).

## Phase 4: System & Developer Experience (Completed)

- [x] **GraphQL API**: Read/write access to assets and actors via GraphQL.
- [x] **Pipeline Integration**: Direct UBT execution with output streaming.
- [x] **Documentation**: Comprehensive handler mappings and API references.
- [x] **Metrics Dashboard**: `ue://health` view backed by bridge/server metrics.
- [x] **UE 5.7 Support**: Full compatibility with Unreal Engine 5.7 (Control Rig, Subobject Data).

## Phase 5: Infrastructure Improvements (Current)

- [ ] **Real-time Streaming**: Streaming logs and test results via SSE or chunked responses.
- [ ] **WASM Expansion**: Move more tools (graph handlers) into Rust/WASM for performance and safety.
- [ ] **Extensibility Framework**: Dynamic handler registry via JSON config and support for custom C++ handlers.
- [ ] **Remote Profiling**: Deep integration with Unreal Insights for remote performance tuning.

---

# Advanced Capabilities Roadmap

The following phases represent the comprehensive expansion to enable **full project creation** - from simple prototypes to AAA-quality games, animated films, and virtual production pipelines.

---

## Phase 6: Geometry & Mesh Creation

**Goal**: Enable AI to CREATE actual 3D geometry, not just place existing meshes.

**Tool**: `manage_geometry`

### 6.1 Primitives
- [ ] `create_box`, `create_sphere`, `create_cylinder`, `create_cone`
- [ ] `create_torus`, `create_capsule`, `create_plane`, `create_disc`
- [ ] `create_stairs`, `create_spiral_stairs`, `create_ramp`
- [ ] `create_arch`, `create_pipe`, `create_ring`

### 6.2 Boolean/CSG Operations
- [ ] `boolean_union`, `boolean_difference`, `boolean_intersection`
- [ ] `boolean_trim`, `self_union`

### 6.3 Modeling Operations
- [ ] `extrude`, `extrude_along_spline`
- [ ] `inset`, `outset`
- [ ] `bevel`, `chamfer`
- [ ] `bridge`, `loft`, `sweep`, `revolve`
- [ ] `mirror`, `symmetrize`
- [ ] `array_linear`, `array_radial`
- [ ] `duplicate_along_spline`
- [ ] `loop_cut`, `edge_split`
- [ ] `poke`, `triangulate`, `quadrangulate`
- [ ] `offset_faces`, `solidify`, `shell`

### 6.4 Deformers
- [ ] `bend`, `twist`, `taper`, `stretch`
- [ ] `lattice_deform`, `noise_deform`
- [ ] `spherify`, `cylindrify`
- [ ] `smooth`, `relax`
- [ ] `displace_by_texture`

### 6.5 Mesh Processing
- [ ] `subdivide` (catmull_clark, loop, linear)
- [ ] `simplify`, `decimate`
- [ ] `remesh_voxel`, `remesh_uniform`
- [ ] `weld_vertices`, `merge_vertices`
- [ ] `remove_degenerates`, `fill_holes`
- [ ] `flip_normals`, `unify_normals`
- [ ] `recompute_normals`, `recompute_tangents`

### 6.6 UV Operations
- [ ] `auto_uv`, `project_uv` (box, planar, cylindrical, spherical)
- [ ] `unwrap_uv`, `pack_uv_islands`
- [ ] `transform_uvs`, `scale_uvs`, `rotate_uvs`

### 6.7 Collision Generation
- [ ] `generate_convex_collision`
- [ ] `generate_complex_collision`
- [ ] `add_box_collision`, `add_sphere_collision`, `add_capsule_collision`
- [ ] `simplify_collision`

### 6.8 LOD & Output
- [ ] `generate_lods`, `set_lod_settings`
- [ ] `set_lod_screen_sizes`
- [ ] `save_as_static_mesh`
- [ ] `convert_to_nanite`

> **Note**: Geometry Collection for destruction is in Phase 46.1 (Chaos Destruction).

---

## Phase 7: Skeletal Mesh & Rigging

**Goal**: Enable creation and editing of animated characters with proper rigs.

**Tool**: `manage_skeleton`

### 7.1 Skeleton Creation
- [ ] `create_skeleton`
- [ ] `add_bone`, `remove_bone`, `rename_bone`
- [ ] `set_bone_transform`, `set_bone_parent`
- [ ] `create_virtual_bone`
- [ ] `create_socket`, `configure_socket`

### 7.2 Skin Weights
- [ ] `auto_skin_weights`
- [ ] `set_vertex_weights`
- [ ] `normalize_weights`, `prune_weights`
- [ ] `copy_weights`, `mirror_weights`

### 7.3 Physics Asset
- [ ] `create_physics_asset`
- [ ] `add_physics_body` (capsule, sphere, box, convex)
- [ ] `configure_physics_body` (mass, damping, collision)
- [ ] `add_physics_constraint`
- [ ] `configure_constraint_limits`

### 7.4 Cloth Setup (Basic)
> **Note**: Full cloth simulation configuration is in Phase 46.3 (Chaos Cloth). This section covers skeletal mesh cloth binding only.

- [ ] `bind_cloth_to_skeletal_mesh`
- [ ] `assign_cloth_asset_to_mesh`

### 7.5 Morph Targets
- [ ] `create_morph_target`
- [ ] `set_morph_target_deltas`
- [ ] `import_morph_targets`

---

## Phase 8: Advanced Material Authoring

**Goal**: Full material creation and shader authoring capabilities.

**Tool**: `manage_material_authoring`

### 8.1 Material Creation
- [ ] `create_material` (surface, deferred_decal, light_function, post_process, UI, volume)
- [ ] `set_blend_mode` (opaque, masked, translucent, additive, modulate)
- [ ] `set_shading_model` (default_lit, unlit, subsurface, clear_coat, two_sided_foliage, hair, eye, cloth)

### 8.2 Material Expressions (Node Graph)
- [ ] `add_texture_sample`, `add_texture_coordinate`
- [ ] `add_scalar_parameter`, `add_vector_parameter`
- [ ] `add_static_switch_parameter`
- [ ] `add_math_node` (add, multiply, divide, power, lerp, clamp, etc.)
- [ ] `add_world_position`, `add_vertex_normal`, `add_pixel_depth`
- [ ] `add_fresnel`, `add_reflection_vector`
- [ ] `add_panner`, `add_rotator`
- [ ] `add_noise`, `add_voronoi`
- [ ] `add_if`, `add_switch`
- [ ] `add_custom_expression` (HLSL code)
- [ ] `connect_nodes`, `disconnect_nodes`

### 8.3 Material Functions & Layers
- [ ] `create_material_function`
- [ ] `add_function_input`, `add_function_output`
- [ ] `use_material_function`
- [ ] `create_material_layer`
- [ ] `create_material_layer_blend`

### 8.4 Material Instances
- [ ] `create_material_instance`
- [ ] `set_scalar_parameter_value`
- [ ] `set_vector_parameter_value`
- [ ] `set_texture_parameter_value`

### 8.5 Specialized Materials
- [ ] `create_landscape_material`, `add_landscape_layer`, `configure_layer_blend`
- [ ] `create_decal_material`
- [ ] `create_post_process_material`

---

## Phase 9: Texture Generation & Processing

**Goal**: Procedural texture creation and processing.

**Tool**: `manage_texture`

### 9.1 Procedural Generation
- [ ] `create_noise_texture` (perlin, simplex, worley, voronoi)
- [ ] `create_gradient_texture` (linear, radial, angular)
- [ ] `create_pattern_texture` (checker, grid, brick, tile)
- [ ] `create_normal_from_height`
- [ ] `create_ao_from_mesh`

### 9.2 Texture Processing
- [ ] `resize_texture`
- [ ] `adjust_levels`, `adjust_curves`
- [ ] `blur`, `sharpen`
- [ ] `invert`, `desaturate`
- [ ] `channel_pack`, `channel_extract`
- [ ] `combine_textures` (blend modes)

### 9.3 Texture Settings
- [ ] `set_compression_settings`
- [ ] `set_texture_group`
- [ ] `set_lod_bias`
- [ ] `configure_virtual_texture`
- [ ] `set_streaming_priority`

---

## Phase 10: Complete Animation System

**Goal**: Full animation authoring from keyframes to state machines.

**Tool**: `manage_animation_authoring`

### 10.1 Animation Sequences
- [ ] `create_animation_sequence`
- [ ] `set_sequence_length`
- [ ] `add_bone_track`
- [ ] `set_bone_key` (location, rotation, scale at time)
- [ ] `set_curve_key` (float curve at time)
- [ ] `add_notify`, `add_notify_state`
- [ ] `add_sync_marker`
- [ ] `set_root_motion_settings`
- [ ] `set_additive_settings`

### 10.2 Animation Montages
- [ ] `create_montage`
- [ ] `add_montage_section`
- [ ] `add_montage_slot`
- [ ] `set_section_timing`
- [ ] `add_montage_notify`
- [ ] `set_blend_in`, `set_blend_out`
- [ ] `link_sections`

### 10.3 Blend Spaces
- [ ] `create_blend_space_1d`
- [ ] `create_blend_space_2d`
- [ ] `add_blend_sample`
- [ ] `set_axis_settings`
- [ ] `set_interpolation_settings`
- [ ] `create_aim_offset`, `add_aim_offset_sample`

### 10.4 Animation Blueprints
- [ ] `create_anim_blueprint`
- [ ] `add_state_machine`
- [ ] `add_state`, `add_transition`
- [ ] `set_transition_rules`
- [ ] `add_blend_node`
- [ ] `add_cached_pose`
- [ ] `add_slot_node`
- [ ] `add_layered_blend_per_bone`
- [ ] `set_anim_graph_node_value`

### 10.5 Control Rig
- [ ] `create_control_rig`
- [ ] `add_control`
- [ ] `add_rig_unit` (FKIK, aim, basic_ik, etc.)
- [ ] `connect_rig_elements`
- [ ] `create_pose_library`

### 10.6 Retargeting
- [ ] `create_ik_rig`
- [ ] `add_ik_chain`
- [ ] `create_ik_retargeter`
- [ ] `set_retarget_chain_mapping`

---

## Phase 11: Complete Audio System

**Goal**: Full audio authoring including MetaSounds.

**Tool**: `manage_audio_authoring`

### 11.1 Sound Cues (Expanded)
- [ ] `create_sound_cue`
- [ ] `add_cue_node` (wave_player, mixer, random, modulator, etc.)
- [ ] `connect_cue_nodes`
- [ ] `set_attenuation`, `set_concurrency`

### 11.2 MetaSounds
- [ ] `create_metasound`
- [ ] `add_metasound_node`
- [ ] `connect_metasound_nodes`
- [ ] `add_metasound_input`, `add_metasound_output`
- [ ] `set_metasound_default`

### 11.3 Sound Classes & Mixes
- [ ] `create_sound_class`, `set_class_properties`, `set_class_parent`
- [ ] `create_sound_mix`, `add_mix_modifier`, `configure_mix_eq`

### 11.4 Attenuation & Spatialization
- [ ] `create_attenuation_settings`
- [ ] `configure_distance_attenuation`
- [ ] `configure_spatialization`
- [ ] `configure_occlusion`
- [ ] `configure_reverb_send`

### 11.5 Dialogue System
- [ ] `create_dialogue_voice`
- [ ] `create_dialogue_wave`
- [ ] `set_dialogue_context`

### 11.6 Effects
- [ ] `create_reverb_effect`
- [ ] `create_source_effect_chain`
- [ ] `add_source_effect` (filter, eq, chorus, delay, etc.)
- [ ] `create_submix_effect`

---

## Phase 12: Complete Niagara VFX System

**Goal**: Full Niagara system authoring.

**Tool**: `manage_niagara_authoring`

### 12.1 Systems & Emitters
- [ ] `create_niagara_system`
- [ ] `create_niagara_emitter`
- [ ] `add_emitter_to_system`
- [ ] `set_emitter_properties`

### 12.2 Module Library
- [ ] `add_spawn_rate_module`, `add_spawn_burst_module`, `add_spawn_per_unit_module`
- [ ] `add_initialize_particle_module`
- [ ] `add_particle_state_module`
- [ ] `add_force_module` (gravity, drag, vortex, point_attraction, curl_noise)
- [ ] `add_velocity_module`, `add_acceleration_module`
- [ ] `add_size_module`, `add_color_module`
- [ ] `add_sprite_size_module`
- [ ] `add_mesh_renderer_module`
- [ ] `add_ribbon_renderer_module`
- [ ] `add_light_renderer_module`
- [ ] `add_collision_module`
- [ ] `add_kill_particles_module`
- [ ] `add_camera_offset_module`

### 12.3 Parameters & Data Interfaces
- [ ] `add_user_parameter` (float, vector, color, texture, mesh, etc.)
- [ ] `set_parameter_value`, `bind_parameter_to_source`
- [ ] `add_skeletal_mesh_data_interface`
- [ ] `add_static_mesh_data_interface`
- [ ] `add_spline_data_interface`
- [ ] `add_audio_spectrum_data_interface`
- [ ] `add_collision_query_data_interface`

### 12.4 Events & GPU
- [ ] `add_event_generator`, `add_event_receiver`
- [ ] `configure_event_payload`
- [ ] `enable_gpu_simulation`
- [ ] `add_simulation_stage`

---

## Phase 13: Gameplay Ability System (GAS)

**Goal**: Complete GAS implementation for abilities, effects, and attributes.

**Tool**: `manage_gas`

### 13.1 Components & Attributes
- [ ] `add_ability_system_component`
- [ ] `configure_asc` (replication_mode, owner)
- [ ] `create_attribute_set`
- [ ] `add_attribute` (health, mana, stamina, damage, armor, etc.)
- [ ] `set_attribute_base_value`, `set_attribute_clamping`

### 13.2 Gameplay Abilities
- [ ] `create_gameplay_ability`
- [ ] `set_ability_tags`
- [ ] `set_ability_costs`, `set_ability_cooldown`
- [ ] `set_ability_targeting`
- [ ] `add_ability_task`
- [ ] `set_activation_policy`, `set_instancing_policy`

### 13.3 Gameplay Effects
- [ ] `create_gameplay_effect`
- [ ] `set_effect_duration` (instant, infinite, duration)
- [ ] `add_effect_modifier` (attribute, operation, magnitude)
- [ ] `set_modifier_magnitude` (scalable, attribute_based, set_by_caller)
- [ ] `add_effect_execution_calculation`
- [ ] `add_effect_cue`
- [ ] `set_effect_stacking`
- [ ] `set_effect_tags` (granted, application, removal)

### 13.4 Gameplay Cues
- [ ] `create_gameplay_cue_notify` (static, actor)
- [ ] `configure_cue_trigger`
- [ ] `set_cue_effects` (particles, sounds, camera_shake)
- [ ] `add_tag_to_asset`

> **Note**: Gameplay Tag creation and management is in Phase 31.3 (Data & Persistence).

---

## Phase 14: Character & Movement System

**Goal**: Complete character setup with advanced movement.

**Tool**: `manage_character`

### 14.1 Character Creation
- [ ] `create_character_blueprint`
- [ ] `configure_capsule_component`
- [ ] `configure_mesh_component`
- [ ] `configure_camera_component`

### 14.2 Movement Component
- [ ] `configure_movement_speeds` (walk, run, sprint, crouch, swim, fly)
- [ ] `configure_jump` (height, air_control, double_jump)
- [ ] `configure_rotation` (orient_to_movement, use_controller_rotation)
- [ ] `add_custom_movement_mode`
- [ ] `configure_nav_movement`

### 14.3 Advanced Movement
- [ ] `setup_mantling`
- [ ] `setup_vaulting`
- [ ] `setup_climbing`
- [ ] `setup_sliding`
- [ ] `setup_wall_running`
- [ ] `setup_grappling`

### 14.4 Footsteps System
> **Note**: Physical Material creation is in Phase 34.5 (Physics Materials).

- [ ] `setup_footstep_system`
- [ ] `map_surface_to_sound`
- [ ] `configure_footstep_fx`

---

## Phase 15: Combat & Weapons System

**Goal**: Complete combat implementation.

**Tool**: `manage_combat`

### 15.1 Weapon Base
- [ ] `create_weapon_blueprint`
- [ ] `configure_weapon_mesh`, `configure_weapon_sockets`
- [ ] `set_weapon_stats` (damage, fire_rate, range, spread)

### 15.2 Firing Modes
- [ ] `configure_hitscan`
- [ ] `configure_projectile`
- [ ] `configure_spread_pattern`
- [ ] `configure_recoil_pattern`
- [ ] `configure_aim_down_sights`

### 15.3 Projectiles
- [ ] `create_projectile_blueprint`
- [ ] `configure_projectile_movement`
- [ ] `configure_projectile_collision`
- [ ] `configure_projectile_homing`

### 15.4 Damage System
- [ ] `create_damage_type`
- [ ] `configure_damage_execution`
- [ ] `setup_hitbox_component`

### 15.5 Weapon Features
- [ ] `setup_reload_system`
- [ ] `setup_ammo_system`
- [ ] `setup_attachment_system`
- [ ] `setup_weapon_switching`

### 15.6 Effects
- [ ] `configure_muzzle_flash`
- [ ] `configure_tracer`
- [ ] `configure_impact_effects`
- [ ] `configure_shell_ejection`

### 15.7 Melee Combat
- [ ] `create_melee_trace`
- [ ] `configure_combo_system`
- [ ] `create_hit_pause` (hitstop)
- [ ] `configure_hit_reaction`
- [ ] `create_parry_system`, `create_block_system`
- [ ] `configure_weapon_trails`

---

## Phase 16: Complete AI System

**Goal**: Full AI pipeline with EQS, perception, and smart objects.

**Tool**: `manage_ai`

### 16.1 AI Controller
- [ ] `create_ai_controller`
- [ ] `assign_behavior_tree`, `assign_blackboard`

### 16.2 Blackboard
- [ ] `create_blackboard_asset`
- [ ] `add_blackboard_key` (bool, int, float, vector, rotator, object, class, enum, name, string)
- [ ] `set_key_instance_synced`

### 16.3 Behavior Tree (Expanded)
- [ ] `create_behavior_tree`
- [ ] `add_composite_node` (selector, sequence, parallel, simple_parallel)
- [ ] `add_task_node` (move_to, rotate_to_face, wait, play_animation, play_sound, run_eqs_query, etc.)
- [ ] `add_decorator` (blackboard, cooldown, cone_check, does_path_exist, is_at_location, loop, time_limit, force_success)
- [ ] `add_service` (default_focus, run_eqs)
- [ ] `connect_nodes`, `set_node_properties`

### 16.4 Environment Query System (EQS)
- [ ] `create_eqs_query`
- [ ] `add_generator` (actors_of_class, current_location, donut, grid, on_circle, pathing_grid, points)
- [ ] `add_context` (querier, item, target)
- [ ] `add_test` (distance, dot, overlap, pathfinding, project, random, trace, gameplay_tags)
- [ ] `configure_test_scoring`

### 16.5 Perception System
- [ ] `add_ai_perception_component`
- [ ] `configure_sight_config` (radius, angle, age, detection_by_affiliation)
- [ ] `configure_hearing_config` (radius)
- [ ] `configure_damage_config`, `configure_touch_config`
- [ ] `set_perception_team`

### 16.6 State Trees (UE5.3+)
- [ ] `create_state_tree`
- [ ] `add_state`, `add_state_transition`
- [ ] `set_state_tasks`, `set_transition_conditions`

### 16.7 Smart Objects
- [ ] `create_smart_object_definition`
- [ ] `add_smart_object_slot`
- [ ] `configure_slot_behavior`
- [ ] `place_smart_object_component`

### 16.8 Mass AI (Crowds)
- [ ] `configure_mass_entity`
- [ ] `create_mass_entity_config`
- [ ] `add_mass_spawner`

---

## Phase 17: Inventory & Items System

**Goal**: Complete inventory and item management.

**Tool**: `manage_inventory`

### 17.1 Data Assets
- [ ] `create_item_data_asset`
- [ ] `set_item_properties` (name, description, icon, mesh, stack_size, weight, rarity)

### 17.2 Inventory Component
- [ ] `create_inventory_component`
- [ ] `configure_inventory_slots`
- [ ] `add_inventory_functions`

### 17.3 Pickups
- [ ] `create_pickup_actor`
- [ ] `configure_pickup_interaction`
- [ ] `configure_pickup_respawn`

### 17.4 Equipment
- [ ] `create_equipment_component`
- [ ] `define_equipment_slots`
- [ ] `configure_equipment_effects`

---

## Phase 18: Interaction System

**Goal**: Complete interaction framework.

**Tool**: `manage_interaction`

### 18.1 Interaction Component
- [ ] `create_interaction_component`
- [ ] `configure_interaction_trace`
- [ ] `configure_interaction_widget`

### 18.2 Interactables
- [ ] `create_interactable_interface`
- [ ] `create_door_actor`
- [ ] `create_switch_actor`
- [ ] `create_chest_actor`

> **Note**: Pickup actors are in Phase 17.3 (Inventory - Pickups).

### 18.3 Destructibles
- [ ] `setup_destructible_mesh`
- [ ] `configure_destruction_levels`
- [ ] `configure_destruction_effects`

---

## Phase 19: Complete UI/UX System

**Goal**: Full UMG widget authoring capabilities.

**Tool**: `manage_widget_authoring`

### 19.1 Widget Creation
- [ ] `create_widget_blueprint`
- [ ] `set_widget_parent_class`

### 19.2 Layout Panels
- [ ] `add_canvas_panel`
- [ ] `add_horizontal_box`, `add_vertical_box`
- [ ] `add_overlay`, `add_grid_panel`, `add_uniform_grid`
- [ ] `add_wrap_box`, `add_scroll_box`
- [ ] `add_size_box`, `add_scale_box`
- [ ] `add_border`, `add_safe_zone`
- [ ] `add_widget_switcher`

### 19.3 Common Widgets
- [ ] `add_text_block`, `add_rich_text_block`
- [ ] `add_image`, `add_button`
- [ ] `add_check_box`, `add_slider`
- [ ] `add_progress_bar`
- [ ] `add_text_input` (editable_text, editable_text_box)
- [ ] `add_combo_box`, `add_spin_box`
- [ ] `add_list_view`, `add_tree_view`, `add_tile_view`

### 19.4 Layout & Styling
- [ ] `set_anchor`, `set_alignment`
- [ ] `set_position`, `set_size`
- [ ] `set_padding`, `set_z_order`
- [ ] `set_render_transform`, `set_clipping`
- [ ] `set_color_and_opacity`, `set_font`, `set_brush`, `set_style`

### 19.5 Bindings & Events
- [ ] `create_property_binding`
- [ ] `bind_text`, `bind_visibility`, `bind_color`, `bind_enabled`
- [ ] `bind_on_clicked`, `bind_on_hovered`, `bind_on_value_changed`, `bind_on_text_committed`

### 19.6 Widget Animations
- [ ] `create_widget_animation`
- [ ] `add_animation_track` (transform, color, opacity, material)
- [ ] `add_animation_keyframe`
- [ ] `set_animation_loop`

### 19.7 UI Templates
- [ ] `create_main_menu`, `create_pause_menu`
- [ ] `create_settings_menu` (video, audio, controls, gameplay)
- [ ] `create_confirmation_dialog`, `create_loading_screen`
- [ ] `create_hud_widget`
- [ ] `add_health_bar`, `add_ammo_counter`, `add_minimap`
- [ ] `add_crosshair`, `add_compass`
- [ ] `add_interaction_prompt`, `add_objective_tracker`
- [ ] `add_damage_indicator`, `add_kill_feed`
- [ ] `add_notification_system`
- [ ] `create_inventory_ui`, `create_scoreboard`
- [ ] `create_chat_box`, `create_radial_menu`

---

## Phase 20: Networking & Multiplayer

**Goal**: Complete networking and replication system.

**Tool**: `manage_networking`

### 20.1 Replication
- [ ] `set_property_replicated`
- [ ] `set_replication_condition` (COND_None, COND_OwnerOnly, COND_SkipOwner, COND_SimulatedOnly, etc.)
- [ ] `configure_net_update_frequency`
- [ ] `configure_net_priority`
- [ ] `set_net_dormancy`
- [ ] `configure_replication_graph`

### 20.2 RPCs
- [ ] `create_rpc_function` (Server, Client, NetMulticast)
- [ ] `configure_rpc_validation`
- [ ] `set_rpc_reliability`

### 20.3 Authority & Ownership
- [ ] `set_owner`
- [ ] `set_autonomous_proxy`
- [ ] `check_has_authority`
- [ ] `check_is_locally_controlled`

### 20.4 Network Relevancy
- [ ] `configure_net_cull_distance`
- [ ] `set_always_relevant`
- [ ] `set_only_relevant_to_owner`

---

## Phase 21: Game Framework

**Goal**: Complete game mode and session management.

**Tool**: `manage_game_framework`

### 21.1 Core Classes
- [ ] `create_game_mode`
- [ ] `create_game_state`
- [ ] `create_player_controller`
- [ ] `create_player_state`
- [ ] `create_game_instance`
- [ ] `create_hud_class`

### 21.2 Game Mode Configuration
- [ ] `set_default_pawn_class`
- [ ] `set_player_controller_class`
- [ ] `set_game_state_class`
- [ ] `set_player_state_class`
- [ ] `configure_game_rules`

### 21.3 Match Flow
- [ ] `setup_match_states` (waiting, warmup, in_progress, post_match)
- [ ] `configure_round_system`
- [ ] `configure_team_system`
- [ ] `configure_scoring_system`
- [ ] `configure_spawn_system`

### 21.4 Player Management
- [ ] `configure_player_start`
- [ ] `set_respawn_rules`
- [ ] `configure_spectating`

---

## Phase 22: Sessions & Local Multiplayer

**Goal**: Session management and split-screen support.

**Tool**: `manage_sessions`

### 22.1 Session Management (Local/LAN)
> **Note**: Online session management (matchmaking, lobbies) is in Phase 43 (Online Services). This section covers local/LAN sessions only.

- [ ] `configure_local_session_settings`
- [ ] `configure_session_interface`

### 22.2 Local Multiplayer
- [ ] `configure_split_screen`
- [ ] `set_split_screen_type` (horizontal, vertical, grid)
- [ ] `add_local_player`
- [ ] `remove_local_player`

### 22.3 LAN
- [ ] `configure_lan_play`
- [ ] `host_lan_server`
- [ ] `join_lan_server`

### 22.4 Voice Chat
- [ ] `enable_voice_chat`
- [ ] `configure_voice_settings`
- [ ] `set_voice_channel`
- [ ] `mute_player`
- [ ] `set_voice_attenuation`
- [ ] `configure_push_to_talk`

---

## Phase 23: World & Level Structure

**Goal**: Complete level and world management.

**Tool**: `manage_level_structure`

### 23.1 Levels
- [ ] `create_level`, `create_sublevel`
- [ ] `configure_level_streaming`
- [ ] `set_streaming_distance`
- [ ] `configure_level_bounds`

### 23.2 World Partition (Expanded)
- [ ] `enable_world_partition`
- [ ] `configure_grid_size`
- [ ] `create_data_layer`
- [ ] `assign_actor_to_data_layer`
- [ ] `configure_hlod_layer`
- [ ] `create_minimap_volume`

### 23.3 Level Blueprint
- [ ] `open_level_blueprint`
- [ ] `add_level_blueprint_node`
- [ ] `connect_level_blueprint_nodes`

### 23.4 Level Instances
- [ ] `create_level_instance`
- [ ] `create_packed_level_actor`

---

## Phase 24: Volumes & Zones

**Goal**: Complete volume and trigger system.

**Tool**: `manage_volumes`

### 24.1 Trigger Volumes
- [ ] `create_trigger_volume`
- [ ] `create_trigger_box`, `create_trigger_sphere`, `create_trigger_capsule`

### 24.2 Gameplay Volumes
- [ ] `create_blocking_volume`
- [ ] `create_kill_z_volume`
- [ ] `create_pain_causing_volume`
- [ ] `create_physics_volume`
- [ ] `create_audio_volume`, `create_reverb_volume`
- [ ] `create_cull_distance_volume`
- [ ] `create_precomputed_visibility_volume`
- [ ] `create_lightmass_importance_volume`
- [ ] `create_nav_mesh_bounds_volume`
- [ ] `create_nav_modifier_volume`
- [ ] `create_camera_blocking_volume`

> **Note**: Post Process Volume configuration is in Phase 29.5 (Post Processing).

### 24.3 Volume Configuration
- [ ] `set_volume_extent`
- [ ] `set_volume_properties`

---

## Phase 25: Navigation System

**Goal**: Complete navigation mesh and pathfinding.

**Tool**: `manage_navigation`

### 25.1 NavMesh
- [ ] `configure_nav_mesh_settings`
- [ ] `set_nav_agent_properties` (radius, height, step_height)
- [ ] `rebuild_navigation`

### 25.2 Nav Modifiers
- [ ] `create_nav_modifier_component`
- [ ] `set_nav_area_class`
- [ ] `configure_nav_area_cost`

### 25.3 Nav Links
- [ ] `create_nav_link_proxy`
- [ ] `configure_nav_link` (start, end, direction, snap_radius)
- [ ] `set_nav_link_type` (simple, smart)
- [ ] `create_smart_link`
- [ ] `configure_smart_link_behavior`

---

## Phase 26: Spline System

**Goal**: Complete spline-based content creation.

**Tool**: `manage_splines`

### 26.1 Spline Creation
- [ ] `create_spline_actor`
- [ ] `add_spline_point`, `remove_spline_point`
- [ ] `set_spline_point_position`
- [ ] `set_spline_point_tangents`
- [ ] `set_spline_point_rotation`, `set_spline_point_scale`
- [ ] `set_spline_type` (linear, curve, constant, clamped_curve)

### 26.2 Spline Mesh
- [ ] `create_spline_mesh_component`
- [ ] `set_spline_mesh_asset`
- [ ] `configure_spline_mesh_axis`
- [ ] `set_spline_mesh_material`

### 26.3 Spline Mesh Array
- [ ] `scatter_meshes_along_spline`
- [ ] `configure_mesh_spacing`
- [ ] `configure_mesh_randomization`

### 26.4 Quick Templates
- [ ] `create_road_spline`
- [ ] `create_river_spline`
- [ ] `create_fence_spline`
- [ ] `create_wall_spline`
- [ ] `create_cable_spline`
- [ ] `create_pipe_spline`

---

## Phase 27: PCG Framework

**Goal**: Complete procedural content generation.

**Tool**: `manage_pcg`

### 27.1 Graph Management
- [ ] `create_pcg_graph`, `create_pcg_subgraph`
- [ ] `add_pcg_node`
- [ ] `connect_pcg_pins`
- [ ] `set_pcg_node_settings`

### 27.2 Input Nodes
- [ ] `add_landscape_data_node`
- [ ] `add_spline_data_node`
- [ ] `add_volume_data_node`
- [ ] `add_actor_data_node`
- [ ] `add_texture_data_node`

### 27.3 Point Operations
- [ ] `add_surface_sampler`, `add_mesh_sampler`
- [ ] `add_spline_sampler`, `add_volume_sampler`
- [ ] `add_bounds_modifier`
- [ ] `add_density_filter`, `add_height_filter`
- [ ] `add_slope_filter`, `add_distance_filter`
- [ ] `add_bounds_filter`, `add_self_pruning`
- [ ] `add_transform_points`
- [ ] `add_project_to_surface`
- [ ] `add_copy_points`, `add_merge_points`

### 27.4 Spawning
- [ ] `add_static_mesh_spawner`
- [ ] `add_actor_spawner`
- [ ] `add_spline_spawner`

### 27.5 Execution
- [ ] `execute_pcg_graph`
- [ ] `set_pcg_partition_grid_size`

---

## Phase 28: Environment Systems

**Goal**: Complete environment (sky, weather, water).

**Tool**: `manage_environment`

### 28.1 Landscape (Expanded)
- [ ] `create_landscape`
- [ ] `import_heightmap`, `export_heightmap`
- [ ] `sculpt_landscape` (raise, lower, smooth, flatten, erosion)
- [ ] `paint_landscape_layer`
- [ ] `create_landscape_layer_info`
- [ ] `configure_landscape_material`
- [ ] `create_landscape_grass_type`
- [ ] `configure_landscape_splines`
- [ ] `configure_landscape_lod`
- [ ] `create_landscape_streaming_proxy`

### 28.2 Foliage (Expanded)
- [ ] `create_foliage_type`
- [ ] `configure_foliage_mesh`
- [ ] `configure_foliage_placement` (density, scale, rotation, align)
- [ ] `configure_foliage_lod`, `configure_foliage_collision`, `configure_foliage_culling`
- [ ] `paint_foliage_instances`, `remove_foliage_instances`

### 28.3 Sky & Atmosphere
- [ ] `configure_sky_atmosphere`
- [ ] `configure_sky_light`
- [ ] `configure_directional_light_atmosphere`
- [ ] `configure_exponential_height_fog`
- [ ] `configure_volumetric_cloud`
- [ ] `create_sky_sphere`

### 28.4 Weather
- [ ] `create_weather_system`
- [ ] `configure_rain_particles`
- [ ] `configure_snow_particles`
- [ ] `configure_wind`
- [ ] `configure_lightning`

### 28.5 Time of Day
- [ ] `create_time_of_day_system`
- [ ] `configure_sun_position`
- [ ] `configure_light_color_curve`
- [ ] `configure_sky_color_curve`

### 28.6 Water (Water Plugin)
- [ ] `create_water_body_ocean`
- [ ] `create_water_body_lake`
- [ ] `create_water_body_river`
- [ ] `create_water_body_custom`
- [ ] `configure_water_waves`
- [ ] `configure_water_material`
- [ ] `configure_water_collision`
- [ ] `create_buoyancy_component`

---

## Phase 29: Advanced Lighting & Rendering

**Goal**: Complete lighting and post-processing.

**Tool**: `manage_lighting` (Expanded) + `manage_post_process`

### 29.1 Ray Tracing
- [ ] `configure_ray_traced_shadows`
- [ ] `configure_ray_traced_gi`
- [ ] `configure_ray_traced_reflections`
- [ ] `configure_ray_traced_ao`
- [ ] `configure_path_tracing`

### 29.2 Light Channels
- [ ] `set_light_channel`
- [ ] `set_actor_light_channel`

### 29.3 Lightmass
- [ ] `configure_lightmass_settings`
- [ ] `build_lighting_quality`
- [ ] `configure_indirect_lighting_cache`

### 29.4 Reflections
- [ ] `create_sphere_reflection_capture`
- [ ] `create_box_reflection_capture`
- [ ] `configure_capture_resolution`, `configure_capture_offset`
- [ ] `recapture_scene`
- [ ] `create_planar_reflection`
- [ ] `configure_planar_reflection` (resolution, clip_plane)
- [ ] `configure_ssr_settings`
- [ ] `configure_lumen_reflection_settings`

### 29.5 Post Processing
- [ ] `create_post_process_volume`
- [ ] `configure_pp_blend` (infinite_unbound, weight)
- [ ] Color Grading: `set_pp_white_balance`, `set_pp_color_grading`, `set_pp_lut`, saturation, contrast, gamma, gain, offset
- [ ] `configure_tonemapper`, `set_tonemapper_type`
- [ ] Bloom: `configure_bloom`, `set_bloom_intensity`, `set_bloom_threshold`, `configure_lens_flare`
- [ ] DOF: `configure_dof`, `set_dof_method`, `set_focal_distance`, `set_aperture`, `configure_bokeh`
- [ ] Motion Blur: `configure_motion_blur`, `set_motion_blur_amount`, `set_motion_blur_max`
- [ ] Exposure: `configure_exposure`, `set_exposure_method`, `set_exposure_compensation`, `set_exposure_min_max`
- [ ] AO: `configure_ssao`, `configure_gtao`
- [ ] Effects: `configure_vignette`, `configure_chromatic_aberration`, `configure_grain`, `configure_screen_percentage`

### 29.6 Scene Capture
- [ ] `create_scene_capture_2d`, `create_scene_capture_cube`
- [ ] `configure_capture_resolution`, `configure_capture_source`
- [ ] `create_render_target`, `assign_render_target`
- [ ] `capture_scene`

---

## Phase 30: Cinematics & Media

**Goal**: Complete sequencer and media capabilities.

**Tool**: `manage_sequencer` (Expanded) + `manage_movie_render` + `manage_media`

### 30.1 Sequencer (Expanded)
- [ ] `create_master_sequence`
- [ ] `add_subsequence`
- [ ] `add_shot_track`, `configure_shot_settings`
- [ ] `create_cine_camera_actor`
- [ ] `configure_camera_settings` (filmback, lens, focus)
- [ ] `add_camera_cut_track`, `add_camera_shake_track`
- [ ] `configure_camera_rig_rail`, `configure_camera_rig_crane`
- [ ] Additional tracks: `add_fade_track`, `add_level_visibility_track`, `add_material_parameter_track`, `add_particle_track`, `add_skeletal_animation_track`, `add_transform_track`, `add_event_track`, `add_property_track`

### 30.2 Movie Render Queue
- [ ] `create_render_job`
- [ ] `configure_output_settings`
- [ ] `add_render_pass` (beauty, depth, normal, motion_vector, object_id, custom_stencil)
- [ ] `configure_anti_aliasing` (spatial, temporal)
- [ ] `configure_console_variables`
- [ ] `configure_burn_ins`
- [ ] `queue_render`, `start_render`

### 30.3 Media Framework
- [ ] `create_media_player`
- [ ] `create_media_source` (file, stream, platform)
- [ ] `create_media_texture`
- [ ] `create_media_sound_component`
- [ ] `create_media_playlist`
- [ ] `play_media`, `pause_media`, `seek_media`

### 30.4 Take Recorder
- [ ] `create_take_recorder_panel`
- [ ] `configure_take_sources`
- [ ] `start_recording`, `stop_recording`
- [ ] `configure_recorded_tracks`

### 30.5 Demo/Replay System
- [ ] `start_demo_recording`, `stop_demo_recording`
- [ ] `configure_demo_settings`
- [ ] `play_demo`, `pause_demo`, `seek_demo`
- [ ] `set_demo_playback_speed`
- [ ] `configure_killcam_duration`, `start_killcam`

---

## Phase 31: Data & Persistence

**Goal**: Complete data management and save systems.

**Tools**: `manage_data_assets`, `manage_save_system`, `manage_gameplay_tags`, `manage_config`

### 31.1 Data Assets
- [ ] `create_data_asset`, `create_primary_data_asset`
- [ ] `create_data_table`, `add_data_table_row`, `modify_data_table_row`, `delete_data_table_row`
- [ ] `import_data_table_csv`, `export_data_table_csv`
- [ ] `create_curve_table`, `create_curve_float`, `create_curve_linear_color`

### 31.2 Save System
- [ ] `create_save_game_class`
- [ ] `add_save_variable`
- [ ] `save_game_to_slot`, `load_game_from_slot`
- [ ] `delete_save_slot`, `check_save_slot_exists`
- [ ] `get_save_slot_names`
- [ ] `configure_async_save_load`

### 31.3 Gameplay Tags
- [ ] `create_gameplay_tag`
- [ ] `create_tag_container`
- [ ] `add_tag_to_container`, `remove_tag_from_container`
- [ ] `check_tag_match`
- [ ] `register_native_tag`
- [ ] `create_tag_table`

### 31.4 Config System
- [ ] `read_config_value`, `write_config_value`
- [ ] `get_section`, `create_config_section`
- [ ] `flush_config`, `reload_config`
- [ ] `get_config_hierarchy`

---

## Phase 32: Build & Deployment

**Goal**: Complete build pipeline and packaging.

**Tool**: `manage_build`

### 32.1 Build Pipeline
- [ ] `run_ubt` (expanded)
- [ ] `generate_project_files`
- [ ] `compile_shaders`
- [ ] `cook_content` (platform)
- [ ] `package_project` (platform)
- [ ] `configure_build_settings`
- [ ] `create_build_target`

### 32.2 Platform Builds
- [ ] `configure_windows_build`
- [ ] `configure_linux_build`
- [ ] `configure_mac_build`
- [ ] `configure_ios_build`, `configure_ios_signing` (provisioning profile, bundle ID)
- [ ] `configure_android_build`, `configure_android_signing` (keystore, package name)

> **Note**: Console builds (PlayStation, Xbox, Switch) require external SDK installation and platform portal registration BEFORE the project can be opened. Once SDKs are installed, MCP can configure build settings within the project.

### 32.3 Asset Management
- [ ] `validate_assets`
- [ ] `audit_assets`
- [ ] `size_map_analysis`
- [ ] `reference_viewer`
- [ ] `configure_chunking`
- [ ] `create_pak_file`
- [ ] `configure_asset_encryption`
- [ ] `configure_compression`

### 32.4 Plugins
- [ ] `list_plugins`
- [ ] `enable_plugin`, `disable_plugin`
- [ ] `get_plugin_status`
- [ ] `configure_plugin_settings`

---

## Phase 33: Testing & Quality

**Goal**: Complete testing and profiling infrastructure.

**Tools**: `manage_testing`, `manage_profiling`, `manage_validation`

### 33.1 Automation Testing
- [ ] `create_functional_test`
- [ ] `create_automation_test`
- [ ] `run_automation_tests`
- [ ] `get_test_results`
- [ ] `create_test_level`
- [ ] `configure_test_settings`
- [ ] `run_gauntlet_test`

### 33.2 Profiling
- [ ] `start_unreal_insights`
- [ ] `capture_insights_trace`
- [ ] `analyze_trace`
- [ ] `start_memory_report`
- [ ] `start_network_profiler`
- [ ] `enable_visual_logger`
- [ ] `add_visual_log_entry`
- [ ] `enable_gameplay_debugger`
- [ ] `configure_stat_commands`

### 33.3 Validation
- [ ] `create_asset_validator`
- [ ] `run_data_validation`
- [ ] `check_for_errors`
- [ ] `fix_redirectors`
- [ ] `check_map_errors`
- [ ] `validate_blueprints`

---

## Phase 34: Editor Utilities

**Goal**: Complete editor automation.

**Tools**: Various editor management tools

### 34.1 Editor Modes
- [ ] `set_editor_mode` (place, paint, landscape, foliage, mesh_paint)
- [ ] `configure_editor_preferences`
- [ ] `set_grid_settings`, `set_snap_settings`
- [ ] `manage_editor_layouts`
- [ ] `create_custom_editor_mode`

### 34.2 Content Browser
- [ ] `set_view_settings`
- [ ] `navigate_to_path`
- [ ] `sync_to_asset`
- [ ] `create_collection`, `add_to_collection`
- [ ] `set_asset_color`
- [ ] `show_in_explorer`

### 34.3 Selection
- [ ] `select_actor`
- [ ] `select_actors_by_class`
- [ ] `select_actors_by_tag`
- [ ] `select_actors_in_volume`
- [ ] `deselect_all`
- [ ] `get_selected_actors`
- [ ] `group_actors`, `ungroup_actors`

### 34.4 Collision
- [ ] `create_collision_channel`
- [ ] `create_collision_profile`
- [ ] `configure_channel_responses`
- [ ] `configure_object_type`
- [ ] `configure_trace_channel`
- [ ] `set_actor_collision_profile`
- [ ] `set_component_collision_profile`

### 34.5 Physics Materials
- [ ] `create_physical_material`
- [ ] `set_friction`, `set_restitution`, `set_density`
- [ ] `configure_surface_type`
- [ ] `assign_physical_material`

### 34.6 Subsystems
- [ ] `create_game_instance_subsystem`
- [ ] `create_world_subsystem`
- [ ] `create_local_player_subsystem`
- [ ] `create_engine_subsystem`
- [ ] `configure_subsystem_tick`

### 34.7 Async & Timers
- [ ] `set_timer`, `clear_timer`, `pause_timer`
- [ ] `create_latent_action`
- [ ] `create_async_action`
- [ ] `create_gameplay_task`
- [ ] `configure_task_priority`

### 34.8 Delegates & Interfaces
- [ ] `create_event_dispatcher`
- [ ] `bind_to_event`, `unbind_from_event`
- [ ] `broadcast_event`
- [ ] `create_delegate`, `bind_delegate`
- [ ] `create_blueprint_interface`
- [ ] `add_interface_function`
- [ ] `implement_interface`
- [ ] `call_interface_function`

---

## Phase 35: Additional Gameplay Systems

**Goal**: Common gameplay patterns and systems.

### 35.1 Targeting System
- [ ] `create_targeting_component`
- [ ] `configure_lock_on_target`
- [ ] `set_target_priority`
- [ ] `configure_target_switching`
- [ ] `configure_soft_lock`
- [ ] `configure_aim_assist`

### 35.2 Checkpoint System
- [ ] `create_checkpoint_actor`
- [ ] `configure_checkpoint_data`
- [ ] `save_checkpoint`, `load_checkpoint`
- [ ] `configure_checkpoint_respawn`

### 35.3 Objective System
- [ ] `create_objective`
- [ ] `set_objective_state` (locked, active, completed, failed)
- [ ] `configure_objective_markers`
- [ ] `create_objective_tracker_widget`
- [ ] `configure_objective_progression`

### 35.4 World Markers/Ping System
- [ ] `create_world_marker`
- [ ] `create_ping_system`
- [ ] `configure_marker_widget`
- [ ] `configure_marker_3d_2d`
- [ ] `configure_marker_distance`
- [ ] `configure_marker_occlusion`

### 35.5 Photo Mode
- [ ] `enable_photo_mode`
- [ ] `configure_photo_mode_camera`
- [ ] `configure_photo_mode_filters`
- [ ] `configure_photo_mode_poses`
- [ ] `take_photo_mode_screenshot`
- [ ] `configure_photo_mode_ui`

### 35.6 Quest/Dialogue System
- [ ] `create_quest_data_asset`
- [ ] `create_quest_manager`
- [ ] `start_quest`, `complete_quest_objective`, `track_quest`
- [ ] `create_dialogue_tree`
- [ ] `add_dialogue_node`, `add_dialogue_choice`
- [ ] `configure_dialogue_conditions`
- [ ] `play_dialogue`

### 35.7 Instancing & HLOD
- [ ] `create_instanced_static_mesh_component`
- [ ] `create_hierarchical_instanced_static_mesh`
- [ ] `add_instance`, `remove_instance`, `update_instance_transform`
- [ ] `configure_instance_culling`, `configure_instance_lod`
- [ ] `create_hlod_layer`
- [ ] `configure_hlod_settings`
- [ ] `add_actors_to_hlod`, `build_hlod`
- [ ] `configure_hlod_transition`

### 35.8 Localization
- [ ] `create_string_table`
- [ ] `add_string_entry`
- [ ] `configure_localization_target`
- [ ] `import_localization`, `export_localization`
- [ ] `set_culture`
- [ ] `get_localized_string`

### 35.9 Scalability
- [ ] `create_device_profile`
- [ ] `configure_scalability_group`
- [ ] `set_cvar_for_profile`
- [ ] `configure_platform_settings`
- [ ] `set_quality_level`

---

# Plugin Integration Phases

---

## Phase 36: Character & Avatar Plugins

### 36.1 MetaHuman
- [ ] `import_metahuman`, `list_available_metahumans`, `spawn_metahuman_actor`
- [ ] `configure_metahuman_component`, `set_lod_settings`, `configure_body_type`
- [ ] Face: `get_face_parameters`, `set_face_parameter`, `set_skin_tone`, `set_eye_color`, `set_hair_style`, `set_hair_color`, `set_eyebrow_style`, `set_teeth_configuration`, `set_makeup`
- [ ] Body: `set_body_proportions`, `set_body_type`, `set_height`
- [ ] DNA/Rig: `export_metahuman_dna`, `create_custom_rig_logic`, `configure_control_rig_for_metahuman`
- [ ] LOD: `configure_metahuman_lod_bias`, `enable_disable_features_for_performance`

### 36.2 Ready Player Me
- [ ] `load_avatar_from_url`, `load_avatar_from_glb`
- [ ] `configure_avatar_component`
- [ ] `setup_avatar_skeleton_mapping`
- [ ] `apply_avatar_to_character`
- [ ] `configure_avatar_lod`

### 36.3 Mutable (Character Customization)
- [ ] `create_customizable_object`, `add_component_mesh`, `add_component_parameter`
- [ ] `create_customizable_instance`
- [ ] `set_int_parameter`, `set_float_parameter`, `set_color_parameter`, `set_projector_parameter`
- [ ] `update_skeletal_mesh`
- [ ] `bake_customizable_instance`, `configure_bake_settings`

### 36.4 Groom/Hair System
- [ ] `create_groom_asset`, `import_groom` (alembic, cache), `configure_groom_lod`
- [ ] `create_groom_binding`, `bind_groom_to_skeletal_mesh`
- [ ] `configure_hair_simulation`, `set_hair_stiffness`, `set_hair_damping`, `configure_hair_collision`
- [ ] `configure_hair_material`, `set_hair_color`, `set_hair_roughness`

---

## Phase 37: Asset & Content Plugins

### 37.1 Quixel Bridge / Megascans / Fab
- [ ] `connect_to_bridge`, `list_bridge_assets`, `list_downloaded_assets`
- [ ] `import_megascan_surface`, `import_megascan_3d_asset`, `import_megascan_3d_plant`, `import_megascan_decal`, `import_megascan_atlas`, `import_megascan_brush`
- [ ] `configure_import_settings`, `set_lod_generation`, `set_material_blend_mode`, `configure_nanite_import`, `configure_virtual_texture`
- [ ] `search_megascan_library`, `filter_by_category`, `filter_by_biome`, `download_asset`
- [ ] Fab: `browse_fab_assets`, `download_fab_asset`, `import_fab_asset`

### 37.2 Interchange Framework
- [ ] `create_interchange_pipeline`, `add_pipeline_step`, `configure_pipeline_settings`
- [ ] `register_custom_translator`, `configure_translator_settings`
- [ ] `import_with_interchange`, `configure_import_asset_type`, `set_reimport_strategy`
- [ ] `import_fbx_interchange`, `import_gltf_interchange`, `import_usd_interchange`, `import_obj_interchange`

### 37.3 USD
- [ ] Stage: `create_usd_stage`, `open_usd_stage`, `save_usd_stage`, `close_usd_stage`
- [ ] Prims: `create_usd_prim`, `set_prim_transform`, `set_prim_attribute`, `add_prim_reference`, `add_prim_payload`
- [ ] Layers: `create_usd_layer`, `add_sublayer`, `set_edit_target`, `mute_layer`
- [ ] Export: `export_level_to_usd`, `export_actors_to_usd`, `configure_usd_export_options`
- [ ] Import: `import_usd_to_level`, `configure_usd_import_options`, `import_usd_animations`
- [ ] Live: `enable_usd_live_edit`, `configure_usd_sync`

### 37.4 Alembic
- [ ] `import_alembic_file`, `configure_alembic_import` (geometry, groom, animation)
- [ ] `set_alembic_sampling`, `set_alembic_frame_range`
- [ ] `create_geometry_cache_track`, `configure_cache_playback`
- [ ] `import_alembic_groom`, `bind_groom_to_mesh`

### 37.5 glTF
- [ ] `import_gltf`, `import_glb`, `configure_gltf_import_options`, `set_gltf_material_import`
- [ ] `export_to_gltf`, `export_to_glb`, `configure_gltf_export_options`, `set_draco_compression`

### 37.6 Substance Plugin
- [ ] `import_sbsar_file`, `create_substance_graph_instance`
- [ ] `get_substance_parameters`, `set_substance_parameter`, `randomize_substance_seed`
- [ ] `render_substance_textures`, `configure_output_size`, `export_substance_outputs`
- [ ] `create_material_from_substance`, `update_substance_material`

### 37.7 Houdini Engine
- [ ] `import_hda`, `instantiate_hda`, `list_hda_parameters`
- [ ] Parameters: `set_hda_float_parameter`, `set_hda_int_parameter`, `set_hda_string_parameter`, `set_hda_toggle_parameter`, `set_hda_ramp_parameter`
- [ ] Input: `set_hda_geometry_input`, `set_hda_curve_input`, `set_hda_world_input`
- [ ] Cooking: `cook_hda`, `recook_hda`, `configure_cook_options`
- [ ] Output: `get_hda_output_meshes`, `get_hda_output_instances`, `bake_hda_to_actors`, `bake_hda_to_blueprint`
- [ ] Sessions: `start_houdini_session`, `stop_houdini_session`, `configure_session_type`

### 37.8 SpeedTree
- [ ] `import_speedtree_model`, `configure_speedtree_import`
- [ ] `configure_speedtree_wind`, `set_wind_parameters`
- [ ] `configure_speedtree_lod`, `set_billboard_settings`
- [ ] `configure_speedtree_material`, `set_subsurface_color`

### 37.9 Datasmith/CAD
- [ ] `import_datasmith_file`, `configure_datasmith_options`
- [ ] `import_cad_file`, `configure_tessellation`
- [ ] `import_revit`, `import_sketchup`

---

## Phase 38: Audio Middleware Plugins

### 38.1 Wwise
- [ ] Project: `connect_wwise_project`, `refresh_wwise_project`, `generate_sound_banks`
- [ ] Events: `list_wwise_events`, `post_wwise_event`, `post_wwise_event_at_location`, `post_wwise_event_attached`, `stop_wwise_event`
- [ ] Game Syncs: `set_rtpc_value`, `set_wwise_switch`, `set_wwise_state`, `post_wwise_trigger`
- [ ] Spatial: `create_ak_spatial_audio_volume`, `configure_room`, `configure_portal`, `set_room_reverb`, `configure_geometry`
- [ ] Components: `add_ak_component`, `configure_ak_component`, `set_ak_listener`
- [ ] Environment: `set_ak_environment`, `add_ak_reverb_volume`
- [ ] Banks: `load_soundbank`, `unload_soundbank`, `configure_auto_load`
- [ ] Profiling: `start_wwise_profiler_capture`, `stop_wwise_profiler_capture`

### 38.2 FMOD
- [ ] Project: `connect_fmod_project`, `refresh_fmod_banks`
- [ ] Events: `list_fmod_events`, `play_fmod_event`, `play_fmod_event_at_location`, `play_fmod_event_attached`, `stop_fmod_event`, `stop_all_fmod_events`
- [ ] Parameters: `set_fmod_global_parameter`, `set_fmod_event_parameter`, `get_fmod_parameter_value`
- [ ] Snapshots: `start_fmod_snapshot`, `stop_fmod_snapshot`
- [ ] Buses: `set_fmod_bus_volume`, `set_fmod_bus_paused`, `set_fmod_bus_muted`, `set_fmod_vca_volume`
- [ ] Banks: `load_fmod_bank`, `unload_fmod_bank`, `get_bank_loading_state`
- [ ] Components: `add_fmod_audio_component`, `configure_fmod_component`

### 38.3 Bink Video
- [ ] `create_bink_media_player`, `open_bink_video`
- [ ] `play_bink`, `pause_bink`, `stop_bink`, `seek_bink`
- [ ] `set_bink_loop`, `set_bink_audio_tracks`, `configure_bink_texture`

---

## Phase 39: Motion Capture & Live Link Plugins

### 39.1 Live Link (Core)
- [ ] Sources: `add_livelink_source`, `remove_livelink_source`, `list_livelink_sources`, `configure_livelink_source`
- [ ] Presets: `create_livelink_preset`, `apply_livelink_preset`, `save_livelink_preset`
- [ ] Subjects: `list_livelink_subjects`, `get_subject_data`, `set_subject_enabled`, `configure_subject_settings`
- [ ] Roles: `set_livelink_role`, `configure_role_mapping`
- [ ] Retargeting: `create_livelink_retarget_asset`, `configure_retarget_bones`
- [ ] Timecode: `configure_livelink_timecode`, `synchronize_livelink_sources`

### 39.2 Live Link Face (iOS)
- [ ] `configure_livelink_face_source`, `set_face_source_ip`
- [ ] `configure_arkit_blendshape_mapping`, `map_blendshape_to_morph`
- [ ] `configure_head_rotation_mapping`, `configure_eye_tracking_mapping`
- [ ] `set_neutral_pose`, `configure_sensitivity_multiplier`

### 39.3 OptiTrack (Motive)
- [ ] `connect_optitrack_server`, `configure_optitrack_settings`
- [ ] `set_bone_naming_convention`, `configure_skeleton_mapping`, `configure_rigid_body_tracking`
- [ ] `list_optitrack_skeletons`, `list_optitrack_rigid_bodies`, `assign_optitrack_to_actor`

### 39.4 Vicon
- [ ] `connect_vicon_datastream`, `configure_vicon_settings`
- [ ] `list_vicon_subjects`, `get_vicon_subject_data`, `configure_vicon_retargeting`

### 39.5 Rokoko
- [ ] `connect_rokoko_studio`, `configure_rokoko_settings`
- [ ] `list_rokoko_actors`, `assign_rokoko_to_character`, `configure_rokoko_mapping`
- [ ] Props: `list_rokoko_props`, `assign_rokoko_prop`
- [ ] Face: `configure_rokoko_face`, `map_rokoko_blendshapes`

### 39.6 Xsens MVN
- [ ] `connect_xsens_mvn`, `configure_xsens_streaming`, `map_xsens_skeleton`, `assign_xsens_to_character`

---

## Phase 40: Virtual Production Plugins

### 40.1 nDisplay
- [ ] Cluster: `create_ndisplay_config`, `add_cluster_node`, `configure_cluster_node`, `set_primary_node`
- [ ] Viewports: `create_viewport`, `configure_viewport_region`, `set_viewport_camera`, `configure_viewport_projection`
- [ ] Projection: `set_projection_policy`, `configure_projection_mesh`, `configure_projection_frustum`, `import_mpcdi_file`
- [ ] ICVFX: `create_icvfx_camera`, `configure_inner_frustum`, `configure_outer_region`, `set_chromakey_color`, `configure_distortion_correction`
- [ ] Sync: `configure_genlock`, `configure_frame_sync`, `set_swap_sync_policy`
- [ ] Color: `configure_per_viewport_color_grading`, `set_viewport_ocio_config`
- [ ] Stage: `create_stage_actor`, `configure_stage_geometry`, `configure_light_cards`
- [ ] Runtime: `switch_ndisplay_config`, `set_viewport_enabled`

### 40.2 Composure
- [ ] `create_composure_actor`, `add_composure_layer`, `configure_layer_blend`
- [ ] Elements: `add_cg_layer`, `add_media_layer`, `add_transform_pass`, `add_compositing_pass`
- [ ] Keying: `add_chroma_keyer`, `configure_chroma_key_color`, `configure_key_settings`
- [ ] Transform: `configure_layer_transform`, `add_distortion_correction`
- [ ] Output: `configure_composure_output`, `set_output_resolution`, `enable_output_to_render_target`

### 40.3 OpenColorIO (OCIO)
- [ ] `load_ocio_config`, `set_active_ocio_config`, `list_color_spaces`, `list_displays`, `list_views`
- [ ] `set_working_color_space`, `configure_viewport_display_transform`, `add_color_transform_to_viewport`
- [ ] `set_texture_color_space`, `configure_material_color_space`

### 40.4 Remote Control
- [ ] Presets: `create_remote_control_preset`, `add_property_to_preset`, `add_function_to_preset`, `expose_actor_properties`, `expose_blueprint_functions`
- [ ] Groups: `create_preset_group`, `add_property_to_group`
- [ ] Web: `start_web_remote_control_server`, `configure_web_server`, `set_cors_settings`
- [ ] API: `register_custom_route`, `configure_api_permissions`
- [ ] Bindings: `create_remote_control_binding`, `bind_to_protocol`

### 40.5 DMX
- [ ] Library: `create_dmx_library`, `add_dmx_universe`, `configure_universe_settings`
- [ ] Fixtures: `add_dmx_fixture_type`, `configure_fixture_modes`, `add_fixture_function`, `add_dmx_fixture_patch`
- [ ] Ports: `create_dmx_input_port`, `create_dmx_output_port`, `configure_port_protocol`, `set_port_ip_address`
- [ ] Components: `add_dmx_component`, `configure_dmx_receive`, `configure_dmx_transmit`
- [ ] GDTF/MVR: `import_gdtf_fixture`, `configure_gdtf_settings`, `import_mvr_scene`, `export_mvr_scene`

### 40.6 OSC
- [ ] Server: `create_osc_server`, `set_osc_server_port`, `start_osc_server`, `stop_osc_server`
- [ ] Client: `create_osc_client`, `set_osc_client_target`
- [ ] Messages: `send_osc_message`, `send_osc_bundle`, `register_osc_address_handler`
- [ ] Binding: `bind_osc_to_property`, `bind_osc_to_function`

### 40.7 MIDI
- [ ] Devices: `list_midi_devices`, `open_midi_input`, `open_midi_output`, `close_midi_device`
- [ ] Messages: `send_midi_note_on`, `send_midi_note_off`, `send_midi_control_change`, `send_midi_program_change`, `send_midi_pitch_bend`
- [ ] Events: `register_midi_note_handler`, `register_midi_cc_handler`
- [ ] Binding: `bind_midi_to_property`, `bind_midi_cc_to_float`

### 40.8 Timecode
- [ ] `set_timecode_provider`, `configure_timecode_framerate`
- [ ] `configure_genlock_source`, `set_custom_timecode`, `configure_timecode_offset`
- [ ] `configure_ltc_input`, `configure_ltc_output`

---

## Phase 41: XR Plugins (VR/AR/MR)

### 41.1 OpenXR
- [ ] System: `get_openxr_runtime_name`, `configure_openxr_settings`
- [ ] Tracking: `configure_tracking_origin`, `get_hmd_transform`, `get_controller_transform`, `configure_hand_tracking`
- [ ] Actions: `create_openxr_action_set`, `add_openxr_action`, `bind_action_to_controller`, `configure_action_binding`
- [ ] Extensions: `enable_openxr_extension`, `configure_passthrough`, `configure_hand_mesh`
- [ ] Haptics: `trigger_haptic_feedback`, `configure_haptic_settings`

### 41.2 Meta Quest
- [ ] Platform: `configure_quest_settings`, `set_cpu_gpu_level`, `configure_foveated_rendering`, `configure_compositor_layers`
- [ ] Passthrough: `enable_passthrough`, `configure_passthrough_style`, `create_passthrough_layer`
- [ ] Anchors: `create_spatial_anchor`, `save_spatial_anchor`, `load_spatial_anchors`, `share_spatial_anchor`
- [ ] Scene: `enable_scene_capture`, `get_room_layout`, `get_scene_planes`, `get_scene_volumes`
- [ ] Hand: `enable_quest_hand_tracking`, `get_hand_skeleton`, `get_hand_gestures`, `configure_hand_mesh`
- [ ] Body: `enable_quest_body_tracking`, `get_body_skeleton`
- [ ] Face: `enable_quest_face_tracking`, `get_face_expressions`
- [ ] Eye: `enable_quest_eye_tracking`, `get_eye_gaze`

### 41.3 SteamVR
- [ ] System: `configure_steamvr_settings`, `get_steamvr_runtime_version`
- [ ] Chaperone: `configure_chaperone_bounds`, `get_chaperone_bounds`, `set_chaperone_color`
- [ ] Controllers: `configure_controller_bindings`, `get_controller_model`
- [ ] Overlays: `create_steamvr_overlay`, `set_overlay_transform`, `set_overlay_texture`
- [ ] Skeletal: `configure_skeletal_tracking`, `get_skeletal_data`

### 41.4 Apple ARKit
- [ ] Session: `configure_arkit_session`, `start_arkit_session`, `pause_arkit_session`
- [ ] Tracking: `configure_world_tracking`, `configure_image_tracking`, `configure_object_tracking`, `configure_body_tracking`, `configure_face_tracking`
- [ ] Anchors: `get_tracked_planes`, `create_arkit_anchor`, `get_tracked_images`, `get_tracked_objects`
- [ ] Occlusion: `enable_people_occlusion`, `enable_scene_depth`, `configure_lidar_meshing`
- [ ] Environment: `capture_environment_probe`, `get_light_estimation`
- [ ] Face: `get_face_geometry`, `get_face_blendshapes`, `get_eye_tracking`

### 41.5 Google ARCore
- [ ] Session: `configure_arcore_session`, `check_arcore_availability`
- [ ] Tracking: `configure_arcore_world_tracking`, `configure_arcore_image_tracking`, `configure_arcore_face_tracking`
- [ ] Planes: `get_arcore_planes`, `configure_plane_detection`
- [ ] Depth: `enable_arcore_depth`, `get_depth_image`
- [ ] Cloud: `host_cloud_anchor`, `resolve_cloud_anchor`
- [ ] Geospatial: `enable_geospatial`, `get_geospatial_pose`, `create_geospatial_anchor`

### 41.6 Varjo
- [ ] `configure_varjo_settings`, `set_varjo_foveated_rendering`
- [ ] MR: `enable_varjo_video_passthrough`, `configure_passthrough_blend`, `configure_depth_test`
- [ ] Eye: `enable_varjo_eye_tracking`, `get_eye_gaze_data`, `configure_gaze_data_output`

### 41.7 HoloLens
- [ ] `configure_hololens_settings`, `set_holographic_remoting`
- [ ] Spatial: `configure_spatial_mapping`, `get_spatial_mesh`, `create_spatial_anchor`
- [ ] QR: `enable_qr_tracking`, `get_tracked_qr_codes`
- [ ] Hand: `configure_hand_tracking`, `get_hand_joint_data`, `configure_hand_mesh`
- [ ] Voice: `register_voice_command`, `configure_voice_recognition`

---

## Phase 42: AI & NPC Plugins

### 42.1 Convai
- [ ] Characters: `create_convai_character`, `configure_character_backstory`, `set_character_personality`
- [ ] Conversation: `start_conversation`, `send_text_to_character`, `send_voice_to_character`, `get_character_response`
- [ ] Actions: `configure_character_actions`, `trigger_character_action`
- [ ] Lip Sync: `configure_convai_lipsync`, `map_visemes_to_morphs`

### 42.2 Inworld AI
- [ ] Characters: `create_inworld_character`, `configure_character_brain`, `set_character_goals`
- [ ] Integration: `connect_inworld_service`, `configure_inworld_settings`
- [ ] Conversation: `start_inworld_session`, `send_player_message`, `receive_character_response`
- [ ] Emotions: `get_character_emotion`, `configure_emotion_response`

### 42.3 NVIDIA ACE
- [ ] `configure_audio2face`
- [ ] `process_audio_to_blendshapes`
- [ ] `stream_audio_to_face`
- [ ] `configure_blendshape_mapping`
- [ ] `set_emotion_weights`

---

## Phase 43: Online Services Plugins

### 43.1 Online Subsystem (Core)
- [ ] `get_online_subsystem`, `configure_default_subsystem`
- [ ] Identity: `login`, `logout`, `get_player_nickname`, `get_unique_net_id`
- [ ] Sessions: `create_session`, `find_sessions`, `join_session`, `destroy_session`, `register_player`, `unregister_player`, `get_session_state`
- [ ] Friends: `get_friends_list`, `send_friend_invite`, `accept_friend_invite`, `get_friend_presence`
- [ ] Achievements: `get_achievements`, `unlock_achievement`, `get_achievement_progress`, `write_achievement_progress`
- [ ] Leaderboards: `read_leaderboard`, `write_leaderboard`, `get_leaderboard_entries`
- [ ] Stats: `read_stats`, `write_stats`
- [ ] Voice: `configure_voice_chat`, `mute_player`, `set_voice_volume`

### 43.2 Epic Online Services (EOS)
- [ ] Platform: `initialize_eos`, `configure_eos_settings`
- [ ] Auth: `login_eos`, `link_account`, `get_eos_product_user_id`, `login_with_connect`, `create_device_id`
- [ ] Lobby: `create_eos_lobby`, `find_eos_lobbies`, `join_eos_lobby`, `leave_eos_lobby`, `update_lobby_attributes`
- [ ] P2P: `configure_p2p`, `create_socket`, `send_p2p_message`, `receive_p2p_message`
- [ ] Voice: `join_eos_voice_room`, `leave_eos_voice_room`, `mute_eos_player`
- [ ] Achievements: `define_eos_achievements`, `unlock_eos_achievement`, `get_player_achievements`
- [ ] Stats: `ingest_eos_stat`, `query_player_stats`
- [ ] Leaderboards: `query_eos_leaderboard`, `submit_leaderboard_score`
- [ ] Storage: `write_player_storage`, `read_player_storage`, `delete_player_storage`, `query_title_storage`, `read_title_storage_file`
- [ ] Anti-Cheat: `configure_eos_anti_cheat`, `start_anti_cheat_session`

### 43.3 Steam
- [ ] Auth: `login_steam`, `get_steam_id`, `get_persona_name`
- [ ] Friends: `get_steam_friends`, `get_friend_avatar`, `get_friend_game_info`
- [ ] Achievements: `set_steam_achievement`, `clear_steam_achievement`, `indicate_achievement_progress`, `store_stats`
- [ ] Leaderboards: `find_steam_leaderboard`, `upload_leaderboard_score`, `download_leaderboard_entries`
- [ ] Workshop: `create_workshop_item`, `update_workshop_item`, `subscribe_workshop_item`, `get_subscribed_items`
- [ ] Cloud: `write_steam_cloud_file`, `read_steam_cloud_file`, `delete_steam_cloud_file`
- [ ] Rich Presence: `set_steam_rich_presence`, `clear_steam_rich_presence`
- [ ] Overlay: `activate_steam_overlay`, `activate_overlay_to_store`, `activate_overlay_to_user`
- [ ] Input: `configure_steam_input`, `get_steam_controller_state`
- [ ] Networking: `configure_steam_networking`, `send_steam_p2p_packet`

### 43.4 PlayStation Network (Requires External SDK)

> **Prerequisites**: PSN SDK must be installed and configured via PlayStation Partners portal BEFORE MCP can interact with these APIs. MCP can only configure/use these once the SDK is available in the project.

- [ ] `configure_psn_settings` (assumes SDK already installed)
- [ ] `login_psn`, `get_psn_user_id`
- [ ] `unlock_trophy`, `get_trophy_pack_info`
- [ ] `start_activity`, `end_activity`
- [ ] `create_player_session`, `join_player_session`
- [ ] `configure_psn_voice`

### 43.5 Xbox Live (Requires External SDK)

> **Prerequisites**: GDK must be installed and project registered in Partner Center BEFORE MCP can interact with these APIs.

- [ ] `configure_xbox_settings` (assumes GDK already installed)
- [ ] `login_xbox`, `get_xbox_user_id`, `get_gamertag`
- [ ] `unlock_xbox_achievement`, `get_achievement_status`
- [ ] `set_xbox_presence`, `get_friend_presence`
- [ ] `create_xbox_session`, `join_xbox_session`
- [ ] `write_connected_storage`, `read_connected_storage`

### 43.6 Nintendo Switch (Requires External SDK)

> **Prerequisites**: Nintendo SDK must be installed and project registered in Nintendo Developer Portal BEFORE MCP can interact with these APIs.

- [ ] `configure_switch_settings` (assumes SDK already installed)
- [ ] `get_switch_user`, `open_account_selector`
- [ ] `configure_nso`, `set_play_report`
- [ ] `configure_joycon`, `configure_hd_rumble`

---

## Phase 44: Streaming & Distribution Plugins

### 44.1 Pixel Streaming
- [ ] Server: `enable_pixel_streaming`, `configure_pixel_streaming_settings`
- [ ] Encoder: `set_encoder_type`, `set_target_bitrate`, `set_max_framerate`, `set_resolution`, `configure_quality_settings`
- [ ] Signaling: `configure_signaling_server`, `set_stun_server`, `set_turn_server`
- [ ] Input: `configure_input_handling`, `set_input_type`, `configure_touch_controller`
- [ ] Streaming: `start_streaming`, `stop_streaming`, `force_keyframe`
- [ ] Multi-Stream: `configure_multi_user`, `set_matchmaker_url`, `configure_sfu_connection`

### 44.2 Media Streaming
- [ ] RTSP: `create_rtsp_source`, `configure_rtsp_stream`
- [ ] NDI: `create_ndi_source`, `create_ndi_output`, `configure_ndi_settings`
- [ ] SRT: `create_srt_source`, `create_srt_output`, `configure_srt_settings`
- [ ] Blackmagic: `list_blackmagic_devices`, `create_blackmagic_input`, `create_blackmagic_output`, `configure_blackmagic_settings`
- [ ] AJA: `list_aja_devices`, `create_aja_input`, `create_aja_output`, `configure_aja_settings`

---

## Phase 45: Utility Plugins

### 45.1 Python Scripting
- [ ] `execute_python_script`, `execute_python_string`, `execute_python_file`
- [ ] `configure_python_paths`, `install_python_package`, `list_python_packages`
- [ ] `create_python_editor_utility`, `register_python_command`, `unregister_python_command`

### 45.2 Editor Scripting Utilities
- [ ] `create_editor_utility_widget`, `create_editor_utility_blueprint`, `create_asset_action`, `run_editor_utility`
- [ ] `create_editor_mode`, `register_editor_mode`, `configure_mode_toolkit`
- [ ] Menus: `add_menu_entry`, `add_toolbar_button`, `create_submenu`, `register_context_menu`
- [ ] Commands: `register_editor_command`, `bind_command_to_action`, `execute_editor_command`

### 45.3 Modeling Tools Editor Mode
- [ ] `activate_modeling_tool`, `deactivate_modeling_tool`
- [ ] PolyEdit: `select_mesh_elements`, `transform_selection`, `extrude_selection`, `inset_selection`, `bevel_selection`, `bridge_edges`, `fill_hole`, `weld_edges`, `split_edges`, `triangulate`, `flip_normals`
- [ ] Sculpt: `set_sculpt_brush`, `set_brush_size`, `set_brush_strength`, `set_brush_falloff`, `sculpt_stroke`
- [ ] Deform: `apply_lattice_deform`, `apply_bend_deform`, `apply_twist_deform`
- [ ] UV: `open_uv_editor`, `select_uv_islands`, `transform_uvs`, `pack_uvs`, `unwrap_uvs`
- [ ] Mesh Ops: `simplify_mesh_tool`, `remesh_tool`, `boolean_tool`, `merge_meshes_tool`, `separate_meshes_tool`

### 45.4 Common UI Plugin
- [ ] `configure_ui_input_config`, `set_input_mode`, `configure_analog_cursor`
- [ ] `create_common_activatable_widget`, `create_common_button_base`, `create_common_tab_list`, `create_common_action_widget`
- [ ] `configure_navigation_rules`, `set_focus_widget`, `configure_back_action`
- [ ] `register_ui_action`, `bind_ui_action_to_input`

### 45.5 Paper2D
- [ ] Sprites: `create_sprite`, `configure_sprite_source`, `set_sprite_pivot`, `configure_sprite_collision`
- [ ] Flipbooks: `create_flipbook`, `add_flipbook_keyframe`, `set_flipbook_framerate`, `configure_flipbook_loop`
- [ ] Tile Maps: `create_tile_map`, `create_tile_set`, `add_tile_to_set`, `paint_tile`, `fill_tile_region`, `configure_tile_collision`
- [ ] Actors: `spawn_paper_sprite_actor`, `spawn_paper_flipbook_actor`, `configure_paper_character`

### 45.6 Procedural Mesh Component
- [ ] `create_procedural_mesh_component`
- [ ] `create_mesh_section`, `update_mesh_section`, `clear_mesh_section`, `clear_all_mesh_sections`
- [ ] `set_mesh_vertices`, `set_mesh_triangles`, `set_mesh_normals`, `set_mesh_uvs`, `set_mesh_colors`, `set_mesh_tangents`
- [ ] `set_collision_from_mesh`, `add_collision_convex_mesh`, `clear_collision_convex_meshes`
- [ ] `convert_to_static_mesh`

### 45.7 Variant Manager
- [ ] `create_variant_set`, `add_variant`, `configure_variant_properties`
- [ ] `set_variant_dependencies`, `set_exclusive_variants`
- [ ] `capture_variant_thumbnail`, `set_variant_thumbnail`
- [ ] `activate_variant`, `get_active_variants`
- [ ] `export_variant_configuration`

---

## Phase 46: Physics & Destruction Plugins

### 46.1 Chaos Destruction
- [ ] Collection: `create_geometry_collection`, `add_geometry_to_collection`, `remove_geometry_from_collection`
- [ ] Fracturing: `apply_uniform_fracture`, `apply_clustered_fracture`, `apply_radial_fracture`, `apply_planar_fracture`, `apply_brick_fracture`, `apply_mesh_fracture`, `configure_fracture_settings`
- [ ] Clustering: `set_cluster_group`, `configure_cluster_level`, `auto_cluster`
- [ ] Damage: `configure_damage_threshold`, `apply_damage`, `enable_strain_damage`
- [ ] Materials: `set_interior_material`, `configure_interior_uv_scale`
- [ ] Physics: `configure_rigid_body_settings`, `set_sleeping_threshold`, `configure_collision_filter`
- [ ] Fields: `add_anchor_field`, `remove_anchor_field`, `create_field_system`, `add_radial_falloff_field`, `add_uniform_vector_field`, `add_radial_vector_field`, `add_plane_falloff_field`, `add_noise_field`, `add_kill_field`

### 46.2 Chaos Vehicles
- [ ] Setup: `create_chaos_wheeled_vehicle`, `create_chaos_hover_vehicle`
- [ ] Wheels: `add_wheel_setup`, `configure_wheel_physics`, `set_wheel_radius`, `set_wheel_width`, `configure_wheel_suspension`, `configure_wheel_friction`, `configure_wheel_brake`, `configure_anti_roll_bar`
- [ ] Engine: `configure_engine_torque_curve`, `set_max_rpm`, `set_engine_idle_rpm`, `configure_throttle_response`
- [ ] Transmission: `configure_transmission`, `set_gear_ratios`, `set_final_drive_ratio`, `configure_gear_change_time`
- [ ] Steering: `configure_steering_curve`, `set_max_steering_angle`, `configure_ackermann_steering`
- [ ] Aero: `configure_drag`, `configure_downforce`
- [ ] Effects: `configure_skid_marks`, `configure_tire_smoke`, `configure_engine_audio`, `configure_exhaust_vfx`

### 46.3 Chaos Cloth
- [ ] `create_cloth_asset`, `configure_cloth_config`
- [ ] Properties: `set_mass`, `set_edge_stiffness`, `set_bending_stiffness`, `set_area_stiffness`, `configure_damping`, `configure_collision_thickness`, `configure_friction`
- [ ] Painting: `paint_max_distance`, `paint_backstop_distance`, `paint_backstop_radius`
- [ ] LOD: `configure_cloth_lod`, `set_lod_transition`

### 46.4 Chaos Flesh
- [ ] `create_flesh_asset`, `configure_flesh_deformer`
- [ ] `configure_flesh_simulation`, `set_flesh_stiffness`, `set_flesh_damping`
- [ ] `configure_flesh_collision`

---

## Phase 47: Accessibility System

**Goal**: Complete accessibility features for inclusive game design.

**Tool**: `manage_accessibility`

### 47.1 Visual Accessibility
- [ ] `create_colorblind_filter` (protanopia, deuteranopia, tritanopia)
- [ ] `configure_colorblind_simulation`
- [ ] `set_ui_scale`, `set_font_scale`
- [ ] `configure_high_contrast_mode`
- [ ] `set_screen_reader_text` (for UI elements)
- [ ] `configure_text_to_speech`
- [ ] `set_color_coding_alternatives` (shapes, patterns)

### 47.2 Subtitle System
- [ ] `create_subtitle_widget`
- [ ] `configure_subtitle_style` (font, size, color, background)
- [ ] `set_subtitle_position`
- [ ] `configure_speaker_identification`
- [ ] `configure_caption_timing`
- [ ] `add_directional_indicators` (sound direction)
- [ ] `configure_subtitle_presets` (small, medium, large, custom)

### 47.3 Audio Accessibility
- [ ] `configure_mono_audio`
- [ ] `configure_audio_visualization` (visual sound indicators)
- [ ] `create_sound_indicator_widget`
- [ ] `configure_haptic_audio_feedback`
- [ ] `set_audio_balance` (left/right)

### 47.4 Motor Accessibility
- [ ] `configure_control_remapping_ui`
- [ ] `configure_hold_vs_toggle`
- [ ] `configure_input_timing` (QTE timing, combo windows)
- [ ] `configure_auto_aim_strength`
- [ ] `configure_one_handed_mode`
- [ ] `configure_sticky_keys`
- [ ] `configure_input_buffering`

### 47.5 Cognitive Accessibility
- [ ] `configure_difficulty_presets`
- [ ] `configure_objective_reminders`
- [ ] `configure_navigation_assistance`
- [ ] `configure_puzzle_hints`
- [ ] `configure_content_warnings`
- [ ] `configure_motion_sickness_options` (FOV, camera shake, motion blur)

### 47.6 Accessibility Presets
- [ ] `create_accessibility_preset`
- [ ] `apply_accessibility_preset`
- [ ] `export_accessibility_settings`
- [ ] `import_accessibility_settings`
- [ ] `configure_accessibility_menu`

---

## Phase 48: Modding & UGC System

**Goal**: Enable mod support and user-generated content within Unreal Engine.

**Tool**: `manage_modding`

### 48.1 Pak/Mod Loading
- [ ] `configure_mod_loading_paths`
- [ ] `scan_for_mod_paks`
- [ ] `load_mod_pak`, `unload_mod_pak`
- [ ] `get_mod_info` (metadata, version, dependencies)
- [ ] `validate_mod_pak`
- [ ] `configure_mod_load_order`
- [ ] `configure_mod_priority`

### 48.2 Mod Discovery
- [ ] `create_mod_browser_widget`
- [ ] `list_installed_mods`
- [ ] `enable_mod`, `disable_mod`
- [ ] `get_mod_thumbnail`
- [ ] `get_mod_dependencies`
- [ ] `check_mod_compatibility`

### 48.3 Asset Replacement
- [ ] `configure_asset_override_paths`
- [ ] `register_mod_asset_redirect`
- [ ] `get_modded_asset_list`
- [ ] `restore_original_asset`

### 48.4 Mod SDK Generation
- [ ] `export_moddable_headers`
- [ ] `create_mod_template_project`
- [ ] `configure_exposed_classes`
- [ ] `configure_moddable_data_assets`
- [ ] `generate_mod_documentation`

### 48.5 Sandboxing & Security
- [ ] `configure_mod_sandbox`
- [ ] `set_allowed_mod_operations`
- [ ] `configure_mod_memory_limits`
- [ ] `log_mod_activity`
- [ ] `validate_mod_content` (check for exploits)

### 48.6 Steam Workshop Integration (via Steam OSS)
- [ ] `upload_to_workshop` (requires Steam SDK)
- [ ] `download_workshop_item`
- [ ] `get_subscribed_workshop_items`
- [ ] `configure_workshop_item_metadata`
- [ ] `update_workshop_item`

> **Note**: Steam Workshop integration requires Steam SDK to be configured (Phase 43.3). MCP handles the UE-side integration.

---

# Summary

## Statistics

| Category | Phases | Estimated Actions |
|----------|--------|-------------------|
| Completed Foundation (1-4) | 4 | ~160 |
| Infrastructure (5) | 1 | ~20 |
| Content Creation (6-12) | 7 | ~400 |
| Gameplay Systems (13-18) | 6 | ~300 |
| UI/UX (19) | 1 | ~80 |
| Networking & Framework (20-22) | 3 | ~100 |
| World & Environment (23-28) | 6 | ~200 |
| Rendering & Post (29) | 1 | ~80 |
| Cinematics & Media (30) | 1 | ~80 |
| Data & Persistence (31) | 1 | ~50 |
| Build & Deploy (32) | 1 | ~35 |
| Testing & Quality (33) | 1 | ~35 |
| Editor Utilities (34) | 1 | ~60 |
| Additional Systems (35) | 1 | ~80 |
| Plugin: Character (36) | 1 | ~60 |
| Plugin: Asset/Content (37) | 1 | ~150 |
| Plugin: Audio Middleware (38) | 1 | ~80 |
| Plugin: Motion Capture (39) | 1 | ~70 |
| Plugin: Virtual Production (40) | 1 | ~150 |
| Plugin: XR (41) | 1 | ~150 |
| Plugin: AI/NPC (42) | 1 | ~30 |
| Plugin: Online Services (43) | 1 | ~160 |
| Plugin: Streaming (44) | 1 | ~50 |
| Plugin: Utilities (45) | 1 | ~100 |
| Plugin: Physics/Destruction (46) | 1 | ~80 |
| Accessibility (47) | 1 | ~40 |
| Modding & UGC (48) | 1 | ~25 |
| **TOTAL** | **48** | **~2,855** |

## What This Enables

With ~2,855 actions covering all Unreal Engine systems and major plugins:

- **Complete Game Development**: Build any genre (FPS, RPG, Racing, Platformer, etc.)
- **Animated Films**: Full pipeline from character to final render
- **Virtual Production**: LED walls, motion capture, real-time compositing
- **VR/AR Experiences**: All major headsets and AR platforms
- **ArchViz**: CAD import, materials, lighting, walkthroughs
- **Live Events**: DMX, OSC, MIDI, timecode sync
- **Cloud Gaming**: Pixel streaming, multi-user sessions
- **Console Development**: PS5, Xbox, Switch support (once SDKs installed externally)
- **Mobile Development**: iOS ARKit, Android ARCore
- **Accessible Games**: Full accessibility support (colorblind, subtitles, motor, cognitive)
- **Mod-Friendly Games**: Pak loading, mod browser, asset override, workshop integration

---

## Legend

- [x] **Completed**: Feature is implemented and verified.
- [ ] **Planned**: Feature is scheduled for implementation.

---

## Contributing

This roadmap represents a massive undertaking. Contributions are welcome for any phase. See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines.

Each new action requires:
1. TypeScript handler in `src/tools/handlers/`
2. Tool definition in `src/tools/consolidated-tool-definitions.ts`
3. C++ handler in `plugins/McpAutomationBridge/Source/.../Private/`
4. Integration test
