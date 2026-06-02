// McpTool_ManagePipeline.cpp — manage_pipeline tool definition (3 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManagePipeline : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_pipeline"); }

	FString GetDescription() const override
	{
		return TEXT("Build automation and pipeline control. Actions: run_ubt "
			"(compile targets), list_categories (show tool categories), "
			"get_status (bridge status). Routes to system_control internally.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	// Pattern A: handler checks Action == "manage_pipeline", reads subAction from payload
	// GetDispatchAction() returns GetName() by default — correct

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("run_ubt"),
				TEXT("list_categories"),
				TEXT("get_status")
			}, TEXT("run_ubt: compile with UnrealBuildTool. "
				"list_categories: show available tool categories. "
				"get_status: get bridge status."))
			.String(TEXT("target"), TEXT("Build target name (e.g., MyProjectEditor)"))
			.String(TEXT("platform"), TEXT("Target platform (Win64, Linux, Mac)"))
			.String(TEXT("configuration"), TEXT("Build configuration (Development, Shipping, Debug)"))
			.String(TEXT("arguments"), TEXT("Additional UBT arguments"))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManagePipeline);
