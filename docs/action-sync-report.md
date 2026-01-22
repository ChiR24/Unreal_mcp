# 3-Way Action Synchronization Report

> **TS ↔ C++ ↔ Live MCP Server**
>
> This report compares actions across three sources:
> 1. **TS (Static)**: TypeScript tool definitions in source code
> 2. **C++ (Static)**: C++ handler implementations in plugin
> 3. **Live MCP (Runtime)**: What the actual running MCP server exposes to LLMs

Generated: 2026-01-22T06:07:00.638Z

## Executive Summary

| Source | Tools | Actions |
|--------|-------|---------|
| TypeScript (Static) | 37 | 2,665 |
| C++ Handlers (Static) | - | 3,170 |
| Live MCP Server | 36 | 2,662 |

### Sync Metrics

| Comparison | Matched | Missing | Sync % |
|------------|---------|---------|--------|
| TS → C++ | 2,646 | 19 | **99%** |
| TS → Live | 2,662 | 3 | **100%** |

### ⚠️ TS vs Live Discrepancies

Actions that differ between static TS definitions and live server exposure:

- **configure_tools**: 3 in TS but NOT in Live

## Prefix Normalization

The following prefixes are stripped for comparison:

| Prefix | Tools |
|--------|-------|
| `chaos_*` | animation_physics |
| `mw_*`, `audio_*` | manage_audio |
| `bp_*` | manage_asset |
| `niagara_*` | manage_effect |
| `seq_*`, `mrq_*` | manage_sequence |

## Sync Status by Tool

| Tool | TS | C++ | Live | TS→C++ | TS→Live |
|------|----|----|------|--------|---------|
| configure_tools | 3 | 0 | 0 | 🔴 0% | 🔴 0% |
| test_gameplay_abilities | 4 | 27 | 4 | 🔴 0% | ✅ 100% |
| manage_gameplay_abilities | 31 | 27 | 31 | 🟡 87% | ✅ 100% |
| manage_effect | 79 | 88 | 79 | 🟡 97% | ✅ 100% |
| manage_lighting | 61 | 59 | 61 | 🟡 97% | ✅ 100% |
| manage_level | 94 | 96 | 94 | 🟡 98% | ✅ 100% |
| manage_gameplay_systems | 50 | 49 | 50 | 🟡 98% | ✅ 100% |
| manage_livelink | 64 | 63 | 64 | 🟡 98% | ✅ 100% |
| manage_asset | 99 | 151 | 99 | ✅ 100% | ✅ 100% |
| control_actor | 45 | 175 | 45 | ✅ 100% | ✅ 100% |
| control_editor | 84 | 155 | 84 | ✅ 100% | ✅ 100% |
| manage_motion_design | 10 | 10 | 10 | ✅ 100% | ✅ 100% |
| animation_physics | 166 | 170 | 166 | ✅ 100% | ✅ 100% |
| build_environment | 73 | 93 | 73 | ✅ 100% | ✅ 100% |
| manage_sequence | 139 | 139 | 139 | ✅ 100% | ✅ 100% |
| manage_audio | 134 | 169 | 134 | ✅ 100% | ✅ 100% |
| manage_performance | 20 | 57 | 20 | ✅ 100% | ✅ 100% |
| manage_geometry | 80 | 83 | 80 | ✅ 100% | ✅ 100% |
| manage_skeleton | 54 | 54 | 54 | ✅ 100% | ✅ 100% |
| manage_material_authoring | 73 | 80 | 73 | ✅ 100% | ✅ 100% |
| manage_character | 78 | 138 | 78 | ✅ 100% | ✅ 100% |
| manage_combat | 67 | 67 | 67 | ✅ 100% | ✅ 100% |
| manage_ai | 103 | 104 | 103 | ✅ 100% | ✅ 100% |
| manage_widget_authoring | 73 | 73 | 73 | ✅ 100% | ✅ 100% |
| manage_networking | 73 | 73 | 73 | ✅ 100% | ✅ 100% |
| manage_volumes | 41 | 41 | 41 | ✅ 100% | ✅ 100% |
| manage_data | 62 | 62 | 62 | ✅ 100% | ✅ 100% |
| manage_build | 48 | 48 | 48 | ✅ 100% | ✅ 100% |
| manage_editor_utilities | 45 | 47 | 45 | ✅ 100% | ✅ 100% |
| manage_gameplay_primitives | 62 | 62 | 62 | ✅ 100% | ✅ 100% |
| manage_character_avatar | 60 | 60 | 60 | ✅ 100% | ✅ 100% |
| manage_asset_plugins | 248 | 258 | 248 | ✅ 100% | ✅ 100% |
| manage_xr | 272 | 272 | 272 | ✅ 100% | ✅ 100% |
| manage_accessibility | 50 | 50 | 50 | ✅ 100% | ✅ 100% |
| manage_ui | 11 | 16 | 11 | ✅ 100% | ✅ 100% |
| manage_attribute_sets | 6 | 27 | 6 | ✅ 100% | ✅ 100% |
| manage_gameplay_cues | 3 | 27 | 3 | ✅ 100% | ✅ 100% |

---

## Detailed Gap Analysis

### configure_tools

| Source | Count |
|--------|-------|
| TS | 3 |
| C++ | 0 |
| Live | 0 |

#### TS → C++ Missing (3)

Actions in TS but NOT implemented in C++:

```
get_status
list_categories
set_categories
```

#### ⚠️ TS → Live Missing (3)

**RUNTIME ISSUE**: Actions defined in TS but NOT exposed by live server:

```
get_status
list_categories
set_categories
```

---

### test_gameplay_abilities

| Source | Count |
|--------|-------|
| TS | 4 |
| C++ | 27 |
| Live | 4 |

#### TS → C++ Missing (4)

Actions in TS but NOT implemented in C++:

```
test_activate_ability
test_apply_effect
test_get_attribute
test_get_gameplay_tags
```

#### C++ → TS Extra (27)

Actions in C++ but NOT exposed in TS:

```
add_ability_system_component
add_ability_task
add_attribute
add_effect_cue
add_effect_execution_calculation
add_effect_modifier
add_tag_to_asset
configure_asc
configure_cue_trigger
create_attribute_set
create_gameplay_ability
create_gameplay_cue_notify
create_gameplay_effect
get_gas_info
set_ability_cooldown
set_ability_costs
set_ability_tags
set_ability_targeting
set_activation_policy
set_attribute_base_value
... and 7 more
```

---

### manage_gameplay_abilities

| Source | Count |
|--------|-------|
| TS | 31 |
| C++ | 27 |
| Live | 31 |

#### TS → C++ Missing (4)

Actions in TS but NOT implemented in C++:

```
test_activate_ability
test_apply_effect
test_get_attribute
test_get_gameplay_tags
```

---

### manage_effect

| Source | Count |
|--------|-------|
| TS | 79 |
| C++ | 88 |
| Live | 79 |

#### TS → C++ Missing (2)

Actions in TS but NOT implemented in C++:

```
niagara
particle
```

#### C++ → TS Extra (8)

Actions in C++ but NOT exposed in TS:

```
add_module
connect_pins
create_effect
get_parameters
niagara_get_parameters
niagara_set_variable
remove_node
set_parameter
```

---

### manage_lighting

| Source | Count |
|--------|-------|
| TS | 61 |
| C++ | 59 |
| Live | 61 |

#### TS → C++ Missing (2)

Actions in TS but NOT implemented in C++:

```
build_lighting
build_lighting_quality
```

---

### manage_level

| Source | Count |
|--------|-------|
| TS | 94 |
| C++ | 96 |
| Live | 94 |

#### TS → C++ Missing (2)

Actions in TS but NOT implemented in C++:

```
build_hlod_for_level
build_lighting
```

#### C++ → TS Extra (4)

Actions in C++ but NOT exposed in TS:

```
hide
list
show
unload
```

---

### manage_gameplay_systems

| Source | Count |
|--------|-------|
| TS | 50 |
| C++ | 49 |
| Live | 50 |

#### TS → C++ Missing (1)

Actions in TS but NOT implemented in C++:

```
build_hlod
```

---

### manage_livelink

| Source | Count |
|--------|-------|
| TS | 64 |
| C++ | 63 |
| Live | 64 |

#### TS → C++ Missing (1)

Actions in TS but NOT implemented in C++:

```
build_preset_from_client
```

---

### manage_asset

| Source | Count |
|--------|-------|
| TS | 99 |
| C++ | 151 |
| Live | 99 |

#### C++ → TS Extra (23)

Actions in C++ but NOT exposed in TS:

```
asset_query
blueprint_probe_subobject_handle
break_connections
configure_nanite_settings
connect_nodes
convert_to_nanite
create_blueprint
enable_nanite
generate_thumbnail
get_blueprint_scs
get_nodes
integer
modifyscs
probe_subobject_handle
probehandle
remove_node
save
save_asset
search
set_scs_component_property
... and 3 more
```

---

### control_actor

| Source | Count |
|--------|-------|
| TS | 45 |
| C++ | 175 |
| Live | 45 |

#### C++ → TS Extra (130)

Actions in C++ but NOT exposed in TS:

```
add_control
add_ik_chain
add_ik_goal
add_mapping
add_widget_child
apply_animation_modifier
apply_force_to_actor
array_append
array_clear
array_get_element
array_insert
array_remove
array_set_element
batch_execute
batch_substrate_migration
batch_transform
cancel_job
capture_viewport
capture_viewport_sequence
clear_event_subscriptions
... and 110 more
```

---

### control_editor

| Source | Count |
|--------|-------|
| TS | 84 |
| C++ | 155 |
| Live | 84 |

#### C++ → TS Extra (71)

Actions in C++ but NOT exposed in TS:

```
add_component
add_control
add_ik_chain
add_ik_goal
add_tag
apply_animation_modifier
apply_force
apply_force_to_actor
attach
batch_set_component_properties
batch_transform
batch_transform_actors
clone_component_hierarchy
clone_components
configure_motion_matching
create_animation_modifier
create_control_rig
create_ik_retargeter
create_ik_rig
create_pose_search_database
... and 51 more
```

---

### animation_physics

| Source | Count |
|--------|-------|
| TS | 166 |
| C++ | 170 |
| Live | 166 |

#### C++ → TS Extra (4)

Actions in C++ but NOT exposed in TS:

```
add_notify_old_unused
animation_physics
create_chaos_cloth_config
create_chaos_cloth_shared_sim_config
```

---

### build_environment

| Source | Count |
|--------|-------|
| TS | 73 |
| C++ | 93 |
| Live | 73 |

#### C++ → TS Extra (22)

Actions in C++ but NOT exposed in TS:

```
console_command
engine_quit
find_by_class
get_bounding_box
get_component_property
get_components
get_engine_version
get_feature_flags
get_project_settings
get_property
inspect
inspect_class
inspect_object
profile
screenshot
set_component_property
set_project_setting
set_property
set_quality
show_fps
... and 2 more
```

---

### manage_audio

| Source | Count |
|--------|-------|
| TS | 134 |
| C++ | 169 |
| Live | 134 |

#### C++ → TS Extra (11)

Actions in C++ but NOT exposed in TS:

```
audio_create_component
configure_audio_modulation
create_envelope
create_filter
create_oscillator
create_procedural_music
create_sequencer_node
export_metasound_preset
import_audio_to_metasound
remove_metasound_node
set_metasound_variable
```

---

### manage_performance

| Source | Count |
|--------|-------|
| TS | 20 |
| C++ | 57 |
| Live | 20 |

#### C++ → TS Extra (37)

Actions in C++ but NOT exposed in TS:

```
add_burn_in
add_console_variable
add_job
add_render_pass
attach_render_target_to_volume
batch_render_sequences
clear_queue
configure_anti_aliasing
configure_burn_in
configure_high_res_settings
configure_job
configure_mrq_settings
configure_output
configure_render_pass
create_queue
create_render_target
get_queue
get_render_passes
get_render_progress
get_render_status
... and 17 more
```

---

### manage_geometry

| Source | Count |
|--------|-------|
| TS | 80 |
| C++ | 83 |
| Live | 80 |

#### C++ → TS Extra (3)

Actions in C++ but NOT exposed in TS:

```
enable_nanite_mesh
set_nanite_settings
split_normals
```

---

### manage_material_authoring

| Source | Count |
|--------|-------|
| TS | 73 |
| C++ | 80 |
| Live | 73 |

#### C++ → TS Extra (7)

Actions in C++ but NOT exposed in TS:

```
add_material_node
add_material_parameter
connect_material_pins
get_material_stats
reflectionvectorws
remove_material_node
texture2d
```

---

### manage_character

| Source | Count |
|--------|-------|
| TS | 78 |
| C++ | 138 |
| Live | 78 |

#### C++ → TS Extra (60)

Actions in C++ but NOT exposed in TS:

```
apply_avatar_to_character
apply_preset
attach_groom_to_skeletal_mesh
bake_customizable_instance
cache_avatar
clear_avatar_cache
compile_customizable_object
configure_face_rig
configure_hair_physics
configure_hair_rendering
configure_hair_simulation
configure_metahuman_lod
configure_rpm_materials
create_customizable_instance
create_customizable_object
create_groom_asset
create_groom_binding
create_rpm_actor
create_rpm_animation_blueprint
enable_body_correctives
... and 40 more
```

---

### manage_ai

| Source | Count |
|--------|-------|
| TS | 103 |
| C++ | 104 |
| Live | 103 |

#### C++ → TS Extra (1)

Actions in C++ but NOT exposed in TS:

```
create
```

---

### manage_editor_utilities

| Source | Count |
|--------|-------|
| TS | 45 |
| C++ | 47 |
| Live | 45 |

#### C++ → TS Extra (2)

Actions in C++ but NOT exposed in TS:

```
execute_console_command
execute_editor_function
```

---

### manage_asset_plugins

| Source | Count |
|--------|-------|
| TS | 248 |
| C++ | 258 |
| Live | 248 |

#### C++ → TS Extra (10)

Actions in C++ but NOT exposed in TS:

```
configure_analog_cursor
configure_gamepad_navigation
configure_navigation_rules
configure_ui_input_config
create_common_activatable_widget
get_common_ui_info
get_ui_input_config
register_common_input_metadata
set_default_focus_widget
set_input_action_data
```

---

### manage_ui

| Source | Count |
|--------|-------|
| TS | 11 |
| C++ | 16 |
| Live | 11 |

#### C++ → TS Extra (5)

Actions in C++ but NOT exposed in TS:

```
play_in_editor
save_all
screenshot
simulate_input
stop_play
```

---

### manage_attribute_sets

| Source | Count |
|--------|-------|
| TS | 6 |
| C++ | 27 |
| Live | 6 |

#### C++ → TS Extra (21)

Actions in C++ but NOT exposed in TS:

```
add_ability_task
add_effect_cue
add_effect_execution_calculation
add_effect_modifier
add_tag_to_asset
configure_cue_trigger
create_gameplay_ability
create_gameplay_cue_notify
create_gameplay_effect
get_gas_info
set_ability_cooldown
set_ability_costs
set_ability_tags
set_ability_targeting
set_activation_policy
set_cue_effects
set_effect_duration
set_effect_stacking
set_effect_tags
set_instancing_policy
... and 1 more
```

---

### manage_gameplay_cues

| Source | Count |
|--------|-------|
| TS | 3 |
| C++ | 27 |
| Live | 3 |

#### C++ → TS Extra (24)

Actions in C++ but NOT exposed in TS:

```
add_ability_system_component
add_ability_task
add_attribute
add_effect_cue
add_effect_execution_calculation
add_effect_modifier
add_tag_to_asset
configure_asc
create_attribute_set
create_gameplay_ability
create_gameplay_effect
get_gas_info
set_ability_cooldown
set_ability_costs
set_ability_tags
set_ability_targeting
set_activation_policy
set_attribute_base_value
set_attribute_clamping
set_effect_duration
... and 4 more
```

---

## Implementation Priority

### High Priority (Core Tools)

```
manage_level::build_hlod_for_level
manage_level::build_lighting
```

### Medium Priority (Frequently Used)

```
manage_effect::niagara
manage_effect::particle
manage_lighting::build_lighting
manage_lighting::build_lighting_quality
```

## Metadata

```json
{
  "generatedAt": "2026-01-22T06:07:00.653Z",
  "sources": {
    "ts": {
      "tools": 37,
      "actions": 2665
    },
    "cpp": {
      "actions": 3170
    },
    "live": {
      "tools": 36,
      "actions": 2662,
      "failed": false
    }
  },
  "sync": {
    "tsCpp": "99%",
    "tsLive": "100%"
  }
}
```
