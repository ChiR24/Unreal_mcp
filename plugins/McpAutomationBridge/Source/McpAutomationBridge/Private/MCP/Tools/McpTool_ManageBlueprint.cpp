// McpTool_ManageBlueprint.cpp — canonical manage_blueprint tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageBlueprint : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_blueprint"); }

	FString GetDescription() const override
	{
		return TEXT("Create Blueprints, add SCS components (mesh, collision, camera), "
			"and manipulate graph nodes.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
				.StringEnum(TEXT("action"), McpConsolidatedActions::ManageBlueprint(),
					TEXT("Blueprint action"))
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
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x"), TEXT("X coordinate."))
				 .Number(TEXT("y"), TEXT("Y coordinate."))
				 .Number(TEXT("z"), TEXT("Z coordinate."))
				 .Required({TEXT("x"), TEXT("y"), TEXT("z")});
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch"), TEXT("Pitch."))
				 .Number(TEXT("yaw"), TEXT("Yaw."))
				 .Number(TEXT("roll"), TEXT("Roll."))
				 .Required({TEXT("pitch"), TEXT("yaw"), TEXT("roll")});
			})
			.Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x"), TEXT("X scale."))
				 .Number(TEXT("y"), TEXT("Y scale."))
				 .Number(TEXT("z"), TEXT("Z scale."))
				 .Required({TEXT("x"), TEXT("y"), TEXT("z")});
			})
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
			.String(TEXT("functionName"), TEXT("Name of the function."))
			.ArrayOfObjects(TEXT("inputs"), TEXT(""))
			.ArrayOfObjects(TEXT("outputs"), TEXT(""))
			.Number(TEXT("posX"), TEXT(""))
			.Number(TEXT("posY"), TEXT(""))
			.String(TEXT("eventName"), TEXT("Name of the event."))
			.String(TEXT("parentComponent"), TEXT(""))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.String(TEXT("materialPath"), TEXT("Material asset path."))
			.Bool(TEXT("applyAndSave"), TEXT(""))
			.String(TEXT("memberClass"), TEXT(""))
			.String(TEXT("targetClass"), TEXT(""))
			.String(TEXT("inputAxisName"), TEXT(""))
			.Bool(TEXT("saveAfterCompile"), TEXT(""))
			.Number(TEXT("timeoutMs"), TEXT(""))
			.String(TEXT("parentClass"), TEXT("Path or name of the parent class."))
			.String(TEXT("fromNodeId"), TEXT("ID of the source node."))
			.String(TEXT("fromPinName"), TEXT("Name of the source pin."))
			.String(TEXT("toNodeId"), TEXT("ID of the target node."))
			.String(TEXT("toPinName"), TEXT("Name of the target pin."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageBlueprint);
