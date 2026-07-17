/**
 * Widget game UI records (8): add_health_bar, add_ammo_counter, add_minimap,
 * add_crosshair, add_compass, add_interaction_prompt, add_objective_tracker,
 * add_damage_indicator.
 *
 * Each adds a specialized game UI widget to a Widget Blueprint. These are
 * higher-level composite widgets built on UMG primitives. Required: widgetPath.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
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
    inputProps: { action: P.action, widgetPath: P.widgetPath, slotName: P.slotName, parentSlot: P.parentSlot, ...extraProps },
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
  gameUi('add_health_bar', 'blueprint.add_health_bar', 'Add a health bar widget to a HUD Widget Blueprint.', { percent: P.percent, fillColorAndOpacity: P.fillColorAndOpacity }),
  gameUi('add_ammo_counter', 'blueprint.add_ammo_counter', 'Add an ammo counter widget to a HUD Widget Blueprint.', { text: P.text, fontSize: P.fontSize }),
  gameUi('add_minimap', 'blueprint.add_minimap', 'Add a minimap widget to a HUD Widget Blueprint.', { brushSize: P.brushSize }),
  gameUi('add_crosshair', 'blueprint.add_crosshair', 'Add a crosshair widget to a HUD Widget Blueprint.', { colorAndOpacity: P.colorAndOpacity }),
  gameUi('add_compass', 'blueprint.add_compass', 'Add a compass widget to a HUD Widget Blueprint.', { angle: P.angle }),
  gameUi('add_interaction_prompt', 'blueprint.add_interaction_prompt', 'Add an interaction prompt widget to a HUD Widget Blueprint.', { promptFormat: P.promptFormat, text: P.text }),
  gameUi('add_objective_tracker', 'blueprint.add_objective_tracker', 'Add an objective tracker widget to a HUD Widget Blueprint.', { maxVisibleObjectives: P.maxVisibleObjectives }),
  gameUi('add_damage_indicator', 'blueprint.add_damage_indicator', 'Add a damage indicator widget to a HUD Widget Blueprint.', { fadeTime: P.fadeTime, colorAndOpacity: P.colorAndOpacity }),
];
