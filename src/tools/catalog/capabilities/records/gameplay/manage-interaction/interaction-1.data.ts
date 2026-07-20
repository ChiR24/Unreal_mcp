/**
 * manage_interaction records, shard 1 of 2 (add_* through configure_trigger_events).
 *
 * Action order is preserved from the previous compact record list because the
 * generated parent action enum is assembled in canonical record sequence.
 *
 * The destruction actions target an editor-world actor by name rather than an
 * asset path, so they declare `actorName` and nothing else — that is the only
 * field McpAutomationBridge_InteractionHandlersDestruction.cpp reads for them.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { NP, interactionRecord } from './schema.js';

export const INTERACTION_1: readonly CapabilityRecordSource[] = [
  interactionRecord({
    action: 'add_destruction_component',
    summary: 'Add a destruction component to a Blueprint asset.',
    inputProps: { blueprintPath: NP.blueprintPath, componentName: NP.componentName },
    required: ['blueprintPath'],
    exampleInput: {
      action: 'add_destruction_component',
      blueprintPath: '/Game/Blueprints/BP_Crate',
      componentName: 'DestructionComponent',
    },
  }),
  interactionRecord({
    action: 'add_interaction_events',
    summary: 'Author interaction event dispatchers.',
    inputProps: { blueprintPath: NP.blueprintPath },
    required: ['blueprintPath'],
    exampleInput: { action: 'add_interaction_events', blueprintPath: '/Game/Blueprints/BP_Player' },
  }),
  interactionRecord({
    action: 'configure_chest_properties',
    summary: 'Configure persistent chest properties.',
    inputProps: {
      chestPath: NP.chestPath,
      locked: NP.locked,
      openAngle: NP.openAngle,
      openTime: NP.openTime,
      lootTablePath: NP.lootTablePath,
    },
    required: ['chestPath'],
    exampleInput: {
      action: 'configure_chest_properties',
      chestPath: '/Game/Interactables/BP_Chest',
      locked: false,
      openAngle: 90,
    },
  }),
  interactionRecord({
    action: 'configure_destruction_damage',
    summary: 'Tag an editor-world actor with destruction-damage state.',
    inputProps: { actorName: NP.actorName },
    required: ['actorName'],
    exampleInput: { action: 'configure_destruction_damage', actorName: 'Crate_01' },
  }),
  interactionRecord({
    action: 'configure_destruction_effects',
    summary: 'Tag an editor-world actor with destruction-effect state.',
    inputProps: { actorName: NP.actorName },
    required: ['actorName'],
    exampleInput: { action: 'configure_destruction_effects', actorName: 'Crate_01' },
  }),
  interactionRecord({
    action: 'configure_destruction_levels',
    summary: 'Tag an editor-world actor with destruction-level state.',
    inputProps: { actorName: NP.actorName },
    required: ['actorName'],
    exampleInput: { action: 'configure_destruction_levels', actorName: 'Crate_01' },
  }),
  interactionRecord({
    action: 'configure_door_properties',
    summary: 'Configure persistent door properties.',
    inputProps: {
      doorPath: NP.doorPath,
      openAngle: NP.openAngle,
      openTime: NP.openTime,
      locked: NP.locked,
    },
    required: ['doorPath'],
    exampleInput: {
      action: 'configure_door_properties',
      doorPath: '/Game/Interactables/BP_Door',
      openAngle: 90,
      openTime: 0.5,
    },
  }),
  interactionRecord({
    action: 'configure_interaction_trace',
    summary: 'Configure persistent interaction trace data.',
    inputProps: {
      blueprintPath: NP.blueprintPath,
      traceType: NP.traceType,
      traceDistance: NP.traceDistance,
      traceRadius: NP.traceRadius,
    },
    required: ['blueprintPath'],
    exampleInput: {
      action: 'configure_interaction_trace',
      blueprintPath: '/Game/Blueprints/BP_Player',
      traceType: 'sphere',
      traceDistance: 200,
    },
  }),
  interactionRecord({
    action: 'configure_interaction_widget',
    summary: 'Configure persistent interaction widget data.',
    inputProps: {
      blueprintPath: NP.blueprintPath,
      widgetClass: NP.widgetClass,
      showOnHover: NP.showOnHover,
      showPromptText: NP.showPromptText,
      promptTextFormat: NP.promptTextFormat,
    },
    required: ['blueprintPath'],
    exampleInput: {
      action: 'configure_interaction_widget',
      blueprintPath: '/Game/Blueprints/BP_Player',
      showOnHover: true,
      promptTextFormat: 'Press {Key} to Interact',
    },
  }),
  interactionRecord({
    action: 'configure_switch_properties',
    summary: 'Configure persistent switch properties.',
    inputProps: {
      switchPath: NP.switchPath,
      switchType: NP.switchType,
      canToggle: NP.canToggle,
      resetTime: NP.resetTime,
    },
    required: ['switchPath'],
    exampleInput: {
      action: 'configure_switch_properties',
      switchPath: '/Game/Interactables/BP_Switch',
      switchType: 'button',
      canToggle: true,
    },
  }),
  interactionRecord({
    action: 'configure_trigger_events',
    summary: 'Author trigger event dispatchers.',
    inputProps: { triggerPath: NP.triggerPath },
    required: ['triggerPath'],
    exampleInput: { action: 'configure_trigger_events', triggerPath: '/Game/Triggers/BP_Trigger' },
  }),
];
