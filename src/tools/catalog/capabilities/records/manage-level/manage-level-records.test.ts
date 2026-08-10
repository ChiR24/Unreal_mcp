/**
 * Focused tests for the manage_level capability record catalog.
 *
 * Proves: exact 24-action set equality with manage-level-tool.ts, unique
 * canonical IDs/aliases/legacy IDs, routing fidelity to the TypeScript
 * level handlers and native Level domain dispatch, save/load/streaming/
 * lighting/metadata semantics, availability, and hash parity. Does not
 * touch the shared core builder, aggregate, pilots, or native code.
 */
import { describe, expect, it } from 'vitest';
import { consolidatedToolDefinitions } from '../../../../catalog/consolidated-tool-definitions.js';
import { createCapabilityRecord, parseCapabilityCatalog } from '../../index.js';

const manageLevelToolDefinition = consolidatedToolDefinitions.find((t) => t.name === 'manage_level') as NonNullable<typeof consolidatedToolDefinitions[number]>;
import {
  MANAGE_LEVEL_RECORD_COUNT,
  MANAGE_LEVEL_RECORDS,
  MANAGE_LEVEL_SOURCES,
} from './index.js';

const EXPECTED_ACTIONS = [
  'load', 'load_level', 'save', 'save_level', 'save_as', 'save_level_as',
  'stream', 'unload', 'unload_level', 'create_level', 'create_light',
  'build_lighting', 'set_metadata', 'export_level', 'import_level',
  'list_levels', 'get_summary', 'delete', 'delete_level', 'validate_level',
  'add_sublevel', 'rename_level', 'duplicate_level', 'get_current_level',
] as const;

function findByAction(action: string) {
  const record = MANAGE_LEVEL_RECORDS.find((r) => r.legacyIds[0].action === action);
  if (!record) throw new Error(`Record not found for action: ${action}`);
  return record;
}

describe('manage_level exact-set: 24 records mapped 1:1 to tool actions', () => {
  it('produces exactly 24 capability records', () => {
    expect(MANAGE_LEVEL_RECORD_COUNT).toBe(24);
    expect(MANAGE_LEVEL_SOURCES).toHaveLength(24);
    expect(MANAGE_LEVEL_RECORDS).toHaveLength(24);
  });

  it('maps every manage_level tool action to exactly one record legacy ID', () => {
    const legacyKeys = new Set(
      MANAGE_LEVEL_RECORDS.map((r) => `${r.legacyIds[0].tool}::${r.legacyIds[0].action}`),
    );
    for (const action of EXPECTED_ACTIONS) {
      expect(legacyKeys.has(`manage_level::${action}`)).toBe(true);
    }
    expect(legacyKeys.size).toBe(24);
  });

  it('the tool definition action enum matches the 24-action set exactly', () => {
    const props = manageLevelToolDefinition.inputSchema.properties as Record<string, { enum?: readonly string[] }>;
    const actionProp = props.action;
    if (!actionProp?.enum) {
      throw new TypeError('manage_level action enum is unavailable');
    }
    const enumSet = new Set(actionProp.enum);
    for (const action of EXPECTED_ACTIONS) {
      expect(enumSet.has(action)).toBe(true);
    }
    expect(enumSet.size).toBe(EXPECTED_ACTIONS.length);
  });

  it('has no duplicate canonical IDs, aliases, or legacy IDs across all 24 records', () => {
    const catalog = parseCapabilityCatalog([...MANAGE_LEVEL_RECORDS]);
    expect(catalog).toHaveLength(24);
  });

  it('emits records in canonical tool-definition enum order', () => {
    const props = manageLevelToolDefinition.inputSchema.properties as Record<string, { enum?: readonly string[] }>;
    const actionProp = props.action;
    if (!actionProp?.enum) {
      throw new TypeError('manage_level action enum is unavailable');
    }
    const recordOrder = MANAGE_LEVEL_RECORDS.map((r) => r.legacyIds[0].action);
    expect(recordOrder).toEqual([...actionProp.enum]);
  });
});

describe('manage_level routing fidelity to TS handlers and native Level dispatch', () => {
  it('routes all records through the manage_level parent tool', () => {
    for (const record of MANAGE_LEVEL_RECORDS) {
      expect(record.routing.parentTool).toBe('manage_level');
    }
  });

  it('parent-dispatched actions use tool mode with the dispatched sub-action', () => {
    const toolRouted: Record<string, string> = {
      load: 'load', load_level: 'load',
      save: 'save', save_level: 'save',
      save_as: 'save_level_as', save_level_as: 'save_level_as',
      export_level: 'export_level', import_level: 'import_level',
      get_summary: 'get_summary', get_current_level: 'get_current_level',
      delete: 'delete_level', delete_level: 'delete_level',
      rename_level: 'rename', duplicate_level: 'duplicate',
      add_sublevel: 'add_sublevel', unload_level: 'unload_level',
    };
    for (const [action, dispatchAction] of Object.entries(toolRouted)) {
      const record = findByAction(action);
      expect(record.routing.dispatchMode).toBe('tool');
      expect(record.routing.dispatchAction).toBe(dispatchAction);
    }
  });

  it('cross-route actions dispatch to a separate bridge action', () => {
    const actionRouted: Record<string, string> = {
      stream: 'stream_level',
      unload: 'stream_level',
      create_level: 'manage_level_structure',
      create_light: 'manage_lighting',
      build_lighting: 'manage_lighting',
      set_metadata: 'set_metadata',
      list_levels: 'list_levels',
      validate_level: 'execute_editor_function',
    };
    for (const [action, dispatchAction] of Object.entries(actionRouted)) {
      const record = findByAction(action);
      expect(record.routing.dispatchMode).toBe('action');
      expect(record.routing.dispatchAction).toBe(dispatchAction);
    }
  });
});

describe('manage_level save/load/streaming/lighting semantics', () => {
  it('flags build_lighting as the only long-running operation', () => {
    const longRunning = MANAGE_LEVEL_RECORDS.filter((r) => r.behavior.longRunning);
    expect(longRunning.map((r) => r.legacyIds[0].action)).toEqual(['build_lighting']);
    const bl = findByAction('build_lighting');
    expect(bl.cost.latency).toBe('long-running');
    expect(bl.cost.resources).toBe('high');
  });

  it('delete and delete_level are destructive and not safe to retry', () => {
    for (const action of ['delete', 'delete_level']) {
      const record = findByAction(action);
      expect(record.behavior.effect).toBe('destructive');
      expect(record.behavior.safeToRetry).toBe(false);
      expect(record.policy.consent).toBe('explicit');
    }
  });

  it('read-only query actions have read effect and idempotent behavior', () => {
    for (const action of ['list_levels', 'get_current_level', 'get_summary', 'validate_level']) {
      const record = findByAction(action);
      expect(record.behavior.effect).toBe('read');
      expect(record.behavior.idempotency).toBe('idempotent');
      expect(record.policy.dataAccess).toBe('project-read');
    }
  });

  it('stream and unload are write effects that toggle level streaming state', () => {
    for (const action of ['stream', 'unload', 'unload_level']) {
      const record = findByAction(action);
      expect(record.behavior.effect).toBe('write');
    }
  });

  it('set_metadata is an idempotent write routed to the shared metadata bridge route', () => {
    const setMeta = findByAction('set_metadata');
    expect(setMeta.behavior.effect).toBe('write');
    expect(setMeta.behavior.idempotency).toBe('idempotent');
    expect(setMeta.routing.dispatchMode).toBe('action');
    expect(setMeta.routing.dispatchAction).toBe('set_metadata');
  });

  it('export_level is a read (file artifact) while import_level is a write', () => {
    expect(findByAction('export_level').behavior.effect).toBe('read');
    expect(findByAction('import_level').behavior.effect).toBe('write');
  });
});

describe('manage_level availability and normalization metadata', () => {
  it('all records target UE 5.0 stable to 5.8 preview 1 with edit state and no plugins', () => {
    for (const record of MANAGE_LEVEL_RECORDS) {
      expect(record.availability.unreal.min).toEqual({
        major: 5, minor: 0, patch: 0, channel: 'stable',
      });
      expect(record.availability.unreal.max).toEqual({
        major: 5, minor: 8, patch: 0, channel: 'preview', preview: 1,
      });
      expect(record.availability.editorStates).toEqual(['edit']);
      expect(record.availability.requiredPlugins).toEqual([]);
    }
  });

  it('all records carry C_SAME_VERB_DIFFERENT_TARGET normalization with retain disposition', () => {
    for (const record of MANAGE_LEVEL_RECORDS) {
      expect(record.normalization.class).toBe('C_SAME_VERB_DIFFERENT_TARGET');
      expect(record.normalization.disposition).toBe('retain');
      expect(record.normalization.rationale.length).toBeGreaterThan(0);
    }
  });

  it('every record is active with a closed input schema containing the action property', () => {
    for (const record of MANAGE_LEVEL_RECORDS) {
      expect(record.deprecation.status).toBe('active');
      expect(record.schemas.input.additionalProperties).toBe(false);
      const props = record.schemas.input.properties as Record<string, unknown>;
      expect(props).toHaveProperty('action');
      expect(record.schemas.input.required).toContain('action');
    }
  });
});

describe('manage_level hash parity: TS source, JSON round-trip, and recompute', () => {
  it('every record hash matches a fresh recompute from its source', () => {
    for (let i = 0; i < MANAGE_LEVEL_SOURCES.length; i++) {
      const recomputed = createCapabilityRecord(MANAGE_LEVEL_SOURCES[i]);
      expect(recomputed.hashes.schema).toBe(MANAGE_LEVEL_RECORDS[i].hashes.schema);
      expect(recomputed.hashes.content).toBe(MANAGE_LEVEL_RECORDS[i].hashes.content);
    }
  });

  it('JSON round-trip preserves all 24 records with identical hashes', () => {
    const json = JSON.stringify(MANAGE_LEVEL_RECORDS);
    const restored = JSON.parse(json) as typeof MANAGE_LEVEL_RECORDS;
    const catalog = parseCapabilityCatalog([...restored]);
    expect(catalog).toHaveLength(24);
    for (let i = 0; i < 24; i++) {
      expect(catalog[i].hashes).toEqual(MANAGE_LEVEL_RECORDS[i].hashes);
    }
  });

  it('deterministic: two createCapabilityRecord calls on the same source produce identical hashes', () => {
    for (const source of MANAGE_LEVEL_SOURCES) {
      const a = createCapabilityRecord(source);
      const b = createCapabilityRecord(source);
      expect(a.hashes).toEqual(b.hashes);
    }
  });
});
