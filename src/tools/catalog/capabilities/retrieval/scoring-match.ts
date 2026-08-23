// src/tools/catalog/capabilities/retrieval/scoring-match.ts
// The ranking internals for the capability retrieval engine: per-field BM25,
// exact-match bonuses, action-verb alignment, coverage and adjacency signals.
// Extracted from scoring.ts so the facade stays a thin public surface.

import { compareAscii as compareCanonicalCapabilityIds } from '../../../../utils/serialization/ordering.js';
import {
  MAX_MATCH_REASONS,
  MAX_REASON_TOKENS,
  RETRIEVAL_FIELD_WEIGHTS,
  RETRIEVAL_FUNCTION_WORDS,
  RETRIEVAL_SCORE_CONSTANTS,
  SCORE_TIE_EPSILON,
} from './constants.js';
import {
  type FieldContribution,
  type IndexedCapability,
  type IndexedField,
  type RankedCapability,
  type ScoreContext,
} from './scoring-types.js';

function queryTokenWeight(token: string): number {
  return RETRIEVAL_FUNCTION_WORDS.has(token)
    ? RETRIEVAL_SCORE_CONSTANTS.functionWordWeight
    : 1;
}

function scoreField(
  field: IndexedField,
  context: ScoreContext,
): FieldContribution | null {
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
    score += inverseFrequency * RETRIEVAL_FIELD_WEIGHTS[field.field] * termScore
      * queryTokenWeight(token);
  }
  if (matchedTokens.length === 0) return null;
  return {
    field: field.field,
    matchedTokens: Object.freeze(matchedTokens.slice(0, MAX_REASON_TOKENS)),
    allMatchedTokens: Object.freeze(matchedTokens),
    score,
  };
}

function tokensEqual(left: readonly string[], right: readonly string[]): boolean {
  return left.length === right.length && left.every((token, index) => token === right[index]);
}

function exactMatchAgainst(document: IndexedCapability, tokens: readonly string[]): number {
  if (tokens.length === 0) return 0;
  if (tokensEqual(document.idTokens, tokens)) {
    return RETRIEVAL_SCORE_CONSTANTS.exactCanonicalIdBonus;
  }
  if (document.aliasTokens.some((alias) => tokensEqual(alias, tokens))
    || document.aliasActionTokens.some((alias) => tokensEqual(alias, tokens))) {
    return RETRIEVAL_SCORE_CONSTANTS.exactAliasBonus;
  }
  for (const pair of document.legacyPairTokens) {
    if (tokensEqual(pair, tokens)) return RETRIEVAL_SCORE_CONSTANTS.exactLegacyPairBonus;
  }
  for (const action of document.legacyActionTokens) {
    if (tokensEqual(action, tokens)) return RETRIEVAL_SCORE_CONSTANTS.exactLegacyActionBonus;
  }
  return 0;
}

/**
 * Matched twice: verbatim, then against the content tokens. An identifier
 * cannot contain "a" or "the", so a query carrying either could otherwise never
 * reach ANY exact-match rung - the whole ladder, not an edge case.
 */
function exactMatchBonus(document: IndexedCapability, context: ScoreContext): number {
  return Math.max(
    exactMatchAgainst(document, context.queryTokens),
    exactMatchAgainst(document, context.contentTokens),
  );
}

/**
 * Action names in this registry are `verb[_object]` by construction, so the
 * leading token is a structural role, not vocabulary. A capability whose verb
 * the query never utters is answering a different question - this is what keeps
 * inflection folding safe, since "delete the imported asset" folds `imported`
 * to `import` but still leads with `delete`, not `import`.
 */
function headAlignmentScore(
  actionTokens: readonly string[],
  context: ScoreContext,
): number {
  const actionHead = actionTokens[0];
  if (actionHead === undefined) return 0;
  if (actionHead === context.contentTokens[0]) {
    return RETRIEVAL_SCORE_CONSTANTS.headVerbAlignmentBonus;
  }
  if (context.querySet.has(actionHead)) return 0;
  return -RETRIEVAL_SCORE_CONSTANTS.headVerbMismatchPenalty;
}

/**
 * Normalized by action length: an unnormalized penalty made one plural cost
 * more than any match could earn, so concise canonical names were structurally
 * punished for being short rather than for being wrong.
 */
function actionCoverageScore(document: IndexedCapability, context: ScoreContext): number {
  let bestScore = Number.NEGATIVE_INFINITY;
  for (const actionTokens of document.actionTokenSets) {
    let matched = 0;
    let unmatched = 0;
    for (const token of actionTokens) {
      if (context.querySet.has(token)) matched += queryTokenWeight(token);
      else unmatched += queryTokenWeight(token);
    }
    const coverage = (matched * RETRIEVAL_SCORE_CONSTANTS.matchedActionTokenBonus
      - unmatched * RETRIEVAL_SCORE_CONSTANTS.unmatchedActionTokenPenalty)
      / Math.max(actionTokens.length, 1);
    bestScore = Math.max(bestScore, coverage + headAlignmentScore(actionTokens, context));
  }
  return Number.isFinite(bestScore) ? bestScore : 0;
}

/**
 * Adjacent query terms appearing adjacently in an identifier are stronger
 * evidence than the same terms scattered across it. Function-word pairs are
 * skipped so English glue cannot manufacture adjacency. Pairs are formed over
 * the precomputed `contentTokens` (query minus function words), so "create a
 * widget" still sees the pair (create, widget): the article carries no meaning
 * an identifier would ever encode.
 */
function adjacencyScore(
  sequences: readonly (readonly string[])[],
  contentTokens: readonly string[],
): number {
  let score = 0;
  for (let index = 0; index + 1 < contentTokens.length; index += 1) {
    const first = contentTokens[index];
    const second = contentTokens[index + 1];
    if (first === undefined || second === undefined) continue;
    const adjacent = sequences.some((sequence) =>
      sequence.some((token, position) =>
        token === first && sequence[position + 1] === second));
    if (adjacent) score += RETRIEVAL_SCORE_CONSTANTS.adjacentQueryPairBonus;
  }
  return score;
}

function rounded(value: number, precision: number): number {
  const factor = 10 ** precision;
  return Math.round(value * factor) / factor;
}

export function scoreDocument(
  document: IndexedCapability,
  context: ScoreContext,
): RankedCapability | null {
  const contributions = document.scoredFields
    .map((field) => scoreField(field, context))
    .filter((entry): entry is FieldContribution => entry !== null);
  let lexicalScore = 0;
  for (const entry of contributions) lexicalScore += entry.score;
  const score = lexicalScore
    + exactMatchBonus(document, context)
    + actionCoverageScore(document, context)
    + adjacencyScore(document.sequences, context.contentTokens);
  if (score < RETRIEVAL_SCORE_CONSTANTS.minimumRelevanceScore) return null;
  contributions.sort((left, right) => {
    if (Math.abs(right.score - left.score) > SCORE_TIE_EPSILON) return right.score - left.score;
    return compareCanonicalCapabilityIds(left.field, right.field);
  });
  const matched = new Set(contributions.flatMap((entry) => entry.allMatchedTokens));
  const coverage = matched.size / context.queryTokens.length;
  const saturation = score / (score + RETRIEVAL_SCORE_CONSTANTS.confidenceSaturation);
  const confidence = rounded(
    Math.min(1,
      saturation * RETRIEVAL_SCORE_CONSTANTS.confidenceSaturationWeight
        + coverage * RETRIEVAL_SCORE_CONSTANTS.confidenceCoverageWeight),
    RETRIEVAL_SCORE_CONSTANTS.confidencePrecision,
  );
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
