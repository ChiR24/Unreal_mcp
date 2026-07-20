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
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-layout';
const DOMAIN = 'widget';

function layout(action: string, id: string, summary: string, extraProps: Record<string, unknown>, extraRequired: readonly string[] = []): CapabilityRecordSource {
  return buildRecord({
    id,
    action,
    family: FAMILY,
    domain: DOMAIN,
    summary,
    whenToUse: [`The ${action.replace(/_/g, ' ')} of a widget must be updated.`],
    whenNotToUse: ['The widget should be removed rather than restyled.'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, ...extraProps },
    required: ['action', 'widgetPath', 'slotName', ...extraRequired],
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    latency: 'instant',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action, widgetPath: '/Game/UI/WBP_MainUI', slotName: 'Widget_Button' },
    exampleOutput: { success: true, message: `${action} set` },
  });
}

export const WIDGET_LAYOUT_RECORDS: readonly CapabilityRecordSource[] = [
  layout('set_anchor', 'blueprint.set_anchor', 'Set the anchor points (min/max) for a widget in a canvas panel slot.', { anchorMin: P.anchorMin, anchorMax: P.anchorMax, preset: P.preset }, ['anchorMin', 'anchorMax']),
  layout('set_alignment', 'blueprint.set_alignment', 'Set the alignment (0-1) for a widget in its slot.', { alignment: P.alignment }, ['alignment']),
  layout('set_position', 'blueprint.set_position', 'Set the position offset for a widget in its slot.', { position: P.position }, ['position']),
  layout('set_size', 'blueprint.set_size', 'Set the size override for a widget in its slot.', { size: P.size }, ['size']),
  layout('set_padding', 'blueprint.set_padding', 'Set the padding for a widget in its slot.', { padding: P.padding }, ['padding']),
  layout('set_z_order', 'blueprint.set_z_order', 'Set the Z-order for a widget in a canvas panel slot.', { zOrder: P.zOrder }, ['zOrder']),
  layout('set_render_transform', 'blueprint.set_render_transform', 'Set the render transform (translation, shear, angle) for a widget.', { translation: P.translation, shear: P.shear, angle: P.angle, scale: P.scale }),
  layout('set_visibility', 'blueprint.set_visibility', 'Set the visibility mode (Visible, Collapsed, Hidden, etc.) for a widget.', { visibility: P.visibility }, ['visibility']),
  layout('set_style', 'blueprint.set_style', 'Set the visual style (color, font, brush) for a widget.', { colorAndOpacity: P.colorAndOpacity, fontSize: P.fontSize, value: P.value }),
  layout('set_clipping', 'blueprint.set_clipping', 'Set the clipping mode (Inherit, ClipToBounds, etc.) for a widget.', { clipping: P.clipping }, ['clipping']),
];
