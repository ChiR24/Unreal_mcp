// McpTool_ManageLevel.cpp — manage_level tool definition (25 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageLevel : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_level"); }

	FString GetDescription() const override
	{
		return TEXT("Load/save levels, configure streaming, manage World Partition cells, "
			"and build lighting.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("load"),
				TEXT("save"),
				TEXT("save_as"),
				TEXT("save_level_as"),
				TEXT("stream"),
				TEXT("unload"),
				TEXT("create_level"),
				TEXT("create_light"),
				TEXT("build_lighting"),
				TEXT("set_metadata"),
				TEXT("load_cells"),
				TEXT("set_datalayer"),
				TEXT("create_datalayer"),
				TEXT("export_level"),
				TEXT("import_level"),
				TEXT("list_levels"),
				TEXT("get_summary"),
				TEXT("delete"),
				TEXT("delete_level"),
				TEXT("validate_level"),
				TEXT("cleanup_invalid_datalayers"),
				TEXT("add_sublevel"),
				TEXT("rename_level"),
				TEXT("duplicate_level"),
				TEXT("get_current_level")
			}, TEXT("Action"))
			.String(TEXT("levelPath"), TEXT("Level asset path."))
			.Array(TEXT("levelPaths"), TEXT(""))
			.String(TEXT("levelName"), TEXT(""))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("savePath"), TEXT("Path to save the asset."))
			.String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
			.String(TEXT("targetPath"), TEXT("Path to a directory."))
			.String(TEXT("exportPath"), TEXT("Export file path."))
			.String(TEXT("packagePath"), TEXT("Path to a directory."))
			.String(TEXT("sourcePath"), TEXT("Source path for import/move/copy."))
			.String(TEXT("sublevelPath"), TEXT("Level asset path."))
			.String(TEXT("parentLevel"), TEXT("Parent level path."))
			.String(TEXT("parentPath"), TEXT("Path to a directory."))
			.String(TEXT("streamingMethod"), TEXT(""))
			.Bool(TEXT("streaming"), TEXT(""))
			.Bool(TEXT("shouldBeLoaded"), TEXT(""))
			.Bool(TEXT("shouldBeVisible"), TEXT(""))
			.StringEnum(TEXT("lightType"), {
				TEXT("Directional"),
				TEXT("Point"),
				TEXT("Spot"),
				TEXT("Rect"),
				TEXT("DirectionalLight"),
				TEXT("PointLight"),
				TEXT("SpotLight"),
				TEXT("RectLight"),
				TEXT("directional"),
				TEXT("point"),
				TEXT("spot"),
				TEXT("rect")
			}, TEXT("Light type. Accepts short names (Point), class names "
				"(PointLight), or lowercase (point)."))
			.Number(TEXT("intensity"), TEXT(""))
			.Array(TEXT("color"), TEXT("RGBA color as an array [r, g, b, a]."),
				TEXT("number"))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Array(TEXT("cells"), TEXT(""))
			.String(TEXT("dataLayerName"), TEXT("Name of the data layer."))
			.String(TEXT("dataLayerLabel"), TEXT(""))
			.String(TEXT("dataLayerState"), TEXT(""))
			.String(TEXT("actorPath"), TEXT("Path to actor."))
			.Object(TEXT("min"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("max"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("origin"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("extent"), TEXT("3D extent (half-size)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("template"), TEXT(""))
			.Bool(TEXT("useWorldPartition"), TEXT(""))
			.FreeformObject(TEXT("metadata"), TEXT(""))
			.String(TEXT("newName"), TEXT(""))
			.Number(TEXT("timeoutMs"), TEXT(""))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageLevel);
