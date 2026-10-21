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
  trackType: str('Track type: "Audio" (music or sound, then add_section with soundPath), "Transform", or a MovieScene track class name such as MovieSceneEventTrack.'),
  trackName: str('The track, by the name get_properties lists (MovieScene3DTransformTrack_1). A whole name picks that track only; part of a name (Transform) picks the tracks whose names contain it.'),
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
  cameraShakeClass: str('Camera shake class path.'),
  levelNames: strArr('Level name.', 'Level names toggled by the visibility track.'),
  message: str('Human-readable result message.'),
  sequencePath: str('Canonical /Game sequence asset path.'),
  subsequencePath: str('Canonical /Game subsequence path.'),
  shotSequencePath: str('Canonical /Game shot sequence path.'),
  mapPath: str('Canonical /Game map path.'),
  jobId: str('Render job identifier.'),
  renderJobName: str('Render job name: names a new job, or selects an existing job (alternative to jobId).'),
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
  renderPass: str('Render pass identifier: beauty (final, lit), depth, motion_vector, normal, object_id, custom_stencil (with materialPath), or ui (the game viewport\'s UMG layer: HUD and menus; widget and ui_renderer also work).'),
  renderPasses: strArr('Render pass identifier, as renderPass takes.', 'Render pass identifiers to add.'),
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
  filePath: str('A local media file under the project\'s Content or Saved folder: an absolute path there, or Content/... or Saved/... (/Saved/... too). It must exist; file:// URLs, traversal and links are refused.'),
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
  takePresetPath: str('Take Preset whose template sequence seeds a new take; cannot be combined with a sequence path or recordInto (INVALID_ARGUMENT), and one that does not load is TAKE_PRESET_NOT_FOUND.'),
  recordingSequencePath: str('Level Sequence the take is set up from: recorded as a new take based on it, or into it with recordInto true; the same as sequencePath. One that is not a Level Sequence is INVALID_SEQUENCE.'),
  takeSequencePath: str('Level Sequence the take is set up from, the same as sequencePath.'),
  recordType: str('How an actor source records the actor: possessable (onto the actor in the level), spawnable (the take spawns its own copy) or projectDefault (the Take Recorder project setting). Anything else is INVALID_ARGUMENT.'),
  sourceClasses: strArr('Source class path.', 'Take Recorder source classes to add besides actors, each listed in the plugin setting TakeRecorderSourceClassAllowlist (by default /Script/TakeRecorderSources.TakeRecorderCameraCutSource and TakeRecorderLevelVisibilitySource); another class is INVALID_SOURCE_CLASS.'),
  clearSources: bool('Remove every existing source before adding the ones sent (default false); alone it empties the take.'),
  recordInto: bool('Record into the sequence named by sequencePath instead of a new take based on it (default false).'),
  recordedTracks: strArr('Track name.', 'Recorded properties to turn on or off, by the names the actor source lists (such as Transform), matched without case.'),
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
