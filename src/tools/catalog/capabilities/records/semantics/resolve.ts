/**
 * Resolver that stamps every capability with its declared preview / undo /
 * compensation semantics.
 *
 * Fail-closed by construction: the pessimistic value is produced unless
 * `evidence-ledger.ts` holds an entry for the capability, so a newly added
 * capability is born claiming nothing. The legacy `supportsPreview` /
 * `supportsUndo` booleans are derived from the same resolution, so a record can
 * never advertise a boolean that contradicts its declared semantics.
 */
import { CapabilityIdSchema } from '../../identifiers.js';
import type {
  CapabilityBehavior,
  CapabilityBehaviorSource,
  CapabilityCompensationSemantics,
  CapabilityPreviewSemantics,
  CapabilitySemantics,
  CapabilityUndoSemantics
} from '../../model.js';
import { COMPENSATION_EVIDENCE, PREVIEW_EVIDENCE, UNDO_EVIDENCE } from './evidence-ledger.js';

const NO_PREVIEW_CITATION =
  'no dry-run path exists on either transport; options.preview cannot be honored by this leaf';
const NO_UNDO_CITATION =
  'no scoped editor transaction fully wrapping this mutation was established from the handler implementation';
const NO_COMPENSATION_CITATION =
  'no compensating capability or cleanup procedure was established from the handler implementation';

function resolvePreview(id: string): CapabilityPreviewSemantics {
  const entry = PREVIEW_EVIDENCE.get(id);
  if (!entry) {
    return {
      mode: 'none',
      reports: [],
      evidence: { grade: 'pessimistic-default', citation: NO_PREVIEW_CITATION }
    };
  }
  return {
    mode: entry.mode,
    reports: [...entry.reports],
    evidence: { grade: 'source-verified', citation: entry.citation }
  };
}

function resolveUndo(id: string): CapabilityUndoSemantics {
  const entry = UNDO_EVIDENCE.get(id);
  if (!entry) {
    return {
      mode: 'none',
      transactionScope: null,
      evidence: { grade: 'pessimistic-default', citation: NO_UNDO_CITATION }
    };
  }
  return {
    mode: 'transaction',
    transactionScope: entry.transactionScope,
    evidence: { grade: 'source-verified', citation: entry.citation }
  };
}

function resolveCompensation(id: string): CapabilityCompensationSemantics {
  const entry = COMPENSATION_EVIDENCE.get(id);
  if (!entry) {
    return {
      mode: 'none',
      inverse: [],
      guidance: null,
      evidence: { grade: 'pessimistic-default', citation: NO_COMPENSATION_CITATION }
    };
  }
  if (entry.kind === 'manual') {
    return {
      mode: 'manual-cleanup',
      inverse: [],
      guidance: entry.guidance,
      evidence: { grade: 'source-verified', citation: entry.citation }
    };
  }
  return {
    mode: 'inverse-capability',
    inverse: entry.inverse.map((target) => CapabilityIdSchema.parse(target)),
    guidance: null,
    evidence: { grade: 'contract-derived', citation: entry.citation }
  };
}

export function resolveCapabilitySemantics(id: string): CapabilitySemantics {
  return {
    preview: resolvePreview(id),
    undo: resolveUndo(id),
    compensation: resolveCompensation(id)
  };
}

/**
 * Read-effect capabilities mutate nothing, so undo and compensation are
 * meaningless for them regardless of what a ledger entry might say.
 */
export function resolveBehaviorSemantics(
  id: string,
  behavior: CapabilityBehaviorSource
): CapabilityBehavior {
  const semantics = resolveCapabilitySemantics(id);
  return {
    ...behavior,
    supportsPreview: semantics.preview.mode !== 'none',
    supportsUndo: semantics.undo.mode === 'transaction',
    semantics
  };
}

/**
 * Every `inverse-capability` target must resolve to a real capability, so a
 * renamed or deleted capability cannot leave a record pointing at cleanup that
 * no longer exists.
 */
export function findUnknownCompensationTargets(
  knownIds: ReadonlySet<string>
): readonly { readonly id: string; readonly missing: string }[] {
  const unknown: { id: string; missing: string }[] = [];
  for (const [id, entry] of COMPENSATION_EVIDENCE) {
    if (entry.kind !== 'inverse') continue;
    for (const target of entry.inverse) {
      if (!knownIds.has(target)) unknown.push({ id, missing: target });
    }
  }
  return unknown;
}

/**
 * Ledger keys must name real capabilities too; a stale key would silently stop
 * granting its claim after a rename.
 */
export function findUnknownLedgerKeys(knownIds: ReadonlySet<string>): readonly string[] {
  const keys = [
    ...PREVIEW_EVIDENCE.keys(),
    ...UNDO_EVIDENCE.keys(),
    ...COMPENSATION_EVIDENCE.keys()
  ];
  return [...new Set(keys.filter((key) => !knownIds.has(key)))].sort();
}
