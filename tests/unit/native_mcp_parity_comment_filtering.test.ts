/// <reference types="node" />

import { describe, expect, it } from 'vitest';

import {
  createTrackedTempRoot,
  registerTempRootCleanup,
  writeFixtureFile
} from './audit-fixture-workspace.js';

registerTempRootCleanup();

// Loaded once at module scope (not per-test) so the heavy native-parity audit
// module is already resolved before the first test runs; a cold import inside the
// first test was exceeding the suite timeout under parallel CI load.
const auditPromise = import('../native-mcp-parity-audit.mjs');

const ACTIVE_TYPESCRIPT_TOOL = [
  'export const alphaToolDefinition = {',
  "  name: 'alpha',",
  "  inputSchema: { properties: { action: { enum: ['run'], description: 'Action' } } }",
  '};',
  ''
].join('\n');

const INLINE_NATIVE_TOOL = [
  'class FAlphaTool {',
  '  FString GetName() const override { return TEXT("alpha"); }',
  '  auto GetSchema() {',
  '    return Builder.StringEnum(TEXT("action"), { TEXT("run") }, TEXT("Action"));',
  '  }',
  '};',
  ''
].join('\n');

const ROUTED_NATIVE_TOOL = [
  'class FAlphaTool {',
  '  FString GetName() const override { return TEXT("alpha"); }',
  '  auto GetSchema() {',
  '    return Builder.StringEnum(',
  '      TEXT("action"), McpConsolidatedActions::AlphaActions(), TEXT("Action"));',
  '  }',
  '};',
  ''
].join('\n');

type FixtureSources = {
  readonly typeScript: string;
  readonly registry: string;
  readonly routing: string;
  readonly nativeTool: string;
};

function createFixture(sources: FixtureSources): string {
  const root = createTrackedTempRoot('native-parity-comments-');
  writeFixtureFile(root, 'src/tools/definitions/shared/action-sets.ts', '');
  writeFixtureFile(root, 'src/tools/definitions/alpha/alpha-tool.ts', sources.typeScript);
  writeFixtureFile(
    root,
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Registry/McpToolRegistry.cpp',
    sources.registry
  );
  writeFixtureFile(
    root,
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Routing/McpConsolidatedActionRoutingFixture.h',
    sources.routing
  );
  writeFixtureFile(
    root,
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/Alpha.cpp',
    sources.nativeTool
  );
  return root;
}

describe('native MCP parity comment filtering', () => {
  it('ignores TypeScript tool definitions inside block comments', async () => {
    const { auditNativeMcpParity } = await auditPromise;
    const root = createFixture({
      typeScript: `/*\n${ACTIVE_TYPESCRIPT_TOOL}*/\n`,
      registry: 'const TArray<FString> CanonicalToolNames = { TEXT("alpha") };\n',
      routing: '',
      nativeTool: INLINE_NATIVE_TOOL
    });

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.counts.typeScriptDefinitions).toBe(0);
    expect(result.toolNameGaps.extraInNativeRegistry).toEqual(['alpha']);
  });

  it('ignores CanonicalToolNames declarations inside line comments', async () => {
    const { auditNativeMcpParity } = await auditPromise;
    const root = createFixture({
      typeScript: ACTIVE_TYPESCRIPT_TOOL,
      registry: '// const TArray<FString> CanonicalToolNames = { TEXT("alpha") };\n',
      routing: '',
      nativeTool: INLINE_NATIVE_TOOL
    });

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.counts.nativeRegistryEntries).toBe(0);
    expect(result.toolNameGaps.missingFromNativeRegistry).toEqual(['alpha']);
  });

  it('ignores commented action lists inside native routing functions', async () => {
    const { auditNativeMcpParity } = await auditPromise;
    const root = createFixture({
      typeScript: ACTIVE_TYPESCRIPT_TOOL,
      registry: 'const TArray<FString> CanonicalToolNames = { TEXT("alpha") };\n',
      routing: [
        'inline const TArray<FString>& AlphaActions() {',
        '  // static const TArray<FString> Actions = { TEXT("run") };',
        '  static const TArray<FString> EmptyActions = {};',
        '  return EmptyActions;',
        '}',
        ''
      ].join('\n'),
      nativeTool: ROUTED_NATIVE_TOOL
    });

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.actionGaps).toEqual([
      { tool: 'alpha', missingNativeActions: ['run'], extraNativeActions: [] }
    ]);
  });

  it('ignores GetName and StringEnum tokens in comments and raw literals', async () => {
    const { auditNativeMcpParity } = await auditPromise;
    const root = createFixture({
      typeScript: ACTIVE_TYPESCRIPT_TOOL,
      registry: 'const TArray<FString> CanonicalToolNames = { TEXT("alpha") };\n',
      routing: '',
      nativeTool: [
        '// FString GetName() const override { return TEXT("alpha"); }',
        '// .StringEnum(TEXT("action"), { TEXT("run") }, TEXT("Action"));',
        'const TCHAR* Noise = R"cpp(FString GetName() const override { return TEXT("alpha"); })cpp";',
        ''
      ].join('\n')
    });

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.counts.nativeDefinitions).toBe(0);
    expect(result.toolNameGaps.missingNativeDefinitions).toEqual(['alpha']);
  });

  it('ignores multiline tool definitions inside raw string literals', async () => {
    const { auditNativeMcpParity } = await auditPromise;
    const root = createFixture({
      typeScript: ACTIVE_TYPESCRIPT_TOOL,
      registry: 'const TArray<FString> CanonicalToolNames = { TEXT("alpha") };\n',
      routing: '',
      nativeTool: [
        'const TCHAR* Fake = R"cpp(',
        'FString GetName() const override { return TEXT("alpha"); }',
        'Builder.StringEnum(TEXT("action"), { TEXT("run") }, TEXT("Action"));',
        ')cpp";',
        ''
      ].join('\n')
    });

    const result = auditNativeMcpParity({ repoRoot: root });

    expect(result.toolNameGaps.missingNativeDefinitions).toEqual(['alpha']);
    expect(result.hasMismatches).toBe(true);
  });
});
