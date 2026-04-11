// McpTool_ManageBlueprint.cpp — manage_blueprint tool definition (35 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageBlueprint : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_blueprint"); }

	FString GetDescription() const override
	{
		return TEXT("Create Blueprints, add SCS components (mesh, collision, camera), "
			"and manipulate graph nodes.");
	}

	FString GetCategory() const override { return TEXT("authoring"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create"),
				TEXT("get_blueprint"),
				TEXT("get"),
				TEXT("compile"),
				TEXT("add_component"),
				TEXT("set_default"),
				TEXT("modify_scs"),
				TEXT("get_scs"),
				TEXT("add_scs_component"),
				TEXT("remove_scs_component"),
				TEXT("reparent_scs_component"),
				TEXT("set_scs_transform"),
				TEXT("set_scs_property"),
				TEXT("ensure_exists"),
				TEXT("probe_handle"),
				TEXT("add_variable"),
				TEXT("remove_variable"),
				TEXT("rename_variable"),
				TEXT("add_function"),
				TEXT("add_event"),
				TEXT("remove_event"),
				TEXT("add_construction_script"),
				TEXT("set_variable_metadata"),
				TEXT("set_metadata"),
				TEXT("create_node"),
				TEXT("add_node"),
				TEXT("delete_node"),
				TEXT("connect_pins"),
				TEXT("break_pin_links"),
				TEXT("set_node_property"),
				TEXT("create_reroute_node"),
				TEXT("get_node_details"),
				TEXT("get_graph_details"),
				TEXT("get_pin_details"),
				TEXT("list_node_types"),
				TEXT("set_pin_default_value")
			}, TEXT("Blueprint action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.String(TEXT("blueprintType"), TEXT("Path or name of the parent class."))
			.String(TEXT("savePath"), TEXT("Path to save the asset."))
			.String(TEXT("componentType"), TEXT(""))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.String(TEXT("componentClass"), TEXT(""))
			.String(TEXT("attachTo"), TEXT(""))
			.String(TEXT("newParent"), TEXT(""))
			.String(TEXT("propertyName"), TEXT("Name of the property."))
			.String(TEXT("variableName"), TEXT("Name of the variable."))
			.String(TEXT("oldName"), TEXT(""))
			.String(TEXT("newName"), TEXT("New name for renaming."))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.FreeformObject(TEXT("metadata"), TEXT(""))
			.FreeformObject(TEXT("properties"), TEXT(""))
			.String(TEXT("graphName"), TEXT("Name of the graph."))
			.String(TEXT("nodeType"), TEXT(""))
			.String(TEXT("nodeId"), TEXT("ID of the node."))
			.String(TEXT("pinName"), TEXT("Name of the pin."))
			.String(TEXT("linkedTo"), TEXT(""))
			.String(TEXT("memberName"), TEXT(""))
			.Number(TEXT("x"), TEXT(""))
			.Number(TEXT("y"), TEXT(""))
			.Array(TEXT("location"), TEXT(""), TEXT("number"))
			.Array(TEXT("rotation"), TEXT(""), TEXT("number"))
			.Array(TEXT("scale"), TEXT(""), TEXT("number"))
			.ArrayOfObjects(TEXT("operations"), TEXT(""))
			.Bool(TEXT("compile"), TEXT("Compile the blueprint(s) after the operation."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.String(TEXT("eventType"), TEXT(""))
			.String(TEXT("customEventName"), TEXT("Name of the event."))
			.ArrayOfObjects(TEXT("parameters"), TEXT(""))
			.String(TEXT("variableType"),
				TEXT("Variable type (e.g., Boolean, Float, Integer, Vector, String, Object)"))
			.FreeformObject(TEXT("defaultValue"), TEXT("Generic value (any type)."))
			.String(TEXT("category"), TEXT(""))
			.Bool(TEXT("isReplicated"), TEXT(""))
			.Bool(TEXT("isPublic"), TEXT(""))
			.FreeformObject(TEXT("variablePinType"), TEXT(""))
			.String(TEXT("functionName"), TEXT("Name of the function."))
			.ArrayOfObjects(TEXT("inputs"), TEXT(""))
			.ArrayOfObjects(TEXT("outputs"), TEXT(""))
			.Number(TEXT("posX"), TEXT(""))
			.Number(TEXT("posY"), TEXT(""))
			.String(TEXT("eventName"), TEXT("Name of the event."))
			.String(TEXT("parentComponent"), TEXT(""))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.String(TEXT("materialPath"), TEXT("Material asset path."))
			.FreeformObject(TEXT("transform"), TEXT(""))
			.Bool(TEXT("applyAndSave"), TEXT(""))
			.String(TEXT("scriptName"), TEXT(""))
			.String(TEXT("memberClass"), TEXT(""))
			.String(TEXT("targetClass"), TEXT(""))
			.String(TEXT("inputAxisName"), TEXT(""))
			.String(TEXT("inputPin"), TEXT("Name of the pin."))
			.String(TEXT("outputPin"), TEXT("Name of the pin."))
			.Bool(TEXT("saveAfterCompile"), TEXT(""))
			.Number(TEXT("timeoutMs"), TEXT(""))
			.Bool(TEXT("waitForCompletion"), TEXT(""))
			.Number(TEXT("waitForCompletionTimeoutMs"), TEXT(""))
			.String(TEXT("parentClass"), TEXT("Path or name of the parent class."))
			.String(TEXT("fromNodeId"), TEXT("ID of the source node."))
			.String(TEXT("fromPin"), TEXT("Name of the source pin."))
			.String(TEXT("fromPinName"), TEXT("Name of the source pin."))
			.String(TEXT("toNodeId"), TEXT("ID of the target node."))
			.String(TEXT("toPin"), TEXT("Name of the target pin."))
			.String(TEXT("toPinName"), TEXT("Name of the target pin."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageBlueprint);
