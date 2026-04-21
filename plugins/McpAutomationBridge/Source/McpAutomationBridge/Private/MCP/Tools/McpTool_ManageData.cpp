// McpTool_ManageData.cpp — manage_data tool definition (Ch2 DataTable + Ch3 DataAsset)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageData : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_data"); }

	FString GetDescription() const override
	{
		return TEXT("Create and modify UDataTable / UDataAsset instances "
			"(row-level CRUD, property paths, schema migration).");
	}

	FString GetCategory() const override { return TEXT("authoring"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_data_table"),
				TEXT("add_data_table_row"),
				TEXT("set_data_table_row"),
				TEXT("update_data_table_row"),
				TEXT("remove_data_table_row"),
				TEXT("get_data_table_rows"),
				TEXT("list_data_table_rows"),
				TEXT("set_data_table_row_struct"),
				TEXT("create_data_asset"),
				TEXT("set_data_asset_property"),
				TEXT("get_data_asset_property"),
				TEXT("list_data_assets_of_class")
			}, TEXT("Data-layer action to perform."))
			.String(TEXT("path"), TEXT("Package path (/Game/...) for the target asset."))
			.String(TEXT("name"), TEXT("Asset name (for create actions)."))
			.String(TEXT("rowStructPath"),
				TEXT("Path to UScriptStruct / UUserDefinedStruct used as DataTable row type."))
			.String(TEXT("newRowStructPath"),
				TEXT("Target row struct path for set_data_table_row_struct migration."))
			.String(TEXT("rowName"), TEXT("DataTable row name."))
			.Array(TEXT("rowNames"),
				TEXT("Optional row filter for get_data_table_rows."), TEXT("string"))
			.FreeformObject(TEXT("fields"),
				TEXT("Field values (row or DataAsset)."))
			.String(TEXT("dataAssetClassPath"),
				TEXT("UDataAsset BP or native class path for create_data_asset."))
			.String(TEXT("propertyPath"),
				TEXT("Dotted/indexed property path (e.g. \"Stats.Health\" or \"Effects.[0].Value\")."))
			.FreeformObject(TEXT("value"),
				TEXT("JSON value to set (any type; resolved via reflection)."))
			.String(TEXT("classPath"),
				TEXT("Class to filter by for list_data_assets_of_class."))
			.Array(TEXT("searchPaths"),
				TEXT("Optional /Game/ subpath roots for scoped search."), TEXT("string"))
			.String(TEXT("subAction"),
				TEXT("Internal routing hint populated by TS handler."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageData);
