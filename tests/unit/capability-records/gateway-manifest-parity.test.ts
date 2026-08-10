/**
 * tests/unit/capability-records/gateway-manifest-parity.test.ts
 *
 * TASK 29 GATE - the DEFAULT surface both transports actually serve.
 *
 * Both transports default to gateway mode, so what a client really sees is the
 * embedded gateway manifest -- not the legacy per-tool `BuildInputSchema`.
 * `npm run manifest:check` only byte-compares each artifact against a freshly
 * rendered copy of itself; it never asserts that the TypeScript payload and the
 * C++ payload carry the SAME DATA. Two artifacts can each be individually
 * "up to date" while disagreeing, and nothing in CI would notice.
 *
 * This gate closes that hole with a zero-tolerance structural comparison of the
 * two embedded payloads for all 23 canonical parents: every tool name, its
 * category, description, action list, parameter-name catalog, and the complete
 * input schema. It also pins `perActionSchemas` to false, because progressive
 * describe (tool -> action -> single param) depends on the parameter catalog
 * being the tool-union rather than per-action schemas.
 *
 * This is a static artifact comparison. It proves the two shipped manifests
 * agree; it does NOT exercise a running Unreal Editor.
 */
import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

const TS_MANIFEST_PATH = resolve(process.cwd(), 'src/gateway/gateway-manifest.generated.json');
const NATIVE_HEADER_PATH = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Gateway/McpNativeGatewayManifest.h',
);

const EXPECTED_PARENTS = 23;

interface ManifestTool {
  readonly name: string;
  readonly category?: string;
  readonly description?: string;
  readonly actions?: readonly string[];
  readonly parameterNames?: readonly string[];
  readonly inputSchema?: Record<string, unknown>;
}

interface GatewayManifest {
  readonly version: number;
  readonly source: string;
  readonly tools: readonly ManifestTool[];
}

const readTypeScriptManifest = (): GatewayManifest =>
  JSON.parse(readFileSync(TS_MANIFEST_PATH, 'utf8')) as GatewayManifest;

/**
 * The native header embeds the compact manifest verbatim inside a
 * `R"MCPGWMANIFEST( ... )MCPGWMANIFEST"` raw string literal. Reading the
 * literal back is the only way to see the bytes the plugin will parse at
 * runtime; anything else would compare the generator to itself.
 */
const readNativeManifest = (): GatewayManifest => {
  const header = readFileSync(NATIVE_HEADER_PATH, 'utf8');
  const openDelimiter = 'R"MCPGWMANIFEST(';
  const closeDelimiter = ')MCPGWMANIFEST"';
  const start = header.indexOf(openDelimiter);
  const end = header.indexOf(closeDelimiter, start);
  if (start < 0 || end < 0) {
    throw new Error(`No MCPGWMANIFEST raw-string payload in ${NATIVE_HEADER_PATH}`);
  }
  return JSON.parse(header.slice(start + openDelimiter.length, end)) as GatewayManifest;
};

describe('task 29 - default gateway manifest parity (TS vs native)', () => {
  it('GREEN: both embedded payloads are structurally identical', () => {
    // Given the two shipped gateway manifests
    const typeScript = readTypeScriptManifest();
    const native = readNativeManifest();

    // Then they carry the same envelope and the same tools, field for field
    expect(native.version).toBe(typeScript.version);
    expect(native.source).toBe(typeScript.source);
    expect(native).toEqual(typeScript);
  });

  it('GREEN: the shared payload covers exactly the 23 canonical parents', async () => {
    // Given the canonical parent list and the two manifests
    const { consolidatedToolDefinitions } = await import(
      '../../../src/tools/catalog/consolidated-tool-definitions.js'
    );
    const canonicalNames = consolidatedToolDefinitions.map((tool) => tool.name).sort();
    const typeScript = readTypeScriptManifest();
    const native = readNativeManifest();

    // Then discovery is non-vacuous and both sides list the same 23 names
    expect(canonicalNames.length).toBe(EXPECTED_PARENTS);
    expect(typeScript.tools.length).toBe(EXPECTED_PARENTS);
    expect(typeScript.tools.map((tool) => tool.name).sort()).toEqual(canonicalNames);
    expect(native.tools.map((tool) => tool.name).sort()).toEqual(canonicalNames);
  });

  it('GREEN: every parent agrees on actions, parameter catalog, and input schema', () => {
    // Given both manifests indexed by tool name
    const typeScript = readTypeScriptManifest();
    const nativeByName = new Map(readNativeManifest().tools.map((tool) => [tool.name, tool]));

    // Then no parent drifts on any discovery-bearing field
    for (const tool of typeScript.tools) {
      const native = nativeByName.get(tool.name);
      expect(native, `native manifest is missing ${tool.name}`).toBeDefined();
      expect(native?.category, `${tool.name}.category`).toEqual(tool.category);
      expect(native?.description, `${tool.name}.description`).toEqual(tool.description);
      expect(native?.actions, `${tool.name}.actions`).toEqual(tool.actions);
      expect(native?.parameterNames, `${tool.name}.parameterNames`).toEqual(tool.parameterNames);
      expect(native?.inputSchema, `${tool.name}.inputSchema`).toEqual(tool.inputSchema);
    }
  });

  it('GREEN: the disputed schemas survive into the manifest unnarrowed', () => {
    // Given the manifest payload both transports actually serve
    const nativeByName = new Map(readNativeManifest().tools.map((tool) => [tool.name, tool]));
    const propertyOf = (tool: string, property: string): Record<string, unknown> | undefined => {
      const properties = nativeByName.get(tool)?.inputSchema?.properties;
      if (typeof properties !== 'object' || properties === null) return undefined;
      return (properties as Record<string, Record<string, unknown>>)[property];
    };

    // Then the permissive array items stay permissive
    expect(propertyOf('manage_networking', 'states')?.items).toEqual({});

    // And each scalar union keeps every branch it was authored with
    for (const [tool, property] of [
      ['manage_asset', 'path'],
      ['manage_blueprint', 'nodeType'],
      ['system_control', 'mode'],
      ['system_control', 'type'],
    ] as const) {
      const branches = propertyOf(tool, property)?.oneOf;
      expect(Array.isArray(branches), `${tool}.${property}.oneOf`).toBe(true);
      expect((branches as readonly unknown[]).length).toBeGreaterThan(1);
    }
  });

  it('GREEN: native progressive describe never promises per-action schemas', () => {
    // Given the native gateway describe implementation
    const header = readFileSync(NATIVE_HEADER_PATH, 'utf8');

    // Then every describe response pins perActionSchemas to false, and the
    // three drill-down scopes (tool -> union catalog -> single param) are intact
    expect(header).toContain('SetBoolField(TEXT("perActionSchemas"), false)');
    expect(header).not.toContain('SetBoolField(TEXT("perActionSchemas"), true)');
    expect(header).toContain('SetStringField(TEXT("scope"), TEXT("tool"))');
    expect(header).toContain('SetStringField(TEXT("scope"), TEXT("union"))');
  });
});
