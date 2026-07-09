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

function toolSource(name: string): string {
  return [
    `export const ${name.replace(/_/g, '')}ToolDefinition = {`,
    `  name: '${name}',`,
    "  inputSchema: { properties: { action: { enum: ['run'], description: 'Action' } }, required: ['action'] }",
    '};',
    ''
  ].join('\n');
}

function nativeToolSource(name: string): string {
  return [
    'class FFixtureTool {',
    `  FString GetName() const override { return TEXT("${name}"); }`,
    '  auto GetSchema() {',
    '    return Builder.StringEnum(TEXT("action"), { TEXT("run") }, TEXT("Action"));',
    '  }',
    '};',
    ''
  ].join('\n');
}

function createParityFixture(
  typeScriptNames: readonly string[],
  registryNames: readonly string[],
  nativeDefinitionNames: readonly string[]
): string {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'native-parity-audit-'));
  temporaryDirectories.push(root);

  writeFile(
    root,
    'src/tools/definitions/shared/action-sets.ts',
    "export const UNUSED_ACTIONS = ['unused'] as const;\n"
  );
  typeScriptNames.forEach((name, index) => {
    writeFile(
      root,
      `src/tools/definitions/group-${index % 2}/${index}-${name}-tool.ts`,
      toolSource(name)
    );
  });

  const registryValues = registryNames.map((name) => `TEXT("${name}")`).join(', ');
  writeFile(
    root,
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Registry/McpToolRegistry.cpp',
    `const TArray<FString> CanonicalToolNames = { ${registryValues} };\n`
  );
  writeFile(
    root,
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Routing/McpConsolidatedActionRoutingFixture.h',
    ''
  );
  nativeDefinitionNames.forEach((name, index) => {
    writeFile(
      root,
      `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/group-${index % 2}/${index}-${name}.cpp`,
      nativeToolSource(name)
    );
  });

  return root;
}

function createClassSlicingFixture(fileName: string, alphaMembers: readonly string[]): string {
  const root = createParityFixture(['alpha', 'beta'], ['alpha', 'beta'], []);
  writeFile(
    root,
    `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/${fileName}`,
    [
      'class FAlphaTool {',
      ...alphaMembers,
      '};',
      'class FBetaTool {',
      '  FString GetName() const override { return TEXT("beta"); }',
      '  auto GetSchema() {',
      '    return Builder.StringEnum(TEXT("action"), { TEXT("run") }, TEXT("Action"));',
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

describe('native MCP parity audit', () => {
  it('fails when 24 TypeScript definitions collapse to 23 unique names', async () => {
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');
    const canonicalNames = Array.from({ length: 23 }, (_, index) => `tool_${index + 1}`);
    const root = createParityFixture(
      [...canonicalNames, canonicalNames[22] ?? 'tool_23'],
      canonicalNames,
      canonicalNames
    );

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.counts).toEqual({
      typeScriptDefinitions: 24,
      uniqueTypeScriptNames: 23,
      nativeRegistryEntries: 23,
      uniqueNativeRegistryNames: 23,
      nativeDefinitions: 23,
      uniqueNativeDefinitionNames: 23,
      toolsWithSchemaPropertyParity: 1
    });
    expect(result.schemaParityTools).toEqual(['manage_sequence']);
    expect(result.duplicateNames.typeScriptTools).toEqual(['tool_23']);
    expect(result.hasMismatches).toBe(true);
  });

  it('reports duplicate native registry and GetName entries before indexing', async () => {
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');
    const root = createParityFixture(
      ['alpha', 'beta'],
      ['alpha', 'alpha', 'beta'],
      ['alpha', 'alpha', 'beta']
    );

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.duplicateNames.nativeCanonicalRegistry).toEqual(['alpha']);
    expect(result.duplicateNames.nativeToolDefinitions).toEqual(['alpha']);
    expect(result.hasMismatches).toBe(true);
  });

  it('excludes non-canonical native definitions from parity counts', async () => {
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');
    const root = createParityFixture(
      ['alpha', 'beta'],
      ['alpha', 'beta'],
      ['alpha', 'beta', 'legacy_tool']
    );

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.counts.nativeDefinitions).toBe(2);
    expect(result.hasMismatches).toBe(false);
  });

  it('attributes action enums to the matching class in multi-tool source files', async () => {
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');
    const root = createParityFixture(['alpha', 'beta'], ['alpha', 'beta'], []);
    writeFile(
      root,
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/multi.cpp',
      [
        'class FAlphaTool {',
        '  FString GetName() const override { return TEXT("alpha"); }',
        '  auto GetSchema() {',
        '    return Builder.StringEnum(TEXT("action"), { TEXT("run") }, TEXT("Action"));',
        '  }',
        '};',
        'class FBetaTool {',
        '  FString GetName() const override { return TEXT("beta"); }',
        '  auto GetSchema() {',
        '    return Builder.StringEnum(TEXT("action"), { TEXT("stop") }, TEXT("Action"));',
        '  }',
        '};',
        '',
      ].join('\n'),
    );

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.actionGaps).toEqual([
      {
        tool: 'beta',
        missingNativeActions: ['run'],
        extraNativeActions: ['stop'],
      },
    ]);
  });

  it('prefers the tool definition action enum over slug-prefixed helpers', async () => {
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');
    const root = createParityFixture(['alpha'], ['alpha'], ['alpha']);
    writeFile(
      root,
      'src/tools/definitions/group-0/0-alpha-a-helper.ts',
      [
        'export const unrelated = {',
        "  action: { enum: ['fake'], description: 'Action' }",
        '};',
        '',
      ].join('\n'),
    );

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.actionGaps).toEqual([]);
    expect(result.hasMismatches).toBe(false);
  });

  it('ignores literal and comment braces when slicing a native tool class', async () => {
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');
    const root = createClassSlicingFixture('literals.cpp', [
      '  FString GetName() const override { return TEXT("alpha"); }',
      '  const TCHAR* DoubleQuoted = TEXT("{");',
      "  const TCHAR SingleQuoted = '{';",
      '  // { unmatched line-comment brace',
      '  /* { unmatched block-comment brace */'
    ]);

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.actionGaps).toEqual([
      {
        tool: 'alpha',
        missingNativeActions: ['run'],
        extraNativeActions: [],
      },
    ]);
  });

  it('ignores quotes and braces inside C++ raw string literals', async () => {
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');
    const root = createClassSlicingFixture('raw-literal.cpp', [
      '  FString GetName() const override { return TEXT("alpha"); }',
      '  const TCHAR* Payload = R"json("quoted { brace")json";'
    ]);

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.actionGaps).toEqual([
      { tool: 'alpha', missingNativeActions: ['run'], extraNativeActions: [] },
    ]);
  });

  it('selects the containing tool class after a closed nested type', async () => {
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');
    const root = createClassSlicingFixture('nested-class.cpp', [
      '  struct FMetadata {',
      '  };',
      '  auto GetSchema() {',
      '    return Builder.StringEnum(TEXT("action"), { TEXT("run") }, TEXT("Action"));',
      '  }',
      '  FString GetName() const override { return TEXT("alpha"); }'
    ]);

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.actionGaps).toEqual([]);
  });

  it('keeps backslash-continued physical lines inside line comments', async () => {
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');
    const root = createClassSlicingFixture('continued-comment.cpp', [
      '  FString GetName() const override { return TEXT("alpha"); }',
      '  // continued comment \\',
      '  {'
    ]);

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.actionGaps).toEqual([
      { tool: 'alpha', missingNativeActions: ['run'], extraNativeActions: [] },
    ]);
  });

});
