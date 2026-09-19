// scripts/gateway-manifest/hash.ts
// Content hashing for manifest output: sha256 over the bytes it is handed.
//
// It does NOT reuse the capability layer's stableJsonStringify, and its callers
// (pilotJson / pilotTsText) use plain JSON.stringify, so keys are NOT sorted and
// numbers are NOT normalized. These hashes are reproducible because
// buildPilotManifest assembles every object in a fixed order, not because the
// serialization is canonical -- so reordering the keys of a record's schema does
// change the hash, and the pilot freeze gate will (correctly) fail.

import { createHash } from 'node:crypto';

export function hashManifestContent(content: string): string {
  return createHash('sha256').update(content).digest('hex');
}
