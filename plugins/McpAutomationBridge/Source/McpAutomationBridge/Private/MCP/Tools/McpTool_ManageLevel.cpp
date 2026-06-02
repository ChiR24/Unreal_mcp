// McpTool_ManageLevel.cpp — manage_level tool definition (24 actions)

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
		return TEXT("Load/save levels, configure streaming, and build lighting.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("load"),
				TEXT("load_level"),
				TEXT("save"),
				TEXT("save_level"),
				TEXT("save_as"),
				TEXT("save_level_as"),
				TEXT("stream"),
				TEXT("unload"),
				TEXT("unload_level"),
				TEXT("create_level"),
				TEXT("create_light"),
				TEXT("build_lighting"),
				TEXT("set_metadata"),
				TEXT("export_level"),
				TEXT("import_level"),
				TEXT("list_levels"),
				TEXT("get_summary"),
				TEXT("delete"),
				TEXT("delete_level"),
				TEXT("validate_level"),
				TEXT("add_sublevel"),
				TEXT("rename_level"),
				TEXT("duplicate_level"),
				TEXT("get_current_level")
			}, TEXT("Action"))
			.String(TEXT("levelPath"), TEXT("Level asset path."))
			.String(TEXT("assetPath"), TEXT("Asset path for metadata or validation aliases."))
			.Array(TEXT("levelPaths"), TEXT(""))
			.String(TEXT("levelName"), TEXT(""))
			.String(TEXT("name"), TEXT("Name alias used by light and utility actions."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("savePath"), TEXT("Path to save the asset."))
			.String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
			.Bool(TEXT("overwrite"), TEXT("Allow replacing an existing destination level."))
			.String(TEXT("newName"), TEXT("New asset name for rename operations."))
			.String(TEXT("targetPath"), TEXT("Path to a directory."))
			.String(TEXT("exportPath"), TEXT("Export file path."))
			.String(TEXT("packagePath"), TEXT("Path to a directory."))
			.String(TEXT("sourcePath"), TEXT("Source path for import/move/copy."))
			.String(TEXT("sublevelPath"), TEXT("Level asset path."))
			.String(TEXT("subLevelPath"), TEXT("Level asset path alias for add_sublevel."))
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
			.String(TEXT("quality"), TEXT("Lighting build quality label."))
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
			.String(TEXT("template"), TEXT(""))
			.Bool(TEXT("useWorldPartition"), TEXT(""))
			.FreeformObject(TEXT("metadata"), TEXT("Metadata key/value object."))
			.Number(TEXT("timeoutMs"), TEXT(""))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageLevel);
