// scripts/canonical-registry/summary-drift.ts
//
// Record-level (summaries) drift detection for the canonical registry. Pure:
// receives typed summaries and appends RegistryDriftEntry rows to the shared
// accumulator. Detects dropped (expected-only), extra (actual-only), and
// field-mutated summaries, while refusing to silently collapse duplicate
// actual ids via a position-aware index.

import type { CanonicalRecordSummary } from './types.js';
import type { RegistryDriftEntry } from '../generate-canonical-registry.js';

const SUMMARY_FIELDS = [
  'parentTool',
  'dispatchAction',
  'domain',
  'schemaHash',
  'contentHash',
] as const;

export function compareSummaryDrift(
  entries: RegistryDriftEntry[],
  expList: readonly CanonicalRecordSummary[],
  actList: readonly CanonicalRecordSummary[],
): void {
  // Map expected id -> first actual position. Duplicate actual ids share the
  // same id key, so position-based consumption below is what prevents them
  // from being silently collapsed: each actual position is consumed at most
  // once, leaving the duplicate copy unmatched and therefore reported.
  const actPosById = new Map<string, number>();
  actList.forEach((s, i) => {
    if (!actPosById.has(s.id)) actPosById.set(s.id, i);
  });

  // Actual positions consumed by an expected match; positions left over are
  // extra (actual-only) summaries.
  const consumed = new Set<number>();
  for (let i = 0; i < expList.length; i += 1) {
    const e = expList[i];
    const aPos = actPosById.get(e.id);
    if (aPos === undefined) {
      entries.push({ id: e.id, pointer: `/summaries/${i}` });
      continue;
    }
    consumed.add(aPos);
    const a = actList[aPos];
    for (const key of SUMMARY_FIELDS) {
      if (e[key] !== a[key]) {
        entries.push({ id: e.id, pointer: `/summaries/${i}/${key}` });
      }
    }
  }

  // Extra summaries present only in actual are reported at their deterministic
  // actual position. A duplicate actual id whose position was not consumed is
  // reported here rather than collapsed.
  for (let j = 0; j < actList.length; j += 1) {
    if (!consumed.has(j)) {
      entries.push({ id: actList[j].id, pointer: `/summaries/${j}` });
    }
  }
}
