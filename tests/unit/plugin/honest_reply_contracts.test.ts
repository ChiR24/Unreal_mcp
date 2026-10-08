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

  // A BlockingVolume from create_volume had the right bounds and blocked nothing: the brush had
  // polys but no BSP and no convex collision, so triggers and kill volumes never overlapped either.
  it('a volume brush gets its BSP and collision, as the editor\'s own volume placement builds them', () => {
    const geometry = code('Volume', 'McpAutomationBridge_VolumeGeometry.cpp');
    expect(geometry).toContain('FBSPOps::csgPrepMovingBrush(Volume);');
    expect(geometry.indexOf('CubeBuilder->Build(')).toBeLessThan(geometry.indexOf('csgPrepMovingBrush'));
    expect(readFileSync(join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'McpAutomationBridge.Build.cs'), 'utf8'))
      .toContain('"BSPUtils"');
  });

  // configure_exposure actorName "PostProcess" on a level with no volume failed ACTOR_NOT_FOUND,
  // while the same call with no name spawned one: a name that matches nothing now resolves alike.
  it('a post-process call naming a volume that does not exist resolves or creates one under that name', () => {
    const support = code('Render', 'McpAutomationBridge_RenderSupport.h');
    expect(support).not.toContain('PostProcessVolume not found: %s');
    expect(support).toMatch(/if \(!Volume\)\s*\{\s*FString ResolveError;/u);
    const resolver = readFileSync(join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Foundation', 'Render', 'McpPostProcessVolumeResolution.cpp'), 'utf8');
    expect(resolver).toContain('PPV->SetActorLabel(Reference);');
  });

  // Seven instances of one parent cost seven describes and seven consents.
  it('create_material_instance makes a palette under one consent, each entry through the single handler', () => {
    const create = code('MaterialAuthoring', 'Creation', 'McpAutomationBridge_MaterialAuthoringHandlersCreateMaterialInstance.cpp');
    expect(create).toContain('Payload->TryGetArrayField(TEXT("instances"), Instances)');
    expect(create).toMatch(/FMcpResponseCaptureRegistry::Get\(\)\.Begin\(StepId\);\s*HandleCreateMaterialInstance\(Bridge, StepId,/u);
    expect(create).toContain('Item->RemoveField(TEXT("instances"));');
    expect(create).toContain('TEXT("INSTANCE_BATCH_INCOMPLETE")');
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

  // create_audio_actor named its new component QuietComp, then get_property "QuietComp.bAutoActivate" on the actor
  // missed it: an instance component has no property behind it.
  it('a middle path segment may name an actor component exactly', () => {
    const resolver = code('..', 'Foundation', 'BridgeHelpers', 'Properties', 'McpAutomationBridgeHelpersNestedPropertyPath.h');
    expect(resolver).toContain('!CurrentProperty && !bIsLastSegment ? Cast<AActor>(CurrentObject) : nullptr');
    expect(resolver).toContain('Component->GetName().Equals(Segment, ESearchCase::IgnoreCase)');
    expect(resolver, 'a struct member is no actor').toMatch(/ContainerPtrToValuePtr<void>\(CurrentContainer\);\s*CurrentObject = nullptr;/u);
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

  // get_property "/Temp/X.X" answered OBJECT_NOT_FOUND while "/Temp/X" worked: an object path is never a package name.
  it('an object path under any mounted root resolves, not only under /Game, /Engine and /Script', () => {
    const resolution = code('..', 'Foundation', 'HandlerUtils', 'McpHandlerUtilsObjectResolution.cpp');
    expect(resolution).toContain('FPackageName::IsValidLongPackageName(FPackageName::ObjectPathToPackageName(Path), true)');
    expect(resolution).not.toContain('FPackageName::IsValidLongPackageName(Path, true)');
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
    expect(own).toContain('const TArray<FString> CompileErrors = McpMaterialCompileErrors(Material);');
    expect(own).toContain('Result->SetBoolField(TEXT("compiled"), CompileErrors.Num() == 0);');
    expect(own).toContain('Result->SetBoolField(TEXT("saved"), Material ? McpSafeAssetSave(Material) : McpSafeAssetSave(Function));');
    expect(own).toContain('WARNING: the material does not compile');
    expect(own.indexOf('Message +='), 'the warning is in the message the reply carries').toBeLessThan(own.indexOf('SendAutomationResponse(Socket, RequestId, true, Message, Result);'));
    const properties = capabilityIndex().byId.get('material.update_custom_expression')?.schemas.output.properties;
    expect(Object.keys(isRecord(properties) ? properties : {})).toEqual(expect.arrayContaining(['compiled', 'compileErrors', 'saved']));
  });

  // A Custom node's HLSL error (float3 .a) shows only once the asynchronous shader compile ends: read right after
  // PostEditChange, the build_material_graph batch answered "compiled": true while the default material rendered.
  it('compile errors are read after the shader compile, not just the translation', () => {
    const compile = code('MaterialAuthoring', 'Properties', 'McpAutomationBridge_MaterialAuthoringHandlersCompileMaterial.cpp');

    expect(compile, 'every shader, not the on-demand few a post-edit recompile asks for').toMatch(/Material->ForceRecompileForRendering\(\);\s*FMaterialResource \*Resource = MCP_GET_MATERIAL_RESOURCE\(Material\);/u);
    expect(compile).toMatch(/Resource->FinishCompilation\(\);\s*TArray<FString> Errors = Resource->GetCompileErrors\(\);/u);
    expect(compile, 'a failed shader map leaves none behind').toContain('if (Errors.Num() == 0 && !Resource->GetGameThreadShaderMap()) {');
    expect(compile).toContain('const TArray<FString> CompileErrors = McpMaterialCompileErrors(Material);');
    expect(code('MaterialAuthoring', 'McpAutomationBridge_MaterialAuthoringGraphBatch.cpp'), 'the batch compiles through compile_material')
      .toContain('Compile->SetStringField(TEXT("subAction"), TEXT("compile_material"));');
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

  // A Ribbon Width written while InitializeParticle.Ribbon Width Mode was Unset changed nothing, and no call could see or flip the
  // switch: a static switch is a pin of the module's call node, not a rapid-iteration parameter.
  it('set_parameter_value sets a module static switch as the stack does, and get_niagara_info lists them', () => {
    const values = code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersParameterValues.cpp');
    const graph = code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersStackGraph.cpp');

    expect(values).toContain('SetModuleStaticSwitch(System, Context.EmitterName, ParamName, Entry->TryGetField(TEXT("parameterValue")), SwitchError)');
    expect(code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersContext.h')).toContain('Pin->Direction == EGPD_Input && Pin->bNotConnectable && !Pin->bOrphanedPin');
    expect(graph).toMatch(/Pin->DefaultValue = Default;\s*Node->MarkNodeRequiresSynchronization\([^;]*, true\);/u);
    expect(graph, 'an enum pin holds the entry name the type editor writes').toContain('Enum->GetNameStringByValue(EntryValue)');
    const info = code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersInfoValidation.cpp');
    expect(info).toContain('EmitterObj->SetObjectField(TEXT("staticSwitches"), CollectModuleStaticSwitches(Handle));');
    expect(info, 'and each renderer by the object path set_property writes').toContain('RendererObj->SetStringField(TEXT("objectPath"), Renderer->GetPathName());');
  });

  // A module for the particle spawn stack of an emitter with interpolated spawning found no stack: its script
  // runs as ParticleSpawnScriptInterpolated while the stack output says ParticleSpawnScript.
  it('add_niagara_module finds the spawn stack of an emitter with interpolated spawning', () => {
    const source = code('NiagaraGraph', 'McpAutomationBridge_NiagaraGraphHandlers.cpp');
    expect(source).toContain('UNiagaraScript::IsEquivalentUsage(Candidate->GetUsage(), TargetScript->GetUsage())');
    expect(source).not.toContain('Candidate->GetUsage() == TargetScript->GetUsage()');
  });

  // A template module that worked against the values set (a size curve, wind) could not be switched off.
  it('set_emitter_properties turns stack modules on or off by name and refuses a name it cannot find', () => {
    const systems = code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersSystems.cpp');
    expect(systems).toMatch(/TryGetObjectField\(TEXT\("moduleEnabled"\), ModulesObj\)/u);
    expect(systems).toMatch(/Module->SetEnabledState\(bOn \? ENodeEnabledState::Enabled : ENodeEnabledState::Disabled, false\);\s*Module->MarkNodeRequiresSynchronization\(/u);
    expect(systems).toContain('TEXT("MODULE_NOT_FOUND")');
    expect(code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersModuleInfo.cpp')).toContain('ModuleObj->SetBoolField(TEXT("enabled"), Module->IsNodeEnabled());');
  });

  // Every placed effect fired once at level start, whatever cue it was placed for.
  it('a Niagara effect can be placed switched off', () => {
    const spawn = code('Effect', 'McpAutomationBridge_EffectHandlersNiagaraSpawn.cpp');
    // SetAutoActivate is refused after registration (it only logs), so the flag is written directly.
    expect(spawn).toMatch(/GetJsonBoolField\(Context\.Payload, TEXT\("autoActivate"\), true\);\s*NiagaraComponent->bAutoActivate = bAutoActivate;/u);
    expect(spawn).not.toContain('SetAutoActivate(');
    // Deactivate only winds a system down, and an editor world never ticks it to the end.
    expect(spawn).toMatch(/if \(bAutoActivate\)\s*\{\s*NiagaraComponent->Activate\(true\);\s*\}\s*else\s*\{\s*NiagaraComponent->DeactivateImmediate\(\);/u);
  });

  // An activate key restarted a finished one-shot every frame of its section: a spark burst fired again and again.
  it('a particle track can fire a one-shot burst once with a trigger key', () => {
    const tracks = code('Sequence', 'Cinematics', 'McpAutomationBridge_SequenceCinematicsTracks.cpp');
    expect(tracks).toMatch(/KeyName == TEXT\("trigger"\) \? EParticleKey::Trigger/u);
    const handler = tracks.slice(tracks.indexOf('bool HandleAddParticleTrack'));
    expect(handler.indexOf('particleKey must be activate, deactivate or trigger')).toBeLessThan(handler.indexOf('CreateBoundSection('));
  });

  // A module added, wired or removed ran the old scripts and listed no inputs until something else compiled.
  it('a Niagara graph edit compiles the system before saving it', () => {
    const save = code('NiagaraGraph', 'McpAutomationBridge_NiagaraGraphHandlersPrivate.h');
    expect(save).toMatch(/inline void SaveNiagaraGraphEdit\([^)]*\)\s*\{\s*System->RequestCompile\(false\);\s*System->MarkPackageDirty\(\);/u);
  });

  // create_niagara_ribbon and create_particle_trail authored LocationBasedRibbon, which spawns only on another emitter's location events.
  it('a default ribbon or trail effect spawns on its own', () => {
    const source = code('Effect', 'McpAutomationBridge_EffectHandlersNiagaraAuthoring.cpp');

    expect(source).toContain('TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate")');
    expect(source, 'at a trail rate, not the module\'s 1 a second').toMatch(/Rate->SetNumberField\(TEXT\("parameterValue"\), 60\.0\);[\s\S]*?SetModuleInputValue\(&System, EmitterName, TEXT\("SpawnRate\.SpawnRate"\), Rate,/u);
    expect(source).toContain('TEXT("InitializeParticle.Ribbon Width Mode"), MakeShared<FJsonValueString>(TEXT("Direct Set"))');
    expect(source, 'and keeps trailing past the template\'s single 5 s loop').toContain('TEXT("EmitterState.Loop Behavior"), MakeShared<FJsonValueString>(TEXT("Infinite"))');
    expect(source).toMatch(/if \(Template && !bExplicitTemplate && TemplatePath\.Contains\(TEXT\("LocationBasedRibbon"\)\)\)\s*\{\s*OutDetails->SetBoolField\(TEXT\("spawnRateAdded"\), MakeRibbonStandalone\(\*System, EmitterName\)\);/u);
  });

  // Niagara objects made without RF_Transactional skipped undo and made every later edit's stack check warn.
  it('Niagara systems, emitters and renderers the tools make are transactional, and older systems are repaired on load', () => {
    expect(code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersSystems.cpp').split('RF_Public | RF_Standalone | RF_Transactional')).toHaveLength(3);
    expect(code('Effect', 'McpAutomationBridge_EffectHandlersNiagaraAuthoring.cpp')).toContain('NewObject<UNiagaraSystem>(Package, FName(*Name), RF_Public | RF_Standalone | RF_Transactional)');
    expect(code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersRenderers.cpp')).toContain('NewObject<TRenderer>(Target.Emitter, NAME_None, RF_Transactional)');
    expect(code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersContext.cpp')).toMatch(/System->SetFlags\(RF_Transactional\);\s*return System;/u);
  });

  // Every effect a creator authored also dropped a preview actor at the world origin of whatever level was open.
  it('an authored effect is placed in the level only when location asks for it', () => {
    const source = code('Effect', 'McpAutomationBridge_EffectHandlersProceduralEffects.cpp');

    expect(source).toMatch(/if \(!Context\.Payload->HasField\(TEXT\("location"\)\)\)\s*\{\s*Details->SetStringField\(TEXT\("systemPath"\), AuthoredSystemPath\);\s*Details->SetBoolField\(TEXT\("placed"\), false\);/u);
    expect(source.indexOf('HasField(TEXT("location"))'), 'checked before the placing call').toBeLessThan(source.lastIndexOf('return CreateNiagaraEffectFromPayload(Context, EffectName, AuthoredSystemPath, Details);'));
  });

  // The editor sets a missing usage flag only outside PIE and never saves it, so a renderer material drew as the default material.
  it('a sprite or ribbon renderer material gets its Niagara usage flag, set and saved', () => {
    const source = code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersRenderers.cpp');

    expect(source).toContain('EnsureNiagaraUsage(Material, bSprite ? TEXT("bUsedWithNiagaraSprites") : TEXT("bUsedWithNiagaraRibbons"));');
    expect(source).toMatch(/Flag->SetPropertyValue_InContainer\(Base, true\);\s*Base->PostEditChange\(\);\s*McpSafeOperations::McpSafeAssetSave\(Base\);/u);
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

// One session's level load dropped another session's unsaved level edits: FEditorFileUtils::LoadMap never asks about
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

describe('graph batches, PIE spawns and trigger volumes', () => {
  it('a build_graph pre-check knows the custom events earlier create_node steps make', () => {
    // A batch that created a custom event and then called it was refused with FUNCTION_NOT_FOUND before anything ran.
    const batch = code('BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersBatch.cpp');
    expect(batch).toMatch(/NodeType\.Equals\(TEXT\("CustomEvent"\), ESearchCase::IgnoreCase\)[\s\S]{0,200}?Declared\.Add\(/u);
    // ...and the event exists on the skeleton class by then, as add_event's does, so the call node resolves.
    const events = code('BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersCustomEvents.cpp');
    expect(events).toMatch(/EventNode->CustomFunctionName = FName\(\*EventName\);\s*Context\.FinalizeNode\(NodeCreator, EventNode, X, Y\);\s*FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified\(Context\.Blueprint\);/u);
  });

  it('a spawned static mesh actor is made movable before its mesh is set, so a spawn during PIE keeps the mesh', () => {
    const spawn = code('ControlActor', 'McpAutomationBridge_ControlActorSpawn.cpp');
    expect(spawn.indexOf('MeshComponent->SetMobility(EComponentMobility::Movable);'))
      .toBeLessThan(spawn.indexOf('MeshComponent->SetStaticMesh(ResolvedStaticMesh);'));
  });

  it('the placement check leaves trigger and pickup volumes out of what an actor intersects', () => {
    // A pawn standing in a trigger box read as intersecting the trigger's actor.
    const placement = code('ControlActor', 'McpAutomationBridge_ControlActorPlacementCheck.cpp');
    expect(placement).toContain('GetCollisionResponseToChannel(Channel) == ECR_Block');
    expect(placement).toContain('!McpBlocksSolids(A) || !McpBlocksSolids(B)');
  });
});

describe('receipts of input edits and deletes', () => {
  // map_action saved the mapping context but its receipt listed no change: the context was named only inside nested
  // verification objects, which changes[] never reads.
  it('every Enhanced Input edit names the asset it saved in changedAssets', () => {
    const mappings = code('Input', 'McpAutomationBridge_InputHandlersMappings.cpp');
    expect(mappings).toContain('const bool bSaved = SaveLoadedAssetThrottled(Context, true);');
    expect(mappings).toContain('SetInputChangedAsset(Result, Context, bSaved);');
    expect(mappings).toContain('SetInputChangedAsset(Result, bChanged ? Context : nullptr, bSaved);');
    expect(mappings).toMatch(/Changed\.Add\(MakeShared<FJsonValueString>\(Asset->GetOutermost\(\)->GetName\(\)\)\);[\s\S]{0,120}?Result->SetArrayField\(TEXT\("changedAssets"\), Changed\);/u);
  });

  it('a trigger or modifier already there, or an input asset that already exists, is reported as no change', () => {
    const triggers = code('Input', 'McpAutomationBridge_InputHandlersTriggerModifiers.cpp');
    expect(triggers).toContain('SetInputChangedAsset(Result, bAlreadyPresent ? nullptr : InAction, bSaved);');
    expect(triggers).toContain('SetInputChangedAsset(Result, bAlreadyPresent ? nullptr : ModifiedAsset, bSaved);');
    const creation = code('Input', 'McpAutomationBridge_InputHandlersCreation.cpp');
    expect(creation).toContain('SetInputChangedAsset(Result, bChanged ? ExistingAsset : nullptr, bSaved);');
    expect(creation).toContain('bUpgrade, bSaved);');
    // Enabling a context changes the running game, not the asset.
    expect(code('Input', 'McpAutomationBridge_InputHandlersRuntimeQueries.cpp')).toMatch(/AddVerification\(Result, Context\);\s*McpHandlerUtils::MarkNoAssetsChanged\(Result\);/u);
  });

  it('an asset delete and an environment delete name what they removed', () => {
    // asset.delete of an input action answered deletedCount 1 with an empty changes[].
    const mutation = code('AssetWorkflow', 'Operations', 'McpAutomationBridge_AssetWorkflowAssetMutation.cpp');
    expect(mutation).toContain('DeletedPaths.Add(FPackageName::ObjectPathToPackageName(SafePath));');
    expect(mutation).toContain('Resp->SetArrayField(TEXT("deleted"), DeletedArray);');
    expect(code('Environment', 'McpAutomationBridge_EnvironmentHandlersBuildDeletion.cpp'))
      .toContain('McpAddStringArrayField(Context.Resp, TEXT("deleted"), DeletedTargets);');
  });

  // A folder move and a legacy input mapping did their work while their receipts listed no change: the
  // reply named the folders and the ini only in fields changes[] never reads.
  it('a folder move names both folders in modifiedPaths, once something moved', () => {
    expect(code('AssetWorkflow', 'Rename', 'McpAutomationBridge_AssetFolderMove.cpp')).toMatch(
      /if \(bMoved\)\s*\{[\s\S]{0,200}?Folders\.Add\(MakeShared<FJsonValueString>\(SourceFolder\)\);\s*Folders\.Add\(MakeShared<FJsonValueString>\(DestinationFolder\)\);\s*Result->SetArrayField\(TEXT\("modifiedPaths"\), Folders\);/u);
  });

  it('a legacy input mapping names DefaultInput.ini when it changed it, not for a mapping already there', () => {
    expect(code('Input', 'McpAutomationBridge_InputHandlersLegacyMappings.cpp')).toMatch(
      /if \(bUpdatedDefaultConfig && \(bRemove \|\| BeforeCount == 0\)\)\s*\{[\s\S]{0,200}?Paths\.Add\(MakeShared<FJsonValueString>\(TEXT\("Config\/DefaultInput\.ini"\)\)\);\s*Result->SetArrayField\(TEXT\("modifiedPaths"\), Paths\);/u);
  });
});

describe('widget input on a minimized editor', () => {
  // A minimized editor paints no widget, so a widget graph's Delay never fires: a timed widget stayed up and
  // widget_list kept listing it with no word on why.
  it('widget_list and widget_click warn while the window showing Play In Editor is minimized', () => {
    const source = code('ControlEditor', 'McpAutomationBridge_ControlEditorWidgetInput.cpp');
    expect(source).toMatch(/if \(Viewport\) \{\s*Window = Viewport->GetWindow\(\);\s*\}\s*if \(Window\.IsValid\(\) && Window->IsWindowMinimized\(\)\) \{/u);
    expect(source).toContain('Resp->SetArrayField(TEXT("warnings"), Warnings);');
    expect(source).toMatch(/FString &Message\) \{\s*WarnWhenPieWindowMinimizedForMcp\(Resp\);\s*if \(InputType == TEXT\("widget_list"\)\)/u);
  });
});

describe('foliage edit receipts', () => {
  // Painting foliage answered changes: []: the reply named the foliage actor only as
  // foliageActorPath, which a read (get_foliage_instances) reports too.
  it('a foliage paint, add or remove names the foliage actor it changed', () => {
    expect(code('Foliage', 'McpAutomationBridge_FoliageHandlersPrivate.h')).toContain('Resp->SetArrayField(TEXT("affectedActors"), Changed);');
    expect(code('Foliage', 'McpAutomationBridge_FoliageHandlersPaint.cpp')).toContain('McpFoliageHandlers::SetFoliageActorChanged(Resp, IFA);');
    expect(code('Foliage', 'McpAutomationBridge_FoliageHandlersInstances.cpp')).toContain('McpFoliageHandlers::SetFoliageActorChanged(Resp, IFA);');
    expect(code('Foliage', 'McpAutomationBridge_FoliageHandlersQueries.cpp')).toMatch(/if \(RemovedCount > 0\) \{\s*McpFoliageHandlers::SetFoliageActorChanged\(Resp, IFA\);\s*\}/u);
    expect(code('Foliage', 'McpAutomationBridge_FoliageHandlersGetInstances.cpp')).not.toContain('SetFoliageActorChanged');
  });
});

describe('Blueprint create with an unresolved parent class', () => {
  // A camera shake asked for under the wrong module (/Script/GameplayCameras.LegacyCameraShake) came out an Actor
  // Blueprint answering "Blueprint created", with only a list of shake properties that did not exist on it.
  it('a create whose named parent class resolves to nothing is refused and names classes with that name', () => {
    const parents = code('BlueprintCreation', 'McpAutomationBridge_BlueprintCreationHandlersParentClasses.cpp');
    expect(parents).toMatch(/if \(!ResolvedParent && !Context\.ParentClassSpec\.IsEmpty\(\)\) \{\s*const FString Candidates = ClassesNamedLikeForMcp\(Context\.ParentClassSpec\);[\s\S]{0,600}?return nullptr;\s*\}/u);
    expect(code('BlueprintCreation', 'McpAutomationBridge_BlueprintCreationHandlersAssets.cpp')).toMatch(
      /UFactory \*Factory = CreateBlueprintFactory\(Context, ParentError\);\s*if \(!Factory\) \{[\s\S]{0,200}?TEXT\("CLASS_NOT_FOUND"\)\);\s*return true;\s*\}/u);
  });
});

describe('mesh view of a placed actor', () => {
  // inspect_object objectKind mesh declares actorName, yet refused a placed StaticMeshActor as "not a static or
  // skeletal mesh"; reading a placed prop's mesh took a second lookup for its path.
  it('the mesh view follows a placed actor or component to the one mesh it draws, and names each when there are several', () => {
    const source = code('Environment', 'Inspection', 'McpAutomationBridge_EnvironmentHandlersInspectObject.cpp');
    expect(source).toContain('UObject *Mesh = McpMeshDrawnBy(TargetObject, Several);');
    expect(source).toMatch(/if \(!Several\.IsEmpty\(\)\)\s*\{[\s\S]{0,300}?TEXT\("AMBIGUOUS_TARGET"\)\);\s*return true;\s*\}/u);
    expect(source).toContain('Resp->SetStringField(TEXT("meshOf"), MeshOf);');
    expect(source).toMatch(/#if ENGINE_MINOR_VERSION >= 1\s*EachMesh = Skeletal->GetSkeletalMeshAsset\(\);\s*#else\s*EachMesh = Skeletal->SkeletalMesh;\s*#endif/u);
  });
});

describe('multi-property reads of dotted paths', () => {
  // get_property with propertyNames TitleText.Text and SubtitleText.Text answered two rows both named "Text".
  it('a multi-property read echoes each dotted path as asked under propertyPath, keeping the resolved propertyName', () => {
    const source = code('Property', 'McpAutomationBridge_PropertyHandlersObjectGet.cpp');
    expect(source).toContain('Row->SetStringField(TEXT("propertyName"), Resolved);');
    expect(source).toMatch(/if \(!Wanted\.Equals\(Resolved, ESearchCase::CaseSensitive\)\) \{\s*Row->SetStringField\(TEXT\("propertyPath"\), Wanted\);\s*\}/u);
  });
});

describe('list reads struct members of an actor', () => {
  // list propertyNames reported Settings.BloomIntensity "missing" on a post process volume while get_property read it:
  // list only followed Component.Property, never a struct member of the actor itself.
  it('list falls back to the get_property resolver for struct members and deeper paths', () => {
    const source = code('ControlActor', 'List', 'McpAutomationBridge_ControlActorList.cpp');
    expect(source).toMatch(/if \(FProperty \*Nested = McpResolvePropertyPath\(Actor, Wanted, Container, Resolved, Error\)\) \{\s*const void \*NestedValue = Nested->ContainerPtrToValuePtr<void>\(Container\);\s*MCP_PROPERTY_EXPORT_TEXT\(Nested, Value, NestedValue, NestedValue, nullptr, PPF_None\);\s*Properties->SetStringField\(Wanted, Value\);/u);
  });
});

describe('multi-property writes on class defaults', () => {
  // set_property properties {CharMoveComp.bCanWalkOffLedges, CharMoveComp.PerchRadiusThreshold} on a Character Blueprint recompiled and
  // saved the Blueprint while its receipt listed no change: the batch kept only actor fields from each write.
  it('a multi-property write names the asset it saved and how many placed copies took the new default', () => {
    const source = code('Property', 'McpAutomationBridge_PropertyHandlersObjectSet.cpp');
    expect(source).toContain('TEXT("assetPath"), TEXT("materialRebuilt")}) {');
    expect(source).toContain('if (InstancesUpdated >= 0.0) Data->SetNumberField(TEXT("instancesUpdated"), InstancesUpdated);');
  });
});

describe('removing a widget subtree', () => {
  // remove_widget RootCanvas moved only the canvas out of the WidgetTree: its descendants kept their names (and the
  // class kept their variables), so adding the same names back was refused with a bare "Name is already in use."
  it('removing a widget takes its whole subtree out of the tree and refreshes the class', () => {
    const source = code('WidgetAuthoring', 'Support', 'McpAutomationBridge_WidgetAuthoringManipulation.cpp');
    expect(source).toMatch(/TArray<UWidget\*> Removed\{TargetWidget\};\s*UWidgetTree::GetChildWidgets\(TargetWidget, Removed\);/u);
    expect(source).toMatch(/for \(UWidget\* Widget : Removed\)\s*\{\s*Widget->Rename\(nullptr, GetTransientPackage\(\), REN_DontCreateRedirectors \| REN_DoNotDirty\);\s*Widget->MarkAsGarbage\(\);\s*\}/u);
    expect(source).toMatch(/MarkWidgetBlueprintModifiedAndSave\(WidgetBP\);\s*WidgetAuthoringHelpers::RefreshWidgetBlueprintClass\(WidgetBP\);\s*ResultJson->SetBoolField\(TEXT\("success"\), true\);\s*ResultJson->SetStringField\(TEXT\("widgetPath"\), WidgetPath\);\s*ResultJson->SetStringField\(TEXT\("removedWidget"\)/u);
  });

  it('a name the Blueprint refuses for a new widget is named in the refusal', () => {
    const source = code('WidgetAuthoring', 'Support', 'McpAutomationBridge_WidgetAuthoringValidation.cpp');
    expect(source).toMatch(/FString::Printf\(TEXT\("'%s': %s"\), \*Name\.ToString\(\), \*INameValidatorInterface::GetErrorString\(Name\.ToString\(\), Result\)\);/u);
  });
});

describe('simulated keys and focused PIE widgets', () => {
  // simulate_input key_tap Enter in PIE went straight to the viewport client, so a focused menu button never saw it
  // (handledByPIE false, handledBySlate false) while a real Enter pressed it.
  it('a key goes through the Slate focus path while a widget inside the PIE viewport holds focus', () => {
    const widgetInput = code('ControlEditor', 'McpAutomationBridge_ControlEditorWidgetInput.cpp');
    expect(widgetInput).toMatch(/if \(!ViewportWidget\.IsValid\(\) \|\| !SlateApp\.HasUserFocusedDescendants\(ViewportWidget\.ToSharedRef\(\), 0\)\) \{\s*return false;\s*\}/u);
    expect(widgetInput).toContain('bOutHandled = InputEvent == IE_Released ? SlateApp.ProcessKeyUpEvent(KeyEvent) : SlateApp.ProcessKeyDownEvent(KeyEvent);');
    const routing = code('ControlEditor', 'McpAutomationBridge_ControlEditorInputRouting.cpp');
    expect(routing).toMatch(/bRoutedToPIE = RouteKeyToFocusedPieWidgetForMcp\(InputKey, InputEvent, bHandledBySlate\) \|\|\s*RouteKeyToPIEForMcp\(InputKey, InputEvent, bHandledByPIE\);/u);
  });
});

describe('Blueprints that depend on a changed Widget Blueprint', () => {
  // After a widget was removed and added back, a game mode focusing it (Set Input Mode UI Only) failed every play
  // with "Attempted to access missing property 'none'" while it read up to date: the engine only relinks a
  // dependent by name, and the removal left it pointing at nothing.
  it('a widget class refresh compiles every Blueprint that depends on it in full', () => {
    const source = code('WidgetAuthoring', 'Support', 'McpAutomationBridge_WidgetAuthoringLoading.cpp');
    expect(source).toMatch(/const bool bCompiled = McpSafeCompileBlueprint\(WidgetBP\);\s*TArray<UBlueprint\*> Dependents;\s*FBlueprintEditorUtils::GetDependentBlueprints\(WidgetBP, Dependents\);\s*for \(UBlueprint\* Dependent : Dependents\)\s*\{\s*McpSafeCompileBlueprint\(Dependent\);\s*\}\s*return bCompiled;/u);
  });
});

describe('connect_pins on pins added on demand', () => {
  // A build_graph step connecting a Sequence's then_2 stopped the batch with PIN_NOT_FOUND ("pins: execute, then_0,
  // then_1"): the editor adds that pin on demand, the tool never did.
  it('connect_pins grows a node that adds pins on demand to the pin it names, and takes back pins added in vain', () => {
    const growth = code('BlueprintGraph', 'PinMutations', 'McpAutomationBridge_BlueprintGraphPinGrowth.cpp');
    expect(growth).toMatch(/IK2Node_AddPinInterface\* Growable = Cast<IK2Node_AddPinInterface>\(Node\)/u);
    expect(growth).toMatch(/Growable->AddInputPin\(\);\s*Pin = Context\.FindPin\(Node, PinName\);/u);
    expect(growth).toMatch(/if \(!Pin && !Before\.Contains\(Extra\)\) Growable->RemoveInputPin\(Extra\);/u);
    const source = code('BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersPinMutations.cpp');
    expect(source).toContain('UEdGraphPin* FromPin = FindOrGrowPin(Context, FromNode, FromPinName);');
    expect(source).toContain('UEdGraphPin* ToPin = FindOrGrowPin(Context, ToNode, ToPinName);');
  });

  // nodeType GetAllActorsOfClass, listed by create_node, answered NODE_TYPE_NOT_FOUND: it was aliased to a K2Node
  // class Unreal does not have, while the node is a plain call of the GameplayStatics function.
  it('GetAllActorsOfClass and GetActorOfClass create function calls', () => {
    const catalog = code('BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersNodeCatalog.cpp');
    expect(catalog).toMatch(/\{TEXT\("GetAllActorsOfClass"\),\s*MakeTuple\(TEXT\("UGameplayStatics"\), TEXT\("GetAllActorsOfClass"\)\)\}/u);
    expect(catalog).toMatch(/\{TEXT\("GetActorOfClass"\),\s*MakeTuple\(TEXT\("UGameplayStatics"\), TEXT\("GetActorOfClass"\)\)\}/u);
    expect(catalog).not.toContain('K2Node_GetAllActorsOfClass');
  });

  // A Switch on Int made over MCP had only its Default pin: a link to case 17 answered PIN_NOT_FOUND.
  it('a Switch on Int grows to the case a link names, and takes back cases added in vain', () => {
    const growth = code('BlueprintGraph', 'PinMutations', 'McpAutomationBridge_BlueprintGraphPinGrowth.cpp');
    expect(growth).toMatch(/Switch->AddPinToSwitchNode\(\);\s*Pin = Context\.FindPin\(Node, PinName\);/u);
    expect(growth).toMatch(/if \(!Pin && !Before\.Contains\(Extra\)\) Switch->RemovePinFromSwitchNode\(Extra\);/u);
  });
});

describe('a spawnable added from a class', () => {
  // add_spawnable stored the class default object as the template; a recompile of the Blueprint replaced it and
  // the binding spawned nothing ("does not have a valid object template").
  it('gets a template the sequence owns', () => {
    const source = code('Sequence', 'McpAutomationBridge_SequenceHandlersSpawnables.cpp');
    expect(source).toContain('static_cast<UMovieSceneSequence *>(LevelSeq)->CreateSpawnable(ResolvedClass)');
    expect(source).not.toContain('GetDefaultObject()');
  });
});

describe('sequence duplicates and the render queue', () => {
  // A duplicated sequence stayed in memory only, while a created one was saved: an editor exit lost the copies.
  it('a duplicated sequence is saved like a created one', () => {
    const source = code('Sequence', 'McpAutomationBridge_SequenceHandlersAssetLibrary.cpp');
    expect(source).toMatch(/if \(DuplicatedSeq\) \{[^}]*McpSafeAssetSave\(DuplicatedSeq\);/u);
  });

  // A render whose game loaded another level hung past its deadline and kept the queue busy until the editor
  // closed: once the cancel wait runs out, its Play In Editor session is ended.
  it('a render still running after the cancel wait has its Play In Editor session ended', () => {
    const source = code('Sequence', 'MovieRender', 'McpAutomationBridge_SequenceMovieRenderCompletion.cpp');
    expect(source).toMatch(/State->bCancellationDeadlineExpired && Cast<UMoviePipelinePIEExecutor>\(Executor\) &&\s*GEditor && GEditor->PlayWorld\)\s*GEditor->RequestEndPlayMap\(\);/u);
  });

  // The queue keeps every job (a finished one renders again with the next unrestricted start) and refuses
  // new ones past its limit; nothing could take a job out, so a long session hit the limit for good.
  it('render jobs can be removed, one or all, but not mid-render', () => {
    const source = code('Sequence', 'MovieRender', 'McpAutomationBridge_SequenceMovieRenderJobCreation.cpp');
    expect(source).toContain('Queue->DeleteAllJobs();');
    expect(source).toContain('Queue->DeleteJob(Job);');
    expect(source).toMatch(/QueueSubsystem->GetActiveExecutor\(\) \|\| QueueSubsystem->IsRendering\(\)\)\s*return SendError/u);
    const routing = code('Sequence', 'MovieRender', 'McpAutomationBridge_SequenceMovieRenderRouting.cpp');
    expect(routing).toContain('{TEXT("remove_render_job"), &HandleRemoveRenderJob}');
  });

  // "MRQ render pass configured." named neither the pass nor the job, so a caller could not tell the ui pass landed.
  it('add_render_pass names the passes it added and the job', () => {
    const source = code('Sequence', 'MovieRender', 'McpAutomationBridge_SequenceMovieRenderPasses.cpp');
    expect(source).toContain('TEXT("Render pass %s added to %s."),');
    expect(source).toContain('*FString::Join(Passes, TEXT(", ")), *Job->JobName)');
  });

  // create took path as a folder only, while every other sequence action reads it as the sequence's own path: a
  // full path with no name answered "sequence_create requires name".
  it('create reads a path that is no folder as the new sequence itself', () => {
    const source = code('Sequence', 'McpAutomationBridge_SequenceHandlersAssetCreation.cpp');
    expect(source).toMatch(/if \(!Probe\.IsEmpty\(\) && !DoesAssetDirectoryExistOnDisk\(Probe\)\) \{\s*Name = FPackageName::GetShortName\(Probe\);\s*Folder = FPackageName::GetLongPackagePath\(Probe\);/u);
    expect(source.indexOf('DoesAssetDirectoryExistOnDisk(Probe)')).toBeLessThan(source.indexOf('sequence_create requires name'));
  });
});

describe('tapped keys while the game is paused', () => {
  // key_tap Enter reached a focused menu button that never clicked: the release waited on game time, which a pause
  // menu stops, so the key stayed down for the 600 s grace.
  it('a tapped key is released on wall time while the game is paused', () => {
    const source = code('ControlEditor', 'McpAutomationBridge_ControlEditorInput.cpp');
    expect(source).toContain('const bool bGameClock = bGameTime && !bWorldGone && !Live->IsPaused();');
    expect(source).toContain('(bGameClock ? Live->GetTimeSeconds() < EndGameTime : FPlatformTime::Seconds() < EndHoldWall)');
  });
});

describe('actors moved from code', () => {
  // After set_transform moved and scaled a ground plane, foliage painted past its old end found no ground and
  // raycast_world missed it there: in the editor world a body moved from code kept its collision queries on the old
  // spot, so only where the old and new footprints overlapped did a trace hit.
  it('a move made from code ends as the editor ends its own moves, with its bodies rebuilt where it stands', () => {
    const helper = code('..', 'Foundation', 'BridgeHelpers', 'Actors', 'McpAutomationBridgeHelpersActorMove.h');
    expect(helper).toMatch(/if \(!World \|\| World->IsGameWorld\(\)\)\s*return;\s*Actor->PostEditMove\(true\);/u);
    expect(helper).toMatch(/if \(Primitive->IsPhysicsStateCreated\(\)\)\s*Primitive->RecreatePhysicsState\(\);/u);
    expect(code('ControlActor', 'McpAutomationBridge_ControlActorTransform.cpp'))
      .toMatch(/Found->SetActorScale3D\(Scale\);\s*McpFinishEditorMove\(Found\);/u);
    expect(code('ControlActor', 'McpAutomationBridge_ControlActorSnapshots.cpp'))
      .toMatch(/Found->SetActorTransform\(SavedTransform\);\s*McpFinishEditorMove\(Found\);/u);
    expect(code('ControlActor', 'Placement', 'McpAutomationBridge_CoplanarFix.cpp'))
      .toMatch(/ETeleportType::TeleportPhysics\);\s*McpFinishEditorMove\(Actor\);/u);
    const property = code('Property', 'McpAutomationBridge_PropertyHandlersActorAccess.cpp');
    expect(property).toMatch(/Actor->SetActorLocation\(NewLoc\);\s*McpFinishEditorMove\(Actor\);/u);
    expect(property).toMatch(/Actor->SetActorRotation\(NewRot\);\s*McpFinishEditorMove\(Actor\);/u);
    expect(property).toMatch(/Actor->SetActorScale3D\(NewScale\);\s*McpFinishEditorMove\(Actor\);/u);
  });
});

describe('material and widget authoring', () => {
  // A material batch wiring a Custom node's colour and alpha ("$panel.RGB", "$panel.A") stopped with "Source output
  // pin 'RGB' not found": a Custom source only matched its extra outputs by name, never the default output's
  // aliases or channel letters, which every other node takes.
  it('a Custom node source resolves names, aliases and channel letters like any other node', () => {
    const source = code('MaterialAuthoring', 'Connections', 'McpAutomationBridge_MaterialAuthoringHandlersConnectNodes.cpp');
    expect(source).not.toContain('CustomSource->AdditionalOutputs');
    expect(source).toMatch(/if \(SourceOutputIndex == INDEX_NONE && Outputs\.Num\(\) > 0 && ParseChannelMask\(SourcePin, Channels\)\)/u);
  });

  // add_widget_tree took a texture but no material, so a panel drawn by a UI material needed a second call to set
  // its brush by reflection.
  it('a widget tree node takes a material as an Image or Border brush', () => {
    const tree = code('WidgetAuthoring', 'Templates', 'McpAutomationBridge_WidgetAuthoringTreeBuild.cpp');
    expect(tree).toContain('TEXT("shadowColor"), TEXT("material")};');
    expect(tree).toMatch(/if \(!Material\.IsEmpty\(\) && Type != TEXT\("Image"\) && Type != TEXT\("Border"\)\)/u);
    expect(tree).toContain('if (!Material.IsEmpty() && !McpLoadSpecMaterial(Material))');
    const props = code('WidgetAuthoring', 'Templates', 'McpAutomationBridge_WidgetAuthoringSpecProps.cpp');
    expect(props).toMatch(/Brush\.SetResourceObject\(Material\);\s*Brush\.DrawAs = ESlateBrushDrawType::Image;/u);
    expect(props).toContain('if (UBorder* Border = Cast<UBorder>(Widget)) { Border->SetBrush(Brush); Border->SetBrushColor(FLinearColor::White); }');
    expect(props).toContain('if (UImage* MaterialImage = Cast<UImage>(Widget)) { MaterialImage->SetBrush(Brush); }');
  });

  // create_material with materialDomain "UserInterface" (the Details panel says "User Interface") was refused,
  // and the refused call had already made the material, so the retry with "UI" hit "already exists".
  it('create_material checks its enum settings before it makes anything, and takes the Details panel labels', () => {
    const create = code('MaterialAuthoring', 'Creation', 'McpAutomationBridge_MaterialAuthoringHandlersCreateMaterial.cpp');
    const check = create.indexOf('if (!ApplyMaterialEnumFields(nullptr, Payload, EnumError))');
    expect(check).toBeGreaterThan(-1);
    expect(check).toBeLessThan(create.indexOf('CreatePackage('));
    expect(check).toBeLessThan(create.indexOf('PrepareNewMaterialAsset('));
    const setters = code('MaterialAuthoring', 'Properties', 'McpAutomationBridge_MaterialAuthoringHandlersSetMaterialEnum.cpp');
    expect(setters.match(/if \(Material\) Material->/gu)).toHaveLength(3);
    const parse = code('MaterialAuthoring', 'Properties', 'McpAutomationBridge_MaterialAuthoringEnumParsing.cpp');
    expect(parse).toContain('Enum->GetDisplayNameTextByIndex(Index).ToString().Replace(TEXT(" "), TEXT(""))');
    expect(parse).toContain('Wanted.Equals(Label, ESearchCase::IgnoreCase)');
  });

  // A widget animation took one call per key.
  it('add_animation_keyframe writes a keys batch, checking every key before writing any', () => {
    const handler = code('WidgetAuthoring', 'Animation', 'McpAutomationBridge_WidgetAuthoringAnimationKeyframe.cpp');
    expect(handler).toContain('Payload->TryGetArrayField(TEXT("keys"), KeyList)');
    expect(handler).toContain('Merged->RemoveField(TEXT("keys"));');
    expect(handler).toContain('FString(Pair.Key.Len(), *Pair.Key)');
    const checked = handler.indexOf(': KeyRefusal(Keys[Index]);');
    expect(checked).toBeGreaterThan(-1);
    expect(checked).toBeLessThan(handler.indexOf('McpAuthorWidgetAnimationKey('));
    expect(handler).toContain('ResultJson->SetNumberField(TEXT("keysAdded"), Keys.Num());');
    const keys = code('WidgetAuthoring', 'Support', 'McpAutomationBridge_WidgetAuthoringAnimationKeys.h');
    expect(keys).toContain('FString KeyValueError(const FString& Kind, const FString& TrackType, const TSharedPtr<FJsonValue>& Value);');
  });

  // Wiring a new animation into PlayAnimation stopped with "Variable not found": the animation was only a property
  // of the generated class after the next compile.
  it('create_widget_animation compiles the Widget Blueprint so a graph can read the animation at once', () => {
    const core = code('WidgetAuthoring', 'Animation', 'McpAutomationBridge_WidgetAuthoringAnimationCore.cpp');
    expect(core).toMatch(/RegisterAnimationGuid\(WidgetBP, NewAnim\);\s*FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified\(WidgetBP\);\s*RefreshWidgetBlueprintClass\(WidgetBP\);\s*McpSafeAssetSave\(WidgetBP\);/u);
  });

  // Re-keying a frame answered "4 keys in the track" for a 3-key track: the new key went in beside the old one on
  // the same frame.
  it('a widget animation key on an occupied frame replaces the key there', () => {
    const keys = code('WidgetAuthoring', 'Support', 'McpAutomationBridge_WidgetAuthoringAnimationKeysInternal.h');
    expect(keys).toMatch(/for \(int32 Existing = Channel->GetData\(\)\.FindKey\(Frame\); Existing != INDEX_NONE; Existing = Channel->GetData\(\)\.FindKey\(Frame\)\)\s*\{\s*Channel->GetData\(\)\.RemoveKey\(Existing\);\s*\}\s*const float FloatValue/u);
  });
});

describe('Make and Break nodes of structs with native functions', () => {
  // Breaking a velocity with create_node BreakStruct (structPath Vector) compiled with "The structure cannot be
  // broken using generic 'break' node Break Vector": the editor breaks such a struct with its native function.
  it('a Make or Break of a struct with a native make or break builds that function node', () => {
    const dynamic = code('BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersNodeCreationDynamic.cpp');
    expect(dynamic).toContain('NodeClass->IsChildOf(UK2Node_MakeStruct::StaticClass()) ? TEXT("HasNativeMake") : TEXT("HasNativeBreak")');
    expect(dynamic).toMatch(/FindObject<UFunction>\(nullptr, \*Native\)\)\s*\{\s*FGraphNodeCreator<UK2Node_CallFunction> NativeCreator\(\*Context\.TargetGraph\);/u);
    expect(dynamic.indexOf('HasNativeBreak')).toBeLessThan(dynamic.indexOf('NewObject<UEdGraphNode>(Context.TargetGraph, NodeClass)'));
  });
});

describe('asset queries point at what they found', () => {
  // Searching for a sound found nothing although a PlaySound2D node played it: an asset picked on a pin is
  // DefaultObject, which the scan never read.
  it('find_text matches the asset picked on a graph pin by its path', () => {
    const scan = code('AssetQuery', 'McpAutomationBridge_AssetQueryFindText.cpp');
    expect(scan).toContain('Reference(Asset, Where, Pin->PinName.ToString(), Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : FString());');
    expect(scan).toMatch(/void Reference\(const FString& Asset, const FString& Where, const FString& Field, const FString& Path\)\s*\{\s*if \(!Path\.IsEmpty\(\) && !Path\.StartsWith\(TEXT\("\/Script\/"\)\)/u);
  });

  // "Which Blueprints call StartCameraFade" found nothing: node titles were never searched.
  it('find_text matches a graph node by its title', () => {
    expect(code('AssetQuery', 'McpAutomationBridge_AssetQueryFindText.cpp'))
      .toContain('Hit(Asset, Where, TEXT("node"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());');
  });

  // lookup=graph on a MetaSound answered "Asset does not have a graph structure".
  it('analyze_graph names the action that reads a MetaSound, Niagara or Behavior Tree graph', () => {
    const report = code('AssetWorkflow', 'Analysis', 'McpAutomationBridge_AssetWorkflowGraphReport.cpp');
    for (const reader of [
      '{TEXT("MetaSoundSource"), TEXT("manage_audio"), TEXT("get_metasound_graph")}',
      '{TEXT("NiagaraSystem"), TEXT("manage_effect"), TEXT("get_niagara_info")}',
      '{TEXT("BehaviorTree"), TEXT("manage_ai"), TEXT("get_tree")}',
    ]) {
      expect(report).toContain(reader);
    }
    expect(report).toContain('Result->SetObjectField(TEXT("nextCall"), Next);');
    expect(report.indexOf('Readers[]')).toBeLessThan(report.indexOf('TEXT("Asset does not have a graph structure")'));
  });
});

describe('function overrides', () => {
  // A widget's OnKeyDown added with add_function compiled as a new function nothing called: it took no
  // signature from the parent. The graph was also made twice and the second entry node deleted afterwards.
  it('add_function makes a parent function override with its signature, in one graph creation', () => {
    const add = code('Blueprint', 'Functions', 'McpAutomationBridge_BlueprintHandlersAddFunction.cpp');
    expect(add).toMatch(/AddFunctionGraph<UClass>\(Blueprint, NewGraph,\s*OverrideClass == nullptr, OverrideClass\);/u);
    expect(add).not.toContain('CreateFunctionGraph<UFunction>');
    const resolve = code('Blueprint', 'Functions', 'McpAutomationBridge_BlueprintHandlersAddFunctionResult.cpp');
    expect(resolve).toContain('Owner == Blueprint->GeneratedClass || !UEdGraphSchema_K2::CanKismetOverrideFunction(Parent)');
    expect(resolve).toContain('if (UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Parent))');
    expect(resolve).toContain('Resp->SetStringField(TEXT("overrides"), OverrideClass->GetName() + TEXT("::") + FuncName);');
  });
});

describe('start minimized', () => {
  // The editor came up already minimized, so the start-minimized ticker never saw it on screen, kept waiting, and
  // put away the first restore made over the bridge minutes later.
  it('the start-minimized hold begins the first time the window is on screen, minimized or not', () => {
    const lifecycle = readFileSync(join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Core', 'Subsystem',
      'McpAutomationBridgeSubsystemLifecycle.cpp'), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
    expect(lifecycle).toMatch(/Root->IsVisible\(\)\)\s*\{\s*if \(!Root->IsWindowMinimized\(\)\)\s*\{\s*MinimizeWindowForMcp\(Root\.ToSharedRef\(\)\);\s*\}\s*if \(\*HoldUntil == 0\.0\)/u);
  });
});

describe('save game slots', () => {
  // The slot name becomes the .sav file name, so a separator or ".." would reach a file outside SaveGames.
  it('a slot name cannot leave Saved/SaveGames', () => {
    const saves = code('SystemControl', 'McpAutomationBridge_SystemControlHandlersSaveGames.cpp');
    for (const refused of ['TEXT("..")', 'TEXT("/")', 'TEXT("\\\\")', 'TEXT(":")']) {
      expect(saves).toContain(`OutSlot.Contains(${refused})`);
    }
    expect(saves.match(/ReadSaveSlot\(Payload, Slot, User, Error\)/gu)).toHaveLength(2);
  });

  // A slot is written only after every requested property resolved and converted, and the reply is read back from disk.
  it('edit_save_game writes after the properties apply and answers what it reloads', () => {
    const saves = code('SystemControl', 'McpAutomationBridge_SystemControlHandlersSaveGames.cpp');
    expect(saves.indexOf('ApplySavedProperties(Self, RequestId, RequestingSocket, Save, *Props)'))
      .toBeLessThan(saves.indexOf('UGameplayStatics::SaveGameToSlot(Save, Slot, User)'));
    expect(saves).toContain('USaveGame* Reloaded = bSaved ? UGameplayStatics::LoadGameFromSlot(Slot, User) : nullptr;');
    expect(saves).toContain('NewClass->HasAnyClassFlags(CLASS_Abstract)');
  });
});

describe('search scope and mesh-particle materials', () => {
  // find_text narrowed to /Game/UI still read every actor of the open level elsewhere: 148 hits, 3 of them asked for.
  it('find_text reads the open level only under the searched paths unless includeLevel says otherwise', () => {
    expect(code('AssetQuery', 'McpAutomationBridge_AssetQueryFindText.cpp'))
      .toContain('if (World && FindTextIncludesLevel(Payload, Filter.PackagePaths, World))');
    const scope = code('AssetQuery', 'McpAutomationBridge_AssetQueryHandlersPrivate.h');
    expect(scope).toContain('if (Payload->TryGetBoolField(TEXT("includeLevel"), bInclude))');
    expect(scope).toContain('Level.StartsWith(Path.ToString() + TEXT("/"))');
  });

  // Sprite and ribbon materials got their Niagara usage flag saved; a mesh renderer's mesh materials did not.
  it('add_mesh_renderer_module flags the mesh materials for mesh particles', () => {
    expect(code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersRenderers.cpp'))
      .toContain('EnsureNiagaraUsage(Slot.MaterialInterface, TEXT("bUsedWithNiagaraMeshParticles"));');
  });
});

describe('material batch steps', () => {
  // {"edit": "add_material_node", "nodeKind": "world_position"} failed inside build_material_graph with "Missing 'nodeType'":
  // the gateway resolves nodeKind through the add_material_node fold, and batch steps never pass the gateway.
  it('a step resolves nodeKind with the add_material_node fold member map', async () => {
    const { MANAGE_ASSET_FOLDS } = await import('../../../src/tools/catalog/capabilities/records/folds/manage-asset.folds.js');
    const fold = MANAGE_ASSET_FOLDS.find((spec) => spec.primary === 'add_material_node');
    const members = fold && !Array.isArray(fold.members) ? fold.members : {};
    const resolver = readFileSync(join(DOMAINS, 'MaterialAuthoring', 'McpAutomationBridge_MaterialAuthoringBatchSteps.h'), 'utf8');
    const named = Object.fromEntries([...resolver.matchAll(/Kind == TEXT\("(\w+)"\)\)\s*\{\s*return TEXT\("(\w+)"\);/gu)].map((m) => [m[1], m[2]]));
    const resolve = (kind: string): string => (kind === 'node' ? 'add_material_node' : kind === 'batch' ? 'build_material_graph' : named[kind] ?? `add_${kind}`);
    expect(resolver).toMatch(/if \(Kind == TEXT\("node"\)\)\s*\{\s*return Edit;/u);
    expect(resolver).toContain('return Kind == TEXT("batch") ? FString(TEXT("build_material_graph")) : TEXT("add_") + Kind;');
    expect(Object.keys(members).length).toBeGreaterThan(15);
    for (const [kind, action] of Object.entries(members)) {
      expect(resolve(kind)).toBe(action);
    }
    expect(code('MaterialAuthoring', 'McpAutomationBridge_MaterialAuthoringGraphBatch.cpp'))
      .toContain('!IsBatchableMaterialEdit(Edit = McpMaterialBatchStepEdit(Edit, *StepPtr))');
  });
});

describe('widget previews', () => {
  // A design-mode preview drew every widget, Collapsed ones too: a hidden controls card or NEW RECORD pill showed.
  it('preview_widget gives each widget the visibility the game creates it with', () => {
    const preview = code('WidgetAuthoring', 'Support', 'McpAutomationBridge_WidgetAuthoringPreview.cpp');
    // GetVisibility() reads the Slate widget, which design mode forces visible: the saved property is read instead.
    expect(preview).toContain('*VisibilityProperty->ContainerPtrToValuePtr<ESlateVisibility>(Child);');
    expect(preview).toContain('Cached->SetVisibility(UWidget::ConvertSerializedVisibilityToRuntime(Saved));');
    expect(preview).not.toContain('Child->GetVisibility()');
    expect(preview.indexOf('Widget->TakeWidget()')).toBeLessThan(preview.indexOf('ConvertSerializedVisibilityToRuntime'));
  });

  // The first preview after the editor started drew material brushes blank, unannounced: the editor compiles a
  // material's Slate shaders when it is first drawn, so the wait has to follow a draw and be followed by another.
  it('preview_widget waits for shaders on request and always reports them', () => {
    const preview = code('WidgetAuthoring', 'Support', 'McpAutomationBridge_WidgetAuthoringPreview.cpp');
    expect(preview.indexOf('McpDrawWidgetPreview(WidgetBP')).toBeLessThan(preview.indexOf('McpDeferForShaderCompile(Payload'));
    expect(preview).toContain('McpAddShaderCompileState(ResultJson, Payload);');
  });
});

// The meter is read once per editor frame: at about 5 fps two recordings read 20-30 dB apart that were not.
describe('sound measurement', () => {
  it('a Sound Wave also reports the level of its own samples', () => {
    const measure = code('Audio', 'McpAutomationBridge_AudioHandlersMeasure.cpp');
    expect(measure).toContain('Wave->GetImportedSoundWaveData(Pcm, SampleRate, Channels)');
    expect(measure).toContain('McpAddWaveLevels(Run->Wave.Get(), Data);');
    const properties = capabilityIndex().byId.get('manage_audio.play_sound')?.schemas.output.properties;
    expect(isRecord(properties) && 'wavePeakDb' in properties && 'waveRmsDb' in properties && 'waveSeconds' in properties).toBe(true);
  });

  // A batch that removed a voice's synth nodes, then named a node class the registry did not hold, saved the voice gutted.
  it('a MetaSound batch looks up every add_node class before any step runs', () => {
    const batch = code('AudioAuthoring', 'MetaSound', 'McpAutomationBridge_AudioAuthoringHandlersMetaSoundBatch.cpp');
    expect(batch.indexOf('ResolveMetaSoundAddNodeClass(*StepObj)')).toBeGreaterThan(-1);
    expect(batch.indexOf('ResolveMetaSoundAddNodeClass(*StepObj)')).toBeLessThan(batch.indexOf('HandleMetaSoundNodeActions(StepSubAction'));
    expect(code('AudioAuthoring', 'McpAutomationBridge_AudioAuthoringHandlersMetaSoundNodes.cpp'))
      .toContain('const FMcpMetaSoundNodeClassRequest Request = ResolveMetaSoundAddNodeClass(Params);');
  });
});

// Removing DefaultSceneRoot answered "removed from SCS" while the compile put it straight back with the mesh still under
// it, so a mesh meant to be the physics root never was.
describe('removing a Blueprint\'s root component hands its place to a child', () => {
  it('remove_scs_component promotes the removed node\'s children and reads the tree back', () => {
    const source = code('SCS', 'McpAutomationBridge_SCSHandlersRemoveComponent.cpp');
    expect(source).toContain('SCS->RemoveNodeAndPromoteChildren(NodeToRemove);');
    expect(source).toContain('TEXT("SCS_ROOT_REQUIRED")');
    expect(source).toContain('TEXT("SCS_REMOVE_REVERTED")');
    expect(source).not.toContain('SCS->RemoveNode(');
  });

  // Removing a Buoyancy component answered newRoot "WaterContact": actor components are SCS roots too.
  it('names a new root only when the scene root went, and only a scene component', () => {
    const source = code('SCS', 'McpAutomationBridge_SCSHandlersRemoveComponent.cpp');
    expect(source).toContain('Node->ComponentTemplate->IsA<USceneComponent>()');
    expect(source).toContain('const bool bWasRoot = IsSceneRootNode(NodeToRemove) && SCS->GetRootNodes().Contains(NodeToRemove);');
    expect(source).toContain('SCS->GetRootNodes().FindByPredicate(IsSceneRootNode)');
  });
});

// set_default changed the class default while every placed actor kept the old value under a success reply.
describe('a class default reaches the actors already placed', () => {
  it('set_default carries the new value to placed actors that still held the old default', () => {
    const source = code('Blueprint', 'Graph', 'McpAutomationBridge_BlueprintHandlersSetDefaultLiteral.cpp');
    expect(source).toContain('CDO->GetArchetypeInstances(Instances);');
    expect(source).toContain('Property->Identical(Property->ContainerPtrToValuePtr<void>(InstanceContainer), OldValue)');
    expect(source).toContain('Result->SetNumberField(TEXT("instancesUpdated"), InstancesUpdated);');
  });
});

// A BuoyancyComponent added to a Blueprint never asked for the begin-overlap a water body sends at level load, so every
// placed floater sank.
describe('a Blueprint given buoyancy floats where it is placed', () => {
  it('asks for the water\'s begin-overlap at level load', () => {
    expect(code('SCS', 'McpAutomationBridge_SCSHandlers.cpp')).toContain('CDO->bGenerateOverlapEventsDuringLevelStreaming = true;');
    expect(code('Blueprint', 'Components', 'McpAutomationBridge_BlueprintHandlersModifyScsFinalize.cpp'))
      .toContain('McpSCSHandlers::EnableLoadOverlapsForBuoyancy(LocalBP);');
  });
});

// A struct read left out every field that was zero (BuoyancyData without BuoyancyDamp2 = 0, its default 1), so the
// reader could not tell a zero from a field the read did not show.
describe('struct reads write every field', () => {
  it('exports a struct against itself, so no field is compared away', () => {
    const source = readFileSync(join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Foundation', 'Reflection', 'McpPropertyReflection.cpp'), 'utf8');
    expect(source).toContain('StructProp->Struct->ExportText(Exported, StructValue, StructValue, nullptr, 0, nullptr, true);');
  });
});

// Six frame steps at speed 0.1 answered "Stepped 6 frame(s)" for 0.02 s of game time.
describe('a frame step says the speed it ran at', () => {
  it('names the game speed and the game time it advanced', () => {
    const source = code('ControlEditor', 'McpAutomationBridge_ControlEditorPlay.cpp');
    expect(source).toContain('Resp->SetNumberField(TEXT("timeDilation"), Dilation);');
    expect(source).toContain('Resp->SetNumberField(TEXT("gameSeconds"), Stepped * FApp::GetFixedDeltaTime() * Dilation);');
  });
});

// A capture that restored the minimized editor photographed the frame drawn before it was minimized.
describe('a full-window screenshot shows the view as it is now', () => {
  it('a capture that restored the editor redraws and waits before taking the picture', () => {
    const source = code('ControlEditor', 'McpAutomationBridge_ControlEditorScreenshot.cpp');
    expect(source).toContain('Payload->SetBoolField(TEXT("_windowRestoredForCapture"), true);');
    expect(source).toContain('GEditor->RedrawAllViewports(true);');
  });
});

// An expression with a unary minus at step 7 stopped a build_graph batch after 7 nodes were already made.
describe('a graph batch checks its expressions before any step runs', () => {
  it('describes every Expression step in the pre-check, bools the batch declares included', () => {
    const steps = code('BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersBatchSteps.cpp');
    expect(steps).toContain('McpBlueprintMathExpression::DescribeProblems(Blueprint, Expression, DeclaredBools)');
    expect(steps).toContain('DeclaredBools.Add(FName(*Name));');
    const batch = code('BlueprintGraph', 'McpAutomationBridge_BlueprintGraphHandlersBatch.cpp');
    expect(batch.indexOf('DescribeExpressionStep(Context.Blueprint, **Step, DeclaredBools)')).toBeGreaterThan(-1);
    expect(batch.indexOf('DescribeExpressionStep(')).toBeLessThan(batch.indexOf('RunBatchStep(Context, State'));
  });
});

// set_default answered instancesUpdated 2 for a class with no placed copies: it counted editor preview actors.
describe('a class default counts only the actors placed in a level', () => {
  it('skips preview copies when counting instancesUpdated', () => {
    const source = code('Blueprint', 'Graph', 'McpAutomationBridge_BlueprintHandlersSetDefaultLiteral.cpp');
    expect(source).toContain('World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::PIE');
  });
});

// A listed BuoyancyData left out BuoyancyDamp2 = 0: the list read exported structs against zero defaults.
describe('listed struct values show their zero fields', () => {
  it('exports a value against itself in the shared property reader and the list fallback', () => {
    const reader = readFileSync(join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Foundation', 'Reflection', 'McpPropertyReflectionUtilities.cpp'), 'utf8');
    expect(reader).toContain('MCP_PROPERTY_EXPORT_TEXT(Property, Result, Value, Value, nullptr, PPF_None);');
    expect(code('ControlActor', 'List', 'McpAutomationBridge_ControlActorList.cpp'))
      .toContain('MCP_PROPERTY_EXPORT_TEXT(Nested, Value, NestedValue, NestedValue, nullptr, PPF_None);');
  });
});

// A paused motion run at interval 0 answered 74 copies of the same t=0 sample.
describe('a paused motion run repeats no sample', () => {
  it('samples only when game time moved since the last sample', () => {
    const source = code('ControlActor', 'McpAutomationBridge_ControlActorMotionSample.cpp');
    expect(source).toContain('const bool bAdvanced = Run.Samples.Num() == 0 || Now > Run.LastSampleGame;');
    expect(source).toContain('Run.LastSampleGame = GameTime;');
  });
});

// edit_scs modify added a component with no attachTo under DefaultSceneRoot and said nothing about where it went.
describe('a batch-added component says where it was attached', () => {
  it('reports attachedTo when the operation named no parent, on both add paths', () => {
    const resolve = code('Blueprint', 'Components', 'McpAutomationBridge_BlueprintHandlersScsParentResolve.h');
    expect(resolve).toContain('OpSummary->SetStringField(TEXT("attachedTo"), Where);');
    const ops = code('Blueprint', 'Components', 'McpAutomationBridge_BlueprintHandlersModifyScsComponentOps.cpp');
    expect(ops.match(/McpScsParent::ReportParent\(/gu)?.length).toBe(2);
  });
});

// add_scs_component with a properties bag answered success without saying which properties took their value.
describe('a component add names the properties it set', () => {
  it('echoes appliedProperties on the single add path', () => {
    const source = code('Blueprint', 'Components', 'McpAutomationBridge_BlueprintHandlersScsAddComponent.cpp');
    expect(source).toContain('AppliedNames.Add(MakeShared<FJsonValueString>(PropName));');
    expect(source).toContain('Result->SetArrayField(TEXT("appliedProperties"), AppliedNames);');
  });
});

// Setting a Float member's default to 2.5 answered success with default 0: the text was read from the struct's first
// bytes (a String) instead of the member's own offset.
describe('a struct member default lands on its own member', () => {
  it('converts in a scratch copy and reads the value back at the member offset', () => {
    const source = code('AssetWorkflow', 'Structs', 'McpAutomationBridge_AssetWorkflowStructsHelpers.cpp');
    expect(source).toContain('FStructOnScope Scratch(S);');
    expect(source).toContain('const void* Value = Prop->ContainerPtrToValuePtr<void>(Container);');
    expect(source).not.toContain('const_cast<uint8*>(DefaultInstance)');
  });
});

// fix_coplanar left twelve flush pier sections as a staircase after four passes: the smaller piece of every pair moved,
// so each section of a row moved the same way as its neighbour and stayed in its plane.
describe('a row of flush pieces is parted in one pass', () => {
  it('2-colours the touching faces and moves only one colour', () => {
    const plan = code('ControlActor', 'Placement', 'McpAutomationBridge_CoplanarPlan.cpp');
    expect(plan).toContain('const TMap<FMcpFaceKey, bool> Moves = McpCoplanarColour(Hits, Sides);');
    expect(plan).toMatch(/Moves\.Add\(Next, !bMoves\);/u);
    expect(plan).toMatch(/Swap\(Mover, Still\);\s*Shift = -Shift;/u);
    expect(code('ControlActor', 'Placement', 'McpAutomationBridge_CoplanarFix.cpp'))
      .toContain('const TMap<AActor*, FMcpCoplanarPlan> Plans = PlanCoplanarPass(Hits, Distance, Inside);');
  });

  // An ocean was reported as a coplanar pair of its two water info meshes, which no view ever draws.
  it('skips meshes kept out of the main view', () => {
    const faces = code('ControlActor', 'Placement', 'McpAutomationBridge_CoplanarFaces.cpp');
    expect(faces).toContain('return Component->bRenderInMainPass && Component->GetClass()->GetFName() != TEXT("WaterBodyInfoMeshComponent");');
    expect(faces).toContain('!McpCoplanarDrawnInView(Component))');
  });
});
