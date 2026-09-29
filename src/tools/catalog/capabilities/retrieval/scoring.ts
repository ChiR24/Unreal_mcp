// src/tools/catalog/capabilities/retrieval/scoring.ts
// The capability retrieval ranking engine: the shared shapes, index
// construction (field projection, byte-identical projection collapse, the
// document/dictionary statistics), the ranking internals (per-field BM25,
// exact-match bonuses, action-verb alignment, coverage and adjacency) and the
// public rankCapabilityRecords.

import type { CapabilityRecord } from '../model.js';
import { compareAscii as compareCanonicalCapabilityIds } from '../../../../utils/serialization/ordering.js';
import { canonicalCapabilityId, deriveAliasFold, type AliasFold } from './alias-fold.js';
import {
  MAX_MATCH_REASONS,
  MAX_REASON_TOKENS,
  RETRIEVAL_FIELD_WEIGHTS,
  RETRIEVAL_FUNCTION_WORDS,
  RETRIEVAL_NAME_FIELDS,
  RETRIEVAL_DELETE_INTENT_WORDS,
  RETRIEVAL_READ_INTENT_WORDS,
  RETRIEVAL_SCORE_CONSTANTS,
  SCORE_TIE_EPSILON,
} from './constants.js';
import { tokenizeCapabilityText, uniqueCapabilityTokens } from './tokenize.js';

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

export type IndexedField = {
  readonly field: CapabilityMatchField;
  readonly tokens: readonly string[];
  readonly counts: ReadonlyMap<string, number>;
};

export type IndexedCapability = {
  readonly record: CapabilityRecord;
  readonly fields: readonly IndexedField[];
  /** `fields` minus byte-identical re-projections. Ranking must read this one. */
  readonly scoredFields: readonly IndexedField[];
  /** Record-derived token views, precomputed once so ranking never tokenizes. */
  readonly sequences: readonly (readonly string[])[];
  readonly idTokens: readonly string[];
  readonly aliasTokens: readonly (readonly string[])[];
  /** The action segment of each alias - the rung `legacy_action` already has. */
  readonly aliasActionTokens: readonly (readonly string[])[];
  readonly legacyPairTokens: readonly (readonly string[])[];
  readonly legacyActionTokens: readonly (readonly string[])[];
  readonly actionTokenSets: readonly (readonly string[])[];
};

export type CapabilitySearchIndex = {
  readonly documents: readonly IndexedCapability[];
  readonly documentFrequency: ReadonlyMap<string, number>;
  readonly averageFieldLengths: ReadonlyMap<CapabilityMatchField, number>;
  readonly aliasFold: AliasFold;
};

export type RankedCapability = {
  readonly record: CapabilityRecord;
  readonly score: number;
  readonly confidence: number;
  readonly reasons: readonly CapabilityMatchReason[];
};

export type FieldContribution = CapabilityMatchReason & {
  readonly score: number;
  /** Full matched token set — coverage and confidence must read this, not the
   *  display-truncated `matchedTokens` (capped at `MAX_REASON_TOKENS`), or a
   *  long query that matched every token would report understated coverage. */
  readonly allMatchedTokens: readonly string[];
};

export type ScoreContext = {
  readonly index: CapabilitySearchIndex;
  readonly queryTokens: readonly string[];
  /** `queryTokens` less closed-class glue, which no identifier can ever spell. */
  readonly contentTokens: readonly string[];
  readonly querySet: ReadonlySet<string>;
  /** The query opens with a read word ("get actor location"): read-effect capabilities rank ahead. */
  readonly readIntent: boolean;
  /** The query opens with a delete word ("delete the spawned actor"): destructive capabilities rank ahead. */
  readonly deleteIntent: boolean;
};

/** The action segment of a declared alias: `control_actor.move_actor` -> `move_actor`. */
function aliasAction(alias: string): string {
  return alias.slice(alias.lastIndexOf('.') + 1);
}

// Identifier fields hold NAMES. A folded family carries every name it stands
// for, so a token repeated across those names is one piece of evidence, not
// several; counting each token once keeps a wide family from outranking the
// record a query actually describes.
function indexedField(field: CapabilityMatchField, values: readonly string[]): IndexedField {
  const every = values.flatMap((value) => tokenizeCapabilityText(value));
  const tokens = RETRIEVAL_NAME_FIELDS.has(field) ? [...new Set(every)] : every;
  const counts = new Map<string, number>();
  for (const token of tokens) counts.set(token, (counts.get(token) ?? 0) + 1);
  return Object.freeze({ field, tokens: Object.freeze(tokens), counts });
}

/**
 * A record's DECLARED aliases are its own names, so their action segments join
 * `legacy_action` beside the legacy verbs (a caller who types the alias verb is
 * naming this capability, not a lookalike). Absorbed legacy actions join
 * `legacy_action` too, not the weaker `alias` field:
 * after folding, the primary IS the capability those legacy actions dispatch
 * to, so filing them as mere aliases discards ranking signal the catalog
 * genuinely carries.
 */
function searchFields(record: CapabilityRecord): readonly IndexedField[] {
  // An alias that is just a folded legacy action under the record's own
  // namespace restates evidence legacy_action already carries; only declared
  // alternate names (move_actor, start_pie) belong to the alias field.
  const foldedActions = new Set(
    record.legacyIds.filter((legacy) => legacy.folded !== undefined).map((legacy) => String(legacy.action)),
  );
  const declaredAliases = record.aliases.filter((alias) => !foldedActions.has(aliasAction(String(alias))));
  return Object.freeze([
    indexedField('canonical_id', [record.id]),
    indexedField('alias', declaredAliases),
    indexedField('legacy_tool', record.legacyIds.map((legacy) => legacy.tool)),
    indexedField('legacy_action', [
      ...record.legacyIds.map((legacy) => legacy.action),
      ...record.aliases.map((alias) => aliasAction(String(alias))),
    ]),
    indexedField('domain', [record.discovery.domain]),
    indexedField('family', [record.discovery.family]),
    indexedField('topic', record.discovery.topics),
    indexedField('summary', [record.discovery.summary]),
    indexedField('when_to_use', record.discovery.whenToUse),
    indexedField('when_not_to_use', record.discovery.whenNotToUse),
  ]);
}

function projectionKey(field: IndexedField): string {
  return [...field.counts]
    .sort(([left], [right]) => compareCanonicalCapabilityIds(left, right))
    .map(([token, count]) => `${token}\u0000${count}`)
    .join('\u0001');
}

/**
 * A field whose token counts are byte-identical to another field's is the same
 * evidence re-projected, not a second independent signal: the catalog echoes
 * one action identifier into `legacy_action`, `topic` and `canonical_id` on
 * essentially every record, so summing all three counted one name three times
 * and let identifier length rather than relevance decide the ranking. Only
 * exact duplicates collapse - a field differing by a single token still
 * contributes in full, so genuinely distinct evidence is never discarded.
 */
function collapseProjections(fields: readonly IndexedField[]): readonly IndexedField[] {
  const strongest = new Map<string, IndexedField>();
  for (const field of fields) {
    if (field.tokens.length === 0) continue;
    const key = projectionKey(field);
    const held = strongest.get(key);
    if (held === undefined || outranks(field, held)) strongest.set(key, field);
  }
  const kept = new Set(strongest.values());
  return Object.freeze(fields.filter((field) => kept.has(field)));
}

function outranks(candidate: IndexedField, held: IndexedField): boolean {
  const difference = RETRIEVAL_FIELD_WEIGHTS[candidate.field] - RETRIEVAL_FIELD_WEIGHTS[held.field];
  if (difference !== 0) return difference > 0;
  return compareCanonicalCapabilityIds(candidate.field, held.field) < 0;
}

/**
 * Adjacency reads a capability's own names: its legacy actions, its id and its
 * declared aliases.
 */
function identifierSequences(record: CapabilityRecord): readonly (readonly string[])[] {
  return Object.freeze([
    ...record.legacyIds.map((legacy) => tokenizeCapabilityText(legacy.action)),
    ...record.aliases.map((alias) => tokenizeCapabilityText(aliasAction(String(alias)))),
    tokenizeCapabilityText(record.id),
  ]);
}

export function createCapabilitySearchIndex(
  records: readonly CapabilityRecord[],
): CapabilitySearchIndex {
  const aliasFold = deriveAliasFold(records);
  const documents = records
    .map((record): IndexedCapability => {
      const legacy = record.legacyIds;
      const fields = searchFields(record);
      const aliases: readonly string[] = record.aliases;
      return Object.freeze({
        record,
        fields,
        scoredFields: collapseProjections(fields),
        sequences: identifierSequences(record),
        idTokens: tokenizeCapabilityText(record.id),
        aliasTokens: Object.freeze(aliases.map((alias) => tokenizeCapabilityText(alias))),
        aliasActionTokens: Object.freeze(
          aliases.map((alias) => tokenizeCapabilityText(aliasAction(alias))),
        ),
        legacyPairTokens: Object.freeze(
          legacy.map((entry) => tokenizeCapabilityText(`${entry.tool} ${entry.action}`)),
        ),
        legacyActionTokens: Object.freeze(
          legacy.map((entry) => tokenizeCapabilityText(entry.action)),
        ),
        // Coverage reads the record's own action names, including declared aliases.
        actionTokenSets: Object.freeze(
          [
            ...record.legacyIds.map((entry) => uniqueCapabilityTokens(entry.action)),
            ...record.aliases.map((alias) => uniqueCapabilityTokens(aliasAction(String(alias)))),
          ],
        ),
      });
    })
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
    aliasFold,
  });
}

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
  // A name field's length is the number of names a record answers to, not
  // verbosity, so it carries no length penalty (see RETRIEVAL_NAME_FIELDS).
  const normalizedLength = RETRIEVAL_NAME_FIELDS.has(field.field) ? 1 : field.tokens.length / Math.max(averageLength, 1);
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
  const matchScore = lexicalScore
    + exactMatchBonus(document, context)
    + actionCoverageScore(document, context)
    + adjacencyScore(document.sequences, context.contentTokens);
  if (matchScore < RETRIEVAL_SCORE_CONSTANTS.minimumRelevanceScore) return null;
  // Only ever reorders records the query already matched; it never makes one a result.
  const effect = document.record.behavior.effect;
  const score = matchScore
    + (context.readIntent && effect === 'read' ? RETRIEVAL_SCORE_CONSTANTS.readIntentBonus : 0)
    + (context.deleteIntent && effect === 'destructive' ? RETRIEVAL_SCORE_CONSTANTS.deleteIntentBonus : 0);
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

export function rankCapabilityRecords(
  index: CapabilitySearchIndex,
  records: readonly CapabilityRecord[],
  query: string,
): readonly RankedCapability[] {
  const queryTokens = uniqueCapabilityTokens(query);
  if (queryTokens.length === 0) return [];
  // The first word as typed, before inflection folding, says what the caller wants done.
  const firstWord = query.toLowerCase().match(/[a-z0-9]+/u)?.[0] ?? '';
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
    readIntent: RETRIEVAL_READ_INTENT_WORDS.has(firstWord),
    deleteIntent: RETRIEVAL_DELETE_INTENT_WORDS.has(firstWord),
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
