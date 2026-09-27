// tests/unit/gateway-manifest.test.ts
// Neutral gateway manifest: deterministic from canonical TS defs and consumed by the TS gateway.

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';
import { generatedParentToolDefinitions } from '../../src/tools/catalog/capabilities/generated/parent-tool-definitions.generated.js';
import { prettyManifest } from '../../scripts/generate-gateway-manifest.js';
import { getGatewayManifestTools, getManifestToolDefinitions } from '../../src/gateway/gateway-manifest.js';
import { searchGatewayCapabilities as searchGatewayCatalog } from '../../src/server/gateway/gateway-search.js';
import { describeGatewayCapability } from '../../src/server/gateway/gateway-describe.js';
import { unrealGatewayToolDefinition } from '../../src/tools/catalog/unreal-gateway-definition.js';

const nativeGatewayDefinitionPath = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Gateway/McpNativeGatewayDefinition.cpp');

const defActions = (def: (typeof generatedParentToolDefinitions)[number]): string[] => {
  const props = (def.inputSchema.properties ?? {}) as Record<string, unknown>;
  const e = (props.action as { enum?: unknown[] } | undefined)?.enum ?? [];
  return [...e].filter((v): v is string => typeof v === 'string');
};

const actionsByName = new Map(generatedParentToolDefinitions.map((d) => [d.name, defActions(d)]));

describe('gateway manifest', () => {
  it('mirrors every canonical tool with stable metadata', () => {
    const tools = JSON.parse(prettyManifest(generatedParentToolDefinitions)).tools as Array<{
      name: string; category: string | null; actions: string[]; parameterNames: string[];
    }>;
    expect(tools).toHaveLength(generatedParentToolDefinitions.length);
    expect(tools.map((t) => t.name).sort()).toEqual(generatedParentToolDefinitions.map((d) => d.name).sort());
    for (const def of generatedParentToolDefinitions) {
      const tool = tools.find((t) => t.name === def.name);
      expect(tool?.category).toBe(def.category ?? null);
      expect(tool?.actions).toEqual(actionsByName.get(def.name));
    }
  });

  // Task 24 split the two consumers apart: the legacy parent-tool summary still
  // describes the manifest dispatch surface, while `search` now ranks canonical
  // capability records. Asserting search against the manifest would re-couple
  // discovery to the union schemas Task 24 removed from the contract.
  it('the legacy TS gateway tool summary still consumes the manifest', () => {
    const tool = getGatewayManifestTools()[0];
    const desc = describeGatewayCapability({ tool: tool.name }) as { perActionSchemas: boolean; actions: string[] };
    expect(desc.perActionSchemas).toBe(false);
    expect(desc.actions).toEqual(tool.actions);
  });

  it('TS gateway search covers every manifest tool through canonical capabilities', () => {
    const parents = new Set<string>();
    for (const tool of getManifestToolDefinitions()) {
      const search = searchGatewayCatalog({ tool: tool.name, limit: 1 }) as {
        total: number; results: Array<{ parentTool: string }>;
      };
      expect(search.total).toBeGreaterThan(0);
      parents.add(search.results[0].parentTool);
    }
    expect(parents.size).toBe(getManifestToolDefinitions().length);
  });

  it('native gateway tool definition mirrors the TS param selector schema', () => {
    const props = (unrealGatewayToolDefinition.inputSchema.properties ?? {}) as Record<string, { description?: string }>;
    expect(props.param).toBeDefined();
    const tsDescription = props.param?.description ?? '';
    expect(tsDescription.length).toBeGreaterThan(0);

    const cpp = readFileSync(nativeGatewayDefinitionPath, 'utf8');
    const match = cpp.match(/\.String\(\s*TEXT\("param"\),\s*TEXT\("([^"]*)"\)/);
    expect(match, 'native gateway definition must declare a param field').not.toBeNull();
    expect(match?.[1]).toBe(tsDescription);
  });
});
