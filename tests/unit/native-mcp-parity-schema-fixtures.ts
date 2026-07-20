/**
 * tests/unit/native-mcp-parity-schema-fixtures.ts
 *
 * Fixture builder and generated-surface projections for
 * native_mcp_parity_schema.test.ts. Extracted verbatim so the test file keeps
 * only its assertions and stays under the project 250 pure-LOC ceiling.
 */
/// <reference types="node" />

import fs from 'node:fs';
import path from 'node:path';

import { createTrackedTempRoot, writeFixtureFile } from './audit-fixture-workspace.js';

export interface SchemaNode {
  readonly type?: unknown;
}

export interface CapabilityRecord {
  readonly legacyIds?: readonly { readonly tool: string }[];
  readonly schemas?: { readonly input?: { readonly properties?: Record<string, SchemaNode> } };
}

export interface ParentDefinition {
  readonly name: string;
  readonly inputSchema: { readonly properties: Record<string, SchemaNode> };
}

/**
 * Builds a single-tool repo root where the TypeScript `alpha` parent declares
 * `typeScriptProperties` and the native `alpha` tool declares `nativeFields`.
 */
export function createSchemaParityFixture(
  typeScriptProperties: string,
  nativeFields: string,
  required = "['action']"
): string {
  const root = createTrackedTempRoot('native-parity-schema-');
  writeFixtureFile(root, 'src/tools/definitions/shared/action-sets.ts', '');
  writeFixtureFile(
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
  writeFixtureFile(
    root,
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Registry/McpToolRegistry.cpp',
    'const TArray<FString> CanonicalToolNames = { TEXT("alpha") };\n'
  );
  writeFixtureFile(
    root,
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Routing/McpConsolidatedActionRoutingFixture.h',
    ''
  );
  writeFixtureFile(
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

/** Every generated capability record that maps back to the given parent tool. */
export function capabilityRecordsFor(tool: string): CapabilityRecord[] {
  const registry = JSON.parse(
    fs.readFileSync(
      path.join(
        process.cwd(),
        'src/tools/catalog/capabilities/generated/canonical-registry.generated.json'
      ),
      'utf8'
    )
  ) as { readonly records: readonly CapabilityRecord[] };

  return registry.records.filter(
    (record) => record.legacyIds?.some((legacyId) => legacyId.tool === tool) === true
  );
}

/** The generated parent definition for the given tool, or a hard failure. */
export async function generatedParentFor(tool: string): Promise<ParentDefinition> {
  const { readGeneratedParentToolDefinitions } = await import('../parameter-audit-context.mjs');
  const definition = (readGeneratedParentToolDefinitions() as ParentDefinition[]).find(
    (candidate) => candidate.name === tool
  );
  if (!definition) throw new Error(`No generated parent definition for ${tool}`);
  return definition;
}

/** Sorted property names excluding the ubiquitous `action` discriminator. */
export function propertyNames(properties: Record<string, SchemaNode>): string[] {
  return Object.keys(properties).filter((name) => name !== 'action').sort();
}
