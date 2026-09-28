/**
 * Ready-made HUD pieces (9), folded into add_game_widget: add_health_bar,
 * add_ammo_counter, add_crosshair, add_minimap, add_compass, add_damage_indicator,
 * add_interaction_prompt, add_objective_tracker, add_quest_tracker.
 *
 * Each builds a small styled widget subtree from a spec
 * (WidgetAuthoring/Templates/...HudSpecs.cpp) and seats it in an existing Widget
 * Blueprint through the same checked add path as the typed adds. A slotName already
 * in the tree is refused (SLOT_EXISTS) rather than duplicated.
 */
import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildRecord, WIDGET_PLUGINS } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'widget-game-ui';
const DOMAIN = 'widget';

// A fold keeps the first member's description for a shared parameter, so shared
// parameters are described for every piece at once.
const HUD_TEXT = { type: 'string', description: 'Text the piece shows: health bar label (default HP), ammo counter (30 / 90), crosshair glyph (+), compass heading (N) or prompt action (Interact).' };
const HUD_TEXTURE = { type: 'string', description: 'Texture drawn as the minimap image or the compass strip; a path that does not load is refused.' };
const HUD_TITLE = { type: 'string', description: 'Objective tracker heading (default OBJECTIVES) or the quest name (default Quest Name).' };

const HUD_OUT = {
  widgetPath: P.widgetPath,
  slotName: P.slotName,
  widgets: { type: 'array', items: { type: 'string' }, description: 'Every widget created, root first; bind or restyle them by these names.' },
  animationName: { type: 'string', description: 'Animation created with the piece (the damage indicator\'s <slot>_Flash); play it on a hit.' },
  saved: { type: 'boolean', description: 'Whether the Widget Blueprint was saved.' },
};

function hud(action: string, summary: string, defaultSlot: string, extraProps: Record<string, unknown>, example: JsonObject): CapabilityRecordSource {
  return buildRecord({
    id: `blueprint.${action}`,
    action,
    family: FAMILY,
    domain: DOMAIN,
    summary,
    whenToUse: [`A HUD needs a ${action.replace('add_', '').replace(/_/g, ' ')} without assembling it widget by widget.`],
    whenNotToUse: ['A single plain widget is enough (add_content_widget).', 'The screen does not exist yet (create_widget_template hud builds one with pieces).'],
    inputProps: {
      widgetPath: P.widgetPath,
      slotName: { type: 'string', description: 'Name of the root widget of the piece (default: the piece name, such as HealthBar); its parts are named <slotName>_<Part>.' },
      parentSlot: P.parentSlot,
      positionX: P.positionX,
      positionY: P.positionY,
      sizeX: P.sizeX,
      sizeY: P.sizeY,
      ...extraProps,
    },
    required: ['widgetPath'],
    outputProps: HUD_OUT,
    outputRequired: ['widgetPath', 'slotName', 'widgets'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: WIDGET_PLUGINS,
    exampleInput: { action, widgetPath: '/Game/UI/WBP_HUD', ...example },
    exampleOutput: { success: true, widgetPath: '/Game/UI/WBP_HUD.WBP_HUD', slotName: defaultSlot, widgets: [defaultSlot] },
  });
}

export const WIDGET_GAME_UI_RECORDS: readonly CapabilityRecordSource[] = [
  hud('add_health_bar', 'Add a health bar (a label and a red progress bar, top-left) to a Widget Blueprint.', 'HealthBar',
    { percent: P.percent, fillColorAndOpacity: P.fillColorAndOpacity, text: HUD_TEXT },
    { percent: 0.75, text: 'HP' }),
  hud('add_ammo_counter', 'Add an ammo counter text (bottom-right) to a Widget Blueprint.', 'AmmoCounter',
    { text: HUD_TEXT, fontSize: P.fontSize, colorAndOpacity: P.colorAndOpacity },
    { text: '12 / 48', fontSize: 32 }),
  hud('add_crosshair', 'Add a crosshair glyph centred on screen (ignores the mouse) to a Widget Blueprint.', 'Crosshair',
    { text: HUD_TEXT, fontSize: P.fontSize, colorAndOpacity: P.colorAndOpacity },
    { fontSize: 40 }),
  hud('add_minimap', 'Add a minimap frame (map image plus a player dot, top-right) to a Widget Blueprint.', 'Minimap',
    { mapSize: P.mapSize, texturePath: HUD_TEXTURE },
    { mapSize: 240 }),
  hud('add_compass', 'Add a compass bar with a heading letter (top-centre) to a Widget Blueprint.', 'Compass',
    { text: HUD_TEXT, texturePath: HUD_TEXTURE },
    { text: 'N' }),
  hud('add_damage_indicator', 'Add a full-screen damage vignette with a flash animation and four edge arrows to a Widget Blueprint.', 'DamageIndicator',
    { colorAndOpacity: P.colorAndOpacity, fadeTime: P.fadeTime },
    { fadeTime: 0.5 }),
  hud('add_interaction_prompt', 'Add an interaction prompt (key badge plus action text, lower centre) to a Widget Blueprint.', 'InteractionPrompt',
    { text: HUD_TEXT, keyLabel: P.keyLabel },
    { text: 'Open door', keyLabel: 'E' }),
  hud('add_objective_tracker', 'Add an objective list panel (title plus rows, right side) to a Widget Blueprint.', 'ObjectiveTracker',
    { title: HUD_TITLE, items: P.items, maxVisibleObjectives: P.maxVisibleObjectives },
    { title: 'OBJECTIVES', items: ['Find the key', 'Open the gate'] }),
  hud('add_quest_tracker', 'Add an active-quest panel (header, quest name and objective rows, left side) to a Widget Blueprint.', 'QuestTracker',
    { title: HUD_TITLE, items: P.items },
    { title: 'The Lost Relic', items: ['Reach the temple'] }),
];
