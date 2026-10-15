/**
 * Source contract for describe on the native /mcp gateway: it takes a capability ID as execute does (the ID every
 * search row and execute nextCall carries). The native door used to ignore the field, so describe {capability}
 * answered with the whole tool list. The C++ cannot run here.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const gateway = readFileSync(
  resolve(process.cwd(), 'plugins/EngineWire/Source/EngineWire/Private/MCP/Transport/McpNativeTransportGateway.cpp'),
  'utf8'
);

describe('native gateway describe', () => {
  it('resolves a capability ID (or a unique alias) to its tool and action, and refuses an unknown one', () => {
    const branch = gateway.slice(gateway.indexOf('if (Operation == TEXT("describe"))'), gateway.indexOf('if (Operation == TEXT("configure"))'));
    expect(branch).toContain('Params->TryGetStringField(TEXT("capability"), Capability)');
    expect(branch).toContain('Index.FindById(Capability)');
    expect(branch).toContain('Index.ResolveAlias(Capability, AliasTarget) == EMcpAliasResolution::Unique');
    expect(branch).toContain('TEXT("UNKNOWN_CAPABILITY")');
    expect(branch).toContain('DiscoveryQuery.Action = McpCapabilityPublicAction(*Record);');
  });
});
