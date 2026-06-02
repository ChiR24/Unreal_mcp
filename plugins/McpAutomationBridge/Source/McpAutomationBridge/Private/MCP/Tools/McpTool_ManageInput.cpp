// McpTool_ManageInput.cpp — manage_input tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageInput : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_input"); }

	FString GetDescription() const override
	{
		return TEXT("Create Input Actions and Mapping Contexts. Add key/gamepad "
			"bindings with modifiers and triggers.");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_input_action"),
				TEXT("create_input_mapping_context"),
				TEXT("add_mapping"),
				TEXT("remove_mapping"),
				TEXT("add_legacy_action_mapping"),
				TEXT("remove_legacy_action_mapping"),
				TEXT("add_legacy_axis_mapping"),
				TEXT("remove_legacy_axis_mapping"),
				TEXT("map_input_action"),
				TEXT("set_input_trigger"),
				TEXT("set_input_modifier"),
				TEXT("enable_input_mapping"),
				TEXT("disable_input_action"),
				TEXT("get_input_info")
			}, TEXT("Action to perform"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Path to a directory."))
			.String(TEXT("contextPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("actionPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("actionName"), TEXT("Legacy input action mapping name."))
			.String(TEXT("axisName"), TEXT("Legacy input axis mapping name."))
			.String(TEXT("key"), TEXT(""))
			.Number(TEXT("scale"), TEXT("Legacy input axis scale."))
			.Bool(TEXT("shift"), TEXT("Require Shift for a legacy action mapping."))
			.Bool(TEXT("ctrl"), TEXT("Require Ctrl for a legacy action mapping."))
			.Bool(TEXT("alt"), TEXT("Require Alt for a legacy action mapping."))
			.Bool(TEXT("cmd"), TEXT("Require Cmd for a legacy action mapping."))
			.String(TEXT("triggerType"), TEXT(""))
			.String(TEXT("modifierType"), TEXT(""))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.Number(TEXT("priority"), TEXT("Priority for input mapping context (default: 0)."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageInput);
