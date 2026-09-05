/**
 * Generic core-only builder for CapabilityRecordSource values.
 *
 * Constructs the boilerplate portions of a CapabilityRecordSource (schemas,
 * availability, behavior, policy, cost, routing, normalization, deprecation)
 * for arbitrary core parent tools (control_actor, control_editor,
 * manage_level, system_control, inspect, manage_tools) so each worker declares
 * only what varies. Does NOT touch frozen pilot builders, the shared model,
 * schema, generator, or any aggregate/retrieval code.
 */
import type {
  CapabilityAvailability,
  CapabilityBehaviorSource,
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
import { getParentToolMetadata } from '../parent-metadata.js';
import { policy, behavior } from '../shared/record-presets.js';

const SCHEMA_URI = 'https://json-schema.org/draft/2020-12/schema' as const;

const V5_0 = { major: 5 as const, minor: 0, patch: 0, channel: 'stable' as const };
const V5_8_P1 = { major: 5 as const, minor: 8, patch: 0, channel: 'preview' as const, preview: 1 };

type EffectType = 'read' | 'write' | 'destructive';
type EditorState = 'edit' | 'pie' | 'simulate';

export type CoreRecordSpec = {
  readonly parentTool: string;
  readonly action: string;
  readonly dispatchAction?: string;
  readonly dispatchMode?: 'tool' | 'action' | 'local';
  readonly domain: string;
  readonly family: string;
  readonly summary: string;
  readonly whenToUse: readonly string[];
  readonly whenNotToUse: readonly string[];
  readonly inputProps: JsonObject;
  readonly required: readonly string[];
  readonly requiredOneOf?: readonly string[];
  readonly outputProps?: JsonObject;
  readonly outputRequired?: readonly string[];
  readonly effect: EffectType;
  readonly behavior?: Partial<CapabilityBehaviorSource>;
  /** Optional policy overrides on top of the effect-derived preset. */
  readonly policyOverride?: Partial<CapabilityPolicy>;
  readonly costLatency: 'instant' | 'interactive' | 'long-running';
  readonly costResources: 'low' | 'medium' | 'high';
  readonly plugins?: readonly string[];
  readonly editorStates?: readonly EditorState[];
  readonly normalizationClass: CapabilityRecordSource['normalization']['class'];
  readonly normalizationDisposition?: CapabilityRecordSource['normalization']['disposition'];
  readonly normalizationRationale: string;
  readonly normalizationAliasOf?: string;
  readonly aliases?: readonly string[];
  readonly topics?: readonly string[];
  readonly exampleInput: JsonObject;
  readonly exampleOutput: JsonObject;
};

const ACTION_PROP: JsonObject = {
  type: 'string',
  description: 'The action to execute on the parent tool.',
};

function schema(
  properties: JsonObject,
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

function outputSchema(props: JsonObject, required: readonly string[]): Draft202012ObjectSchema {
  const full: JsonObject = {
    success: { type: 'boolean', description: 'Whether the action succeeded.' },
    message: { type: 'string', description: 'Human-readable result message.' },
    // Every contract carries a `details` reflection boundary: both gateways fold
    // handler fields the contract does not name into it, so a read action's
    // payload survives projection instead of collapsing to a bare success.
    details: {
      type: 'object',
      'x-unreal-reflection-boundary': true,
      description: 'Additional handler result fields not named by the contract.',
    },
    ...props,
  };
  return schema(full, ['success', ...required]);
}

const EMPTY_OUTPUT = outputSchema({}, []);

function availability(
  requiredPlugins: readonly string[] = [],
  editorStates: readonly EditorState[] = ['edit'],
): CapabilityAvailability {
  return {
    unreal: { min: V5_0, max: V5_8_P1 },
    requiredPlugins: [...requiredPlugins],
    editorStates: [...editorStates],
  };
}



function routing(
  parentTool: string,
  dispatchAction: string,
  dispatchMode: 'tool' | 'action' | 'local' = 'tool',
): CapabilityRouting {
  return {
    parentTool: LegacyToolNameSchema.parse(parentTool),
    dispatchAction: LegacyActionNameSchema.parse(dispatchAction),
    dispatchMode,
  };
}

/**
 * Build a CapabilityRecordSource for any core parent tool from a CoreRecordSpec.
 * The canonical id is `<parentTool>.<action>`; aliases are branded separately.
 *
 * Every record is stamped with canonical parent metadata (description + category)
 * resolved by `routing.parentTool`, so the data files never duplicate the
 * parent's description or category locally.
 */
export function buildCoreRecord(
  spec: CoreRecordSpec,
): CapabilityRecordSource {
  const required = [...new Set(['action', ...spec.required])];
  const input = schema({ action: ACTION_PROP, ...spec.inputProps }, required, spec.requiredOneOf);
  const output = spec.outputProps
    ? outputSchema(spec.outputProps, spec.outputRequired ?? [])
    : EMPTY_OUTPUT;
  return {
    id: CapabilityIdSchema.parse(`${spec.parentTool}.${spec.action}`),
    aliases: (spec.aliases ?? []).map((alias) => CapabilityAliasSchema.parse(alias)),
    legacyIds: [
      { tool: LegacyToolNameSchema.parse(spec.parentTool), action: LegacyActionNameSchema.parse(spec.action) },
    ],
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
    policy: { ...policy(spec.effect), ...(spec.policyOverride ?? {}) },
    cost: { latency: spec.costLatency, resources: spec.costResources },
    routing: routing(spec.parentTool, spec.dispatchAction ?? spec.action, spec.dispatchMode),
    normalization: {
      class: spec.normalizationClass,
      disposition: spec.normalizationDisposition ?? 'retain',
      rationale: spec.normalizationRationale,
      ...(spec.normalizationAliasOf === undefined
        ? {}
        : { aliasOf: CapabilityIdSchema.parse(spec.normalizationAliasOf) }),
    },
    deprecation: { status: 'active' },
    parent: getParentToolMetadata(spec.parentTool),
  };
}
