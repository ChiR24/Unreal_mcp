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
			.StringEnum(TEXT("type"), {
				TEXT("CPU"),
				TEXT("GPU"),
				TEXT("Memory"),
				TEXT("RenderThread"),
				TEXT("GameThread"),
				TEXT("All")
			}, TEXT("Profiling or benchmark type."))
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
			.Number(TEXT("duration"), TEXT("Duration in seconds."))
			.String(TEXT("outputPath"), TEXT("Output file or directory path."))
			.Bool(TEXT("detailed"), TEXT("Whether to include detailed output."))
			.Number(TEXT("scale"), TEXT("Resolution scale or percentage."))
			.Number(TEXT("maxFPS"), TEXT("Frame rate limit."))
			.Number(TEXT("poolSize"), TEXT("Texture streaming pool size in MB."))
			.Bool(TEXT("boostPlayerLocation"), TEXT("Whether to boost streaming around the player location."))
			.Number(TEXT("forceLOD"), TEXT("Forced LOD index."))
			.Number(TEXT("lodBias"), TEXT("LOD bias."))
			.Bool(TEXT("enableInstancing"), TEXT("Whether to enable instancing."))
			.Bool(TEXT("enableBatching"), TEXT("Whether to enable batching."))
			.Bool(TEXT("mergeActors"), TEXT("Whether to merge source actors."))
			.Array(TEXT("actors"), TEXT("Actor names."))
			.Number(TEXT("streamingDistance"), TEXT("World Partition streaming distance."))
			.Number(TEXT("cellSize"), TEXT("World Partition cell size."))
			.String(TEXT("packageName"), TEXT("Package name or asset path."))
			.Bool(TEXT("replaceSourceActors"), TEXT("Whether to replace source actors after merge."))
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
