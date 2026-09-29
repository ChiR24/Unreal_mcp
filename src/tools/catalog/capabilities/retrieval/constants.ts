import type { CapabilityMatchField } from './scoring.js';

export const RETRIEVAL_TOKENIZATION = Object.freeze({
  locale: 'invariant',
  caseFold: 'lowercase',
  tokenPattern: '[a-z0-9]+',
  splitCamelCase: true,
  foldInflections: true,
  maxQueryLength: 512,
  maxTokens: 48,
  maxTokenLength: 64,
} as const);

/**
 * Fields that hold NAMES (identifiers). A folded family lists every name it
 * answers to, so these fields count each token once and carry no BM25 length
 * penalty: their length is how many names a record has, not verbosity.
 */
export const RETRIEVAL_NAME_FIELDS: ReadonlySet<CapabilityMatchField> = new Set<CapabilityMatchField>([
  'canonical_id', 'alias', 'legacy_tool', 'legacy_action',
]);

export const RETRIEVAL_FIELD_WEIGHTS = Object.freeze({
  canonical_id: 6,
  alias: 7,
  legacy_tool: 2,
  legacy_action: 10,
  domain: 2,
  family: 4,
  topic: 8,
  summary: 3,
  when_to_use: 2,
  when_not_to_use: 0.25,
} satisfies Readonly<Record<CapabilityMatchField, number>>);

/**
 * Closed-class English function words. This is a grammatical category, not
 * project vocabulary: no capability name, domain term or corpus phrase may be
 * added here, because doing so would tune retrieval to specific queries.
 */
export const RETRIEVAL_FUNCTION_WORDS: ReadonlySet<string> = Object.freeze(new Set([
  'a', 'an', 'the', 'this', 'that', 'these', 'those', 'of', 'in', 'on', 'at',
  'to', 'for', 'from', 'by', 'with', 'into', 'onto', 'and', 'or', 'but', 'it',
  'its', 'is', 'are', 'be', 'as', 'all', 'every', 'any', 'some', 'my', 'our',
  'their', 'his', 'her', 'you', 'me', 'we', 'i', 'please', 'then', 'so',
  // Interrogatives and auxiliaries: a question ('what is the current level',
  // 'how do i spawn an actor') carries the same content words as the imperative.
  'what', 'which', 'how', 'where', 'who', 'when', 'why', 'do', 'does', 'did',
  'can', 'could', 'should', 'would', 'will', 'want', 'need',
]));

/**
 * Words that open a request to READ something: "get actor location", "what is
 * in this folder". A grammatical role, not vocabulary: ranking uses it only to
 * prefer capabilities whose declared effect is read, so "get actor location"
 * stops ranking set_transform first. Verbs that also open requests to change
 * things (view, look, see, check, print) are left out. Matched against the
 * first word as typed, before inflection folding. The native gateway search
 * uses the same list (McpNativeGatewaySearchMatch.cpp ReadIntentWords).
 */
export const RETRIEVAL_READ_INTENT_WORDS: ReadonlySet<string> = Object.freeze(new Set([
  'get', 'read', 'list', 'inspect', 'query', 'describe', 'find', 'count', 'show',
  'what', 'which', 'where', 'who', 'how', 'is', 'does',
]));

export const SCORE_TIE_EPSILON = 1e-9 as const;
export const MAX_MATCH_REASONS = 3 as const;
export const MAX_REASON_TOKENS = 3 as const;
export const RETRIEVAL_SCORE_CONSTANTS = Object.freeze({
  bm25K1: 1.2,
  bm25LengthNormalization: 0.75,
  exactCanonicalIdBonus: 60,
  exactAliasBonus: 55,
  exactLegacyPairBonus: 50,
  exactLegacyActionBonus: 30,
  matchedActionTokenBonus: 8,
  adjacentQueryPairBonus: 8,
  unmatchedActionTokenPenalty: 20,
  functionWordWeight: 0.25,
  headVerbAlignmentBonus: 6,
  headVerbMismatchPenalty: 10,
  readIntentBonus: 14,
  minimumRelevanceScore: 0.01,
  confidenceSaturation: 40,
  confidenceSaturationWeight: 0.85,
  confidenceCoverageWeight: 0.15,
  minimumAutoSelectConfidence: 0.35,
  scorePrecision: 6,
  confidencePrecision: 4,
} as const);
