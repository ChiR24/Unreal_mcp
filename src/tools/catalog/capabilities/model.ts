import type {
  BEHAVIOR_EFFECTS,
  CONSENT_MODES,
  DATA_ACCESS_CLASSES,
  EDITOR_STATES,
  HASH_ALGORITHM,
  IDEMPOTENCY_CLASSES,
  LATENCY_CLASSES,
  POLICY_SCOPES,
  RESOURCE_CLASSES,

} from './constants.js';
import type {
  CapabilityAlias,
  CapabilityId,
  LegacyActionName,
  LegacyToolName
} from './identifiers.js';

export type JsonPrimitive = string | number | boolean | null;
export type JsonValue = JsonPrimitive | JsonObject | readonly JsonValue[];
export type JsonObject = { readonly [key: string]: JsonValue };

export type Draft202012ObjectSchema = JsonObject & {
  readonly $schema: 'https://json-schema.org/draft/2020-12/schema';
  readonly type: 'object';
  readonly properties: JsonObject;
  readonly required: readonly string[];
  readonly additionalProperties: boolean | JsonObject;
  /**
   * At-least-one-of: at least one of the listed property names must be
   * present in a validated value. NOT true XOR - supplying more than one
   * listed property is valid at the schema level (a native handler may
   * still reject the combination, which is a handler contract, not a
   * schema keyword). Names must reference declared `properties` entries.
   * The keyword is presence-only: an empty-string value (or `null`) still
   * satisfies the group at the schema level; handlers enforce non-empty
   * values separately.
   */
  readonly requiredOneOf?: readonly string[];
};

export type LegacyCapabilityId = {
  readonly tool: LegacyToolName;
  readonly action: LegacyActionName;
  /**
   * Present when this pair was folded into the record's primary operation.
   * The object pins the selector parameters the old name implied, so a call
   * by the old name validates against the folded contract and dispatches
   * unchanged. A folded pair stays callable but is not advertised in the
   * parent action enum.
   */
  readonly folded?: JsonObject;
};

export type CapabilityDiscovery = {
  readonly domain: string;
  readonly family: string;
  readonly topics: readonly string[];
  readonly summary: string;
  readonly whenToUse: readonly string[];
  readonly whenNotToUse: readonly string[];
};

export type CapabilitySchemas = {
  readonly input: Draft202012ObjectSchema;
  readonly output: Draft202012ObjectSchema;
};

export type CapabilityExample = {
  readonly title: string;
  readonly input: JsonObject;
  readonly output: JsonObject;
};

export type CapabilityAvailability = {
  readonly requiredPlugins: readonly string[];
  readonly editorStates: readonly (typeof EDITOR_STATES)[number][];
};

/** How to reverse a successful call: an inverse capability, or a manual cleanup note. */
export type CapabilityCompensation =
  | { readonly inverse: readonly CapabilityId[] }
  | { readonly guidance: string };

export type CapabilityBehavior = {
  readonly effect: (typeof BEHAVIOR_EFFECTS)[number];
  readonly idempotency: (typeof IDEMPOTENCY_CLASSES)[number];
  readonly longRunning: boolean;
  readonly safeToRetry: boolean;
  readonly compensation?: CapabilityCompensation;
};

export type CapabilityBehaviorSource = CapabilityBehavior;

export type CapabilityPolicy = {
  readonly requiredScope: (typeof POLICY_SCOPES)[number];
  readonly consent: (typeof CONSENT_MODES)[number];
  readonly dataAccess: (typeof DATA_ACCESS_CLASSES)[number];
};

export type CapabilityCost = {
  readonly latency: (typeof LATENCY_CLASSES)[number];
  readonly resources: (typeof RESOURCE_CLASSES)[number];
};

/**
 * Selects the bridge action from one selector parameter's value, so a single
 * record stands for a family of handler actions that differ only by that
 * value. Keys are the selector's declared enum values; every action is one of
 * the record's folded legacy actions, so nothing is dispatched that the
 * handlers did not already implement.
 */
export type CapabilityDispatchBy = {
  readonly param: string;
  readonly actions: { readonly [value: string]: LegacyActionName };
  /**
   * Parameter -> the selector values whose variant declares it, for every
   * parameter the variants do not all share. A call that omits the selector
   * runs the one variant its parameters point to (see inferSelector).
   */
  readonly declaredBy?: { readonly [param: string]: readonly string[] };
};

export type CapabilityRouting = {
  readonly parentTool: LegacyToolName;
  readonly dispatchAction: LegacyActionName;
  readonly dispatchBy?: CapabilityDispatchBy;
};

export type CapabilityHashes = {
  readonly algorithm: typeof HASH_ALGORITHM;
  readonly schema: string;
  readonly content: string;
};

export type CapabilityRecordSource = {
  readonly id: CapabilityId;
  readonly aliases: readonly CapabilityAlias[];
  readonly legacyIds: readonly LegacyCapabilityId[];
  readonly discovery: CapabilityDiscovery;
  readonly schemas: CapabilitySchemas;
  readonly examples: readonly CapabilityExample[];
  readonly availability: CapabilityAvailability;
  readonly behavior: CapabilityBehaviorSource;
  readonly policy: CapabilityPolicy;
  readonly cost: CapabilityCost;
  readonly routing: CapabilityRouting;
};

export type CapabilityRecord = CapabilityRecordSource & {
  readonly hashes: CapabilityHashes;
};

export type CapabilityCatalog = readonly CapabilityRecord[];
