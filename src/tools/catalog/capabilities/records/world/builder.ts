/**
 * Shared builder for world-domain capability records (manage_level_structure,
 * manage_geometry, manage_pcg).
 *
 * Constructs the boilerplate portions of a CapabilityRecordSource (schemas,
 * availability with optional UE version/plugin gates, behavior with async PCG
 * task contracts, policy, cost, routing, normalization, deprecation) so each
 * family file declares only what varies. Does NOT touch frozen pilot builders
 * (build-environment), the shared model, schema, generator, or any aggregate.
 *
 * Grounded in: src/tools/definitions/world/{manage-level-structure,
 * manage-geometry,manage-pcg}-tool.ts, world handler routing, native World
 * domain dispatch, the GeometryScripting/PCG Build.cs probes, and Water runtime
 * detection notes from the world closeout evidence.
 */
import type {
  CapabilityAvailability,
  CapabilityBehaviorSource,
  CapabilityRecordSource,
  CapabilityRouting,
  Draft202012ObjectSchema,
  JsonObject,
  UnrealVersion,
} from '../../index.js';
import {
  CapabilityAliasSchema,
  CapabilityIdSchema,
  LegacyActionNameSchema,
  LegacyToolNameSchema,
} from '../../index.js';
import { getParentToolMetadata } from '../parent-metadata.js';
import { policy, behavior, SCHEMA_URI, V5_0, V5_8_P1 } from '../shared/record-presets.js';



type EffectType = 'read' | 'write' | 'destructive';
type EditorState = 'edit' | 'pie' | 'simulate';

export type WorldRecordSpec = {
  readonly parentTool: 'manage_level_structure' | 'manage_geometry' | 'manage_pcg';
  readonly action: string;
  readonly dispatchAction?: string;
  readonly dispatchMode?: 'tool' | 'action' | 'local';
  readonly family: string;
  readonly summary: string;
  readonly whenToUse: readonly string[];
  readonly whenNotToUse: readonly string[];
  readonly inputProps: JsonObject;
  readonly required: readonly string[];
  /** At-least-one group, for actions that accept either of two alias names. */
  readonly requiredOneOf?: readonly string[];
  readonly outputProps?: JsonObject;
  readonly outputRequired?: readonly string[];
  readonly effect: EffectType;
  readonly behavior?: Partial<CapabilityBehaviorSource>;
  readonly costLatency: 'instant' | 'interactive' | 'long-running';
  readonly costResources: 'low' | 'medium' | 'high';
  readonly plugins?: readonly string[];
  readonly editorStates?: readonly EditorState[];
  readonly unrealMin?: UnrealVersion;
  readonly unrealMax?: UnrealVersion;
  readonly aliases?: readonly string[];
  readonly topics?: readonly string[];
  readonly normalizationRationale: string;
  readonly normalizationProvenance?: CapabilityRecordSource['normalization']['provenance'];
  /**
   * Old action names this record replaced. Each stays callable by its own
   * name: the pins are the selector values that name implied, injected before
   * validation, and the old action is what the bridge receives.
   */
  readonly folded?: readonly FoldedActionSpec[];
  /** Selector value -> bridge action, for a call that names this record's own action. */
  readonly dispatchBy?: { readonly param: string; readonly actions: Readonly<Record<string, string>> };
  /**
   * Set when the record's own action is new while its folded pairs shipped
   * pre-gateway: the audit then skips the new pair and keeps counting the old.
   */
  readonly primaryProvenance?: CapabilityRecordSource['normalization']['provenance'];
  readonly exampleInput: JsonObject;
  readonly exampleOutput: JsonObject;
};

export type FoldedActionSpec = {
  readonly action: string;
  readonly pins: JsonObject;
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
    // Handlers report more than the contract names; the gateways fold those fields here
    // instead of dropping them (dogfood: thin reads such as #28/#210).
    details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Additional handler result fields not named by the contract.' },
    ...props,
  };
  return schema(full, ['success', ...required]);
}

const EMPTY_OUTPUT = outputSchema({}, []);

function availability(
  spec: WorldRecordSpec,
): CapabilityAvailability {
  const min = spec.unrealMin ?? V5_0;
  const max = spec.unrealMax ?? V5_8_P1;
  return {
    unreal: { min, max },
    requiredPlugins: [...(spec.plugins ?? [])],
    editorStates: [...(spec.editorStates ?? ['edit'])],
  };
}

function routing(
  parentTool: string,
  dispatchAction: string,
  dispatchMode: 'tool' | 'action' | 'local' = 'tool',
  dispatchBy?: WorldRecordSpec['dispatchBy'],
): CapabilityRouting {
  return {
    parentTool: LegacyToolNameSchema.parse(parentTool),
    dispatchAction: LegacyActionNameSchema.parse(dispatchAction),
    dispatchMode,
    ...(dispatchBy === undefined
      ? {}
      : {
        dispatchBy: {
          param: dispatchBy.param,
          actions: Object.fromEntries(
            Object.entries(dispatchBy.actions).map(([value, action]) => [value, LegacyActionNameSchema.parse(action)]),
          ),
        },
      }),
  };
}

export function buildWorldRecord(
  spec: WorldRecordSpec,
): CapabilityRecordSource {
  const required = [...new Set(['action', ...spec.required])];
  const input = schema({ action: ACTION_PROP, ...spec.inputProps }, required, spec.requiredOneOf);
  const output = spec.outputProps
    ? outputSchema(spec.outputProps, spec.outputRequired ?? [])
    : EMPTY_OUTPUT;
  const tool = LegacyToolNameSchema.parse(spec.parentTool);
  return {
    id: CapabilityIdSchema.parse(`${spec.parentTool}.${spec.action}`),
    aliases: (spec.aliases ?? []).map((alias) => CapabilityAliasSchema.parse(alias)),
    legacyIds: [
      {
        tool,
        action: LegacyActionNameSchema.parse(spec.action),
        ...(spec.primaryProvenance === undefined ? {} : { provenance: spec.primaryProvenance }),
      },
      ...(spec.folded ?? []).map((entry) => ({
        tool,
        action: LegacyActionNameSchema.parse(entry.action),
        folded: entry.pins,
      })),
    ],
    discovery: {
      domain: 'world',
      family: spec.family,
      topics: [spec.action, ...(spec.topics ?? [])],
      summary: spec.summary,
      whenToUse: [...spec.whenToUse],
      whenNotToUse: [...spec.whenNotToUse],
    },
    schemas: { input, output },
    examples: [{ title: spec.summary, input: spec.exampleInput, output: spec.exampleOutput }],
    availability: availability(spec),
    behavior: behavior(spec.effect, spec.behavior),
    policy: policy(spec.effect),
    cost: { latency: spec.costLatency, resources: spec.costResources },
    routing: routing(spec.parentTool, spec.dispatchAction ?? spec.action, spec.dispatchMode, spec.dispatchBy),
    normalization: {
      class: 'C_SAME_VERB_DIFFERENT_TARGET',
      disposition: 'retain',
      rationale: spec.normalizationRationale,
      ...(spec.normalizationProvenance === undefined
        ? {}
        : { provenance: spec.normalizationProvenance }),
    },
    deprecation: { status: 'active' },
    parent: getParentToolMetadata(spec.parentTool),
  };
}

