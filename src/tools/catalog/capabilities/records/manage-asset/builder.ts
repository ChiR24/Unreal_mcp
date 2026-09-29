// Shared builder for manage_asset capability records. Produces concise specs
// that are expanded to full CapabilityRecordSource objects. Every record shares
// availability and routing parent; per-record variation is in schemas,
// behavior, policy, cost and aliases.
import type {
  CapabilityAvailability,
  CapabilityBehaviorSource,
  CapabilityCost,
  CapabilityExample,
  CapabilityPolicy,
  Draft202012ObjectSchema,
  JsonObject,
} from '../../model.js';

// --- Schema helpers ---

export { bool, num, str } from '../shared/schema-props.js';
// Several material parameter values in one consented call (set_material_parameter, create_material_instance).
export const MATERIAL_PARAMETER_LIST: JsonObject = {
  type: 'array',
  items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true },
  'x-unreal-reflection-boundary': true,
  description: 'Several parameter values at once, each {parameterName, parameterType (scalar | vector | texture), value, or texturePath for a texture}; every entry is reported, and the call fails naming any that did not apply.',
};
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

export const READ: CapabilityBehaviorSource = { effect: 'read', idempotency: 'idempotent', longRunning: false, safeToRetry: true };
export const WRITE: CapabilityBehaviorSource = { effect: 'write', idempotency: 'idempotent', longRunning: false, safeToRetry: true };
export const DESTRUCTIVE: CapabilityBehaviorSource = { effect: 'destructive', idempotency: 'idempotent', longRunning: true, safeToRetry: false };
export const NON_IDEMPOTENT: CapabilityBehaviorSource = { effect: 'write', idempotency: 'non-idempotent', longRunning: false, safeToRetry: false };

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
  requiredPlugins: ['EditorScriptingUtilities'],
  editorStates: ['edit']
};

// --- Example helper ---

export function ex(title: string, input: JsonObject, output: JsonObject): CapabilityExample {
  return { title, input, output };
}

// --- Record spec and builder ---

export type Family = 'asset' | 'material' | 'texture' | 'struct' | 'datatable' | 'enum';

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
  readonly topics: readonly string[];
  readonly dispatchAction: string;
  readonly examples: NonEmptyExamples;
  readonly availability: CapabilityAvailability;
  readonly whenToUse?: readonly string[];
  readonly whenNotToUse?: readonly string[];
}

export interface SpecOptions {
  readonly aliases?: readonly string[];
  readonly topics?: readonly string[];
  readonly dispatchAction?: string;
  readonly examples: NonEmptyExamples;
  readonly requiredPlugins?: readonly string[];
  /** Situations this action is the answer to, as a caller would meet them. */
  readonly whenToUse?: readonly string[];
  /** Situations that look similar but need a sibling capability (name it). */
  readonly whenNotToUse?: readonly string[];
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
    topics: options.topics ?? [],
    dispatchAction: options.dispatchAction ?? action,
    examples: options.examples,
    whenToUse: options.whenToUse ?? [],
    whenNotToUse: options.whenNotToUse ?? [],
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
      topics: [spec.action, ...spec.topics],
      summary: spec.summary,
      whenToUse: [...(spec.whenToUse ?? [])],
      whenNotToUse: [...(spec.whenNotToUse ?? [])]
    },
    schemas: { input: spec.input, output: spec.output },
    examples: spec.examples,
    availability: spec.availability,
    behavior: spec.behavior,
    policy: spec.policy,
    cost: spec.cost,
    routing: { parentTool: 'manage_asset', dispatchAction: spec.dispatchAction }
  };
}
