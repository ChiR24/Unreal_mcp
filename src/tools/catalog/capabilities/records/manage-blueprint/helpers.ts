/**
 * Local builders for manage_blueprint capability records.
 *
 * Private to the manage-blueprint pilot. Constructs the boilerplate portions of
 * a CapabilityRecordSource (schemas, availability, behavior, policy, cost,
 * routing, normalization) so each family file declares only what varies.
 *
 * Grounded in: src/tools/definitions/core/blueprint/manage-blueprint-tool.ts,
 * src/tools/handlers/blueprint/, native BlueprintGraph/WidgetAuthoring domains,
 * SCS safety rules (SCS->CreateNode/AddNode template ownership), and the
 * normalization inventory (104 manage_blueprint occurrences, all classification
 * C, disposition keep).
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
import type { PropertyMap } from './properties.js';
import { getParentToolMetadata } from '../parent-metadata.js';
import { policy, behavior, SCHEMA_URI, V5_0, V5_8_P1 } from '../shared/record-presets.js';



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
    // Handlers report more than the contract names; the gateways fold those fields here
    // instead of dropping them (dogfood: thin reads such as #28/#210).
    details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Additional handler result fields not named by the contract.' },
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
    parent: getParentToolMetadata('manage_blueprint'),
  };
}

/**
 * A capability authored after the gateway migration.
 *
 * `legacyIds` still carries the pair the parent action enum and the gateway
 * route resolve through, but that pair never shipped on the pre-gateway
 * surface, so `provenance` keeps `extractOccurrences()` from counting it into
 * an audit of what did.
 */
export function buildPromotedRecord(spec: RecordSpec, rationale: string): CapabilityRecordSource {
  return {
    ...buildRecord(spec),
    normalization: {
      class: 'C_SAME_VERB_DIFFERENT_TARGET',
      disposition: 'retain',
      rationale,
      provenance: 'post-migration',
    },
  };
}

export { BP_PLUGINS, WIDGET_PLUGINS };
