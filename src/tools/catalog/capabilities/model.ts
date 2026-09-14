import type {
  BEHAVIOR_EFFECTS,
  CAPABILITY_PROVENANCE,
  COMPENSATION_MODES,
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
  PREVIEW_MODES,
  PREVIEW_REPORTS,
  RESOURCE_CLASSES,
  SEMANTICS_EVIDENCE_GRADES,
  UNDO_MODES
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
   * `post-migration` marks a pair authored after the gateway migration on a
   * record whose other pairs shipped before it: a folded family's new primary
   * name. The audit skips the pair; routing still resolves it.
   */
  readonly provenance?: (typeof CAPABILITY_PROVENANCE)[number];
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
  readonly unreal: {
    readonly min: UnrealVersion;
    readonly max: UnrealVersion;
  };
  readonly requiredPlugins: readonly string[];
  readonly editorStates: readonly (typeof EDITOR_STATES)[number][];
};

export type CapabilitySemanticsEvidence = {
  readonly grade: (typeof SEMANTICS_EVIDENCE_GRADES)[number];
  readonly citation: string;
};

export type CapabilityPreviewSemantics = {
  readonly mode: (typeof PREVIEW_MODES)[number];
  readonly reports: readonly (typeof PREVIEW_REPORTS)[number][];
  readonly evidence: CapabilitySemanticsEvidence;
};

export type CapabilityUndoSemantics = {
  readonly mode: (typeof UNDO_MODES)[number];
  readonly transactionScope: string | null;
  readonly evidence: CapabilitySemanticsEvidence;
};

export type CapabilityCompensationSemantics = {
  readonly mode: (typeof COMPENSATION_MODES)[number];
  readonly inverse: readonly CapabilityId[];
  readonly guidance: string | null;
  readonly evidence: CapabilitySemanticsEvidence;
};

export type CapabilitySemantics = {
  readonly preview: CapabilityPreviewSemantics;
  readonly undo: CapabilityUndoSemantics;
  readonly compensation: CapabilityCompensationSemantics;
};

export type CapabilityBehavior = {
  readonly effect: (typeof BEHAVIOR_EFFECTS)[number];
  readonly idempotency: (typeof IDEMPOTENCY_CLASSES)[number];
  readonly longRunning: boolean;
  readonly safeToRetry: boolean;
  readonly supportsPreview: boolean;
  readonly supportsUndo: boolean;
  readonly semantics: CapabilitySemantics;
};

export type CapabilityBehaviorSource = Omit<CapabilityBehavior, 'semantics'> & {
  readonly semantics?: CapabilitySemantics;
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
};

export type CapabilityRouting = {
  readonly parentTool: LegacyToolName;
  readonly dispatchAction: LegacyActionName;
  readonly dispatchMode: (typeof DISPATCH_MODES)[number];
  readonly dispatchBy?: CapabilityDispatchBy;
};

export type CapabilityNormalization = {
  readonly class: (typeof NORMALIZATION_CLASSES)[number];
  readonly disposition: (typeof NORMALIZATION_DISPOSITIONS)[number];
  readonly rationale: string;
  /**
   * The capability this one defers to, stated as data rather than left for a
   * reader to infer from `rationale`. Consumers must never parse the prose:
   * the native mirror cannot reproduce English parsing, so an unstated
   * relation is not a relation.
   */
  readonly aliasOf?: CapabilityId;
  /**
   * Whether this capability shipped on the pre-gateway surface. ABSENT means
   * `legacy-surface`, so every migrated record omits it and no content hash
   * moves by this field existing.
   *
   * For a migrated record `legacyIds` names both the pair the audit counts and
   * the pair the action enum is built from. A capability authored later has the
   * second without the first; `post-migration` states that, so the audit skips
   * the record while routing still resolves it. Omitting the marker is the safe
   * default — the record is counted and the reviewed total stops reproducing.
   */
  readonly provenance?: (typeof CAPABILITY_PROVENANCE)[number];
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
  readonly behavior: CapabilityBehaviorSource;
  readonly policy: CapabilityPolicy;
  readonly cost: CapabilityCost;
  readonly routing: CapabilityRouting;
  readonly normalization: CapabilityNormalization;
  readonly deprecation: CapabilityDeprecation;
  readonly parent: ParentToolMetadata;
};

// Only hand-authored sources may omit semantics; a minted record always has them.
export type CapabilityRecord = Omit<CapabilityRecordSource, 'behavior'> & {
  readonly behavior: CapabilityBehavior;
  readonly hashes: CapabilityHashes;
};

export type CapabilityCatalog = readonly CapabilityRecord[];
