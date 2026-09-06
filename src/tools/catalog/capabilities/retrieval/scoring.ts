// src/tools/catalog/capabilities/retrieval/scoring.ts
// Public surface of the capability retrieval ranking engine. Index construction
// lives in scoring-index.ts, the ranking internals in scoring-match.ts, and the
// shared shapes in scoring-types.ts. This facade preserves the module's exports
// exactly; importers of `./scoring.js` are unchanged.

import type { CapabilityRecord } from '../model.js';
import { compareAscii as compareCanonicalCapabilityIds } from '../../../../utils/serialization/ordering.js';
import { canonicalCapabilityId } from './alias-fold.js';
import {
  NEAR_TIE_RATIO,
  RETRIEVAL_FUNCTION_WORDS,
  SCORE_TIE_EPSILON,
} from './constants.js';
import { uniqueCapabilityTokens } from './tokenize.js';
import { scoreDocument } from './scoring-match.js';
import type {
  CapabilitySearchIndex,
  RankedCapability,
  ScoreContext,
} from './scoring-types.js';
export type {
  CapabilitySearchIndex,
  IndexedCapability,
  IndexedField,
  RankedCapability,
} from './scoring-types.js';

export { createCapabilitySearchIndex } from './scoring-index.js';

export function isNearTieScore(topScore: number, candidateScore: number): boolean {
  if (topScore <= 0) return false;
  const threshold = Math.max(SCORE_TIE_EPSILON, topScore * NEAR_TIE_RATIO);
  return topScore - candidateScore <= threshold;
}

export function rankCapabilityRecords(
  index: CapabilitySearchIndex,
  records: readonly CapabilityRecord[],
  query: string,
): readonly RankedCapability[] {
  const queryTokens = uniqueCapabilityTokens(query);
  if (queryTokens.length === 0) return [];
  // A caller may name an alias; ranking answers in primary space, so the
  // allow-list is canonicalised before it is applied.
  const allowedIds = new Set(
    records.map((record) => canonicalCapabilityId(index.aliasFold, String(record.id))),
  );
  const context = {
    index,
    queryTokens,
    contentTokens: queryTokens.filter((token) => !RETRIEVAL_FUNCTION_WORDS.has(token)),
    querySet: new Set(queryTokens),
  } satisfies ScoreContext;
  const ranked = index.documents
    .filter((document) => allowedIds.has(String(document.record.id)))
    .map((document) => scoreDocument(document, context))
    .filter((entry): entry is RankedCapability => entry !== null);
  ranked.sort((left, right) => {
    if (Math.abs(right.score - left.score) > SCORE_TIE_EPSILON) return right.score - left.score;
    return compareCanonicalCapabilityIds(left.record.id, right.record.id);
  });
  return ranked;
}
