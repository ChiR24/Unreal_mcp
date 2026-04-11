// McpTool_ManageBehaviorTree.cpp — manage_behavior_tree tool definition (6 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageBehaviorTree : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_behavior_tree"); }

	FString GetDescription() const override
	{
		return TEXT("Create Behavior Trees, add task/decorator/service nodes, "
			"and configure node properties.");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create"),
				TEXT("add_node"),
				TEXT("connect_nodes"),
				TEXT("remove_node"),
				TEXT("break_connections"),
				TEXT("set_node_properties")
			}, TEXT("Action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("savePath"), TEXT("Path to save the asset."))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("nodeType"), TEXT(""))
			.String(TEXT("nodeId"), TEXT("ID of the node."))
			.String(TEXT("parentNodeId"), TEXT("ID of the node."))
			.String(TEXT("childNodeId"), TEXT("ID of the node."))
			.Number(TEXT("x"), TEXT(""))
			.Number(TEXT("y"), TEXT(""))
			.String(TEXT("comment"), TEXT(""))
			.FreeformObject(TEXT("properties"), TEXT(""))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageBehaviorTree);
