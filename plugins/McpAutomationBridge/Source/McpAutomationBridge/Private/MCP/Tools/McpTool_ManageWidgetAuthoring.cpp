// McpTool_ManageWidgetAuthoring.cpp — manage_widget_authoring tool definition (64 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageWidgetAuthoring : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_widget_authoring"); }

	FString GetDescription() const override
	{
		return TEXT("Create UMG widgets: buttons, text, images, sliders. Configure "
			"layouts, bindings, animations. Build HUDs and menus.");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_widget_blueprint"),
				TEXT("set_widget_parent_class"),
				TEXT("add_canvas_panel"),
				TEXT("add_horizontal_box"),
				TEXT("add_vertical_box"),
				TEXT("add_overlay"),
				TEXT("add_grid_panel"),
				TEXT("add_uniform_grid"),
				TEXT("add_wrap_box"),
				TEXT("add_scroll_box"),
				TEXT("add_size_box"),
				TEXT("add_scale_box"),
				TEXT("add_border"),
				TEXT("add_text_block"),
				TEXT("add_rich_text_block"),
				TEXT("add_image"),
				TEXT("add_button"),
				TEXT("add_check_box"),
				TEXT("add_slider"),
				TEXT("add_progress_bar"),
				TEXT("add_text_input"),
				TEXT("add_combo_box"),
				TEXT("add_spin_box"),
				TEXT("add_list_view"),
				TEXT("add_tree_view"),
				TEXT("set_anchor"),
				TEXT("set_alignment"),
				TEXT("set_position"),
				TEXT("set_size"),
				TEXT("set_padding"),
				TEXT("set_z_order"),
				TEXT("set_render_transform"),
				TEXT("set_visibility"),
				TEXT("set_style"),
				TEXT("set_clipping"),
				TEXT("create_property_binding"),
				TEXT("bind_text"),
				TEXT("bind_visibility"),
				TEXT("bind_color"),
				TEXT("bind_enabled"),
				TEXT("bind_on_clicked"),
				TEXT("bind_on_hovered"),
				TEXT("bind_on_value_changed"),
				TEXT("create_widget_animation"),
				TEXT("add_animation_track"),
				TEXT("add_animation_keyframe"),
				TEXT("set_animation_loop"),
				TEXT("create_main_menu"),
				TEXT("create_pause_menu"),
				TEXT("create_settings_menu"),
				TEXT("create_loading_screen"),
				TEXT("create_hud_widget"),
				TEXT("add_health_bar"),
				TEXT("add_ammo_counter"),
				TEXT("add_minimap"),
				TEXT("add_crosshair"),
				TEXT("add_compass"),
				TEXT("add_interaction_prompt"),
				TEXT("add_objective_tracker"),
				TEXT("add_damage_indicator"),
				TEXT("create_inventory_ui"),
				TEXT("create_dialog_widget"),
				TEXT("create_radial_menu"),
				TEXT("get_widget_info"),
				TEXT("preview_widget")
			}, TEXT("The widget authoring action to perform."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("folder"), TEXT("Path to a directory."))
			.String(TEXT("widgetPath"), TEXT("Widget blueprint path."))
			.String(TEXT("slotName"), TEXT("Name of the slot."))
			.String(TEXT("parentSlot"), TEXT("Parent slot to add widget to."))
			.String(TEXT("parentClass"), TEXT("Path or name of the parent class."))
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
			.Number(TEXT("alignmentX"), TEXT("Horizontal alignment (0-1)."))
			.Number(TEXT("alignmentY"), TEXT("Vertical alignment (0-1)."))
			.Number(TEXT("positionX"), TEXT("X position."))
			.Number(TEXT("positionY"), TEXT("Y position."))
			.Number(TEXT("sizeX"), TEXT("Width."))
			.Number(TEXT("sizeY"), TEXT("Height."))
			.Bool(TEXT("sizeToContent"), TEXT("Size to content."))
			.Number(TEXT("left"), TEXT("Left padding."))
			.Number(TEXT("top"), TEXT("Top padding."))
			.Number(TEXT("right"), TEXT("Right padding."))
			.Number(TEXT("bottom"), TEXT("Bottom padding."))
			.Number(TEXT("zOrder"), TEXT("Z-order for canvas slot."))
			.Object(TEXT("translation"), TEXT("Render translation."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Object(TEXT("scale"), TEXT("Render scale."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Object(TEXT("shear"), TEXT("Render shear."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Number(TEXT("angle"), TEXT("Angle in degrees."))
			.Object(TEXT("pivot"), TEXT("Rotation/scale pivot."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
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
			.String(TEXT("font"), TEXT("Font asset path."))
			.Number(TEXT("fontSize"), TEXT("Font size."))
			.Object(TEXT("colorAndOpacity"),
				TEXT("Color and opacity (0-1 values)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("r")).Number(TEXT("g"))
					.Number(TEXT("b")).Number(TEXT("a"));
			})
			.StringEnum(TEXT("justification"), {
				TEXT("Left"),
				TEXT("Center"),
				TEXT("Right")
			}, TEXT("Text justification."))
			.Bool(TEXT("autoWrap"), TEXT("Enable text auto-wrap."))
			.String(TEXT("texturePath"), TEXT("Texture asset path."))
			.Object(TEXT("brushSize"), TEXT("Brush/image size."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.StringEnum(TEXT("brushTiling"), {
				TEXT("NoTile"),
				TEXT("Horizontal"),
				TEXT("Vertical"),
				TEXT("Both")
			}, TEXT("Image tiling mode."))
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
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("r")).Number(TEXT("g"))
					.Number(TEXT("b")).Number(TEXT("a"));
			})
			.StringEnum(TEXT("barFillType"), {
				TEXT("LeftToRight"),
				TEXT("RightToLeft"),
				TEXT("TopToBottom"),
				TEXT("BottomToTop"),
				TEXT("FillFromCenter")
			}, TEXT("Progress bar fill direction."))
			.Bool(TEXT("isMarquee"), TEXT("Progress bar marquee mode."))
			.StringEnum(TEXT("inputType"), {
				TEXT("single"),
				TEXT("multi")
			}, TEXT("Text input type."))
			.String(TEXT("hintText"), TEXT("Placeholder hint text."))
			.Bool(TEXT("isPassword"), TEXT("Password masking."))
			.Array(TEXT("options"), TEXT("Combo box options."))
			.String(TEXT("selectedOption"), TEXT("Selected combo box option."))
			.String(TEXT("entryWidgetClass"),
				TEXT("List/tree view entry widget class."))
			.StringEnum(TEXT("orientation"), {
				TEXT("Horizontal"),
				TEXT("Vertical")
			}, TEXT("Widget orientation."))
			.StringEnum(TEXT("selectionMode"), {
				TEXT("None"),
				TEXT("Single"),
				TEXT("Multi")
			}, TEXT("Selection mode for list/tree."))
			.StringEnum(TEXT("scrollBarVisibility"), {
				TEXT("Visible"),
				TEXT("Collapsed"),
				TEXT("Auto")
			}, TEXT("Scroll bar visibility."))
			.Bool(TEXT("alwaysShowScrollbar"), TEXT("Always show scrollbar."))
			.Number(TEXT("columnCount"), TEXT("Number of columns."))
			.Number(TEXT("rowCount"), TEXT("Number of rows."))
			.Number(TEXT("slotPadding"), TEXT("Padding between slots."))
			.Number(TEXT("minDesiredSlotWidth"), TEXT("Minimum slot width."))
			.Number(TEXT("minDesiredSlotHeight"), TEXT("Minimum slot height."))
			.Number(TEXT("innerSlotPadding"), TEXT("Inner slot padding."))
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
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("r")).Number(TEXT("g"))
					.Number(TEXT("b")).Number(TEXT("a"));
			})
			.Number(TEXT("padding"), TEXT("Uniform padding."))
			.StringEnum(TEXT("horizontalAlignment"), {
				TEXT("Fill"),
				TEXT("Left"),
				TEXT("Center"),
				TEXT("Right")
			}, TEXT("Horizontal alignment."))
			.StringEnum(TEXT("verticalAlignment"), {
				TEXT("Fill"),
				TEXT("Top"),
				TEXT("Center"),
				TEXT("Bottom")
			}, TEXT("Vertical alignment."))
			.Object(TEXT("color"), TEXT("Widget color."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("r")).Number(TEXT("g"))
					.Number(TEXT("b")).Number(TEXT("a"));
			})
			.Number(TEXT("opacity"), TEXT("Widget opacity (0-1)."))
			.String(TEXT("brush"), TEXT("Brush asset path."))
			.String(TEXT("backgroundImage"), TEXT("Background image path."))
			.String(TEXT("style"), TEXT("Style preset name."))
			.String(TEXT("propertyName"), TEXT("Name of the property."))
			.StringEnum(TEXT("bindingType"), {
				TEXT("function"),
				TEXT("variable")
			}, TEXT("Binding type."))
			.String(TEXT("bindingSource"),
				TEXT("Variable or function name to bind to."))
			.String(TEXT("functionName"), TEXT("Name of the function."))
			.String(TEXT("onHoveredFunction"),
				TEXT("Function to call on hover."))
			.String(TEXT("onUnhoveredFunction"),
				TEXT("Function to call on unhover."))
			.String(TEXT("animationName"), TEXT("Animation name."))
			.Number(TEXT("length"), TEXT("Animation length in seconds."))
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
			.Bool(TEXT("includePlayButton"),
				TEXT("Include play button in menu."))
			.Bool(TEXT("includeSettingsButton"),
				TEXT("Include settings button."))
			.Bool(TEXT("includeQuitButton"), TEXT("Include quit button."))
			.Bool(TEXT("includeResumeButton"), TEXT("Include resume button."))
			.Bool(TEXT("includeQuitToMenuButton"),
				TEXT("Include quit to menu button."))
			.StringEnum(TEXT("settingsType"), {
				TEXT("video"),
				TEXT("audio"),
				TEXT("controls"),
				TEXT("gameplay"),
				TEXT("all")
			}, TEXT("Settings menu type."))
			.Bool(TEXT("includeApplyButton"), TEXT("Include apply button."))
			.Bool(TEXT("includeResetButton"), TEXT("Include reset button."))
			.Bool(TEXT("includeProgressBar"), TEXT("Include progress bar."))
			.Bool(TEXT("includeTipText"), TEXT("Include tip text."))
			.Bool(TEXT("includeBackgroundImage"),
				TEXT("Include background image."))
			.String(TEXT("titleText"), TEXT("Menu title text."))
			.Array(TEXT("elements"), TEXT("HUD elements to include."))
			.StringEnum(TEXT("barStyle"), {
				TEXT("simple"),
				TEXT("segmented"),
				TEXT("radial")
			}, TEXT("Health bar style."))
			.Bool(TEXT("showNumbers"), TEXT("Show numeric values."))
			.Object(TEXT("barColor"), TEXT("Bar color."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("r")).Number(TEXT("g"))
					.Number(TEXT("b")).Number(TEXT("a"));
			})
			.StringEnum(TEXT("ammoStyle"), {
				TEXT("numeric"),
				TEXT("icon")
			}, TEXT("Ammo counter style."))
			.Bool(TEXT("showReserve"), TEXT("Show reserve ammo."))
			.String(TEXT("ammoIcon"), TEXT("Ammo icon texture."))
			.Number(TEXT("minimapSize"), TEXT("Minimap size."))
			.StringEnum(TEXT("minimapShape"), {
				TEXT("circle"),
				TEXT("square")
			}, TEXT("Minimap shape."))
			.Bool(TEXT("rotateWithPlayer"),
				TEXT("Rotate minimap with player."))
			.Bool(TEXT("showObjectives"),
				TEXT("Show objectives on minimap."))
			.StringEnum(TEXT("crosshairStyle"), {
				TEXT("dot"),
				TEXT("cross"),
				TEXT("circle"),
				TEXT("custom")
			}, TEXT("Crosshair style."))
			.Number(TEXT("crosshairSize"), TEXT("Crosshair size."))
			.Number(TEXT("spreadMultiplier"),
				TEXT("Crosshair spread multiplier."))
			.Bool(TEXT("showDegrees"), TEXT("Show compass degrees."))
			.Bool(TEXT("showCardinals"), TEXT("Show cardinal directions."))
			.String(TEXT("promptFormat"),
				TEXT("Interaction prompt format."))
			.Bool(TEXT("showKeyIcon"), TEXT("Show key icon in prompt."))
			.String(TEXT("keyIconStyle"), TEXT("Key icon style."))
			.Number(TEXT("maxVisibleObjectives"),
				TEXT("Maximum visible objectives."))
			.Bool(TEXT("showProgress"), TEXT("Show objective progress."))
			.Bool(TEXT("animateUpdates"),
				TEXT("Animate objective updates."))
			.StringEnum(TEXT("indicatorStyle"), {
				TEXT("directional"),
				TEXT("vignette"),
				TEXT("both")
			}, TEXT("Damage indicator style."))
			.Number(TEXT("fadeTime"), TEXT("Fade time in seconds."))
			.Object(TEXT("gridSize"), TEXT("Inventory grid size."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("columns")).Number(TEXT("rows"));
			})
			.Number(TEXT("slotSize"), TEXT("Inventory slot size."))
			.Bool(TEXT("showEquipment"), TEXT("Show equipment panel."))
			.Bool(TEXT("showDetails"), TEXT("Show item details panel."))
			.Bool(TEXT("showPortrait"), TEXT("Show speaker portrait."))
			.Bool(TEXT("showSpeakerName"), TEXT("Show speaker name."))
			.StringEnum(TEXT("choiceLayout"), {
				TEXT("vertical"),
				TEXT("horizontal"),
				TEXT("radial")
			}, TEXT("Dialog choice layout."))
			.Number(TEXT("segmentCount"),
				TEXT("Number of radial segments."))
			.Number(TEXT("innerRadius"),
				TEXT("Inner radius of radial menu."))
			.Number(TEXT("outerRadius"),
				TEXT("Outer radius of radial menu."))
			.Bool(TEXT("showIcons"), TEXT("Show icons in radial menu."))
			.Bool(TEXT("showLabels"), TEXT("Show labels in radial menu."))
			.StringEnum(TEXT("previewSize"), {
				TEXT("1080p"),
				TEXT("720p"),
				TEXT("mobile"),
				TEXT("custom")
			}, TEXT("Preview resolution preset."))
			.Number(TEXT("customWidth"), TEXT("Custom preview width."))
			.Number(TEXT("customHeight"), TEXT("Custom preview height."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageWidgetAuthoring);
