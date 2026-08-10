/**
 * Widget panel container records (11): add_canvas_panel, add_horizontal_box,
 * add_vertical_box, add_overlay, add_grid_panel, add_uniform_grid, add_wrap_box,
 * add_scroll_box, add_size_box, add_scale_box, add_border.
 *
 * Each adds a UMG panel container to a Widget Blueprint's WidgetTree. The
 * widget handle returned is `slotName` (the name of the child widget inside
 * its parent slot). Required: widgetPath.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildPromotedRecord, buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-panels';
const DOMAIN = 'widget';
const SLOT_OUT = { slotName: P.slotName };

function panel(action: string, id: string, summary: string, extraProps: Record<string, unknown> = {}): CapabilityRecordSource {
  return buildRecord({
    id,
    action,
    family: FAMILY,
    domain: DOMAIN,
    summary,
    whenToUse: [`A ${action.replace(/_/g, ' ')} panel must be added to a Widget Blueprint.`],
    whenNotToUse: ['A content widget (button, text, etc.) is needed instead.'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, parentSlot: P.parentSlot, ...extraProps },
    required: ['action', 'widgetPath'],
    outputProps: SLOT_OUT,
    outputRequired: ['slotName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action, widgetPath: '/Game/UI/WBP_MainUI', slotName: action.replace(/_/g, ' ').replace('add ', 'Panel_') },
    exampleOutput: { success: true, slotName: action.replace(/_/g, ' ').replace('add ', 'Panel_') },
  });
}

export const WIDGET_PANELS_RECORDS: readonly CapabilityRecordSource[] = [
  panel('add_canvas_panel', 'blueprint.add_canvas_panel', 'Add a Canvas Panel to a Widget Blueprint for absolute-positioned child layout.'),
  panel('add_horizontal_box', 'blueprint.add_horizontal_box', 'Add a Horizontal Box to a Widget Blueprint for left-to-right child layout.'),
  panel('add_vertical_box', 'blueprint.add_vertical_box', 'Add a Vertical Box to a Widget Blueprint for top-to-bottom child layout.'),
  panel('add_overlay', 'blueprint.add_overlay', 'Add an Overlay to a Widget Blueprint for stacked/z-order child layout.'),
  panel('add_grid_panel', 'blueprint.add_grid_panel', 'Add a Grid Panel to a Widget Blueprint for row/column-based child layout.'),
  panel('add_uniform_grid', 'blueprint.add_uniform_grid', 'Add a Uniform Grid Panel to a Widget Blueprint for equal-cell child layout.', { columnCount: P.columnCount, rowCount: P.rowCount, slotPadding: P.slotPadding, minDesiredSlotWidth: P.minDesiredSlotWidth, minDesiredSlotHeight: P.minDesiredSlotHeight }),
  panel('add_wrap_box', 'blueprint.add_wrap_box', 'Add a Wrap Box to a Widget Blueprint for auto-wrapping child layout.', { wrapWidth: P.wrapWidth, explicitWrapWidth: P.explicitWrapWidth, innerSlotPadding: P.innerSlotPadding }),
  panel('add_scroll_box', 'blueprint.add_scroll_box', 'Add a Scroll Box to a Widget Blueprint for scrollable child layout.', { scrollBarVisibility: P.scrollBarVisibility, alwaysShowScrollbar: P.alwaysShowScrollbar, orientation: P.orientation }),
  panel('add_size_box', 'blueprint.add_size_box', 'Add a Size Box to a Widget Blueprint for explicit child size override.', { widthOverride: P.widthOverride, heightOverride: P.heightOverride, minDesiredWidth: P.minDesiredWidth, minDesiredHeight: P.minDesiredHeight }),
  panel('add_scale_box', 'blueprint.add_scale_box', 'Add a Scale Box to a Widget Blueprint for scalable child content.', { stretch: P.stretch, stretchDirection: P.stretchDirection, userSpecifiedScale: P.userSpecifiedScale }),
  panel('add_border', 'blueprint.add_border', 'Add a Border widget to a Widget Blueprint for framed/decorative child layout.', { brushColor: P.brushColor, padding: P.padding }),
  buildPromotedRecord({
    id: 'blueprint.add_spacer',
    action: 'add_spacer',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add a Spacer to a Widget Blueprint to reserve fixed empty space inside a layout.',
    whenToUse: ['Fixed empty space must separate sibling widgets in a box or panel.'],
    whenNotToUse: ['The gap should belong to an existing widget (set its padding instead).'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, parentSlot: P.parentSlot, sizeX: P.sizeX, sizeY: P.sizeY },
    required: ['action', 'widgetPath'],
    outputProps: { widgetPath: P.widgetPath, slotName: P.slotName, sizeX: P.sizeX, sizeY: P.sizeY },
    outputRequired: ['widgetPath', 'slotName', 'sizeX', 'sizeY'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'add_spacer', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'Spacer_0', sizeX: 100, sizeY: 24 },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'Spacer_0', sizeX: 100, sizeY: 24 },
  }, 'Reserves fixed layout space rather than hosting children, so it stays distinct from the panel containers.'),
  buildPromotedRecord({
    id: 'blueprint.add_safe_zone',
    action: 'add_safe_zone',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add a Safe Zone to a Widget Blueprint so child content respects the platform title-safe area.',
    whenToUse: ['HUD content must stay inside the title-safe area on console or TV output.'],
    whenNotToUse: ['The layout is desktop-only and no safe-area inset applies.'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, parentSlot: P.parentSlot },
    required: ['action', 'widgetPath'],
    outputProps: { widgetPath: P.widgetPath, slotName: P.slotName },
    outputRequired: ['widgetPath', 'slotName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'add_safe_zone', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'SafeZone_0' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'SafeZone_0' },
  }, 'Applies a platform safe-area inset to its children; no other panel container performs that adjustment.'),
  buildPromotedRecord({
    id: 'blueprint.add_widget_switcher',
    action: 'add_widget_switcher',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add a Widget Switcher to a Widget Blueprint to show one child at a time by index.',
    whenToUse: ['Several pages or tabs must occupy one region with only one visible at a time.'],
    whenNotToUse: ['All children should be visible together (use an overlay or a box).'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, parentSlot: P.parentSlot, activeIndex: P.activeIndex },
    required: ['action', 'widgetPath'],
    outputProps: { widgetPath: P.widgetPath, slotName: P.slotName, activeIndex: P.activeIndex },
    outputRequired: ['widgetPath', 'slotName', 'activeIndex'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'add_widget_switcher', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'PageSwitcher', activeIndex: 0 },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'PageSwitcher', activeIndex: 0 },
  }, 'Shows exactly one child selected by index, which no other panel container does.'),
];
