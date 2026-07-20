/**
 * Local builders for manage_blueprint capability records.
 *
 * Private to the manage-blueprint pilot. Constructs the boilerplate portions of
 * a CapabilityRecordSource (schemas, availability, behavior, policy, cost,
 * routing, normalization) so each family file declares only what varies.
 *
 * Does NOT touch the shared capability model, schema, or generator.
 *
 * Grounded in: src/tools/definitions/core/blueprint/manage-blueprint-tool.ts,
 * src/tools/handlers/blueprint/, native BlueprintGraph/WidgetAuthoring domains,
 * SCS safety rules (SCS->CreateNode/AddNode template ownership), and the
 * normalization inventory (104 manage_blueprint occurrences, all classification
 * C, disposition keep).
 */
import type {
  CapabilityAvailability,
  CapabilityBehavior,
  CapabilityPolicy,
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
import type { PropertyMap } from './properties.js';
import { getParentToolMetadata } from '../parent-metadata.js';

const SCHEMA_URI = 'https://json-schema.org/draft/2020-12/schema';

const V5_0 = { major: 5 as const, minor: 0, patch: 0, channel: 'stable' as const };
const V5_8_P1 = { major: 5 as const, minor: 8, patch: 0, channel: 'preview' as const, preview: 1 };

export function schema(properties: PropertyMap, required: readonly string[]): Draft202012ObjectSchema {
  return {
    $schema: SCHEMA_URI,
    type: 'object',
    properties: properties,
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

// UMG requires the UMG plugin; Blueprint core needs only EditorScriptingUtilities.
const BP_PLUGINS = ['EditorScriptingUtilities'];
const WIDGET_PLUGINS = ['EditorScriptingUtilities', 'UMG'];

function availability(
  requiredPlugins: readonly string[] = BP_PLUGINS,
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
    parentTool: LegacyToolNameSchema.parse('manage_blueprint'),
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

const NR = 'Distinct manage_blueprint capability with unique target, schema, and policy.';

export function buildRecord(spec: RecordSpec): CapabilityRecordSource {
  const input = schema(spec.inputProps, spec.required);
  const output = spec.outputProps
    ? outputSchema(spec.outputProps, spec.outputRequired ?? [])
    : EMPTY_OUTPUT;
  return {
    id: CapabilityIdSchema.parse(spec.id),
    aliases: (spec.aliases ?? []).map((alias) => CapabilityAliasSchema.parse(alias)),
    legacyIds: [{ tool: LegacyToolNameSchema.parse('manage_blueprint'), action: LegacyActionNameSchema.parse(spec.action) }],
    discovery: {
      domain: spec.domain,
      family: spec.family,
      topics: [spec.action],
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
    parent: getParentToolMetadata('manage_blueprint'),
  };
}

export { availability, BP_PLUGINS, behavior, outputSchema, policy, routing, V5_0, V5_8_P1, WIDGET_PLUGINS };
