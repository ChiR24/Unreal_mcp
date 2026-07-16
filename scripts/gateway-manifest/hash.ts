// scripts/gateway-manifest/hash.ts
// Deterministic content hashing for manifest output. Reuses stableJsonStringify
// from the capability layer so pilot hashes match the same canonical
// serialization rules (sorted keys, normalized numbers).

import { createHash } from 'node:crypto';

export function hashManifestContent(content: string): string {
  return createHash('sha256').update(content).digest('hex');
}
