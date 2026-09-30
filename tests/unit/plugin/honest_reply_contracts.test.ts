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
    expect(source).toContain('*McpBlueprintVariableList(ActorClass)');
  });

  it('get_blueprint with a property that misses names the variables the Blueprint has', () => {
    expect(code('Blueprint', 'Queries', 'McpAutomationBridge_BlueprintHandlersGet.cpp'))
      .toMatch(/Its variables: %s\."\), \*PropertyName, \*McpBlueprintVariableList\(Generated\)\),\s*Resp, TEXT\("PROPERTY_NOT_FOUND"\)\);/u);
  });

  // {"RotOscillation": {"Pitch": {"Amplitude": 0.8}}} on a LegacyCameraShake became empty text and
  // vanished; create still answered "Blueprint created".
  it('create sets properties through the shared writer and names each one it did not set', () => {
    const writer = code('BlueprintCreation', 'McpAutomationBridge_BlueprintCreationHandlersProperties.cpp');
    expect(writer).toContain('ApplyJsonValueToProperty(TargetObject, Property, Pair.Value, Error)');
    expect(writer).not.toContain('McpJsonScalarToString');
    expect(writer).toMatch(/if \(!Property\) \{\s*OutFailed\.Add\(/u);
    const reply = code('BlueprintCreation', 'McpAutomationBridge_BlueprintCreationHandlersAssets.cpp');
    expect(reply).toContain('ResultPayload->SetArrayField(TEXT("failedProperties")');
    expect(reply).toContain('of its properties were not set: %s');
  });

  it('a loot entry added after a removal takes a new key', () => {
    expect(code('Inventory', 'McpAutomationBridge_InventoryHandlersLootTables.cpp')).toContain('NextIndexedPropertyIndex(');
  });

  // get_property "FadeAmount" on a PlayerCameraManager said only "not found in scope".
  it('a property path miss names the similar properties of that scope', () => {
    const resolver = code('..', 'Foundation', 'BridgeHelpers', 'Properties', 'McpAutomationBridgeHelpersNestedPropertyPath.h');
    expect(resolver).toContain('static inline FString McpSimilarPropertyNames(const UStruct *Scope, const FString &Wanted)');
    expect(resolver).toMatch(/McpSimilarPropertyNames\(CurrentTypeScope, Segment\);\s*OutError = FString::Printf\(/u);
    expect(resolver).toContain('TEXT("; similar: ")');
  });

  // A scale key without an {x,y} pair created the binding and an empty track, then answered
  // INVALID_ARGUMENT; the later valid key reported createdTrack false.
  it('a refused widget animation key or track leaves nothing behind', () => {
    const keys = code('WidgetAuthoring', 'Support', 'McpAutomationBridge_WidgetAuthoringAnimationKeys.cpp');
    const author = keys.slice(keys.indexOf('bool McpAuthorWidgetAnimationKey('));
    expect(author.indexOf('OutError = KeyValueError(Kind, TrackType, ValueField);')).toBeGreaterThan(-1);
    expect(author.indexOf('OutError = KeyValueError(Kind, TrackType, ValueField);')).toBeLessThan(author.indexOf('FindOrCreateBinding('));
    const track = keys.slice(keys.indexOf('bool McpAddWidgetAnimationTrack('));
    expect(track.indexOf('!IsTransformKind(Kind)')).toBeLessThan(track.indexOf('FindOrCreateBinding('));
  });

  it('widget animation track and key replies name the Widget Blueprint for the receipt', () => {
    expect(code('WidgetAuthoring', 'Animation', 'McpAutomationBridge_WidgetAuthoringAnimationKeyframe.cpp'))
      .toContain('ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBP));');
    const core = code('WidgetAuthoring', 'Animation', 'McpAutomationBridge_WidgetAuthoringAnimationCore.cpp');
    expect(core).toContain('ResultJson->SetBoolField(TEXT("createdTrack"), Track.bCreatedTrack);');
    expect(core).not.toContain('TEXT("trackCreated")');
  });

  // widget_list named WBP_HUD_C_0, then get_property answered OBJECT_NOT_FOUND for it.
  it('a live widget resolves by the name widget_list reports', () => {
    const resolution = code('..', 'Foundation', 'HandlerUtils', 'McpHandlerUtilsObjectResolution.cpp');
    const role = resolution.slice(resolution.indexOf('UObject* ResolveRuntimeRole('));
    expect(role).toMatch(/for \(TObjectIterator<UUserWidget> It; It; \+\+It\)/u);
    expect(role).toContain('It->GetWorld() == World');
  });

  // set_pp_color_grading with {"saturation": 1.1} answered "applied" with every key under unsupportedSettings.
  it('a settings object where no key is a field fails and names the close fields', () => {
    const settings = code('Render', 'McpAutomationBridge_RenderSupportSettings.h');
    expect(settings).toMatch(/if \(Unknown\.Num\(\) > 0 && OutApplied\.Num\(\) == AppliedBefore\)[\s\S]*?return false;/u);
    expect(settings).toContain('It->GetName().Contains(Key)');
  });

  // A custom node got its input before the wire feeding it; that mid-batch compile's
  // "missing input" stayed on a receipt whose final compile was clean.
  it('build_material_graph drops compile lines its final compile supersedes', () => {
    const batch = code('MaterialAuthoring', 'McpAutomationBridge_MaterialAuthoringGraphBatch.cpp');
    expect(batch.indexOf('Bridge->ForgetCapturedMessages(')).toBeGreaterThan(-1);
    expect(batch.indexOf('Bridge->ForgetCapturedMessages(')).toBeLessThan(batch.indexOf('RunStep(CompileId, Compile);'));
  });

  // update_custom_expression and connect_nodes answered with changes: [] for the material they edited.
  it('a material graph edit names the material it changed', () => {
    for (const file of [
      ['Nodes', 'McpAutomationBridge_MaterialAuthoringHandlersUpdateCustomExpression.cpp'],
      ['Nodes', 'McpAutomationBridge_MaterialAuthoringHandlersDeleteNode.cpp'],
      ['Parameters', 'McpAutomationBridge_MaterialAuthoringHandlersAddParameter.cpp'],
    ] as const) {
      expect(code('MaterialAuthoring', ...file)).toContain('McpMaterialHostResult(HostOuter)');
    }
    expect(code('MaterialAuthoring', 'Connections', 'McpAutomationBridge_MaterialAuthoringHandlersConnectNodes.cpp').split('McpMaterialHostResult(HostOuter)').length).toBe(5);
  });

  // disconnect_nodes answered "Disconnect operation completed." with nothing unplugged.
  it('disconnect_nodes fails when no pin matched and unplugs a custom node input by label', () => {
    const source = code('MaterialAuthoring', 'Connections', 'McpAutomationBridge_MaterialAuthoringHandlersDisconnectNodes.cpp');
    expect(source).not.toContain('Disconnect operation completed');
    expect(source).toContain('Target = TargetExpr->GetInput(InputIndex);');
  });
});
