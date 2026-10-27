/**
 * Widget layout and styling records (11): set_anchor, set_alignment,
 * set_position, set_size, set_padding, set_z_order, set_render_transform,
 * set_visibility, set_style, set_clipping, and arrange_widgets (many widgets
 * placed at once by relations).
 *
 * Each modifies the layout slot or visual style of a widget identified by
 * widgetPath + slotName. These are distinct from the no-op native route
 * `apply_style_to_widget` (route disposition: remove), which returns success
 * without mutating the widget style at design time.
 */
import type { CapabilityRecordSource } from '../../model.js';
import type { JsonObject } from '../../model.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-layout';
const DOMAIN = 'widget';

// Every layout setter replies with the widget's layout read back after the write.
export const LAYOUT_OUT = {
  slotName: P.slotName,
  applied: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'The widget as it is now: visibility, renderOpacity, renderTransform, slotClass, and the canvas anchors, alignment, position, size, zOrder and autoSize or the box padding and horizontal and vertical alignment.' },
  saved: { type: 'boolean', description: 'Whether the Widget Blueprint was saved.' },
  appliedFields: { type: 'array', items: { type: 'string' }, description: 'set_style: the style fields that were written (text, fontSize, colorAndOpacity, cornerRadius...).' },
};

/**
 * `required` and `example` share the key parameter K, so a property declared
 * required with no example value fails to compile. The example is what a client
 * copies; a required field missing from it is a broken call, not a shorthand.
 */
interface LayoutExtras<K extends string> {
  readonly props: Record<string, unknown>;
  readonly required: readonly K[];
  readonly example: Record<K, unknown> & JsonObject;
  /** Names a caller types for this edit when the action name lacks their words. */
  readonly aliases?: readonly string[];
}

function layout<K extends string>(action: string, id: string, summary: string, extras: LayoutExtras<K>): CapabilityRecordSource {
  return buildRecord({
    id,
    ...(extras.aliases === undefined ? {} : { aliases: extras.aliases }),
    action,
    family: FAMILY,
    domain: DOMAIN,
    summary,
    whenToUse: [`The ${action.replace(/_/g, ' ')} of a widget must be updated.`],
    whenNotToUse: ['The widget should be removed rather than restyled.'],
    inputProps: { widgetPath: P.widgetPath, slotName: P.slotName, ...extras.props },
    required: ['widgetPath', 'slotName', ...extras.required],
    outputProps: LAYOUT_OUT,
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    latency: 'instant',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'Widget_Button', ...extras.example },
    exampleOutput: { success: true, message: `${action} set` },
  });
}

// Canvas slot geometry is read as {x,y} objects and padding as an FMargin
// {left,top,right,bottom}; see WidgetAuthoringCanvasSlotGeometry/SlotAppearance.
export const WIDGET_LAYOUT_RECORDS: readonly CapabilityRecordSource[] = [
  layout('set_anchor', 'blueprint.set_anchor', 'Set the anchor points (min/max) for a widget in a canvas panel slot.',
    { props: { anchorMin: P.anchorMin, anchorMax: P.anchorMax, preset: P.preset }, required: [], example: { preset: 'StretchAll' } }),
  layout('set_alignment', 'blueprint.set_alignment', 'Set the alignment (0-1) for a widget in its slot.',
    { props: { alignment: P.alignment }, required: ['alignment'], example: { alignment: { x: 0.5, y: 0.5 } } }),
  layout('set_position', 'blueprint.set_position', 'Set the position offset for a widget in its slot.',
    { props: { position: P.position }, required: ['position'], example: { position: { x: 120, y: 64 } } }),
  layout('set_size', 'blueprint.set_size', 'Set the size of a widget in its slot: a size override {x,y} for a CanvasPanel child, or sizeRule (Auto or Fill) and fillValue (the Fill weight) for a HorizontalBox, VerticalBox or ScrollBox child.',
    { props: { size: P.size, sizeRule: P.sizeRule, fillValue: P.fillValue }, required: [], example: { size: { x: 240, y: 80 } } }),
  layout('set_padding', 'blueprint.set_padding', 'Set the padding for a widget in its slot.',
    { props: { padding: P.padding }, required: ['padding'], example: { padding: { left: 8, top: 4, right: 8, bottom: 4 } } }),
  layout('set_z_order', 'blueprint.set_z_order', 'Set the Z-order for a widget in a canvas panel slot.',
    { props: { zOrder: P.zOrder }, required: ['zOrder'], example: { zOrder: 10 } }),
  layout('set_render_transform', 'blueprint.set_render_transform', 'Set the render transform (translation, shear, angle) for a widget.',
    { props: { translation: P.translation, shear: P.shear, angle: P.angle, scale: P.scale }, required: [], example: {} }),
  layout('set_visibility', 'blueprint.set_visibility', 'Set the visibility mode (Visible, Collapsed, Hidden, etc.) for a widget.',
    { props: { visibility: P.visibility }, required: ['visibility'], example: { visibility: 'Visible' }, aliases: ['blueprint.hide_widget'] }),
  // text/texturePath/renderOpacity were implemented in the handler but never
  // declared here, so the gateway rejected them as undeclared and there was no
  // published way to change an existing widget's label or icon at all.
  layout('set_style', 'blueprint.set_style', 'Set the visual style (color, font size, face and letter spacing, text, text justification, brush texture, button hover/press sounds) for a widget, or copy the look of another text block with copyStyleFrom.',
    { props: { colorAndOpacity: P.colorAndOpacity, fontSize: P.fontSize, text: P.text, texturePath: P.texturePath, renderOpacity: P.renderOpacity, propertyName: { type: 'string', description: 'Any other widget property, by reflection (such as WidgetStyle or Brush): read without value, written with it. A dotted path reaches into a struct: Font.OutlineSettings.OutlineSize (a number) outlines the text of a TextBlock and Font.OutlineSettings.OutlineColor colors that outline; ShadowOffset ({X, Y}) with ShadowColorAndOpacity gives it a drop shadow.' }, value: { description: 'New value for propertyName: a string, number or bool, or an object or array for a struct property.' }, cornerRadius: P.cornerRadius, outlineColor: P.outlineColor, outlineWidth: P.outlineWidth, hoverSoundPath: P.hoverSoundPath, pressSoundPath: P.pressSoundPath, justification: P.justification, fontFamily: P.fontFamily, typeface: P.typeface, letterSpacing: P.letterSpacing, copyStyleFrom: P.copyStyleFrom }, required: [], example: { cornerRadius: 18 } }),
  layout('set_clipping', 'blueprint.set_clipping', 'Set the clipping mode (Inherit, ClipToBounds, etc.) for a widget.',
    { props: { clipping: P.clipping }, required: ['clipping'], example: { clipping: 'ClipToBounds' } }),
  buildRecord({
    id: 'blueprint.arrange_widgets',
    action: 'arrange_widgets',
    family: FAMILY,
    domain: DOMAIN,
    topics: ['arrange widgets', 'lay out widgets', 'lay out a menu', 'layout by relations', 'auto layout ui', 'stack buttons under the title', 'space buttons evenly', 'buttons the same width', 'center the title', 'place widgets relative to each other', 'ui constraints'],
    summary: 'Place the children of a Canvas Panel by how they relate instead of by coordinates: one left of, right of, above or below another (gap: exactly that far apart, else at least 0), edges or centre lines lined up (alignLeft, alignRight, alignTop, alignBottom, alignCenterX, alignCenterY over widgets), the same width or height, a width or height, an inset from an edge of the panel, or centred in it. Each relation is required, or preferred with a strength (strong, medium, weak) and kept as far as the required ones allow (Cassowary); besides them every widget stays inside the panel and no narrower than its text (strong), and otherwise as large as it is and then where it is (weak): set in from an edge, it moves there rather than stretching. The boxes are solved at the reference resolution (default 1920x1080, the project\'s DPI scaling applied) and each widget\'s anchors follow what holds it (an inset from the right anchors it right, centred anchors it at the centre, insets from both sides stretch it, a widget tied to another follows that one), so the layout holds at other sizes; every relation is then measured again at each of resolutions (default 1920x1080, 1280x720 and 3440x1440) and the reply says where it holds. One undo step; compiled and saved; dryRun reports the boxes and anchors and changes nothing. A required relation that cannot hold with the required ones before it is UNSATISFIABLE (which one) and nothing changes; a widget not in a Canvas Panel, or in another one than the rest, is NOT_A_CANVAS.',
    whenToUse: ['Laying out a menu or HUD: a title centred near the top, buttons stacked under it with even gaps and the same width, a score 24 in from the top right corner, held there at every screen size.'],
    whenNotToUse: ['One widget moved to exact coordinates (set_widget_layout position).', 'Children of a Vertical or Horizontal Box, which lay themselves out (set_widget_layout padding).'],
    inputProps: {
      widgetPath: P.widgetPath,
      relations: { type: 'array', minItems: 1, maxItems: 64, description: 'How the widgets relate, each one an object; the widgets named must be children of one Canvas Panel.', items: { type: 'object', additionalProperties: false, required: ['type'], properties: {
        type: { type: 'string', enum: ['leftOf', 'rightOf', 'above', 'below', 'alignLeft', 'alignRight', 'alignTop', 'alignBottom', 'alignCenterX', 'alignCenterY', 'sameWidth', 'sameHeight', 'width', 'height', 'inset', 'center'] },
        widget: { type: 'string', description: 'The widget the relation is about (every type but the align and same ones).' },
        of: { type: 'string', description: 'leftOf, rightOf, above, below: the widget it is placed against (widget leftOf of).' },
        widgets: { type: 'array', items: { type: 'string' }, minItems: 2, description: 'align and same types: the widgets lined up or sized alike.' },
        gap: { type: 'number', description: 'leftOf, rightOf, above, below: exactly this far apart, in Slate units (left out: at least 0 apart).' },
        value: { type: 'number', minimum: 0, description: 'width, height: the size; inset: the distance from the edge; in Slate units.' },
        edge: { type: 'string', enum: ['left', 'right', 'top', 'bottom'], description: 'inset: the edge of the panel.' },
        axis: { type: 'string', enum: ['x', 'y', 'both'], description: 'center: across (x), up and down (y) or both (default both).' },
        strength: { type: 'string', enum: ['required', 'strong', 'medium', 'weak'], description: 'required (default) holds exactly or the call is refused; the others are kept as far as the required ones allow, stronger first.' },
      } } },
      resolution: { type: 'string', description: 'Screen size the boxes are solved at, "WxH" (default 1920x1080).' },
      resolutions: { type: 'array', items: { type: 'string' }, minItems: 1, maxItems: 6, description: 'Screen sizes the relations are checked at afterwards, each "WxH" up to 3840x2160 (default 1920x1080, 1280x720 and 3440x1440).' },
      dryRun: { type: 'boolean', description: 'Report the boxes and anchors without changing the Widget Blueprint.' },
    },
    required: ['widgetPath', 'relations'],
    outputProps: {
      widgetPath: P.widgetPath,
      widgets: { type: 'array', description: 'Each widget arranged.', items: { type: 'object', additionalProperties: false, properties: {
        name: { type: 'string' },
        box: { type: 'array', items: { type: 'number' }, description: '[left, top, width, height] in the panel at the reference resolution, Slate units.' },
        anchors: { type: 'string', description: 'What it holds on to, across then up and down: left, centre, right or stretch; top, middle, bottom or stretch.' },
      } } },
      relations: { type: 'array', description: 'Each relation as checked at the sizes.', items: { type: 'object', additionalProperties: false, properties: {
        relation: { type: 'string', description: 'The relation as written.' },
        metAt: { type: 'array', items: { type: 'string' }, description: 'The sizes it holds at (within 1 Slate unit).' },
        worstMiss: { type: 'number', description: 'Its largest miss over the sizes, in Slate units.' },
      } } },
      dryRun: { type: 'boolean' },
      saved: { type: 'boolean', description: 'Whether the Widget Blueprint was saved.' },
      undo: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'What editor undo takes back: {undoable, transactionScope, steps}; all the moves are one step. Absent under dryRun.' },
      auditCall: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'The audit_widget call that checks the result at the same sizes (cut-off text, overlaps, safe area, contrast).' },
    },
    outputRequired: ['widgetPath'],
    effect: 'write',
    latency: 'interactive',
    resources: 'medium',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'arrange_widgets', widgetPath: '/Game/UI/WBP_MainMenu', relations: [{ type: 'center', widget: 'Title', axis: 'x' }, { type: 'inset', widget: 'Title', edge: 'top', value: 80 }, { type: 'below', widget: 'PlayButton', of: 'Title', gap: 40 }, { type: 'alignCenterX', widgets: ['PlayButton', 'Title'] }] },
    exampleOutput: { success: true, message: 'Arranged 2 widget(s); 4 of 4 relation(s) hold at all 3 size(s).', widgetPath: '/Game/UI/WBP_MainMenu', widgets: [{ name: 'Title', box: [760, 80, 400, 60], anchors: 'centre top' }], dryRun: false },
  }),
];
