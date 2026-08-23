import { describe, expect, it } from 'vitest';
import {
  CINEMATICS_ACTIONS,
  MEDIA_ACTIONS,
  MOVIE_RENDER_ACTIONS,
  RECORD_REPLAY_ACTIONS,
} from '../../../../definitions/shared/action-sets.js';
import { consolidatedToolDefinitions } from '../../../../catalog/consolidated-tool-definitions.js';
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
    const seqTool = consolidatedToolDefinitions.find((t) => t.name === 'manage_sequence') as NonNullable<typeof consolidatedToolDefinitions[number]>;
    const props = seqTool.inputSchema.properties as Record<string, { enum?: readonly string[] }>;
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

function inputProps(action: string): Record<string, Record<string, unknown>> {
  return findByAction(action).schemas.input.properties as Record<string, Record<string, unknown>>;
}

describe('manage_sequence generic value, frame rate, and tick resolution contracts', () => {
  it('value stays an unconstrained any (no type) so C++ emits AnyValue', () => {
    const value = inputProps('add_keyframe').value;
    // No declared type is the contract (C++ emits AnyValue); the prose moved in
    // Todo 13/BB-069 to document the composed Transform shape, so assert the
    // invariant and the documented components instead of one frozen sentence.
    // `description` remains the ONLY key, so this still forbids any added
    // constraint keyword, not just `type`.
    expect(Object.keys(value).sort()).toEqual(['description']);
    expect(Object.hasOwn(value, 'type')).toBe(false);
    expect(String(value.description)).toContain('location');
    expect(String(value.description)).toContain('rotation');
    expect(String(value.description)).toContain('scale');
  });

  it('frameRate is a number|string union naming the rational rate form', () => {
    for (const action of ['set_display_rate', 'set_properties']) {
      const frameRate = inputProps(action).frameRate;
      expect(frameRate.type).toEqual(['number', 'string']);
      expect(String(frameRate.description)).toContain('24000/1001');
    }
  });

  it('tick resolution is a number|string union, not a keyframe property string', () => {
    const resolution = inputProps('set_tick_resolution').resolution;
    expect(resolution.type).toEqual(['number', 'string']);
    expect(String(resolution.description)).not.toContain('Property name to keyframe');
  });

  it('MRQ output resolution is the WIDTHxHEIGHT string, distinct from tick resolution', () => {
    const resolution = inputProps('configure_output_settings').resolution;
    expect(resolution.type).toBe('string');
    expect(String(resolution.description)).toContain('WIDTHxHEIGHT');
  });

  it('no record reuses the keyframe property descriptor for an unrelated field', () => {
    const KEYFRAME_DESC = 'Property name to keyframe (Transform, Location, Rotation, Scale).';
    for (const record of MANAGE_SEQUENCE_RECORDS) {
      const props = record.schemas.input.properties as Record<string, Record<string, unknown>>;
      for (const [name, shape] of Object.entries(props)) {
        if (name === 'property') continue;
        expect(shape.description).not.toBe(KEYFRAME_DESC);
      }
    }
  });
});

describe('manage_sequence MRQ fields match the native accepted contract', () => {
  it('configure_output_settings declares the integer resolution and frame-range fields', () => {
    const props = inputProps('configure_output_settings');
    for (const name of ['width', 'height', 'startFrame', 'endFrame']) {
      expect(props[name].type).toBe('integer');
    }
    expect(props.frameRate.type).toEqual(['number', 'string']);
  });

  it('configure_output_settings nests handleFrameCount/zeroPadFrameNumbers under settings', () => {
    const settings = inputProps('configure_output_settings').settings;
    expect(settings.type).toBe('object');
    const nested = settings.properties as Record<string, Record<string, unknown>>;
    expect(nested.handleFrameCount.type).toBe('integer');
    expect(nested.zeroPadFrameNumbers.type).toBe('integer');
  });

  it('anti-aliasing sample counts are integers', () => {
    const props = inputProps('configure_anti_aliasing');
    expect(props.spatialSampleCount.type).toBe('integer');
    expect(props.temporalSampleCount.type).toBe('integer');
  });

  it('add_render_pass accepts the batch, material, and translucency fields', () => {
    const props = inputProps('add_render_pass');
    expect(props.renderPasses.type).toBe('array');
    expect(props.materialPath.type).toBe('string');
    expect(props.includeTranslucentObjects.type).toBe('boolean');
  });

  it('queue_render and start_render both accept useCurrentLevel', () => {
    for (const action of ['queue_render', 'start_render']) {
      expect(inputProps(action).useCurrentLevel.type).toBe('boolean');
    }
  });
});

describe('manage_sequence media fields match the native accepted contract', () => {
  it('create_media_source declares the platform/stream/precache fields', () => {
    const props = inputProps('create_media_source');
    expect(props.platformSources.type).toBe('object');
    expect(props.defaultSourcePath.type).toBe('string');
    expect(props.streamUrl.type).toBe('string');
    expect(props.precacheFile.type).toBe('boolean');
  });

  it('create_media_playlist declares the three string-array item sources', () => {
    const props = inputProps('create_media_playlist');
    for (const name of ['sourcePaths', 'urls', 'filePaths']) {
      expect(props[name].type).toBe('array');
      expect((props[name].items as Record<string, unknown>).type).toBe('string');
    }
  });

  it('does not advertise the verified-dead media path aliases', () => {
    for (const record of MANAGE_SEQUENCE_RECORDS) {
      const props = record.schemas.input.properties as Record<string, unknown>;
      expect(Object.hasOwn(props, 'mediaTexturePath')).toBe(false);
      expect(Object.hasOwn(props, 'mediaPlaylistPath')).toBe(false);
      expect(Object.hasOwn(props, 'particleSystemPath')).toBe(false);
    }
  });
});

describe('manage_sequence replay and take fields match the native accepted contract', () => {
  it('prioritizeActors is a boolean, matching TryGetBoolField', () => {
    expect(inputProps('configure_demo_settings').prioritizeActors.type).toBe('boolean');
  });

  it('configure_demo_settings accepts additionalOptions as a string array', () => {
    const options = inputProps('configure_demo_settings').additionalOptions;
    expect(options.type).toBe('array');
    expect((options.items as Record<string, unknown>).type).toBe('string');
  });

  it('pause_demo accepts the paused toggle', () => {
    expect(inputProps('pause_demo').paused.type).toBe('boolean');
  });

  it('configure_take_sources accepts sourceClasses and clearSources', () => {
    const props = inputProps('configure_take_sources');
    expect(props.sourceClasses.type).toBe('array');
    expect(props.clearSources.type).toBe('boolean');
  });

  it('start_recording accepts recordInto and the frame-rate union', () => {
    const props = inputProps('start_recording');
    expect(props.recordInto.type).toBe('boolean');
    expect(props.frameRate.type).toEqual(['number', 'string']);
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
