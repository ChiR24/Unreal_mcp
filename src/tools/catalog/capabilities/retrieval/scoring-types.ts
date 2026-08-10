// src/tools/catalog/capabilities/retrieval/scoring-types.ts
// Shared types for the capability retrieval ranking engine. Kept dependency-free
// so both the index builder and the matcher can import the same shape without
// pulling each other's implementations.

import type { CapabilityRecord } from '../model.js';
import type { AliasFold } from './alias-fold.js';
import type { CapabilityMatchField, CapabilityMatchReason } from './types.js';

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

export type FieldContribution = CapabilityMatchReason & { readonly score: number };

export type ScoreContext = {
  readonly index: CapabilitySearchIndex;
  readonly queryTokens: readonly string[];
  /** `queryTokens` less closed-class glue, which no identifier can ever spell. */
  readonly contentTokens: readonly string[];
  readonly querySet: ReadonlySet<string>;
};
