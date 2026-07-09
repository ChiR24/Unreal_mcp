import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginEnumsDir = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/Enums',
);

const read = (file: string): string => readFileSync(file, 'utf8');

const lifecycle = read(
  `${pluginEnumsDir}/McpAutomationBridge_AssetWorkflowEnumLifecycle.cpp`,
);
const values = read(
  `${pluginEnumsDir}/McpAutomationBridge_AssetWorkflowEnumValues.cpp`,
);

describe('UserDefinedEnum authoring contracts (struct ecosystem)', () => {
  it('creates enums through the UserDefinedEnum factory', () => {
    // Given / When: create_enum must allocate a UUserDefinedEnum via the engine
    // factory rather than hand-rolling the object.
    // Then
    expect(lifecycle).toContain('CreateUserDefinedEnum');
    expect(lifecycle).toContain('RF_Standalone');
  });

  it('commits enum mutations via SetEnums / PostEditChange', () => {
    // Given / When: value and lifecycle mutations must go through the editor
    // util (SetEnums) and refresh editor state (PostEditChange) so the enum
    // reinstances correctly.
    // Then
    expect(lifecycle).toContain('SetEnums');
    expect(lifecycle).toContain('PostEditChange');
    expect(values).toContain('SetEnums');
  });

  it('persists enums through McpSafeAssetSave only', () => {
    // Given / When: save is gated on bSave and routed through the safe wrapper.
    // The direct UPackage::SavePackage path must never appear.
    // Then
    expect(lifecycle).toContain('McpSafeAssetSave');
    expect(values).toContain('McpSafeAssetSave');
    expect(lifecycle).not.toContain('UPackage::SavePackage');
    expect(values).not.toContain('UPackage::SavePackage');
  });

  it('dispatching enum actions through a single HandleEnumAction entry', () => {
    // Given / When: all enum actions route through one dispatcher that fans out
    // to lifecycle and value shards, mirroring HandleStructAction.
    // Then
    expect(lifecycle).toContain('HandleEnumAction');
    expect(lifecycle).toContain('HandleEnumLifecycleActions');
    expect(values).toContain('HandleEnumValueActions');
  });

  it('guards reorder_enum_values with a no-op self-guard', () => {
    // Given / When: reorder_enum_values must short-circuit (report success
    // without mutating) when the requested order is invalid.
    // Then
    expect(values).toContain('reorder');
    expect(values).toContain('no-op');
  });
});
