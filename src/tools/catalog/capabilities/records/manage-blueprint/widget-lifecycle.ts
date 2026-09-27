/**
 * Widget lifecycle records: create_widget_blueprint, set_widget_parent_class,
 * preview_widget.
 *
 * create_widget_blueprint is the canonical Widget Blueprint creation action.
 * The native route `create_widget` (reachable via system_control) maps to it
 * as an alias (route disposition: map -> cap:manage_blueprint:create_widget_blueprint).
 * Widget handles: `widgetPath` (asset path) identifies the Widget Blueprint.
 */
import type { CapabilityRecordSource } from '../../model.js';
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
    topics: ['umg widget', 'user widget', 'ui widget', 'hud', 'new widget', 'menu widget'],
    summary: 'Create a new Widget Blueprint (UMG) asset with an optional parent class.',
    whenToUse: ['A new UMG Widget Blueprint must be created.'],
    whenNotToUse: ['A non-widget Blueprint is needed (use create or create_blueprint).'],
    inputProps: { name: P.name, path: P.path, folder: P.folder, parentClass: P.parentClass },
    required: ['name'],
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
    inputProps: { widgetPath: P.widgetPath, parentClass: P.parentClass },
    required: ['widgetPath', 'parentClass'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'set_widget_parent_class', widgetPath: '/Game/UI/WBP_MainUI', parentClass: '/Game/Blueprints/WBP_BaseUI' },
  }),
  buildRecord({
    id: 'blueprint.preview_widget',
    action: 'preview_widget',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Open a Widget Blueprint in its editor so the Designer tab shows the preview.',
    whenToUse: ['A Widget Blueprint must be visually previewed without running PIE.'],
    whenNotToUse: ['Full PIE testing is needed.'],
    inputProps: { widgetPath: P.widgetPath },
    required: ['widgetPath'],
    // Native handler (WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringPreview.cpp)
    // opens the asset editor and returns widgetPath + editorOpened; no preview
    // artifact is produced (a preview image would need a C++-side change, MCPBB-045).
    outputProps: { widgetPath: P.widgetPath },
    outputRequired: ['widgetPath'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    latency: 'interactive',
    resources: 'medium',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'preview_widget', widgetPath: '/Game/UI/WBP_MainUI' },
    exampleOutput: { success: true, message: 'Widget Blueprint opened in the Widget Blueprint Editor; the Designer tab shows the preview', widgetPath: '/Game/UI/WBP_MainUI' },
  }),
  buildRecord({
    id: 'blueprint.remove_widget',
    action: 'remove_widget',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Remove a widget from the widget tree of a Widget Blueprint.',
    whenToUse: ['A widget must be deleted from a Widget Blueprint layout.'],
    whenNotToUse: ['The widget should stay but be hidden (use set_visibility).'],
    inputProps: { widgetPath: P.widgetPath, slotName: P.slotName },
    required: ['widgetPath', 'slotName'],
    outputProps: {
      widgetPath: P.widgetPath,
      removedWidget: { type: 'string', description: 'Name of the widget that was removed from the widget tree.' },
    },
    outputRequired: ['widgetPath', 'removedWidget'],
    effect: 'destructive',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'remove_widget', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'TitleText' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', removedWidget: 'TitleText' },
  }),
  buildRecord({
    id: 'blueprint.rename_widget',
    action: 'rename_widget',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Rename a widget inside the widget tree of a Widget Blueprint.',
    whenToUse: ['A widget name must change so bindings and lookups can address it.'],
    whenNotToUse: ['The Widget Blueprint asset itself should be renamed (use manage_asset).'],
    inputProps: { widgetPath: P.widgetPath, slotName: P.slotName, newName: P.newName },
    required: ['widgetPath', 'slotName', 'newName'],
    outputProps: { widgetPath: P.widgetPath, oldName: P.oldName, newName: P.newName },
    outputRequired: ['widgetPath', 'oldName', 'newName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'rename_widget', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'TitleText', newName: 'HeaderText' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', oldName: 'TitleText', newName: 'HeaderText' },
  }),
  buildRecord({
    id: 'blueprint.reparent_widget',
    action: 'reparent_widget',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Move a widget under a different parent widget inside a Widget Blueprint.',
    whenToUse: ['An existing widget must move into another panel without being recreated.'],
    whenNotToUse: ['The widget is being added for the first time (use the matching add action).'],
    inputProps: { widgetPath: P.widgetPath, slotName: P.slotName, newParent: P.newParent },
    required: ['widgetPath', 'slotName', 'newParent'],
    outputProps: {
      widgetPath: P.widgetPath,
      widget: { type: 'string', description: 'Name of the widget that was reparented.' },
      newParent: P.newParent,
    },
    outputRequired: ['widgetPath', 'widget', 'newParent'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'reparent_widget', widgetPath: '/Game/UI/WBP_MainUI', slotName: 'TitleText', newParent: 'HeaderBox' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_MainUI', widget: 'TitleText', newParent: 'HeaderBox' },
  }),
];
