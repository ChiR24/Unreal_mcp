// McpTool_Inspect.cpp — inspect tool definition (32 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_Inspect : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("inspect"); }

	FString GetDescription() const override
	{
		return TEXT("Inspect any UObject: read/write properties, list components, export snapshots, "
			"and query class info. Actions: inspect_cdo (Blueprint CDO properties + all components "
			"without spawning an actor; use blueprintPath, optional detailed/componentName/propertyNames), "
			"inspect_class (class metadata), inspect_object (world actor), get_property/set_property, "
			"get_components, list_objects, find_by_class, find_by_tag, runtime_report.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	// Pattern A: registered as "inspect" in O(1 map
	// Default GetDispatchAction() returns GetName() — correct

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("inspect_object"),
				TEXT("get_actor_details"),
				TEXT("get_blueprint_details"),
				TEXT("get_mesh_details"),
				TEXT("get_texture_details"),
				TEXT("get_material_details"),
				TEXT("get_level_details"),
				TEXT("get_component_details"),
				TEXT("set_property"),
				TEXT("get_property"),
				TEXT("get_components"),
				TEXT("get_component_property"),
				TEXT("set_component_property"),
				TEXT("inspect_class"),
				TEXT("inspect_cdo"),
				TEXT("runtime_report"),
				TEXT("pie_report"),
				TEXT("list_objects"),
				TEXT("get_metadata"),
				TEXT("add_tag"),
				TEXT("find_by_tag"),
				TEXT("create_snapshot"),
				TEXT("restore_snapshot"),
				TEXT("export"),
				TEXT("delete_object"),
				TEXT("find_by_class"),
				TEXT("get_bounding_box"),
				TEXT("get_project_settings"),
				TEXT("get_world_settings"),
				TEXT("get_viewport_info"),
				TEXT("get_selected_actors"),
				TEXT("get_scene_stats"),
				TEXT("get_performance_stats"),
				TEXT("get_memory_stats"),
				TEXT("get_editor_settings")
			}, TEXT("Action"))
			.String(TEXT("objectPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("propertyName"), TEXT("Name of the property."))
			.String(TEXT("propertyPath"), TEXT(""))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.String(TEXT("className"), TEXT(""))
			.String(TEXT("classPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("tag"), TEXT("Name of the tag."))
			.String(TEXT("filter"), TEXT(""))
			.String(TEXT("snapshotName"), TEXT(""))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.Bool(TEXT("detailed"), TEXT(""))
			.Array(TEXT("propertyNames"), TEXT(""))
			.Array(TEXT("componentNames"), TEXT("Component names to include detailed property readback for."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_Inspect);
