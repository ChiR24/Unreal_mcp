// McpTool_ManageAsset.cpp — manage_asset tool definition (45 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageAsset : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_asset"); }

	FString GetDescription() const override
	{
		return TEXT("Create, import, duplicate, rename, delete assets. "
			"Edit Material graphs and instances. Analyze dependencies.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("list"),
				TEXT("import"),
				TEXT("duplicate"),
				TEXT("duplicate_asset"),
				TEXT("rename"),
				TEXT("rename_asset"),
				TEXT("move"),
				TEXT("move_asset"),
				TEXT("delete"),
				TEXT("delete_asset"),
				TEXT("delete_assets"),
				TEXT("create_folder"),
				TEXT("search_assets"),
				TEXT("get_dependencies"),
				TEXT("get_source_control_state"),
				TEXT("analyze_graph"),
				TEXT("get_asset_graph"),
				TEXT("create_thumbnail"),
				TEXT("set_tags"),
				TEXT("get_metadata"),
				TEXT("set_metadata"),
				TEXT("validate"),
				TEXT("fixup_redirectors"),
				TEXT("find_by_tag"),
				TEXT("generate_report"),
				TEXT("create_material"),
				TEXT("create_material_instance"),
				TEXT("create_render_target"),
				TEXT("generate_lods"),
				TEXT("add_material_parameter"),
				TEXT("list_instances"),
				TEXT("reset_instance_parameters"),
				TEXT("exists"),
				TEXT("get_material_stats"),
				TEXT("nanite_rebuild_mesh"),
				TEXT("bulk_rename"),
				TEXT("bulk_delete"),
				TEXT("source_control_checkout"),
				TEXT("source_control_submit"),
				TEXT("add_material_node"),
				TEXT("connect_material_pins"),
				TEXT("remove_material_node"),
				TEXT("break_material_connections"),
				TEXT("get_material_node_details"),
				TEXT("rebuild_material")
			}, TEXT("Action to perform"))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("directory"), TEXT("Path to a directory."))
			.Array(TEXT("classNames"), TEXT(""))
			.Array(TEXT("packagePaths"), TEXT(""))
			.Bool(TEXT("recursivePaths"), TEXT(""))
			.Bool(TEXT("recursiveClasses"), TEXT(""))
			.Number(TEXT("limit"), TEXT(""))
			.Number(TEXT("offset"), TEXT(""))
			.String(TEXT("sourcePath"), TEXT("Source path for import/move/copy."))
			.String(TEXT("destinationPath"), TEXT("Destination path for move/copy."))
			.Array(TEXT("assetPaths"), TEXT(""))
			.Number(TEXT("lodCount"), TEXT(""))
			.FreeformObject(TEXT("reductionSettings"), TEXT(""))
			.String(TEXT("nodeName"), TEXT("Name identifier."))
			.String(TEXT("eventName"), TEXT("Name of the event."))
			.String(TEXT("memberClass"), TEXT(""))
			.Number(TEXT("posX"), TEXT(""))
			.Number(TEXT("posY"), TEXT(""))
			.String(TEXT("newName"), TEXT("New name for renaming."))
			.Bool(TEXT("overwrite"), TEXT("Overwrite if the asset/file already exists."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Bool(TEXT("fixupRedirectors"), TEXT(""))
			.String(TEXT("directoryPath"), TEXT("Path to a directory."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Path to a directory."))
			.String(TEXT("parentMaterial"), TEXT("Material asset path."))
			.FreeformObject(TEXT("parameters"), TEXT(""))
			.Number(TEXT("width"), TEXT(""))
			.Number(TEXT("height"), TEXT(""))
			.String(TEXT("format"), TEXT(""))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.String(TEXT("tag"), TEXT("Name of the tag."))
			.FreeformObject(TEXT("metadata"), TEXT(""))
			.String(TEXT("graphName"), TEXT("Name of the graph."))
			.String(TEXT("nodeType"), TEXT(""))
			.String(TEXT("nodeId"), TEXT("ID of the node."))
			.String(TEXT("sourceNodeId"), TEXT("ID of the source node."))
			.String(TEXT("targetNodeId"), TEXT("ID of the target node."))
			.String(TEXT("inputName"), TEXT("Name of the pin."))
			.String(TEXT("fromNodeId"), TEXT("ID of the source node."))
			.String(TEXT("fromPin"), TEXT("Name of the source pin."))
			.String(TEXT("toNodeId"), TEXT("ID of the target node."))
			.String(TEXT("toPin"), TEXT("Name of the target pin."))
			.String(TEXT("parameterName"), TEXT("Name of the parameter."))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.Number(TEXT("x"), TEXT(""))
			.Number(TEXT("y"), TEXT(""))
			.String(TEXT("comment"), TEXT(""))
			.String(TEXT("parentNodeId"), TEXT("ID of the node."))
			.String(TEXT("childNodeId"), TEXT("ID of the node."))
			.Number(TEXT("maxDepth"), TEXT(""))
			.String(TEXT("prefix"), TEXT(""))
			.String(TEXT("suffix"), TEXT(""))
			.String(TEXT("searchText"), TEXT(""))
			.String(TEXT("replaceText"), TEXT(""))
			.Array(TEXT("paths"), TEXT(""))
			.String(TEXT("description"), TEXT(""))
			.Bool(TEXT("checkoutFiles"), TEXT(""))
			.Bool(TEXT("showConfirmation"), TEXT(""))
			.String(TEXT("pinName"), TEXT("Name of the pin."))
			.String(TEXT("desc"), TEXT(""))
			.String(TEXT("materialPath"), TEXT("Material asset path."))
			.String(TEXT("texturePath"), TEXT("Texture asset path."))
			.String(TEXT("expressionClass"), TEXT(""))
			.Number(TEXT("coordinateIndex"), TEXT(""))
			.String(TEXT("parameterType"), TEXT(""))
			.ArrayOfObjects(TEXT("nodes"), TEXT(""))
			.Array(TEXT("tags"), TEXT(""))
			.String(TEXT("folderPath"), TEXT("Path to a directory."))
			.String(TEXT("sourceNode"), TEXT("ID of the source node."))
			.String(TEXT("targetNode"), TEXT("ID of the target node."))
			.String(TEXT("outputPin"), TEXT("Name of the source pin."))
			.String(TEXT("inputPin"), TEXT("Name of the target pin."))
			.String(TEXT("type"), TEXT(""))
			.FreeformObject(TEXT("defaultValue"), TEXT("Generic value (any type)."))
			.String(TEXT("expressionIndex"), TEXT("ID of the node."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageAsset);
