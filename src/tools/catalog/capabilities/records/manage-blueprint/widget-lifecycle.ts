/**
 * Widget lifecycle records: create_widget_blueprint, set_widget_parent_class,
 * preview_widget.
 *
 * create_widget_blueprint is the canonical Widget Blueprint creation action.
 * The native route `create_widget` (reachable via system_control) maps to it
 * as an alias (route disposition: map -> cap:manage_blueprint:create_widget_blueprint).
 * Widget handles: `widgetPath` (asset path) identifies the Widget Blueprint.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-lifecycle';
const DOMAIN = 'widget';

export const WIDGET_LIFECYCLE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.create_widget_blueprint',
    action: 'create_widget_blueprint',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Create a new Widget Blueprint (UMG) asset with an optional parent class.',
    whenToUse: ['A new UMG Widget Blueprint must be created.'],
    whenNotToUse: ['A non-widget Blueprint is needed (use create or create_blueprint).'],
    inputProps: { action: P.action, name: P.name, path: P.path, folder: P.folder, parentClass: P.parentClass },
    required: ['action', 'name'],
    outputProps: { widgetPath: P.widgetPath },
    outputRequired: ['widgetPath'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'create_widget_blueprint', name: 'WBP_MainUI', path: '/Game/UI', parentClass: '/Script/UMG.UserWidget' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI' },
    aliases: ['blueprint.create_widget'],
  }),
  buildRecord({
    id: 'blueprint.set_widget_parent_class',
    action: 'set_widget_parent_class',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Set the parent class of a Widget Blueprint.',
    whenToUse: ['A Widget Blueprint must inherit from a specific UserWidget subclass.'],
    whenNotToUse: ['The Widget Blueprint already has the correct parent.'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, parentClass: P.parentClass },
    required: ['action', 'widgetPath', 'parentClass'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'set_widget_parent_class', widgetPath: '/Game/UI/WBP_MainUI', parentClass: '/Game/Blueprints/WBP_BaseUI' },
    exampleOutput: { success: true, message: 'Widget parent class set' },
  }),
  buildRecord({
    id: 'blueprint.preview_widget',
    action: 'preview_widget',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Preview a Widget Blueprint at a specified resolution preset in the editor.',
    whenToUse: ['A Widget Blueprint must be visually previewed without running PIE.'],
    whenNotToUse: ['Full PIE testing is needed.'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, previewSize: P.previewSize, width: P.width, height: P.height, duration: P.duration },
    required: ['action', 'widgetPath'],
    // Native handler (WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringPreview.cpp)
    // calls MarkBlueprintAsStructurallyModified — a real mutation (package
    // revision increases) — so the effect is `write`, not `read`. It returns
    // only success/message/widgetPath: no preview artifact is produced, so the
    // output schema declares only the widgetPath handle; a genuine preview
    // image would require a C++-side change (MCPBB-045).
    outputProps: { widgetPath: P.widgetPath },
    outputRequired: ['widgetPath'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    latency: 'interactive',
    resources: 'medium',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'preview_widget', widgetPath: '/Game/UI/WBP_MainUI', previewSize: '1080p' },
    exampleOutput: { success: true, message: 'Widget blueprint marked for recompilation. Open in Widget Blueprint Editor to see preview.', widgetPath: '/Game/UI/WBP_MainUI' },
  }),
];
