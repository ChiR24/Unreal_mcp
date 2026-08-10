/// <reference types="node" />

import fs from 'node:fs';
import path from 'node:path';
import { describe, expect, it } from 'vitest';

import { registerTempRootCleanup, writeFixtureFile } from './audit-fixture-workspace.js';
import {
  capabilityRecordsFor,
  createSchemaParityFixture,
  generatedParentFor,
  propertyNames
} from './native-mcp-parity-schema-fixtures.js';

registerTempRootCleanup();

describe('native MCP schema parity', () => {
  it('derives the manage_sequence parent property union from its capability records', async () => {
    // Given the generated parent surface and the records it is derived from
    const parent = await generatedParentFor('manage_sequence');
    const records = capabilityRecordsFor('manage_sequence');

    // When both property sets are collected
    const recordProperties = [
      ...new Set(records.flatMap((record) => Object.keys(record.schemas?.input?.properties ?? {})))
    ].filter((name) => name !== 'action').sort();

    // Then the parent declares exactly its records' union -- nothing invented, nothing dropped
    expect(records.length).toBeGreaterThan(0);
    expect(recordProperties.length).toBeGreaterThan(0);
    expect(propertyNames(parent.inputSchema.properties)).toEqual(recordProperties);
  });

  it('keeps manage_sequence cinematics, media, and take runtime fields typed as declared', async () => {
    // Given the generated parent and the records behind it
    const parent = await generatedParentFor('manage_sequence');
    const records = capabilityRecordsFor('manage_sequence');

    // When frame-addressed timeline fields are inspected on both layers
    for (const field of ['frame', 'lengthInFrames', 'playbackStart', 'playbackEnd']) {
      const declaring = records.filter((record) => record.schemas?.input?.properties?.[field]);

      // Then the parent and every declaring record agree the field is integer-typed
      expect(declaring.length).toBeGreaterThan(0);
      expect(parent.inputSchema.properties[field]?.type).toBe('integer');
      for (const record of declaring) {
        expect(record.schemas?.input?.properties?.[field]?.type).toBe('integer');
      }
    }

    // And the Take Recorder destination stays a string path
    expect(parent.inputSchema.properties.takeSequencePath?.type).toBe('string');
  });

  it('matches the native manage_sequence parent with no schema drift', async () => {
    // Given the real repository on both surfaces
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    // When manage_sequence schema parity is audited
    const result = auditNativeMcpParity({
      repoRoot: process.cwd(),
      schemaParityTools: ['manage_sequence']
    });
    const manageSequence = result.schemaPropertyGaps.find(
      ({ tool }) => tool === 'manage_sequence'
    );

    // Then discovery is non-vacuous and neither side has a missing property, an
    // extra property, or any shape drift
    expect(result.counts.typeScriptDefinitions).toBe(23);
    expect(result.emptyDiscovery).toEqual([]);
    expect(manageSequence?.missingNativeProperties ?? []).toEqual([]);
    expect(manageSequence?.extraNativeProperties ?? []).toEqual([]);
    expect(manageSequence?.schemaMismatches ?? []).toEqual([]);
  });

  it('does not let an unused same-prefix sibling satisfy a property', async () => {
    // Given
    const root = createSchemaParityFixture("frameRate: { type: 'number' }", '');
    writeFixtureFile(
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
    const root = createSchemaParityFixture(
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
    const root = createSchemaParityFixture(
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
    const root = createSchemaParityFixture("frameRate: { type: 'number' }", [
      'McpAlphaFields::AddFields(Schema);'
    ].join('\n'));
    const toolsRoot = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools';
    const toolPath = `${toolsRoot}/Utility/McpTool_Alpha.cpp`;
    const toolSource = fs.readFileSync(path.join(root, toolPath), 'utf8');
    writeFixtureFile(
      root,
      toolPath,
      `#include "MCP/Tools/Utility/McpAlphaFields.h"\n${toolSource}`
    );
    writeFixtureFile(
      root,
      `${toolsRoot}/Utility/McpAlphaFields.h`,
      'namespace McpAlphaFields { void AddFields(FMcpSchemaBuilder& Schema); }\n'
    );
    writeFixtureFile(
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
    const root = createSchemaParityFixture(
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

  it('extracts a top-level AnyValue builder method as an unconstrained property', async () => {
    // Given
    const root = createSchemaParityFixture(
      'value: {}',
      'Schema.AnyValue(TEXT("value"), TEXT("Any"));'
    );
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    // When
    const result = auditNativeMcpParity({ repoRoot: root, schemaParityTools: ['alpha'] });

    // Then
    expect(result.schemaPropertyGaps).toEqual([]);
  });

  it('extracts a top-level TypeUnion builder method as a sorted type-array schema', async () => {
    // Given
    const root = createSchemaParityFixture(
      "frameRate: { type: ['number', 'string'] }",
      'Schema.TypeUnion(TEXT("frameRate"), { TEXT("number"), TEXT("string") }, TEXT("Rate"));'
    );
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    // When
    const result = auditNativeMcpParity({ repoRoot: root, schemaParityTools: ['alpha'] });

    // Then
    expect(result.schemaPropertyGaps).toEqual([]);
  });

  it('normalizes TypeUnion type-list order regardless of source order', async () => {
    // Given: native lists string before number, TS lists number before string
    const root = createSchemaParityFixture(
      "frameRate: { type: ['number', 'string'] }",
      'Schema.TypeUnion(TEXT("frameRate"), { TEXT("string"), TEXT("number") }, TEXT("Rate"));'
    );
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    // When
    const result = auditNativeMcpParity({ repoRoot: root, schemaParityTools: ['alpha'] });

    // Then
    expect(result.schemaPropertyGaps).toEqual([]);
  });

  it('extracts a TypeUnion inside a nested builder callback', async () => {
    // Given: nested object callback declares a TypeUnion on the inner builder
    const root = createSchemaParityFixture(
      [
        "settings: { type: 'object', properties: { rate: { type: ['number'] } } }"
      ].join(' '),
      [
        'Schema.Object(TEXT("settings"), TEXT("Settings"), [](FMcpSchemaBuilder& S) {',
        '  S.TypeUnion(TEXT("rate"), { TEXT("number") }, TEXT("Rate"));',
        '});'
      ].join('\n')
    );
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    // When
    const result = auditNativeMcpParity({ repoRoot: root, schemaParityTools: ['alpha'] });

    // Then
    expect(result.schemaPropertyGaps).toEqual([]);
  });

  it('extracts an AnyValue inside a nested builder callback', async () => {
    // Given: nested object callback declares an unconstrained AnyValue on the inner builder
    const root = createSchemaParityFixture(
      [
        "settings: { type: 'object', properties: { payload: {} } }"
      ].join(' '),
      [
        'Schema.Object(TEXT("settings"), TEXT("Settings"), [](FMcpSchemaBuilder& S) {',
        '  S.AnyValue(TEXT("payload"), TEXT("Payload"));',
        '});'
      ].join('\n')
    );
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    // When
    const result = auditNativeMcpParity({ repoRoot: root, schemaParityTools: ['alpha'] });

    // Then
    expect(result.schemaPropertyGaps).toEqual([]);
  });

  it('handles a TypeUnion with no type entries as an empty type array', async () => {
    // Given: malformed/empty type list on the native side
    const root = createSchemaParityFixture(
      'frameRate: { type: [] }',
      'Schema.TypeUnion(TEXT("frameRate"), {}, TEXT("Rate"));'
    );
    const { auditNativeMcpParity } = await import('../native-mcp-parity-audit.mjs');

    // When
    const result = auditNativeMcpParity({ repoRoot: root, schemaParityTools: ['alpha'] });

    // Then
    expect(result.schemaPropertyGaps).toEqual([]);
  });
});
