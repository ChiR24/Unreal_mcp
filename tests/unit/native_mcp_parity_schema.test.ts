/// <reference types="node" />

import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { afterEach, describe, expect, it } from 'vitest';

const temporaryDirectories: string[] = [];

function writeFile(root: string, relativePath: string, source: string): void {
  const filePath = path.join(root, relativePath);
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, source);
}

function createFixture(
  typeScriptProperties: string,
  nativeFields: string,
  required = "['action']"
): string {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'native-parity-schema-'));
  temporaryDirectories.push(root);
  writeFile(root, 'src/tools/definitions/shared/action-sets.ts', '');
  writeFile(
    root,
    'src/tools/definitions/utility/alpha-tool.ts',
    [
      'export const alphaToolDefinition = {',
      "  name: 'alpha',",
      '  inputSchema: {',
      `    properties: { action: { type: 'string', enum: ['run'], description: 'Action' }, ${typeScriptProperties} },`,
      `    required: ${required}`,
      '  }',
      '};',
      ''
    ].join('\n')
  );
  writeFile(
    root,
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Registry/McpToolRegistry.cpp',
    'const TArray<FString> CanonicalToolNames = { TEXT("alpha") };\n'
  );
  writeFile(
    root,
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Routing/McpConsolidatedActionRoutingFixture.h',
    ''
  );
  writeFile(
    root,
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/Utility/McpTool_Alpha.cpp',
    [
      'class FAlphaTool {',
      '  FString GetName() const override { return TEXT("alpha"); }',
      '  auto GetSchema() {',
      '    FMcpSchemaBuilder Schema;',
      '    Schema.StringEnum(TEXT("action"), { TEXT("run") }, TEXT("Action"));',
      `    ${nativeFields}`,
      '    Schema.Required({ TEXT("action") });',
      '    return Schema.Build();',
      '  }',
      '};',
      ''
    ].join('\n')
  );
  return root;
}

afterEach(() => {
  for (const directory of temporaryDirectories.splice(0)) {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

describe('native MCP schema parity', () => {
  it('declares every supported cinematics and media runtime field', async () => {
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    const result = auditNativeMcpParity({
      repoRoot: process.cwd(),
      schemaParityTools: ['manage_sequence']
    });
    const manageSequence = result.schemaPropertyGaps.find(
      ({ tool }) => tool === 'manage_sequence'
    );

    expect(manageSequence).toBeUndefined();
    const typeScriptSource = fs.readFileSync(
      path.join(
        process.cwd(),
        'src/tools/definitions/utility/manage-sequence-tool.ts'
      ),
      'utf8'
    );
    for (const field of [
      'defaultSourcePath',
      'platformSources',
      'playlistIndex',
      'takeSequencePath',
      'recordInto'
    ]) {
      expect(typeScriptSource).toContain(`${field}:`);
    }
    for (const field of [
      'frame',
      'width',
      'height',
      'startFrame',
      'endFrame',
      'lengthInFrames',
      'playbackStart',
      'playbackEnd'
    ]) {
      expect(typeScriptSource).toContain(
        `${field}: commonSchemas.integerProp`
      );
    }
  });

  it('does not let an unused same-prefix sibling satisfy a property', async () => {
    // Given
    const root = createFixture("frameRate: { type: 'number' }", '');
    writeFile(
      root,
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/Utility/McpTool_AlphaUnused.cpp',
      'void AddUnused(FMcpSchemaBuilder& Schema) { Schema.Number(TEXT("frameRate")); }\n'
    );
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    // When
    const result = auditNativeMcpParity({ repoRoot: root, schemaParityTools: ['alpha'] });

    // Then
    expect(result.schemaPropertyGaps).toEqual([
      {
        tool: 'alpha',
        missingNativeProperties: ['frameRate'],
        extraNativeProperties: [],
        schemaMismatches: []
      }
    ]);
  });

  it('reports property type and requiredness mismatches', async () => {
    // Given
    const root = createFixture(
      "frameRate: { type: 'number' }, value: { type: 'object' }",
      'Schema.String(TEXT("frameRate"), TEXT("Rate")).FreeformObject(TEXT("value"), TEXT("Value"));',
      "['action', 'frameRate']"
    );
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    // When
    const result = auditNativeMcpParity({ repoRoot: root, schemaParityTools: ['alpha'] });

    // Then
    expect(result.schemaPropertyGaps[0]?.schemaMismatches).toEqual([
      { path: 'frameRate.type', typeScript: 'number', native: 'string' },
      { path: 'required', typeScript: ['action', 'frameRate'], native: ['action'] }
    ]);
  });

  it('compares enum values, array items, and nested object shapes', async () => {
    // Given
    const root = createFixture(
      [
        "modes: { type: 'array', items: { type: 'string', enum: ['a', 'b'] } },",
        "settings: { type: 'object', properties: { enabled: { type: 'boolean' } } }"
      ].join(' '),
      [
        'Schema.Array(TEXT("modes"), TEXT("Modes"), TEXT("number"));',
        'Schema.Object(TEXT("settings"), TEXT("Settings"), [](FMcpSchemaBuilder& S) {',
        '  S.String(TEXT("enabled"), TEXT("Enabled"));',
        '});'
      ].join('\n')
    );
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    // When
    const result = auditNativeMcpParity({ repoRoot: root, schemaParityTools: ['alpha'] });

    // Then
    expect(result.schemaPropertyGaps[0]?.schemaMismatches).toEqual([
      { path: 'modes.items.enum', typeScript: ['a', 'b'], native: [] },
      { path: 'modes.items.type', typeScript: 'string', native: 'number' },
      { path: 'settings.enabled.type', typeScript: 'boolean', native: 'string' }
    ]);
  });

  it('follows an explicitly included and called schema field implementation', async () => {
    // Given
    const root = createFixture("frameRate: { type: 'number' }", [
      'McpAlphaFields::AddFields(Schema);'
    ].join('\n'));
    const toolsRoot = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools';
    const toolPath = `${toolsRoot}/Utility/McpTool_Alpha.cpp`;
    const toolSource = fs.readFileSync(path.join(root, toolPath), 'utf8');
    writeFile(
      root,
      toolPath,
      `#include "MCP/Tools/Utility/McpAlphaFields.h"\n${toolSource}`
    );
    writeFile(
      root,
      `${toolsRoot}/Utility/McpAlphaFields.h`,
      'namespace McpAlphaFields { void AddFields(FMcpSchemaBuilder& Schema); }\n'
    );
    writeFile(
      root,
      `${toolsRoot}/Utility/McpAlphaFields.cpp`,
      [
        '#include "MCP/Tools/Utility/McpAlphaFields.h"',
        'namespace McpAlphaFields {',
        'void AddFields(FMcpSchemaBuilder& Schema) { Schema.Number(TEXT("frameRate")); }',
        '}',
        ''
      ].join('\n')
    );
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    // When
    const result = auditNativeMcpParity({ repoRoot: root, schemaParityTools: ['alpha'] });

    // Then
    expect(result.schemaPropertyGaps).toEqual([]);
    expect(result.hasMismatches).toBe(false);
  });

  it('extracts raw any-value and type-union schema helper calls', async () => {
    // Given
    const root = createFixture(
      "value: { description: 'Any' }, frameRate: { type: ['number', 'string'] }",
      [
        'AddAnyValue(Schema, TEXT("value"), TEXT("Any"));',
        'AddTypeUnion(Schema, TEXT("frameRate"), { TEXT("number"), TEXT("string") }, TEXT("Rate"));'
      ].join('\n')
    );
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    // When
    const result = auditNativeMcpParity({ repoRoot: root, schemaParityTools: ['alpha'] });

    // Then
    expect(result.schemaPropertyGaps).toEqual([]);
  });
});
