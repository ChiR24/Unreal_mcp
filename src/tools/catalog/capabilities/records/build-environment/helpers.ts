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
  CapabilityBehavior,
  CapabilityPolicy,
  CapabilityRecordSource,
  CapabilityRouting,
  CapabilitySchemas,
  Draft202012ObjectSchema,
  JsonObject,
} from '../../index.js';
import type { PropertyMap } from './properties.js';

const SCHEMA_URI = 'https://json-schema.org/draft/2020-12/schema';
const V5_0 = { major: 5 as const, minor: 0, patch: 0, channel: 'stable' as const };
const V5_8_P1 = { major: 5 as const, minor: 8, patch: 0, channel: 'preview' as const, preview: 1 };

export function schema(properties: PropertyMap, required: readonly string[]): Draft202012ObjectSchema {
  return {
    $schema: SCHEMA_URI,
    type: 'object',
    properties: properties as unknown as JsonObject,
    required: [...required],
    additionalProperties: false,
  };
}

function outputSchema(props: PropertyMap, required: readonly string[]): Draft202012ObjectSchema {
  const full: PropertyMap = {
    success: { type: 'boolean', description: 'Whether the action succeeded.' },
    message: { type: 'string', description: 'Human-readable result message.' },
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

function behavior(effect: EffectType, opts: Partial<CapabilityBehavior> = {}): CapabilityBehavior {
  const isWrite = effect !== 'read';
  return {
    effect,
    idempotency: opts.idempotency ?? (effect === 'read' ? 'idempotent' : 'non-idempotent'),
    longRunning: opts.longRunning ?? false,
    safeToRetry: opts.safeToRetry ?? effect === 'read',
    supportsPreview: opts.supportsPreview ?? false,
    supportsUndo: opts.supportsUndo ?? isWrite,
  };
}

function policy(effect: EffectType): CapabilityPolicy {
  return {
    requiredScope: effect,
    consent: effect === 'destructive' ? 'explicit' : 'none',
    dataAccess: effect === 'read' ? 'project-read' : 'project-write',
  };
}

function routing(dispatchAction: string, dispatchMode: 'tool' | 'action' | 'local' = 'tool'): CapabilityRouting {
  return {
    parentTool: 'build_environment' as unknown as CapabilityRouting['parentTool'],
    dispatchAction: dispatchAction as unknown as CapabilityRouting['dispatchAction'],
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
  readonly outputProps?: PropertyMap;
  readonly outputRequired?: readonly string[];
  readonly effect: EffectType;
  readonly behavior?: Partial<CapabilityBehavior>;
  readonly latency: 'instant' | 'interactive' | 'long-running';
  readonly resources: 'low' | 'medium' | 'high';
  readonly plugins?: readonly string[];
  readonly editorStates?: readonly ('edit' | 'pie' | 'simulate')[];
  readonly dispatchAction?: string;
  readonly dispatchMode?: 'tool' | 'action' | 'local';
  readonly exampleInput: JsonObject;
  readonly exampleOutput: JsonObject;
  readonly aliases?: readonly string[];
}

const NR = 'Distinct build_environment target and semantics; no cross-tool duplicate.';

export function buildRecord(spec: RecordSpec): CapabilityRecordSource {
  const input = schema(spec.inputProps, spec.required);
  const output = spec.outputProps
    ? outputSchema(spec.outputProps, spec.outputRequired ?? [])
    : EMPTY_OUTPUT;
  return {
    id: spec.id as unknown as CapabilityRecordSource['id'],
    aliases: (spec.aliases ?? []) as unknown as CapabilityRecordSource['aliases'],
    legacyIds: [{ tool: 'build_environment', action: spec.action }] as unknown as CapabilityRecordSource['legacyIds'],
    discovery: {
      domain: 'environment',
      family: spec.family,
      topics: [spec.action],
      summary: spec.summary,
      whenToUse: [...spec.whenToUse],
      whenNotToUse: [...spec.whenNotToUse],
    },
    schemas: { input, output } as unknown as CapabilitySchemas,
    examples: [{ title: spec.summary, input: spec.exampleInput, output: spec.exampleOutput }],
    availability: availability(spec.plugins, spec.editorStates),
    behavior: behavior(spec.effect, spec.behavior),
    policy: policy(spec.effect),
    cost: { latency: spec.latency, resources: spec.resources },
    routing: routing(spec.dispatchAction ?? spec.action, spec.dispatchMode),
    normalization: { class: 'C_SAME_VERB_DIFFERENT_TARGET', disposition: 'retain', rationale: NR },
    deprecation: { status: 'active' },
  };
}

export { availability, behavior, outputSchema, policy, routing, V5_0, V5_8_P1 };
