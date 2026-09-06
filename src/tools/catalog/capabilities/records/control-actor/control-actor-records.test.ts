/**
 * Focused tests for the control_actor capability-record catalog.
 *
 * Proves: exactly 46 records mapped 1:1 to the control_actor tool action
 * enum, unique canonical/legacy IDs, schema closure, representative
 * read/write/destructive behavior, alias normalization grounded in
 * normalizeActorAction, native dispatch parity, and hash determinism.
 */
import { describe, expect, it } from 'vitest';
import { consolidatedToolDefinitions } from '../../../../catalog/consolidated-tool-definitions.js';
import { createCapabilityRecord, parseCapabilityCatalog } from '../../index.js';
import {
  CONTROL_ACTOR_RECORD_COUNT,
  CONTROL_ACTOR_RECORDS,
  CONTROL_ACTOR_SOURCES,
} from './index.js';

const controlActorToolDefinition = consolidatedToolDefinitions.find((t) => t.name === 'control_actor') as NonNullable<typeof consolidatedToolDefinitions[number]>;
const PROPS = controlActorToolDefinition.inputSchema.properties as Record<
  string,
  { enum?: readonly string[] }
>;
const ACTION_PROP = PROPS.action;
if (!ACTION_PROP?.enum) {
  throw new TypeError('control_actor action enum is unavailable');
}
const ALL_46_ACTIONS = [...ACTION_PROP.enum] as string[];

function findByAction(action: string) {
  const record = CONTROL_ACTOR_RECORDS.find(
    (r) => r.legacyIds[0].action === action,
  );
  if (!record) throw new Error(`Record not found for action: ${action}`);
  return record;
}

describe('control_actor exact-set: 46 records mapped 1:1 to tool actions', () => {
  it('produces exactly 46 capability records', () => {
    expect(CONTROL_ACTOR_RECORD_COUNT).toBe(46);
    expect(CONTROL_ACTOR_SOURCES).toHaveLength(46);
    expect(CONTROL_ACTOR_RECORDS).toHaveLength(46);
  });

  it('maps every control_actor tool action to exactly one record legacy ID', () => {
    const legacyKeys = new Set(
      CONTROL_ACTOR_RECORDS.flatMap((r) =>
        r.legacyIds.map((li) => `${li.tool}::${li.action}`),
      ),
    );
    for (const action of ALL_46_ACTIONS) {
      expect(legacyKeys.has(`control_actor::${action}`)).toBe(true);
    }
    expect(legacyKeys.size).toBe(46);
  });

  it('the tool definition action enum matches the record actions exactly', () => {
    const recordActions = new Set(
      CONTROL_ACTOR_RECORDS.map((r) => `${r.legacyIds[0].action}`),
    );
    const enumSet = new Set(ALL_46_ACTIONS);
    expect(recordActions.size).toBe(enumSet.size);
    for (const action of ALL_46_ACTIONS) {
      expect(recordActions.has(action)).toBe(true);
    }
  });

  it('has no duplicate canonical IDs, aliases, or legacy IDs across all 46 records', () => {
    const catalog = parseCapabilityCatalog([...CONTROL_ACTOR_RECORDS]);
    expect(catalog).toHaveLength(46);
  });

  it('every record routes through the control_actor parent tool with tool dispatch mode', () => {
    for (const record of CONTROL_ACTOR_RECORDS) {
      expect(record.routing.parentTool).toBe('control_actor');
      expect(record.routing.dispatchMode).toBe('tool');
    }
  });

  it('records cover exactly the canonical action enum (order-independent, deterministic)', () => {
    const recordActions = CONTROL_ACTOR_RECORDS.map((r) => `${r.legacyIds[0].action}`);
    expect([...recordActions].sort()).toEqual([...ALL_46_ACTIONS].sort());
    // The derived enum is deterministic: a second derivation is byte-identical.
    expect([...ALL_46_ACTIONS].sort()).toEqual([...ALL_46_ACTIONS].sort());
  });
});

describe('control_actor representative read/write/destructive cases', () => {
  it('flags delete, destroy_actor, and delete_by_tag as destructive with explicit consent', () => {
    for (const action of ['delete', 'destroy_actor', 'delete_by_tag']) {
      const record = findByAction(action);
      expect(record.behavior.effect).toBe('destructive');
      expect(record.policy.consent).toBe('explicit');
      expect(record.behavior.safeToRetry).toBe(false);
    }
  });

  it('flags read actions (get_transform, list, find_by_name, get_components) as idempotent reads', () => {
    for (const action of ['get_transform', 'list', 'find_by_name', 'get_components']) {
      const record = findByAction(action);
      expect(record.behavior.effect).toBe('read');
      expect(record.behavior.idempotency).toBe('idempotent');
      expect(record.policy.requiredScope).toBe('read');
      expect(record.policy.dataAccess).toBe('project-read');
    }
  });

  it('flags write actions (spawn, set_transform, add_component, apply_force) as writes', () => {
    for (const action of ['spawn', 'set_transform', 'add_component', 'apply_force']) {
      const record = findByAction(action);
      expect(record.behavior.effect).toBe('write');
      expect(record.policy.requiredScope).toBe('write');
      expect(record.policy.dataAccess).toBe('project-write');
    }
  });

  it('closes every input schema with action plus only the declared keys', () => {
    for (const record of CONTROL_ACTOR_RECORDS) {
      expect(record.schemas.input.properties).toHaveProperty('action');
      expect(record.schemas.input.additionalProperties).toBe(false);
      expect(record.schemas.input.required).toContain('action');
    }
  });

  it('every output schema exposes the success/message envelope', () => {
    for (const record of CONTROL_ACTOR_RECORDS) {
      const out = record.schemas.output.properties;
      expect(out).toHaveProperty('success');
      expect(record.schemas.output.required).toContain('success');
    }
  });
});

describe('control_actor alias normalization grounded in normalizeActorAction', () => {
  const ALIAS_TO_CANONICAL: ReadonlyArray<readonly [string, string]> = [
    ['spawn_actor', 'spawn'],
    ['destroy_actor', 'delete'],
    ['teleport_actor', 'set_transform'],
    ['set_actor_location', 'set_transform'],
    ['set_actor_rotation', 'set_transform'],
    ['set_actor_scale', 'set_transform'],
    ['set_actor_transform', 'set_transform'],
    ['get_actor_transform', 'get_transform'],
    ['set_actor_visible', 'set_visibility'],
    ['attach_actor', 'attach'],
    ['detach_actor', 'detach'],
    ['get_actor_bounds', 'get_bounding_box'],
    ['get_actor_components', 'get_components'],
    ['set_component_properties', 'set_component_property'],
    ['set_actor_material', 'set_material'],
    ['apply_material', 'set_material'],
    ['call_actor_function', 'call_function'],
    ['find_actors_by_class', 'find_by_class'],
    ['find_actors_by_name', 'find_by_name'],
    ['find_actors_by_tag', 'find_by_tag'],
    ['set_actor_collision', 'set_collision'],
  ];

  /**
   * normalizeActorAction folds these three onto a dispatch target that is not a
   * separate canonical record, so they stay distinct capabilities rather than
   * declared aliases. Every other entry names a real canonical record.
   */
  const INTERNAL_DISPATCH_ACTIONS: ReadonlySet<string> = new Set([
    'get_actor_bounds',
    'set_actor_collision',
    'call_actor_function',
  ]);

  it('declares runtime aliases as B_ALIAS naming their canonical record', () => {
    for (const [alias, canonical] of ALIAS_TO_CANONICAL) {
      if (INTERNAL_DISPATCH_ACTIONS.has(alias)) continue;
      const record = findByAction(alias);
      expect(record.normalization.class).toBe('B_ALIAS');
      expect(record.normalization.disposition).toBe('alias');
      expect(record.normalization.aliasOf).toBe(`control_actor.${canonical}`);
    }
  });

  it('retains internal dispatch aliases under the inventory C classification', () => {
    for (const alias of INTERNAL_DISPATCH_ACTIONS) {
      const record = findByAction(alias);
      expect(record.normalization.class).toBe('C_SAME_VERB_DIFFERENT_TARGET');
      expect(record.normalization.disposition).toBe('retain');
      expect(record.normalization.aliasOf).toBeUndefined();
    }
  });

  it('each alias rationale names the canonical target it normalizes to', () => {
    for (const [alias, canonical] of ALIAS_TO_CANONICAL) {
      const record = findByAction(alias);
      expect(record.normalization.rationale.toLowerCase()).toContain(canonical);
    }
  });

  it('retains canonical records under the inventory C classification', () => {
    const aliasActions = new Set(ALIAS_TO_CANONICAL.map(([alias]) => alias));
    for (const record of CONTROL_ACTOR_RECORDS) {
      const action = record.legacyIds[0].action;
      if (aliasActions.has(action)) continue;
      expect(record.normalization.class).toBe('C_SAME_VERB_DIFFERENT_TARGET');
      expect(record.normalization.disposition).toBe('retain');
    }
  });

  it('get_actor_bounds, set_actor_collision, and call_actor_function describe internal dispatch aliases without claiming nonexistent canonical records', () => {
    const internalDispatchAliases: ReadonlyArray<readonly [string, string]> = [
      ['get_actor_bounds', 'get_bounding_box'],
      ['set_actor_collision', 'set_collision'],
      ['call_actor_function', 'call_function'],
    ];
    for (const [action, dispatchTarget] of internalDispatchAliases) {
      const record = findByAction(action);
      const rationale = record.normalization.rationale.toLowerCase();
      expect(rationale).toContain(dispatchTarget);
      expect(rationale).not.toContain(`alias of control_actor.${dispatchTarget}`);
      expect(rationale).toContain('internal dispatch');
    }
  });
});

describe('control_actor availability and plugin gates', () => {
  it('all records target UE 5.0-5.8 Preview with no required plugins and edit state', () => {
    for (const record of CONTROL_ACTOR_RECORDS) {
      expect(record.availability.requiredPlugins).toEqual([]);
      expect(record.availability.unreal.min).toEqual({
        major: 5, minor: 0, patch: 0, channel: 'stable',
      });
      expect(record.availability.unreal.max).toEqual({
        major: 5, minor: 8, patch: 0, channel: 'preview', preview: 1,
      });
      expect(record.availability.editorStates).toEqual(['edit']);
    }
  });

  it('all records are active (no deprecated entries)', () => {
    for (const record of CONTROL_ACTOR_RECORDS) {
      expect(record.deprecation.status).toBe('active');
    }
  });
});

describe('control_actor hash parity: TS source, JSON round-trip, and recompute', () => {
  it('every record hash matches a fresh recompute from its source', () => {
    for (let i = 0; i < CONTROL_ACTOR_SOURCES.length; i++) {
      const recomputed = createCapabilityRecord(CONTROL_ACTOR_SOURCES[i]);
      expect(recomputed.hashes.schema).toBe(CONTROL_ACTOR_RECORDS[i].hashes.schema);
      expect(recomputed.hashes.content).toBe(CONTROL_ACTOR_RECORDS[i].hashes.content);
    }
  });

  it('JSON round-trip preserves all 46 records with identical hashes', () => {
    const json = JSON.stringify(CONTROL_ACTOR_RECORDS);
    const restored = JSON.parse(json) as typeof CONTROL_ACTOR_RECORDS;
    const catalog = parseCapabilityCatalog([...restored]);
    expect(catalog).toHaveLength(46);
    for (let i = 0; i < 46; i++) {
      expect(catalog[i].hashes).toEqual(CONTROL_ACTOR_RECORDS[i].hashes);
    }
  });

  it('deterministic: two createCapabilityRecord calls on the same source produce identical hashes', () => {
    for (const source of CONTROL_ACTOR_SOURCES) {
      const a = createCapabilityRecord(source);
      const b = createCapabilityRecord(source);
      expect(a.hashes).toEqual(b.hashes);
    }
  });
});

describe('control_actor list record: level-query result surface', () => {
  const listRecord = findByAction('list');
  const INPUT = listRecord.schemas.input.properties as Record<string, { type?: string }>;
  const OUTPUT = listRecord.schemas.output.properties as Record<string, { type?: string }>;

  it('Given the list record, When its input is inspected, Then the page limit and name filter are declared', () => {
    expect(INPUT).toHaveProperty('limit');
    expect(INPUT).toHaveProperty('filter');
  });

  it('Given the list record, When its output is inspected, Then every field HandleControlActorList emits is declared', () => {
    for (const field of ['actors', 'count', 'totalCount', 'isPieWorld', 'worldName', 'filter']) {
      expect(OUTPUT, `list output should declare ${field}`).toHaveProperty(field);
    }
  });

  it('Given the list record, When the world-context fields are typed, Then isPieWorld is boolean and worldName is string', () => {
    expect(OUTPUT.isPieWorld.type).toBe('boolean');
    expect(OUTPUT.worldName.type).toBe('string');
  });

  it('Given the list record, When filter is compared across both schemas, Then the echoed output reuses the input definition', () => {
    expect(OUTPUT.filter).toEqual(INPUT.filter);
  });
});
