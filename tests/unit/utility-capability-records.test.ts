import { createHash } from 'node:crypto';
import { describe, expect, it } from 'vitest';
import { createCapabilityRecord, type CapabilityRecord } from '../../src/tools/catalog/capabilities/index.js';
import { MANAGE_SEQUENCE_RECORDS } from '../../src/tools/catalog/capabilities/records/manage-sequence/index.js';
import {
  MANAGE_AUDIO_RECORDS,
  MANAGE_AUDIO_RECORD_COUNT,
  MANAGE_AUDIO_SOURCES,
} from '../../src/tools/catalog/capabilities/records/manage-audio/index.js';
import {
  MANAGE_NETWORKING_RECORDS,
  MANAGE_NETWORKING_RECORD_COUNT,
  MANAGE_NETWORKING_SOURCES,
  NETWORKING_PARTITION_COUNTS,
} from '../../src/tools/catalog/capabilities/records/manage-networking/index.js';
import {
  UTILITY_CAPABILITY_CATALOG,
  UTILITY_CAPABILITY_RECORD_COUNT,
  UTILITY_NET_NEW_COUNT,
  UTILITY_REUSED_SEQUENCE_COUNT,
  UTILITY_SOURCE_RECORDS,
} from '../../src/tools/catalog/capabilities/records/utility/index.js';
import { consolidatedToolDefinitions } from '../../src/tools/catalog/consolidated-tool-definitions.js';
import type { ToolDefinition } from '../../src/tools/definitions/shared/tool-definition.js';

function actions(definition: ToolDefinition): readonly string[] {
  const properties = Reflect.get(definition.inputSchema, 'properties');
  if (typeof properties !== 'object' || properties === null) {
    throw new TypeError('Tool properties are unavailable');
  }
  const action = Reflect.get(properties, 'action');
  if (typeof action !== 'object' || action === null) {
    throw new TypeError('Tool action schema is unavailable');
  }
  const values = Reflect.get(action, 'enum');
  if (!Array.isArray(values) || !values.every((value) => typeof value === 'string')) {
    throw new TypeError('Tool action enum is unavailable');
  }
  return values;
}

function ids(records: readonly { readonly id: string }[]): readonly string[] {
  return records.map((record) => record.id);
}

function recordById(id: string) {
  const record = UTILITY_CAPABILITY_CATALOG.find((candidate) => candidate.id === id);
  if (record === undefined) throw new TypeError(`Missing utility capability: ${id}`);
  return record;
}

describe('Task 18 utility exact sets and canonical order', () => {
  it('has 50 audio and 77 networking net-new records', () => {
    expect(MANAGE_AUDIO_RECORD_COUNT).toBe(50);
    expect(MANAGE_AUDIO_RECORDS).toHaveLength(50);
    expect(MANAGE_NETWORKING_RECORD_COUNT).toBe(77);
    expect(MANAGE_NETWORKING_RECORDS).toHaveLength(77);
    expect(UTILITY_NET_NEW_COUNT).toBe(127);
  });

  it('matches manage_audio definition order exactly', () => {
    expect(ids(MANAGE_AUDIO_RECORDS)).toEqual(
      actions(consolidatedToolDefinitions.find((t) => t.name === 'manage_audio') as NonNullable<typeof consolidatedToolDefinitions[number]>).map((action) => `manage_audio.${action}`),
    );
  });

  it('matches manage_networking definition order exactly', () => {
    expect(ids(MANAGE_NETWORKING_RECORDS)).toEqual(
      actions(consolidatedToolDefinitions.find((t) => t.name === 'manage_networking') as NonNullable<typeof consolidatedToolDefinitions[number]>).map((action) => `manage_networking.${action}`),
    );
  });

  it('partitions networking as 27 replication + 16 session + 20 framework + 14 input', () => {
    expect(NETWORKING_PARTITION_COUNTS).toEqual({
      replication: 27,
      session: 16,
      gameFramework: 20,
      input: 14,
    });
    const counts = MANAGE_NETWORKING_RECORDS.reduce<Record<string, number>>((result, record) => {
      result[record.discovery.family] = (result[record.discovery.family] ?? 0) + 1;
      return result;
    }, {});
    expect(counts).toEqual({ replication: 27, session: 16, gameFramework: 20, input: 14 });
  });
});

describe('Task 18 deterministic frozen utility aggregate', () => {
  it('contains 208 unique records and reuses all 81 sequence objects by identity', () => {
    expect(UTILITY_CAPABILITY_RECORD_COUNT).toBe(208);
    expect(new Set(ids(UTILITY_CAPABILITY_CATALOG)).size).toBe(208);
    expect(UTILITY_REUSED_SEQUENCE_COUNT).toBe(81);
    for (const sequenceRecord of MANAGE_SEQUENCE_RECORDS) {
      expect(UTILITY_SOURCE_RECORDS.find((record) => record.id === sequenceRecord.id))
        .toBe(sequenceRecord);
    }
  });

  it('is frozen and sorted by canonical ID', () => {
    expect(Object.isFrozen(UTILITY_CAPABILITY_CATALOG)).toBe(true);
    expect(ids(UTILITY_CAPABILITY_CATALOG)).toEqual([...ids(UTILITY_CAPABILITY_CATALOG)].sort());
  });

  it('matches the pinned canonical ID/schema/content hash', () => {
    const body = UTILITY_CAPABILITY_CATALOG.map(
      (record) => `${record.id}|${record.hashes.schema}|${record.hashes.content}`,
    ).join('\n');
    expect(createHash('sha256').update(body).digest('hex'))
      .toBe('48262ba08f85792cad4b6136dc8501cb4f506ef723008c00b098422809bd5f8a');
  });

  it('retains stable record hashes after recomputation', () => {
    const sources = [...MANAGE_AUDIO_SOURCES, ...MANAGE_NETWORKING_SOURCES];
    const records = [...MANAGE_AUDIO_RECORDS, ...MANAGE_NETWORKING_RECORDS];
    // Records are sorted by canonical ID; sources keep authored order. Pair by ID, not position.
    const recordsById = new Map<string, CapabilityRecord>();
    for (const record of records) {
      if (recordsById.has(record.id)) {
        throw new TypeError(`Duplicate utility capability record: ${record.id}`);
      }
      recordsById.set(record.id, record);
    }

    const matched = new Set<string>();
    for (const source of sources) {
      const record = recordsById.get(source.id);
      if (record === undefined) {
        throw new TypeError(`Missing utility capability record for source: ${source.id}`);
      }
      if (matched.has(source.id)) {
        throw new TypeError(`Duplicate utility capability source: ${source.id}`);
      }
      matched.add(source.id);
      expect(createCapabilityRecord(source).hashes).toEqual(record.hashes);
    }
    expect(matched.size).toBe(records.length);
  });
});

describe('Task 18 truthful availability, routing, async, and artifact metadata', () => {
  it('gates MetaSound actions on MetaSound and input actions on EnhancedInput', () => {
    expect(recordById('manage_audio.create_metasound').availability.requiredPlugins)
      .toContain('MetaSound');
    expect(recordById('manage_networking.create_input_action').availability.requiredPlugins)
      .toContain('EnhancedInput');
  });

  it('gates online session and voice actions on OnlineSubsystem capabilities', () => {
    expect(recordById('manage_networking.host_lan_server').availability.requiredPlugins)
      .toEqual(expect.arrayContaining(['OnlineSubsystem', 'OnlineSubsystemUtils']));
    expect(recordById('manage_networking.enable_voice_chat').availability.requiredPlugins)
      .toContain('OnlineSubsystem');
  });

  it('routes networking families through their truthful native child domains', () => {
    expect(recordById('manage_networking.set_property_replicated').routing.dispatchAction)
      .toBe('manage_networking');
    expect(recordById('manage_networking.host_lan_server').routing.dispatchAction)
      .toBe('manage_sessions');
    expect(recordById('manage_networking.create_game_mode').routing.dispatchAction)
      .toBe('manage_game_framework');
    expect(recordById('manage_networking.create_input_action').routing.dispatchAction)
      .toBe('manage_input');
  });

  it('does not claim unproven async completion or cancellation', () => {
    for (const record of [...MANAGE_AUDIO_RECORDS, ...MANAGE_NETWORKING_RECORDS]) {
      expect(record.behavior.longRunning).toBe(false);
      expect(record.discovery.summary.toLowerCase()).not.toContain('completed asynchronously');
    }
  });

  it('requires identifiable artifacts or state in creation and session query outputs', () => {
    expect(recordById('manage_audio.create_sound_cue').schemas.output.required)
      .toContain('assetPath');
    expect(recordById('manage_networking.create_game_mode').schemas.output.required)
      .toContain('assetPath');
    expect(recordById('manage_networking.get_sessions_info').schemas.output.required)
      .toContain('sessions');
  });

  it('keeps destructive/reconfiguration operations fail-closed for retries', () => {
    expect(recordById('manage_networking.remove_mapping').behavior.safeToRetry).toBe(false);
    expect(recordById('manage_networking.remove_local_player').behavior.safeToRetry).toBe(false);
    expect(recordById('manage_audio.pop_sound_mix').behavior.safeToRetry).toBe(false);
  });
});
