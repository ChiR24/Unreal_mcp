// Replies that claimed more than the handler did: a success that wrote nothing, a "saved" that
// echoed the request, a debug field, a modal dialog nobody can answer. Wiring contracts only;
// behaviour needs an editor (tests/mcp-tools/**).

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { validateAgainstCapabilitySchema } from '../../../src/server/gateway/gateway-schema-validate.js';
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

  // A folder of 391 unsaved imported assets took minutes (one engine delete per asset) and two of them
  // survived while the reply said the folder was deleted.
  it('a folder delete runs a few batched engine passes, never one per asset, and names what survived', () => {
    const safety = (file: string): string =>
      readFileSync(join(DOMAINS, '..', 'Safety', file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
    const batch = safety('McpSafeOperationsFolderDeleteAssets.h');
    expect(batch).toContain('DeletedByEngine += ObjectTools::ForceDeleteObjects(Objects, false);');
    expect(batch.split('ObjectTools::ForceDeleteObjects('), 'one call site, run per pass').toHaveLength(2);
    // At most about twenty passes, each reporting progress: one pass for thousands of assets said nothing for minutes.
    expect(batch).toContain('const int32 PassSize = FMath::Max(100, SafeAssets.Num() / 20 + 1);');
    expect(safety('McpSafeOperationsAnimationDelete.h')).toContain('ForceDeleteBatch(InMemoryStillLoaded, TEXT("InMemoryOnlyLeftovers"))');
    const verify = safety('McpSafeOperationsFolderDeleteVerify.h');
    expect(verify).toContain('IsValid(FindObject<UObject>(nullptr, *ObjectPath))');
    expect(verify).toContain('if (OutRemaining) { OutRemaining->Add(SurvivorPath); }');
    expect(verify).not.toContain('without backing files remain');
    expect(code('AssetWorkflow', 'Operations', 'McpAutomationBridge_AssetWorkflowAssetMutation.cpp'))
      .toContain('FailedToDeletePaths.Add(Left + TEXT(" (left in the deleted folder)"));');
  });

  it('bulk_delete never asks the engine for a confirmation dialog', () => {
    const source = code('AssetWorkflow', 'Operations', 'McpAutomationBridge_AssetWorkflowBulkDelete.cpp');
    expect(source).not.toContain('showConfirmation');
    expect(source).not.toContain('bShowConfirmation)');
  });

  // Five failed saves of a texture were reported saved: true because an older file was still on
  // disk. And the first overwrite of an existing texture in a session built a new object over the
  // unloaded asset, whose source the editor then refused to save.
  it('a generated texture is saved for real or reported unsaved, and an existing one is loaded and refilled', () => {
    const assets = code('Texture', 'McpAutomationBridge_TextureHandlersAssets.cpp');
    expect(assets).toContain('return McpSafeAssetSave(Texture);');
    expect(assets).not.toContain('bExistsOnDisk');
    expect(assets).toMatch(/LoadObject<UTexture2D>\(nullptr, \*ObjectPath/u);
    expect(assets.indexOf('LoadObject<UTexture2D>')).toBeLessThan(assets.indexOf('CreatePackage('));
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

  // Each step of a batch recompiled the material: a Custom node that got its inputs from update_custom_expression and its
  // wires two steps later was translated half-built ("Custom material X missing input 20 (OP)") once per step, and the lines
  // stood on a receipt whose final compile was clean, on top of N compiles for one edit.
  it('a step of build_material_graph leaves the recompile to the batch, and every batchable edit finishes through the one helper', () => {
    const header = code('MaterialAuthoring', 'McpAutomationBridge_MaterialAuthoringHandlersPrivate.h');
    const batch = code('MaterialAuthoring', 'McpAutomationBridge_MaterialAuthoringGraphBatch.cpp');
    const compile = code('MaterialAuthoring', 'Properties', 'McpAutomationBridge_MaterialAuthoringHandlersCompileMaterial.cpp');

    expect(header).toMatch(/inline void McpFinishMaterialEdit\(const FString &RequestId, UObject \*Host\) \{\s*if \(Host && !FMcpResponseCaptureRegistry::Get\(\)\.IsCapturing\(RequestId\)\) \{ Host->PostEditChange\(\); \}\s*if \(Host\) \{ Host->MarkPackageDirty\(\); \}\s*\}/u);
    expect(header).toContain('#define FINALIZE_HOST() McpFinishMaterialEdit(RequestId, Material ? static_cast<UObject *>(Material) : static_cast<UObject *>(Function))');
    for (const file of [
      ['Nodes', 'McpAutomationBridge_MaterialAuthoringHandlersAddMaterialNode.cpp'],
      ['Nodes', 'McpAutomationBridge_MaterialAuthoringHandlersUseMaterialFunction.cpp'],
      ['Nodes', 'McpAutomationBridge_MaterialAuthoringHandlersFunctionInputsOutputs.cpp'],
      ['Properties', 'McpAutomationBridge_MaterialAuthoringHandlersSetMaterialEnum.cpp'],
      ['Properties', 'McpAutomationBridge_MaterialAuthoringHandlersSetTwoSided.cpp'],
    ] as const) {
      const source = code('MaterialAuthoring', ...file);
      expect(source, file[1]).toContain('McpFinishMaterialEdit(RequestId, ');
      expect(source, `${file[1]} recompiles only through the helper`).not.toMatch(/(?:HostOuter|Material|Func|Function)->PostEditChange\(\)/u);
    }
    expect(compile, 'the compile that ends the batch always recompiles').toContain('Host->PostEditChange();');
    expect(compile).not.toContain('McpFinishMaterialEdit');
    expect(batch, 'the steps before a failing one were applied without a compile of their own').toContain('if (Index > 0) { McpFinishMaterialEdit(RequestId, HostOuter); }');
    expect(batch.indexOf('if (Index > 0) { McpFinishMaterialEdit(RequestId, HostOuter); }')).toBeLessThan(batch.indexOf('TEXT("build_material_graph stopped at operations[%d]'));
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

  // update_custom_expression ended in the edit helper (a recompile and a dirty mark) and answered "Custom expression updated.":
  // live, the material stayed in unsavedPackages and a broken edit went unseen. Called on its own it compiles, saves and
  // reports as compile_material does; as a step of build_material_graph it leaves both to the batch.
  it('update_custom_expression on its own saves the material and says whether it compiles, as compile_material does', () => {
    const source = code('MaterialAuthoring', 'Nodes', 'McpAutomationBridge_MaterialAuthoringHandlersUpdateCustomExpression.cpp');
    const own = source.slice(source.indexOf('if (!FMcpResponseCaptureRegistry::Get().IsCapturing(RequestId)) {'));

    expect(source.indexOf('FINALIZE_HOST();'), 'after the edit').toBeLessThan(source.indexOf('IsCapturing(RequestId)'));
    expect(own).toContain('CompileErrors = Resource->GetCompileErrors();');
    expect(own).toContain('Result->SetBoolField(TEXT("compiled"), CompileErrors.Num() == 0);');
    expect(own).toContain('Result->SetBoolField(TEXT("saved"), Material ? McpSafeAssetSave(Material) : McpSafeAssetSave(Function));');
    expect(own).toContain('WARNING: the material does not compile');
    expect(own.indexOf('Message +='), 'the warning is in the message the reply carries').toBeLessThan(own.indexOf('SendAutomationResponse(Socket, RequestId, true, Message, Result);'));
    const properties = capabilityIndex().byId.get('material.update_custom_expression')?.schemas.output.properties;
    expect(Object.keys(isRecord(properties) ? properties : {})).toEqual(expect.arrayContaining(['compiled', 'compileErrors', 'saved']));
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

  // Tuning one emitter took a call, a compile request and a save per value: twelve calls for one emitter.
  it('set_parameter_value writes a parameters list in one pass, reports every entry and saves once', () => {
    const source = code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersParameterValues.cpp');
    const between = (from: string, to?: string): string => source.slice(source.indexOf(from), to === undefined ? undefined : source.indexOf(to));
    const writer = between('static FParameterWrite WriteParameter(', 'static bool SetParameterValueList(');
    const list = between('static bool SetParameterValueList(', 'bool SetParameterValue(FActionContext& Context)');
    const single = between('bool SetParameterValue(FActionContext& Context)');

    expect(writer, 'one value written: no reply, no compile request, no save').not.toMatch(/SendError|SendSuccess|MarkDirtyAndVerify|RequestCompile/u);
    expect(list).toContain('Write = WriteParameter(Context, System, Name, Entry);');
    expect(list.split('MarkDirtyAndVerify(Context, System);'), 'one save for the whole list').toHaveLength(2);
    expect(list.split('System->RequestCompile(false);'), 'and one compile request').toHaveLength(2);
    expect(list).toMatch(/if \(Applied > 0\)\s*\{\s*MarkDirtyAndVerify\(Context, System\);\s*\}/u);
    expect(list).toContain('Row->SetBoolField(TEXT("applied"), Write.bApplied);');
    expect(list).toContain('Row->SetStringField(TEXT("error"), Write.Error);');
    expect(list).toContain('Context.Result->SetArrayField(TEXT("parameters"), Results);');
    expect(list).toMatch(/Context\.Subsystem->SendAutomationResponse\(Context\.RequestingSocket, Context\.RequestId, false,[\s\S]*?TEXT\("PARAMETER_BATCH_INCOMPLETE"\)\);/u);
    expect(single).toMatch(/Context\.Payload->TryGetArrayField\(TEXT\("parameters"\), Entries\) && Entries->Num\(\) > 0\)/u);
    expect(single).toMatch(/if \(!ParamName\.IsEmpty\(\)\)\s*\{\s*Context\.SendError\(TEXT\("Send parameters[^"]*not both\."\), TEXT\("INVALID_ARGUMENT"\)\);/u);
    expect(single, 'one parameter keeps its own errors and its own compile request').toContain('Context.SendError(Write.Error, Write.ErrorCode);');
    expect(code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersParameters.cpp')).toContain('if (SubAction == TEXT("set_parameter_value")) return SetParameterValue(Context);');
  });

  it('the record declares the list, one of the two ways to name a value, and what the reply carries', () => {
    const record = capabilityIndex().byId.get('manage_effect.edit_niagara_system');
    const input = record?.schemas.input;
    const output = record?.schemas.output.properties;

    expect(record?.routing.dispatchBy?.declaredBy?.parameters, 'a call that sends a list and no edit runs set_parameter_value').toEqual(['set_parameter_value']);
    const ok = { edit: 'set_parameter_value', systemPath: '/Game/NS_Fire', parameters: [{ parameterName: 'InitializeParticle.Lifetime', parameterValue: 2 }, { parameterName: 'Size', parameterValue: [1, 2, 3] }] };
    expect(validateAgainstCapabilitySchema(ok, input)).toBeUndefined();
    expect(validateAgainstCapabilitySchema({ ...ok, parameters: [{ parameterName: 'Size' }] }, input)?.pointer, 'an entry without a value').toMatch(/^\/parameters\/0/u);
    expect(validateAgainstCapabilitySchema({ ...ok, parameters: [{ parameterName: 'Size', parameterValue: 1, save: false }] }, input)?.pointer, 'an entry key nothing reads').toMatch(/^\/parameters\/0/u);
    expect(validateAgainstCapabilitySchema({ ...ok, parameters: [] }, input)?.pointer, 'an empty list').toBe('/parameters');
    for (const name of ['parameters', 'applied', 'moduleInputCopiesWritten', 'saved']) {
      expect(isRecord(output) ? Object.keys(output) : [], name).toContain(name);
    }
  });

  // manage_level load of the level that is already open answered "Level loaded" in 34 ms with dirtyWorldPackagesBeforeLoad 0, while
  // get_summary listed that world under unsavedPackages: McpSafeLoadMap skips the open map, and only the headless path ever counted.
  describe('manage_level load of the level that is already open', () => {
    const load = (): string => code('Level', 'Lifecycle', 'McpAutomationBridge_LevelHandlersLoad.cpp');
    const compact = (): string => load().split(/\s+/u).join(' ');

    it('counts the dirty packages in every mode, before anything else decides', () => {
      const source = load();

      expect(source.split('CountBlockingDirtyPackages(DirtyWorldPackagesBeforeLoad, DirtyContentPackagesBeforeLoad);'), 'once, outside the headless branch').toHaveLength(2);
      expect(source.indexOf('CountBlockingDirtyPackages('), 'before the refusal and the save').toBeLessThan(source.indexOf('const bool bHeadless ='));
    });

    it('answers a no-op as one: alreadyLoaded, not reloaded, the unsaved state, and it never calls McpSafeLoadMap', () => {
      const source = load();
      const noop = source.slice(source.indexOf('if (bAlreadyOpen) {'), source.indexOf('const bool bLoaded = McpSafeLoadMap('));

      expect(source).toContain('const bool bAlreadyOpen = OpenWorld && OpenWorld->GetOutermost()->GetName().Equals(ExpectedLoadedPath, ESearchCase::IgnoreCase);');
      expect(noop).toContain('TSharedPtr<FJsonObject> Resp = LoadReply(false);');
      expect(noop).toContain('AddUnsavedState(Resp, OpenWorld->PersistentLevel);');
      expect(noop).toContain('Level already open: nothing was reloaded');
      expect(noop).toContain('its unsaved changes were kept');
      expect(noop, 'a no-op neither stops a running PIE session nor loads').not.toContain('McpSafeLoadMap');
      expect(source.indexOf('if (bAlreadyOpen) {'), 'decided before the load runs').toBeLessThan(source.indexOf('const bool bLoaded = McpSafeLoadMap('));
      expect(compact()).toContain('Resp->SetBoolField(TEXT("alreadyLoaded"), !bReloaded); Resp->SetBoolField(TEXT("reloaded"), bReloaded);');
      expect(compact()).toContain('TEXT("Level loaded"), LoadReply(true), FString());');
    });

    it('refuses the headless load over dirty packages only for a real load, and honours saveDirtyPackages in every mode', () => {
      const source = compact();

      expect(source).toContain('if (bHeadless && !bAlreadyOpen && DirtyWorldPackagesBeforeLoad + DirtyContentPackagesBeforeLoad > 0 && !bSaveDirtyPackages) {');
      expect(source).toContain('if (bSaveDirtyPackages) { bSavedDirtyPackagesBeforeLoad = SaveBlockingDirtyPackagesForLevelLoad(');
      expect(source, 'the save is no longer inside the headless branch').not.toContain('if (FApp::IsUnattended() || IsRunningCommandlet()');
    });

    it('the record says what the call does for the open level, and declares the fields that tell', () => {
      const record = capabilityIndex().byId.get('manage_level.load');
      const output = record?.schemas.output.properties;

      expect(record?.discovery.whenNotToUse.join(' ')).toMatch(/already the open one: the call reloads nothing and keeps its unsaved changes/u);
      for (const name of ['alreadyLoaded', 'reloaded', 'dirtyWorldPackagesBeforeLoad', 'dirtyContentPackagesBeforeLoad', 'unsaved', 'unsavedPackages', 'unsavedPackageCount']) {
        expect(isRecord(output) ? Object.keys(output) : [], name).toContain(name);
      }
    });
  });
});

// One session's level load dropped another session's unsaved stage edits: FEditorFileUtils::LoadMap never asks about
// unsaved levels (only the editor's Open Level dialog does), and only a headless editor was ever refused.
describe('opening another level over unsaved level changes', () => {
  // Each caller, and the call that loads there: the check has to answer before it.
  const callers: readonly (readonly [readonly string[], string])[] = [
    [['Level', 'Lifecycle', 'McpAutomationBridge_LevelHandlersLoad.cpp'], 'const bool bLoaded = McpSafeLoadMap('],
    [['Level', 'Lifecycle', 'McpAutomationBridge_LevelHandlersCreate.cpp'], 'McpSafeLoadMap(SavePath, true)'],
    [['ControlEditor', 'McpAutomationBridge_ControlEditorLevel.cpp'], 'McpSafeLoadMap(MapPathToLoad)'],
    [['LevelStructure', 'McpAutomationBridge_LevelStructureLevelCreation.cpp'], 'LoadIfRequested(Result,'],
  ];

  it('is refused in every mode by one shared check, which lists what would be lost', () => {
    const safety = readFileSync(join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Safety', 'McpSafeOperationsMapLoad.h'), 'utf8');
    expect(safety).toContain('inline FString McpRefuseLoadOverUnsavedLevels(const FString& MapPath, const TCHAR* WayOut, TSharedPtr<FJsonObject>& OutDetails)');
    expect(safety).toContain('FEditorFileUtils::GetDirtyWorldPackages(Dirty);');
    expect(safety).toContain('OutDetails->SetArrayField(TEXT("unsavedPackages"), Names);');

    for (const [segments, loadCall] of callers) {
      const file = segments[segments.length - 1];
      const source = code(...segments);
      const check = source.indexOf('McpRefuseLoadOverUnsavedLevels(');
      expect(check, file).toBeGreaterThan(-1);
      expect(check, `${file}: the check answers before the load`).toBeLessThan(source.indexOf(loadCall));
      expect(source.slice(check, check + 600), file).toContain('TEXT("DIRTY_PACKAGES")');
    }
  });

  it('manage_level load gets past it only with discardUnsaved or a save, and says so', () => {
    const source = code('Level', 'Lifecycle', 'McpAutomationBridge_LevelHandlersLoad.cpp').split(/\s+/u).join(' ');
    expect(source).toContain('Payload->TryGetBoolField(TEXT("discardUnsaved"), bDiscardUnsaved);');
    expect(source).toContain('const FString Loss = bDiscardUnsaved ? FString() : McpSafeOperations::McpRefuseLoadOverUnsavedLevels(');
    expect(source, 'the refusal names the /Game level, never the file it loads from').toContain('McpRefuseLoadOverUnsavedLevels( ExpectedLoadedPath,');
    expect(source.indexOf('if (bSaveDirtyPackages) {'), 'a save asked for runs first, so nothing is left to refuse').toBeLessThan(source.indexOf('McpRefuseLoadOverUnsavedLevels('));

    const record = capabilityIndex().byId.get('manage_level.load');
    expect(Object.keys(record?.schemas.input.properties ?? {})).toContain('discardUnsaved');
    expect(record?.discovery.whenNotToUse.join(' '), 'the fold keeps the line').toMatch(/refused with DIRTY_PACKAGES/u);
  });
});

// The camera of a running game: the level viewport is hidden behind the game view, so a camera moved there changed
// nothing on screen while the call said success, and `Exec("Eject")` (no such console command) ejected nobody.
describe('the view of a running game', () => {
  const editorControl = (...segments: readonly string[]): string => code('ControlEditor', ...segments);
  const outputNames = (id: string): readonly string[] => Object.keys(capabilityIndex().byId.get(id)?.schemas.output.properties ?? {});

  it('eject switches the session the way the editor\'s Eject button does, and answers once the view has switched', () => {
    const source = editorControl('Session', 'McpAutomationBridge_ControlEditorEject.cpp');
    expect(source).toContain('GEditor->RequestToggleBetweenPIEandSIE();');
    expect(source, 'there is no Eject console command').not.toMatch(/Exec\([^)]*Eject/u);
    expect(source).toContain('GetEjectedPieViewportClientForMcp()');
    expect(source).toContain('TEXT("EJECT_FAILED")');
    expect(editorControl('McpAutomationBridge_ControlEditorPlay.cpp')).not.toContain('HandleControlEditorEject');
    expect(outputNames('control_editor.play')).toEqual(expect.arrayContaining(['ejected', 'alreadyEjected', 'view']));
  });

  it('set_camera refuses until the player is ejected, and says which view it moved', () => {
    const camera = editorControl('McpAutomationBridge_ControlEditorCamera.cpp');
    const setCamera = camera.slice(camera.indexOf('HandleControlEditorSetCamera('));
    const refusal = setCamera.indexOf('RefuseCameraMoveWhilePieFollowsPawnForMcp(');
    expect(refusal).toBeGreaterThan(-1);
    expect(refusal, 'the refusal comes before a viewport is picked').toBeLessThan(setCamera.indexOf('GetActiveEditorViewportClientForMcp()'));
    expect(setCamera).toContain('TEXT("pie_ejected")');
    const support = editorControl('McpAutomationBridge_ControlEditorViewportSupport.cpp');
    expect(support).toContain('TEXT("PIE_VIEW_NOT_EJECTED")');
    expect(support, 'the shared viewport lookup answers the ejected view first').toContain('if (FEditorViewportClient *Ejected = GetEjectedPieViewportClientForMcp()) {');
    expect(outputNames('control_editor.set_camera')).toEqual(expect.arrayContaining(['view', 'cameraLocation', 'cameraRotation']));
  });

  it('a screenshot of an ejected game is taken from the editor viewport that draws it, and a camera it cannot move is refused', () => {
    const shot = editorControl('McpAutomationBridge_ControlEditorScreenshot.cpp');
    expect(shot).toContain('Mode == TEXT("game_viewport") && !bEjectedView');
    expect(shot).toContain('GEditor->PlayWorld != nullptr && !bEjectedView && GEditor->GetPIEViewport() != nullptr');
    expect(shot).toContain('RefuseCameraMoveWhilePieFollowsPawnForMcp(this, Socket, RequestId,');
    expect(shot, 'the note that said location and rotation were ignored is gone').not.toContain('cameraNote');
    expect(code('Ui', 'McpAutomationBridge_UiHandlersScreenshot.cpp')).toContain('Mode == TEXT("game_viewport") && GetEjectedPieViewportClientForMcp()');
    expect(outputNames('control_editor.screenshot')).toContain('view');
  });

  it('possess hands the player controller the pawn, and with no pawn brings an ejected player back', () => {
    const session = editorControl('Session', 'McpAutomationBridge_ControlEditorEject.cpp');
    const possess = session.slice(session.indexOf('HandleControlEditorPossess('));
    expect(possess).toContain('Controller->Possess(Pawn);');
    expect(possess, 'the way back from eject is the same toggle').toContain('GEditor->RequestToggleBetweenPIEandSIE();');
    expect(possess, 'there is no POSSESS console command').not.toMatch(/Exec\([^)]*POSSESS/u);
    expect(editorControl('McpAutomationBridge_ControlEditorPlay.cpp')).not.toContain('HandleControlEditorPossess(');
    expect(outputNames('control_editor.play')).toEqual(expect.arrayContaining(['possessed', 'returnedFromEject']));
  });

  it('the other level-viewport moves refuse while the player plays, and the view settings reach the game on screen', () => {
    expect(editorControl('Camera', 'McpAutomationBridge_ControlEditorCameraFocus.cpp'))
      .toContain('RefuseCameraMoveWhilePieFollowsPawnForMcp(this, Socket, RequestId, TEXT("focus_actor"))');
    const toggles = editorControl('McpAutomationBridge_ControlEditorViewportToggles.cpp');
    const gameView = toggles.slice(toggles.indexOf('HandleControlEditorSetGameView('));
    const refusal = gameView.indexOf('TEXT("PIE_VIEW_NOT_EJECTED")');
    expect(refusal).toBeGreaterThan(-1);
    expect(refusal, 'refused before the hidden level viewport is toggled').toBeLessThan(gameView.indexOf('Client->SetGameView('));
    const viewMode = editorControl('McpAutomationBridge_ControlEditorViewMode.cpp');
    expect(viewMode).toMatch(/ApplyViewMode\(ViewModeIndex,\s*true,\s*GEditor->GameViewport->EngineShowFlags\)/u);
    expect(viewMode).toContain('Controller->PlayerCameraManager->SetFOV(static_cast<float>(Fov));');
  });
});

// A mesh ASSET's material slots: what each one holds, and which part of the mesh it covers.
describe('the material slots of a mesh asset', () => {
  const outputNames = (id: string): readonly string[] => Object.keys(capabilityIndex().byId.get(id)?.schemas.output.properties ?? {});

  it('set_mesh_materials loads each material strictly, fails on a refused entry and never saves engine content', () => {
    const slots = code('AssetWorkflow', 'Materials', 'McpAutomationBridge_AssetWorkflowMeshMaterialSlots.h');
    expect(slots).toContain('McpLoadAsset(SafePath)');
    expect(slots, 'a material that does not load is refused, never replaced by a default').not.toContain('McpLoadMaterialWithFallback');
    const handler = code('AssetWorkflow', 'Materials', 'McpAutomationBridge_AssetWorkflowMeshMaterials.cpp');
    expect(handler).toContain('TEXT("MATERIAL_SLOTS_PARTIAL")');
    expect(handler).toContain('TEXT("MATERIAL_SLOTS_REFUSED")');
    expect(handler).toContain('TEXT("engine content is not saved")');
    expect(code('AssetWorkflow', 'McpAutomationBridge_AssetWorkflowHandlers.cpp')).toContain('McpMeshMaterials::HandleSetMeshMaterials(');
    expect(outputNames('asset.process_asset')).toEqual(expect.arrayContaining(['materialSlots', 'applied', 'refused', 'saved']));
  });

  it('mesh details attribute each LOD0 section to the slot its MaterialIndex names', () => {
    const mesh = code('Environment', 'Inspection', 'McpAutomationBridge_EnvironmentHandlersInspectAssetMesh.cpp');
    expect(mesh).toContain('Slots[Section.MaterialIndex]');
    expect(mesh).toContain('TEXT("slotBoundsAvailable")');
    expect(mesh).toContain('TEXT("triangles")');
    expect(outputNames('inspect.inspect_object')).toEqual(expect.arrayContaining(['materialSlots', 'slotBoundsAvailable']));
  });
});

describe('the main inputs of a material', () => {
  const inputs = code('MaterialAuthoring', 'McpAutomationBridge_MaterialAuthoringMainInputs.h');

  it('take the names other shading models show for the same pins', () => {
    expect(inputs).toContain('PinName == TEXT("Cloth") || PinName == TEXT("CustomData0")) return TEXT("ClearCoat");');
    expect(inputs).toContain('PinName == TEXT("CustomData1")) return TEXT("ClearCoatRoughness");');
    expect(inputs).toContain('PinName == TEXT("FuzzColor")) return TEXT("SubsurfaceColor");');
    expect(inputs).toContain('const FString Canonical = CanonicalMainInputName(PinName);');
  });

  it('list every accepted pin when connect or disconnect does not know the one asked for', () => {
    const connect = code('MaterialAuthoring', 'Connections', 'McpAutomationBridge_MaterialAuthoringHandlersConnectNodes.cpp');
    expect(connect).toContain('Unknown input on main node: %s. Main inputs: %s."), *InputName, *ListMainMaterialInputs(Material)');
    const disconnect = code('MaterialAuthoring', 'Connections', 'McpAutomationBridge_MaterialAuthoringHandlersDisconnectNodes.cpp');
    expect(disconnect).toContain('GetMainMaterialInput(Material, NormalizeMaterialInputName(PinName))');
    expect(disconnect).toContain('*ListMainMaterialInputs(Material)');
  });
});

describe('object and class references in a property bag', () => {
  const objects = readFileSync(
    join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Foundation', 'BridgeHelpers', 'Properties', 'McpAutomationBridgeHelpersPropertyApplyObjects.h'),
    'utf8',
  ).replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

  it('null, "" and "None" clear a hard or soft reference (edit_scs ChildActorClass "None" was refused)', () => {
    expect(objects).toContain('ValueField->AsString().Equals(TEXT("None"), ESearchCase::IgnoreCase)');
    expect(objects).toMatch(/if \(bClearReference\) \{\s*OP->SetObjectPropertyValue_InContainer\(TargetContainer, nullptr\);/u);
    expect(objects).toMatch(/if \(bClearReference\) \{\s*\*SoftPtr = FSoftObjectPtr\(\);/u);
  });

  it('an object of the wrong class is refused, not stored; a class must derive from the property\'s meta class', () => {
    expect(objects).toContain('CP ? !(AsClass && AsClass->IsChildOf(Wanted)) : !Res->IsA(Wanted)');
  });
});

describe('Nanite bakes and rebuilds', () => {
  it('convert_to_nanite turns Nanite on in the settings the new mesh copies and replies with what the mesh holds', () => {
    // From 5.1 the asset takes NaniteSettings whole and its bEnabled defaults to false, so bEnableNanite alone
    // baked plain meshes while the reply echoed naniteEnabled: true.
    const conversion = code('Geometry', 'Support', 'McpAutomationBridge_GeometryAssetConversion.cpp');
    expect(conversion).toContain('CreateOptions.NaniteSettings.bEnabled = bNanite;');
    expect(conversion).toContain('Result->SetBoolField(TEXT("naniteEnabled"), bNaniteOn);');
    expect(conversion).toContain('TEXT("NANITE_NOT_ENABLED")');
  });

  it('nanite_rebuild_mesh rebuilds and saves on every version, reads the result back, and both routes share it', () => {
    const nanite = code('AssetWorkflow', 'Optimization', 'McpAutomationBridge_AssetWorkflowNanite.cpp');
    expect(nanite).toContain('StaticMesh->Build(true);');
    expect(nanite).toMatch(/if \(!McpSafeAssetSave\(StaticMesh\)\) \{[\s\S]*?TEXT\("SAVE_FAILED"\)/u);
    expect(nanite).toContain('Resp->SetBoolField(TEXT("naniteEnabled"), After.bEnabled);');
    expect(nanite).not.toContain('PositionPrecision = 8');
    expect(code('Render', 'McpAutomationBridge_RenderHandlers.cpp')).toContain('return HandleNaniteRebuildMesh(RequestId, SubAction, Payload, RequestingSocket);');
  });
});
