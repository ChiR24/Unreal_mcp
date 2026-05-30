// McpTool_SystemControl.cpp — system_control tool definition (25 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_SystemControl : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("system_control"); }

	FString GetDescription() const override
	{
		return TEXT("Run profiling, set quality/CVars, execute console commands, "
			"execute Python scripts, run UBT, manage widgets, and take screenshots.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
				.StringEnum(TEXT("action"), McpConsolidatedActions::SystemControl(),
					TEXT("Action"))
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
			.String(TEXT("filename"), TEXT("Screenshot filename."))
			.String(TEXT("mode"), TEXT("Screenshot source: editor_viewport, game_viewport, full_editor_window."))
			.Bool(TEXT("returnBase64"), TEXT("Return PNG image data as base64 when supported. Defaults to true for full_editor_window and game_viewport modes."))
			.Bool(TEXT("includeMetadata"), TEXT("Attach caller-provided metadata to the response."))
			.FreeformObject(TEXT("metadata"), TEXT("Caller-provided screenshot metadata."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_SystemControl);
