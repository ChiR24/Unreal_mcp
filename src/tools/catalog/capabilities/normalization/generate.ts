/**
 * Orchestrator: build the inventory from authoritative source and self-validate
 * before returning, so any structural defect fails at generation time.
 */
import { buildInventory } from './build.js';
import { stableStringify } from './io.js';
import type { NormalizationInventory } from './types.js';
import { validateInventoryData } from './validate.js';

export function generateInventory(): NormalizationInventory {
  const artifact = buildInventory();
  // Re-parse + validate the serialized form to catch ordering/collision bugs.
  return validateInventoryData(JSON.parse(stableStringify(artifact)));
}
