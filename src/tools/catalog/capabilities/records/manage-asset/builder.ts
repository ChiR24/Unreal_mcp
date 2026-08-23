// Shared builder for manage_asset capability records. Produces concise specs
// that are expanded to full CapabilityRecordSource objects and validated via
// createCapabilityRecord. All 167 records share availability, normalization
// defaults, and routing parent; per-record variation is in schemas, behavior,
// policy, cost, and optional divergence/alias metadata.
import { DRAFT_2020_12_SCHEMA_URI } from '../../constants.js';
import { CapabilityIdSchema } from '../../identifiers.js';
import type {
  CapabilityAvailability,
  CapabilityBehaviorSource,
  CapabilityCost,
  CapabilityExample,
  CapabilityNormalization,
  CapabilityPolicy,
  Draft202012ObjectSchema,
  JsonObject
} from '../../model.js';
import { getParentToolMetadata } from '../parent-metadata.js';

// --- Schema helpers ---

export function schema(
  properties: JsonObject,
  required: readonly string[] = [],
  requiredOneOf?: readonly string[],
): Draft202012ObjectSchema {
  return {
    $schema: DRAFT_2020_12_SCHEMA_URI,
    type: 'object',
    properties,
    required,
    ...(requiredOneOf === undefined ? {} : { requiredOneOf: [...requiredOneOf] }),
    additionalProperties: false,
  };
}

export const str = (desc: string): JsonObject => ({ type: 'string', description: desc });
export const num = (desc: string): JsonObject => ({ type: 'number', description: desc });
export const bool = (desc: string): JsonObject => ({ type: 'boolean', description: desc });
export const arr = (desc: string): JsonObject => ({ type: 'array', items: { type: 'string' }, description: desc });
export const arrObj = (desc: string): JsonObject => ({ type: 'array', items: { type: 'object', 'x-unreal-reflection-boundary': true }, description: desc });
export const refObj = (desc: string): JsonObject => ({ type: 'object', 'x-unreal-reflection-boundary': true, description: desc });

// Bounded list-limit property for continuation-modeling list operations.
export const boundedLimit = (maxPageSize: number, defaultPageSize: number): JsonObject => ({
  type: 'number', minimum: 1, maximum: maxPageSize, default: defaultPageSize,
  description: `Page size (1-${maxPageSize}, default ${defaultPageSize}).`
});

// Nested pagination envelope accepted alongside the flat limit/offset pair.
// handleListAssets reads `params.limit ?? pagination.limit` (same for offset),
// so the flat form wins whenever both are supplied.
export const boundedPagination = (maxPageSize: number, defaultPageSize: number): JsonObject => ({
  type: 'object',
  properties: {
    limit: boundedLimit(maxPageSize, defaultPageSize),
    offset: { type: 'number', minimum: 0, description: 'Zero-based offset into the full result set.' }
  },
  additionalProperties: false,
  description: 'Nested pagination envelope. Top-level limit/offset take precedence when both are supplied.'
});

// --- Behavior presets ---

export const READ: CapabilityBehaviorSource = { effect: 'read', idempotency: 'idempotent', longRunning: false, safeToRetry: true, supportsPreview: false, supportsUndo: false };
export const WRITE: CapabilityBehaviorSource = { effect: 'write', idempotency: 'idempotent', longRunning: false, safeToRetry: true, supportsPreview: true, supportsUndo: true };
export const DESTRUCTIVE: CapabilityBehaviorSource = { effect: 'destructive', idempotency: 'idempotent', longRunning: true, safeToRetry: false, supportsPreview: true, supportsUndo: false };
export const NON_IDEMPOTENT: CapabilityBehaviorSource = { effect: 'write', idempotency: 'non-idempotent', longRunning: false, safeToRetry: false, supportsPreview: true, supportsUndo: true };

// --- Policy presets ---

export const READ_POLICY: CapabilityPolicy = { requiredScope: 'read', consent: 'none', dataAccess: 'project-read' };
export const WRITE_POLICY: CapabilityPolicy = { requiredScope: 'write', consent: 'explicit', dataAccess: 'project-write' };
export const DESTRUCTIVE_POLICY: CapabilityPolicy = { requiredScope: 'destructive', consent: 'elevated', dataAccess: 'project-write' };

// --- Cost presets ---

export const LOW: CapabilityCost = { latency: 'instant', resources: 'low' };
export const MEDIUM: CapabilityCost = { latency: 'interactive', resources: 'medium' };
export const HIGH: CapabilityCost = { latency: 'long-running', resources: 'high' };

// --- Availability ---

const DEFAULT_AVAILABILITY: CapabilityAvailability = {
  unreal: { min: { major: 5, minor: 0, patch: 0, channel: 'stable' }, max: { major: 5, minor: 8, patch: 0, channel: 'preview', preview: 1 } },
  requiredPlugins: ['EditorScriptingUtilities'],
  editorStates: ['edit']
};

// --- Normalization ---

export const RETAIN: CapabilityNormalization = {
  class: 'C_SAME_VERB_DIFFERENT_TARGET', disposition: 'retain',
  rationale: 'Distinct manage_asset capability with unique schema, target, and policy.'
};

export function aliasCanonical(aliasAction: string): CapabilityNormalization {
  return { class: 'B_ALIAS', disposition: 'canonical', rationale: `Short-form canonical; ${aliasAction} is an alias.` };
}

export function aliasOf(canonicalId: string): CapabilityNormalization {
  return {
    class: 'B_ALIAS',
    disposition: 'alias',
    rationale: `Long-form alias of ${canonicalId}.`,
    aliasOf: CapabilityIdSchema.parse(canonicalId),
  };
}

export function divergence(rationale: string): CapabilityNormalization {
  return { class: 'C_SAME_VERB_DIFFERENT_TARGET', disposition: 'retain', rationale };
}

// --- Example helper ---

export function ex(title: string, input: JsonObject, output: JsonObject): CapabilityExample {
  return { title, input, output };
}

// --- Record spec and builder ---

export type Family = 'asset' | 'material' | 'texture' | 'struct' | 'datatable' | 'enum';
export type DispatchMode = 'tool' | 'action' | 'local';

// An example is the only executable documentation a client sees for an action,
// so absence is a contract defect rather than a default worth tolerating.
export type NonEmptyExamples = readonly [CapabilityExample, ...CapabilityExample[]];

export interface RecordSpec {
  readonly action: string;
  readonly family: Family;
  readonly summary: string;
  readonly input: Draft202012ObjectSchema;
  readonly output: Draft202012ObjectSchema;
  readonly behavior: CapabilityBehaviorSource;
  readonly policy: CapabilityPolicy;
  readonly cost: CapabilityCost;
  readonly aliases: readonly string[];
  readonly dispatchAction: string;
  readonly dispatchMode: DispatchMode;
  readonly normalization: CapabilityNormalization;
  readonly examples: NonEmptyExamples;
  readonly availability: CapabilityAvailability;
}

export interface SpecOptions {
  readonly aliases?: readonly string[];
  readonly dispatchAction?: string;
  readonly dispatchMode?: DispatchMode;
  readonly normalization?: CapabilityNormalization;
  readonly examples: NonEmptyExamples;
  readonly requiredPlugins?: readonly string[];
}

const FAMILY_NAMES: Readonly<Record<Family, string>> = { asset: 'lifecycle', material: 'authoring', texture: 'procedural', struct: 'struct-authoring', datatable: 'datatable', enum: 'enum' };

export function r(
  action: string,
  family: Family,
  summary: string,
  input: Draft202012ObjectSchema,
  output: Draft202012ObjectSchema,
  behavior: CapabilityBehaviorSource,
  policy: CapabilityPolicy,
  cost: CapabilityCost,
  options: SpecOptions
): RecordSpec {
  const plugins = options.requiredPlugins
    ? [...new Set([...DEFAULT_AVAILABILITY.requiredPlugins, ...options.requiredPlugins])]
    : DEFAULT_AVAILABILITY.requiredPlugins;
  return {
    action, family, summary, input, output, behavior, policy, cost,
    aliases: options.aliases ?? [],
    dispatchAction: options.dispatchAction ?? action,
    dispatchMode: options.dispatchMode ?? 'tool',
    normalization: options.normalization ?? RETAIN,
    examples: options.examples,
    availability: { ...DEFAULT_AVAILABILITY, requiredPlugins: plugins }
  };
}

// Expand a spec to a plain object matching CapabilityRecordSource shape.
// createCapabilityRecord (called in index.ts) validates via Zod and mints hashes.
export function toSource(spec: RecordSpec): Record<string, unknown> {
  const id = `${spec.family}.${spec.action}`;
  return {
    id,
    aliases: spec.aliases,
    legacyIds: [{ tool: 'manage_asset', action: spec.action }],
    discovery: {
      domain: spec.family,
      family: FAMILY_NAMES[spec.family],
      topics: [spec.action],
      summary: spec.summary,
      whenToUse: [`Use when: ${spec.summary}`],
      whenNotToUse: ['Do not use when a different manage_asset action is more specific.']
    },
    schemas: { input: spec.input, output: spec.output },
    examples: spec.examples,
    availability: spec.availability,
    behavior: spec.behavior,
    policy: spec.policy,
    cost: spec.cost,
    routing: { parentTool: 'manage_asset', dispatchAction: spec.dispatchAction, dispatchMode: spec.dispatchMode },
    normalization: spec.normalization,
    deprecation: { status: 'active' },
    parent: getParentToolMetadata('manage_asset')
  };
}
