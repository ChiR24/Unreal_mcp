import type { CapabilityMatchField } from './types.js';

export const RETRIEVAL_PARITY_VECTOR_SCHEMA =
  'unreal.capability-retrieval.parity.v1' as const;

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
]));

export const SCORE_TIE_EPSILON = 1e-9 as const;
export const NEAR_TIE_RATIO = 0.02 as const;
export const MAX_MATCH_REASONS = 3 as const;
export const MAX_REASON_TOKENS = 3 as const;
export const DEFAULT_RESULT_LIMIT = 5 as const;
export const MAX_RESULT_LIMIT = 10 as const;

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
  minimumRelevanceScore: 0.01,
  confidenceSaturation: 40,
  minimumAutoSelectConfidence: 0.35,
  scorePrecision: 6,
  confidencePrecision: 4,
} as const);
