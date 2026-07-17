/**
 * Widget info record (1): get_widget_info.
 *
 * Reads the widget tree of a Widget Blueprint, returning slot names, types,
 * and hierarchy. This is the read-only companion to the widget creation and
 * styling actions.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

export const WIDGET_INFO_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.get_widget_info',
    action: 'get_widget_info',
    family: 'widget-info',
    domain: 'widget',
    summary: 'Read the widget tree of a Widget Blueprint, returning slot names, types, and hierarchy.',
    whenToUse: ['The widget tree of a Widget Blueprint must be inspected before modifying widgets.'],
    whenNotToUse: ['A single widget property is needed (use get or set_style).'],
    inputProps: { action: P.action, widgetPath: P.widgetPath },
    required: ['action', 'widgetPath'],
    outputProps: {
      widgets: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Widget descriptors with slotName, type, and parent.', 'x-unreal-reflection-boundary': true },
    },
    outputRequired: ['widgets'],
    effect: 'read',
    latency: 'instant',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'get_widget_info', widgetPath: '/Game/UI/WBP_MainUI' },
    exampleOutput: { success: true, widgets: [{ slotName: 'CanvasPanel_0', type: 'CanvasPanel' }] },
  }),
];
