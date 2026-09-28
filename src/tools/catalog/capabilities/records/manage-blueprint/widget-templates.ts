/**
 * Ready-made screens (10), folded into create_widget_template: create_main_menu,
 * create_pause_menu, create_settings_menu, create_loading_screen, create_hud_widget,
 * create_dialog_widget, create_inventory_ui, create_radial_menu,
 * create_credits_screen, create_shop_ui.
 *
 * Each creates a NEW Widget Blueprint (name + path) holding a styled screen built
 * from a spec (WidgetAuthoring/Templates/...Screens*.cpp). An existing asset is
 * refused (ALREADY_EXISTS) rather than rebuilt, and a bad knob is refused before
 * the asset is created.
 */
import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-templates';
const DOMAIN = 'widget';

const SCREEN_OUT = {
  widgetPath: P.widgetPath,
  widgetCount: { type: 'number', description: 'Widgets in the new tree.' },
  buttons: { type: 'array', items: { type: 'string' }, description: 'Every Button created; wire them with bind_widget on_clicked.' },
  animationName: { type: 'string', description: 'Animation created with the screen (the loading screen FadeIn, the HUD damage flash).' },
  compiled: { type: 'boolean', description: 'Whether the new Widget Blueprint compiled.' },
  saved: { type: 'boolean', description: 'Whether the new Widget Blueprint was saved.' },
};

function screen(action: string, summary: string, extraProps: Record<string, unknown>, example: JsonObject): CapabilityRecordSource {
  const name = `WBP_${action.replace('create_', '').split('_').map((part) => part.charAt(0).toUpperCase() + part.slice(1)).join('')}`;
  return buildRecord({
    id: `blueprint.${action}`,
    action,
    family: FAMILY,
    domain: DOMAIN,
    summary,
    whenToUse: [`A game needs a ${action.replace('create_', '').replace(/_/g, ' ')} and none exists yet.`],
    whenNotToUse: ['The Widget Blueprint already exists (edit it with the add_* and set_* widget actions).'],
    inputProps: { name: P.name, path: P.path, ...extraProps },
    required: ['name'],
    outputProps: SCREEN_OUT,
    outputRequired: ['widgetPath', 'widgetCount'],
    effect: 'write',
    latency: 'interactive',
    resources: 'medium',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action, name, path: '/Game/UI', ...example },
    exampleOutput: { success: true, widgetPath: `/Game/UI/${name}.${name}`, widgetCount: 12 },
  });
}

export const WIDGET_TEMPLATES_RECORDS: readonly CapabilityRecordSource[] = [
  screen('create_main_menu', 'Create a main menu Widget Blueprint: backdrop, title and a column of buttons (default Play, Settings, Quit).',
    { title: P.title, buttons: P.buttons }, { title: 'My Game', buttons: ['Play', 'Options', 'Quit'] }),
  screen('create_pause_menu', 'Create a pause menu Widget Blueprint: dimmed backdrop, PAUSED title and buttons (default Resume, Settings, Main Menu).',
    { title: P.title, buttons: P.buttons }, { buttons: ['Resume', 'Quit'] }),
  screen('create_settings_menu', 'Create a settings menu Widget Blueprint: graphics, audio and controls rows with real combo boxes, check boxes and sliders, plus Apply and Back.',
    { settingsType: P.settingsType }, { settingsType: 'audio' }),
  screen('create_loading_screen', 'Create a loading screen Widget Blueprint: backdrop, Loading text, optional progress bar and optional FadeIn animation.',
    { includeProgressBar: P.includeProgressBar, fadeTime: P.fadeTime }, { includeProgressBar: true, fadeTime: 0.5 }),
  screen('create_hud_widget', 'Create a HUD Widget Blueprint on a canvas holding ready-made pieces (default health bar, crosshair, ammo counter).',
    { elements: P.elements }, { elements: ['health_bar', 'crosshair'] }),
  screen('create_dialog_widget', 'Create a dialog box Widget Blueprint: speaker name, wrapped dialog line, response buttons and a continue hint.',
    { showSpeakerName: P.showSpeakerName, responseCount: P.responseCount }, { responseCount: 2 }),
  screen('create_inventory_ui', 'Create an inventory Widget Blueprint: titled panel with a grid of item slots (default 6 by 4).',
    { columns: P.columns, rows: P.rows }, { columns: 5, rows: 3 }),
  screen('create_radial_menu', 'Create a radial menu Widget Blueprint: a ring of round buttons around a centre label (default 8 segments).',
    { segmentCount: P.segmentCount }, { segmentCount: 6 }),
  screen('create_credits_screen', 'Create a credits Widget Blueprint: centred scrolling list of role and name pairs with a Back button.',
    { title: P.title, entries: P.entries }, { entries: [{ title: 'Design', name: 'Ada' }, { title: 'Music', name: 'Grace' }] }),
  screen('create_shop_ui', 'Create a shop Widget Blueprint: header with currency, a scrolling grid of item cards each with a Buy button, and Close.',
    { columns: P.columns, itemCount: P.itemCount }, { columns: 4, itemCount: 8 }),
];
