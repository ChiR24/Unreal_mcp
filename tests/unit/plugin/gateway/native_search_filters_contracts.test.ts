/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { unrealGatewayToolDefinition } from '../../../../src/tools/catalog/unreal-gateway-definition.js';
import { searchCapabilities } from './native-discovery-search.js';

// The server instructions tell every client to "narrow with a filter (domain,
// tool, effect)". The TS gateway honoured all three; the native /mcp gateway
// declared no `effect` and ignored both `effect` and `tool` on search, so a
// filtered search silently came back unfiltered.

const privateDir = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
const read = (path: string): string => readFileSync(resolve(privateDir, path), 'utf8');

interface SearchOut {
  readonly errorCode?: string;
  readonly suggestions?: readonly string[];
  readonly effect?: string;
  readonly tool?: string;
  readonly results?: readonly { readonly effect: string; readonly parent: string }[];
}

describe('native search filters match the TS gateway', () => {
  it('declares the effect filter in both gateway schemas', () => {
    expect(unrealGatewayToolDefinition.inputSchema.properties).toHaveProperty('effect');
    expect(read('MCP/Gateway/McpNativeGatewayDefinition.cpp')).toMatch(/StringEnum\(TEXT\("effect"\)/);
  });

  it('parses effect and tool for search on both native entry points', () => {
    for (const path of ['MCP/Transport/McpNativeTransportGateway.cpp', 'MCP/Primitives/McpTaskMethods.cpp']) {
      const source = read(path);
      expect(source, path).toMatch(/bHasEffect = \w+->TryGetStringField\(TEXT\("effect"\)/);
      expect(source, path).toMatch(/bHasTool = \w+->TryGetStringField\(TEXT\("tool"\)/);
    }
  });

  it('keeps only matching rows and echoes both filters', () => {
    const out = searchCapabilities({ query: 'material info', effect: 'read', tool: 'manage_asset' }) as SearchOut;
    expect(out.results?.length).toBeGreaterThan(0);
    expect(out.results?.every((row) => row.effect === 'read' && row.parent === 'manage_asset')).toBe(true);
    expect(out).toMatchObject({ effect: 'read', tool: 'manage_asset' });
  });

  it('refuses an unknown effect or tool with the closest declared value', () => {
    const effect = searchCapabilities({ query: 'spawn', effect: 'reads' }) as SearchOut;
    expect(effect.errorCode).toBe('UNKNOWN_EFFECT');
    expect(effect.suggestions?.[0]).toBe('read');
    const tool = searchCapabilities({ query: 'spawn', tool: 'control_actors' }) as SearchOut;
    expect(tool.errorCode).toBe('UNKNOWN_TOOL');
    expect(tool.suggestions?.[0]).toBe('control_actor');
  });
});
