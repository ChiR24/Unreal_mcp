// src/tools/catalog/capabilities/retrieval/scoring-index.ts
// Index construction for the capability retrieval engine: field projection,
// byte-identical projection collapse, and the document/dictionary statistics
// the matcher reads. Extracted from scoring.ts so ranking internals stay lean.

import type { CapabilityRecord } from '../model.js';
import { compareAscii as compareCanonicalCapabilityIds } from '../../../../utils/serialization/ordering.js';
import { deriveAliasFold } from './alias-fold.js';
import {
  RETRIEVAL_FIELD_WEIGHTS,
} from './constants.js';
import { tokenizeCapabilityText, uniqueCapabilityTokens } from './tokenize.js';
import type { CapabilityMatchField } from './types.js';
import {
  type CapabilitySearchIndex,
  type IndexedCapability,
  type IndexedField,
} from './scoring-types.js';

function indexedField(field: CapabilityMatchField, values: readonly string[]): IndexedField {
  const tokens = values.flatMap((value) => tokenizeCapabilityText(value));
  const counts = new Map<string, number>();
  for (const token of tokens) counts.set(token, (counts.get(token) ?? 0) + 1);
  return Object.freeze({ field, tokens: Object.freeze(tokens), counts });
}

/**
 * Absorbed legacy actions join `legacy_action`, not the weaker `alias` field:
 * after folding, the primary IS the capability those legacy actions dispatch
 * to, so filing them as mere aliases discards ranking signal the catalog
 * genuinely carries.
 */
function searchFields(
  record: CapabilityRecord,
  absorbed: readonly CapabilityRecord[],
): readonly IndexedField[] {
  return Object.freeze([
    indexedField('canonical_id', [record.id]),
    indexedField('alias', [
      ...record.aliases,
      ...absorbed.map((alias) => String(alias.id)),
      ...absorbed.flatMap((alias) => alias.aliases),
    ]),
    indexedField('legacy_tool', record.legacyIds.map((legacy) => legacy.tool)),
    indexedField('legacy_action', [
      ...record.legacyIds.map((legacy) => legacy.action),
      ...absorbed.flatMap((alias) => alias.legacyIds.map((legacy) => legacy.action)),
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
 * Adjacency reads a capability's OWN names only. An absorbed alias still lends
 * its tokens to field matching, but allowing it to supply a phrase would let a
 * secondary name claim that the capability is what the query described.
 */
function identifierSequences(record: CapabilityRecord): readonly (readonly string[])[] {
  return Object.freeze([
    ...record.legacyIds.map((legacy) => tokenizeCapabilityText(legacy.action)),
    tokenizeCapabilityText(record.id),
  ]);
}

export function createCapabilitySearchIndex(
  records: readonly CapabilityRecord[],
): CapabilitySearchIndex {
  const aliasFold = deriveAliasFold(records);
  const documents = records
    .filter((record) => !aliasFold.targets.has(String(record.id)))
    .map((record): IndexedCapability => {
      const absorbed = aliasFold.absorbed.get(String(record.id)) ?? [];
      const legacy = [...record.legacyIds, ...absorbed.flatMap((alias) => alias.legacyIds)];
      const fields = searchFields(record, absorbed);
      const aliases = [
        ...record.aliases,
        ...absorbed.map((alias) => String(alias.id)),
        ...absorbed.flatMap((alias) => alias.aliases),
      ];
      return Object.freeze({
        record,
        fields,
        scoredFields: collapseProjections(fields),
        sequences: identifierSequences(record),
        idTokens: tokenizeCapabilityText(record.id),
        aliasTokens: Object.freeze(aliases.map((alias) => tokenizeCapabilityText(alias))),
        aliasActionTokens: Object.freeze(
          aliases.map((alias) => tokenizeCapabilityText(alias.slice(alias.lastIndexOf('.') + 1))),
        ),
        legacyPairTokens: Object.freeze(
          legacy.map((entry) => tokenizeCapabilityText(`${entry.tool} ${entry.action}`)),
        ),
        legacyActionTokens: Object.freeze(
          legacy.map((entry) => tokenizeCapabilityText(entry.action)),
        ),
        // Coverage stays on the record's OWN action names. An absorbed alias
        // may lend its tokens to matching and adjacency, but letting it also
        // claim coverage would let a capability look more precisely named than
        // it is and outrank the capability the query actually described.
        actionTokenSets: Object.freeze(
          record.legacyIds.map((entry) => uniqueCapabilityTokens(entry.action)),
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
