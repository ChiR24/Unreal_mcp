import { readdirSync, readFileSync, statSync } from 'node:fs';
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

  it('routes struct value validation through the shared reflection importer on scratch memory', () => {
    // Given / When: set must import via McpPropertyReflection::ApplyJsonValueToProperty
    // on a scratch instance (call syntax, not a comment or dead reference), never via a
    // direct FJsonObjectConverter call. The shared importer must own the FStructProperty
    // JSON-object branch. Scratch-instance atomicity (validate before commit, then move)
    // must be preserved so malformed structValues never mutates the live FInstancedStruct.
    // Then
    const cpp = read(instancedStructCpp);
    // Handler must invoke the shared importer with call syntax.
    expect(cpp).toMatch(/McpPropertyReflection::ApplyJsonValueToProperty\s*\(/);
    // Handler must not deserialize struct values directly with the engine converter.
    expect(cpp).not.toContain('FJsonObjectConverter::JsonObjectToUStruct');
    // The now-unused JsonObjectConverter include must be gone from the handler.
    expect(cpp).not.toMatch(/#\s*include\s*"[^"]*JsonObjectConverter\.h"/);
    // Scratch-instance atomicity: validate before commit, then move.
    expect(cpp).toContain('Scratch');
    expect(cpp).toContain('MoveTemp');

    // The shared importer must own the FStructProperty branch with explicit guards.
    const importCpp = read(resolve(
      process.cwd(),
      'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Foundation/Reflection/McpPropertyReflectionImport.cpp',
    ));
    expect(importCpp).toContain('FStructProperty');
    expect(importCpp).toContain('FJsonObjectConverter::JsonObjectToUStruct');
    expect(importCpp).toContain('EJson::Object');
    expect(importCpp).toContain('UScriptStruct');
    expect(importCpp).toContain('Failed to convert JSON object to struct');
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
