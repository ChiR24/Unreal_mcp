// scripts/gateway-manifest/sort.ts
// Deterministic sort for canonical pilot records ONLY.
// Production legacy definitions are NEVER sorted here - byte identity wins.
// Sorting by canonical ID guarantees two pilot runs with the same input
// produce identical JSON/TS/H output regardless of input file ordering.

import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';

export function sortPilotRecords(records: readonly CapabilityRecord[]): readonly CapabilityRecord[] {
  return [...records].sort((a, b) => (a.id < b.id ? -1 : a.id > b.id ? 1 : 0));
}
