/**
 * Widget game UI records (8): add_health_bar, add_ammo_counter, add_minimap,
 * add_crosshair, add_compass, add_interaction_prompt, add_objective_tracker,
 * add_damage_indicator.
 *
 * Each adds a specialized game UI widget to a Widget Blueprint. These are
 * higher-level composite widgets built on UMG primitives. Required: widgetPath.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildPromotedRecord, buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-game-ui';
const DOMAIN = 'widget';
const SLOT_OUT = { slotName: P.slotName };

function gameUi(action: string, id: string, summary: string, extraProps: Record<string, unknown> = {}): CapabilityRecordSource {
  return buildRecord({
    id,
    action,
    family: FAMILY,
    domain: DOMAIN,
    summary,
    whenToUse: [`A ${action.replace(/_/g, ' ')} must be added to a HUD or game UI Widget Blueprint.`],
    whenNotToUse: ['A generic content widget is sufficient (see widget-content family).'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, parentSlot: P.parentSlot, parentName: P.parentName, ...extraProps },
    required: ['action', 'widgetPath'],
    outputProps: SLOT_OUT,
    outputRequired: ['slotName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action, widgetPath: '/Game/UI/WBP_HUD', slotName: action.replace(/_/g, ' ').replace('add ', 'UI_') },
    exampleOutput: { success: true, slotName: action.replace(/_/g, ' ').replace('add ', 'UI_') },
  });
}

export const WIDGET_GAME_UI_RECORDS: readonly CapabilityRecordSource[] = [
  gameUi('add_health_bar', 'blueprint.add_health_bar', 'Add a health bar widget to a HUD Widget Blueprint.', { percent: P.percent, fillColorAndOpacity: P.fillColorAndOpacity, x: P.x, y: P.y }),
  gameUi('add_ammo_counter', 'blueprint.add_ammo_counter', 'Add an ammo counter widget to a HUD Widget Blueprint.', { text: P.text, fontSize: P.fontSize }),
  gameUi('add_minimap', 'blueprint.add_minimap', 'Add a minimap widget to a HUD Widget Blueprint.', { brushSize: P.brushSize }),
  gameUi('add_crosshair', 'blueprint.add_crosshair', 'Add a crosshair widget to a HUD Widget Blueprint.', { colorAndOpacity: P.colorAndOpacity }),
  gameUi('add_compass', 'blueprint.add_compass', 'Add a compass widget to a HUD Widget Blueprint.', { angle: P.angle }),
  gameUi('add_interaction_prompt', 'blueprint.add_interaction_prompt', 'Add an interaction prompt widget to a HUD Widget Blueprint.', { promptFormat: P.promptFormat, text: P.text }),
  gameUi('add_objective_tracker', 'blueprint.add_objective_tracker', 'Add an objective tracker widget to a HUD Widget Blueprint.', { maxVisibleObjectives: P.maxVisibleObjectives }),
  gameUi('add_damage_indicator', 'blueprint.add_damage_indicator', 'Add a damage indicator widget to a HUD Widget Blueprint.', { fadeTime: P.fadeTime, colorAndOpacity: P.colorAndOpacity }),
  buildPromotedRecord({
    id: 'blueprint.add_quest_tracker',
    action: 'add_quest_tracker',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Add a quest tracker widget to a HUD Widget Blueprint.',
    whenToUse: ['Active quest or mission state must be shown on the HUD.'],
    whenNotToUse: ['A generic objective list is enough (use add_objective_tracker).'],
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName },
    required: ['action', 'widgetPath'],
    outputProps: { widgetPath: P.widgetPath, slotName: P.slotName },
    outputRequired: ['widgetPath', 'slotName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'add_quest_tracker', widgetPath: '/Game/UI/WBP_HUD', slotName: 'QuestTracker' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_HUD', slotName: 'QuestTracker' },
  }, 'Builds the quest-specific tracker composite; add_objective_tracker targets the generic objective list.'),
  buildPromotedRecord({
    id: 'blueprint.create_credits_screen',
    action: 'create_credits_screen',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Create a credits screen Widget Blueprint pre-populated with a scrolling credits layout.',
    whenToUse: ['A credits screen must be scaffolded as a new Widget Blueprint asset.'],
    whenNotToUse: ['The credits widget already exists and only needs content edits.'],
    inputProps: { action: P.action, name: P.name, path: P.path, folder: P.folder },
    required: ['action'],
    outputProps: { widgetPath: P.widgetPath },
    outputRequired: ['widgetPath'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'create_credits_screen', name: 'WBP_Credits', path: '/Game/UI' },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_Credits' },
  }, 'Creates a new Widget Blueprint asset from a credits template rather than adding a widget to an existing one.'),
  buildPromotedRecord({
    id: 'blueprint.create_shop_ui',
    action: 'create_shop_ui',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Create a shop Widget Blueprint pre-populated with a configurable item grid.',
    whenToUse: ['A shop or store screen must be scaffolded as a new Widget Blueprint asset.'],
    whenNotToUse: ['Only the item grid of an existing shop widget needs changing.'],
    inputProps: { action: P.action, name: P.name, path: P.path, folder: P.folder, columns: P.columns },
    required: ['action'],
    outputProps: { widgetPath: P.widgetPath, columns: P.columns },
    outputRequired: ['widgetPath', 'columns'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action: 'create_shop_ui', name: 'WBP_Shop', path: '/Game/UI', columns: 4 },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_Shop', columns: 4 },
  }, 'Creates a new Widget Blueprint asset from a shop template rather than adding a widget to an existing one.'),
];
