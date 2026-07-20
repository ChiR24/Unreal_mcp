/**
 * tests/unit/native-mcp-parity-fixtures.ts
 *
 * Repository-shaped fixture builders for native_mcp_parity_audit.test.ts.
 * Extracted verbatim so the test file keeps only its assertions and stays
 * under the project 250 pure-LOC ceiling.
 */
/// <reference types="node" />

import { createTrackedTempRoot, writeFixtureFile } from './audit-fixture-workspace.js';

const NATIVE_TOOLS_ROOT =
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools';

/** A TypeScript parent definition exposing a single `run` action. */
export function toolSource(name: string): string {
  return [
    `export const ${name.replace(/_/g, '')}ToolDefinition = {`,
    `  name: '${name}',`,
    "  inputSchema: { properties: { action: { enum: ['run'], description: 'Action' } }, required: ['action'] }",
    '};',
    ''
  ].join('\n');
}

/** A native tool class exposing the matching single `run` action. */
export function nativeToolSource(name: string): string {
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

/** Builds a repo root whose three canonical surfaces declare exactly the given names. */
export function createParityFixture(
  typeScriptNames: readonly string[],
  registryNames: readonly string[],
  nativeDefinitionNames: readonly string[]
): string {
  const root = createTrackedTempRoot('native-parity-audit-');

  writeFixtureFile(
    root,
    'src/tools/definitions/shared/action-sets.ts',
    "export const UNUSED_ACTIONS = ['unused'] as const;\n"
  );
  typeScriptNames.forEach((name, index) => {
    writeFixtureFile(
      root,
      `src/tools/definitions/group-${index % 2}/${index}-${name}-tool.ts`,
      toolSource(name)
    );
  });

  const registryValues = registryNames.map((name) => `TEXT("${name}")`).join(', ');
  writeFixtureFile(
    root,
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Registry/McpToolRegistry.cpp',
    `const TArray<FString> CanonicalToolNames = { ${registryValues} };\n`
  );
  writeFixtureFile(
    root,
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Routing/McpConsolidatedActionRoutingFixture.h',
    ''
  );
  nativeDefinitionNames.forEach((name, index) => {
    writeFixtureFile(
      root,
      `${NATIVE_TOOLS_ROOT}/group-${index % 2}/${index}-${name}.cpp`,
      nativeToolSource(name)
    );
  });

  return root;
}

/**
 * An alpha/beta parity fixture whose alpha class body is supplied verbatim, so a
 * test can probe how the class slicer handles literals, comments, and nesting.
 */
export function createClassSlicingFixture(
  fileName: string,
  alphaMembers: readonly string[]
): string {
  const root = createParityFixture(['alpha', 'beta'], ['alpha', 'beta'], []);
  writeFixtureFile(
    root,
    `${NATIVE_TOOLS_ROOT}/${fileName}`,
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
