// scripts/gateway-manifest/sort.ts
// Deterministic sort for canonical pilot records ONLY.
// Production legacy definitions are NEVER sorted here - byte identity wins.
// Sorting by canonical ID guarantees two pilot runs with the same input
// produce identical JSON/TS/H output regardless of input file ordering.

import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { sortById } from '../../src/utils/serialization/ordering.js';

export function sortPilotRecords(records: readonly CapabilityRecord[]): readonly CapabilityRecord[] {
  return sortById(records);
}
