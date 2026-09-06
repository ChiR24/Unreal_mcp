import { SCHEMA_URI, V5_0, V5_8_P1 } from '../shared/record-presets.js';
import type {
  CapabilityBehaviorSource,
  CapabilityRecordSource,
  Draft202012ObjectSchema,
  JsonObject,
} from '../../index.js';
import {
  CapabilityAliasSchema,
  CapabilityIdSchema,
  LegacyActionNameSchema,
  LegacyToolNameSchema,
} from '../../index.js';
import { getParentToolMetadata } from '../parent-metadata.js';
import { outputProperty } from './output-glossary.js';
import { buildExampleInput, buildExampleOutput } from './example-values.js';


const BOOLEAN_FIELDS = new Set([
  'enabled', 'enable', 'replicated', 'reliable', 'withValidation', 'alwaysRelevant',
  'onlyRelevantToOwner', 'usePushModel', 'enablePrediction', 'replicateMovement',
  'autoPlay', 'looping', 'save', 'muted', 'voiceEnabled', 'pushToTalkEnabled',
  'bIsLANMatch', 'bAllowJoinInProgress', 'bAllowInvites', 'bUsesPresence',
  'bUseLobbiesIfAvailable', 'bShouldAdvertise', 'executeTravel', 'forceRespawn',
  'canRespawn', 'systemWide',
  // Legacy input-mapping modifier flags. The native handler reads these with
  // TryGetBoolField (McpAutomationBridge_InputHandlersLegacyMappings.cpp), so
  // publishing them as strings made a schema-valid boolean unrepresentable.
  'shift', 'ctrl', 'alt', 'cmd',
]);
const NUMBER_FIELDS = new Set([
  'volume', 'pitch', 'startTime', 'fadeTime', 'fadeInTime', 'fadeOutTime',
  'targetVolume', 'innerRadius', 'falloffDistance', 'windowSize', 'dopplerIntensity',
  'velocityScale', 'occlusionVolumeScale', 'occlusionFilterScale',
  'occlusionInterpolationTime', 'netUpdateFrequency', 'minNetUpdateFrequency',
  'netPriority', 'netCullDistanceSquared', 'correctionThreshold', 'smoothingRate',
  'priority', 'playerIndex', 'controllerId', 'serverPort', 'attenuationRadius',
  'attenuationFalloff', 'numRounds', 'roundTime', 'intermissionTime', 'numTeams',
  'teamSize', 'scorePerKill', 'scorePerAssist', 'scorePerObjective', 'winScore',
  'respawnDelay', 'teamIndex', 'scale',
  'lowPassFilterFrequency', 'maxRespawns', 'localPlayerNum',
]);
const OBJECT_FIELDS = new Set(['location', 'rotation', 'size', 'properties', 'settings', 'voiceSettings']);
const ARRAY_FIELDS = new Set(['states', 'sessions', 'players', 'mappings']);

// Real descriptions for fields whose bare names read as placeholder text.
// Descriptions are the one place the caller can learn what a value means
// without reading the native handler, so echoing the field name is content-free
// (the MCPBB-091 defect class).
const FIELD_DESCRIPTIONS: Readonly<Record<string, string>> = {
  name: 'Name of the asset or mapping to create or remove.',
  actionName: 'Legacy input action name. Overrides name when both are supplied.',
  key: 'Input key name, e.g. SpaceBar, W, LeftMouseButton.',
  shift: 'Whether the Shift modifier must be held.',
  ctrl: 'Whether the Ctrl modifier must be held.',
  alt: 'Whether the Alt modifier must be held.',
  cmd: 'Whether the Cmd modifier must be held.',
  axisName: 'Legacy input axis name. Overrides name when both are supplied.',
  scale: 'Axis scale value.',
  actorName: 'Target actor label or name in the current level.',
  analysisType: 'Audio analysis type to enable (for example spectrum or loudness).',
  assetPath: 'Canonical /Game asset path.',
  attachPointName: 'Socket or bone name to attach the sound to.',
  attenuationPath: 'Canonical /Game SoundAttenuation asset path.',
  attenuationShape: 'Attenuation shape (Sphere, Capsule, Box, Cone).',
  autoPlay: 'Whether the sound starts playing on spawn.',
  componentName: 'Name of the component to create or address.',
  concurrencyPath: 'Canonical /Game SoundConcurrency asset path.',
  defaultValue: 'Default value for the input.',
  dopplerIntensity: 'Doppler effect intensity multiplier.',
  effectType: 'Source effect preset class or short name.',
  enable: 'Whether the feature is enabled.',
  enableReverbSend: 'Whether the sound sends to reverb.',
  enabled: 'Whether the feature is enabled.',
  fadeInTime: 'Fade-in duration in seconds.',
  fadeOutTime: 'Fade-out duration in seconds.',
  fadeTime: 'Fade duration in seconds.',
  fadeType: 'Fade curve type (FadeTo, FadeIn, FadeOut).',
  falloffDistance: 'Distance over which attenuation falls off, in centimetres.',
  falloffMode: 'Attenuation falloff mode.',
  innerRadius: 'Inner radius of full volume, in centimetres.',
  inputName: 'Graph input name.',
  inputType: 'Graph input data type (Float, Int32, Bool, String, Trigger, Audio).',
  location: 'World location as {x, y, z} (an [x, y, z] array is accepted).',
  looping: 'Whether playback loops.',
  lowPassFilterFrequency: 'Low-pass filter cutoff frequency in Hz.',
  mixName: 'Sound Mix name.',
  nodeClassName: 'Node class name; short names such as Sine resolve against the MetaSound registry (UE.Sine.Audio).',
  nodeType: 'Node type or class short name.',
  occlusionFilterScale: 'Low-pass filter scale applied while occluded (0-1).',
  occlusionInterpolationTime: 'Seconds to interpolate occlusion changes.',
  occlusionVolumeScale: 'Volume scale applied while occluded (0-1).',
  outputName: 'Graph output name.',
  outputType: 'Graph output data type.',
  parentClass: 'Parent class path or short name.',
  path: 'Canonical /Game folder for the created asset.',
  pitch: 'Pitch multiplier.',
  properties: 'Key-value property map applied by reflection.',
  reverbDistanceMax: 'Distance at which the reverb wet level reaches its maximum.',
  reverbDistanceMin: 'Distance at which the reverb send starts.',
  reverbEffect: 'Canonical /Game ReverbEffect asset path.',
  reverbWetLevelMax: 'Maximum reverb wet level (0-1).',
  reverbWetLevelMin: 'Minimum reverb wet level (0-1).',
  rotation: 'World rotation as {pitch, yaw, roll}.',
  save: 'Persist the created or modified asset to disk.',
  size: 'Reverb zone extent as {x, y, z}.',
  soundClassName: 'Sound Class name.',
  soundClassPath: 'Canonical /Game SoundClass asset path.',
  soundName: 'Actor label/name or AudioComponent name of the playing sound.',
  soundPath: 'Canonical /Game sound asset path (SoundWave, SoundCue or MetaSound).',
  sourceNode: 'Source node id or name.',
  sourceNodeId: 'Source graph node id.',
  sourceOutputName: 'Output pin name on the source node.',
  sourcePin: 'Output pin name on the source node.',
  spatialization: 'Spatialization method (Default, Binaural).',
  speakerPath: 'Canonical /Game DialogueVoice asset path of the speaker.',
  startTime: 'Playback start offset in seconds.',
  targetInputName: 'Input pin name on the target node.',
  targetNode: 'Target node id or name.',
  targetNodeId: 'Target graph node id.',
  targetPin: 'Input pin name on the target node.',
  targetVolume: 'Target volume multiplier (0-1).',
  velocityScale: 'Velocity scale for Doppler calculations.',
  volume: 'Volume multiplier.',
  volumeAdjuster: 'Volume multiplier applied by the mix modifier.',
  wavePath: 'Canonical /Game SoundWave asset path.',
  windowSize: 'Analysis window size in samples.',
};

// INPUT-ONLY. Keyed on bare field names, so adding an envelope field such as
// `success` here would silently retype any input sharing that name. The output
// envelope is built explicitly in ./output-glossary.js instead.
/** Turns a bare camelCase field name into a readable sentence so no schema ships a name as its own description (dogfood #110). */
function humanizeFieldName(name: string): string {
  const words = name
    .replace(/^b(?=[A-Z])/, '')
    .replace(/([a-z0-9])([A-Z])/g, '$1 $2')
    .replace(/([A-Z]+)([A-Z][a-z])/g, '$1 $2')
    .replace(/_/g, ' ')
    .toLowerCase();
  const sentence = words.charAt(0).toUpperCase() + words.slice(1);
  if (/path$/i.test(name)) return `${sentence} (canonical /Game asset path).`;
  if (/^(b|is|has|should|enable)/.test(name) && !/path$/i.test(name)) return `Whether ${words.replace(/^(is|has|should|enable) /, '')} applies.`;
  return `${sentence}.`;
}

function property(name: string): JsonObject {
  const description = FIELD_DESCRIPTIONS[name] ?? humanizeFieldName(name);
  if (BOOLEAN_FIELDS.has(name)) return { type: 'boolean', description };
  if (NUMBER_FIELDS.has(name)) return { type: 'number', description };
  if (OBJECT_FIELDS.has(name)) {
    return {
      type: 'object',
      description,
      additionalProperties: true,
      'x-unreal-reflection-boundary': true,
    };
  }
  if (ARRAY_FIELDS.has(name)) return { type: 'array', description, items: {} };
  return { type: 'string', description };
}

function schema(
  fields: readonly string[],
  required: readonly string[],
  requiredOneOf?: readonly string[],
): Draft202012ObjectSchema {
  const properties: Record<string, JsonObject> = {};
  for (const field of fields) properties[field] = property(field);
  return {
    $schema: SCHEMA_URI,
    type: 'object',
    properties,
    required: [...required],
    ...(requiredOneOf === undefined ? {} : { requiredOneOf: [...requiredOneOf] }),
    additionalProperties: false,
  };
}

function outputSchema(fields: readonly string[], required: readonly string[]): Draft202012ObjectSchema {
  const properties: Record<string, JsonObject> = {};
  for (const field of fields) properties[field] = outputProperty(field);
  return {
    $schema: SCHEMA_URI,
    type: 'object',
    properties,
    required: [...required],
    additionalProperties: false,
  };
}

type Effect = 'read' | 'write' | 'destructive';

export type UtilityRecordSpec = {
  readonly tool: 'manage_audio' | 'manage_networking';
  readonly action: string;
  readonly family: string;
  readonly summary: string;
  readonly topics?: readonly string[];
  readonly params?: readonly string[];
  readonly required?: readonly string[];
  readonly requiredOneOf?: readonly string[];
  readonly outputs?: readonly string[];
  readonly outputRequired?: readonly string[];
  readonly plugins?: readonly string[];
  readonly states?: readonly ('edit' | 'pie' | 'simulate')[];
  readonly effect?: Effect;
  readonly safeToRetry?: boolean;
  readonly supportsUndo?: boolean;
  readonly dispatchAction?: string;
  readonly resources?: 'low' | 'medium' | 'high';
};

function behavior(spec: UtilityRecordSpec): CapabilityBehaviorSource {
  const effect = spec.effect ?? 'write';
  const idempotency = effect === 'read' ? 'idempotent' : 'non-idempotent';
  return {
    effect,
    idempotency,
    longRunning: false,
    safeToRetry: spec.safeToRetry ?? (idempotency === 'idempotent' && effect !== 'destructive'),
    supportsPreview: false,
    supportsUndo: spec.supportsUndo ?? (effect === 'write' && (spec.states ?? ['edit']).includes('edit')),
  };
}

export function utilityRecord(spec: UtilityRecordSpec): CapabilityRecordSource {
  const effect = spec.effect ?? 'write';
  const inputFields = ['action', ...(spec.params ?? [])];
  const required = ['action', ...(spec.required ?? [])];
  const outputFields = ['success', 'message', 'details', ...(spec.outputs ?? [])];
  const outputRequired = ['success', ...(spec.outputRequired ?? [])];
  return {
    id: CapabilityIdSchema.parse(`${spec.tool}.${spec.action}`),
    aliases: [],
    legacyIds: [{
      tool: LegacyToolNameSchema.parse(spec.tool),
      action: LegacyActionNameSchema.parse(spec.action),
    }],
    discovery: {
      domain: spec.tool === 'manage_audio' ? 'audio' : 'networking',
      family: spec.family,
      topics: [spec.action, ...(spec.topics ?? [])],
      summary: spec.summary,
      whenToUse: [`Use when ${spec.summary.toLowerCase()}`],
      whenNotToUse: ['Do not use when the required Unreal capability or target is unavailable.'],
    },
    schemas: {
      input: schema(inputFields, required, spec.requiredOneOf),
      output: outputSchema(outputFields, outputRequired),
    },
    examples: [{
      title: spec.summary,
      input: buildExampleInput(spec.action, spec.family, required, spec.requiredOneOf),
      output: buildExampleOutput(spec.action, spec.family, outputRequired),
    }],
    availability: {
      unreal: { min: V5_0, max: V5_8_P1 },
      requiredPlugins: [...(spec.plugins ?? [])],
      editorStates: [...(spec.states ?? ['edit'])],
    },
    behavior: behavior(spec),
    policy: {
      requiredScope: effect,
      consent: effect === 'destructive' ? 'explicit' : 'none',
      dataAccess: effect === 'read' ? 'project-read' : 'project-write',
    },
    cost: { latency: 'interactive', resources: spec.resources ?? 'low' },
    routing: {
      parentTool: LegacyToolNameSchema.parse(spec.tool),
      dispatchAction: LegacyActionNameSchema.parse(spec.dispatchAction ?? spec.tool),
      dispatchMode: 'tool',
    },
    normalization: {
      class: 'C_SAME_VERB_DIFFERENT_TARGET',
      disposition: 'retain',
      rationale: `Distinct ${spec.family} capability routed through ${spec.dispatchAction ?? spec.tool}.`,
    },
    deprecation: { status: 'active' },
    parent: getParentToolMetadata(spec.tool),
  };
}

/**
 * Append retrieval vocabulary to a record built by a positional wrapper. Topics are
 * the strongest free-text ranking field on both gateways, so this is where the words
 * a caller types ('play sound', 'replicate variable') are attached when the builder
 * signature has no room for them.
 */
export function withTopics(record: CapabilityRecordSource, topics: readonly string[]): CapabilityRecordSource {
  return { ...record, discovery: { ...record.discovery, topics: [...record.discovery.topics, ...topics] } };
}

/** Declare alternate ids for a positional-wrapper record; they resolve on describe/execute and rank as the record's own names. */
export function withAliases(record: CapabilityRecordSource, aliases: readonly string[]): CapabilityRecordSource {
  return { ...record, aliases: [...record.aliases, ...aliases.map((alias) => CapabilityAliasSchema.parse(alias))] };
}
