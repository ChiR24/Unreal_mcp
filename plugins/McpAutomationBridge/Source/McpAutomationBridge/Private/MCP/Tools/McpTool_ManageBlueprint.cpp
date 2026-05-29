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
			.String(TEXT("path"), TEXT("Path to a directory or asset."))
			.String(TEXT("folder"), TEXT("Path to a directory."))
			.String(TEXT("widgetPath"), TEXT("Widget blueprint path."))
			.String(TEXT("slotName"), TEXT("Name of the widget slot."))
			.String(TEXT("parentSlot"), TEXT("Parent slot to add widget to."))
			.Object(TEXT("anchorMin"), TEXT("Minimum anchor point (0-1)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Object(TEXT("anchorMax"), TEXT("Maximum anchor point (0-1)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Object(TEXT("alignment"), TEXT("Widget alignment (0-1)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Object(TEXT("position"), TEXT("Widget position."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Object(TEXT("size"), TEXT("Widget size."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Number(TEXT("zOrder"), TEXT("Z-order for canvas slot."))
			.Object(TEXT("translation"), TEXT("Render translation."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Object(TEXT("shear"), TEXT("Render shear."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Number(TEXT("angle"), TEXT("Angle in degrees."))
			.StringEnum(TEXT("visibility"), {
				TEXT("Visible"),
				TEXT("Collapsed"),
				TEXT("Hidden"),
				TEXT("HitTestInvisible"),
				TEXT("SelfHitTestInvisible")
			}, TEXT("Widget visibility state."))
			.StringEnum(TEXT("clipping"), {
				TEXT("Inherit"),
				TEXT("ClipToBounds"),
				TEXT("ClipToBoundsWithoutIntersecting"),
				TEXT("ClipToBoundsAlways"),
				TEXT("OnDemand")
			}, TEXT("Widget clipping mode."))
			.String(TEXT("text"), TEXT("Text content."))
			.Number(TEXT("fontSize"), TEXT("Font size."))
			.Object(TEXT("colorAndOpacity"), TEXT("Color and opacity (0-1 values)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("r")).Number(TEXT("g"))
				 .Number(TEXT("b")).Number(TEXT("a"));
			})
			.Bool(TEXT("autoWrap"), TEXT("Enable text auto-wrap."))
			.String(TEXT("texturePath"), TEXT("Texture asset path."))
			.Object(TEXT("brushSize"), TEXT("Brush/image size."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Bool(TEXT("isEnabled"), TEXT("Widget enabled state."))
			.Bool(TEXT("isChecked"), TEXT("Checkbox checked state."))
			.Number(TEXT("minValue"), TEXT("Minimum value."))
			.Number(TEXT("maxValue"), TEXT("Maximum value."))
			.Number(TEXT("stepSize"), TEXT("Value step size."))
			.Number(TEXT("delta"), TEXT("Spinbox increment."))
			.Number(TEXT("percent"), TEXT("Progress bar percentage (0-1)."))
			.Object(TEXT("fillColorAndOpacity"), TEXT("Fill color for progress bar."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("r")).Number(TEXT("g"))
				 .Number(TEXT("b")).Number(TEXT("a"));
			})
			.Bool(TEXT("isMarquee"), TEXT("Progress bar marquee mode."))
			.StringEnum(TEXT("inputType"), {
				TEXT("single"),
				TEXT("multi")
			}, TEXT("Text input type."))
			.String(TEXT("hintText"), TEXT("Placeholder hint text."))
			.Array(TEXT("options"), TEXT("Combo box options."))
			.String(TEXT("selectedOption"), TEXT("Selected combo box option."))
			.StringEnum(TEXT("orientation"), {
				TEXT("Horizontal"),
				TEXT("Vertical")
			}, TEXT("Widget orientation."))
			.StringEnum(TEXT("scrollBarVisibility"), {
				TEXT("Visible"),
				TEXT("Collapsed"),
				TEXT("Auto")
			}, TEXT("Scroll bar visibility."))
			.Bool(TEXT("alwaysShowScrollbar"), TEXT("Always show scrollbar."))
			.Number(TEXT("columnCount"), TEXT("Number of columns."))
			.Number(TEXT("rowCount"), TEXT("Number of rows."))
			.FreeformObject(TEXT("slotPadding"), TEXT("Padding between uniform grid slots."))
			.Number(TEXT("minDesiredSlotWidth"), TEXT("Minimum slot width."))
			.Number(TEXT("minDesiredSlotHeight"), TEXT("Minimum slot height."))
			.FreeformObject(TEXT("innerSlotPadding"), TEXT("Inner wrap box slot padding."))
			.Number(TEXT("wrapWidth"), TEXT("Wrap width for wrap box."))
			.Bool(TEXT("explicitWrapWidth"), TEXT("Use explicit wrap width."))
			.Number(TEXT("widthOverride"), TEXT("Width override for size box."))
			.Number(TEXT("heightOverride"), TEXT("Height override for size box."))
			.Number(TEXT("minDesiredWidth"), TEXT("Minimum desired width."))
			.Number(TEXT("minDesiredHeight"), TEXT("Minimum desired height."))
			.StringEnum(TEXT("stretch"), {
				TEXT("None"),
				TEXT("Fill"),
				TEXT("ScaleToFit"),
				TEXT("ScaleToFitX"),
				TEXT("ScaleToFitY"),
				TEXT("ScaleToFill"),
				TEXT("UserSpecified")
			}, TEXT("Scale box stretch mode."))
			.StringEnum(TEXT("stretchDirection"), {
				TEXT("Both"),
				TEXT("DownOnly"),
				TEXT("UpOnly")
			}, TEXT("Scale box stretch direction."))
			.Number(TEXT("userSpecifiedScale"), TEXT("User specified scale value."))
			.FreeformObject(TEXT("brushColor"), TEXT("Border brush color."))
			.FreeformObject(TEXT("padding"), TEXT("Widget slot padding."))
			.String(TEXT("bindingSource"), TEXT("Variable or function name to bind to."))
			.String(TEXT("onHoveredFunction"), TEXT("Function to call on hover."))
			.String(TEXT("onUnhoveredFunction"), TEXT("Function to call on unhover."))
			.String(TEXT("animationName"), TEXT("Animation name."))
			.StringEnum(TEXT("trackType"), {
				TEXT("transform"),
				TEXT("color"),
				TEXT("opacity"),
				TEXT("renderOpacity"),
				TEXT("material")
			}, TEXT("Animation track type."))
			.Number(TEXT("time"), TEXT("Keyframe time."))
			.StringEnum(TEXT("interpolation"), {
				TEXT("linear"),
				TEXT("cubic"),
				TEXT("constant")
			}, TEXT("Keyframe interpolation."))
			.Number(TEXT("loopCount"), TEXT("Number of loops (-1 for infinite)."))
			.StringEnum(TEXT("playMode"), {
				TEXT("forward"),
				TEXT("reverse"),
				TEXT("pingpong")
			}, TEXT("Animation play mode."))
			.StringEnum(TEXT("settingsType"), {
				TEXT("video"),
				TEXT("audio"),
				TEXT("controls"),
				TEXT("gameplay"),
				TEXT("all")
			}, TEXT("Settings menu type."))
			.Bool(TEXT("includeProgressBar"), TEXT("Include progress bar."))
			.String(TEXT("promptFormat"), TEXT("Interaction prompt format."))
			.Number(TEXT("maxVisibleObjectives"), TEXT("Maximum visible objectives."))
			.Number(TEXT("fadeTime"), TEXT("Fade time in seconds."))
			.FreeformObject(TEXT("gridSize"), TEXT("Inventory grid size."))
			.Bool(TEXT("showSpeakerName"), TEXT("Show speaker name."))
			.Number(TEXT("segmentCount"), TEXT("Number of radial segments."))
			.StringEnum(TEXT("previewSize"), {
				TEXT("1080p"),
				TEXT("720p"),
				TEXT("mobile"),
				TEXT("custom")
			}, TEXT("Preview resolution preset."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageBlueprint);
