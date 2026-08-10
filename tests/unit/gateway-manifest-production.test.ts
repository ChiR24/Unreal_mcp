// tests/unit/gateway-manifest-production.test.ts
// Production byte-stability and schema contracts for the gateway manifest.

import { describe, expect, it } from 'vitest';
import { compactManifest, prettyManifest } from '../../scripts/gateway-manifest/emit.js';
import { getGatewayManifest, getManifestToolDefinitions } from '../../src/gateway/gateway-manifest.js';
import { GatewayManifestSchema } from '../../src/gateway/gateway-manifest-types.js';
import { consolidatedToolDefinitions } from '../../src/tools/catalog/consolidated-tool-definitions.js';

describe('gateway-manifest production byte-stability', () => {
  it('prettyManifest is deterministic', () => {
    const once = prettyManifest(consolidatedToolDefinitions);
    const twice = prettyManifest(consolidatedToolDefinitions);
    expect(once).toBe(twice);
  });

  it('compactManifest is deterministic', () => {
    const once = compactManifest(consolidatedToolDefinitions);
    const twice = compactManifest(consolidatedToolDefinitions);
    expect(once).toBe(twice);
  });

  it('production manifest passes GatewayManifestSchema (strict)', () => {
    const manifest = getGatewayManifest();
    const result = GatewayManifestSchema.safeParse(manifest);
    expect(result.success).toBe(true);
  });

  it('getManifestToolDefinitions has no cast in category narrowing', () => {
    const defs = getManifestToolDefinitions();
    expect(defs).toHaveLength(consolidatedToolDefinitions.length);
    for (const def of defs) {
      if (def.category !== undefined) {
        expect(['core', 'world', 'gameplay', 'utility']).toContain(def.category);
      }
    }
  });
});
