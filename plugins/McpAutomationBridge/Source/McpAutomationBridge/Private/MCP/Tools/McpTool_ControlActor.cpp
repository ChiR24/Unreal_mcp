// McpTool_ControlActor.cpp — control_actor tool definition (43 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ControlActor : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("control_actor"); }

	FString GetDescription() const override
	{
		return TEXT("Spawn actors, set transforms, enable physics, add components, "
			"manage tags, and attach actors.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("spawn"),
				TEXT("spawn_actor"),
				TEXT("spawn_blueprint"),
				TEXT("delete"),
				TEXT("destroy_actor"),
				TEXT("delete_by_tag"),
				TEXT("duplicate"),
				TEXT("apply_force"),
				TEXT("set_transform"),
				TEXT("teleport_actor"),
				TEXT("set_actor_location"),
				TEXT("set_actor_rotation"),
				TEXT("set_actor_scale"),
				TEXT("set_actor_transform"),
				TEXT("get_transform"),
				TEXT("get_actor_transform"),
				TEXT("set_visibility"),
				TEXT("set_actor_visible"),
				TEXT("add_component"),
				TEXT("remove_component"),
				TEXT("set_component_properties"),
				TEXT("set_component_property"),
				TEXT("get_component_property"),
				TEXT("get_components"),
				TEXT("get_actor_components"),
				TEXT("get_actor_bounds"),
				TEXT("add_tag"),
				TEXT("remove_tag"),
				TEXT("find_by_tag"),
				TEXT("find_actors_by_tag"),
				TEXT("find_by_name"),
				TEXT("find_actors_by_name"),
				TEXT("find_by_class"),
				TEXT("find_actors_by_class"),
				TEXT("list"),
				TEXT("set_blueprint_variables"),
				TEXT("create_snapshot"),
				TEXT("attach"),
				TEXT("attach_actor"),
				TEXT("detach"),
				TEXT("detach_actor"),
				TEXT("set_actor_collision"),
				TEXT("call_actor_function")
			}, TEXT("Action"))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("childActor"),
				TEXT("Name of the child actor (for attach/detach operations)."))
			.String(TEXT("parentActor"),
				TEXT("Name of the parent actor (for attach operations)."))
			.String(TEXT("classPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("force"), TEXT("3D vector."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("componentType"), TEXT(""))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.FreeformObject(TEXT("properties"), TEXT(""))
			.Bool(TEXT("visible"), TEXT("Whether the item/actor is visible."))
			.String(TEXT("newName"), TEXT("New name for renaming."))
			.String(TEXT("tag"), TEXT("Name of the tag."))
			.FreeformObject(TEXT("variables"), TEXT(""))
			.String(TEXT("snapshotName"), TEXT(""))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ControlActor);
