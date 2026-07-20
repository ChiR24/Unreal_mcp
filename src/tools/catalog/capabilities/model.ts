import type {
  BEHAVIOR_EFFECTS,
  CONSENT_MODES,
  DATA_ACCESS_CLASSES,
  DEPRECATION_STATUSES,
  DISPATCH_MODES,
  EDITOR_STATES,
  HASH_ALGORITHM,
  IDEMPOTENCY_CLASSES,
  LATENCY_CLASSES,
  NORMALIZATION_CLASSES,
  NORMALIZATION_DISPOSITIONS,
  POLICY_SCOPES,
  RESOURCE_CLASSES
} from './constants.js';
import type {
  CapabilityAlias,
  CapabilityId,
  LegacyActionName,
  LegacyToolName,
  UnrealVersion
} from './identifiers.js';
import type { ParentToolMetadata } from './records/parent-metadata.js';

export type JsonPrimitive = string | number | boolean | null;
export type JsonValue = JsonPrimitive | JsonObject | readonly JsonValue[];
export type JsonObject = { readonly [key: string]: JsonValue };

export type Draft202012ObjectSchema = JsonObject & {
  readonly $schema: 'https://json-schema.org/draft/2020-12/schema';
  readonly type: 'object';
  readonly properties: JsonObject;
  readonly required: readonly string[];
  readonly additionalProperties: boolean | JsonObject;
};

export type LegacyCapabilityId = {
  readonly tool: LegacyToolName;
  readonly action: LegacyActionName;
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
  readonly unreal: {
    readonly min: UnrealVersion;
    readonly max: UnrealVersion;
  };
  readonly requiredPlugins: readonly string[];
  readonly editorStates: readonly (typeof EDITOR_STATES)[number][];
};

export type CapabilityBehavior = {
  readonly effect: (typeof BEHAVIOR_EFFECTS)[number];
  readonly idempotency: (typeof IDEMPOTENCY_CLASSES)[number];
  readonly longRunning: boolean;
  readonly safeToRetry: boolean;
  readonly supportsPreview: boolean;
  readonly supportsUndo: boolean;
};

export type CapabilityPolicy = {
  readonly requiredScope: (typeof POLICY_SCOPES)[number];
  readonly consent: (typeof CONSENT_MODES)[number];
  readonly dataAccess: (typeof DATA_ACCESS_CLASSES)[number];
};

export type CapabilityCost = {
  readonly latency: (typeof LATENCY_CLASSES)[number];
  readonly resources: (typeof RESOURCE_CLASSES)[number];
};

export type CapabilityRouting = {
  readonly parentTool: LegacyToolName;
  readonly dispatchAction: LegacyActionName;
  readonly dispatchMode: (typeof DISPATCH_MODES)[number];
};

export type CapabilityNormalization = {
  readonly class: (typeof NORMALIZATION_CLASSES)[number];
  readonly disposition: (typeof NORMALIZATION_DISPOSITIONS)[number];
  readonly rationale: string;
};

export type ActiveCapability = {
  readonly status: Extract<(typeof DEPRECATION_STATUSES)[number], 'active'>;
};

export type DeprecatedCapability = {
  readonly status: Extract<(typeof DEPRECATION_STATUSES)[number], 'deprecated' | 'removed'>;
  readonly since: string;
  readonly guidance: string;
  readonly replacement?: CapabilityId;
};

export type CapabilityDeprecation = ActiveCapability | DeprecatedCapability;

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
  readonly behavior: CapabilityBehavior;
  readonly policy: CapabilityPolicy;
  readonly cost: CapabilityCost;
  readonly routing: CapabilityRouting;
  readonly normalization: CapabilityNormalization;
  readonly deprecation: CapabilityDeprecation;
  readonly parent: ParentToolMetadata;
};

export type CapabilityRecord = CapabilityRecordSource & {
  readonly hashes: CapabilityHashes;
};

export type CapabilityCatalog = readonly CapabilityRecord[];
