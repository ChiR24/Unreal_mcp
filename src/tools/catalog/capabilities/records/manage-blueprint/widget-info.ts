/**
 * Widget info record (1): get_widget_info.
 *
 * Reads the widget tree of a Widget Blueprint, returning the widget names,
 * animations, and class info. This is the read-only companion to the widget
 * creation and styling actions.
 *
 * Output shape matches the native handler
 * (WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringInfo.cpp):
 * it returns `widgetInfo` { widgetClass, parentClass?, slots[], animations[] }
 * plus AddVerification fields (assetPath/assetName/existsAfter/assetClass).
 * The handler never emits a `widgets` array.
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
      widgetInfo: {
        type: 'object',
        additionalProperties: false,
        properties: {
          widgetClass: { type: 'string', description: 'Widget Blueprint class name.' },
          parentClass: { type: 'string', description: 'Native parent class name (omitted when the Widget Blueprint has no parent class).' },
          slots: { type: 'array', items: { type: 'string' }, description: 'Names of every widget in the widget tree.' },
          animations: { type: 'array', items: { type: 'string' }, description: 'Names of the Widget Blueprint animations.' },
        },
        required: ['widgetClass', 'slots', 'animations'],
      },
      assetPath: { type: 'string', description: 'Asset path of the inspected Widget Blueprint (verification).' },
      assetName: { type: 'string', description: 'Asset name of the inspected Widget Blueprint (verification).' },
      existsAfter: { type: 'boolean', description: 'Whether the Widget Blueprint exists after inspection (verification).' },
      assetClass: { type: 'string', description: 'Class of the inspected Widget Blueprint (verification).' },
    },
    outputRequired: ['widgetInfo'],
    effect: 'read',
    latency: 'instant',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'get_widget_info', widgetPath: '/Game/UI/WBP_MainUI' },
    exampleOutput: { success: true, widgetInfo: { widgetClass: 'WBP_MainUI', parentClass: 'UserWidget', slots: ['CanvasPanel_0', 'TitleText'], animations: [] } },
  }),
];
