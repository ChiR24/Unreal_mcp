#pragma once

#include "CoreMinimal.h"

namespace McpConsolidatedActions
{
inline const TArray<FString>& AnimationPhysicsCore()
{
	static const TArray<FString> Actions = {
		TEXT("create_animation_blueprint"), TEXT("create_animation_bp"),
		TEXT("create_anim_blueprint"), TEXT("create_blend_space"),
		TEXT("create_blend_space_1d"), TEXT("create_blend_space_2d"),
		TEXT("create_blend_tree"), TEXT("create_procedural_anim"),
		TEXT("create_aim_offset"), TEXT("add_aim_offset_sample"),
		TEXT("create_state_machine"), TEXT("add_state_machine"),
		TEXT("add_state"), TEXT("add_transition"),
		TEXT("set_transition_rules"), TEXT("add_blend_node"),
		TEXT("add_cached_pose"), TEXT("add_slot_node"),
		TEXT("create_control_rig"), TEXT("create_ik_rig"),
		TEXT("setup_ik"), TEXT("create_pose_library"),
		TEXT("create_animation_asset"), TEXT("create_animation_sequence"),
		TEXT("set_sequence_length"), TEXT("add_bone_track"),
		TEXT("set_bone_key"), TEXT("set_curve_key"), TEXT("create_montage"),
		TEXT("add_montage_section"), TEXT("add_montage_slot"),
		TEXT("set_section_timing"), TEXT("add_montage_notify"),
		TEXT("set_blend_in"), TEXT("set_blend_out"), TEXT("link_sections"),
		TEXT("add_notify"), TEXT("play_montage"),
		TEXT("play_anim_montage"), TEXT("setup_ragdoll"),
		TEXT("activate_ragdoll"), TEXT("configure_vehicle"),
		TEXT("setup_physics_simulation"), TEXT("add_blend_sample"),
		TEXT("set_axis_settings"), TEXT("set_interpolation_settings"),
		TEXT("setup_retargeting"), TEXT("cleanup")
	};
	return Actions;
}

inline const TArray<FString>& AnimationAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_animation_sequence"), TEXT("set_sequence_length"),
		TEXT("add_bone_track"), TEXT("set_bone_key"), TEXT("set_curve_key"),
		TEXT("add_notify_state"), TEXT("add_sync_marker"),
		TEXT("set_root_motion_settings"), TEXT("set_additive_settings"),
		TEXT("create_montage"), TEXT("add_montage_section"),
		TEXT("add_montage_slot"), TEXT("set_section_timing"),
		TEXT("add_montage_notify"), TEXT("set_blend_in"),
		TEXT("set_blend_out"), TEXT("link_sections"),
		TEXT("create_blend_space_1d"), TEXT("create_blend_space_2d"),
		TEXT("add_blend_sample"), TEXT("force_rebuild_blend_space"),
		TEXT("set_axis_settings"), TEXT("set_interpolation_settings"),
		TEXT("create_aim_offset"), TEXT("add_aim_offset_sample"),
		TEXT("create_anim_blueprint"), TEXT("create_animation_bp"),
		TEXT("create_animation_blueprint"), TEXT("add_state_machine"),
		TEXT("add_state"), TEXT("add_transition"),
		TEXT("set_transition_rules"), TEXT("add_blend_node"),
		TEXT("add_cached_pose"), TEXT("add_slot_node"),
		TEXT("add_layered_blend_per_bone"),
		TEXT("set_anim_graph_node_value"), TEXT("create_control_rig"),
		TEXT("create_ik_rig"), TEXT("create_ik_retargeter"),
		TEXT("set_retarget_chain_mapping"), TEXT("get_animation_info")
	};
	return Actions;
}

inline const TArray<FString>& Skeleton()
{
	static const TArray<FString> Actions = {
		TEXT("create_skeleton"), TEXT("add_bone"), TEXT("remove_bone"),
		TEXT("rename_bone"), TEXT("set_bone_transform"),
		TEXT("set_bone_parent"), TEXT("create_virtual_bone"),
		TEXT("create_socket"), TEXT("configure_socket"),
		TEXT("auto_skin_weights"), TEXT("set_vertex_weights"),
		TEXT("normalize_weights"), TEXT("prune_weights"), TEXT("copy_weights"),
		TEXT("mirror_weights"), TEXT("create_physics_asset"),
		TEXT("add_physics_body"), TEXT("configure_physics_body"),
		TEXT("add_physics_constraint"), TEXT("configure_constraint_limits"),
		TEXT("bind_cloth_to_skeletal_mesh"),
		TEXT("assign_cloth_asset_to_mesh"), TEXT("create_morph_target"),
		TEXT("set_morph_target_deltas"), TEXT("import_morph_targets"),
		TEXT("get_skeleton_info"), TEXT("list_bones"), TEXT("list_sockets"),
		TEXT("list_physics_bodies")
	};
	return Actions;
}

inline TArray<FString> AnimationPhysics()
{
	TArray<FString> Actions = AnimationPhysicsCore();
	AppendUniqueActions(Actions, AnimationAuthoring());
	AppendUniqueActions(Actions, Skeleton());
	return Actions;
}

inline const TArray<FString>& AudioAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_sound_cue"), TEXT("create_sound_class"),
		TEXT("create_sound_mix"),
		TEXT("add_cue_node"), TEXT("connect_cue_nodes"),
		TEXT("set_cue_attenuation"), TEXT("set_cue_concurrency"),
		TEXT("create_metasound"), TEXT("add_metasound_node"),
		TEXT("connect_metasound_nodes"), TEXT("add_metasound_input"),
		TEXT("add_metasound_output"), TEXT("set_metasound_default"),
		TEXT("set_class_properties"), TEXT("set_class_parent"),
		TEXT("add_mix_modifier"), TEXT("configure_mix_eq"),
		TEXT("create_attenuation_settings"),
		TEXT("configure_distance_attenuation"),
		TEXT("configure_spatialization"), TEXT("configure_occlusion"),
		TEXT("configure_reverb_send"), TEXT("create_dialogue_voice"),
		TEXT("create_dialogue_wave"), TEXT("set_dialogue_context"),
		TEXT("create_reverb_effect"), TEXT("create_source_effect_chain"),
		TEXT("add_source_effect"), TEXT("create_submix_effect"),
		TEXT("get_audio_info")
	};
	return Actions;
}

inline const TArray<FString>& SystemControlCore()
{
	static const TArray<FString> Actions = {
		TEXT("profile"), TEXT("show_fps"), TEXT("set_quality"),
		TEXT("screenshot"), TEXT("set_resolution"), TEXT("set_fullscreen"),
		TEXT("execute_command"), TEXT("console_command"), TEXT("run_ubt"),
		TEXT("run_tests"), TEXT("subscribe"), TEXT("unsubscribe"),
		TEXT("spawn_category"), TEXT("start_session"),
		TEXT("lumen_update_scene"), TEXT("play_sound"), TEXT("create_widget"),
		TEXT("show_widget"), TEXT("add_widget_child"), TEXT("set_cvar"),
		TEXT("get_project_settings"), TEXT("validate_assets"),
		TEXT("set_project_setting"), TEXT("execute_python")
	};
	return Actions;
}

inline const TArray<FString>& Performance()
{
	static const TArray<FString> Actions = {
		TEXT("start_profiling"), TEXT("stop_profiling"),
		TEXT("run_benchmark"), TEXT("show_fps"), TEXT("show_stats"),
		TEXT("generate_memory_report"), TEXT("set_scalability"),
		TEXT("set_resolution_scale"), TEXT("set_vsync"),
		TEXT("set_frame_rate_limit"), TEXT("enable_gpu_timing"),
		TEXT("configure_texture_streaming"), TEXT("configure_lod"),
		TEXT("apply_baseline_settings"), TEXT("optimize_draw_calls"),
		TEXT("merge_actors"), TEXT("configure_occlusion_culling"),
		TEXT("optimize_shaders"), TEXT("configure_nanite"),
		TEXT("configure_world_partition")
	};
	return Actions;
}

inline TArray<FString> SystemControl()
{
	TArray<FString> Actions = SystemControlCore();
	AppendUniqueActions(Actions, Performance());
	return Actions;
}
}
