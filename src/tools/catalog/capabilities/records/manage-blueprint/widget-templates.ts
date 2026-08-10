/**
 * Widget template records (8): create_main_menu, create_pause_menu,
 * create_settings_menu, create_loading_screen, create_hud_widget,
 * create_inventory_ui, create_dialog_widget, create_radial_menu.
 *
 * Two native handler shapes sit behind these eight records:
 * - create_settings_menu / create_loading_screen / create_inventory_ui /
 *   create_dialog_widget / create_radial_menu CREATE a new Widget Blueprint
 *   from name/path/folder and return its widgetPath.
 * - create_main_menu / create_pause_menu / create_hud_widget REBUILD the
 *   widget tree of an EXISTING Widget Blueprint. The native handler
 *   (WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringMenuTemplates.cpp)
 *   reads `widgetPath` and rejects the call with MISSING_PARAMETER when it is
 *   absent; it never reads name/path/folder.
 *
 * The widget handle returned is `widgetPath`.
 */
import type { CapabilityRecordSource, JsonObject } from '../../index.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-templates';
const DOMAIN = 'widget';
const PATH_OUT = { widgetPath: P.widgetPath };

function template(action: string, id: string, summary: string, extraProps: Record<string, unknown> = {}): CapabilityRecordSource {
  return buildRecord({
    id,
    action,
    family: FAMILY,
    domain: DOMAIN,
    summary,
    whenToUse: [`A ${action.replace(/_/g, ' ')} template Widget Blueprint must be created.`],
    whenNotToUse: ['A custom Widget Blueprint built from scratch is needed (use create_widget_blueprint).'],
    inputProps: { action: P.action, name: P.name, path: P.path, folder: P.folder, ...extraProps },
    required: ['action', 'name'],
    outputProps: PATH_OUT,
    outputRequired: ['widgetPath'],
    effect: 'write',
    latency: 'interactive',
    resources: 'medium',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action, name: `WBP_${action.replace(/create_/g, '').replace(/_/g, '')}`, path: '/Game/UI' },
    exampleOutput: { success: true, widgetPath: `/Game/UI/WBP_${action.replace(/create_/g, '').replace(/_/g, '')}` },
  });
}

interface MenuTemplateOptions {
  extraInput?: Record<string, unknown>;
  extraOutput?: Record<string, unknown>;
  exampleInput?: JsonObject;
  exampleOutput?: JsonObject;
}

/**
 * Menu-style templates whose native handler modifies an EXISTING Widget
 * Blueprint: `widgetPath` is handler-mandated (MISSING_PARAMETER when absent)
 * and name/path/folder are ignored.
 */
function menuTemplate(action: string, id: string, summary: string, opts: MenuTemplateOptions = {}): CapabilityRecordSource {
  return buildRecord({
    id,
    action,
    family: FAMILY,
    domain: DOMAIN,
    summary,
    whenToUse: [`A ${action.replace(/_/g, ' ')} template Widget Blueprint must be created.`],
    whenNotToUse: ['A custom Widget Blueprint built from scratch is needed (use create_widget_blueprint).'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, ...opts.extraInput },
    required: ['action', 'widgetPath'],
    outputProps: opts.extraOutput ? { widgetPath: P.widgetPath, ...opts.extraOutput } : PATH_OUT,
    outputRequired: ['widgetPath'],
    effect: 'write',
    latency: 'interactive',
    resources: 'medium',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action, widgetPath: `/Game/UI/WBP_${action.replace(/create_/g, '').replace(/_/g, '')}`, ...opts.exampleInput },
    exampleOutput: { success: true, widgetPath: `/Game/UI/WBP_${action.replace(/create_/g, '').replace(/_/g, '')}`, ...opts.exampleOutput },
  });
}

export const WIDGET_TEMPLATES_RECORDS: readonly CapabilityRecordSource[] = [
  menuTemplate('create_main_menu', 'blueprint.create_main_menu', 'Create a main menu Widget Blueprint from a standard template.', {
    extraInput: { title: P.title },
    extraOutput: { title: P.title },
    exampleInput: { title: 'Main Menu' },
    exampleOutput: { title: 'Main Menu' },
  }),
  menuTemplate('create_pause_menu', 'blueprint.create_pause_menu', 'Create a pause menu Widget Blueprint from a standard template.'),
  template('create_settings_menu', 'blueprint.create_settings_menu', 'Create a settings menu Widget Blueprint with video/audio/controls tabs.', { settingsType: P.settingsType }),
  template('create_loading_screen', 'blueprint.create_loading_screen', 'Create a loading screen Widget Blueprint with optional progress bar.', { includeProgressBar: P.includeProgressBar, fadeTime: P.fadeTime }),
  menuTemplate('create_hud_widget', 'blueprint.create_hud_widget', 'Create a HUD Widget Blueprint from a standard template.', {
    extraOutput: { note: { type: 'string', description: 'Guidance on follow-up HUD element actions (add_health_bar, add_crosshair, add_ammo_counter).' } },
    exampleOutput: { note: 'HUD canvas created. Use add_health_bar, add_crosshair, add_ammo_counter to add HUD elements.' },
  }),
  template('create_inventory_ui', 'blueprint.create_inventory_ui', 'Create an inventory UI Widget Blueprint with a grid layout.', { gridSize: P.gridSize }),
  template('create_dialog_widget', 'blueprint.create_dialog_widget', 'Create a dialog Widget Blueprint for NPC conversations.', { showSpeakerName: P.showSpeakerName }),
  template('create_radial_menu', 'blueprint.create_radial_menu', 'Create a radial menu Widget Blueprint for weapon/item selection.', { segmentCount: P.segmentCount }),
];