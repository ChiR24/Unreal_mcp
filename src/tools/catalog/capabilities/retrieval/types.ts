import type {
  BEHAVIOR_EFFECTS,
  EDITOR_STATES,
  POLICY_SCOPES,
} from '../constants.js';
import type { CapabilityId, UnrealVersion } from '../identifiers.js';

export type CapabilityCategory = 'core' | 'world' | 'gameplay' | 'utility';
export type CapabilityEffect = (typeof BEHAVIOR_EFFECTS)[number];
export type CapabilityEditorState = (typeof EDITOR_STATES)[number];
export type CapabilityPolicyScope = (typeof POLICY_SCOPES)[number];

export type CapabilityRuntimeProfile = {
  readonly unrealVersion: UnrealVersion;
  readonly installedPlugins: readonly string[];
  readonly editorState: CapabilityEditorState;
  readonly enabledParents: readonly string[];
  readonly enabledCategories: readonly CapabilityCategory[];
  readonly authorizedScopes: readonly CapabilityPolicyScope[];
  readonly requestedEffects: readonly CapabilityEffect[];
  readonly requiredOutputFields: readonly string[];
};

export type CapabilityRetrievalRequest = {
  readonly query: string;
  readonly limit: number;
  readonly profile: CapabilityRuntimeProfile;
};

export type CapabilityMatchField =
  | 'canonical_id'
  | 'alias'
  | 'legacy_tool'
  | 'legacy_action'
  | 'domain'
  | 'family'
  | 'topic'
  | 'summary'
  | 'when_to_use'
  | 'when_not_to_use';

export type CapabilityMatchReason = {
  readonly field: CapabilityMatchField;
  readonly matchedTokens: readonly string[];
};

export type CapabilityAvailabilitySummary = {
  readonly status: 'available';
  readonly unreal: {
    readonly min: UnrealVersion;
    readonly max: UnrealVersion;
  };
  readonly requiredPlugins: readonly string[];
  readonly editorStates: readonly CapabilityEditorState[];
};

export type CapabilityDescribeNextCall = {
  readonly operation: 'describe';
  readonly capability: CapabilityId;
};

export type CapabilityRetrievalMatch = {
  readonly id: CapabilityId;
  readonly score: number;
  readonly confidence: number;
  readonly effect: CapabilityEffect;
  readonly reasons: readonly CapabilityMatchReason[];
  readonly availability: CapabilityAvailabilitySummary;
  readonly nextCall: CapabilityDescribeNextCall;
};

export type CapabilitySelection =
  | {
      readonly kind: 'selected';
      readonly capability: CapabilityId;
      readonly requiresConfirmation: boolean;
    }
  | {
      readonly kind: 'none';
      readonly reason:
        | 'empty_query'
        | 'no_match'
        | 'low_confidence'
        | 'destructive_near_tie';
    };

export type CapabilityRetrievalResult = {
  readonly matches: readonly CapabilityRetrievalMatch[];
  readonly nearTieCapabilityIds: readonly CapabilityId[];
  readonly selection: CapabilitySelection;
};

export type CapabilityRetriever = {
  readonly retrieve: (input: unknown) => CapabilityRetrievalResult;
};

export type CapabilityRetrievalParityVector = {
  readonly schema: 'unreal.capability-retrieval.parity.v1';
  readonly name: string;
  readonly request: CapabilityRetrievalRequest;
  readonly expected: {
    readonly rankedCapabilityIds: readonly CapabilityId[];
    readonly nearTieCapabilityIds: readonly CapabilityId[];
    readonly selection: CapabilitySelection;
  };
};
