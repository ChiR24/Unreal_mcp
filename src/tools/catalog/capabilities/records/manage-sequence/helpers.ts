/**
 * Local builders for manage_sequence capability records.
 *
 * These helpers are private to the manage-sequence pilot. They construct the
 * boilerplate portions of a CapabilityRecordSource (schemas, availability,
 * behavior, policy, cost, routing, normalization) so that each family file
 * only declares the fields that actually vary per action.
 *
 * The helpers do NOT touch the shared capability model, schema, or generator.
 */
import { getParentToolMetadata } from '../parent-metadata.js';
import type {
  CapabilityAvailability,
  CapabilityBehaviorSource,
  CapabilityRecordSource,
  CapabilityRouting,
  Draft202012ObjectSchema,
  JsonObject,
} from '../../index.js';
import {
  CapabilityAliasSchema,
  CapabilityIdSchema,
  LegacyActionNameSchema,
  LegacyToolNameSchema,
} from '../../index.js';
import { policy, SCHEMA_URI, V5_0, V5_8_P1 } from '../shared/record-presets.js';



export type PropertyMap = JsonObject;

function schema(
  properties: PropertyMap,
  required: readonly string[],
  requiredOneOf?: readonly string[],
): Draft202012ObjectSchema {
  return {
    $schema: SCHEMA_URI,
    type: 'object',
    properties,
    required: [...required],
    ...(requiredOneOf === undefined ? {} : { requiredOneOf: [...requiredOneOf] }),
    additionalProperties: false,
  };
}

const strProp = (desc: string): JsonObject => ({ type: 'string', description: desc });
const intProp = (desc: string): JsonObject => ({ type: 'integer', description: desc });
const numProp = (desc: string): JsonObject => ({ type: 'number', description: desc });
const boolProp = (desc: string): JsonObject => ({ type: 'boolean', description: desc });
const strArrProp = (itemDesc: string, desc: string): JsonObject => ({
  type: 'array',
  items: strProp(itemDesc),
  description: desc,
});

export const P = {
  action: strProp('The manage_sequence action to execute.'),
  path: strProp('Canonical /Game sequence asset path.'),
  name: strProp('Name for the new sequence or asset.'),
  assetPath: strProp('Canonical /Game asset path.'),
  newName: strProp('New name for the sequence asset.'),
  destinationPath: strProp('Destination /Game folder for the copy.'),
  actorName: strProp('Actor name in the current level.'),
  actorNames: { type: 'array', items: strProp('Actor name.'), description: 'Actor names.' },
  className: strProp('Unreal class path for the spawnable.'),
  trackType: strProp('MovieScene track type string.'),
  trackName: strProp('Name of the track to modify.'),
  property: strProp('Property name to keyframe (Transform, Location, Rotation, Scale).'),
  frame: intProp('Frame number for the keyframe.'),
  value: { description: 'Generic value (any type).' },
  bindingId: strProp('Sequencer binding GUID to key against.'),
  speed: numProp('Playback speed multiplier (positive).'),
  start: numProp('Range start frame or time.'),
  end: numProp('Range end frame or time.'),
  resolution: {
    type: ['number', 'string'],
    description: 'Tick resolution as ticks per second or a rate string such as 24000/1001.',
  },
  frameRate: {
    type: ['number', 'string'],
    description: 'Frame rate as fps or a rate string such as 24fps or 24000/1001.',
  },
  loopMode: strProp('Playback loop mode: once, loop, or pingpong.'),
  cameraShakeClass: strProp('Camera shake class path.'),
  levelNames: strArrProp('Level name.', 'Level names toggled by the visibility track.'),
  success: boolProp('Whether the action succeeded.'),
  message: strProp('Human-readable result message.'),
  sequencePath: strProp('Canonical /Game sequence asset path.'),
  masterSequencePath: strProp('Canonical /Game master sequence path.'),
  subsequencePath: strProp('Canonical /Game subsequence path.'),
  shotSequencePath: strProp('Canonical /Game shot sequence path.'),
  mapPath: strProp('Canonical /Game map path.'),
  jobId: strProp('Render job identifier.'),
  renderJobName: strProp('Name for the render job.'),
  outputDirectory: strProp('Output directory for rendered frames.'),
  fileNameFormat: strProp('Output file name format string.'),
  mrqResolution: strProp('Output resolution in WIDTHxHEIGHT format, such as 1920x1080.'),
  width: intProp('Output width in pixels (positive; paired with height).'),
  height: intProp('Output height in pixels (positive; paired with width).'),
  startFrame: intProp('Custom playback range start frame (paired with endFrame).'),
  endFrame: intProp('Custom playback range end frame (>= startFrame).'),
  mrqSettings: {
    type: 'object',
    description: 'Nested MRQ settings.',
    additionalProperties: false,
    properties: {
      handleFrameCount: intProp('Handle frame count clamped to >= 0.'),
      zeroPadFrameNumbers: intProp('Zero-padding width for frame numbers.'),
      spatialSampleCount: intProp('Spatial sample count per render sample pass.'),
      temporalSampleCount: intProp('Temporal sample count per render sample pass.'),
      antiAliasingMethod: strProp('Anti-aliasing method name, such as TSAA or FXAA.'),
      method: strProp('Anti-aliasing method alias.'),
    },
  },
  renderPass: strProp('Render pass identifier, such as beauty or object_id.'),
  renderPasses: strArrProp('Render pass identifier.', 'Render pass identifiers to add.'),
  materialPath: strProp('Material asset path for a material render pass.'),
  includeTranslucentObjects: boolProp('Whether the pass includes translucent objects.'),
  antiAliasingMethod: strProp('Anti-aliasing method name, such as TSAA or FXAA.'),
  sampleCount: intProp('Sample count per render sample pass.'),
  useCurrentLevel: boolProp('Whether to render against the currently loaded level.'),
  executorClass: strProp('Movie pipeline executor class path.'),
  burnInClassPath: strProp('Burn-in widget class path.'),
  mediaPlayerPath: strProp('Canonical /Game media player path.'),
  mediaSourcePath: strProp('Canonical /Game media source path.'),
  playlistPath: strProp('Canonical /Game media playlist path.'),
  playerPath: strProp('Canonical /Game media player path.'),
  sourcePath: strProp('Source file or asset path.'),
  filePath: strProp('File system path to a media file.'),
  url: strProp('URL to a media stream.'),
  sourceType: strProp('Media source type: file, stream, or platform.'),
  precacheFile: boolProp('Whether the file media source precaches on open.'),
  streamUrl: strProp('URL for a stream media source.'),
  defaultSourcePath: strProp('Default media source asset path for a platform media source.'),
  platformSources: {
    type: 'object',
    description: 'Per-platform media source asset paths keyed by platform name.',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
  sourcePaths: strArrProp('Media source asset path.', 'Media source asset paths for the playlist.'),
  urls: strArrProp('Media stream URL.', 'Stream URLs appended to the playlist.'),
  filePaths: strArrProp('Media file path.', 'Media file paths appended to the playlist.'),
  replayName: strProp('Name for the demo replay.'),
  demoName: strProp('Demo replay name.'),
  friendlyName: strProp('Human-readable replay name.'),
  additionalOptions: strArrProp('Replay option string.', 'Additional replay streamer options.'),
  prioritizeActors: boolProp('Whether to prioritize actor replication during recording.'),
  paused: boolProp('Whether replay playback is paused.'),
  takePresetPath: strProp('Canonical /Game take preset path.'),
  recordingSequencePath: strProp('Canonical /Game recording sequence path.'),
  takeSequencePath: strProp('Canonical /Game take sequence path.'),
  recordType: strProp('Take recording source type.'),
  sourceClasses: strArrProp('Source class path.', 'Take Recorder source class paths.'),
  clearSources: boolProp('Whether to clear existing Take Recorder sources first.'),
  recordInto: boolProp('Whether to record into the supplied sequence rather than a new take.'),
  recordedTracks: strArrProp('Track name.', 'Track names to record per source.'),
  metadata: {
    type: 'object',
    description: 'Arbitrary metadata key-value pairs.',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
};

function outputSchema(props: PropertyMap, required: readonly string[]): Draft202012ObjectSchema {
  const full: PropertyMap = { success: P.success, message: P.message, ...props };
  return schema(full, ['success', ...required]);
}

const EMPTY_OUTPUT = outputSchema({}, []);

const SEQ_PLUGINS = ['LevelSequenceEditor'];
const MRQ_PLUGINS = ['LevelSequenceEditor', 'MovieRenderPipeline'];
const TAKE_PLUGINS = ['LevelSequenceEditor', 'Takes'];
const MEDIA_PLUGINS = ['LevelSequenceEditor', 'ElectraPlayer'];

function availability(
  requiredPlugins: readonly string[],
  editorStates: readonly ('edit' | 'pie' | 'simulate')[] = ['edit'],
): CapabilityAvailability {
  return {
    unreal: { min: V5_0, max: V5_8_P1 },
    requiredPlugins: [...requiredPlugins],
    editorStates: [...editorStates],
  };
}

type EffectType = 'read' | 'write' | 'destructive';

function behavior(
  effect: EffectType,
  opts: Partial<CapabilityBehaviorSource> = {},
): CapabilityBehaviorSource {
  const isWrite = effect !== 'read';
  const idempotency = opts.idempotency ?? (effect === 'read' ? 'idempotent' : 'non-idempotent');
  return {
    effect,
    idempotency,
    longRunning: opts.longRunning ?? false,
    safeToRetry: opts.safeToRetry ?? (idempotency === 'idempotent' && effect !== 'destructive'),
    supportsPreview: opts.supportsPreview ?? false,
    supportsUndo: opts.supportsUndo ?? isWrite,
  };
}


function cost(
  latency: 'instant' | 'interactive' | 'long-running',
  resources: 'low' | 'medium' | 'high',
) {
  return { latency, resources };
}

function routing(
  dispatchAction: string,
  dispatchMode: 'tool' | 'action' | 'local' = 'tool',
): CapabilityRouting {
  return {
    parentTool: LegacyToolNameSchema.parse('manage_sequence'),
    dispatchAction: LegacyActionNameSchema.parse(dispatchAction),
    dispatchMode,
  };
}

export interface RecordSpec {
  readonly id: string;
  readonly action: string;
  readonly family: string;
  readonly domain: string;
  readonly summary: string;
  readonly whenToUse: readonly string[];
  readonly whenNotToUse: readonly string[];
  readonly inputProps: PropertyMap;
  readonly required: readonly string[];
  readonly requiredOneOf?: readonly string[];
  readonly outputProps?: PropertyMap;
  readonly outputRequired?: readonly string[];
  readonly effect: EffectType;
  readonly behavior?: Partial<CapabilityBehaviorSource>;
  readonly latency: 'instant' | 'interactive' | 'long-running';
  readonly resources: 'low' | 'medium' | 'high';
  readonly plugins: readonly string[];
  readonly editorStates?: readonly ('edit' | 'pie' | 'simulate')[];
  readonly dispatchMode?: 'tool' | 'action' | 'local';
  readonly exampleInput: JsonObject;
  readonly exampleOutput: JsonObject;
  readonly normalizationClass: CapabilityRecordSource['normalization']['class'];
  readonly normalizationRationale: string;
  readonly aliases?: readonly string[];
  readonly topics?: readonly string[];
}

export function buildRecord(spec: RecordSpec): CapabilityRecordSource {
  const input = schema(spec.inputProps, spec.required, spec.requiredOneOf);
  const output = spec.outputProps
    ? outputSchema(spec.outputProps, spec.outputRequired ?? [])
    : EMPTY_OUTPUT;
  return {
    id: CapabilityIdSchema.parse(spec.id),
    aliases: (spec.aliases ?? []).map((alias) => CapabilityAliasSchema.parse(alias)),
    legacyIds: [{ tool: LegacyToolNameSchema.parse('manage_sequence'), action: LegacyActionNameSchema.parse(spec.action) }],
    discovery: {
      domain: spec.domain,
      family: spec.family,
      topics: [spec.action, ...(spec.topics ?? [])],
      summary: spec.summary,
      whenToUse: [...spec.whenToUse],
      whenNotToUse: [...spec.whenNotToUse],
    },
    schemas: { input, output },
    examples: [{ title: spec.summary, input: spec.exampleInput, output: spec.exampleOutput }],
    availability: availability(spec.plugins, spec.editorStates),
    behavior: behavior(spec.effect, spec.behavior),
    policy: policy(spec.effect),
    cost: cost(spec.latency, spec.resources),
    routing: routing(spec.action, spec.dispatchMode),
    normalization: {
      class: spec.normalizationClass,
      disposition: 'canonical',
      rationale: spec.normalizationRationale,
    },
    deprecation: { status: 'active' },
    parent: getParentToolMetadata('manage_sequence'),
  };
}

export { MEDIA_PLUGINS, MRQ_PLUGINS, SEQ_PLUGINS, TAKE_PLUGINS };
