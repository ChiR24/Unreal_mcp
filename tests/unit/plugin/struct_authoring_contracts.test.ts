import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginStructsDir = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/Structs',
);
const manageAssetParentPath = resolve(
  process.cwd(),
  'src/tools/catalog/capabilities/generated/parent-tool-definitions.generated.ts',
);

const read = (file: string): string => readFileSync(file, 'utf8');

const membersAddRemove = read(
  `${pluginStructsDir}/McpAutomationBridge_AssetWorkflowStructsMembersAddRemove.cpp`,
);
const assetOpsRename = read(
  `${pluginStructsDir}/McpAutomationBridge_AssetWorkflowStructsAssetOpsRename.cpp`,
);
const analysisUsage = read(
  `${pluginStructsDir}/McpAutomationBridge_AssetWorkflowStructsAnalysisUsage.cpp`,
);
const membersEdit = read(
  `${pluginStructsDir}/McpAutomationBridge_AssetWorkflowStructsMembersEdit.cpp`,
);
const helpers = read(
  `${pluginStructsDir}/McpAutomationBridge_AssetWorkflowStructsHelpers.cpp`,
);
const refresh = read(
  `${pluginStructsDir}/McpAutomationBridge_AssetWorkflowStructsRefresh.cpp`,
);
const sharedHeader = read(
  `${pluginStructsDir}/McpAutomationBridge_AssetWorkflowStructsShared.h`,
);
const addVariableToolPath = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Blueprint/Variables/McpAutomationBridge_BlueprintHandlersAddVariable.cpp',
);
const inspectStructPath = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/Inspect/McpAutomationBridge_InspectStruct.cpp',
);
const typeResolverPath = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Foundation/Blueprint/McpBlueprintUtilsTypeResolver.cpp',
);
const typeHelpersPath = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsTypeHelpers.cpp',
);
const addVariable = read(addVariableToolPath);
const inspectStruct = read(inspectStructPath);
const typeResolver = read(typeResolverPath);
const typeHelpers = read(typeHelpersPath);
const manageAssetParent = read(manageAssetParentPath);

describe('Blueprint Struct authoring contracts (issue #511)', () => {
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
    // UObjectRedirector at the former path rather than deleting the package,
    // using the supported AssetTools::RenameAssets path. This logic lives in
    // AssetOpsRename.cpp after the issue #511 split of AssetOps.cpp.
    // Then
    expect(assetOpsRename).toContain('UObjectRedirector');
    expect(assetOpsRename).toContain('AssetTools::RenameAssets');
  });

  it('honors searchScope on search_struct_usage and gates save on recompile_struct', () => {
    // Given / When: search_struct_usage filters by the optional scope and
    // recompile_struct only persists when save/bSave is set. This logic lives
    // in AnalysisUsage.cpp after the issue #511 split of Analysis.cpp.
    // Then
    expect(analysisUsage).toContain('searchScope');
    expect(analysisUsage).toContain('bSave');
    expect(analysisUsage).toContain('McpSafeAssetSave');
  });

  it('reports TYPE_RESOLUTION_FAILED (not CLASS_NOT_FOUND) when add_variable type resolution fails', () => {
    // Given / When: add_variable validates variableType through the shared
    // McpBlueprintUtils::ResolvePinType resolver; an unresolvable struct type
    // spec must fail with TYPE_RESOLUTION_FAILED rather than CLASS_NOT_FOUND.
    // Then
    expect(addVariable).toContain('ResolvePinType');
    expect(addVariable).toContain('TYPE_RESOLUTION_FAILED');
  });

  it('treats reorder onto the same member as a no-op success', () => {
    // Given / When: reorder_struct_members with TargetGuid == G must short
    // circuit instead of mutating order for a single member.
    // Then
    expect(membersEdit).toContain('TargetGuid == G');
  });

  it('declares members.items as an object array for import_struct (record-derived contract)', () => {
    // Given / When: the generated manage_asset parent definition must advertise
    // the nested member shape so clients know the import_struct grammar.
    // Then
    expect(manageAssetParent).toContain('"name": "manage_asset"');
    expect(manageAssetParent).toContain('"members":');
    expect(manageAssetParent).toContain('"type": "array"');
  });

  it('exposes ForEachReferencingAsset that visits all referencer types (not just Blueprints)', () => {
    // Given / When: ForEachReferencingAsset is the generalized referencer
    // enumerator; ForEachReferencingBlueprint is now a thin wrapper that
    // filters ForEachReferencingAsset results to UBlueprint only.
    // Then
    expect(sharedHeader).toContain('ForEachReferencingAsset');
    expect(helpers).toContain('ForEachReferencingAsset');
    expect(helpers).toContain('ForEachReferencingBlueprint');
  });

  it('search_struct_usage reports referencerType for Struct and DataAsset referencers', () => {
    // Given / When: search_struct_usage must classify each referencer as
    // Blueprint, Struct, DataAsset, or Other — not only Blueprints.
    // Then
    expect(analysisUsage).toContain('referencerType');
    expect(analysisUsage).toContain('"Struct"');
    expect(analysisUsage).toContain('"DataAsset"');
  });

  it('McpRefreshStructDependents recompiles nested structs before Blueprints and reports DataAssets', () => {
    // Given / When: refresh must recompile UUserDefinedStruct referencers
    // before Blueprint referencers (ordering invariant) and report both
    // nested structs and DataAsset referencers via new out-params.
    // Then
    expect(sharedHeader).toContain('OutStructs');
    expect(sharedHeader).toContain('OutDataAssets');
    expect(refresh).toContain('UUserDefinedStruct');
    expect(refresh).toContain('UDataAsset');
    expect(refresh).toContain('CompileStructure(Nested)');
  });

  it('recompile_struct recompiles nested struct referencers before Blueprints', () => {
    // Given / When: the explicit recompile_struct handler must also process
    // nested UUserDefinedStruct referencers via ForEachReferencingAsset
    // before recompiling Blueprints.
    // Then
    expect(analysisUsage).toContain('ForEachReferencingAsset');
    expect(analysisUsage).toContain('UUserDefinedStruct');
    expect(analysisUsage).toContain('CompileStructure(Nested)');
  });

  it('calls McpRefreshStructDependents after reorder, set_default, and set_metadata mutations', () => {
    // Given / When: every struct mutation must trigger the refresh cascade.
    // reorder, set_default, and set_metadata are the three edit-path mutations
    // that previously missed the refresh call.
    // Then
    const matches = membersEdit.match(/McpRefreshStructDependents/g);
    expect(matches).not.toBeNull();
    expect(matches ? matches.length : 0).toBeGreaterThanOrEqual(3);
  });

  it('inspect_struct returns guid and metadata per member for UUserDefinedStruct', () => {
    // Given / When: inspect_struct must enrich each member with the stable
    // GUID and metadata map from FStructVariableDescription (reflection-only
    // FProperty lacks both).
    // Then
    expect(inspectStruct).toContain('VarGuid.ToString()');
    expect(inspectStruct).toContain('UDSVarMap');
    expect(inspectStruct).toContain('"guid"');
    expect(inspectStruct).toContain('"metadata"');
  });

  it('ResolvePinType accepts Enum as a map key type', () => {
    // Given / When: the type resolver documents that Enum is a valid map key
    // alongside Byte/Int/Int64/Name/String. The bValidKey check must include
    // PC_Enum so that Map:Enum:/...,String is accepted at runtime.
    // Then
    expect(typeResolver).toContain('PC_Enum');
  });

  it('PinTypeToSummary preserves SoftObject and SoftClass class paths for round-trip', () => {
    // Given / When: export_struct must emit the referenced class path for
    // SoftObject and SoftClass members so import_struct can reconstruct them.
    // Previously these fell into the default branch, losing the class path.
    // Then
    expect(typeHelpers).toContain('PC_SoftObject');
    expect(typeHelpers).toContain('PC_SoftClass');
    expect(typeHelpers).toContain('SoftObject');
    expect(typeHelpers).toContain('SoftClass');
  });
});
