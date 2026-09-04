/**
 * Local builder for gameplay capability records.
 *
 * Private to the gameplay domain. Constructs boilerplate portions of a
 * CapabilityRecordSource so each family file declares only what varies.
 * Does NOT touch the shared capability model, schema, generator, or any
 * aggregate code outside this directory.
 *
 * Grounded in: src/tools/definitions/gameplay/*, src/tools/definitions/utility/
 * manage-effect-tool.ts, the per-tool handler bodies under src/tools/handlers/
 * {animation,skeleton,gas,character,combat,ai,inventory,interaction}/, the native
 * Gameplay/Effect/GAS/AI/Skeleton/Character/Combat/Inventory/Interaction domains
 * under plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/,
 * and the route-disposition ledger in
 * src/tools/catalog/capabilities/normalization/routedispositions-*.data.ts
 * (authoritative source for the 16 skeleton + 4 GAS + 3 AI hidden routes).
 */
import type {
  CapabilityAvailability,
  CapabilityBehaviorSource,
  CapabilityDeprecation,
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
    // Both gateways fold handler fields the contract does not name into this
    // boundary, so a read action's payload survives projection.
    details: {
      type: 'object',
      'x-unreal-reflection-boundary': true,
      description: 'Additional handler result fields not named by the contract.',
    },
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

export interface RecordSpec {
  readonly parentTool: string;
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
  readonly normalizationClass?: CapabilityRecordSource['normalization']['class'];
  readonly normalizationDisposition?: CapabilityRecordSource['normalization']['disposition'];
  readonly normalizationRationale?: string;
  readonly normalizationAliasOf?: string;
  readonly normalizationProvenance?: CapabilityRecordSource['normalization']['provenance'];
  readonly deprecation?: CapabilityDeprecation;
  readonly aliases?: readonly string[];
  readonly exampleInput: JsonObject;
  readonly exampleOutput: JsonObject;
}

export function buildRecord(spec: RecordSpec): CapabilityRecordSource {
  const inputProperties: PropertyMap = Object.fromEntries(
    Object.entries(spec.inputProps).filter(([name]) => name !== 'action'),
  );
  const input = schema(inputProperties, spec.required.filter((name) => name !== 'action'), spec.requiredOneOf);
  const output = spec.outputProps
    ? outputSchema(spec.outputProps, spec.outputRequired ?? [])
    : EMPTY_OUTPUT;
  return {
    id: CapabilityIdSchema.parse(spec.id),
    aliases: (spec.aliases ?? []).map((alias) => CapabilityAliasSchema.parse(alias)),
    legacyIds: [
      { tool: LegacyToolNameSchema.parse(spec.parentTool), action: LegacyActionNameSchema.parse(spec.action) },
    ],
    discovery: {
      domain: spec.parentTool.replace(/_/g, ' '),
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
    routing: routing(spec.parentTool, spec.dispatchAction ?? spec.action, spec.dispatchMode),
    normalization: {
      class: spec.normalizationClass ?? 'C_SAME_VERB_DIFFERENT_TARGET',
      disposition: spec.normalizationDisposition ?? 'retain',
      rationale: spec.normalizationRationale ?? 'Distinct gameplay target; no cross-tool duplicate.',
      ...(spec.normalizationAliasOf === undefined
        ? {}
        : { aliasOf: CapabilityIdSchema.parse(spec.normalizationAliasOf) }),
      ...(spec.normalizationProvenance === undefined
        ? {}
        : { provenance: spec.normalizationProvenance }),
    },
    deprecation: spec.deprecation ?? { status: 'active' },
    parent: getParentToolMetadata(spec.parentTool),
  };
}

export { availability, behavior, outputSchema, policy, routing, V5_0, V5_8_P1 };
