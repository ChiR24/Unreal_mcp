// McpTool_ControlEditor.cpp — control_editor tool definition (40 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ControlEditor : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("control_editor"); }

	FString GetDescription() const override
	{
		return TEXT("Start/stop PIE, control viewport camera, run console commands, "
			"take screenshots, simulate input.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("play"),
				TEXT("stop"),
				TEXT("stop_pie"),
				TEXT("pause"),
				TEXT("resume"),
				TEXT("eject"),
				TEXT("possess"),
				TEXT("set_game_speed"),
				TEXT("set_fixed_delta_time"),
				TEXT("set_camera"),
				TEXT("set_camera_position"),
				TEXT("set_viewport_camera"),
				TEXT("set_camera_fov"),
				TEXT("set_view_mode"),
				TEXT("set_viewport_resolution"),
				TEXT("console_command"),
				TEXT("execute_command"),
				TEXT("screenshot"),
				TEXT("take_screenshot"),
				TEXT("step_frame"),
				TEXT("single_frame_step"),
				TEXT("start_recording"),
				TEXT("stop_recording"),
				TEXT("create_bookmark"),
				TEXT("jump_to_bookmark"),
				TEXT("set_preferences"),
				TEXT("set_viewport_realtime"),
				TEXT("open_asset"),
				TEXT("close_asset"),
				TEXT("simulate_input"),
				TEXT("open_level"),
				TEXT("focus_actor"),
				TEXT("show_stats"),
				TEXT("hide_stats"),
				TEXT("set_editor_mode"),
				TEXT("set_immersive_mode"),
				TEXT("set_game_view"),
				TEXT("undo"),
				TEXT("redo"),
				TEXT("save_all")
			}, TEXT("Editor action. Note: screenshot/take_screenshot is async "
				"\u2014 the file is written on the next rendered viewport frame, "
				"not immediately. The editor window must be visible and actively "
				"rendering for capture to complete."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.String(TEXT("viewMode"), TEXT(""))
			.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
			.Number(TEXT("speed"), TEXT(""))
			.String(TEXT("filename"), TEXT(""))
			.Number(TEXT("fov"), TEXT(""))
			.Number(TEXT("width"), TEXT(""))
			.Number(TEXT("height"), TEXT(""))
			.String(TEXT("command"), TEXT(""))
			.Integer(TEXT("steps"), TEXT(""))
			.String(TEXT("bookmarkName"), TEXT(""))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("levelPath"), TEXT("Level asset path."))
			.String(TEXT("path"), TEXT("Path to a directory."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("mode"), TEXT(""))
			.Number(TEXT("deltaTime"), TEXT(""))
			.String(TEXT("resolution"), TEXT("Resolution setting (e.g., 1024x1024)."))
			.Bool(TEXT("realtime"), TEXT(""))
			.String(TEXT("stat"), TEXT(""))
			.String(TEXT("category"), TEXT(""))
			.FreeformObject(TEXT("preferences"), TEXT(""))
			.String(TEXT("section"), TEXT(""))
			.String(TEXT("key"), TEXT(""))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.String(TEXT("inputAction"), TEXT(""))
			.String(TEXT("axis"), TEXT(""))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ControlEditor);
