// Replies that claimed more than the handler did: a success that wrote nothing, a "saved" that
// echoed the request, a debug field, a modal dialog nobody can answer. Wiring contracts only;
// behaviour needs an editor (tests/mcp-tools/**).

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

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
  // One pass over every plugin source file: 0.1 s alone, but over the 10 s default on a loaded
  // machine (the suite's other whole-tree tests carry 60 s for the same reason).
  it('no handler reads an asset through UEditorAssetLibrary, which refuses every call during Play', () => {
    const walk = (dir: string): string[] => readdirSync(dir, { withFileTypes: true }).flatMap((entry) =>
      entry.isDirectory() ? walk(join(dir, entry.name)) : /\.(?:cpp|h)$/u.test(entry.name) ? [join(dir, entry.name)] : []);
    const offenders = walk(join(DOMAINS, '..'))
      .filter((file) => /UEditorAssetLibrary::(?:DoesAssetExist|LoadAsset|FindAssetData|LoadBlueprintClass)\(/u
        .test(readFileSync(file, 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ')));
    expect(offenders).toEqual([]);
    expect(code('AssetWorkflow', 'Analysis', 'McpAutomationBridge_AssetWorkflowMaterialGraph.cpp'))
      .toContain('McpAssetExists(SafeAssetPath, &AssetData)');
  }, 60_000);

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
    expect(settings, 'bOverride_ flags are no suggestion').toContain('!It->GetName().StartsWith(TEXT("bOverride_"))');
  });

  // A custom node got its input before the wire feeding it; that mid-batch compile's
  // "missing input" stayed on a receipt whose final compile was clean.
  it('build_material_graph drops compile lines its final compile supersedes', () => {
    const batch = code('MaterialAuthoring', 'McpAutomationBridge_MaterialAuthoringGraphBatch.cpp');
    expect(batch.indexOf('Bridge->ForgetCapturedMessages(')).toBeGreaterThan(-1);
    expect(batch.indexOf('Bridge->ForgetCapturedMessages(')).toBeLessThan(batch.indexOf('RunStep(CompileId, Compile);'));
  });

  // The engine names the asset in a compile failure by the file it is saved in ("[AssetLog] <disk path>.uasset: Failed to
  // compile Material ..."), and by the object path only while no file exists. The forget matched the package name alone,
  // so every line of a saved material stayed (live: M_Toon, three "missing input" lines in warnings and engineWarnings
  // beside "compiles and was saved"); the receipt shows the disk path as its package path, which hid the mismatch.
  it('the compile lines it forgets are found by the asset file the engine prints for a saved material, not only by its object path', () => {
    const batch = code('MaterialAuthoring', 'McpAutomationBridge_MaterialAuthoringGraphBatch.cpp');

    expect(batch).toContain('const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);');
    expect(batch).toContain('const FString FileName = FPackageName::GetShortName(*PackageName) + FPackageName::GetAssetPackageExtension();');
    expect(batch).toMatch(
      /ForgetCapturedMessages\(\[&PackageName, &FileName\]\(const FString& Line\) \{\s*return Line\.StartsWith\(TEXT\("\[LogMaterial\]"\)\) && \(Line\.Contains\(PackageName\)\s*\|\| Line\.Contains\(TEXT\("\\\\"\) \+ FileName\) \|\| Line\.Contains\(TEXT\("\/"\) \+ FileName\)\);\s*\}\);/u
    );
  });

  it('add_material_node nodeKind=batch is that same handler, so its receipt is pruned alike', () => {
    const fold = capabilityIndex().byId.get('material.add_material_node')?.routing.dispatchBy;

    expect(fold?.param).toBe('nodeKind');
    expect(fold?.actions.batch).toBe('build_material_graph');
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

  // set_material_parameter with a parameters list answered {parameters, applied} and no assetPath, so the receipt
  // named nothing for a call that wrote an instance; every single-parameter setter answers it through AddVerification.
  it('set_material_parameter names the asset its parameters list wrote, as the single-parameter setters do', () => {
    const list = code('MaterialAuthoring', 'Parameters', 'McpAutomationBridge_MaterialAuthoringHandlersSetMaterialParameter.cpp');
    const setter = code('MaterialAuthoring', 'Parameters', 'McpAutomationBridge_MaterialAuthoringParameterValue.cpp');

    expect(list).toMatch(
      /if \(Reply\.bSuccess\) \{\s*if \(OutChangedAssetPath && OutChangedAssetPath->IsEmpty\(\) && Reply\.Result\.IsValid\(\)\) \{\s*Reply\.Result->TryGetStringField\(TEXT\("assetPath"\), \*OutChangedAssetPath\);\s*\}\s*\} else \{/u
    );
    expect(list).toMatch(/FString ChangedAsset;\s*ApplyMaterialParameterList\([^;]*, &ChangedAsset\);/u);
    expect(list).toMatch(/if \(!ChangedAsset\.IsEmpty\(\)\) \{\s*Data->SetStringField\(TEXT\("assetPath"\), ChangedAsset\);\s*\}/u);
    expect(setter.split('McpHandlerUtils::AddVerification(Result,'), 'the instance branch and the base material branch').toHaveLength(3);
  });

  // The parameter setter reads save from its own payload and a list entry carries only its own fields, so a
  // save:false given once for the call was dropped and every entry saved the asset.
  it('a parameters list gives every entry the call\'s own save flag unless the entry names one', () => {
    const list = code('MaterialAuthoring', 'Parameters', 'McpAutomationBridge_MaterialAuthoringHandlersSetMaterialParameter.cpp');
    const create = code('MaterialAuthoring', 'Creation', 'McpAutomationBridge_MaterialAuthoringHandlersCreateMaterialInstance.cpp');
    const setter = code('MaterialAuthoring', 'Parameters', 'McpAutomationBridge_MaterialAuthoringParameterValue.cpp');

    expect(setter).toContain('Payload->TryGetBoolField(TEXT("save"), bSave);');
    expect(list).toMatch(
      /One->SetStringField\(TEXT\("assetPath"\), AssetPath\);\s*if \(Shared\.IsValid\(\) && !One->HasField\(TEXT\("save"\)\) && Shared->HasField\(TEXT\("save"\)\)\) \{\s*One->SetField\(TEXT\("save"\), Shared->TryGetField\(TEXT\("save"\)\)\);\s*\}/u
    );
    expect(list).toMatch(/ApplyMaterialParameterList\(Bridge, RequestId, GetJsonStringField\(Payload, TEXT\("assetPath"\)\), \*Entries, Payload, Socket,/u);
    expect(create).toMatch(/ApplyMaterialParameterList\(Bridge, RequestId, NewInstance->GetOutermost\(\)->GetName\(\), \*Entries, Payload, Socket,/u);
  });

  // get_material_info listed the UMaterialExpressionParameter kinds only, so a material built from
  // TextureObjectParameters (BaseColor, Normal, ORM) answered its scalars and vectors and no textures.
  it('get_material_info lists texture parameters with their default texture and sampler type, and find_node matches them by name', () => {
    const info = code('MaterialAuthoring', 'Queries', 'McpAutomationBridge_MaterialAuthoringHandlersGetMaterialInfo.cpp');
    const find = code('MaterialAuthoring', 'Queries', 'McpAutomationBridge_MaterialAuthoringHandlersFindNode.cpp');

    expect(info).toMatch(/if \(Expr->HasAParameterName\(\)\) \{\s*TSharedPtr<FJsonObject> ParamObj = McpHandlerUtils::CreateResultObject\(\);\s*ParamObj->SetStringField\(TEXT\("name"\), Expr->GetParameterName\(\)\.ToString\(\)\);[\s\S]*?AddTextureParameterFields\(Expr, ParamObj\);/u);
    expect(info).toMatch(/if \(Expr->HasAParameterName\(\)\) \{\s*ExprObj->SetStringField\(TEXT\("name"\), Expr->GetParameterName\(\)\.ToString\(\)\);/u);
    expect(info).toContain('Row->SetStringField(TEXT("texture"), Sampled->Texture ? Sampled->Texture->GetPathName() : FString());');
    expect(info).toContain('Row->SetStringField(TEXT("samplerType"), MaterialEnumShortName(StaticEnum<EMaterialSamplerType>(), Sampled->SamplerType));');
    expect(info).toContain('Row->SetStringField(TEXT("texture"), Rvt->VirtualTexture ? Rvt->VirtualTexture->GetPathName() : FString());');
    expect(info, 'the cast that skipped the texture kinds is gone').not.toContain('Cast<UMaterialExpressionParameter>');
    expect(find).toMatch(/if \(Expr->HasAParameterName\(\)\) \{\s*bNameMatch = Expr->GetParameterName\(\)\.ToString\(\)\.Contains\(SearchName\);/u);

    const properties = capabilityIndex().byId.get('material.get_material_info')?.schemas.output.properties;
    const parameters = isRecord(properties) ? properties.parameters : undefined;
    expect(isRecord(parameters) ? parameters.description : '').toMatch(/texture parameter also carries texture .* and samplerType/u);
  });

  // set_property on a material expression (<material>.<material>:<nodeName>) ran only the expression's own PostEditChange,
  // which never reaches its material: the cached expression data every instance reads kept the old ParameterName until
  // some other call rebuilt the material.
  it('set_property on an object inside a material or material function rebuilds that material, as compile_material does', () => {
    const access = code('Property', 'McpAutomationBridge_PropertyHandlersActorAccess.cpp');
    const set = code('Property', 'McpAutomationBridge_PropertyHandlersObjectSet.cpp');
    const compile = code('MaterialAuthoring', 'Properties', 'McpAutomationBridge_MaterialAuthoringHandlersCompileMaterial.cpp');

    expect(access).toMatch(/bool RefreshMaterialHostAfterEdit\(UObject\* Edited\)\s*\{\s*UObject\* Host = Edited \? Edited->GetTypedOuter<UMaterial>\(\) : nullptr;\s*if \(!Host && Edited\)\s*\{\s*Host = Edited->GetTypedOuter<UMaterialFunction>\(\);\s*\}\s*if \(!Host\)\s*\{\s*return false;\s*\}\s*Host->PreEditChange\(nullptr\);\s*Host->PostEditChange\(\);\s*return true;\s*\}/u);
    expect(compile, 'the same two calls compile_material makes').toMatch(/Host->PreEditChange\(nullptr\);\s*Host->PostEditChange\(\);/u);
    expect(set).toMatch(/RefreshK2NodeTitleCacheIfNeeded\(RootObject\);\s*const bool bMaterialRebuilt = McpPropertyActorAccess::RefreshMaterialHostAfterEdit\(RootObject\);/u);
    expect(set.indexOf('RefreshMaterialHostAfterEdit(RootObject)'), 'before the save, so the saved material is the rebuilt one').toBeLessThan(set.indexOf('McpSafeAssetSave(OwningPackage)'));
    expect(set).toMatch(/if \(bMaterialRebuilt\) \{\s*ResultPayload->SetBoolField\(TEXT\("materialRebuilt"\), true\);\s*\}/u);

    const properties = capabilityIndex().byId.get('inspect.set_property')?.schemas.output.properties;
    const rebuilt = isRecord(properties) ? properties.materialRebuilt : undefined;
    expect(isRecord(rebuilt) ? rebuilt.description : '').toMatch(/inside a material or material function .* that material was rebuilt/u);
  });

  // disconnect_nodes answered "Disconnect operation completed." with nothing unplugged.
  it('disconnect_nodes fails when no pin matched and unplugs a custom node input by label', () => {
    const source = code('MaterialAuthoring', 'Connections', 'McpAutomationBridge_MaterialAuthoringHandlersDisconnectNodes.cpp');
    expect(source).not.toContain('Disconnect operation completed');
    expect(source).toContain('Target = TargetExpr->GetInput(InputIndex);');
  });

  // A TextureObjectParameter added by type stayed named "None" and had no texture, so no
  // instance could set it; add_texture_sample dropped a path that did not load.
  it('texture nodes take their name and texture, and a texture that does not load fails', () => {
    const generic = code('MaterialAuthoring', 'Nodes', 'McpAutomationBridge_MaterialAuthoringHandlersAddMaterialNode.cpp');
    expect(generic).toContain('if (NewExpr->HasAParameterName()) {');
    expect(generic).toContain('TextureNode->AutoSetSampleType();');
    const sample = code('MaterialAuthoring', 'Nodes', 'McpAutomationBridge_MaterialAuthoringHandlersAddTextureSample.cpp');
    expect(sample).toMatch(/if \(!ResolvedTexture\) \{\s*Bridge->SendAutomationError\([\s\S]*?TEXT\("ASSET_NOT_FOUND"\)\);/u);
    expect(sample).toContain('CreatedExpr->AutoSetSampleType();');
  });

  // revolve turned the same fixed vase at the origin whatever profile, name or place was asked.
  it('revolve turns the given profile, placed and named as asked', () => {
    const source = code('Geometry', 'Primitives', 'McpAutomationBridge_GeometryPrimitivesShapes.cpp');
    expect(source).toContain('Payload->TryGetArrayField(TEXT("profile"), Profile)');
    expect(source).toContain('RevolveOptions.RevolveDegrees = ');
    expect(source).toContain('ProfilePoints, RevolveOptions, Steps, bCap, nullptr);');
    expect(source).toContain('SpawnPrimitiveOrReply(Self, RequestId, Socket, ReadTransformFromPayload(Payload), Name, DynMesh, Result)');
    expect(source).toContain('Result->SetBoolField(TEXT("usedDefaultProfile"), bDefaultProfile);');
  });
});
