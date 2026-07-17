import { describe, expect, it } from 'vitest';
import {
  CINEMATICS_ACTIONS,
  MEDIA_ACTIONS,
  MOVIE_RENDER_ACTIONS,
  RECORD_REPLAY_ACTIONS,
} from '../../../../definitions/shared/action-sets.js';
import { manageSequenceToolDefinition } from '../../../../definitions/utility/manage-sequence-tool.js';
import { createCapabilityRecord, parseCapabilityCatalog } from '../../index.js';
import {
  MANAGE_SEQUENCE_RECORD_COUNT,
  MANAGE_SEQUENCE_RECORDS,
  MANAGE_SEQUENCE_SOURCES,
} from './index.js';

const CORE_ACTIONS = [
  'create', 'open', 'add_camera', 'add_actor', 'add_actors', 'remove_actors',
  'get_bindings', 'play', 'pause', 'stop', 'set_playback_speed', 'add_keyframe',
  'get_properties', 'set_properties', 'duplicate', 'rename', 'delete', 'list', 'get_metadata', 'set_metadata',
  'add_spawnable_from_class', 'add_track', 'add_section', 'set_display_rate', 'set_tick_resolution',
  'set_work_range', 'set_view_range', 'set_track_muted', 'set_track_solo', 'set_track_locked',
  'list_tracks', 'remove_track', 'list_track_types',
] as const;

const ALL_81_ACTIONS = [
  ...CORE_ACTIONS,
  ...CINEMATICS_ACTIONS,
  ...MOVIE_RENDER_ACTIONS,
  ...MEDIA_ACTIONS,
  ...RECORD_REPLAY_ACTIONS,
];

function findByAction(action: string) {
  const record = MANAGE_SEQUENCE_RECORDS.find(
    (r) => r.legacyIds[0].action === action,
  );
  if (!record) throw new Error(`Record not found for action: ${action}`);
  return record;
}

describe('manage_sequence exact-set: 81 records mapped 1:1 to tool actions', () => {
  it('produces exactly 81 capability records', () => {
    expect(MANAGE_SEQUENCE_RECORD_COUNT).toBe(81);
    expect(MANAGE_SEQUENCE_SOURCES).toHaveLength(81);
    expect(MANAGE_SEQUENCE_RECORDS).toHaveLength(81);
  });

  it('maps every manage_sequence tool action to exactly one record legacy ID', () => {
    const legacyKeys = new Set(
      MANAGE_SEQUENCE_RECORDS.flatMap((r) =>
        r.legacyIds.map((li) => `${li.tool}::${li.action}`),
      ),
    );
    for (const action of ALL_81_ACTIONS) {
      expect(legacyKeys.has(`manage_sequence::${action}`)).toBe(true);
    }
    expect(legacyKeys.size).toBe(81);
  });

  it('the tool definition action enum matches the union of action sets exactly', () => {
    const props = manageSequenceToolDefinition.inputSchema.properties as Record<string, { enum?: readonly string[] }>;
    const actionProp = props.action;
    if (!actionProp?.enum) {
      throw new TypeError('manage_sequence action enum is unavailable');
    }
    const enumSet = new Set(actionProp.enum);
    for (const action of ALL_81_ACTIONS) {
      expect(enumSet.has(action)).toBe(true);
    }
    expect(enumSet.size).toBe(ALL_81_ACTIONS.length);
  });

  it('has no duplicate canonical IDs, aliases, or legacy IDs across all 81 records', () => {
    const catalog = parseCapabilityCatalog([...MANAGE_SEQUENCE_RECORDS]);
    expect(catalog).toHaveLength(81);
  });
});

describe('manage_sequence async/output contracts', () => {
  const longRunning = MANAGE_SEQUENCE_RECORDS.filter((r) => r.behavior.longRunning);
  const longRunningActions = new Set(longRunning.map((r) => r.legacyIds[0].action));

  it('flags only start_render, start_recording, start_demo_recording, and start_killcam as long-running', () => {
    expect(longRunningActions).toEqual(
      new Set(['start_render', 'start_recording', 'start_demo_recording', 'start_killcam']),
    );
  });

  it('start_render output exposes artifact completion truth fields', () => {
    const startRender = findByAction('start_render');
    const outputProps = startRender.schemas.output.properties as Record<string, unknown>;
    expect(outputProps).toHaveProperty('outputDirectory');
    expect(outputProps).toHaveProperty('renderContinuesAsynchronously');
    expect(outputProps).toHaveProperty('bCancellationDeadlineExpired');
  });

  it('queue_render summary does not equate enqueue to render completion', () => {
    const queueRender = findByAction('queue_render');
    expect(queueRender.discovery.summary.toLowerCase()).toContain('does not');
  });

  it('stop_recording output exposes hasRecordedData artifact field', () => {
    const stopRecording = findByAction('stop_recording');
    const outputProps = stopRecording.schemas.output.properties as Record<string, unknown>;
    expect(outputProps).toHaveProperty('hasRecordedData');
  });

  it('stop_demo_recording output exposes replayName artifact field', () => {
    const stopDemo = findByAction('stop_demo_recording');
    const outputProps = stopDemo.schemas.output.properties as Record<string, unknown>;
    expect(outputProps).toHaveProperty('replayName');
  });
});

describe('manage_sequence availability and plugin gates', () => {
  it('all records require LevelSequenceEditor and target UE 5.0-5.8 Preview', () => {
    for (const record of MANAGE_SEQUENCE_RECORDS) {
      expect(record.availability.requiredPlugins).toContain('LevelSequenceEditor');
      expect(record.availability.unreal.min).toEqual({
        major: 5, minor: 0, patch: 0, channel: 'stable',
      });
      expect(record.availability.unreal.max).toEqual({
        major: 5, minor: 8, patch: 0, channel: 'preview', preview: 1,
      });
    }
  });

  it('MRQ records require MovieRenderPipeline plugin', () => {
    const mrqRecords = MANAGE_SEQUENCE_RECORDS.filter(
      (r) => r.discovery.family === 'mrq',
    );
    expect(mrqRecords).toHaveLength(8);
    for (const record of mrqRecords) {
      expect(record.availability.requiredPlugins).toContain('MovieRenderPipeline');
    }
  });

  it('take records require Takes plugin', () => {
    const takeRecords = MANAGE_SEQUENCE_RECORDS.filter(
      (r) => r.discovery.family === 'take',
    );
    expect(takeRecords).toHaveLength(5);
    for (const record of takeRecords) {
      expect(record.availability.requiredPlugins).toContain('Takes');
    }
  });

  it('media records require ElectraPlayer plugin', () => {
    const mediaRecords = MANAGE_SEQUENCE_RECORDS.filter(
      (r) => r.discovery.family === 'media',
    );
    expect(mediaRecords).toHaveLength(8);
    for (const record of mediaRecords) {
      expect(record.availability.requiredPlugins).toContain('ElectraPlayer');
    }
  });

  it('replay records require OnlineSubsystem plugin', () => {
    const replayRecords = MANAGE_SEQUENCE_RECORDS.filter(
      (r) => r.discovery.family === 'replay',
    );
    expect(replayRecords).toHaveLength(9);
    for (const record of replayRecords) {
      expect(record.availability.requiredPlugins).toContain('OnlineSubsystem');
    }
  });
});

describe('manage_sequence routing and cross-parent set_metadata', () => {
  it('all records route through manage_sequence parent tool except set_metadata', () => {
    for (const record of MANAGE_SEQUENCE_RECORDS) {
      const action = record.legacyIds[0].action;
      if (action === 'set_metadata') continue;
      expect(record.routing.parentTool).toBe('manage_sequence');
      expect(record.routing.dispatchMode).toBe('tool');
    }
  });

  it('set_metadata uses cross-parent action dispatch mode (Level domain)', () => {
    const setMeta = findByAction('set_metadata');
    expect(setMeta.routing.dispatchMode).toBe('action');
    expect(setMeta.routing.dispatchAction).toBe('set_metadata');
    expect(setMeta.normalization.rationale.toLowerCase()).toContain('cross-parent');
  });

  it('get_metadata routes through manage_sequence tool dispatch', () => {
    const getMeta = findByAction('get_metadata');
    expect(getMeta.routing.dispatchMode).toBe('tool');
  });

  it('timeline edits are modeled separately from MRQ renders', () => {
    const timeline = MANAGE_SEQUENCE_RECORDS.filter(
      (r) => r.discovery.family === 'timeline',
    );
    const mrq = MANAGE_SEQUENCE_RECORDS.filter(
      (r) => r.discovery.family === 'mrq',
    );
    expect(timeline.length + mrq.length).toBeLessThan(81);
    expect(timeline.every((r) => r.discovery.domain === 'sequence')).toBe(true);
    expect(mrq.every((r) => r.discovery.domain === 'movie_render')).toBe(true);
  });
});

describe('manage_sequence failure and cancellation semantics', () => {
  it('only start_render supports advisory cancellation (longRunning + write + high resources)', () => {
    const startRender = findByAction('start_render');
    expect(startRender.behavior.longRunning).toBe(true);
    expect(startRender.cost.latency).toBe('long-running');
    expect(startRender.cost.resources).toBe('high');
  });

  it('take recorder records preserve no-cancel limitation', () => {
    const startRec = findByAction('start_recording');
    expect(startRec.behavior.longRunning).toBe(true);
    expect(startRec.normalization.rationale).toContain('interrupt');
  });

  it('replay records preserve no-cancel limitation', () => {
    const startDemo = findByAction('start_demo_recording');
    expect(startDemo.behavior.longRunning).toBe(true);
    expect(startDemo.normalization.rationale).toContain('interrupt');
  });

  it('destructive records (delete, remove_track) are not safe to retry', () => {
    const destructive = MANAGE_SEQUENCE_RECORDS.filter(
      (r) => r.behavior.effect === 'destructive',
    );
    for (const record of destructive) {
      expect(record.behavior.safeToRetry).toBe(false);
    }
  });
});

describe('manage_sequence hash parity: TS source, JSON round-trip, and recompute', () => {
  it('every record hash matches a fresh recompute from its source', () => {
    for (let i = 0; i < MANAGE_SEQUENCE_SOURCES.length; i++) {
      const recomputed = createCapabilityRecord(MANAGE_SEQUENCE_SOURCES[i]);
      expect(recomputed.hashes.schema).toBe(MANAGE_SEQUENCE_RECORDS[i].hashes.schema);
      expect(recomputed.hashes.content).toBe(MANAGE_SEQUENCE_RECORDS[i].hashes.content);
    }
  });

  it('JSON round-trip preserves all 81 records with identical hashes', () => {
    const json = JSON.stringify(MANAGE_SEQUENCE_RECORDS);
    const restored = JSON.parse(json) as typeof MANAGE_SEQUENCE_RECORDS;
    const catalog = parseCapabilityCatalog([...restored]);
    expect(catalog).toHaveLength(81);
    for (let i = 0; i < 81; i++) {
      expect(catalog[i].hashes).toEqual(MANAGE_SEQUENCE_RECORDS[i].hashes);
    }
  });

  it('deterministic: two createCapabilityRecord calls on the same source produce identical hashes', () => {
    for (const source of MANAGE_SEQUENCE_SOURCES) {
      const a = createCapabilityRecord(source);
      const b = createCapabilityRecord(source);
      expect(a.hashes).toEqual(b.hashes);
    }
  });
});
