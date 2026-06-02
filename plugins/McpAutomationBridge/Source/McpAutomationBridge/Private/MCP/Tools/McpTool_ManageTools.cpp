// McpTool_ManageTools.cpp — manage_tools tool definition (8 actions)
// Intercepted locally in HandleToolsCall — never reaches ProcessAutomationRequest

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageTools : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_tools"); }

	FString GetDescription() const override
	{
		return TEXT("Dynamic MCP tool management. Enable/disable tools and categories at runtime. "
			"Actions: list_tools, list_categories, enable_tools, disable_tools, "
			"enable_category, disable_category, get_status, reset.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("list_tools"),
				TEXT("list_categories"),
				TEXT("enable_tools"),
				TEXT("disable_tools"),
				TEXT("enable_category"),
				TEXT("disable_category"),
				TEXT("get_status"),
				TEXT("reset")
			}, TEXT("list_tools: show all tools with status. list_categories: show categories. "
				"enable/disable_tools: toggle specific tools. enable/disable_category: toggle category. "
				"get_status: current state. reset: restore defaults."))
			.Array(TEXT("tools"), TEXT("Tool names to enable/disable"))
			.String(TEXT("category"), TEXT("Category name to enable/disable (core, world, gameplay, utility, all)"))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageTools);
