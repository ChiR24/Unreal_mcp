/**
 * Widget Authoring Handlers (Phase 19)
 *
 * Complete UMG widget authoring capabilities including:
 * - Widget Creation (blueprints, parent classes)
 * - Layout Panels (canvas, box, overlay, grid, scroll, etc.)
 * - Common Widgets (text, image, button, slider, progress, input, etc.)
 * - Layout & Styling (anchor, alignment, position, size, padding, style)
 * - Bindings & Events (property bindings, event handlers)
 * - Widget Animations (animation tracks, keyframes, playback)
 * - UI Templates (main menu, pause menu, HUD, inventory, etc.)
 * - Utility (info queries, preview)
 *
 * @module widget-authoring-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { requireNonEmptyString, executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Handles all widget authoring actions for the manage_widget_authoring tool.
 */
export async function handleWidgetAuthoringTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (subAction: string): Promise<Record<string, unknown>> => {
    const payload = { ...argsRecord, subAction };
    const result = await executeAutomationRequest(
      tools,
      'manage_widget_authoring',
      payload as HandlerArgs,
      `Automation bridge not available for widget authoring action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as Record<string, unknown>;
  };

  switch (action) {
    // =========================================================================
    // 19.1 Widget Creation (2 actions)
    // =========================================================================

    case 'create_widget_blueprint': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates UUserWidget blueprint
      // Optional: folder, parentClass (default: UserWidget)
      return sendRequest('create_widget_blueprint');
    }

    case 'set_widget_parent_class': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.parentClass, 'parentClass', 'Missing required parameter: parentClass');
      // Sets the parent class for the widget blueprint
      return sendRequest('set_widget_parent_class');
    }

    // =========================================================================
    // 19.2 Layout Panels (11 actions)
    // =========================================================================

    case 'add_canvas_panel': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UCanvasPanel to widget
      // Optional: slotName, parentSlot
      return sendRequest('add_canvas_panel');
    }

    case 'add_horizontal_box': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UHorizontalBox to widget
      // Optional: slotName, parentSlot
      return sendRequest('add_horizontal_box');
    }

    case 'add_vertical_box': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UVerticalBox to widget
      // Optional: slotName, parentSlot
      return sendRequest('add_vertical_box');
    }

    case 'add_overlay': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UOverlay to widget
      // Optional: slotName, parentSlot
      return sendRequest('add_overlay');
    }

    case 'add_grid_panel': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UGridPanel to widget
      // Optional: slotName, parentSlot, columnCount, rowCount
      return sendRequest('add_grid_panel');
    }

    case 'add_uniform_grid': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UUniformGridPanel to widget
      // Optional: slotName, parentSlot, slotPadding, minDesiredSlotWidth, minDesiredSlotHeight
      return sendRequest('add_uniform_grid');
    }

    case 'add_wrap_box': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UWrapBox to widget
      // Optional: slotName, parentSlot, innerSlotPadding, wrapWidth, explicitWrapWidth
      return sendRequest('add_wrap_box');
    }

    case 'add_scroll_box': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UScrollBox to widget
      // Optional: slotName, parentSlot, orientation, scrollBarVisibility, alwaysShowScrollbar
      return sendRequest('add_scroll_box');
    }

    case 'add_size_box': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds USizeBox to widget
      // Optional: slotName, parentSlot, widthOverride, heightOverride, minDesiredWidth, minDesiredHeight
      return sendRequest('add_size_box');
    }

    case 'add_scale_box': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UScaleBox to widget
      // Optional: slotName, parentSlot, stretch, stretchDirection, userSpecifiedScale
      return sendRequest('add_scale_box');
    }

    case 'add_border': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UBorder to widget
      // Optional: slotName, parentSlot, brushColor, padding, horizontalAlignment, verticalAlignment
      return sendRequest('add_border');
    }

    // =========================================================================
    // 19.3 Common Widgets (12 actions)
    // =========================================================================

    case 'add_text_block': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UTextBlock to widget
      // Optional: slotName, parentSlot, text, font, fontSize, colorAndOpacity, justification, autoWrap
      return sendRequest('add_text_block');
    }

    case 'add_rich_text_block': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds URichTextBlock to widget
      // Optional: slotName, parentSlot, text, textStyleSet, decoratorClasses
      return sendRequest('add_rich_text_block');
    }

    case 'add_image': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UImage to widget
      // Optional: slotName, parentSlot, texturePath, brushSize, colorAndOpacity, brushTiling
      return sendRequest('add_image');
    }

    case 'add_button': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UButton to widget
      // Optional: slotName, parentSlot, style, colorAndOpacity, isEnabled, clickMethod, touchMethod
      return sendRequest('add_button');
    }

    case 'add_check_box': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UCheckBox to widget
      // Optional: slotName, parentSlot, isChecked, checkedState, style
      return sendRequest('add_check_box');
    }

    case 'add_slider': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds USlider to widget
      // Optional: slotName, parentSlot, value, minValue, maxValue, stepSize, orientation, sliderBarColor, sliderHandleColor
      return sendRequest('add_slider');
    }

    case 'add_progress_bar': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UProgressBar to widget
      // Optional: slotName, parentSlot, percent, fillColorAndOpacity, barFillType, isMarquee
      return sendRequest('add_progress_bar');
    }

    case 'add_text_input': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UEditableTextBox or UMultiLineEditableText to widget
      // Optional: slotName, parentSlot, inputType (single, multi), hintText, isPassword, font, fontSize
      return sendRequest('add_text_input');
    }

    case 'add_combo_box': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UComboBoxString to widget
      // Optional: slotName, parentSlot, options (array), selectedOption, font
      return sendRequest('add_combo_box');
    }

    case 'add_spin_box': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds USpinBox to widget
      // Optional: slotName, parentSlot, value, minValue, maxValue, delta, minSliderValue, maxSliderValue
      return sendRequest('add_spin_box');
    }

    case 'add_list_view': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UListView to widget
      // Optional: slotName, parentSlot, entryWidgetClass, orientation, selectionMode
      return sendRequest('add_list_view');
    }

    case 'add_tree_view': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds UTreeView to widget
      // Optional: slotName, parentSlot, entryWidgetClass, selectionMode
      return sendRequest('add_tree_view');
    }

    // =========================================================================
    // 19.4 Layout & Styling (10 actions)
    // =========================================================================

    case 'set_anchor': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Sets anchor for a widget in canvas slot
      // Accepts: anchorMin (x, y), anchorMax (x, y), alignment (x, y)
      return sendRequest('set_anchor');
    }

    case 'set_alignment': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Sets alignment for a widget
      // Accepts: alignmentX, alignmentY (0-1 values)
      return sendRequest('set_alignment');
    }

    case 'set_position': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Sets position for a widget in canvas slot
      // Accepts: positionX, positionY
      return sendRequest('set_position');
    }

    case 'set_size': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Sets size for a widget
      // Accepts: sizeX, sizeY, sizeToContent
      return sendRequest('set_size');
    }

    case 'set_padding': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Sets padding for a widget slot
      // Accepts: left, top, right, bottom (or uniform padding)
      return sendRequest('set_padding');
    }

    case 'set_z_order': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Sets z-order for canvas panel slot
      // Accepts: zOrder
      return sendRequest('set_z_order');
    }

    case 'set_render_transform': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Sets render transform for a widget
      // Accepts: translation, scale, shear, angle, pivot
      return sendRequest('set_render_transform');
    }

    case 'set_visibility': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Sets visibility for a widget
      // Accepts: visibility (Visible, Collapsed, Hidden, HitTestInvisible, SelfHitTestInvisible)
      return sendRequest('set_visibility');
    }

    case 'set_style': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Sets style properties for a widget
      // Accepts: color, opacity, font, fontSize, brush, backgroundImage
      return sendRequest('set_style');
    }

    case 'set_clipping': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Sets clipping mode for a widget
      // Accepts: clipping (Inherit, ClipToBounds, ClipToBoundsWithoutIntersecting, ClipToBoundsAlways, OnDemand)
      return sendRequest('set_clipping');
    }

    // =========================================================================
    // 19.5 Bindings & Events (8 actions)
    // =========================================================================

    case 'create_property_binding': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      requireNonEmptyString(argsRecord.propertyName, 'propertyName', 'Missing required parameter: propertyName');
      // Creates a property binding function for the widget
      // Optional: bindingType (function, variable)
      return sendRequest('create_property_binding');
    }

    case 'bind_text': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Binds text property to a variable or function
      // Accepts: bindingSource (variable name or function name)
      return sendRequest('bind_text');
    }

    case 'bind_visibility': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Binds visibility to a variable or function
      // Accepts: bindingSource
      return sendRequest('bind_visibility');
    }

    case 'bind_color': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Binds color and opacity to a variable or function
      // Accepts: bindingSource
      return sendRequest('bind_color');
    }

    case 'bind_enabled': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Binds enabled state to a variable or function
      // Accepts: bindingSource
      return sendRequest('bind_enabled');
    }

    case 'bind_on_clicked': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Binds OnClicked event to a function
      // Accepts: functionName
      return sendRequest('bind_on_clicked');
    }

    case 'bind_on_hovered': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Binds OnHovered/OnUnhovered events to functions
      // Accepts: onHoveredFunction, onUnhoveredFunction
      return sendRequest('bind_on_hovered');
    }

    case 'bind_on_value_changed': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Binds OnValueChanged event (for sliders, checkboxes, etc.)
      // Accepts: functionName
      return sendRequest('bind_on_value_changed');
    }

    // =========================================================================
    // 19.6 Widget Animations (4 actions)
    // =========================================================================

    case 'create_widget_animation': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.animationName, 'animationName', 'Missing required parameter: animationName');
      // Creates a UWidgetAnimation in the widget blueprint
      // Optional: length (duration in seconds)
      return sendRequest('create_widget_animation');
    }

    case 'add_animation_track': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.animationName, 'animationName', 'Missing required parameter: animationName');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      requireNonEmptyString(argsRecord.trackType, 'trackType', 'Missing required parameter: trackType');
      // Adds an animation track to widget animation
      // trackType: transform, color, opacity, renderOpacity, material
      return sendRequest('add_animation_track');
    }

    case 'add_animation_keyframe': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.animationName, 'animationName', 'Missing required parameter: animationName');
      requireNonEmptyString(argsRecord.slotName, 'slotName', 'Missing required parameter: slotName');
      // Adds a keyframe to an animation track
      // Accepts: time, value (type depends on track), interpolation (linear, cubic, constant)
      return sendRequest('add_animation_keyframe');
    }

    case 'set_animation_loop': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      requireNonEmptyString(argsRecord.animationName, 'animationName', 'Missing required parameter: animationName');
      // Configures animation looping
      // Accepts: loopCount (-1 for infinite), playMode (forward, reverse, pingpong)
      return sendRequest('set_animation_loop');
    }

    // =========================================================================
    // 19.7 UI Templates (16 actions)
    // =========================================================================

    case 'create_main_menu': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates a main menu widget template
      // Optional: folder, includePlayButton, includeSettingsButton, includeQuitButton, backgroundImage, titleText
      return sendRequest('create_main_menu');
    }

    case 'create_pause_menu': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates a pause menu widget template
      // Optional: folder, includeResumeButton, includeSettingsButton, includeQuitToMenuButton
      return sendRequest('create_pause_menu');
    }

    case 'create_settings_menu': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates a settings menu widget template
      // Optional: folder, settingsType (video, audio, controls, gameplay, all), includeApplyButton, includeResetButton
      return sendRequest('create_settings_menu');
    }

    case 'create_loading_screen': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates a loading screen widget template
      // Optional: folder, includeProgressBar, includeTipText, includeBackgroundImage
      return sendRequest('create_loading_screen');
    }

    case 'create_hud_widget': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates a HUD widget template
      // Optional: folder, elements (array of: health_bar, ammo_counter, minimap, crosshair, compass, etc.)
      return sendRequest('create_hud_widget');
    }

    case 'add_health_bar': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds a health bar element to a HUD widget
      // Optional: slotName, parentSlot, style (simple, segmented, radial), showNumbers, barColor
      return sendRequest('add_health_bar');
    }

    case 'add_ammo_counter': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds an ammo counter element to a HUD widget
      // Optional: slotName, parentSlot, style (numeric, icon), showReserve, ammoIcon
      return sendRequest('add_ammo_counter');
    }

    case 'add_minimap': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds a minimap element to a HUD widget
      // Optional: slotName, parentSlot, size, shape (circle, square), rotateWithPlayer, showObjectives
      return sendRequest('add_minimap');
    }

    case 'add_crosshair': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds a crosshair element to a HUD widget
      // Optional: slotName, parentSlot, style (dot, cross, circle, custom), color, size, spreadMultiplier
      return sendRequest('add_crosshair');
    }

    case 'add_compass': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds a compass element to a HUD widget
      // Optional: slotName, parentSlot, showDegrees, showCardinals, showObjectives
      return sendRequest('add_compass');
    }

    case 'add_interaction_prompt': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds an interaction prompt element to a HUD widget
      // Optional: slotName, parentSlot, promptFormat, showKeyIcon, keyIconStyle
      return sendRequest('add_interaction_prompt');
    }

    case 'add_objective_tracker': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds an objective tracker element to a HUD widget
      // Optional: slotName, parentSlot, maxVisibleObjectives, showProgress, animateUpdates
      return sendRequest('add_objective_tracker');
    }

    case 'add_damage_indicator': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Adds a damage indicator element to a HUD widget
      // Optional: slotName, parentSlot, style (directional, vignette, both), fadeTime, color
      return sendRequest('add_damage_indicator');
    }

    case 'create_inventory_ui': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates an inventory UI widget template
      // Optional: folder, gridSize (columns, rows), slotSize, showEquipment, showDetails
      return sendRequest('create_inventory_ui');
    }

    case 'create_dialog_widget': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates a dialog/conversation widget template
      // Optional: folder, showPortrait, showSpeakerName, choiceLayout (vertical, horizontal, radial)
      return sendRequest('create_dialog_widget');
    }

    case 'create_radial_menu': {
      requireNonEmptyString(argsRecord.name, 'name', 'Missing required parameter: name');
      // Creates a radial menu widget template
      // Optional: folder, segmentCount, innerRadius, outerRadius, showIcons, showLabels
      return sendRequest('create_radial_menu');
    }

    // =========================================================================
    // 19.8 Utility (2 actions)
    // =========================================================================

    case 'get_widget_info': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Returns widget blueprint information
      // Optional: slotName (to get specific slot info)
      return sendRequest('get_widget_info');
    }

    case 'preview_widget': {
      requireNonEmptyString(argsRecord.widgetPath, 'widgetPath', 'Missing required parameter: widgetPath');
      // Opens widget in preview/designer mode
      // Optional: previewSize (1080p, 720p, mobile, custom), customWidth, customHeight
      return sendRequest('preview_widget');
    }

    // =========================================================================
    // Default / Unknown Action
    // =========================================================================

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown widget authoring action: ${action}`
      });
  }
}
