/**
 * manage_interaction exact-schema record contract.
 *
 * The expected parameter sets below are transcribed from the editor-authoring
 * shards under plugins/McpAutomationBridge/.../Private/Domains/Interaction/.
 * The world-actor variants in *RuntimeActors.cpp / *RuntimeComponents.cpp are
 * intentionally excluded: the dispatcher in
 * McpAutomationBridge_InteractionHandlers.cpp tries the authoring handlers
 * first and each returns true for its sub-action, so their payload fields
 * (doorName, chestName, isLocked, maxItems, offsetZ, ...) are unreachable.
 */
import { describe, expect, it } from 'vitest';
import type { CapabilityRecordSource } from '../../../index.js';
import { MANAGE_INTERACTION_SOURCES } from './index.js';

const records = MANAGE_INTERACTION_SOURCES;
const byAction = new Map<string, CapabilityRecordSource>(
  records.map((r) => [r.legacyIds[0].action, r]),
);

const inputProps = (action: string): Record<string, unknown> =>
  byAction.get(action)?.schemas.input.properties ?? {};
const requiredOf = (action: string): readonly string[] =>
  byAction.get(action)?.schemas.input.required ?? [];

/** Exactly the fields each native authoring sub-action reads. */
const EXPECTED: Readonly<Record<string, readonly string[]>> = {
  add_destruction_component: ['blueprintPath', 'componentName'],
  add_interaction_events: ['blueprintPath'],
  configure_chest_properties: ['chestPath', 'locked', 'openAngle', 'openTime', 'lootTablePath'],
  configure_destruction_damage: ['actorName'],
  configure_destruction_effects: ['actorName'],
  configure_destruction_levels: ['actorName'],
  configure_door_properties: ['doorPath', 'openAngle', 'openTime', 'locked'],
  configure_interaction_trace: ['blueprintPath', 'traceType', 'traceDistance', 'traceRadius'],
  configure_interaction_widget: [
    'blueprintPath', 'widgetClass', 'showOnHover', 'showPromptText', 'promptTextFormat',
  ],
  configure_switch_properties: ['switchPath', 'switchType', 'canToggle', 'resetTime'],
  configure_trigger_events: ['triggerPath'],
  configure_trigger_filter: ['triggerPath'],
  configure_trigger_response: ['triggerPath'],
  create_chest_actor: ['name', 'folder', 'locked'],
  create_door_actor: [
    'name', 'folder', 'openAngle', 'openTime', 'autoClose', 'autoCloseDelay', 'requiresKey',
  ],
  create_interactable_interface: ['name', 'folder'],
  create_interaction_component: ['blueprintPath', 'componentName', 'traceDistance'],
  create_lever_actor: ['name', 'folder'],
  create_switch_actor: ['name', 'folder', 'switchType'],
  create_trigger_actor: ['name', 'folder', 'triggerShape'],
  get_interaction_info: [
    'blueprintPath', 'actorName', 'doorPath', 'switchPath', 'chestPath', 'triggerPath',
  ],
  setup_destructible_mesh: ['actorName'],
};

/** Mirrors the requireNonEmptyString guards in the TS handler. */
const REQUIRED: Readonly<Record<string, readonly string[]>> = {
  add_destruction_component: ['blueprintPath'],
  configure_chest_properties: ['chestPath'],
  configure_destruction_damage: ['actorName'],
  configure_door_properties: ['doorPath'],
  configure_switch_properties: ['switchPath'],
  configure_trigger_events: ['triggerPath'],
  create_door_actor: ['name'],
  get_interaction_info: [],
  setup_destructible_mesh: ['actorName'],
};

/** Payload fields only the unreachable world-actor variants would read. */
const UNREACHABLE_RUNTIME_FIELDS = [
  'doorName', 'doorType', 'switchName', 'isToggle', 'chestName', 'isLocked', 'requiredKey',
  'maxItems', 'location', 'interactionDistance', 'requiresLineOfSight', 'traceChannel',
  'useComplexCollision', 'widgetText', 'offsetZ',
];

describe('manage_interaction capability records', () => {
  it('keeps all 22 records with unique canonical ids', () => {
    expect(records).toHaveLength(22);
    expect(new Set(records.map((r) => r.id)).size).toBe(22);
    expect(Object.keys(EXPECTED)).toHaveLength(22);
  });

  it('preserves route metadata for every record', () => {
    for (const record of records) {
      const action = record.legacyIds[0].action;
      expect(record.id).toBe(`manage_interaction.${action}`);
      expect(record.legacyIds[0].tool).toBe('manage_interaction');
      expect(record.routing.parentTool).toBe('manage_interaction');
      expect(record.routing.dispatchAction).toBe(action);
      expect(record.routing.dispatchMode).toBe('tool');
    }
  });

  it('declares exactly the parameters the native authoring handler reads', () => {
    for (const [action, expected] of Object.entries(EXPECTED)) {
      expect(Object.keys(inputProps(action)).sort()).toEqual([...expected].sort());
    }
  });

  it('marks the handler-guarded parameters required', () => {
    for (const [action, expected] of Object.entries(REQUIRED)) {
      expect([...requiredOf(action)].sort()).toEqual([...expected].sort());
    }
  });

  it('drops the generic fields no interaction action consumes', () => {
    for (const record of records) {
      const props = Object.keys(record.schemas.input.properties);
      expect(props).not.toContain('assetPath');
      expect(props).not.toContain('path');
      expect(props).not.toContain('properties');
      expect(props).not.toContain('action');
    }
  });

  it('does not advertise the unreachable world-actor payload fields', () => {
    const declared = new Set(records.flatMap((r) => Object.keys(r.schemas.input.properties)));
    for (const field of UNREACHABLE_RUNTIME_FIELDS) {
      expect(declared.has(field)).toBe(false);
    }
  });

  it('folders newly created assets with folder rather than path', () => {
    // create_interaction_component is excluded: it adds a component to an
    // existing Blueprint (blueprintPath) instead of authoring a new asset.
    for (const action of [
      'create_chest_actor',
      'create_door_actor',
      'create_interactable_interface',
      'create_lever_actor',
      'create_switch_actor',
      'create_trigger_actor',
    ]) {
      expect(Object.keys(inputProps(action))).toContain('folder');
      expect(requiredOf(action)).toEqual(['name']);
    }
    expect(Object.keys(inputProps('create_interaction_component'))).toContain('blueprintPath');
  });

  it('keeps the editor-world actions keyed by actorName', () => {
    for (const action of [
      'setup_destructible_mesh',
      'configure_destruction_levels',
      'configure_destruction_effects',
      'configure_destruction_damage',
    ]) {
      expect(Object.keys(inputProps(action))).toEqual(['actorName']);
      expect(requiredOf(action)).toEqual(['actorName']);
    }
  });

  it('preserves the creation output handle on every record', () => {
    for (const record of records) {
      expect(Object.keys(record.schemas.output.properties)).toContain('assetPath');
    }
  });

  it('keeps get_interaction_info the only read action', () => {
    const reads = records.filter((r) => r.behavior.effect === 'read').map((r) => r.legacyIds[0].action);
    expect(reads).toEqual(['get_interaction_info']);
  });

  it('gives each parameter name a single shape so parent derivation emits no oneOf', () => {
    const shapes = new Map<string, Set<string>>();
    for (const record of records) {
      for (const [name, shape] of Object.entries(record.schemas.input.properties)) {
        const seen = shapes.get(name) ?? new Set<string>();
        seen.add(JSON.stringify(shape));
        shapes.set(name, seen);
      }
    }
    const conflicting = [...shapes.entries()].filter(([, seen]) => seen.size > 1).map(([name]) => name);
    expect(conflicting).toEqual([]);
    expect(shapes.size).toBe(27);
  });
});
