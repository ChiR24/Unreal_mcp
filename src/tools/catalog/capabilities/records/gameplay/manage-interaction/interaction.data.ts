/**
 * manage_interaction records. Action order is the canonical record sequence the
 * generated parent action enum is assembled from.
 *
 * The destruction actions target an editor-world actor by name rather than an
 * asset path, so they declare `actorName` and nothing else — that is the only
 * field McpAutomationBridge_InteractionHandlersDestruction.cpp reads for them.
 * The create_* actions place their asset with `folder`, not `path`, matching
 * the GetJsonStringField(Payload, TEXT("folder"), ...) reads in the native shards.
 */
import type { CapabilityRecordSource } from '../../../model.js';
import { NP, interactionRecord } from './schema.js';

export const INTERACTION_RECORDS: readonly CapabilityRecordSource[] = [
  interactionRecord({
    action: 'configure_chest_properties',
    summary: 'Configure persistent chest properties.',
    whenToUse: ['An existing chest needs its locked state, lid open angle, open time or loot table path changed; fields left out keep their value.'],
    whenNotToUse: [
      'The Blueprint does not exist yet or lacks the components its create step adds (use manage_interaction.create_interactable).',
      'One placed actor needs its own values, not the Blueprint default (use control_actor.set_blueprint_variables).',
    ],
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
    action: 'configure_door_properties',
    summary: 'Configure persistent door properties.',
    whenToUse: ['An existing door needs a new open angle, open time or locked state; fields left out keep their value.'],
    whenNotToUse: [
      'The Blueprint does not exist yet or lacks the components its create step adds (use manage_interaction.create_interactable).',
      'One placed actor needs its own values, not the Blueprint default (use control_actor.set_blueprint_variables).',
    ],
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
    whenToUse: [
      'The trigger of an interactable Blueprint must be resized: every sphere gets radius traceDistance, every box gets extent traceDistance by traceRadius.',
      'Give traceDistance, traceRadius and traceType together, since an omitted one resets to 200, 50 or sphere; traceType only stores a name.',
    ],
    whenNotToUse: [
      'The Blueprint has no sphere or box component to resize (add one with manage_interaction.create_interactable, kind=component).',
      'Only one of several sphere or box components should change, since every one is resized (use blueprint.edit_scs).',
    ],
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
    action: 'configure_switch_properties',
    summary: 'Configure persistent switch properties.',
    whenToUse: ['An existing switch needs a different type, a toggle-back rule or a reset delay; fields left out keep their value.'],
    whenNotToUse: [
      'The Blueprint does not exist yet or lacks the components its create step adds (use manage_interaction.create_interactable).',
      'One placed actor needs its own values, not the Blueprint default (use control_actor.set_blueprint_variables).',
    ],
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
    action: 'create_chest_actor',
    summary: 'Create a chest actor Blueprint asset.',
    whenToUse: ['A chest Blueprint is needed with a base mesh, a lid mesh on a pivot, a trigger sphere and an optional locked state.'],
    whenNotToUse: [
      'An actor must be placed in the level from an existing Blueprint (use control_actor.spawn).',
      'An existing door, chest or switch only needs new settings (use manage_interaction.configure_interactable).',
    ],
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
    topics: ['door', 'new door', 'door actor', 'openable door', 'door that opens'],
    summary: 'Create a door actor Blueprint asset.',
    whenToUse: ['A door Blueprint is needed with a pivot, a door mesh and a trigger box, plus open-angle, timing, lock and key variables.'],
    whenNotToUse: [
      'An actor must be placed in the level from an existing Blueprint (use control_actor.spawn).',
      'An existing door, chest or switch only needs new settings (use manage_interaction.configure_interactable).',
    ],
    inputProps: {
      name: NP.name,
      folder: NP.folder,
      openAngle: NP.openAngle,
      openTime: NP.openTime,
      autoClose: NP.autoClose,
      autoCloseDelay: NP.autoCloseDelay,
      requiresKey: NP.requiresKey,
      locked: NP.locked,
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
    whenToUse: ['A Blueprint interface is needed that defines Interact, CanInteract and GetInteractionPrompt for actors to implement.'],
    whenNotToUse: ['A normal actor Blueprint is needed, not an interface (use blueprint.create).'],
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
    whenToUse: ['An existing Blueprint, such as a player, needs a named overlap sphere whose radius is the interaction range.'],
    whenNotToUse: ['A component other than an overlap sphere is needed (use blueprint.edit_scs).'],
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
    // Same whenToUse line as create_switch_actor on purpose: the fold merges them, and a seventh
    // distinct line would push create_interaction_component's out of the six a folded list keeps.
    whenToUse: ['A switch, button or lever needs an actor Blueprint with mesh parts and an overlap sphere as its trigger.'],
    whenNotToUse: [
      'An actor must be placed in the level from an existing Blueprint (use control_actor.spawn).',
      'Switch type, toggle and reset settings must be editable later, which only a switch Blueprint allows.',
    ],
    inputProps: { name: NP.name, folder: NP.folder },
    required: ['name'],
    exampleInput: { action: 'create_lever_actor', name: 'BP_Lever', folder: '/Game/Interactables' },
  }),
  interactionRecord({
    action: 'create_switch_actor',
    summary: 'Create a switch actor Blueprint asset.',
    whenToUse: ['A switch, button or lever needs an actor Blueprint with mesh parts and an overlap sphere as its trigger.'],
    whenNotToUse: [
      'An actor must be placed in the level from an existing Blueprint (use control_actor.spawn).',
      'An existing door, chest or switch only needs new settings (use manage_interaction.configure_interactable).',
    ],
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
    whenToUse: ['A reusable overlap-volume Blueprint is needed with a box, sphere or capsule as its root shape.'],
    whenNotToUse: [
      'An actor must be placed in the level from an existing Blueprint (use control_actor.spawn).',
      'A single trigger volume is wanted in the level with no Blueprint asset (use manage_level_structure.create_volume).',
    ],
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
    summary: 'Read a door, switch, chest, trigger or other interaction Blueprint: its components and editable property defaults. With actorName, an editor-world actor\'s name and class only.',
    topics: ['door settings', 'switch and chest', 'trigger properties', 'interactable defaults'],
    whenToUse: [
      'The components and default values of a door, switch, chest or trigger Blueprint must be checked, for example after configuring it.',
      'The class of a named actor in the level must be confirmed.',
    ],
    whenNotToUse: [
      'The components or properties of a placed actor are needed (use inspect.inspect_object).',
      'Functions, events, graph node counts or component transforms of a Blueprint are needed (use inspect.get_blueprint_details).',
    ],
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
    // The reader needs one target to resolve; without this the schema advertised
    // `required: []` and a bare call was refused by the handler instead.
    requiredOneOf: ['blueprintPath', 'actorName', 'doorPath', 'switchPath', 'chestPath', 'triggerPath'],
    exampleInput: { action: 'get_interaction_info', blueprintPath: '/Game/Blueprints/BP_Player' },
  }),
];
