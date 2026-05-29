#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpConsolidatedActions
{
inline void AppendUniqueActions(TArray<FString>& Target, const TArray<FString>& Source)
{
	for (const FString& Action : Source)
	{
		Target.AddUnique(Action);
	}
}

inline FString GetPayloadSubAction(const TSharedPtr<FJsonObject>& Payload)
{
	FString SubAction;
	if (Payload.IsValid())
	{
		if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty())
		{
			Payload->TryGetStringField(TEXT("action"), SubAction);
		}
	}
	SubAction = SubAction.ToLower();
	SubAction.ReplaceInline(TEXT("-"), TEXT("_"));
	SubAction.ReplaceInline(TEXT(" "), TEXT("_"));
	return SubAction;
}

inline TSharedPtr<FJsonObject> WithPayloadSubAction(const TSharedPtr<FJsonObject>& Payload, const FString& SubAction)
{
	if (!Payload.IsValid() || SubAction.IsEmpty())
	{
		return Payload;
	}

	TSharedPtr<FJsonObject> RoutedPayload = MakeShared<FJsonObject>();
	RoutedPayload->Values = Payload->Values;
	RoutedPayload->SetStringField(TEXT("action"), SubAction);
	RoutedPayload->SetStringField(TEXT("subAction"), SubAction);
	return RoutedPayload;
}

inline bool ContainsAction(const TArray<FString>& Actions, const FString& Action)
{
	return Actions.Contains(Action);
}

inline const TArray<FString>& ManageAssetCore()
{
	static const TArray<FString> Actions = {
		TEXT("list"), TEXT("import"), TEXT("duplicate"), TEXT("duplicate_asset"),
		TEXT("rename"), TEXT("rename_asset"), TEXT("move"), TEXT("move_asset"),
		TEXT("delete"), TEXT("delete_asset"), TEXT("delete_assets"),
		TEXT("create_folder"), TEXT("search_assets"), TEXT("get_dependencies"),
		TEXT("get_source_control_state"), TEXT("analyze_graph"),
		TEXT("get_asset_graph"), TEXT("create_thumbnail"), TEXT("set_tags"),
		TEXT("get_metadata"), TEXT("set_metadata"), TEXT("validate"),
		TEXT("fixup_redirectors"), TEXT("find_by_tag"), TEXT("generate_report"),
		TEXT("create_material"), TEXT("create_material_instance"),
		TEXT("create_render_target"), TEXT("generate_lods"),
		TEXT("add_material_parameter"), TEXT("list_instances"),
		TEXT("reset_instance_parameters"), TEXT("exists"),
		TEXT("get_material_stats"), TEXT("nanite_rebuild_mesh"),
		TEXT("bulk_rename"), TEXT("bulk_delete"),
		TEXT("source_control_checkout"), TEXT("source_control_submit"),
		TEXT("add_material_node"), TEXT("connect_material_pins"),
		TEXT("remove_material_node"), TEXT("break_material_connections"),
		TEXT("get_material_node_details"), TEXT("rebuild_material")
	};
	return Actions;
}

inline const TArray<FString>& MaterialAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_material"), TEXT("set_blend_mode"),
		TEXT("set_shading_model"), TEXT("set_material_domain"),
		TEXT("add_texture_sample"), TEXT("add_texture_coordinate"),
		TEXT("add_scalar_parameter"), TEXT("add_vector_parameter"),
		TEXT("add_static_switch_parameter"), TEXT("add_math_node"),
		TEXT("add_world_position"), TEXT("add_vertex_normal"),
		TEXT("add_pixel_depth"), TEXT("add_fresnel"),
		TEXT("add_reflection_vector"), TEXT("add_panner"), TEXT("add_rotator"),
		TEXT("add_noise"), TEXT("add_voronoi"), TEXT("add_if"),
		TEXT("add_switch"), TEXT("add_custom_expression"),
		TEXT("connect_nodes"), TEXT("connect_material_pins"),
		TEXT("disconnect_nodes"), TEXT("break_material_connections"),
		TEXT("create_material_function"), TEXT("add_function_input"),
		TEXT("add_function_output"), TEXT("use_material_function"),
		TEXT("get_material_function_info"), TEXT("create_material_instance"),
		TEXT("set_scalar_parameter_value"), TEXT("set_vector_parameter_value"),
		TEXT("set_texture_parameter_value"), TEXT("create_landscape_material"),
		TEXT("create_decal_material"), TEXT("create_post_process_material"),
		TEXT("add_landscape_layer"), TEXT("configure_layer_blend"),
		TEXT("compile_material"), TEXT("get_material_info"), TEXT("find_node"),
		TEXT("get_node_connections"), TEXT("get_node_properties"),
		TEXT("set_static_switch_parameter_value"), TEXT("delete_node"),
		TEXT("update_custom_expression"), TEXT("get_node_chain"),
		TEXT("get_connected_subgraph"), TEXT("add_material_node"),
		TEXT("rebuild_material"), TEXT("set_material_parameter"),
		TEXT("get_material_node_details"), TEXT("remove_material_node"),
		TEXT("set_two_sided")
	};
	return Actions;
}

inline const TArray<FString>& Texture()
{
	static const TArray<FString> Actions = {
		TEXT("create_noise_texture"), TEXT("create_gradient_texture"),
		TEXT("create_pattern_texture"), TEXT("create_normal_from_height"),
		TEXT("create_ao_from_mesh"), TEXT("resize_texture"),
		TEXT("adjust_levels"), TEXT("adjust_curves"), TEXT("blur"),
		TEXT("sharpen"), TEXT("invert"), TEXT("desaturate"),
		TEXT("channel_pack"), TEXT("channel_extract"), TEXT("combine_textures"),
		TEXT("set_compression_settings"), TEXT("set_texture_group"),
		TEXT("set_lod_bias"), TEXT("configure_virtual_texture"),
		TEXT("set_streaming_priority"), TEXT("get_texture_info")
	};
	return Actions;
}

inline TArray<FString> ManageAsset()
{
	TArray<FString> Actions = ManageAssetCore();
	AppendUniqueActions(Actions, MaterialAuthoring());
	AppendUniqueActions(Actions, Texture());
	return Actions;
}

inline const TArray<FString>& ManageBlueprintCore()
{
	static const TArray<FString> Actions = {
		TEXT("create"), TEXT("create_blueprint"), TEXT("get_blueprint"),
		TEXT("get"), TEXT("compile"), TEXT("add_component"),
		TEXT("set_default"), TEXT("modify_scs"), TEXT("get_scs"),
		TEXT("add_scs_component"),
		TEXT("remove_scs_component"), TEXT("reparent_scs_component"),
		TEXT("set_scs_transform"), TEXT("set_scs_property"),
		TEXT("ensure_exists"), TEXT("probe_handle"), TEXT("add_variable"),
		TEXT("remove_variable"), TEXT("rename_variable"), TEXT("add_function"),
		TEXT("add_event"), TEXT("remove_event"),
		TEXT("add_construction_script"), TEXT("set_variable_metadata"),
		TEXT("set_metadata"), TEXT("create_node"), TEXT("add_node"),
		TEXT("delete_node"), TEXT("connect_pins"), TEXT("break_pin_links"),
		TEXT("set_node_property"), TEXT("create_reroute_node"),
		TEXT("get_node_details"), TEXT("get_graph_details"),
		TEXT("get_pin_details"), TEXT("list_node_types"),
		TEXT("set_pin_default_value")
	};
	return Actions;
}

inline const TArray<FString>& WidgetAuthoring()
{
	static const TArray<FString> Actions = {
		TEXT("create_widget_blueprint"), TEXT("set_widget_parent_class"),
		TEXT("add_canvas_panel"), TEXT("add_horizontal_box"),
		TEXT("add_vertical_box"), TEXT("add_overlay"), TEXT("add_grid_panel"),
		TEXT("add_uniform_grid"), TEXT("add_wrap_box"), TEXT("add_scroll_box"),
		TEXT("add_size_box"), TEXT("add_scale_box"), TEXT("add_border"),
		TEXT("add_text_block"), TEXT("add_rich_text_block"), TEXT("add_image"),
		TEXT("add_button"), TEXT("add_check_box"), TEXT("add_slider"),
		TEXT("add_progress_bar"), TEXT("add_text_input"),
		TEXT("add_combo_box"), TEXT("add_spin_box"), TEXT("add_list_view"),
		TEXT("add_tree_view"), TEXT("set_anchor"), TEXT("set_alignment"),
		TEXT("set_position"), TEXT("set_size"), TEXT("set_padding"),
		TEXT("set_z_order"), TEXT("set_render_transform"),
		TEXT("set_visibility"), TEXT("set_style"), TEXT("set_clipping"),
		TEXT("create_property_binding"), TEXT("bind_text"),
		TEXT("bind_visibility"), TEXT("bind_color"), TEXT("bind_enabled"),
		TEXT("bind_on_clicked"), TEXT("bind_on_hovered"),
		TEXT("bind_on_value_changed"), TEXT("create_widget_animation"),
		TEXT("add_animation_track"), TEXT("add_animation_keyframe"),
		TEXT("set_animation_loop"), TEXT("create_main_menu"),
		TEXT("create_pause_menu"), TEXT("create_settings_menu"),
		TEXT("create_loading_screen"), TEXT("create_hud_widget"),
		TEXT("add_health_bar"), TEXT("add_ammo_counter"),
		TEXT("add_minimap"), TEXT("add_crosshair"), TEXT("add_compass"),
		TEXT("add_interaction_prompt"), TEXT("add_objective_tracker"),
		TEXT("add_damage_indicator"), TEXT("create_inventory_ui"),
		TEXT("create_dialog_widget"), TEXT("create_radial_menu"),
		TEXT("get_widget_info"), TEXT("preview_widget")
	};
	return Actions;
}

inline TArray<FString> ManageBlueprint()
{
	TArray<FString> Actions = ManageBlueprintCore();
	AppendUniqueActions(Actions, WidgetAuthoring());
	return Actions;
}

inline const TArray<FString>& BuildEnvironmentCore()
{
	static const TArray<FString> Actions = {
		TEXT("create_landscape"), TEXT("sculpt"), TEXT("sculpt_landscape"),
		TEXT("add_foliage"), TEXT("paint_foliage"),
		TEXT("create_procedural_terrain"),
		TEXT("create_procedural_foliage"),
		TEXT("add_foliage_instances"), TEXT("get_foliage_instances"),
		TEXT("remove_foliage"), TEXT("paint_landscape"),
		TEXT("paint_landscape_layer"), TEXT("modify_heightmap"),
		TEXT("set_landscape_material"), TEXT("create_landscape_grass_type"),
		TEXT("generate_lods"), TEXT("bake_lightmap"),
		TEXT("export_snapshot"), TEXT("import_snapshot"), TEXT("delete"),
		TEXT("create_sky_sphere"), TEXT("set_time_of_day"),
		TEXT("create_fog_volume")
	};
	return Actions;
}

inline const TArray<FString>& Lighting()
{
	static const TArray<FString> Actions = {
		TEXT("spawn_light"), TEXT("create_light"), TEXT("spawn_sky_light"),
		TEXT("create_sky_light"), TEXT("ensure_single_sky_light"),
		TEXT("create_lightmass_volume"),
		TEXT("create_lighting_enabled_level"), TEXT("create_dynamic_light"),
		TEXT("setup_global_illumination"), TEXT("configure_shadows"),
		TEXT("set_exposure"), TEXT("set_ambient_occlusion"),
		TEXT("setup_volumetric_fog"), TEXT("build_lighting"),
		TEXT("list_light_types")
	};
	return Actions;
}

inline const TArray<FString>& Splines()
{
	static const TArray<FString> Actions = {
		TEXT("create_spline_actor"), TEXT("add_spline_point"),
		TEXT("remove_spline_point"), TEXT("set_spline_point_position"),
		TEXT("set_spline_point_tangents"), TEXT("set_spline_point_rotation"),
		TEXT("set_spline_point_scale"), TEXT("set_spline_type"),
		TEXT("create_spline_mesh_component"), TEXT("set_spline_mesh_asset"),
		TEXT("configure_spline_mesh_axis"),
		TEXT("set_spline_mesh_material"),
		TEXT("scatter_meshes_along_spline"),
		TEXT("configure_mesh_spacing"),
		TEXT("configure_mesh_randomization"), TEXT("create_road_spline"),
		TEXT("create_river_spline"), TEXT("create_fence_spline"),
		TEXT("create_wall_spline"), TEXT("create_cable_spline"),
		TEXT("create_pipe_spline"), TEXT("get_splines_info")
	};
	return Actions;
}

inline TArray<FString> BuildEnvironment()
{
	TArray<FString> Actions = BuildEnvironmentCore();
	AppendUniqueActions(Actions, Lighting());
	AppendUniqueActions(Actions, Splines());
	return Actions;
}

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

inline const TArray<FString>& ManageNetworkingCore()
{
	static const TArray<FString> Actions = {
		TEXT("set_property_replicated"), TEXT("set_replication_condition"),
		TEXT("configure_net_update_frequency"), TEXT("configure_net_priority"),
		TEXT("set_net_dormancy"), TEXT("configure_replication_graph"),
		TEXT("create_rpc_function"), TEXT("configure_rpc_validation"),
		TEXT("set_rpc_reliability"), TEXT("set_owner"),
		TEXT("set_autonomous_proxy"), TEXT("check_has_authority"),
		TEXT("check_is_locally_controlled"),
		TEXT("configure_net_cull_distance"), TEXT("set_always_relevant"),
		TEXT("set_only_relevant_to_owner"),
		TEXT("configure_net_serialization"), TEXT("set_replicated_using"),
		TEXT("configure_push_model"), TEXT("configure_client_prediction"),
		TEXT("configure_server_correction"),
		TEXT("add_network_prediction_data"),
		TEXT("configure_movement_prediction"), TEXT("configure_net_driver"),
		TEXT("set_net_role"), TEXT("configure_replicated_movement"),
		TEXT("get_networking_info")
	};
	return Actions;
}

inline const TArray<FString>& Input()
{
	static const TArray<FString> Actions = {
		TEXT("create_input_action"), TEXT("create_input_mapping_context"),
		TEXT("add_mapping"), TEXT("remove_mapping"), TEXT("map_input_action"),
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
		TEXT("set_game_state_class"), TEXT("set_player_state_class"),
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

inline TArray<FString> ManageNetworking()
{
	TArray<FString> Actions = ManageNetworkingCore();
	AppendUniqueActions(Actions, Input());
	AppendUniqueActions(Actions, GameFramework());
	AppendUniqueActions(Actions, Sessions());
	return Actions;
}

inline const TArray<FString>& ManageLevelStructureCore()
{
	static const TArray<FString> Actions = {
		TEXT("create_level"), TEXT("create_sublevel"),
		TEXT("configure_level_streaming"), TEXT("set_streaming_distance"),
		TEXT("configure_level_bounds"), TEXT("enable_world_partition"),
		TEXT("configure_grid_size"), TEXT("create_data_layer"),
		TEXT("assign_actor_to_data_layer"), TEXT("configure_hlod_layer"),
		TEXT("create_minimap_volume"), TEXT("open_level_blueprint"),
		TEXT("add_level_blueprint_node"),
		TEXT("connect_level_blueprint_nodes"), TEXT("create_level_instance"),
		TEXT("create_packed_level_actor"), TEXT("get_level_structure_info")
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

inline TArray<FString> ManageLevelStructure()
{
	TArray<FString> Actions = ManageLevelStructureCore();
	AppendUniqueActions(Actions, Volumes());
	return Actions;
}

inline const TArray<FString>& ManageAICore()
{
	static const TArray<FString> Actions = {
		TEXT("create_ai_controller"), TEXT("assign_behavior_tree"),
		TEXT("assign_blackboard"), TEXT("create_blackboard_asset"),
		TEXT("add_blackboard_key"), TEXT("set_key_instance_synced"),
		TEXT("create_behavior_tree"), TEXT("add_composite_node"),
		TEXT("add_task_node"), TEXT("add_decorator"), TEXT("add_service"),
		TEXT("configure_bt_node"), TEXT("create_eqs_query"),
		TEXT("add_eqs_generator"), TEXT("add_eqs_context"),
		TEXT("add_eqs_test"), TEXT("configure_test_scoring"),
		TEXT("add_ai_perception_component"), TEXT("configure_sight_config"),
		TEXT("configure_hearing_config"),
		TEXT("configure_damage_sense_config"), TEXT("set_perception_team"),
		TEXT("create_state_tree"), TEXT("add_state_tree_state"),
		TEXT("add_state_tree_transition"), TEXT("configure_state_tree_task"),
		TEXT("create_smart_object_definition"),
		TEXT("add_smart_object_slot"), TEXT("configure_slot_behavior"),
		TEXT("add_smart_object_component"),
		TEXT("create_mass_entity_config"), TEXT("configure_mass_entity"),
		TEXT("add_mass_spawner"), TEXT("get_ai_info"),
		TEXT("create_blackboard"), TEXT("setup_perception"),
		TEXT("create_nav_link_proxy"), TEXT("set_focus"), TEXT("clear_focus"),
		TEXT("set_blackboard_value"), TEXT("get_blackboard_value"),
		TEXT("run_behavior_tree"), TEXT("stop_behavior_tree"),
		TEXT("create"), TEXT("add_node"), TEXT("connect_nodes"),
		TEXT("remove_node"), TEXT("break_connections"),
		TEXT("set_node_properties"), TEXT("configure_nav_mesh_settings"),
		TEXT("set_nav_agent_properties"), TEXT("rebuild_navigation"),
		TEXT("create_nav_modifier_component"), TEXT("set_nav_area_class"),
		TEXT("configure_nav_area_cost"), TEXT("configure_nav_link"),
		TEXT("set_nav_link_type"), TEXT("create_smart_link"),
		TEXT("configure_smart_link_behavior"), TEXT("get_navigation_info")
	};
	return Actions;
}

inline const TArray<FString>& BehaviorTree()
{
	static const TArray<FString> Actions = {
		TEXT("create"), TEXT("add_node"), TEXT("connect_nodes"),
		TEXT("remove_node"), TEXT("break_connections"),
		TEXT("set_node_properties"), TEXT("add_subnode")
	};
	return Actions;
}

inline const TArray<FString>& Navigation()
{
	static const TArray<FString> Actions = {
		TEXT("configure_nav_mesh_settings"),
		TEXT("set_nav_agent_properties"), TEXT("rebuild_navigation"),
		TEXT("create_nav_modifier_component"), TEXT("set_nav_area_class"),
		TEXT("configure_nav_area_cost"), TEXT("create_nav_link_proxy"),
		TEXT("configure_nav_link"), TEXT("set_nav_link_type"),
		TEXT("create_smart_link"), TEXT("configure_smart_link_behavior"),
		TEXT("get_navigation_info")
	};
	return Actions;
}

inline TArray<FString> ManageAI()
{
	TArray<FString> Actions = ManageAICore();
	AppendUniqueActions(Actions, BehaviorTree());
	AppendUniqueActions(Actions, Navigation());
	return Actions;
}

inline bool IsMaterialAuthoringAction(const FString& Action) { return ContainsAction(MaterialAuthoring(), Action); }
inline bool IsTextureAction(const FString& Action) { return ContainsAction(Texture(), Action); }
inline bool IsWidgetAuthoringAction(const FString& Action) { return ContainsAction(WidgetAuthoring(), Action); }
inline bool IsAnimationAuthoringAction(const FString& Action) { return ContainsAction(AnimationAuthoring(), Action); }
inline bool IsAudioAuthoringAction(const FString& Action) { return ContainsAction(AudioAuthoring(), Action); }
inline bool IsLightingAction(const FString& Action) { return ContainsAction(Lighting(), Action); }
inline bool IsSplineAction(const FString& Action) { return ContainsAction(Splines(), Action); }
inline bool IsSkeletonAction(const FString& Action) { return ContainsAction(Skeleton(), Action); }
inline bool IsPerformanceAction(const FString& Action) { return ContainsAction(Performance(), Action); }
inline bool IsInputAction(const FString& Action) { return ContainsAction(Input(), Action); }
inline bool IsGameFrameworkAction(const FString& Action) { return ContainsAction(GameFramework(), Action); }
inline bool IsSessionAction(const FString& Action) { return ContainsAction(Sessions(), Action); }
inline bool IsVolumeAction(const FString& Action) { return ContainsAction(Volumes(), Action); }
inline bool IsBehaviorTreeAction(const FString& Action) { return ContainsAction(BehaviorTree(), Action); }
inline bool IsNavigationAction(const FString& Action) { return ContainsAction(Navigation(), Action); }
}
