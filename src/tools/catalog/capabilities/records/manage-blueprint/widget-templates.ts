/**
 * Widget template records (8): create_main_menu, create_pause_menu,
 * create_settings_menu, create_loading_screen, create_hud_widget,
 * create_inventory_ui, create_dialog_widget, create_radial_menu.
 *
 * Each creates a complete pre-built Widget Blueprint from a template. These
 * are composite widget blueprints, not individual widget additions. The
 * widget handle returned is `widgetPath`. Required: name.
 */
import type { CapabilityRecordSource } from '../../index.js';
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

export const WIDGET_TEMPLATES_RECORDS: readonly CapabilityRecordSource[] = [
  template('create_main_menu', 'blueprint.create_main_menu', 'Create a main menu Widget Blueprint from a standard template.'),
  template('create_pause_menu', 'blueprint.create_pause_menu', 'Create a pause menu Widget Blueprint from a standard template.'),
  template('create_settings_menu', 'blueprint.create_settings_menu', 'Create a settings menu Widget Blueprint with video/audio/controls tabs.', { settingsType: P.settingsType }),
  template('create_loading_screen', 'blueprint.create_loading_screen', 'Create a loading screen Widget Blueprint with optional progress bar.', { includeProgressBar: P.includeProgressBar, fadeTime: P.fadeTime }),
  template('create_hud_widget', 'blueprint.create_hud_widget', 'Create a HUD Widget Blueprint from a standard template.'),
  template('create_inventory_ui', 'blueprint.create_inventory_ui', 'Create an inventory UI Widget Blueprint with a grid layout.', { gridSize: P.gridSize }),
  template('create_dialog_widget', 'blueprint.create_dialog_widget', 'Create a dialog Widget Blueprint for NPC conversations.', { showSpeakerName: P.showSpeakerName }),
  template('create_radial_menu', 'blueprint.create_radial_menu', 'Create a radial menu Widget Blueprint for weapon/item selection.', { segmentCount: P.segmentCount }),
];
