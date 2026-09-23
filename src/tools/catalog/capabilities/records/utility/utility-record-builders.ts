// The `utilityRecord` spec builder behind the utility-lane records.
//
// Assembles a `CapabilityRecordSource` from the deltas in a `UtilityRecordSpec`
// so the positional family files (`manage-audio/authoring.data.ts`,
// `manage-networking/framework.data.ts`, ...) declare only what varies. The
// sections that used to live in the same file - input field pins and the
// Draft-2020-12 schema assembly - are in `utility-schema-pins.ts` and
// `utility-output-schema.ts`; this module owns the record body, the
// effect-derived behavior (the shared `behavior()` preset narrows `undo` for
// run-anywhere capabilities, so this one stays local) and the two wrappers.

import { V5_0, V5_8_P1 } from '../shared/record-presets.js';
import type { CapabilityBehaviorSource, CapabilityRecordSource, JsonObject } from '../../index.js';
import {
  CapabilityAliasSchema,
  CapabilityIdSchema,
  LegacyActionNameSchema,
  LegacyToolNameSchema,
} from '../../index.js';
import { getParentToolMetadata } from '../parent-metadata.js';
import { inputSchema, outputSchema } from './utility-output-schema.js';
import { buildExampleInput, buildExampleOutput } from './example-values.js';

type Effect = 'read' | 'write' | 'destructive';

/** The delta a utility family file declares for one action. */
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

/** Fills the boilerplate portions of a utility capability record. */
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
      input: inputSchema(inputFields, required, spec.requiredOneOf),
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

/**
 * Replace or add input properties the name-keyed pins cannot express: a field
 * whose type depends on its target (a MetaSound literal), or one no other
 * utility record shares.
 */
export function withInputProps(record: CapabilityRecordSource, props: Readonly<Record<string, JsonObject>>): CapabilityRecordSource {
  const input = record.schemas.input;
  return { ...record, schemas: { ...record.schemas, input: { ...input, properties: { ...input.properties, ...props } } } };
}

/** Mark a record added after the gateway migration, so the normalization audit skips it. */
export function asPostMigration(record: CapabilityRecordSource, rationale: string): CapabilityRecordSource {
  return { ...record, normalization: { ...record.normalization, rationale, provenance: 'post-migration' } };
}

/** Declare alternate ids for a positional-wrapper record; they resolve on describe/execute and rank as the record's own names. */
export function withAliases(record: CapabilityRecordSource, aliases: readonly string[]): CapabilityRecordSource {
  return { ...record, aliases: [...record.aliases, ...aliases.map((alias) => CapabilityAliasSchema.parse(alias))] };
}
