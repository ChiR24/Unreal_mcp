// Replies that claimed more than the handler did: a success that wrote nothing, a "saved" that
// echoed the request, a debug field, a modal dialog nobody can answer. Wiring contracts only;
// behaviour needs an editor (tests/mcp-tools/**).

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const DOMAINS = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(DOMAINS, ...segments), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('handlers answer what they did', () => {
  it('configure_item_stacking fails when no stacking property took the value', () => {
    const source = code('Inventory', 'McpAutomationBridge_InventoryHandlersItemPresentation.cpp');
    expect(source).toMatch(/if \(ModifiedProps\.Num\(\) == 0\) \{\s*Bridge\.SendAutomationError\([\s\S]*?TEXT\("PROPERTY_NOT_FOUND"\)\);\s*return true;/u);
    expect(source).not.toContain('No stacking properties found');
  });

  it('create_enum and create_struct report the save result, not the request', () => {
    expect(code('AssetWorkflow', 'Enums', 'LifecycleEnums.cpp')).toContain('const bool bSaved = FinalizeEnum(Enum, bSave);');
    expect(code('AssetWorkflow', 'Enums', 'Shared.h')).toContain('return bSave && McpSafeAssetSave(Enum);');
    expect(code('AssetWorkflow', 'Structs', 'McpAutomationBridge_AssetWorkflowStructsLifecycle.cpp'))
      .toContain('const bool bSaved = bSave && McpSafeAssetSave(S);');
  });

  it('get_metadata sends no debug field', () => {
    expect(code('AssetWorkflow', 'Operations', 'McpAutomationBridge_AssetWorkflowMetadata.cpp')).not.toContain('debug_has_meta');
  });

  it('bulk_delete never asks the engine for a confirmation dialog', () => {
    const source = code('AssetWorkflow', 'Operations', 'McpAutomationBridge_AssetWorkflowBulkDelete.cpp');
    expect(source).not.toContain('showConfirmation');
    expect(source).not.toContain('bShowConfirmation)');
  });

  it('a resized or combined texture keeps its source colour space', () => {
    expect(code('Texture', 'McpAutomationBridge_TextureHandlersResize.cpp')).toContain('NewTexture->SRGB = SourceTexture->SRGB;');
    expect(code('Texture', 'McpAutomationBridge_TextureHandlersCombine.cpp')).toContain('OutputTexture->SRGB = BaseTex->SRGB;');
  });

  // UEditorAssetLibrary's reads answer false or null for every path while Play In Editor runs, so
  // play_sound, a Blueprint class path, exists and the asset graph called a real asset missing.
  it('no handler reads an asset through UEditorAssetLibrary, which refuses every call during Play', () => {
    const walk = (dir: string): string[] => readdirSync(dir, { withFileTypes: true }).flatMap((entry) =>
      entry.isDirectory() ? walk(join(dir, entry.name)) : /\.(?:cpp|h)$/u.test(entry.name) ? [join(dir, entry.name)] : []);
    const offenders = walk(join(DOMAINS, '..'))
      .filter((file) => /UEditorAssetLibrary::(?:DoesAssetExist|LoadAsset|FindAssetData|LoadBlueprintClass)\(/u
        .test(readFileSync(file, 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ')));
    expect(offenders).toEqual([]);
    expect(code('AssetWorkflow', 'Analysis', 'McpAutomationBridge_AssetWorkflowMaterialGraph.cpp'))
      .toContain('McpAssetExists(SafeAssetPath, &AssetData)');
  });

  // A GameMode whose Audio component plays MS_Music gave 0 matches for "MS_Music": object
  // references were skipped outright.
  it('find_text matches a reference to another asset by its path', () => {
    const source = code('AssetQuery', 'McpAutomationBridge_AssetQueryFindText.cpp');
    expect(source).toMatch(/else if \(const FObjectPropertyBase\* Ref = CastField<FObjectPropertyBase>\(Property\)\)/u);
    expect(source).toContain('Soft->GetPropertyValue(Data).ToString()');
    expect(source).toContain('!Path.StartsWith(TEXT("/Script/"))');
    expect(source).toContain('FPackageName::ObjectPathToPackageName(Path) != FPackageName::ObjectPathToPackageName(Asset)');
  });

  // {Direction: 0} on an enemy whose variable is Dir answered success "Variables updated", set
  // nothing, and dirtied the level.
  it('set_blueprint_variables fails when no name is a variable, and names the real ones', () => {
    const source = code('ControlActor', 'McpAutomationBridge_ControlActorAdvanced.cpp');
    expect(source).toMatch(/if \(Missing\.Num\(\) > 0 && Missing\.Num\(\) == \(\*VariablesPtr\)->Values\.Num\(\)\) \{/u);
    expect(source).toContain('TEXT("PROPERTY_NOT_FOUND")');
    expect(source.indexOf('TEXT("PROPERTY_NOT_FOUND")')).toBeLessThan(source.indexOf('Found->Modify();'));
  });

  it('a loot entry added after a removal takes a new key', () => {
    expect(code('Inventory', 'McpAutomationBridge_InventoryHandlersLootTables.cpp')).toContain('NextIndexedPropertyIndex(');
  });
});
