/**
 * manage_sequence shared input properties and record builder (buildCoreRecord
 * with an explicit id and a per-family plugin set).
 */
import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildCoreRecord, type CoreRecordSpec } from '../core/builder.js';
import { bool, int, num, str, strArr } from '../shared/schema-props.js';

export type PropertyMap = JsonObject;

export const P = {
  path: str('Canonical /Game sequence asset path.'),
  name: str('Name for the new sequence or asset.'),
  assetPath: str('Canonical /Game asset path.'),
  newName: str('New name for the sequence asset.'),
  destinationPath: str('Destination /Game folder for the copy.'),
  actorName: str('Actor name in the current level.'),
  actorNames: { type: 'array', items: str('Actor name.'), description: 'Actor names.' },
  className: str('Unreal class path for the spawnable.'),
  trackType: str('MovieScene track type string.'),
  trackName: str('Name of the track to modify.'),
  property: str('Property name to keyframe (Transform, Location, Rotation, Scale).'),
  frame: int('Frame number for the keyframe.'),
  bindingId: str('Sequencer binding GUID to key against.'),
  speed: num('Playback speed multiplier (positive).'),
  start: num('Range start frame or time.'),
  end: num('Range end frame or time.'),
  resolution: {
    type: ['number', 'string'],
    description: 'Tick resolution as ticks per second or a rate string such as 24000/1001.',
  },
  frameRate: {
    type: ['number', 'string'],
    description: 'Frame rate as fps or a rate string such as 24fps or 24000/1001.',
  },
  loopMode: str('Playback loop mode: once, loop, or pingpong.'),
  cameraShakeClass: str('Camera shake class path.'),
  levelNames: strArr('Level name.', 'Level names toggled by the visibility track.'),
  message: str('Human-readable result message.'),
  sequencePath: str('Canonical /Game sequence asset path.'),
  masterSequencePath: str('Canonical /Game master sequence path.'),
  subsequencePath: str('Canonical /Game subsequence path.'),
  shotSequencePath: str('Canonical /Game shot sequence path.'),
  mapPath: str('Canonical /Game map path.'),
  jobId: str('Render job identifier.'),
  renderJobName: str('Name for the render job.'),
  outputDirectory: str('Output directory for rendered frames.'),
  fileNameFormat: str('Output file name format string.'),
  mrqResolution: str('Output resolution in WIDTHxHEIGHT format, such as 1920x1080.'),
  width: int('Output width in pixels (positive; paired with height).'),
  height: int('Output height in pixels (positive; paired with width).'),
  startFrame: int('Custom playback range start frame (paired with endFrame).'),
  mrqSettings: {
    type: 'object',
    description: 'Nested MRQ settings.',
    additionalProperties: false,
    properties: {
      handleFrameCount: int('Handle frame count clamped to >= 0.'),
      zeroPadFrameNumbers: int('Zero-padding width for frame numbers.'),
      spatialSampleCount: int('Spatial sample count per render sample pass.'),
      temporalSampleCount: int('Temporal sample count per render sample pass.'),
      antiAliasingMethod: str('Anti-aliasing method name, such as TSAA or FXAA.'),
      method: str('Anti-aliasing method alias.'),
    },
  },
  renderPass: str('Render pass identifier, such as beauty or object_id.'),
  renderPasses: strArr('Render pass identifier.', 'Render pass identifiers to add.'),
  materialPath: str('Material asset path for a material render pass.'),
  includeTranslucentObjects: bool('Whether the pass includes translucent objects.'),
  antiAliasingMethod: str('Anti-aliasing method name, such as TSAA or FXAA.'),
  sampleCount: int('Sample count per render sample pass.'),
  useCurrentLevel: bool('Whether to render against the currently loaded level.'),
  executorClass: str('Movie pipeline executor class path.'),
  burnInClassPath: str('Burn-in widget class path.'),
  mediaPlayerPath: str('Canonical /Game media player path.'),
  mediaSourcePath: str('Canonical /Game media source path.'),
  playlistPath: str('Canonical /Game media playlist path.'),
  playerPath: str('Canonical /Game media player path.'),
  sourcePath: str('Source file or asset path.'),
  filePath: str('File system path to a media file.'),
  sourceType: str('Media source type: file or platform (network streams are refused).'),
  precacheFile: bool('Whether the file media source precaches on open.'),
  defaultSourcePath: str('Default media source asset path for a platform media source.'),
  platformSources: {
    type: 'object',
    description: 'Per-platform media source asset paths keyed by platform name.',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
  sourcePaths: strArr('Media source asset path.', 'Media source asset paths for the playlist.'),
  filePaths: strArr('Media file path.', 'Media file paths appended to the playlist.'),
  replayName: str('Name for the demo replay.'),
  demoName: str('Demo replay name.'),
  friendlyName: str('Human-readable replay name.'),
  additionalOptions: strArr('Replay option string.', 'Additional replay streamer options.'),
  prioritizeActors: bool('Whether to prioritize actor replication during recording.'),
  paused: bool('Whether replay playback is paused.'),
  takePresetPath: str('Canonical /Game take preset path.'),
  recordingSequencePath: str('Canonical /Game recording sequence path.'),
  takeSequencePath: str('Canonical /Game take sequence path.'),
  recordType: str('Take recording source type.'),
  sourceClasses: strArr('Source class path.', 'Take Recorder source class paths.'),
  clearSources: bool('Whether to clear existing Take Recorder sources first.'),
  recordInto: bool('Whether to record into the supplied sequence rather than a new take.'),
  recordedTracks: strArr('Track name.', 'Track names to record per source.'),
  metadata: {
    type: 'object',
    description: 'Arbitrary metadata key-value pairs.',
    additionalProperties: true,
    'x-unreal-reflection-boundary': true,
  },
};

export const SEQ_PLUGINS = ['LevelSequenceEditor'];
export const MRQ_PLUGINS = ['LevelSequenceEditor', 'MovieRenderPipeline'];
export const TAKE_PLUGINS = ['LevelSequenceEditor', 'Takes'];
export const MEDIA_PLUGINS = ['LevelSequenceEditor', 'ElectraPlayer'];

export type RecordSpec = Omit<CoreRecordSpec, 'parentTool' | 'bareInput' | 'costLatency' | 'costResources' | 'id' | 'dispatchAction' | 'plugins'> & {
  readonly id: string;
  readonly plugins: readonly string[];
  readonly latency: NonNullable<CoreRecordSpec['costLatency']>;
  readonly resources: NonNullable<CoreRecordSpec['costResources']>;
};

export const buildRecord = ({ latency, resources, ...spec }: RecordSpec): CapabilityRecordSource =>
  buildCoreRecord({ ...spec, parentTool: 'manage_sequence', costLatency: latency, costResources: resources });
