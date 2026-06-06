// McpTool_ManageWidgetAuthoring.cpp — manage_widget_authoring tool definition (64 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageWidgetAuthoring : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_widget_authoring"); }

	FString GetDescription() const override
	{
		return TEXT("Create UMG widgets: buttons, text, images, sliders. Configure "
			"layouts, bindings, animations. Build HUDs and menus.");
	}

	FString GetCategory() const override { return TEXT("core"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::WidgetAuthoring(),
				TEXT("The widget authoring action to perform."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("folder"), TEXT("Path to a directory."))
			.String(TEXT("widgetPath"), TEXT("Widget blueprint path."))
			.String(TEXT("slotName"), TEXT("Name of the slot."))
			.String(TEXT("parentSlot"), TEXT("Parent slot to add widget to."))
			.String(TEXT("parentClass"), TEXT("Path or name of the parent class."))
			.Object(TEXT("anchorMin"), TEXT("Minimum anchor point (0-1)."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")); })
			.Object(TEXT("anchorMax"), TEXT("Maximum anchor point (0-1)."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")); })
			.Object(TEXT("alignment"), TEXT("Widget alignment (0-1)."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")); })
			.Object(TEXT("position"), TEXT("Canvas slot position."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")); })
			.Object(TEXT("size"), TEXT("Canvas slot size."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")); })
			.Number(TEXT("zOrder"), TEXT("Z-order for canvas slot."))
			.Object(TEXT("translation"), TEXT("Render translation."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")); })
			.Object(TEXT("scale"), TEXT("Render scale."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")); })
			.Object(TEXT("shear"), TEXT("Render shear."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")); })
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
			.Object(TEXT("colorAndOpacity"),
				TEXT("Color and opacity (0-1 values)."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")) .Number(TEXT("b")).Number(TEXT("a")); })
			.Bool(TEXT("autoWrap"), TEXT("Enable text auto-wrap."))
			.String(TEXT("texturePath"), TEXT("Texture asset path."))
			.Object(TEXT("brushSize"), TEXT("Brush/image size."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("x")).Number(TEXT("y")); })
			.Bool(TEXT("isEnabled"), TEXT("Widget enabled state."))
			.Bool(TEXT("isChecked"), TEXT("Checkbox checked state."))
			.Number(TEXT("value"), TEXT("Slider/spinbox value."))
			.Number(TEXT("minValue"), TEXT("Minimum value."))
			.Number(TEXT("maxValue"), TEXT("Maximum value."))
			.Number(TEXT("stepSize"), TEXT("Value step size."))
			.Number(TEXT("delta"), TEXT("Spinbox increment."))
			.Number(TEXT("percent"), TEXT("Progress bar percentage (0-1)."))
			.Object(TEXT("fillColorAndOpacity"),
				TEXT("Fill color for progress bar."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")) .Number(TEXT("b")).Number(TEXT("a")); })
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
			.Object(TEXT("slotPadding"), TEXT("Padding between uniform grid slots."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("left")).Number(TEXT("top")) .Number(TEXT("right")).Number(TEXT("bottom")); })
			.Number(TEXT("minDesiredSlotWidth"), TEXT("Minimum slot width."))
			.Number(TEXT("minDesiredSlotHeight"), TEXT("Minimum slot height."))
			.Object(TEXT("innerSlotPadding"), TEXT("Inner wrap box slot padding."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("left")).Number(TEXT("top")) .Number(TEXT("right")).Number(TEXT("bottom")); })
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
			.Number(TEXT("userSpecifiedScale"),
				TEXT("User specified scale value."))
			.Object(TEXT("brushColor"), TEXT("Border brush color."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("r")).Number(TEXT("g")) .Number(TEXT("b")).Number(TEXT("a")); })
			.Object(TEXT("padding"), TEXT("Widget slot padding."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("left")).Number(TEXT("top")) .Number(TEXT("right")).Number(TEXT("bottom")); })
			.String(TEXT("propertyName"), TEXT("Name of the property."))
			.String(TEXT("bindingSource"),
				TEXT("Variable or function name to bind to."))
			.String(TEXT("functionName"), TEXT("Name of the function."))
			.String(TEXT("onHoveredFunction"),
				TEXT("Function to call on hover."))
			.String(TEXT("onUnhoveredFunction"),
				TEXT("Function to call on unhover."))
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
			.Number(TEXT("loopCount"),
				TEXT("Number of loops (-1 for infinite)."))
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
			.String(TEXT("promptFormat"),
				TEXT("Interaction prompt format."))
			.Number(TEXT("maxVisibleObjectives"),
				TEXT("Maximum visible objectives."))
			.Number(TEXT("fadeTime"), TEXT("Fade time in seconds."))
			.Object(TEXT("gridSize"), TEXT("Inventory grid size."),
				[](FMcpSchemaBuilder& S) { S.Number(TEXT("columns")).Number(TEXT("rows")); })
			.Bool(TEXT("showSpeakerName"), TEXT("Show speaker name."))
			.Number(TEXT("segmentCount"),
				TEXT("Number of radial segments."))
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

MCP_REGISTER_TOOL(FMcpTool_ManageWidgetAuthoring);
