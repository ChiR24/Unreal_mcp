import { readFileSync, readdirSync, statSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const pluginSourceDir = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source',
);
const instancedStructCpp = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/StructProperty/McpAutomationBridge_StructPropertyInstanced.cpp',
);

const read = (file: string): string => readFileSync(file, 'utf8');

// Recursively collect every C++ source/header under the plugin source tree.
function collectSourceFiles(dir: string, acc: string[] = []): string[] {
  for (const entry of readdirSync(dir)) {
    const full = resolve(dir, entry);
    const info = statSync(full);
    if (info.isDirectory()) {
      collectSourceFiles(full, acc);
    } else if (/\.(cpp|h|hpp)$/i.test(entry)) {
      acc.push(full);
    }
  }
  return acc;
}

describe('FInstancedStruct property access contracts (struct ecosystem)', () => {
  it('implements get/set_instanced_struct_property in a dedicated handler', () => {
    // Given / When: the new handler must drive FInstancedStruct access through
    // the generic reflection layer and the safe save wrapper.
    // Then
    const cpp = read(instancedStructCpp);
    expect(cpp).toContain('get_instanced_struct_property');
    expect(cpp).toContain('set_instanced_struct_property');
    expect(cpp).toContain('FInstancedStruct');
    expect(cpp).toContain('GetScriptStruct');
    expect(cpp).toContain('InitializeAs');
    expect(cpp).toContain('McpSafeAssetSave');
  });

  it('reuses the generic reflection layer instead of reimplementing struct (de)serialization', () => {
    // Given / When: get must serialize the inner struct via
    // ExportPropertyToJsonValue; set must import via the reflection layer.
    // Then
    const cpp = read(instancedStructCpp);
    expect(cpp).toContain('ExportPropertyToJsonValue');
    expect(cpp).toContain('ApplyJsonValueToProperty');
  });

  it('reports the correct error codes and never uses UPackage::SavePackage', () => {
    // Given / When: missing params, missing asset, and wrong property type must
    // use the canonical error codes; saves go through McpSafeAssetSave only.
    // Then
    const cpp = read(instancedStructCpp);
    expect(cpp).toContain('MISSING_PARAMETER');
    expect(cpp).toContain('ASSET_NOT_FOUND');
    expect(cpp).toContain('INVALID_OPERATION');
    expect(cpp).toContain('OPERATION_FAILED');
    expect(cpp).not.toContain('UPackage::SavePackage');
  });

  it('ensures no plugin source file calls the forbidden UPackage::SavePackage', () => {
    // Given / When: the whole plugin must route saves through the safe wrapper.
    // Then
    const files = collectSourceFiles(pluginSourceDir);
    const offenders = files.filter((f) => read(f).includes('UPackage::SavePackage'));
    expect(offenders).toEqual([]);
  });
});
