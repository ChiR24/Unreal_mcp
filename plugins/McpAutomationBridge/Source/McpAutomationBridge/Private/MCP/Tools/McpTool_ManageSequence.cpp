// McpTool_ManageSequence.cpp — manage_sequence tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageSequence : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_sequence"); }

	FString GetDescription() const override
	{
		return TEXT("Edit Level Sequences: add tracks, bind actors, set keyframes, "
			"control playback, and record camera.");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
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
				TEXT("list_track_types")
			}, TEXT("Action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.Array(TEXT("actorNames"), TEXT(""))
			.Number(TEXT("frame"), TEXT(""))
			.FreeformObject(TEXT("value"), TEXT(""))
			.String(TEXT("property"), TEXT("Name of the property."))
			.String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
			.String(TEXT("newName"), TEXT("New name for renaming."))
			.Number(TEXT("speed"), TEXT(""))
			.Number(TEXT("startTime"), TEXT(""))
			.String(TEXT("loopMode"), TEXT(""))
			.String(TEXT("className"), TEXT(""))
			.Bool(TEXT("spawnable"), TEXT(""))
			.String(TEXT("trackType"), TEXT(""))
			.String(TEXT("trackName"), TEXT(""))
			.Bool(TEXT("muted"), TEXT(""))
			.Bool(TEXT("solo"), TEXT(""))
			.Bool(TEXT("locked"), TEXT(""))
			.Number(TEXT("startFrame"), TEXT(""))
			.Number(TEXT("endFrame"), TEXT(""))
			.String(TEXT("frameRate"), TEXT(""))
			.String(TEXT("resolution"), TEXT(""))
			.Number(TEXT("start"), TEXT(""))
			.Number(TEXT("end"), TEXT(""))
			.Number(TEXT("lengthInFrames"), TEXT(""))
			.Number(TEXT("playbackStart"), TEXT(""))
			.Number(TEXT("playbackEnd"), TEXT(""))
			.FreeformObject(TEXT("metadata"), TEXT(""))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageSequence);
