import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginEnumsDir = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/Enums',
);

const read = (file: string): string => readFileSync(file, 'utf8');

const shared = read(`${pluginEnumsDir}/Shared.h`);
const lifecycle = read(`${pluginEnumsDir}/Lifecycle.cpp`);
const values = read(`${pluginEnumsDir}/Values.cpp`);

// The exact action-name string literals the TypeScript layer sends as `action`.
const ACTION_LITERALS = [
  'create_enum',
  'delete_enum',
  'get_enum',
  'add_enum_value',
  'remove_enum_value',
  'rename_enum_value',
  'reorder_enum_values',
  'set_enum_value_metadata',
  'split_enum',
] as const;

describe('UserDefinedEnum authoring contracts (struct ecosystem)', () => {
  it('exposes the single HandleEnumAction entry point', () => {
    expect(shared).toContain('HandleEnumAction');
    expect(lifecycle).toContain('HandleEnumAction');
    expect(lifecycle).toContain('HandleEnumLifecycleActions');
    expect(values).toContain('HandleEnumValueActions');
  });

  it('declares every enum action-name string literal', () => {
    for (const literal of ACTION_LITERALS) {
      const presentInLifecycle = lifecycle.includes(literal);
      const presentInValues = values.includes(literal);
      expect(presentInLifecycle || presentInValues).toBe(true);
    }
  });

  it('routes each action to the correct shard', () => {
    // Lifecycle shard owns create/delete/get.
    expect(lifecycle).toContain('"create_enum"');
    expect(lifecycle).toContain('"delete_enum"');
    expect(lifecycle).toContain('"get_enum"');
    // Value shard owns the remaining six.
    expect(values).toContain('"add_enum_value"');
    expect(values).toContain('"remove_enum_value"');
    expect(values).toContain('"rename_enum_value"');
    expect(values).toContain('"reorder_enum_values"');
    expect(values).toContain('"set_enum_value_metadata"');
    expect(values).toContain('"split_enum"');
  });

  it('persists enums only through McpSafeAssetSave', () => {
    expect(lifecycle).toContain('McpSafeAssetSave');
    expect(shared).toContain('McpSafeAssetSave');
    expect(values).not.toContain('UPackage::SavePackage');
    expect(lifecycle).not.toContain('UPackage::SavePackage');
    expect(shared).not.toContain('UPackage::SavePackage');
  });
});
