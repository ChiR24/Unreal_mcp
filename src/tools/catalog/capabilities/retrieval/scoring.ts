import type { CapabilityRecord } from '../model.js';
import {
  MAX_MATCH_REASONS,
  MAX_REASON_TOKENS,
  NEAR_TIE_RATIO,
  RETRIEVAL_FIELD_WEIGHTS,
  RETRIEVAL_SCORE_CONSTANTS,
  SCORE_TIE_EPSILON,
} from './constants.js';
import { tokenizeCapabilityText, uniqueCapabilityTokens } from './tokenize.js';
import type { CapabilityMatchField, CapabilityMatchReason } from './types.js';

type IndexedField = {
  readonly field: CapabilityMatchField;
  readonly tokens: readonly string[];
  readonly counts: ReadonlyMap<string, number>;
};

type IndexedCapability = {
  readonly record: CapabilityRecord;
  readonly fields: readonly IndexedField[];
};

export type CapabilitySearchIndex = {
  readonly documents: readonly IndexedCapability[];
  readonly documentFrequency: ReadonlyMap<string, number>;
  readonly averageFieldLengths: ReadonlyMap<CapabilityMatchField, number>;
};

export type RankedCapability = {
  readonly record: CapabilityRecord;
  readonly score: number;
  readonly confidence: number;
  readonly reasons: readonly CapabilityMatchReason[];
};

type FieldContribution = CapabilityMatchReason & { readonly score: number };
type ScoreContext = {
  readonly index: CapabilitySearchIndex;
  readonly queryTokens: readonly string[];
};

function indexedField(field: CapabilityMatchField, values: readonly string[]): IndexedField {
  const tokens = values.flatMap((value) => tokenizeCapabilityText(value));
  const counts = new Map<string, number>();
  for (const token of tokens) counts.set(token, (counts.get(token) ?? 0) + 1);
  return Object.freeze({ field, tokens: Object.freeze(tokens), counts });
}

function searchFields(record: CapabilityRecord): readonly IndexedField[] {
  return Object.freeze([
    indexedField('canonical_id', [record.id]),
    indexedField('alias', record.aliases),
    indexedField('legacy_tool', record.legacyIds.map((legacy) => legacy.tool)),
    indexedField('legacy_action', record.legacyIds.map((legacy) => legacy.action)),
    indexedField('domain', [record.discovery.domain]),
    indexedField('family', [record.discovery.family]),
    indexedField('topic', record.discovery.topics),
    indexedField('summary', [record.discovery.summary]),
    indexedField('when_to_use', record.discovery.whenToUse),
    indexedField('when_not_to_use', record.discovery.whenNotToUse),
  ]);
}

export function compareCanonicalCapabilityIds(left: string, right: string): number {
  if (left < right) return -1;
  if (left > right) return 1;
  return 0;
}

export function isNearTieScore(topScore: number, candidateScore: number): boolean {
  if (topScore <= 0) return false;
  const threshold = Math.max(SCORE_TIE_EPSILON, topScore * NEAR_TIE_RATIO);
  return topScore - candidateScore <= threshold;
}

export function createCapabilitySearchIndex(
  records: readonly CapabilityRecord[],
): CapabilitySearchIndex {
  const documents = records
    .map((record) => Object.freeze({ record, fields: searchFields(record) }))
    .sort((left, right) => compareCanonicalCapabilityIds(left.record.id, right.record.id));
  const documentFrequency = new Map<string, number>();
  const fieldLengths = new Map<CapabilityMatchField, number>();
  for (const document of documents) {
    const documentTokens = new Set(document.fields.flatMap((field) => field.tokens));
    for (const token of documentTokens) {
      documentFrequency.set(token, (documentFrequency.get(token) ?? 0) + 1);
    }
    for (const field of document.fields) {
      fieldLengths.set(field.field, (fieldLengths.get(field.field) ?? 0) + field.tokens.length);
    }
  }
  const averageFieldLengths = new Map<CapabilityMatchField, number>();
  for (const [field, length] of fieldLengths) {
    averageFieldLengths.set(field, documents.length === 0 ? 1 : length / documents.length);
  }
  return Object.freeze({
    documents: Object.freeze(documents),
    documentFrequency,
    averageFieldLengths,
  });
}

function scoreField(field: IndexedField, context: ScoreContext): FieldContribution | null {
  const matchedTokens: string[] = [];
  let score = 0;
  const averageLength = context.index.averageFieldLengths.get(field.field) ?? 1;
  const normalizedLength = field.tokens.length / Math.max(averageLength, 1);
  const normalization = RETRIEVAL_SCORE_CONSTANTS.bm25K1
    * (1 - RETRIEVAL_SCORE_CONSTANTS.bm25LengthNormalization
      + RETRIEVAL_SCORE_CONSTANTS.bm25LengthNormalization * normalizedLength);
  for (const token of context.queryTokens) {
    const frequency = field.counts.get(token) ?? 0;
    if (frequency === 0) continue;
    matchedTokens.push(token);
    const documentFrequency = context.index.documentFrequency.get(token) ?? 0;
    const inverseFrequency = Math.log(
      1 + (context.index.documents.length - documentFrequency + 0.5)
        / (documentFrequency + 0.5),
    );
    const termScore = frequency * (RETRIEVAL_SCORE_CONSTANTS.bm25K1 + 1)
      / (frequency + normalization);
    score += inverseFrequency * RETRIEVAL_FIELD_WEIGHTS[field.field] * termScore;
  }
  if (matchedTokens.length === 0) return null;
  return {
    field: field.field,
    matchedTokens: Object.freeze(matchedTokens.slice(0, MAX_REASON_TOKENS)),
    score,
  };
}

function tokensEqual(left: readonly string[], right: readonly string[]): boolean {
  return left.length === right.length && left.every((token, index) => token === right[index]);
}

function exactMatchBonus(record: CapabilityRecord, queryTokens: readonly string[]): number {
  if (tokensEqual(tokenizeCapabilityText(record.id), queryTokens)) {
    return RETRIEVAL_SCORE_CONSTANTS.exactCanonicalIdBonus;
  }
  if (record.aliases.some((alias) => tokensEqual(tokenizeCapabilityText(alias), queryTokens))) {
    return RETRIEVAL_SCORE_CONSTANTS.exactAliasBonus;
  }
  for (const legacy of record.legacyIds) {
    if (tokensEqual(tokenizeCapabilityText(`${legacy.tool} ${legacy.action}`), queryTokens)) {
      return RETRIEVAL_SCORE_CONSTANTS.exactLegacyPairBonus;
    }
    if (tokensEqual(tokenizeCapabilityText(legacy.action), queryTokens)) {
      return RETRIEVAL_SCORE_CONSTANTS.exactLegacyActionBonus;
    }
  }
  return 0;
}

function actionCoverageScore(record: CapabilityRecord, queryTokens: readonly string[]): number {
  const query = new Set(queryTokens);
  let bestScore = Number.NEGATIVE_INFINITY;
  for (const legacy of record.legacyIds) {
    const actionTokens = uniqueCapabilityTokens(legacy.action);
    const matched = actionTokens.filter((token) => query.has(token)).length;
    const unmatched = actionTokens.length - matched;
    const score = matched * RETRIEVAL_SCORE_CONSTANTS.matchedActionTokenBonus
      - unmatched * RETRIEVAL_SCORE_CONSTANTS.unmatchedActionTokenPenalty;
    bestScore = Math.max(bestScore, score);
  }
  return Number.isFinite(bestScore) ? bestScore : 0;
}

function rounded(value: number, precision: number): number {
  const factor = 10 ** precision;
  return Math.round(value * factor) / factor;
}

function scoreDocument(document: IndexedCapability, context: ScoreContext): RankedCapability | null {
  const contributions = document.fields
    .map((field) => scoreField(field, context))
    .filter((entry): entry is FieldContribution => entry !== null);
  const lexicalScore = contributions.reduce((total, entry) => total + entry.score, 0);
  const score = lexicalScore
    + exactMatchBonus(document.record, context.queryTokens)
    + actionCoverageScore(document.record, context.queryTokens);
  if (score < RETRIEVAL_SCORE_CONSTANTS.minimumRelevanceScore) return null;
  contributions.sort((left, right) => {
    if (Math.abs(right.score - left.score) > SCORE_TIE_EPSILON) return right.score - left.score;
    return compareCanonicalCapabilityIds(left.field, right.field);
  });
  const matched = new Set(contributions.flatMap((entry) => entry.matchedTokens));
  const coverage = matched.size / context.queryTokens.length;
  const saturation = score / (score + RETRIEVAL_SCORE_CONSTANTS.confidenceSaturation);
  const confidence = rounded(Math.min(1, saturation * 0.85 + coverage * 0.15),
    RETRIEVAL_SCORE_CONSTANTS.confidencePrecision);
  return {
    record: document.record,
    score: rounded(score, RETRIEVAL_SCORE_CONSTANTS.scorePrecision),
    confidence,
    reasons: Object.freeze(contributions.slice(0, MAX_MATCH_REASONS).map(({ field, matchedTokens }) => ({
      field,
      matchedTokens,
    }))),
  };
}

export function rankCapabilityRecords(
  index: CapabilitySearchIndex,
  records: readonly CapabilityRecord[],
  query: string,
): readonly RankedCapability[] {
  const queryTokens = uniqueCapabilityTokens(query);
  if (queryTokens.length === 0) return [];
  const allowedIds = new Set(records.map((record) => record.id));
  const context = { index, queryTokens } satisfies ScoreContext;
  const ranked = index.documents
    .filter((document) => allowedIds.has(document.record.id))
    .map((document) => scoreDocument(document, context))
    .filter((entry): entry is RankedCapability => entry !== null);
  ranked.sort((left, right) => {
    if (Math.abs(right.score - left.score) > SCORE_TIE_EPSILON) return right.score - left.score;
    return compareCanonicalCapabilityIds(left.record.id, right.record.id);
  });
  return ranked;
}
