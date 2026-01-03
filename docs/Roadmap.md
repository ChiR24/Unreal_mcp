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

## Phase 6: Geometry & Mesh Creation (Complete)

**Goal**: Enable AI to CREATE actual 3D geometry, not just place existing meshes.

**Tool**: `manage_geometry`

**Status**: All primitives, booleans, modeling operations, deformers, topology, mesh processing, mesh repair, UV operations, collision, LOD, and Nanite conversion implemented.

### 6.1 Primitives
- [x] `create_box`, `create_sphere`, `create_cylinder`, `create_cone`
- [x] `create_torus`, `create_capsule`, `create_plane`, `create_disc`
- [x] `create_stairs`, `create_spiral_stairs`, `create_ring`
- [x] `create_ramp`, `create_arch`, `create_pipe`

### 6.2 Boolean/CSG Operations
- [x] `boolean_union`, `boolean_subtract`, `boolean_intersection`
- [x] `boolean_trim`, `self_union`

### 6.3 Modeling Operations
- [x] `extrude`, `inset`, `outset`
- [x] `bevel`, `offset_faces`, `shell`
- [x] `extrude_along_spline`
- [x] `chamfer`
- [x] `bridge`, `loft`, `sweep`
- [x] `revolve`
- [x] `mirror`, `symmetrize`
- [x] `array_linear`, `array_radial`
- [x] `duplicate_along_spline`
- [x] `loop_cut`, `edge_split`
- [x] `poke`, `triangulate`
- [x] `quadrangulate`

### 6.4 Deformers
- [x] `bend`, `twist`, `taper`
- [x] `noise_deform`, `smooth`
- [x] `stretch`
- [x] `lattice_deform`
- [x] `spherify`, `cylindrify`
- [x] `relax`
- [x] `displace_by_texture`

### 6.5 Mesh Processing
- [x] `subdivide` (PN tessellation)
- [x] `simplify_mesh` (QEM decimation)
- [x] `remesh_uniform`
- [x] `weld_vertices`
- [x] `remove_degenerates`, `fill_holes`
- [x] `flip_normals`, `recalculate_normals`
- [x] `remesh_voxel`
- [x] `merge_vertices`
- [x] `recompute_tangents`

### 6.6 UV Operations
- [x] `auto_uv` (XAtlas)
- [x] `project_uv` (box, planar, cylindrical)
- [x] `unwrap_uv`, `pack_uv_islands`
- [x] `transform_uvs`, `scale_uvs`, `rotate_uvs`

### 6.7 Collision Generation
- [x] `generate_collision` (convex, box, sphere, capsule, decomposition)
- [x] `generate_complex_collision`
- [x] `simplify_collision`

### 6.8 LOD & Output
- [x] `generate_lods`, `set_lod_settings`
- [x] `set_lod_screen_sizes`
- [x] `convert_to_static_mesh` (save DynamicMesh as StaticMesh asset)
- [x] `convert_to_nanite`

### 6.9 Mesh Query
- [x] `get_mesh_info` (vertex/triangle count, UV/normal status)

> **Note**: Geometry Collection for destruction is in Phase 46.1 (Chaos Destruction).

---

## Phase 7: Skeletal Mesh & Rigging (Complete)

**Goal**: Enable creation and editing of animated characters with proper rigs.

**Tool**: `manage_skeleton`

**Status**: All 29 actions fully implemented in TypeScript and C++.

### 7.1 Skeleton Creation
- [x] `get_skeleton_info`, `list_bones`, `list_sockets` (query operations)
- [x] `create_socket`, `configure_socket`
- [x] `create_virtual_bone`
- [x] `rename_bone` (virtual bones only; regular bones require reimport)
- [x] `set_bone_transform` (reference pose modification)
- [x] `create_skeleton` (uses FReferenceSkeletonModifier)
- [x] `add_bone`, `remove_bone`, `set_bone_parent` (uses FReferenceSkeletonModifier)

### 7.2 Skin Weights
- [x] `normalize_weights` (rebuilds mesh)
- [x] `prune_weights` (rebuilds mesh with threshold)
- [x] `auto_skin_weights` (triggers mesh rebuild)
- [x] `set_vertex_weights` (uses FSkinWeightProfileData)
- [x] `copy_weights`, `mirror_weights` (skin weight profile operations)

### 7.3 Physics Asset
- [x] `create_physics_asset`, `list_physics_bodies`
- [x] `add_physics_body` (capsule, sphere, box, convex)
- [x] `configure_physics_body` (mass, damping, collision)
- [x] `add_physics_constraint`
- [x] `configure_constraint_limits`

### 7.4 Cloth Setup (Basic)
> **Note**: Full cloth simulation configuration is in Phase 46.3 (Chaos Cloth). This section covers skeletal mesh cloth binding only.

- [x] `bind_cloth_to_skeletal_mesh`
- [x] `assign_cloth_asset_to_mesh`

### 7.5 Morph Targets
- [x] `create_morph_target`
- [x] `set_morph_target_deltas`
- [x] `import_morph_targets` (lists targets; FBX import via asset pipeline)

---

## Phase 8: Advanced Material Authoring (Complete)

**Goal**: Full material creation and shader authoring capabilities.

**Tool**: `manage_material_authoring`

**Status**: All 39 actions fully implemented in TypeScript and C++.

### 8.1 Material Creation
- [x] `create_material` (surface, deferred_decal, light_function, post_process, UI, volume)
- [x] `set_blend_mode` (opaque, masked, translucent, additive, modulate)
- [x] `set_shading_model` (default_lit, unlit, subsurface, clear_coat, two_sided_foliage, hair, eye, cloth)
- [x] `set_material_domain` (surface, deferred_decal, light_function, post_process, UI, volume)

### 8.2 Material Expressions (Node Graph)
- [x] `add_texture_sample`, `add_texture_coordinate`
- [x] `add_scalar_parameter`, `add_vector_parameter`
- [x] `add_static_switch_parameter`
- [x] `add_math_node` (add, multiply, divide, power, lerp, clamp, etc.)
- [x] `add_world_position`, `add_vertex_normal`, `add_pixel_depth`
- [x] `add_fresnel`, `add_reflection_vector`
- [x] `add_panner`, `add_rotator`
- [x] `add_noise`, `add_voronoi`
- [x] `add_if`, `add_switch`
- [x] `add_custom_expression` (HLSL code)
- [x] `connect_nodes`, `disconnect_nodes`

### 8.3 Material Functions & Layers
- [x] `create_material_function`
- [x] `add_function_input`, `add_function_output`
- [x] `use_material_function`

### 8.4 Material Instances
- [x] `create_material_instance`
- [x] `set_scalar_parameter_value`
- [x] `set_vector_parameter_value`
- [x] `set_texture_parameter_value`

### 8.5 Specialized Materials
- [x] `create_landscape_material`, `add_landscape_layer`, `configure_layer_blend`
- [x] `create_decal_material`
- [x] `create_post_process_material`

### 8.6 Utilities
- [x] `compile_material`
- [x] `get_material_info`

---

## Phase 9: Texture Generation & Processing (Complete)

**Goal**: Procedural texture creation and processing.

**Tool**: `manage_texture`

**Status**: All 21 actions fully implemented in TypeScript and C++.

### 9.1 Procedural Generation
- [x] `create_noise_texture` (perlin, simplex, worley, voronoi)
- [x] `create_gradient_texture` (linear, radial, angular)
- [x] `create_pattern_texture` (checker, grid, brick, tile, dots, stripes)
- [x] `create_normal_from_height` (Sobel, Prewitt, Scharr algorithms)
- [x] `create_ao_from_mesh`

### 9.2 Texture Processing
- [x] `resize_texture`
- [x] `adjust_levels`, `adjust_curves` (LUT-based curve adjustment)
- [x] `blur`, `sharpen`
- [x] `invert`, `desaturate`
- [x] `channel_pack`, `channel_extract` (extract R/G/B/A to grayscale)
- [x] `combine_textures` (blend modes)

### 9.3 Texture Settings
- [x] `set_compression_settings` (TC_Default, TC_Normalmap, TC_Masks, etc.)
- [x] `set_texture_group` (TEXTUREGROUP_World, Character, UI, etc.)
- [x] `set_lod_bias`
- [x] `configure_virtual_texture`
- [x] `set_streaming_priority`

### 9.4 Utility
- [x] `get_texture_info` (width, height, format, compression, mip count, sRGB, etc.)

---

## Phase 10: Complete Animation System (Complete)

**Goal**: Full animation authoring from keyframes to state machines.

**Tool**: `manage_animation_authoring`

**Status**: All 35 actions fully implemented in TypeScript and C++.

### 10.1 Animation Sequences
- [x] `create_animation_sequence`
- [x] `set_sequence_length`
- [x] `add_bone_track`
- [x] `set_bone_key` (location, rotation, scale at time)
- [x] `set_curve_key` (float curve at time)
- [x] `add_notify`, `add_notify_state`
- [x] `add_sync_marker`
- [x] `set_root_motion_settings`
- [x] `set_additive_settings`

### 10.2 Animation Montages
- [x] `create_montage`
- [x] `add_montage_section`
- [x] `add_montage_slot`
- [x] `set_section_timing`
- [x] `add_montage_notify`
- [x] `set_blend_in`, `set_blend_out`
- [x] `link_sections`

### 10.3 Blend Spaces
- [x] `create_blend_space_1d`
- [x] `create_blend_space_2d`
- [x] `add_blend_sample`
- [x] `set_axis_settings`
- [x] `set_interpolation_settings`
- [x] `create_aim_offset`, `add_aim_offset_sample`

### 10.4 Animation Blueprints
- [x] `create_anim_blueprint`
- [x] `add_state_machine`
- [x] `add_state`, `add_transition`
- [x] `set_transition_rules`
- [x] `add_blend_node`
- [x] `add_cached_pose`
- [x] `add_slot_node`
- [x] `add_layered_blend_per_bone`
- [x] `set_anim_graph_node_value`

### 10.5 Control Rig
- [x] `create_control_rig`
- [x] `add_control`
- [x] `add_rig_unit` (FKIK, aim, basic_ik, etc.)
- [x] `connect_rig_elements`
- [x] `create_pose_library`

### 10.6 Retargeting
- [x] `create_ik_rig`
- [x] `add_ik_chain`
- [x] `create_ik_retargeter`
- [x] `set_retarget_chain_mapping`

### 10.7 Utility
- [x] `get_animation_info`

---

## Phase 11: Complete Audio System

**Goal**: Full audio authoring including MetaSounds.

**Tool**: `manage_audio_authoring`

### 11.1 Sound Cues (Expanded)
- [x] `create_sound_cue`
- [x] `add_cue_node` (wave_player, mixer, random, modulator, etc.)
- [x] `connect_cue_nodes`
- [x] `set_cue_attenuation`, `set_cue_concurrency`

### 11.2 MetaSounds
- [x] `create_metasound`
- [x] `add_metasound_node`
- [x] `connect_metasound_nodes`
- [x] `add_metasound_input`, `add_metasound_output`
- [x] `set_metasound_default`

### 11.3 Sound Classes & Mixes
- [x] `create_sound_class`, `set_class_properties`, `set_class_parent`
- [x] `create_sound_mix`, `add_mix_modifier`, `configure_mix_eq`

### 11.4 Attenuation & Spatialization
- [x] `create_attenuation_settings`
- [x] `configure_distance_attenuation`
- [x] `configure_spatialization`
- [x] `configure_occlusion`
- [x] `configure_reverb_send`

### 11.5 Dialogue System
- [x] `create_dialogue_voice`
- [x] `create_dialogue_wave`
- [x] `set_dialogue_context`

### 11.6 Effects
- [x] `create_reverb_effect`
- [x] `create_source_effect_chain`
- [x] `add_source_effect` (filter, eq, chorus, delay, etc.)
- [x] `create_submix_effect`

### 11.7 Utility
- [x] `get_audio_info`

---

## Phase 12: Complete Niagara VFX System

**Goal**: Full Niagara system authoring.

**Tool**: `manage_niagara_authoring`

**Status**: All 35 actions fully implemented in TypeScript and C++.

### 12.1 Systems & Emitters
- [x] `create_niagara_system`
- [x] `create_niagara_emitter`
- [x] `add_emitter_to_system`
- [x] `set_emitter_properties`

### 12.2 Module Library
- [x] `add_spawn_rate_module`, `add_spawn_burst_module`, `add_spawn_per_unit_module`
- [x] `add_initialize_particle_module`
- [x] `add_particle_state_module`
- [x] `add_force_module` (gravity, drag, vortex, point_attraction, curl_noise)
- [x] `add_velocity_module`, `add_acceleration_module`
- [x] `add_size_module`, `add_color_module`
- [x] `add_sprite_renderer_module`
- [x] `add_mesh_renderer_module`
- [x] `add_ribbon_renderer_module`
- [x] `add_light_renderer_module`
- [x] `add_collision_module`
- [x] `add_kill_particles_module`
- [x] `add_camera_offset_module`

### 12.3 Parameters & Data Interfaces
- [x] `add_user_parameter` (float, vector, color, texture, mesh, etc.)
- [x] `set_parameter_value`, `bind_parameter_to_source`
- [x] `add_skeletal_mesh_data_interface`
- [x] `add_static_mesh_data_interface`
- [x] `add_spline_data_interface`
- [x] `add_audio_spectrum_data_interface`
- [x] `add_collision_query_data_interface`

### 12.4 Events & GPU
- [x] `add_event_generator`, `add_event_receiver`
- [x] `configure_event_payload`
- [x] `enable_gpu_simulation`
- [x] `add_simulation_stage`

### 12.5 Utility
- [x] `get_niagara_info`
- [x] `validate_niagara_system`

---

## Phase 13: Gameplay Ability System (GAS)

**Goal**: Complete GAS implementation for abilities, effects, and attributes.

**Tool**: `manage_gas`

### 13.1 Components & Attributes
- [x] `add_ability_system_component`
- [x] `configure_asc` (replication_mode, owner)
- [x] `create_attribute_set`
- [x] `add_attribute` (health, mana, stamina, damage, armor, etc.)
- [x] `set_attribute_base_value`, `set_attribute_clamping`

### 13.2 Gameplay Abilities
- [x] `create_gameplay_ability`
- [x] `set_ability_tags`
- [x] `set_ability_costs`, `set_ability_cooldown`
- [x] `set_ability_targeting`
- [x] `add_ability_task`
- [x] `set_activation_policy`, `set_instancing_policy`

### 13.3 Gameplay Effects
- [x] `create_gameplay_effect`
- [x] `set_effect_duration` (instant, infinite, duration)
- [x] `add_effect_modifier` (attribute, operation, magnitude)
- [x] `set_modifier_magnitude` (scalable, attribute_based, set_by_caller)
- [x] `add_effect_execution_calculation`
- [x] `add_effect_cue`
- [x] `set_effect_stacking`
- [x] `set_effect_tags` (granted, application, removal)

### 13.4 Gameplay Cues
- [x] `create_gameplay_cue_notify` (static, actor)
- [x] `configure_cue_trigger`
- [x] `set_cue_effects` (particles, sounds, camera_shake)
- [x] `add_tag_to_asset`

### 13.5 Utility
- [x] `get_gas_info`

> **Note**: Gameplay Tag creation and management is in Phase 31.3 (Data & Persistence).

---

## Phase 14: Character & Movement System (Complete)

**Goal**: Complete character setup with advanced movement.

**Tool**: `manage_character`

**Status**: All 19 actions fully implemented in TypeScript and C++.

### 14.1 Character Creation
- [x] `create_character_blueprint`
- [x] `configure_capsule_component`
- [x] `configure_mesh_component`
- [x] `configure_camera_component`

### 14.2 Movement Component
- [x] `configure_movement_speeds` (walk, run, sprint, crouch, swim, fly)
- [x] `configure_jump` (height, air_control, double_jump)
- [x] `configure_rotation` (orient_to_movement, use_controller_rotation)
- [x] `add_custom_movement_mode`
- [x] `configure_nav_movement`

### 14.3 Advanced Movement
- [x] `setup_mantling`
- [x] `setup_vaulting`
- [x] `setup_climbing`
- [x] `setup_sliding`
- [x] `setup_wall_running`
- [x] `setup_grappling`

### 14.4 Footsteps System
> **Note**: Physical Material creation is in Phase 34.5 (Physics Materials).

- [x] `setup_footstep_system`
- [x] `map_surface_to_sound`
- [x] `configure_footstep_fx`

### 14.5 Utility
- [x] `get_character_info`

---

## Phase 15: Combat & Weapons System (Complete)

**Goal**: Complete combat implementation.

**Tool**: `manage_combat`

**Status**: All 31 actions fully implemented in TypeScript and C++.

### 15.1 Weapon Base
- [x] `create_weapon_blueprint`
- [x] `configure_weapon_mesh`, `configure_weapon_sockets`
- [x] `set_weapon_stats` (damage, fire_rate, range, spread)

### 15.2 Firing Modes
- [x] `configure_hitscan`
- [x] `configure_projectile`
- [x] `configure_spread_pattern`
- [x] `configure_recoil_pattern`
- [x] `configure_aim_down_sights`

### 15.3 Projectiles
- [x] `create_projectile_blueprint`
- [x] `configure_projectile_movement`
- [x] `configure_projectile_collision`
- [x] `configure_projectile_homing`

### 15.4 Damage System
- [x] `create_damage_type`
- [x] `configure_damage_execution`
- [x] `setup_hitbox_component`

### 15.5 Weapon Features
- [x] `setup_reload_system`
- [x] `setup_ammo_system`
- [x] `setup_attachment_system`
- [x] `setup_weapon_switching`

### 15.6 Effects
- [x] `configure_muzzle_flash`
- [x] `configure_tracer`
- [x] `configure_impact_effects`
- [x] `configure_shell_ejection`

### 15.7 Melee Combat
- [x] `create_melee_trace`
- [x] `configure_combo_system`
- [x] `create_hit_pause` (hitstop)
- [x] `configure_hit_reaction`
- [x] `setup_parry_block_system`
- [x] `configure_weapon_trails`

### 15.8 Utility
- [x] `get_combat_info`

---

## Phase 16: Complete AI System (Complete)

**Goal**: Full AI pipeline with EQS, perception, and smart objects.

**Tool**: `manage_ai`

**Status**: All 35 actions fully implemented in TypeScript and C++.

### 16.1 AI Controller
- [x] `create_ai_controller`
- [x] `assign_behavior_tree`, `assign_blackboard`

### 16.2 Blackboard
- [x] `create_blackboard_asset`
- [x] `add_blackboard_key` (bool, int, float, vector, rotator, object, class, enum, name, string)
- [x] `set_key_instance_synced`

### 16.3 Behavior Tree (Expanded)
- [x] `create_behavior_tree`
- [x] `add_composite_node` (selector, sequence, parallel, simple_parallel)
- [x] `add_task_node` (move_to, rotate_to_face, wait, play_animation, play_sound, run_eqs_query, etc.)
- [x] `add_decorator` (blackboard, cooldown, cone_check, does_path_exist, is_at_location, loop, time_limit, force_success)
- [x] `add_service` (default_focus, run_eqs)
- [x] `configure_bt_node`

### 16.4 Environment Query System (EQS)
- [x] `create_eqs_query`
- [x] `add_eqs_generator` (actors_of_class, current_location, donut, grid, on_circle, pathing_grid, points)
- [x] `add_eqs_context` (querier, item, target)
- [x] `add_eqs_test` (distance, dot, overlap, pathfinding, project, random, trace, gameplay_tags)
- [x] `configure_test_scoring`

### 16.5 Perception System
- [x] `add_ai_perception_component`
- [x] `configure_sight_config` (radius, angle, age, detection_by_affiliation)
- [x] `configure_hearing_config` (radius)
- [x] `configure_damage_sense_config`
- [x] `set_perception_team`

### 16.6 State Trees (UE5.3+)
- [x] `create_state_tree`
- [x] `add_state_tree_state`
- [x] `add_state_tree_transition`
- [x] `configure_state_tree_task`

### 16.7 Smart Objects
- [x] `create_smart_object_definition`
- [x] `add_smart_object_slot`
- [x] `configure_slot_behavior`
- [x] `add_smart_object_component`

### 16.8 Mass AI (Crowds)
- [x] `create_mass_entity_config`
- [x] `configure_mass_entity`
- [x] `add_mass_spawner`

### 16.9 Utility
- [x] `get_ai_info`

---

## Phase 17: Inventory & Items System (Complete)

**Goal**: Complete inventory and item management.

**Tool**: `manage_inventory`

**Status**: All 27 actions fully implemented in TypeScript and C++.

### 17.1 Data Assets
- [x] `create_item_data_asset`
- [x] `set_item_properties` (name, description, icon, mesh, stack_size, weight, rarity)
- [x] `create_item_category`
- [x] `assign_item_category`

### 17.2 Inventory Component
- [x] `create_inventory_component`
- [x] `configure_inventory_slots`
- [x] `add_inventory_functions`
- [x] `configure_inventory_events`
- [x] `set_inventory_replication`

### 17.3 Pickups
- [x] `create_pickup_actor`
- [x] `configure_pickup_interaction`
- [x] `configure_pickup_respawn`
- [x] `configure_pickup_effects`

### 17.4 Equipment
- [x] `create_equipment_component`
- [x] `define_equipment_slots`
- [x] `configure_equipment_effects`
- [x] `add_equipment_functions`
- [x] `configure_equipment_visuals`

### 17.5 Loot System
- [x] `create_loot_table`
- [x] `add_loot_entry`
- [x] `configure_loot_drop`
- [x] `set_loot_quality_tiers`

### 17.6 Crafting
- [x] `create_crafting_recipe`
- [x] `configure_recipe_requirements`
- [x] `create_crafting_station`
- [x] `add_crafting_component`

### 17.7 Utility
- [x] `get_inventory_info`

---

## Phase 18: Interaction System (Complete)

**Goal**: Complete interaction framework.

**Tool**: `manage_interaction`

**Status**: All 22 actions fully implemented in TypeScript and C++.

### 18.1 Interaction Component
- [x] `create_interaction_component`
- [x] `configure_interaction_trace`
- [x] `configure_interaction_widget`
- [x] `add_interaction_events`

### 18.2 Interactables
- [x] `create_interactable_interface`
- [x] `create_door_actor`
- [x] `configure_door_properties`
- [x] `create_switch_actor`
- [x] `configure_switch_properties`
- [x] `create_chest_actor`
- [x] `configure_chest_properties`
- [x] `create_lever_actor`

> **Note**: Pickup actors are in Phase 17.3 (Inventory - Pickups).

### 18.3 Destructibles
- [x] `setup_destructible_mesh`
- [x] `configure_destruction_levels`
- [x] `configure_destruction_effects`
- [x] `configure_destruction_damage`
- [x] `add_destruction_component`

### 18.4 Trigger System
- [x] `create_trigger_actor`
- [x] `configure_trigger_events`
- [x] `configure_trigger_filter`
- [x] `configure_trigger_response`

### 18.5 Utility
- [x] `get_interaction_info`

---

## Phase 19: Complete UI/UX System (Complete)

**Goal**: Full UMG widget authoring capabilities.

**Tool**: `manage_widget_authoring`

**Status**: All 65 actions fully implemented in TypeScript and C++.

### 19.1 Widget Creation
- [x] `create_widget_blueprint`
- [x] `set_widget_parent_class`

### 19.2 Layout Panels
- [x] `add_canvas_panel`
- [x] `add_horizontal_box`, `add_vertical_box`
- [x] `add_overlay`, `add_grid_panel`, `add_uniform_grid`
- [x] `add_wrap_box`, `add_scroll_box`
- [x] `add_size_box`, `add_scale_box`
- [x] `add_border`

### 19.3 Common Widgets
- [x] `add_text_block`, `add_rich_text_block`
- [x] `add_image`, `add_button`
- [x] `add_check_box`, `add_slider`
- [x] `add_progress_bar`
- [x] `add_text_input` (editable_text, editable_text_box)
- [x] `add_combo_box`, `add_spin_box`
- [x] `add_list_view`, `add_tree_view`

### 19.4 Layout & Styling
- [x] `set_anchor`, `set_alignment`
- [x] `set_position`, `set_size`
- [x] `set_padding`, `set_z_order`
- [x] `set_render_transform`, `set_clipping`
- [x] `set_visibility`, `set_style`

### 19.5 Bindings & Events
- [x] `create_property_binding`
- [x] `bind_text`, `bind_visibility`, `bind_color`, `bind_enabled`
- [x] `bind_on_clicked`, `bind_on_hovered`, `bind_on_value_changed`

### 19.6 Widget Animations
- [x] `create_widget_animation`
- [x] `add_animation_track` (transform, color, opacity, material)
- [x] `add_animation_keyframe`
- [x] `set_animation_loop`

### 19.7 UI Templates
- [x] `create_main_menu`, `create_pause_menu`
- [x] `create_settings_menu` (video, audio, controls, gameplay)
- [x] `create_loading_screen`
- [x] `create_hud_widget`
- [x] `add_health_bar`, `add_ammo_counter`, `add_minimap`
- [x] `add_crosshair`, `add_compass`
- [x] `add_interaction_prompt`, `add_objective_tracker`
- [x] `add_damage_indicator`
- [x] `create_inventory_ui`
- [x] `create_dialog_widget`, `create_radial_menu`

### 19.8 Utility
- [x] `get_widget_info`
- [x] `preview_widget`

---

## Phase 20: Networking & Multiplayer (Complete)

**Goal**: Complete networking and replication system.

**Tool**: `manage_networking`

**Status**: All 27 actions fully implemented in TypeScript and C++.

### 20.1 Replication
- [x] `set_property_replicated`
- [x] `set_replication_condition` (COND_None, COND_OwnerOnly, COND_SkipOwner, COND_SimulatedOnly, etc.)
- [x] `configure_net_update_frequency`
- [x] `configure_net_priority`
- [x] `set_net_dormancy`
- [x] `configure_replication_graph`

### 20.2 RPCs
- [x] `create_rpc_function` (Server, Client, NetMulticast)
- [x] `configure_rpc_validation`
- [x] `set_rpc_reliability`

### 20.3 Authority & Ownership
- [x] `set_owner`
- [x] `set_autonomous_proxy`
- [x] `check_has_authority`
- [x] `check_is_locally_controlled`

### 20.4 Network Relevancy
- [x] `configure_net_cull_distance`
- [x] `set_always_relevant`
- [x] `set_only_relevant_to_owner`

### 20.5 Net Serialization
- [x] `configure_net_serialization`
- [x] `set_replicated_using`
- [x] `configure_push_model`

### 20.6 Network Prediction
- [x] `configure_client_prediction`
- [x] `configure_server_correction`
- [x] `add_network_prediction_data`
- [x] `configure_movement_prediction`

### 20.7 Connection & Session
- [x] `configure_net_driver`
- [x] `set_net_role`
- [x] `configure_replicated_movement`

### 20.8 Utility
- [x] `get_networking_info`

---

## Phase 21: Game Framework (Complete)

**Goal**: Complete game mode and session management.

**Tool**: `manage_game_framework`

**Status**: All 20 actions fully implemented in TypeScript and C++.

### 21.1 Core Classes
- [x] `create_game_mode`
- [x] `create_game_state`
- [x] `create_player_controller`
- [x] `create_player_state`
- [x] `create_game_instance`
- [x] `create_hud_class`

### 21.2 Game Mode Configuration
- [x] `set_default_pawn_class`
- [x] `set_player_controller_class`
- [x] `set_game_state_class`
- [x] `set_player_state_class`
- [x] `configure_game_rules`

### 21.3 Match Flow
- [x] `setup_match_states` (waiting, warmup, in_progress, post_match)
- [x] `configure_round_system`
- [x] `configure_team_system`
- [x] `configure_scoring_system`
- [x] `configure_spawn_system`

### 21.4 Player Management
- [x] `configure_player_start`
- [x] `set_respawn_rules`
- [x] `configure_spectating`

### 21.5 Utility
- [x] `get_game_framework_info`

---

## Phase 22: Sessions & Local Multiplayer ✅

**Goal**: Session management and split-screen support.

**Tool**: `manage_sessions`

### 22.1 Session Management (Local/LAN)
> **Note**: Online session management (matchmaking, lobbies) is in Phase 43 (Online Services). This section covers local/LAN sessions only.

- [x] `configure_local_session_settings`
- [x] `configure_session_interface`

### 22.2 Local Multiplayer
- [x] `configure_split_screen`
- [x] `set_split_screen_type` (horizontal, vertical, grid)
- [x] `add_local_player`
- [x] `remove_local_player`

### 22.3 LAN
- [x] `configure_lan_play`
- [x] `host_lan_server`
- [x] `join_lan_server`

### 22.4 Voice Chat
- [x] `enable_voice_chat`
- [x] `configure_voice_settings`
- [x] `set_voice_channel`
- [x] `mute_player`
- [x] `set_voice_attenuation`
- [x] `configure_push_to_talk`

### 22.5 Utility
- [x] `get_sessions_info`

---

## Phase 23: World & Level Structure ✅

**Goal**: Complete level and world management.

**Tool**: `manage_level_structure`

**Status**: All 17 actions fully implemented in TypeScript and C++.

### 23.1 Levels
- [x] `create_level`, `create_sublevel`
- [x] `configure_level_streaming`
- [x] `set_streaming_distance`
- [x] `configure_level_bounds`

### 23.2 World Partition (Expanded)
- [x] `enable_world_partition`
- [x] `configure_grid_size`
- [x] `create_data_layer`
- [x] `assign_actor_to_data_layer`
- [x] `configure_hlod_layer`
- [x] `create_minimap_volume`

### 23.3 Level Blueprint
- [x] `open_level_blueprint`
- [x] `add_level_blueprint_node`
- [x] `connect_level_blueprint_nodes`

### 23.4 Level Instances
- [x] `create_level_instance`
- [x] `create_packed_level_actor`

### 23.5 Utility
- [x] `get_level_structure_info`

---

## Phase 24: Volumes & Zones ✅

**Goal**: Complete volume and trigger system.

**Tool**: `manage_volumes`

**Status**: All 18 actions fully implemented in TypeScript and C++.

### 24.1 Trigger Volumes
- [x] `create_trigger_volume`
- [x] `create_trigger_box`, `create_trigger_sphere`, `create_trigger_capsule`

### 24.2 Gameplay Volumes
- [x] `create_blocking_volume`
- [x] `create_kill_z_volume`
- [x] `create_pain_causing_volume`
- [x] `create_physics_volume`
- [x] `create_audio_volume`, `create_reverb_volume`
- [x] `create_cull_distance_volume`
- [x] `create_precomputed_visibility_volume`
- [x] `create_lightmass_importance_volume`
- [x] `create_nav_mesh_bounds_volume`
- [x] `create_nav_modifier_volume`
- [x] `create_camera_blocking_volume`

> **Note**: Post Process Volume configuration is in Phase 29.5 (Post Processing).

### 24.3 Volume Configuration
- [x] `set_volume_extent`
- [x] `set_volume_properties`

### 24.4 Utility
- [x] `get_volumes_info`

---

## Phase 25: Navigation System

**Goal**: Complete navigation mesh and pathfinding.

**Tool**: `manage_navigation`

### 25.1 NavMesh
- [x] `configure_nav_mesh_settings`
- [x] `set_nav_agent_properties` (radius, height, step_height)
- [x] `rebuild_navigation`

### 25.2 Nav Modifiers
- [x] `create_nav_modifier_component`
- [x] `set_nav_area_class`
- [x] `configure_nav_area_cost`

### 25.3 Nav Links
- [x] `create_nav_link_proxy`
- [x] `configure_nav_link` (start, end, direction, snap_radius)
- [x] `set_nav_link_type` (simple, smart)
- [x] `create_smart_link`
- [x] `configure_smart_link_behavior`

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
