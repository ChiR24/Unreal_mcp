// McpTool_ManageSequence.cpp — manage_sequence tool definition

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "MCP/Tools/Utility/McpTool_ManageSequenceSchemaFields.h"
#include "MCP/Registry/McpToolDefinition.h"
#include "MCP/Registry/McpToolRegistry.h"
#include "MCP/Registry/McpSchemaBuilder.h"

class FMcpTool_ManageSequence : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_sequence"); }

	FString GetDescription() const override
	{
		return TEXT("Edit Level Sequences, cinematic tracks, Movie Render Queue jobs, "
			"media playback assets, Take Recorder, and replay controls.");
	}

	FString GetCategory() const override { return TEXT("utility"); }
	bool EnforceStrictArguments() const override { return true; }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		FMcpSchemaBuilder Schema;
		Schema.StringEnum(TEXT("action"), {
				TEXT("create"),
				TEXT("open"),
				TEXT("add_camera"),
				TEXT("add_actor"),
				TEXT("add_actors"),
				TEXT("remove_actors"),
				TEXT("get_bindings"),
				TEXT("play"),
				TEXT("pause"),
				TEXT("stop"),
				TEXT("set_playback_speed"),
				TEXT("add_keyframe"),
				TEXT("get_properties"),
				TEXT("set_properties"),
				TEXT("duplicate"),
				TEXT("rename"),
				TEXT("delete"),
				TEXT("list"),
				TEXT("get_metadata"),
				TEXT("set_metadata"),
				TEXT("add_spawnable_from_class"),
				TEXT("add_track"),
				TEXT("add_section"),
				TEXT("set_display_rate"),
				TEXT("set_tick_resolution"),
				TEXT("set_work_range"),
				TEXT("set_view_range"),
				TEXT("set_track_muted"),
				TEXT("set_track_solo"),
				TEXT("set_track_locked"),
				TEXT("list_tracks"),
				TEXT("remove_track"),
				TEXT("list_track_types"),
				TEXT("create_master_sequence"),
				TEXT("add_subsequence"),
				TEXT("add_shot_track"),
				TEXT("configure_shot_settings"),
				TEXT("create_cine_camera_actor"),
				TEXT("configure_camera_settings"),
				TEXT("add_camera_cut_track"),
				TEXT("add_camera_shake_track"),
				TEXT("configure_camera_rig_rail"),
				TEXT("configure_camera_rig_crane"),
				TEXT("add_fade_track"),
				TEXT("add_level_visibility_track"),
				TEXT("add_material_parameter_track"),
				TEXT("add_particle_track"),
				TEXT("add_skeletal_animation_track"),
				TEXT("add_transform_track"),
				TEXT("add_event_track"),
				TEXT("add_property_track"),
				TEXT("create_render_job"),
				TEXT("configure_output_settings"),
				TEXT("add_render_pass"),
				TEXT("configure_anti_aliasing"),
				TEXT("configure_console_variables"),
				TEXT("configure_burn_ins"),
				TEXT("queue_render"),
				TEXT("start_render"),
				TEXT("create_media_player"),
				TEXT("create_media_source"),
				TEXT("create_media_texture"),
				TEXT("create_media_sound_component"),
				TEXT("create_media_playlist"),
				TEXT("play_media"),
				TEXT("pause_media"),
				TEXT("seek_media"),
				TEXT("create_take_recorder_panel"),
				TEXT("configure_take_sources"),
				TEXT("start_recording"),
				TEXT("stop_recording"),
				TEXT("configure_recorded_tracks"),
				TEXT("start_demo_recording"),
				TEXT("stop_demo_recording"),
				TEXT("configure_demo_settings"),
				TEXT("play_demo"),
				TEXT("pause_demo"),
				TEXT("seek_demo"),
				TEXT("set_demo_playback_speed"),
				TEXT("configure_killcam_duration"),
				TEXT("start_killcam")
			}, TEXT("Action"));
		McpManageSequenceSchemaFields::AddManageSequenceFields(Schema);
		Schema.Required({TEXT("action")});
		return Schema.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageSequence);
