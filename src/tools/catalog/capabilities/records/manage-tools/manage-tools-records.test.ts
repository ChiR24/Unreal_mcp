/**
 * Focused tests for the manage_tools capability-record shard.
 *
 * Proves: exactly 8 records mapped 1:1 to the manage_tools action enum in
 * canonical order, unique canonical/legacy IDs, schema closure, read/write
 * behavior, local dispatch (no Unreal round-trip), protected tool/category
 * truthfulness, normalization metadata, and hash determinism.
 */
import { describe, expect, it } from 'vitest';
import { consolidatedToolDefinitions } from '../../../../catalog/consolidated-tool-definitions.js';
import { createCapabilityRecord, parseCapabilityCatalog } from '../../index.js';
import {
  MANAGE_TOOLS_RECORD_COUNT,
  MANAGE_TOOLS_RECORDS,
  MANAGE_TOOLS_SOURCES,
} from './index.js';

const manageToolsToolDefinition = consolidatedToolDefinitions.find((t) => t.name === 'manage_tools') as NonNullable<typeof consolidatedToolDefinitions[number]>;
const PROPS = manageToolsToolDefinition.inputSchema.properties as Record<
  string,
  { enum?: readonly string[] }
>;
const ACTION_PROP = PROPS.action;
if (!ACTION_PROP?.enum) {
  throw new TypeError('manage_tools action enum is unavailable');
}
const CANONICAL_ACTIONS = [...ACTION_PROP.enum] as string[];

function findByAction(action: string) {
  const record = MANAGE_TOOLS_RECORDS.find(
    (r) => r.legacyIds[0].action === action,
  );
  if (!record) throw new Error(`Record not found for action: ${action}`);
  return record;
}

describe('manage_tools exact-set: 8 records mapped 1:1 to tool actions', () => {
  it('produces exactly 8 capability records', () => {
    expect(MANAGE_TOOLS_RECORD_COUNT).toBe(8);
    expect(MANAGE_TOOLS_SOURCES).toHaveLength(8);
    expect(MANAGE_TOOLS_RECORDS).toHaveLength(8);
  });

  it('preserves the canonical action enum order in record sequence', () => {
    const recordActions = MANAGE_TOOLS_RECORDS.map((r) => r.legacyIds[0].action);
    expect(recordActions).toEqual(CANONICAL_ACTIONS);
  });

  it('maps every manage_tools tool action to exactly one record legacy ID', () => {
    const legacyKeys = new Set(
      MANAGE_TOOLS_RECORDS.flatMap((r) =>
        r.legacyIds.map((li) => `${li.tool}::${li.action}`),
      ),
    );
    for (const action of CANONICAL_ACTIONS) {
      expect(legacyKeys.has(`manage_tools::${action}`)).toBe(true);
    }
    expect(legacyKeys.size).toBe(8);
  });

  it('has no duplicate canonical IDs, aliases, or legacy IDs across all 8 records', () => {
    const catalog = parseCapabilityCatalog([...MANAGE_TOOLS_RECORDS]);
    expect(catalog).toHaveLength(8);
  });

  it('every record routes through the manage_tools parent tool with local dispatch', () => {
    for (const record of MANAGE_TOOLS_RECORDS) {
      expect(record.routing.parentTool).toBe('manage_tools');
      expect(record.routing.dispatchMode).toBe('local');
      expect(record.routing.dispatchAction).toBe(record.legacyIds[0].action);
    }
  });
});

describe('manage_tools representative read/write behavior', () => {
  it('flags list_tools, list_categories, get_status as idempotent reads', () => {
    for (const action of ['list_tools', 'list_categories', 'get_status']) {
      const record = findByAction(action);
      expect(record.behavior.effect).toBe('read');
      expect(record.behavior.idempotency).toBe('idempotent');
      expect(record.policy.requiredScope).toBe('read');
      expect(record.policy.dataAccess).toBe('project-read');
      expect(record.behavior.safeToRetry).toBe(true);
    }
  });

  it('flags enable_tools, disable_tools, enable_category, disable_category, reset as writes', () => {
    for (const action of [
      'enable_tools',
      'disable_tools',
      'enable_category',
      'disable_category',
      'reset',
    ]) {
      const record = findByAction(action);
      expect(record.behavior.effect).toBe('write');
      expect(record.policy.requiredScope).toBe('write');
      expect(record.policy.dataAccess).toBe('project-write');
    }
  });

  it('closes every input schema with action plus only the declared keys', () => {
    for (const record of MANAGE_TOOLS_RECORDS) {
      expect(record.schemas.input.properties).toHaveProperty('action');
      expect(record.schemas.input.additionalProperties).toBe(false);
      expect(record.schemas.input.required).toContain('action');
    }
  });

  it('every output schema exposes the success/message envelope', () => {
    for (const record of MANAGE_TOOLS_RECORDS) {
      const out = record.schemas.output.properties;
      expect(out).toHaveProperty('success');
      expect(record.schemas.output.required).toContain('success');
    }
  });
});

describe('manage_tools action-specific parameters and output shapes', () => {
  it('enable_tools and disable_tools require a tools array parameter', () => {
    for (const action of ['enable_tools', 'disable_tools']) {
      const record = findByAction(action);
      expect(record.schemas.input.properties).toHaveProperty('tools');
      expect(record.schemas.input.required).toContain('tools');
    }
  });

  it('enable_category and disable_category require a category parameter', () => {
    for (const action of ['enable_category', 'disable_category']) {
      const record = findByAction(action);
      expect(record.schemas.input.properties).toHaveProperty('category');
      expect(record.schemas.input.required).toContain('category');
    }
  });

  it('list_tools, list_categories, get_status, reset take only the action parameter', () => {
    for (const action of [
      'list_tools',
      'list_categories',
      'get_status',
      'reset',
    ]) {
      const record = findByAction(action);
      expect(record.schemas.input.required).toEqual(['action']);
    }
  });

  it('disable_tools output surfaces disabled, notFound, and protected arrays', () => {
    const record = findByAction('disable_tools');
    const out = record.schemas.output.properties;
    expect(out).toHaveProperty('disabled');
    expect(out).toHaveProperty('notFound');
    expect(out).toHaveProperty('protected');
  });

  it('disable_category output surfaces disabled and protected arrays', () => {
    const record = findByAction('disable_category');
    const out = record.schemas.output.properties;
    expect(out).toHaveProperty('disabled');
    expect(out).toHaveProperty('protected');
  });

  it('list_tools output surfaces tools, totalTools, enabledCount, disabledCount', () => {
    const record = findByAction('list_tools');
    const out = record.schemas.output.properties;
    expect(out).toHaveProperty('tools');
    expect(out).toHaveProperty('totalTools');
    expect(out).toHaveProperty('enabledCount');
    expect(out).toHaveProperty('disabledCount');
  });

  it('get_status output surfaces totalTools, enabledTools, disabledTools, categories', () => {
    const record = findByAction('get_status');
    const out = record.schemas.output.properties;
    expect(out).toHaveProperty('totalTools');
    expect(out).toHaveProperty('enabledTools');
    expect(out).toHaveProperty('disabledTools');
    expect(out).toHaveProperty('categories');
  });
});

describe('manage_tools protected tool and category truthfulness', () => {
  it('disable_tools discovery states protected tools cannot be disabled', () => {
    const record = findByAction('disable_tools');
    const guidance = [
      ...record.discovery.whenNotToUse,
      record.normalization.rationale,
    ]
      .join(' ')
      .toLowerCase();
    expect(guidance).toContain('manage_tools');
    expect(guidance).toContain('inspect');
    expect(guidance).toContain('protected');
  });

  it('disable_category discovery states the core category cannot be disabled', () => {
    const record = findByAction('disable_category');
    const guidance = [
      ...record.discovery.whenNotToUse,
      record.normalization.rationale,
    ]
      .join(' ')
      .toLowerCase();
    expect(guidance).toContain('core');
    expect(guidance).toContain('protected');
  });

  it('disable_tools and disable_category output schemas surface a protected field', () => {
    for (const action of ['disable_tools', 'disable_category']) {
      const record = findByAction(action);
      expect(record.schemas.output.properties).toHaveProperty('protected');
    }
  });
});

describe('manage_tools normalization metadata', () => {
  it('marks every record as C_SAME_VERB_DIFFERENT_TARGET with retain disposition', () => {
    for (const record of MANAGE_TOOLS_RECORDS) {
      expect(record.normalization.class).toBe('C_SAME_VERB_DIFFERENT_TARGET');
      expect(record.normalization.disposition).toBe('retain');
    }
  });

  it('every normalization rationale references the manage_tools namespace', () => {
    for (const record of MANAGE_TOOLS_RECORDS) {
      expect(record.normalization.rationale.toLowerCase()).toContain('manage_tools');
    }
  });
});

describe('manage_tools availability and deprecation', () => {
  it('all records target UE 5.0-5.8 Preview with no required plugins and edit state', () => {
    for (const record of MANAGE_TOOLS_RECORDS) {
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
    for (const record of MANAGE_TOOLS_RECORDS) {
      expect(record.deprecation.status).toBe('active');
    }
  });
});

describe('manage_tools hash parity: TS source, JSON round-trip, and recompute', () => {
  it('every record hash matches a fresh recompute from its source', () => {
    for (let i = 0; i < MANAGE_TOOLS_SOURCES.length; i++) {
      const recomputed = createCapabilityRecord(MANAGE_TOOLS_SOURCES[i]);
      expect(recomputed.hashes.schema).toBe(MANAGE_TOOLS_RECORDS[i].hashes.schema);
      expect(recomputed.hashes.content).toBe(MANAGE_TOOLS_RECORDS[i].hashes.content);
    }
  });

  it('JSON round-trip preserves all 8 records with identical hashes', () => {
    const json = JSON.stringify(MANAGE_TOOLS_RECORDS);
    const restored = JSON.parse(json) as typeof MANAGE_TOOLS_RECORDS;
    const catalog = parseCapabilityCatalog([...restored]);
    expect(catalog).toHaveLength(8);
    for (let i = 0; i < 8; i++) {
      expect(catalog[i].hashes).toEqual(MANAGE_TOOLS_RECORDS[i].hashes);
    }
  });

  it('deterministic: two createCapabilityRecord calls on the same source produce identical hashes', () => {
    for (const source of MANAGE_TOOLS_SOURCES) {
      const a = createCapabilityRecord(source);
      const b = createCapabilityRecord(source);
      expect(a.hashes).toEqual(b.hashes);
    }
  });
});
