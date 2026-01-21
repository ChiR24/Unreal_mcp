# TS/C++ Action Synchronization Report

> Auto-generated. Compares TypeScript tool definitions with C++ handler implementations.
> **Note:** Uses prefix normalization (chaos_*, mw_*, bp_*, etc.) to match actions across naming conventions.

Generated: 2026-01-21T16:47:50.215Z

## Executive Summary

| Metric | Count |
|--------|-------|
| Total TS Actions | 2,578 |
| Total C++ Actions | 3,485 |
| Matched (TS→C++) | 2,575 |
| Missing in C++ | 3 |
| Extra in C++ | 856 |
| **Overall Sync** | **100%** |

## Prefix Normalization Applied

The following prefixes are stripped for comparison to handle naming convention differences:

| TS Prefix | Applied To Tools |
|-----------|------------------|
| `chaos_*` | animation_physics |
| `mw_*` | manage_audio |
| `bp_*` | manage_asset |
| `audio_*` | manage_audio |
| `niagara_*` | manage_effect |
| `seq_*`, `mrq_*` | manage_sequence |
| `water_*`, `weather_*` | build_environment |

## Sync Status by Tool

| Tool | TS | C++ | Matched | Missing | Extra | Sync |
|------|----|----|---------|---------|-------|------|
| configure_tools | 3 | 0 | 0 | 3 | 0 | 🔴 0% |
| manage_asset | 99 | 229 | 99 | 0 | 101 | ✅ 100% |
| control_actor | 45 | 187 | 45 | 0 | 142 | ✅ 100% |
| control_editor | 84 | 167 | 84 | 0 | 83 | ✅ 100% |
| manage_level | 87 | 99 | 87 | 0 | 12 | ✅ 100% |
| manage_motion_design | 10 | 10 | 10 | 0 | 0 | ✅ 100% |
| animation_physics | 162 | 170 | 162 | 0 | 8 | ✅ 100% |
| manage_effect | 74 | 90 | 74 | 0 | 13 | ✅ 100% |
| build_environment | 58 | 97 | 58 | 0 | 41 | ✅ 100% |
| manage_sequence | 100 | 143 | 100 | 0 | 43 | ✅ 100% |
| manage_audio | 134 | 216 | 134 | 0 | 58 | ✅ 100% |
| manage_lighting | 61 | 62 | 61 | 0 | 1 | ✅ 100% |
| manage_performance | 20 | 58 | 20 | 0 | 38 | ✅ 100% |
| manage_geometry | 80 | 83 | 80 | 0 | 3 | ✅ 100% |
| manage_skeleton | 54 | 55 | 54 | 0 | 1 | ✅ 100% |
| manage_material_authoring | 73 | 153 | 73 | 0 | 80 | ✅ 100% |
| manage_character | 78 | 147 | 78 | 0 | 69 | ✅ 100% |
| manage_combat | 67 | 78 | 67 | 0 | 11 | ✅ 100% |
| manage_ai | 103 | 104 | 103 | 0 | 1 | ✅ 100% |
| manage_widget_authoring | 73 | 73 | 73 | 0 | 0 | ✅ 100% |
| manage_networking | 73 | 73 | 73 | 0 | 0 | ✅ 100% |
| manage_volumes | 41 | 41 | 41 | 0 | 0 | ✅ 100% |
| manage_data | 62 | 63 | 62 | 0 | 1 | ✅ 100% |
| manage_build | 48 | 50 | 48 | 0 | 2 | ✅ 100% |
| manage_editor_utilities | 45 | 51 | 45 | 0 | 6 | ✅ 100% |
| manage_gameplay_systems | 50 | 51 | 50 | 0 | 1 | ✅ 100% |
| manage_gameplay_primitives | 62 | 63 | 62 | 0 | 1 | ✅ 100% |
| manage_character_avatar | 60 | 60 | 60 | 0 | 0 | ✅ 100% |
| manage_asset_plugins | 248 | 258 | 248 | 0 | 10 | ✅ 100% |
| manage_livelink | 64 | 64 | 64 | 0 | 0 | ✅ 100% |
| manage_xr | 272 | 272 | 272 | 0 | 0 | ✅ 100% |
| manage_accessibility | 50 | 50 | 50 | 0 | 0 | ✅ 100% |
| manage_ui | 7 | 16 | 7 | 0 | 9 | ✅ 100% |
| manage_gameplay_abilities | 18 | 38 | 18 | 0 | 20 | ✅ 100% |
| manage_attribute_sets | 6 | 38 | 6 | 0 | 32 | ✅ 100% |
| manage_gameplay_cues | 3 | 38 | 3 | 0 | 35 | ✅ 100% |
| test_gameplay_abilities | 4 | 38 | 4 | 0 | 34 | ✅ 100% |

---

## Detailed Gap Analysis

### configure_tools

**TS Actions:** 3 | **C++ Actions:** 0 | **Sync:** 0%

#### Missing in C++ (3)

These actions are defined in TypeScript but have NO C++ implementation:

```
get_status
list_categories
set_categories
```

---

### manage_asset

**TS Actions:** 99 | **C++ Actions:** 229 | **Sync:** 100%

#### Extra in C++ (101)

These actions are in C++ but NOT exposed in TypeScript:

```
actor
add
adsr
asset_query
audio
audioinput
audiooutput
bandpass
bandpassfilter
blueprint
blueprint_probe_subobject_handle
blueprints
bool
boolean
bpf
break_connections
byte
character
chorus
clamp
class
compressor
configure_nanite_settings
connect_nodes
convert_to_nanite
create_blueprint
decay
delay
double
enable_nanite
envelope
filter
flanger
float
floatinput
gain
generate_thumbnail
get_blueprint_scs
get_nodes
highpass
highpassfilter
hpf
input
int
int64
integer
limiter
lowpass
lowpassfilter
lpf
manage_asset
manage_audio
manage_blueprint_graph
material
materials
mesh
meshes
mixer
modifyscs
multiply
name
noise
noisegenerator
object
oscillator
output
parameter
pawn
phaser
probe_subobject_handle
probehandle
remove_node
reverb
rotator
save
save_asset
saw
sawtooth
sawtoothoscillator
search
set_scs_component_property
set_scs_component_transform
setdefault
sine
sineoscillator
sound
sounds
square
squareoscillator
staticmesh
string
subtract
text
texture
textures
transform
triangle
triangleoscillator
validate_asset
vector
whitenoise
```

---

### control_actor

**TS Actions:** 45 | **C++ Actions:** 187 | **Sync:** 100%

#### Extra in C++ (142)

These actions are in C++ but NOT exposed in TypeScript:

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
clone_components
configure_event_channel
configure_megalights
configure_motion_matching
console_command
control_actor
control_editor
convert_to_substrate
create_animation_modifier
create_bookmark
create_control_rig
create_ik_retargeter
create_ik_rig
create_input_action
create_input_mapping_context
create_pose_search_database
create_widget
detaillighting
eject
execute_command
explain_action_parameters
flush_operation_queue
focus_actor
get
get_action_statistics
get_active_jobs
get_actor
get_actor_by_name
get_actor_transform
get_asset_dependencies
get_asset_references
get_available_actions
get_bridge_health
get_class_hierarchy
get_event_history
get_job_status
get_last_error_details
get_light_budget_stats
get_object_property
get_operation_history
get_project_settings
get_selection_info
get_subscribed_events
jump_to_bookmark
lightcomplexity
lightingonly
lightmapdensity
list_actors
lit
lumen_update_scene
map_clear
map_get_keys
map_get_value
map_has_key
map_remove_key
map_set_value
open_asset
parallel_execute
pause
play
play_sound
playback_input_session
possess
profile
queue_operations
record_input_session
reflectionoverride
remove
remove_mapping
restore_state
resume
run_tests
run_ubt
screenshot
set_actor_transform
set_actor_visibility
set_add
set_camera
set_camera_fov
set_camera_position
set_clear
set_contains
set_cvar
set_editor_mode
set_fullscreen
set_game_speed
set_object_property
set_preferences
set_project_setting
set_quality
set_remove
set_resolution
set_retarget_chain_mapping
set_view_mode
set_viewport_camera
set_viewport_realtime
set_viewport_resolution
setup_ml_deformer
shadercomplexity
show_fps
show_widget
simulate_input
spawn_category
start_background_job
start_recording
start_session
stationarylightoverlap
step_frame
stop
stop_pie
stop_recording
subscribe
subscribe_to_event
suggest_fix_for_error
toggle_realtime_rendering
unlit
unsubscribe
unsubscribe_from_event
validate_action_input
validate_assets
validate_operation_preconditions
wireframe
```

---

### control_editor

**TS Actions:** 84 | **C++ Actions:** 167 | **Sync:** 100%

#### Extra in C++ (83)

These actions are in C++ but NOT exposed in TypeScript:

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
control_actor
control_editor
create_animation_modifier
create_control_rig
create_ik_retargeter
create_ik_rig
create_pose_search_database
create_snapshot
delete
delete_by_tag
delete_object
deserialize_actor_state
detach
detaillighting
duplicate
export
find_by_class
find_by_name
find_by_tag
focus_actor
get
get_actor
get_actor_bounds
get_actor_by_name
get_actor_references
get_actor_transform
get_all_component_properties
get_bounding_box
get_component_property
get_components
get_metadata
get_property
get_transform
inspect_class
inspect_object
lightcomplexity
lightingonly
lightmapdensity
list
list_actors
list_objects
lit
merge_actors
query_actors_by_predicate
reflectionoverride
remove
remove_tag
replace_actor_class
restore_snapshot
restore_state
serialize_actor_state
set_actor_transform
set_actor_visibility
set_blueprint_variables
set_component_properties
set_component_property
set_property
set_retarget_chain_mapping
set_transform
set_viewport_camera
set_visibility
setup_ml_deformer
shadercomplexity
spawn
spawn_blueprint
stationarylightoverlap
unlit
wireframe
```

---

### manage_level

**TS Actions:** 87 | **C++ Actions:** 99 | **Sync:** 100%

#### Extra in C++ (12)

These actions are in C++ but NOT exposed in TypeScript:

```
bake_lightmap
create_datalayer
create_new_level
hide
list
load_level
manage_level
save_current_level
show
spawn_light
stream_level
unload
```

---

### animation_physics

**TS Actions:** 162 | **C++ Actions:** 170 | **Sync:** 100%

#### Extra in C++ (8)

These actions are in C++ but NOT exposed in TypeScript:

```
add_notify_old_unused
animation_physics
apply_animation_modifier
create_animation_blueprint
create_chaos_cloth_config
create_chaos_cloth_shared_sim_config
get_chaos_plugin_status
play_anim_montage
```

---

### manage_effect

**TS Actions:** 74 | **C++ Actions:** 90 | **Sync:** 100%

#### Extra in C++ (13)

These actions are in C++ but NOT exposed in TypeScript:

```
activate_niagara
add_module
connect_pins
create_effect
deactivate_niagara
get_parameters
modify_niagara_parameter
niagara_get_parameters
niagara_set_variable
remove_node
reset_niagara
set_parameter
spawn_niagara_actor
```

---

### build_environment

**TS Actions:** 58 | **C++ Actions:** 97 | **Sync:** 100%

#### Extra in C++ (41)

These actions are in C++ but NOT exposed in TypeScript:

```
add_foliage_type
build_environment
configure_foliage_lod
configure_foliage_placement
configure_landscape_lod
configure_sun_atmosphere
configure_sun_color
configure_sun_position
console_command
control_environment
create_time_of_day_controller
engine_quit
export_heightmap
find_by_class
get_bounding_box
get_component_property
get_components
get_engine_version
get_feature_flags
get_foliage_types
get_landscape_info
get_project_settings
get_property
get_water_depth_info
import_heightmap
inspect
inspect_class
inspect_object
manage_water
manage_weather
profile
screenshot
set_component_property
set_project_setting
set_property
set_quality
set_skylight_intensity
set_sun_intensity
show_fps
system_control
validate_assets
```

---

### manage_sequence

**TS Actions:** 100 | **C++ Actions:** 143 | **Sync:** 100%

#### Extra in C++ (43)

These actions are in C++ but NOT exposed in TypeScript:

```
add_animation_track
add_camera_track
add_sequencer_keyframe
add_transform_track
delete_sequence
duplicate_sequence
list_sequences
manage_movie_render
manage_sequence
manage_sequencer
manage_sequencer_track
sequence_add_actor
sequence_add_actors
sequence_add_camera
sequence_add_keyframe
sequence_add_section
sequence_add_spawnable_from_class
sequence_add_track
sequence_create
sequence_delete
sequence_duplicate
sequence_get_bindings
sequence_get_metadata
sequence_get_properties
sequence_list
sequence_list_track_types
sequence_list_tracks
sequence_open
sequence_pause
sequence_play
sequence_remove_actors
sequence_remove_track
sequence_rename
sequence_set_display_rate
sequence_set_playback_speed
sequence_set_properties
sequence_set_tick_resolution
sequence_set_track_locked
sequence_set_track_muted
sequence_set_track_solo
sequence_set_view_range
sequence_set_work_range
sequence_stop
```

---

### manage_audio

**TS Actions:** 134 | **C++ Actions:** 216 | **Sync:** 100%

#### Extra in C++ (58)

These actions are in C++ but NOT exposed in TypeScript:

```
add
adsr
audio_create_component
audioinput
audiooutput
bandpass
bandpassfilter
bpf
chorus
clamp
compressor
configure_audio_modulation
create_envelope
create_filter
create_oscillator
create_procedural_music
create_sequencer_node
decay
delay
envelope
export_metasound_preset
filter
flanger
floatinput
gain
highpass
highpassfilter
hpf
import_audio_to_metasound
input
limiter
lowpass
lowpassfilter
lpf
manage_asset
manage_audio
mixer
multiply
noise
noisegenerator
oscillator
output
parameter
phaser
remove_metasound_node
reverb
saw
sawtooth
sawtoothoscillator
set_metasound_variable
sine
sineoscillator
square
squareoscillator
subtract
triangle
triangleoscillator
whitenoise
```

---

### manage_lighting

**TS Actions:** 61 | **C++ Actions:** 62 | **Sync:** 100%

#### Extra in C++ (1)

These actions are in C++ but NOT exposed in TypeScript:

```
manage_post_process
```

---

### manage_performance

**TS Actions:** 20 | **C++ Actions:** 58 | **Sync:** 100%

#### Extra in C++ (38)

These actions are in C++ but NOT exposed in TypeScript:

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
lumen_update_scene
manage_movie_render
nanite_rebuild_mesh
remove_burn_in
remove_console_variable
remove_job
remove_render_pass
set_file_name_format
set_frame_rate
set_map
set_output_directory
set_resolution
set_sequence
set_spatial_sample_count
set_temporal_sample_count
set_tile_count
start_render
stop_render
```

---

### manage_geometry

**TS Actions:** 80 | **C++ Actions:** 83 | **Sync:** 100%

#### Extra in C++ (3)

These actions are in C++ but NOT exposed in TypeScript:

```
enable_nanite_mesh
set_nanite_settings
split_normals
```

---

### manage_skeleton

**TS Actions:** 54 | **C++ Actions:** 55 | **Sync:** 100%

#### Extra in C++ (1)

These actions are in C++ but NOT exposed in TypeScript:

```
manage_media
```

---

### manage_material_authoring

**TS Actions:** 73 | **C++ Actions:** 153 | **Sync:** 100%

#### Extra in C++ (80)

These actions are in C++ but NOT exposed in TypeScript:

```
Add
Append
Clamp
Divide
Frac
Lerp
Multiply
OneMinus
Power
Subtract
add
add_material_node
add_material_parameter
append
appendvector
bool
boolparam
clamp
color
colorparam
connect_material_pins
constant
constant2vector
constant3vector
constant4vector
custom
customexpression
depth
div
divide
float
float2
float3
float4
floatparam
frac
fraction
fresnel
functioncall
get_material_stats
hlsl
if
lerp
linearinterpolate
manage_material_authoring
materialfunctioncall
mul
multiply
noise
oneminus
panner
pixeldepth
pow
power
reflectionvector
reflectionvectorws
remove_material_node
rgb
rgba
rotator
scalar
scalarparameter
staticswitch
staticswitchparameter
sub
subtract
switch
texcoord
texture
texture2d
texturecoordinate
textureparameter
texturesample
texturesampleparameter2d
uv
vector
vectorparameter
vertexnormal
vertexnormalws
worldposition
```

---

### manage_character

**TS Actions:** 78 | **C++ Actions:** 147 | **Sync:** 100%

#### Extra in C++ (69)

These actions are in C++ but NOT exposed in TypeScript:

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
enable_hair_simulation
enable_neck_correctives
export_metahuman_settings
fall
falling
fly
flying
get_avatar_metadata
get_groom_info
get_instance_info
get_metahuman_component
get_metahuman_info
get_parameter_info
get_rpm_info
import_groom
import_metahuman
list_available_presets
load_avatar_from_glb
load_avatar_from_url
none
retarget_rpm_animation
set_body_part
set_body_type
set_bool_parameter
set_color_parameter
set_eye_color
set_face_parameter
set_float_parameter
set_hair_color
set_hair_root_scale
set_hair_style
set_hair_tip_scale
set_hair_width
set_int_parameter
set_projector_parameter
set_quality_level
set_rpm_outfit
set_skin_tone
set_texture_parameter
set_transform_parameter
set_vector_parameter
spawn_customizable_actor
spawn_groom_actor
spawn_metahuman_actor
swim
swimming
update_skeletal_mesh
walk
walking
```

---

### manage_combat

**TS Actions:** 67 | **C++ Actions:** 78 | **Sync:** 100%

#### Extra in C++ (11)

These actions are in C++ but NOT exposed in TypeScript:

```
add
additive
divide
division
multiplicative
multiply
override
test_activate_ability
test_apply_effect
test_get_attribute
test_get_gameplay_tags
```

---

### manage_ai

**TS Actions:** 103 | **C++ Actions:** 104 | **Sync:** 100%

#### Extra in C++ (1)

These actions are in C++ but NOT exposed in TypeScript:

```
create
```

---

### manage_data

**TS Actions:** 62 | **C++ Actions:** 63 | **Sync:** 100%

#### Extra in C++ (1)

These actions are in C++ but NOT exposed in TypeScript:

```
manage_data
```

---

### manage_build

**TS Actions:** 48 | **C++ Actions:** 50 | **Sync:** 100%

#### Extra in C++ (2)

These actions are in C++ but NOT exposed in TypeScript:

```
manage_build
manage_testing
```

---

### manage_editor_utilities

**TS Actions:** 45 | **C++ Actions:** 51 | **Sync:** 100%

#### Extra in C++ (6)

These actions are in C++ but NOT exposed in TypeScript:

```
execute_console_command
execute_editor_function
high
manage_editor_utilities
medium
preview
```

---

### manage_gameplay_systems

**TS Actions:** 50 | **C++ Actions:** 51 | **Sync:** 100%

#### Extra in C++ (1)

These actions are in C++ but NOT exposed in TypeScript:

```
manage_gameplay_systems
```

---

### manage_gameplay_primitives

**TS Actions:** 62 | **C++ Actions:** 63 | **Sync:** 100%

#### Extra in C++ (1)

These actions are in C++ but NOT exposed in TypeScript:

```
manage_gameplay_primitives
```

---

### manage_asset_plugins

**TS Actions:** 248 | **C++ Actions:** 258 | **Sync:** 100%

#### Extra in C++ (10)

These actions are in C++ but NOT exposed in TypeScript:

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

**TS Actions:** 7 | **C++ Actions:** 16 | **Sync:** 100%

#### Extra in C++ (9)

These actions are in C++ but NOT exposed in TypeScript:

```
add_widget_child
create_hud
play_in_editor
save_all
screenshot
set_widget_image
set_widget_text
simulate_input
stop_play
```

---

### manage_gameplay_abilities

**TS Actions:** 18 | **C++ Actions:** 38 | **Sync:** 100%

#### Extra in C++ (20)

These actions are in C++ but NOT exposed in TypeScript:

```
add
add_ability_system_component
add_attribute
additive
configure_asc
configure_cue_trigger
create_attribute_set
create_gameplay_cue_notify
divide
division
multiplicative
multiply
override
set_attribute_base_value
set_attribute_clamping
set_cue_effects
test_activate_ability
test_apply_effect
test_get_attribute
test_get_gameplay_tags
```

---

### manage_attribute_sets

**TS Actions:** 6 | **C++ Actions:** 38 | **Sync:** 100%

#### Extra in C++ (32)

These actions are in C++ but NOT exposed in TypeScript:

```
add
add_ability_task
add_effect_cue
add_effect_execution_calculation
add_effect_modifier
add_tag_to_asset
additive
configure_cue_trigger
create_gameplay_ability
create_gameplay_cue_notify
create_gameplay_effect
divide
division
get_gas_info
multiplicative
multiply
override
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
set_modifier_magnitude
test_activate_ability
test_apply_effect
test_get_attribute
test_get_gameplay_tags
```

---

### manage_gameplay_cues

**TS Actions:** 3 | **C++ Actions:** 38 | **Sync:** 100%

#### Extra in C++ (35)

These actions are in C++ but NOT exposed in TypeScript:

```
add
add_ability_system_component
add_ability_task
add_attribute
add_effect_cue
add_effect_execution_calculation
add_effect_modifier
add_tag_to_asset
additive
configure_asc
create_attribute_set
create_gameplay_ability
create_gameplay_effect
divide
division
get_gas_info
multiplicative
multiply
override
set_ability_cooldown
set_ability_costs
set_ability_tags
set_ability_targeting
set_activation_policy
set_attribute_base_value
set_attribute_clamping
set_effect_duration
set_effect_stacking
set_effect_tags
set_instancing_policy
set_modifier_magnitude
test_activate_ability
test_apply_effect
test_get_attribute
test_get_gameplay_tags
```

---

### test_gameplay_abilities

**TS Actions:** 4 | **C++ Actions:** 38 | **Sync:** 100%

#### Extra in C++ (34)

These actions are in C++ but NOT exposed in TypeScript:

```
add
add_ability_system_component
add_ability_task
add_attribute
add_effect_cue
add_effect_execution_calculation
add_effect_modifier
add_tag_to_asset
additive
configure_asc
configure_cue_trigger
create_attribute_set
create_gameplay_ability
create_gameplay_cue_notify
create_gameplay_effect
divide
division
get_gas_info
multiplicative
multiply
override
set_ability_cooldown
set_ability_costs
set_ability_tags
set_ability_targeting
set_activation_policy
set_attribute_base_value
set_attribute_clamping
set_cue_effects
set_effect_duration
set_effect_stacking
set_effect_tags
set_instancing_policy
set_modifier_magnitude
```

---

## Implementation Priority

### High Priority (Core Tools)

*All core tool actions are implemented!*

### Medium Priority (Frequently Used)

*All medium priority actions are implemented!*

### Lower Priority (Plugin/Optional)

Tools like `manage_xr` (0 missing) and `manage_asset_plugins` (0 missing) have many missing actions but are optional plugin integrations.
