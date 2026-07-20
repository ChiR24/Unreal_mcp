/// <reference types="node" />

import fs from 'node:fs';
import path from 'node:path';
import { describe, expect, it } from 'vitest';

import {
  createTrackedTempRoot,
  registerTempRootCleanup,
  writeFixtureFile
} from './audit-fixture-workspace.js';
import {
  createClassSlicingFixture,
  createParityFixture
} from './native-mcp-parity-fixtures.js';

registerTempRootCleanup();

describe('native MCP parity audit', () => {
  it('discovers both real canonical surfaces instead of comparing nothing', async () => {
    // Given the real repository on both surfaces
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    // When parity is audited with no fixture root
    const result = auditNativeMcpParity({ repoRoot: process.cwd() });

    // Then all 23 canonical parents are compared, with no name or action drift
    expect(result.emptyDiscovery).toEqual([]);
    expect(result.counts.typeScriptDefinitions).toBe(23);
    expect(result.counts.uniqueTypeScriptNames).toBe(23);
    expect(result.counts.nativeDefinitions).toBe(23);
    expect(result.toolNameGaps).toEqual({
      missingFromNativeRegistry: [],
      extraInNativeRegistry: [],
      missingNativeDefinitions: []
    });
    expect(result.actionGaps).toEqual([]);
    expect(result.duplicateNames).toEqual({
      typeScriptTools: [],
      nativeCanonicalRegistry: [],
      nativeToolDefinitions: []
    });
  });

  it('fails closed when the TypeScript side discovers no definitions', async () => {
    // Given a repository whose parent definitions all vanished
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');
    const canonicalNames = Array.from({ length: 23 }, (_, index) => `tool_${index + 1}`);
    const root = createParityFixture([], canonicalNames, canonicalNames);

    // When parity is audited
    const result = auditNativeMcpParity({ repoRoot: root });

    // Then the empty side is itself a mismatch, not a clean comparison of nothing
    expect(result.counts.typeScriptDefinitions).toBe(0);
    expect(result.actionGaps).toEqual([]);
    expect(result.emptyDiscovery).toEqual(['typeScriptTools']);
    expect(result.hasMismatches).toBe(true);
  });

  it('fails closed when no native tool definitions are discovered', async () => {
    // Given a native Tools tree that declares no tool classes
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');
    const root = createParityFixture(['alpha'], ['alpha'], []);
    writeFixtureFile(
      root,
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/none.cpp',
      'void Unrelated() {}\n'
    );

    // When parity is audited
    const result = auditNativeMcpParity({ repoRoot: root });

    // Then the missing native side is reported rather than silently skipped
    expect(result.emptyDiscovery).toEqual(['nativeToolDefinitions']);
    expect(result.hasMismatches).toBe(true);
  });

  it('fails closed when the generated parent artifact is emptied', async () => {
    // Given a generated parent artifact that emits an empty definition list
    const { readGeneratedParentToolDefinitions } = await import('../parameter-audit-context.mjs');
    const root = createTrackedTempRoot('native-parity-empty-generated-');
    const artifactPath = path.join(root, 'parent-tool-definitions.generated.ts');
    fs.writeFileSync(
      artifactPath,
      'export const generatedParentToolDefinitions: readonly unknown[] = [];\n'
    );

    // When the parity parser loads it
    // Then it raises instead of handing the audit an empty TypeScript surface
    expect(() => readGeneratedParentToolDefinitions(artifactPath)).toThrow(
      /zero parent tool definitions/
    );
  });

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
    writeFixtureFile(
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
    writeFixtureFile(
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
