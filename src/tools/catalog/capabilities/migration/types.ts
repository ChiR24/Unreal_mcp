import type { CapabilityId, LegacyActionName, LegacyToolName } from '../identifiers.js';

/**
 * A migration target is always a SHIPPED capability record, named by its dotted
 * record id. The normalization inventory's `cap:<namespace>:<action>` string is
 * a Task 5 analysis reference that no record ever carried, so it is never
 * published here — targets are resolved from the record source by legacy pair
 * (see `canonical-targets.ts`). Receipts therefore quote a live identity.
 */
export type CanonicalCapabilityRef = CapabilityId;

/**
 * Migration domain types for Task 20.
 *
 * Every shipped legacy `{tool, action}` occurrence (1,335 of them, sourced from
 * the audited normalization inventory) resolves to exactly one of:
 *   - a live capability record id (lossless, possibly through an alias), or
 *   - an explicit typed removal (the verb was retired), or
 *   - a non-translatable entry (the legacy call cannot be losslessly mapped to
 *     a single canonical capability because the semantics do not line up — e.g.
 *     distinct volume bounds vs extent semantics).
 *
 * Aliases are GENERATED from the capability records and the inventory, never
 * hand-written into handlers. Translation is refusal-first for lossy mismatches.
 */

export type LegacyKey = `${LegacyToolName}::${LegacyActionName}`;

export type MigrationDisposition =
  | 'canonical' // legacy occurrence maps 1:1 to a live canonical capability
  | 'alias' // legacy occurrence is an alias of a canonical capability
  | 'removed' // the verb was retired; explicit typed removal with guidance
  | 'non-translatable'; // lossy mismatch; translation refuses with guidance

export type ArgumentTransformKind =
  | 'identity' // params pass through unchanged
  | 'rename' // one or more param keys are renamed
  | 'drop-unsupported'; // unsupported legacy params are dropped with a note

export type ArgumentTransform = {
  readonly kind: ArgumentTransformKind;
  /** Source param key -> target canonical param key (for rename). */
  readonly renames: Readonly<Record<string, string>>;
  /** Legacy param keys that are intentionally dropped by the transform. */
  readonly dropped: ReadonlyArray<string>;
};

export type ReplacementGuidance = {
  readonly canonicalId?: CanonicalCapabilityRef;
  readonly reason: string;
  readonly nextCall: {
    readonly operation: 'execute';
    readonly tool: LegacyToolName;
    readonly action: LegacyActionName;
  };
};

export type MigrationEntry = {
  readonly legacyKey: LegacyKey;
  readonly tool: LegacyToolName;
  readonly action: LegacyActionName;
  readonly disposition: MigrationDisposition;
  /** Present for canonical/alias dispositions. */
  readonly canonicalId?: CanonicalCapabilityRef;
  /** Present for removed dispositions; a retired verb names no canonical target. */
  readonly removal?: {
    readonly since: string;
    readonly guidance: string;
    readonly replacement?: CanonicalCapabilityRef;
  };
  /** Present for non-translatable dispositions. */
  readonly nonTranslatable?: {
    readonly reason: string;
    readonly guidance: ReplacementGuidance;
  };
  /** Argument transform when the translation is lossless. */
  readonly argumentTransform?: ArgumentTransform;
  /** Deprecation window metadata. */
  readonly deprecation: {
    readonly status: 'active' | 'deprecated' | 'removed';
    readonly window: string;
  };
};

export type MigrationMap = {
  readonly schemaVersion: 'task20.migration.v1';
  readonly generatedAt: string;
  readonly occurrenceCount: number;
  readonly entries: ReadonlyMap<LegacyKey, MigrationEntry>;
};

export class NonTranslatableMigrationError extends Error {
  readonly code = 'MIGRATION_NON_TRANSLATABLE' as const;
  readonly legacyKey: LegacyKey;
  readonly guidance: ReplacementGuidance;

  constructor(legacyKey: LegacyKey, guidance: ReplacementGuidance) {
    super(
      `Legacy ${legacyKey} cannot be translated losslessly: ${guidance.reason}. ` +
        `Use ${guidance.canonicalId} (${guidance.nextCall.tool}.${guidance.nextCall.action}).`
    );
    this.name = 'NonTranslatableMigrationError';
    this.legacyKey = legacyKey;
    this.guidance = guidance;
  }
}

export class UnknownLegacyCallError extends Error {
  readonly code = 'MIGRATION_UNKNOWN_LEGACY' as const;
  readonly legacyKey: LegacyKey;

  constructor(legacyKey: LegacyKey) {
    super(`Legacy ${legacyKey} is not present in the migration map.`);
    this.name = 'UnknownLegacyCallError';
    this.legacyKey = legacyKey;
  }
}

export class UnmappedLegacyPairError extends Error {
  readonly code = 'MIGRATION_UNMAPPED_LEGACY_PAIR' as const;
  readonly legacyKey: LegacyKey;

  constructor(legacyKey: LegacyKey) {
    super(
      `Legacy ${legacyKey} selects no live capability record. A migration target must ` +
        'name a shipped record; the normalization inventory reference is never a fallback.'
    );
    this.name = 'UnmappedLegacyPairError';
    this.legacyKey = legacyKey;
  }
}

export type TranslateResult = {
  readonly canonicalId: CanonicalCapabilityRef;
  readonly transformedParams: Readonly<Record<string, unknown>>;
  /** The resolved migration entry, for receipt/observability. */
  readonly entry: MigrationEntry;
};
