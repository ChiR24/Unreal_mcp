// McpTool_SystemControl.cpp — system_control tool definition (25 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_SystemControl : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("system_control"); }

	FString GetDescription() const override
	{
		return TEXT("Run profiling, set quality/CVars, execute console commands, "
			"execute Python scripts, run UBT, and manage widgets.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("profile"),
				TEXT("show_fps"),
				TEXT("set_quality"),
				TEXT("screenshot"),
				TEXT("set_resolution"),
				TEXT("set_fullscreen"),
				TEXT("execute_command"),
				TEXT("console_command"),
				TEXT("run_ubt"),
				TEXT("run_tests"),
				TEXT("subscribe"),
				TEXT("unsubscribe"),
				TEXT("spawn_category"),
				TEXT("start_session"),
				TEXT("lumen_update_scene"),
				TEXT("play_sound"),
				TEXT("create_widget"),
				TEXT("show_widget"),
				TEXT("add_widget_child"),
				TEXT("set_cvar"),
				TEXT("get_project_settings"),
				TEXT("validate_assets"),
				TEXT("set_project_setting"),
				TEXT("execute_python")
			}, TEXT("Action"))
			.String(TEXT("profileType"), TEXT(""))
			.String(TEXT("category"), TEXT(""))
			.Number(TEXT("level"), TEXT(""))
			.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
			.String(TEXT("resolution"), TEXT("Resolution setting (e.g., 1024x1024)."))
			.String(TEXT("command"), TEXT(""))
			.String(TEXT("target"), TEXT(""))
			.String(TEXT("platform"), TEXT(""))
			.String(TEXT("configuration"), TEXT(""))
			.String(TEXT("arguments"), TEXT(""))
			.String(TEXT("filter"), TEXT(""))
			.String(TEXT("channels"), TEXT(""))
			.String(TEXT("widgetPath"), TEXT("Widget blueprint path."))
			.String(TEXT("childClass"), TEXT(""))
			.String(TEXT("parentName"), TEXT(""))
			.String(TEXT("section"), TEXT(""))
			.String(TEXT("key"), TEXT(""))
			.String(TEXT("value"), TEXT(""))
			.String(TEXT("configName"), TEXT(""))
			.String(TEXT("code"), TEXT("Python code to execute inline"))
			.String(TEXT("file"), TEXT("Path to .py file to execute"))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_SystemControl);
