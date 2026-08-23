/**
 * manage_interaction records, shard 2 of 2 (configure_trigger_filter onward).
 *
 * See interaction-1.data.ts for ordering and grounding notes. The create_*
 * actions place their asset with `folder`, not `path`, matching the
 * GetJsonStringField(Payload, TEXT("folder"), ...) reads in the native shards.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { NP, interactionRecord } from './schema.js';

export const INTERACTION_2: readonly CapabilityRecordSource[] = [
  interactionRecord({
    action: 'configure_trigger_filter',
    summary: 'Author persistent trigger filter variables.',
    inputProps: { triggerPath: NP.triggerPath },
    required: ['triggerPath'],
    exampleInput: { action: 'configure_trigger_filter', triggerPath: '/Game/Triggers/BP_Trigger' },
  }),
  interactionRecord({
    action: 'configure_trigger_response',
    summary: 'Author persistent trigger response variables.',
    inputProps: { triggerPath: NP.triggerPath },
    required: ['triggerPath'],
    exampleInput: { action: 'configure_trigger_response', triggerPath: '/Game/Triggers/BP_Trigger' },
  }),
  interactionRecord({
    action: 'create_chest_actor',
    summary: 'Create a chest actor Blueprint asset.',
    inputProps: { name: NP.name, folder: NP.folder, locked: NP.locked },
    required: ['name'],
    exampleInput: {
      action: 'create_chest_actor',
      name: 'BP_Chest',
      folder: '/Game/Interactables',
      locked: false,
    },
  }),
  interactionRecord({
    action: 'create_door_actor',
    summary: 'Create a door actor Blueprint asset.',
    inputProps: {
      name: NP.name,
      folder: NP.folder,
      openAngle: NP.openAngle,
      openTime: NP.openTime,
      autoClose: NP.autoClose,
      autoCloseDelay: NP.autoCloseDelay,
      requiresKey: NP.requiresKey,
    },
    required: ['name'],
    exampleInput: {
      action: 'create_door_actor',
      name: 'BP_Door',
      folder: '/Game/Interactables',
      openAngle: 90,
      autoClose: true,
    },
  }),
  interactionRecord({
    action: 'create_interactable_interface',
    summary: 'Create an interactable Blueprint interface asset.',
    inputProps: { name: NP.name, folder: NP.folder },
    required: ['name'],
    exampleInput: {
      action: 'create_interactable_interface',
      name: 'BPI_Interactable',
      folder: '/Game/Interfaces',
    },
  }),
  interactionRecord({
    action: 'create_interaction_component',
    summary: 'Add an interaction component to a Blueprint asset.',
    inputProps: {
      blueprintPath: NP.blueprintPath,
      componentName: NP.componentName,
      traceDistance: NP.traceDistance,
    },
    required: ['blueprintPath'],
    exampleInput: {
      action: 'create_interaction_component',
      blueprintPath: '/Game/Blueprints/BP_Player',
      componentName: 'InteractionComponent',
      traceDistance: 200,
    },
  }),
  interactionRecord({
    action: 'create_lever_actor',
    summary: 'Create a lever actor Blueprint asset.',
    inputProps: { name: NP.name, folder: NP.folder },
    required: ['name'],
    exampleInput: { action: 'create_lever_actor', name: 'BP_Lever', folder: '/Game/Interactables' },
  }),
  interactionRecord({
    action: 'create_switch_actor',
    summary: 'Create a switch actor Blueprint asset.',
    inputProps: { name: NP.name, folder: NP.folder, switchType: NP.switchType },
    required: ['name'],
    exampleInput: {
      action: 'create_switch_actor',
      name: 'BP_Switch',
      folder: '/Game/Interactables',
      switchType: 'button',
    },
  }),
  interactionRecord({
    action: 'create_trigger_actor',
    summary: 'Create a trigger actor Blueprint asset.',
    inputProps: { name: NP.name, folder: NP.folder, triggerShape: NP.triggerShape },
    required: ['name'],
    exampleInput: {
      action: 'create_trigger_actor',
      name: 'BP_Trigger',
      folder: '/Game/Triggers',
      triggerShape: 'box',
    },
  }),
  interactionRecord({
    action: 'get_interaction_info',
    summary: 'Read interaction asset or editor-world actor metadata.',
    read: true,
    // projectCanonicalOutput keeps ONLY declared fields, so while the shared
    // {assetPath} default was the whole contract every metadata field the
    // native reader emits was stripped and this read answered {}. Bounded
    // union of exactly what HandleInteractionInfoAction emits.
    outputProps: {
      assetType: { type: 'string', description: 'Resolved kind: Blueprint, Actor, Door, Switch, Chest or Trigger.' },
      blueprintName: { type: 'string', description: 'Blueprint asset name, when the target resolved to a Blueprint.' },
      blueprintPath: NP.blueprintPath,
      actorName: NP.actorName,
      actorClass: { type: 'string', description: 'Class name of the resolved editor-world actor.' },
      doorPath: NP.doorPath,
      switchPath: NP.switchPath,
      chestPath: NP.chestPath,
      triggerPath: NP.triggerPath,
    },
    inputProps: {
      blueprintPath: NP.blueprintPath,
      actorName: NP.actorName,
      doorPath: NP.doorPath,
      switchPath: NP.switchPath,
      chestPath: NP.chestPath,
      triggerPath: NP.triggerPath,
    },
    exampleInput: { action: 'get_interaction_info', blueprintPath: '/Game/Blueprints/BP_Player' },
  }),
  interactionRecord({
    action: 'setup_destructible_mesh',
    summary: 'Tag an editor-world actor as destructible-ready.',
    inputProps: { actorName: NP.actorName },
    required: ['actorName'],
    exampleInput: { action: 'setup_destructible_mesh', actorName: 'Crate_01' },
  }),
];
