/**
 * Generic core-only builder for CapabilityRecordSource values.
 *
 * Constructs the boilerplate portions of a CapabilityRecordSource (schemas,
 * availability, behavior, policy, cost, routing, deprecation)
 * for arbitrary core parent tools (control_actor, control_editor,
 * manage_level, system_control, inspect, manage_tools) so each worker declares
 * only what varies. Does NOT touch frozen pilot builders, the shared model,
 * schema, generator, or any aggregate/retrieval code.
 */
import type { CapabilityAvailability, CapabilityBehaviorSource, CapabilityPolicy, CapabilityRecordSource, CapabilityRouting, JsonObject } from '../../model.js';
import { CapabilityAliasSchema, CapabilityIdSchema, LegacyActionNameSchema, LegacyToolNameSchema } from '../../identifiers.js';
import { actionInputSchema, behavior, EMPTY_OUTPUT, outputSchema, policy, schema } from '../shared/record-presets.js';

type EffectType = 'read' | 'write' | 'destructive';
type EditorState = 'edit' | 'pie' | 'simulate';

export type CoreRecordSpec = {
  readonly parentTool: string;
  readonly action: string;
  /** Defaults to `<parentTool>.<action>`. */
  readonly id?: string;
  /** Input schema without the `action` discriminator (the gameplay parents). */
  readonly bareInput?: boolean;
  readonly dispatchAction?: string;
  readonly domain: string;
  readonly family: string;
  readonly summary: string;
  readonly whenToUse: readonly string[];
  readonly whenNotToUse: readonly string[];
  readonly inputProps: JsonObject;
  readonly required?: readonly string[];
  readonly requiredOneOf?: readonly string[];
  readonly outputProps?: JsonObject;
  readonly outputRequired?: readonly string[];
  readonly effect: EffectType;
  readonly behavior?: Partial<CapabilityBehaviorSource>;
  /** Optional policy overrides on top of the effect-derived preset. */
  readonly policyOverride?: Partial<CapabilityPolicy>;
  /** Defaults to 'instant'. */
  readonly costLatency?: 'instant' | 'interactive' | 'long-running';
  /** Defaults to 'low'. */
  readonly costResources?: 'low' | 'medium' | 'high';
  readonly plugins?: readonly string[];
  readonly editorStates?: readonly EditorState[];
  readonly aliases?: readonly string[];
  readonly topics?: readonly string[];
  readonly exampleInput: JsonObject;
  /** Defaults to { success: true }; write it only when the reply carries more. */
  readonly exampleOutput?: JsonObject;
};

function availability(
  requiredPlugins: readonly string[] = [],
  editorStates: readonly EditorState[] = ['edit'],
): CapabilityAvailability {
  return {
    requiredPlugins: [...requiredPlugins],
    editorStates: [...editorStates],
  };
}

function routing(
  parentTool: string,
  dispatchAction: string,
): CapabilityRouting {
  return {
    parentTool: LegacyToolNameSchema.parse(parentTool),
    dispatchAction: LegacyActionNameSchema.parse(dispatchAction),
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
  const input = spec.bareInput
    ? schema(spec.inputProps, spec.required, spec.requiredOneOf)
    : actionInputSchema(spec.inputProps, spec.required, spec.requiredOneOf);
  const output = spec.outputProps
    ? outputSchema(spec.outputProps, spec.outputRequired ?? [])
    : EMPTY_OUTPUT;
  return {
    id: CapabilityIdSchema.parse(spec.id ?? `${spec.parentTool}.${spec.action}`),
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
    examples: [{ title: spec.summary, input: spec.exampleInput, output: spec.exampleOutput ?? { success: true } }],
    availability: availability(spec.plugins, spec.editorStates),
    behavior: behavior(spec.effect, spec.behavior),
    policy: { ...policy(spec.effect), ...(spec.policyOverride ?? {}) },
    cost: { latency: spec.costLatency ?? 'instant', resources: spec.costResources ?? 'low' },
    routing: routing(spec.parentTool, spec.dispatchAction ?? spec.action),
  };
}
