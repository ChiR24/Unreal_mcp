import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginStructsDir = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/Structs',
);
const manageAssetToolPath = resolve(
  process.cwd(),
  'src/tools/definitions/core/manage-asset-tool.ts',
);

const read = (file: string): string => readFileSync(file, 'utf8');

const membersAddRemove = read(
  `${pluginStructsDir}/McpAutomationBridge_AssetWorkflowStructsMembersAddRemove.cpp`,
);
const assetOps = read(
  `${pluginStructsDir}/McpAutomationBridge_AssetWorkflowStructsAssetOps.cpp`,
);
const analysis = read(
  `${pluginStructsDir}/McpAutomationBridge_AssetWorkflowStructsAnalysis.cpp`,
);
const membersEdit = read(
  `${pluginStructsDir}/McpAutomationBridge_AssetWorkflowStructsMembersEdit.cpp`,
);
const manageAssetTool = read(manageAssetToolPath);

describe('Blueprint Struct authoring contracts (issue #510)', () => {
  it('rejects self-referencing and unresolved struct members on add', () => {
    // Given / When: add_struct_member and import_struct must guard before
    // FStructureEditorUtils::AddVariable so a struct can never contain itself.
    // Then
    expect(membersAddRemove).toContain('SELF_REFERENCE');
    expect(membersAddRemove).toContain('Sub == static_cast<UObject*>(S)');
    expect(membersAddRemove).toContain('ASSET_NOT_FOUND');
  });

  it('applies member default value when provided on add_struct_member', () => {
    // Given / When: the handler reads defaultValue and pushes it through the
    // editor util instead of silently ignoring it.
    // Then
    expect(membersAddRemove).toContain('ChangeVariableDefaultValue');
  });

  it('leaves a redirector instead of deleting the old asset on rename_struct', () => {
    // Given / When: rename_struct must preserve soft references by dropping a
    // UObjectRedirector at the former path rather than deleting the package.
    // Then
    expect(assetOps).toContain('NewObject<UObjectRedirector>');
    expect(assetOps).toContain('DestinationObject');
  });

  it('honors searchScope on search_struct_usage and gates save on recompile_struct', () => {
    // Given / When: search_struct_usage filters by the optional scope and
    // recompile_struct only persists when save/bSave is set.
    // Then
    expect(analysis).toContain('searchScope');
    expect(analysis).toContain('bSave');
    expect(analysis).toContain('McpSafeAssetSave');
  });

  it('treats reorder onto the same member as a no-op success', () => {
    // Given / When: reorder_struct_members with TargetGuid == G must short
    // circuit instead of mutating order for a single member.
    // Then
    expect(membersEdit).toContain('TargetGuid == G');
  });

  it('declares members.items with a string defaultValue for import_struct', () => {
    // Given / When: the TS contract must advertise the nested member shape so
    // clients know the defaultValue grammar.
    // Then
    expect(manageAssetTool).toContain('members:');
    expect(manageAssetTool).toContain(
      "defaultValue: { type: 'string'",
    );
  });
});
