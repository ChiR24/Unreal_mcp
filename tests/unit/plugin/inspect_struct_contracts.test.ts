import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginInspectDir = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Inspect',
);

const read = (file: string): string => readFileSync(file, 'utf8');

const inspectStruct = read(
  `${pluginInspectDir}/McpAutomationBridge_InspectStruct.cpp`,
);

describe('inspect_struct read-only reflection contracts (struct ecosystem)', () => {
  it('enumerates members via TFieldIterator<FProperty>', () => {
    // Given / When: the handler must iterate UStruct fields with TFieldIterator.
    // Then
    expect(inspectStruct).toContain('TFieldIterator<FProperty>');
  });

  it('reads property metadata via GetMetaData', () => {
    // Given / When: each member reports Tooltip and Category metadata.
    // Then
    expect(inspectStruct).toContain('GetMetaData');
  });

  it('resolves the target as a UScriptStruct', () => {
    // Given / When: resolution must produce a UScriptStruct (FindObject/LoadObject).
    // Then
    expect(inspectStruct).toContain('UScriptStruct');
  });

  it('detects nested struct members via CastField<FStructProperty>', () => {
    // Given / When: a member that is itself a struct must be flagged with its inner name.
    // Then
    expect(inspectStruct).toContain('CastField<FStructProperty>');
  });

  it('guards the required structPath parameter and reports ASSET_NOT_FOUND', () => {
    // Given / When: missing path and unresolved path must fail with the same
    // codes used by the UDS authoring handlers.
    // Then
    expect(inspectStruct).toContain('MISSING_PARAMETER');
    expect(inspectStruct).toContain('ASSET_NOT_FOUND');
  });

  it('exposes the inspect_struct action name and HandleInspectStructAction entry point', () => {
    // Given / When: the shard must register the exact action-name string
    // literal and a single public entry point with the expected signature.
    // Then
    expect(inspectStruct).toContain('inspect_struct');
    expect(inspectStruct).toContain('HandleInspectStructAction');
  });

  it('never uses the forbidden UPackage::SavePackage', () => {
    // Given / When: read-only reflection must not persist anything.
    // Then
    expect(inspectStruct).not.toContain('UPackage::SavePackage');
  });
});
