/**
 * Widget panel container records (11): add_canvas_panel, add_horizontal_box,
 * add_vertical_box, add_overlay, add_grid_panel, add_uniform_grid, add_wrap_box,
 * add_scroll_box, add_size_box, add_scale_box, add_border.
 *
 * Each adds a UMG panel container to a Widget Blueprint's WidgetTree. The
 * widget handle returned is `slotName` (the name of the child widget inside
 * its parent slot). Required: widgetPath.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
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
    whenToUse: [`A new ${action.replace(/^add_/, '').replace(/_/g, ' ')} must be added to a Widget Blueprint.`],
    whenNotToUse: ['A content widget (button, text, etc.) is needed instead.'],
    inputProps: { widgetPath: P.widgetPath, slotName: P.slotName, parentSlot: P.parentSlot, positionX: P.positionX, positionY: P.positionY, sizeX: P.sizeX, sizeY: P.sizeY, ...extraProps },
    required: ['widgetPath'],
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
  panel('add_grid_panel', 'blueprint.add_grid_panel', 'Add a Grid Panel to a Widget Blueprint for row/column-based child layout.', { columnCount: { type: 'number', description: 'Columns given an equal share of the width (0-64).' }, rowCount: { type: 'number', description: 'Rows given an equal share of the height (0-64).' } }),
  panel('add_uniform_grid', 'blueprint.add_uniform_grid', 'Add a Uniform Grid Panel to a Widget Blueprint for equal-cell child layout.', { slotPadding: P.slotPadding, minDesiredSlotWidth: P.minDesiredSlotWidth, minDesiredSlotHeight: P.minDesiredSlotHeight }),
  panel('add_wrap_box', 'blueprint.add_wrap_box', 'Add a Wrap Box to a Widget Blueprint for auto-wrapping child layout.', { wrapWidth: P.wrapWidth, explicitWrapWidth: P.explicitWrapWidth, innerSlotPadding: P.innerSlotPadding }),
  panel('add_scroll_box', 'blueprint.add_scroll_box', 'Add a Scroll Box to a Widget Blueprint for scrollable child layout.', { scrollBarVisibility: P.scrollBarVisibility, alwaysShowScrollbar: P.alwaysShowScrollbar, orientation: P.orientation }),
  panel('add_size_box', 'blueprint.add_size_box', 'Add a Size Box to a Widget Blueprint for explicit child size override.', { widthOverride: P.widthOverride, heightOverride: P.heightOverride, minDesiredWidth: P.minDesiredWidth, minDesiredHeight: P.minDesiredHeight, maxDesiredWidth: P.maxDesiredWidth, maxDesiredHeight: P.maxDesiredHeight }),
  panel('add_scale_box', 'blueprint.add_scale_box', 'Add a Scale Box to a Widget Blueprint for scalable child content.', { stretch: P.stretch, stretchDirection: P.stretchDirection, userSpecifiedScale: P.userSpecifiedScale }),
  panel('add_border', 'blueprint.add_border', 'Add a Border widget to a Widget Blueprint for framed/decorative child layout.', { brushColor: P.brushColor, padding: P.padding, contentColorAndOpacity: P.contentColorAndOpacity }),
  buildRecord({
    id: 'blueprint.add_spacer',
    action: 'add_spacer',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add a Spacer to a Widget Blueprint to reserve fixed empty space inside a layout.',
    whenToUse: ['Fixed empty space must separate sibling widgets in a box or panel.'],
    whenNotToUse: ['The gap should belong to an existing widget (set its padding instead).'],
    inputProps: { widgetPath: P.widgetPath, slotName: P.slotName, parentSlot: P.parentSlot, positionX: P.positionX, positionY: P.positionY, sizeX: P.sizeX, sizeY: P.sizeY },
    required: ['widgetPath'],
    outputProps: { widgetPath: P.widgetPath, slotName: P.slotName, sizeX: P.sizeX, sizeY: P.sizeY },
    outputRequired: ['widgetPath', 'slotName', 'sizeX', 'sizeY'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'add_spacer', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'Spacer_0', sizeX: 100, sizeY: 24 },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'Spacer_0', sizeX: 100, sizeY: 24 },
  }),
  buildRecord({
    id: 'blueprint.add_safe_zone',
    action: 'add_safe_zone',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add a Safe Zone to a Widget Blueprint so child content respects the platform title-safe area.',
    whenToUse: ['HUD content must stay inside the title-safe area on console or TV output.'],
    whenNotToUse: ['The layout is desktop-only and no safe-area inset applies.'],
    inputProps: { widgetPath: P.widgetPath, slotName: P.slotName, parentSlot: P.parentSlot },
    required: ['widgetPath'],
    outputProps: { widgetPath: P.widgetPath, slotName: P.slotName },
    outputRequired: ['widgetPath', 'slotName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'add_safe_zone', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'SafeZone_0' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'SafeZone_0' },
  }),
  buildRecord({
    id: 'blueprint.add_widget_tree',
    action: 'add_widget_tree',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add a whole widget layout to a Widget Blueprint in one call: nested panels, borders, text, images and bars with their looks and slots.',
    whenToUse: [
      'A HUD, a panel of labels or any layout of several widgets must be built in one call instead of an add_* call per widget and a set_style call per look.',
      'Text needs its face, letter spacing, outline or shadow, or a border its rounded corners and outline, as it is created.',
    ],
    whenNotToUse: ['One widget must be added (use the matching widgetKind).', 'A widget already in the tree must change (use set_widget_layout).'],
    inputProps: {
      widgetPath: P.widgetPath,
      tree: {
        type: 'object',
        additionalProperties: true,
        'x-unreal-reflection-boundary': true,
        description: 'The layout as one nested node {type, name, children[], slot{}, ...props}. type: a UMG class short name (CanvasPanel, Overlay, VerticalBox, HorizontalBox, Border, SizeBox, ScaleBox, UniformGridPanel, Spacer, TextBlock, RichTextBlock, Image, ProgressBar, Button). name: the widget name, unique in the Widget Blueprint (each becomes a variable a graph can read). Props: text, fontSize, color [r,g,b,a] (text colour, border brush, image tint, bar fill), justify left|center|right, autoWrap, opacity, visibility, padding (number or [l,t,r,b]: a border\'s inner padding), radius (rounded border, image or button) with outlineColor and outlineWidth, imageSize [w,h], texture (an image\'s texture path), material (an Image\'s or Border\'s brush: a UI material or instance that draws the whole widget, such as a rounded panel with its rim and gradient), percent (progress bar), width, height, maxHeight (size box). Text blocks also take typeface (Bold, Regular, Light, Italic), fontFamily, letterSpacing, outline with outlineColor, and shadowOffset [x,y] with shadowColor. slot places the node in its parent: in a canvas anchors [minX,minY,maxX,maxY], alignment [x,y], position [x,y], size [w,h], autoSize, z; in a box padding, hAlign (left|center|right|fill), vAlign (top|center|bottom|fill) and fill (a share of the free space). A Widget Blueprint with no root yet gets a canvas, and a tree root with no slot (and no positionX/Y or sizeX/Y) fills it. An unknown field is refused, not ignored; at most 200 widgets.',
      },
      parentSlot: P.parentSlot,
      positionX: P.positionX,
      positionY: P.positionY,
      sizeX: P.sizeX,
      sizeY: P.sizeY,
    },
    required: ['widgetPath', 'tree'],
    outputProps: {
      widgetPath: P.widgetPath,
      slotName: P.slotName,
      widgets: { type: 'array', items: { type: 'string' }, description: 'Every widget the tree made, by name, parents first.' },
      widgetCount: { type: 'number', description: 'How many widgets the tree made.' },
      saved: { type: 'boolean', description: 'Whether the Widget Blueprint was saved.' },
    },
    outputRequired: ['widgetPath', 'slotName', 'widgets'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    topics: ['widget tree', 'whole hud layout', 'build ui in one call', 'hud panel with labels'],
    exampleInput: {
      action: 'add_widget_tree',
      widgetPath: '/Game/UI/WBP_HUD',
      tree: {
        type: 'Border', name: 'ScorePanel', color: [0, 0, 0, 0.55], radius: 12, padding: [18, 10, 18, 10],
        slot: { anchors: [0, 0, 0, 0], position: [32, 24], autoSize: true },
        children: [{
          type: 'VerticalBox', name: 'ScoreStack',
          children: [
            { type: 'TextBlock', name: 'ScoreLabel', text: 'SCORE', fontSize: 14, letterSpacing: 200, color: [0.4, 1, 0.9, 1] },
            { type: 'TextBlock', name: 'ScoreText', text: '0', fontSize: 36, typeface: 'Bold', outline: 2 },
          ],
        }],
      },
    },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_HUD', slotName: 'ScorePanel', widgets: ['ScorePanel', 'ScoreStack', 'ScoreLabel', 'ScoreText'], widgetCount: 4, saved: true },
  }),
  buildRecord({
    id: 'blueprint.add_widget_switcher',
    action: 'add_widget_switcher',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add a Widget Switcher to a Widget Blueprint to show one child at a time by index.',
    whenToUse: ['Several pages or tabs must occupy one region with only one visible at a time.'],
    whenNotToUse: ['All children should be visible together (use an overlay or a box).'],
    inputProps: { widgetPath: P.widgetPath, slotName: P.slotName, parentSlot: P.parentSlot, activeIndex: P.activeIndex },
    required: ['widgetPath'],
    outputProps: { widgetPath: P.widgetPath, slotName: P.slotName, activeIndex: P.activeIndex },
    outputRequired: ['widgetPath', 'slotName', 'activeIndex'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'add_widget_switcher', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'PageSwitcher', activeIndex: 0 },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'PageSwitcher', activeIndex: 0 },
  }),
];
