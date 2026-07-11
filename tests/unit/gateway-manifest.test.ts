// tests/unit/gateway-manifest.test.ts
// Neutral gateway manifest: deterministic from canonical TS defs, consumed by the TS
// gateway, and mirrored exactly by the native MCP Gateway header (no hand-maintained drift).

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';
import { consolidatedToolDefinitions } from '../../src/tools/catalog/consolidated-tool-definitions.js';
import { prettyManifest } from '../../scripts/generate-gateway-manifest.js';
import { getGatewayManifestTools, getManifestToolDefinitions } from '../../src/gateway/gateway-manifest.js';
import { describeGatewayCapability, searchGatewayCatalog } from '../../src/server/tool-registry-gateway.js';

const manifestPath = resolve(process.cwd(), 'src/gateway/gateway-manifest.generated.json');
const nativeHeaderPath = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Gateway/McpNativeGatewayManifest.h');

const nativeManifestJson = (src: string): { tools: Array<{ name: string; actions: string[] }> } => {
  const m = src.match(/R"MCPGWMANIFEST\(([\s\S]*?)\)MCPGWMANIFEST"/);
  if (!m) throw new Error('native manifest raw-string not found');
  return JSON.parse(m[1]);
};

const defActions = (def: (typeof consolidatedToolDefinitions)[number]): string[] => {
  const props = (def.inputSchema.properties ?? {}) as Record<string, unknown>;
  const e = (props.action as { enum?: unknown[] } | undefined)?.enum ?? [];
  return [...e].filter((v): v is string => typeof v === 'string');
};

const actionsByName = new Map(consolidatedToolDefinitions.map((d) => [d.name, defActions(d)]));

describe('gateway manifest', () => {
  it('is deterministic and matches the committed asset (no drift)', () => {
    const once = prettyManifest(consolidatedToolDefinitions);
    expect(once).toBe(prettyManifest(consolidatedToolDefinitions));
    expect(JSON.parse(readFileSync(manifestPath, 'utf8'))).toEqual(JSON.parse(once));
  });

  it('mirrors every canonical tool with stable metadata', () => {
    const tools = JSON.parse(prettyManifest(consolidatedToolDefinitions)).tools as Array<{
      name: string; category: string | null; actions: string[]; parameterNames: string[];
    }>;
    expect(tools).toHaveLength(consolidatedToolDefinitions.length);
    expect(tools.map((t) => t.name).sort()).toEqual(consolidatedToolDefinitions.map((d) => d.name).sort());
    for (const def of consolidatedToolDefinitions) {
      const tool = tools.find((t) => t.name === def.name);
      expect(tool?.category).toBe(def.category ?? null);
      expect(tool?.actions).toEqual(actionsByName.get(def.name));
    }
  });

  it('TS gateway search/describe consume the manifest', () => {
    const search = searchGatewayCatalog({ limit: 25 }) as { total: number; results: Array<{ name: string }> };
    expect(search.total).toBe(getManifestToolDefinitions().length);
    expect(search.results.map((r) => r.name).sort()).toEqual(getGatewayManifestTools().map((t) => t.name).sort());
    const tool = getGatewayManifestTools()[0];
    const desc = describeGatewayCapability({ tool: tool.name }) as { perActionSchemas: boolean; actions: string[] };
    expect(desc.perActionSchemas).toBe(false);
    expect(desc.actions).toEqual(tool.actions);
  });

  it('native MCP Gateway embeds the identical manifest contract', () => {
    const native = nativeManifestJson(readFileSync(nativeHeaderPath, 'utf8'));
    expect(native.tools).toHaveLength(consolidatedToolDefinitions.length);
    for (const tool of native.tools) expect(tool.actions).toEqual(actionsByName.get(tool.name));
  });

  it('native manifest assigns a wide (TEXT-wrapped) raw string to TCHAR* (UE5.7 safe)', () => {
    const h = readFileSync(nativeHeaderPath, 'utf8');
    // Must wrap the raw literal in TEXT(...) so the compiler forms a wide raw
    // string literal (LR"..."). On UE5.7 TCHAR is char16_t; a bare narrow
    // raw literal (R"...") cannot bind to const TCHAR* and fails to compile.
    expect(/\bGatewayManifestJson\s*=\s*TEXT\(R"MCPGWMANIFEST\(/.test(h)).toBe(true);
    // Regression guard: a bare narrow raw string assigned to TCHAR* is the bug.
    expect(/\bGatewayManifestJson\s*=\s*R"MCPGWMANIFEST\(/.test(h)).toBe(false);
  });
});
