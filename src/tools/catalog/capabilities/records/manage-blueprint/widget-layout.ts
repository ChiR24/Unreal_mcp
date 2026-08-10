/**
 * Widget layout and styling records (10): set_anchor, set_alignment,
 * set_position, set_size, set_padding, set_z_order, set_render_transform,
 * set_visibility, set_style, set_clipping.
 *
 * Each modifies the layout slot or visual style of a widget identified by
 * widgetPath + slotName. These are distinct from the no-op native route
 * `apply_style_to_widget` (route disposition: remove), which returns success
 * without mutating the widget style at design time.
 */
import type { CapabilityRecordSource } from '../../index.js';
import type { JsonObject } from '../../model.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-layout';
const DOMAIN = 'widget';

/**
 * `required` and `example` share the key parameter K, so a property declared
 * required with no example value fails to compile. The example is what a client
 * copies; a required field missing from it is a broken call, not a shorthand.
 */
interface LayoutExtras<K extends string> {
  readonly props: Record<string, unknown>;
  readonly required: readonly K[];
  readonly example: Record<K, unknown> & JsonObject;
}

function layout<K extends string>(action: string, id: string, summary: string, extras: LayoutExtras<K>): CapabilityRecordSource {
  return buildRecord({
    id,
    action,
    family: FAMILY,
    domain: DOMAIN,
    summary,
    whenToUse: [`The ${action.replace(/_/g, ' ')} of a widget must be updated.`],
    whenNotToUse: ['The widget should be removed rather than restyled.'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, ...extras.props },
    required: ['action', 'widgetPath', 'slotName', ...extras.required],
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
    { props: { anchorMin: P.anchorMin, anchorMax: P.anchorMax, preset: P.preset }, required: ['anchorMin', 'anchorMax'], example: { anchorMin: { x: 0, y: 0 }, anchorMax: { x: 1, y: 1 } } }),
  layout('set_alignment', 'blueprint.set_alignment', 'Set the alignment (0-1) for a widget in its slot.',
    { props: { alignment: P.alignment }, required: ['alignment'], example: { alignment: { x: 0.5, y: 0.5 } } }),
  layout('set_position', 'blueprint.set_position', 'Set the position offset for a widget in its slot.',
    { props: { position: P.position }, required: ['position'], example: { position: { x: 120, y: 64 } } }),
  layout('set_size', 'blueprint.set_size', 'Set the size override for a widget in its slot.',
    { props: { size: P.size }, required: ['size'], example: { size: { x: 240, y: 80 } } }),
  layout('set_padding', 'blueprint.set_padding', 'Set the padding for a widget in its slot.',
    { props: { padding: P.padding }, required: ['padding'], example: { padding: { left: 8, top: 4, right: 8, bottom: 4 } } }),
  layout('set_z_order', 'blueprint.set_z_order', 'Set the Z-order for a widget in a canvas panel slot.',
    { props: { zOrder: P.zOrder }, required: ['zOrder'], example: { zOrder: 10 } }),
  layout('set_render_transform', 'blueprint.set_render_transform', 'Set the render transform (translation, shear, angle) for a widget.',
    { props: { translation: P.translation, shear: P.shear, angle: P.angle, scale: P.scale }, required: [], example: {} }),
  layout('set_visibility', 'blueprint.set_visibility', 'Set the visibility mode (Visible, Collapsed, Hidden, etc.) for a widget.',
    { props: { visibility: P.visibility }, required: ['visibility'], example: { visibility: 'Visible' } }),
  layout('set_style', 'blueprint.set_style', 'Set the visual style (color, font, brush) for a widget.',
    { props: { colorAndOpacity: P.colorAndOpacity, fontSize: P.fontSize, value: P.value }, required: [], example: {} }),
  layout('set_clipping', 'blueprint.set_clipping', 'Set the clipping mode (Inherit, ClipToBounds, etc.) for a widget.',
    { props: { clipping: P.clipping }, required: ['clipping'], example: { clipping: 'ClipToBounds' } }),
];
