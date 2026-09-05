/**
 * Local builder for build_environment capability records.
 *
 * Private to the build-environment pilot. Constructs boilerplate portions of a
 * CapabilityRecordSource so each family file declares only what varies.
 * Does NOT touch the shared capability model, schema, or generator.
 *
 * Grounded in: src/tools/definitions/world/build-environment-tool.ts,
 * src/tools/handlers/environment/, native Environment domain handlers, and
 * the normalization inventory (150 build_environment occurrences, all
 * classification C, disposition keep).
 */
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
import { getParentToolMetadata } from '../parent-metadata.js';
import type { PropertyMap } from './properties.js';
import { policy, behavior } from '../shared/record-presets.js';

const SCHEMA_URI = 'https://json-schema.org/draft/2020-12/schema';
const V5_0 = { major: 5 as const, minor: 0, patch: 0, channel: 'stable' as const };
const V5_8_P1 = { major: 5 as const, minor: 8, patch: 0, channel: 'preview' as const, preview: 1 };

export function schema(
  properties: PropertyMap,
  required: readonly string[],
  requiredOneOf?: readonly string[],
): Draft202012ObjectSchema {
  return {
    $schema: SCHEMA_URI,
    type: 'object',
    properties,
    required: [...required],
    additionalProperties: false,
    ...(requiredOneOf === undefined ? {} : { requiredOneOf: [...requiredOneOf] }),
  };
}

function outputSchema(props: PropertyMap, required: readonly string[]): Draft202012ObjectSchema {
  const full: PropertyMap = {
    success: { type: 'boolean', description: 'Whether the action succeeded.' },
    message: { type: 'string', description: 'Human-readable result message.' },
    // Handlers report more than the contract names; the gateways fold those fields here
    // instead of dropping them (dogfood: thin reads such as #28/#210).
    details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Additional handler result fields not named by the contract.' },
    ...props,
  };
  return schema(full, ['success', ...required]);
}

export const EMPTY_OUTPUT = outputSchema({}, []);

function availability(
  requiredPlugins: readonly string[] = [],
  editorStates: readonly ('edit' | 'pie' | 'simulate')[] = ['edit'],
): CapabilityAvailability {
  return {
    unreal: { min: V5_0, max: V5_8_P1 },
    requiredPlugins: [...requiredPlugins],
    editorStates: [...editorStates],
  };
}

type EffectType = 'read' | 'write' | 'destructive';

function routing(dispatchAction: string, dispatchMode: 'tool' | 'action' | 'local' = 'tool'): CapabilityRouting {
  return {
    parentTool: LegacyToolNameSchema.parse('build_environment'),
    dispatchAction: LegacyActionNameSchema.parse(dispatchAction),
    dispatchMode,
  };
}

export interface RecordSpec {
  readonly id: string;
  readonly action: string;
  readonly family: string;
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
  readonly plugins?: readonly string[];
  readonly editorStates?: readonly ('edit' | 'pie' | 'simulate')[];
  readonly dispatchAction?: string;
  readonly dispatchMode?: 'tool' | 'action' | 'local';
  readonly exampleInput: JsonObject;
  readonly exampleOutput: JsonObject;
  readonly aliases?: readonly string[];
  readonly topics?: readonly string[];
}

const NR = 'Distinct build_environment target and semantics; no cross-tool duplicate.';

export function buildRecord(
  spec: RecordSpec,
): CapabilityRecordSource {
  const input = schema(spec.inputProps, spec.required, spec.requiredOneOf);
  const output = spec.outputProps
    ? outputSchema(spec.outputProps, spec.outputRequired ?? [])
    : EMPTY_OUTPUT;
  return {
    id: CapabilityIdSchema.parse(spec.id),
    aliases: (spec.aliases ?? []).map((alias) => CapabilityAliasSchema.parse(alias)),
    legacyIds: [{ tool: LegacyToolNameSchema.parse('build_environment'), action: LegacyActionNameSchema.parse(spec.action) }],
    discovery: {
      domain: 'environment',
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
    cost: { latency: spec.latency, resources: spec.resources },
    routing: routing(spec.dispatchAction ?? spec.action, spec.dispatchMode),
    normalization: { class: 'C_SAME_VERB_DIFFERENT_TARGET', disposition: 'retain', rationale: NR },
    deprecation: { status: 'active' },
    parent: getParentToolMetadata('build_environment'),
  };
}

export { availability, behavior, outputSchema, policy, routing, V5_0, V5_8_P1 };
