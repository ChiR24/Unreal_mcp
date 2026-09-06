#pragma once

#include "CoreMinimal.h"

namespace McpConsolidatedActions
{
inline const TArray<FString>& Input()
{
	static const TArray<FString> Actions = {
		TEXT("create_input_action"), TEXT("create_input_mapping_context"),
		TEXT("add_mapping"), TEXT("remove_mapping"), TEXT("map_input_action"),
		TEXT("add_legacy_action_mapping"), TEXT("remove_legacy_action_mapping"),
		TEXT("add_legacy_axis_mapping"), TEXT("remove_legacy_axis_mapping"),
		TEXT("set_input_trigger"), TEXT("set_input_modifier"),
		TEXT("enable_input_mapping"), TEXT("disable_input_action"),
		TEXT("get_input_info")
	};
	return Actions;
}

inline const TArray<FString>& GameFramework()
{
	static const TArray<FString> Actions = {
		TEXT("create_game_mode"), TEXT("create_game_state"),
		TEXT("create_player_controller"), TEXT("create_player_state"),
		TEXT("create_game_instance"), TEXT("create_hud_class"),
		TEXT("set_default_pawn_class"), TEXT("set_player_controller_class"),
		TEXT("set_game_state_class"), TEXT("set_player_state_class"), TEXT("set_hud_class"),
		TEXT("configure_game_rules"), TEXT("setup_match_states"),
		TEXT("configure_round_system"), TEXT("configure_team_system"),
		TEXT("configure_scoring_system"), TEXT("configure_spawn_system"),
		TEXT("configure_player_start"), TEXT("set_respawn_rules"),
		TEXT("configure_spectating"), TEXT("get_game_framework_info")
	};
	return Actions;
}

inline const TArray<FString>& Sessions()
{
	static const TArray<FString> Actions = {
		TEXT("configure_local_session_settings"),
		TEXT("configure_session_interface"), TEXT("configure_split_screen"),
		TEXT("set_split_screen_type"), TEXT("add_local_player"),
		TEXT("remove_local_player"), TEXT("configure_lan_play"),
		TEXT("host_lan_server"), TEXT("join_lan_server"),
		TEXT("enable_voice_chat"), TEXT("configure_voice_settings"),
		TEXT("set_voice_channel"), TEXT("mute_player"),
		TEXT("set_voice_attenuation"), TEXT("configure_push_to_talk"),
		TEXT("get_sessions_info")
	};
	return Actions;
}

inline const TArray<FString>& Volumes()
{
	static const TArray<FString> Actions = {
		TEXT("create_trigger_volume"), TEXT("add_trigger_volume"),
		TEXT("create_trigger_box"), TEXT("create_trigger_sphere"),
		TEXT("create_trigger_capsule"), TEXT("create_blocking_volume"),
		TEXT("add_blocking_volume"), TEXT("create_kill_z_volume"),
		TEXT("add_kill_z_volume"), TEXT("create_pain_causing_volume"),
		TEXT("create_physics_volume"), TEXT("add_physics_volume"),
		TEXT("create_audio_volume"), TEXT("create_reverb_volume"),
		TEXT("create_cull_distance_volume"),
		TEXT("add_cull_distance_volume"),
		TEXT("create_precomputed_visibility_volume"),
		TEXT("create_lightmass_importance_volume"),
		TEXT("create_nav_mesh_bounds_volume"),
		TEXT("create_nav_modifier_volume"),
		TEXT("create_camera_blocking_volume"),
		TEXT("create_post_process_volume"),
		TEXT("add_post_process_volume"), TEXT("set_volume_extent"),
		TEXT("set_volume_bounds"), TEXT("set_volume_properties"),
		TEXT("remove_volume"), TEXT("get_volumes_info")
	};
	return Actions;
}
}
