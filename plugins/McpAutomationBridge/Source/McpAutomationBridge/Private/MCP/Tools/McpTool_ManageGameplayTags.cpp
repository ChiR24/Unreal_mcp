// McpTool_ManageGameplayTags.cpp — manage_gameplay_tags tool definition (Ch4)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageGameplayTags : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_gameplay_tags"); }

	FString GetDescription() const override
	{
		return TEXT("Manage GameplayTag registrations in project config (ini-based tags). "
			"Add/remove/list tags and register additional tag source ini files via "
			"IGameplayTagsEditorModule.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("add_gameplay_tag"),
				TEXT("remove_gameplay_tag"),
				TEXT("list_gameplay_tags"),
				TEXT("add_gameplay_tag_source")
			}, TEXT("GameplayTag action to perform."))
			.String(TEXT("tag"), TEXT("Tag name, e.g. \"Modifier.Weather.Rain\"."))
			.String(TEXT("comment"), TEXT("Developer comment for the tag (add_gameplay_tag)."))
			.String(TEXT("sourceIni"),
				TEXT("Ini file name for the tag source (e.g. \"DefaultGameplayTags.ini\"). "
					"Optional; defaults to DefaultGameplayTags.ini."))
			.String(TEXT("prefix"), TEXT("Optional prefix filter for list_gameplay_tags."))
			.String(TEXT("iniRelativePath"),
				TEXT("Path for add_gameplay_tag_source, relative to Config/."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageGameplayTags);
