import type {
  CapabilityBehavior,
  CapabilityRecordSource,
  Draft202012ObjectSchema,
  JsonObject,
} from '../../index.js';
import {
  CapabilityIdSchema,
  LegacyActionNameSchema,
  LegacyToolNameSchema,
} from '../../index.js';
import { getParentToolMetadata } from '../parent-metadata.js';
import { outputProperty } from './output-glossary.js';
import { buildExampleInput, buildExampleOutput } from './example-values.js';

const SCHEMA_URI = 'https://json-schema.org/draft/2020-12/schema';
const V5_0 = { major: 5 as const, minor: 0, patch: 0, channel: 'stable' as const };
const V5_8_P1 = { major: 5 as const, minor: 8, patch: 0, channel: 'preview' as const, preview: 1 };

const BOOLEAN_FIELDS = new Set([
  'enabled', 'enable', 'replicated', 'reliable', 'withValidation', 'alwaysRelevant',
  'onlyRelevantToOwner', 'usePushModel', 'enablePrediction', 'replicateMovement',
  'autoPlay', 'looping', 'save', 'muted', 'voiceEnabled', 'pushToTalkEnabled',
  'bIsLANMatch', 'bAllowJoinInProgress', 'bAllowInvites', 'bUsesPresence',
  'bUseLobbiesIfAvailable', 'bShouldAdvertise', 'executeTravel', 'forceRespawn',
  'canRespawn', 'systemWide',
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
  'respawnDelay', 'teamIndex', 'scale', 'timeoutMs',
  'lowPassFilterFrequency', 'maxRespawns', 'localPlayerNum',
]);
const OBJECT_FIELDS = new Set(['location', 'rotation', 'size', 'properties', 'settings', 'voiceSettings']);
const ARRAY_FIELDS = new Set(['states', 'sessions', 'players', 'mappings']);

// INPUT-ONLY. Keyed on bare field names, so adding an envelope field such as
// `success` here would silently retype any input sharing that name. The output
// envelope is built explicitly in ./output-glossary.js instead.
function property(name: string): JsonObject {
  if (BOOLEAN_FIELDS.has(name)) return { type: 'boolean', description: name };
  if (NUMBER_FIELDS.has(name)) return { type: 'number', description: name };
  if (OBJECT_FIELDS.has(name)) {
    return {
      type: 'object',
      description: name,
      additionalProperties: true,
      'x-unreal-reflection-boundary': true,
    };
  }
  if (ARRAY_FIELDS.has(name)) return { type: 'array', description: name, items: {} };
  return { type: 'string', description: name };
}

function schema(fields: readonly string[], required: readonly string[]): Draft202012ObjectSchema {
  const properties: Record<string, JsonObject> = {};
  for (const field of fields) properties[field] = property(field);
  return {
    $schema: SCHEMA_URI,
    type: 'object',
    properties,
    required: [...required],
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
  readonly params?: readonly string[];
  readonly required?: readonly string[];
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

function behavior(spec: UtilityRecordSpec): CapabilityBehavior {
  const effect = spec.effect ?? 'write';
  return {
    effect,
    idempotency: effect === 'read' ? 'idempotent' : 'non-idempotent',
    longRunning: false,
    safeToRetry: spec.safeToRetry ?? effect === 'read',
    supportsPreview: false,
    supportsUndo: spec.supportsUndo ?? (effect === 'write' && (spec.states ?? ['edit']).includes('edit')),
  };
}

export function utilityRecord(spec: UtilityRecordSpec): CapabilityRecordSource {
  const effect = spec.effect ?? 'write';
  const inputFields = ['action', ...(spec.params ?? [])];
  const required = ['action', ...(spec.required ?? [])];
  const outputFields = ['success', 'message', ...(spec.outputs ?? [])];
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
      topics: [spec.action],
      summary: spec.summary,
      whenToUse: [`Use when ${spec.summary.toLowerCase()}`],
      whenNotToUse: ['Do not use when the required Unreal capability or target is unavailable.'],
    },
    schemas: {
      input: schema(inputFields, required),
      output: outputSchema(outputFields, outputRequired),
    },
    examples: [{
      title: spec.summary,
      input: buildExampleInput(spec.action, spec.family, required),
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
