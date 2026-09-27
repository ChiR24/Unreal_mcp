// tests/unit/gateway-manifest-production.test.ts
// Production byte-stability and schema contracts for the gateway manifest.

import { describe, expect, it } from 'vitest';
import { getGatewayManifest, getManifestToolDefinitions } from '../../src/gateway/gateway-manifest.js';
import { GatewayManifestSchema } from '../../src/gateway/gateway-manifest-types.js';
import { generatedParentToolDefinitions } from '../../src/tools/catalog/capabilities/generated/parent-tool-definitions.generated.js';

describe('gateway-manifest production byte-stability', () => {
  it('production manifest passes GatewayManifestSchema (strict)', () => {
    const manifest = getGatewayManifest();
    const result = GatewayManifestSchema.safeParse(manifest);
    expect(result.success).toBe(true);
  });

  it('getManifestToolDefinitions has no cast in category narrowing', () => {
    const defs = getManifestToolDefinitions();
    expect(defs).toHaveLength(generatedParentToolDefinitions.length);
    for (const def of defs) {
      if (def.category !== undefined) {
        expect(['core', 'world', 'gameplay', 'utility']).toContain(def.category);
      }
    }
  });
});
