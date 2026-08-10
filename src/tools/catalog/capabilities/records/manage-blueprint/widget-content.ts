/**
 * Widget content widget records (12): add_text_block, add_rich_text_block,
 * add_image, add_button, add_check_box, add_slider, add_progress_bar,
 * add_text_input, add_combo_box, add_spin_box, add_list_view, add_tree_view.
 *
 * Each adds a UMG content widget to a Widget Blueprint's WidgetTree. The
 * widget handle returned is `slotName`. Required: widgetPath.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildPromotedRecord, buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-content';
const DOMAIN = 'widget';
const SLOT_OUT = { slotName: P.slotName };

function content(action: string, id: string, summary: string, extraProps: Record<string, unknown> = {}, extraRequired: readonly string[] = []): CapabilityRecordSource {
  return buildRecord({
    id,
    action,
    family: FAMILY,
    domain: DOMAIN,
    summary,
    whenToUse: [`A ${action.replace(/_/g, ' ')} widget must be added to a Widget Blueprint.`],
    whenNotToUse: ['A panel container is needed instead (see widget-panels family).'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, parentSlot: P.parentSlot, ...extraProps },
    required: ['action', 'widgetPath', ...extraRequired],
    outputProps: SLOT_OUT,
    outputRequired: ['slotName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action, widgetPath: '/Game/UI/WBP_MainUI', slotName: action.replace(/_/g, ' ').replace('add ', 'Widget_') },
    exampleOutput: { success: true, slotName: action.replace(/_/g, ' ').replace('add ', 'Widget_') },
  });
}

export const WIDGET_CONTENT_RECORDS: readonly CapabilityRecordSource[] = [
  content('add_text_block', 'blueprint.add_text_block', 'Add a Text Block widget for displaying read-only text.', { text: P.text, fontSize: P.fontSize, colorAndOpacity: P.colorAndOpacity, autoWrap: P.autoWrap }),
  content('add_rich_text_block', 'blueprint.add_rich_text_block', 'Add a Rich Text Block widget for styled/markup text.', { text: P.text, fontSize: P.fontSize }),
  content('add_image', 'blueprint.add_image', 'Add an Image widget for displaying a texture or brush.', { texturePath: P.texturePath, brushSize: P.brushSize, colorAndOpacity: P.colorAndOpacity }),
  content('add_button', 'blueprint.add_button', 'Add a Button widget for click interaction.', { text: P.text, isEnabled: P.isEnabled }),
  content('add_check_box', 'blueprint.add_check_box', 'Add a Check Box widget for boolean toggle.', { isChecked: P.isChecked, isEnabled: P.isEnabled }),
  content('add_slider', 'blueprint.add_slider', 'Add a Slider widget for range selection.', { minValue: P.minValue, maxValue: P.maxValue, stepSize: P.stepSize, isEnabled: P.isEnabled, value: P.value }),
  content('add_progress_bar', 'blueprint.add_progress_bar', 'Add a Progress Bar widget for percentage display.', { percent: P.percent, fillColorAndOpacity: P.fillColorAndOpacity, isMarquee: P.isMarquee }),
  content('add_text_input', 'blueprint.add_text_input', 'Add a Text Input (editable text) widget for user text entry.', { inputType: P.inputType, hintText: P.hintText, isEnabled: P.isEnabled }),
  content('add_combo_box', 'blueprint.add_combo_box', 'Add a Combo Box (dropdown) widget for option selection.', { options: P.options, selectedOption: P.selectedOption, isEnabled: P.isEnabled }),
  content('add_spin_box', 'blueprint.add_spin_box', 'Add a Spin Box widget for numeric increment/decrement.', { minValue: P.minValue, maxValue: P.maxValue, delta: P.delta, stepSize: P.stepSize, value: P.value }),
  content('add_list_view', 'blueprint.add_list_view', 'Add a List View widget for scrollable entry lists.', { orientation: P.orientation, scrollBarVisibility: P.scrollBarVisibility }),
  content('add_tree_view', 'blueprint.add_tree_view', 'Add a Tree View widget for hierarchical entry display.', { orientation: P.orientation, scrollBarVisibility: P.scrollBarVisibility }),
  buildPromotedRecord({
    id: 'blueprint.add_widget_component',
    action: 'add_widget_component',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add a widget of any UMG class to a Widget Blueprint by class name.',
    whenToUse: ['A widget class is needed that no dedicated add_* action covers.'],
    whenNotToUse: ['A dedicated action exists for the class (prefer add_button, add_text_block, and the rest).'],
    inputProps: {
      action: P.action,
      widgetPath: P.widgetPath,
      componentType: P.componentType,
      componentName: P.componentName,
      parentName: P.parentName,
      positionX: P.positionX,
      positionY: P.positionY,
      sizeX: P.sizeX,
      sizeY: P.sizeY,
      text: P.text,
    },
    required: ['action', 'widgetPath', 'componentType'],
    outputProps: {
      widgetPath: P.widgetPath,
      componentName: P.componentName,
      componentType: { type: 'string', description: 'Resolved widget class name, which may differ from the requested componentType.' },
      parentName: P.parentName,
    },
    outputRequired: ['widgetPath', 'componentName', 'componentType', 'parentName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'add_widget_component', widgetPath: '/Game/UI/WBP_MainUI', componentType: 'TextBlock', componentName: 'Caption', text: 'Ready' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', componentName: 'Caption', componentType: 'TextBlock', parentName: 'CanvasPanel_0' },
  }, 'Resolves an arbitrary widget class by name, so it stays distinct from the per-class add actions.'),
  buildPromotedRecord({
    id: 'blueprint.set_font',
    action: 'set_font',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Set the font asset and size on a Text Block or Rich Text Block inside a Widget Blueprint.',
    whenToUse: ['A text widget needs a specific font face or size.'],
    whenNotToUse: ['The target is not a text widget; the call reports success false.'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, font: P.font, fontSize: P.fontSize },
    required: ['action', 'widgetPath', 'slotName'],
    outputProps: { widgetPath: P.widgetPath, slotName: P.slotName, fontSize: P.fontSize },
    outputRequired: ['widgetPath', 'slotName', 'fontSize'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'set_font', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'TitleText', fontSize: 32 },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'TitleText', fontSize: 32 },
  }, 'Targets the font face and size of a text widget, which the generic set_style path does not reach.'),
  buildPromotedRecord({
    id: 'blueprint.set_margin',
    action: 'set_margin',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Set the slot margin on a widget held by a Horizontal Box, Vertical Box, Overlay, or Border.',
    whenToUse: ['A widget needs spacing applied through its parent slot.'],
    whenNotToUse: ['The parent slot type carries no margin; the call reports success false.'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, left: P.left, top: P.top, right: P.right, bottom: P.bottom },
    required: ['action', 'widgetPath', 'slotName'],
    outputProps: { widgetPath: P.widgetPath, slotName: P.slotName, left: P.left, top: P.top, right: P.right, bottom: P.bottom },
    outputRequired: ['widgetPath', 'slotName', 'left', 'top', 'right', 'bottom'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'set_margin', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'TitleText', left: 8, top: 8, right: 8, bottom: 8 },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'TitleText', left: 8, top: 8, right: 8, bottom: 8 },
  }, 'Writes the four margin edges on the parent slot, unlike the padding set by the panel actions.'),
];
