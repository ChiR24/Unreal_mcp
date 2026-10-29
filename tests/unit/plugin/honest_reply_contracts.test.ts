// Replies that claimed more than the handler did: a success that wrote nothing, a "saved" that
// echoed the request, a debug field, a modal dialog nobody can answer. Wiring contracts only;
// behaviour needs an editor (tests/mcp-tools/**).

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../support/capability-index.js';
import { validateAgainstCapabilitySchema } from '../support/schema-validate.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { countPureLines, sliceBetween } from './plugin-contract-fixtures.js';

const DOMAINS = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const code = (...segments: readonly string[]): string =>
  readFileSync(join(DOMAINS, ...segments), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('handlers answer what they did', () => {
  it('configure_item_stacking fails when no stacking property took the value', () => {
    const source = code('Inventory', 'McpInventoryItemPresentation.cpp');
    expect(source).toMatch(/if \(ModifiedProps\.Num\(\) == 0\) \{\s*Bridge\.SendAutomationError\([\s\S]*?TEXT\("PROPERTY_NOT_FOUND"\)\);\s*return true;/u);
    expect(source).not.toContain('No stacking properties found');
  });

  it('create_enum and create_struct report the save result, not the request', () => {
    expect(code('AssetWorkflow', 'Enums', 'LifecycleEnums.cpp')).toContain('const bool bSaved = FinalizeEnum(Enum, bSave);');
    expect(code('AssetWorkflow', 'Enums', 'Shared.h')).toContain('return bSave && McpSafeAssetSave(Enum);');
    expect(code('AssetWorkflow', 'Structs', 'McpAssetStructsLifecycle.cpp'))
      .toContain('const bool bSaved = bSave && McpSafeAssetSave(S);');
  });

  it('get_metadata sends no debug field', () => {
    expect(code('AssetWorkflow', 'Operations', 'McpAssetMetadata.cpp')).not.toContain('debug_has_meta');
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
    expect(code('AssetWorkflow', 'Operations', 'McpAssetAssetMutation.cpp'))
      .toContain('FailedToDeletePaths.Add(Left + TEXT(" (left in the deleted folder)"));');
  });

  it('bulk_delete never asks the engine for a confirmation dialog', () => {
    const source = code('AssetWorkflow', 'Operations', 'McpAssetBulkDelete.cpp');
    expect(source).not.toContain('showConfirmation');
    expect(source).not.toContain('bShowConfirmation)');
  });

  // Without the dialog the engine deletes nothing when any asset of the batch is still referenced, and
  // bulk_delete said only BULK_DELETE_FAILED: one used mesh kept a whole folder. It keeps the used ones,
  // with the same outside-referencer check delete uses, and names them.
  // Renumbering an enum by hand (SetEnums with fresh values) left every stored value after a removed or moved
  // entry reading as another one. The engine's own remove and move remap loaded data and pins by name.
  it('enum value removal and reorder go through the enum editor, and a refused delete names its users', () => {
    const values = code('AssetWorkflow', 'Enums', 'Values.cpp');
    const removal = values.slice(values.indexOf('TEXT("remove_enum_value")'), values.indexOf('TEXT("rename_enum_value")'));
    expect(removal).toContain('FEnumEditorUtils::RemoveEnumeratorFromUserDefinedEnum(Enum, Index);');
    expect(removal).not.toContain('MCP_SET_ENUMS');
    const reorder = values.slice(values.indexOf('TEXT("reorder_enum_values")'), values.indexOf('TEXT("set_enum_value_metadata")'));
    expect(reorder).toContain('FEnumEditorUtils::MoveEnumeratorInUserDefinedEnum(Enum, From, Target);');
    expect(reorder).not.toContain('MCP_SET_ENUMS');
    expect(code('AssetWorkflow', 'Enums', 'LifecycleEnums.cpp')).toContain('OutResult->SetArrayField(TEXT("referencedBy"), McpHandlerUtils::ToJsonStringArray(Users));');
  });

  // A missing struct answered success, and force:true went through ObjectTools::DeleteObjects, which deletes
  // nothing while anything references the asset: a forced delete of a used struct always failed.
  it('delete_struct refuses a missing struct, names its users, and force really deletes', () => {
    const source = code('AssetWorkflow', 'Structs', 'McpAssetStructsAssetOpsDelete.cpp');
    expect(source).toContain('TEXT("NOT_FOUND")');
    expect(source).not.toContain('Struct deleted (or did not exist)');
    expect(source).toContain('McpOutsideReferencers(Package, {Package})');
    expect(source).toContain('TEXT("STRUCT_IN_USE")');
    expect(source).toContain('McpSafeOperations::McpDeleteAssetAndFile(StructPath)');
    expect(source).not.toContain('ObjectTools::DeleteObjects(');
  });

  it('remove_variable names the get/set nodes the engine deletes with it', () => {
    const source = code('Blueprint', 'Variables', 'McpBPVariableRemovalRename.cpp');
    expect(source.indexOf('McpBlueprintVariableUsers(Blueprint, FName(*Name))'))
      .toBeLessThan(source.indexOf('FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, FName(*Name));'));
    expect(source).toContain('Result->SetArrayField(TEXT("removedNodes"), McpHandlerUtils::ToJsonStringArray(RemovedNodes));');
  });

  // UEditorAssetLibrary::DeleteAsset is a force delete; remove_track and remove_keyframe called Modify() with no
  // transaction open, so neither could be undone; a key removal naming no track cleared the whole sequence.
  it('sequence removals refuse a sequence in use, open a transaction, and need a track or binding', () => {
    const library = code('Sequence', 'McpSeqAssetLibrary.cpp');
    expect(library.indexOf('TEXT("SEQUENCE_IN_USE")')).toBeLessThan(library.indexOf('UEditorAssetLibrary::DeleteAsset(Path)'));
    expect(code('Sequence', 'McpSeqTrackState.cpp')).toContain('FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Remove Sequence Track"))');
    const keys = code('Sequence', 'Metadata', 'McpSeqKeyRemoval.cpp');
    expect(keys).toContain('FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Remove Sequence Keys"))');
    expect(keys).toContain('TEXT("TRACK_REQUIRED")');
  });

  // delete as well: one pass deleted an asset whose only user was a kept one, so both keep until nothing changes.
  it('bulk_delete and delete keep an asset others use and name it, and delete the rest', () => {
    const source = code('AssetWorkflow', 'Operations', 'McpAssetBulkDelete.cpp');
    expect(source).toContain('McpOutsideReferencers(DeleteSet[Index], DeleteSet)');
    expect(source).toContain('Row->SetArrayField(TEXT("referencedBy"), McpHandlerUtils::ToJsonStringArray(Users));');
    expect(source).toContain('TEXT("ASSETS_REFERENCED")');
    expect(source).not.toContain('SetNumberField(TEXT("deleted")');
    expect(code('AssetWorkflow', 'Operations', 'McpAssetAssetMutation.cpp')).toContain('McpOutsideReferencers(DeleteSet[Index], DeleteSet)');
  });

  // Five failed saves of a texture were reported saved: true because an older file was still on
  // disk. And the first overwrite of an existing texture in a session built a new object over the
  // unloaded asset, whose source the editor then refused to save.
  it('a generated texture is saved for real or reported unsaved, and an existing one is loaded and refilled', () => {
    const assets = code('Texture', 'McpTextureAssets.cpp');
    expect(assets).toContain('return McpSafeAssetSave(Texture);');
    expect(assets).not.toContain('bExistsOnDisk');
    expect(assets).toMatch(/LoadObject<UTexture2D>\(nullptr, \*ObjectPath/u);
    expect(assets.indexOf('LoadObject<UTexture2D>')).toBeLessThan(assets.indexOf('CreatePackage('));
  });

  // A BlockingVolume from create_volume had the right bounds and blocked nothing: the brush had
  // polys but no BSP and no convex collision, so triggers and kill volumes never overlapped either.
  it('a volume brush gets its BSP and collision, as the editor\'s own volume placement builds them', () => {
    const geometry = code('Volume', 'McpVolumeGeometry.cpp');
    expect(geometry).toContain('FBSPOps::csgPrepMovingBrush(Volume);');
    expect(geometry.indexOf('CubeBuilder->Build(')).toBeLessThan(geometry.indexOf('csgPrepMovingBrush'));
    expect(readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'EngineWire.Build.cs'), 'utf8'))
      .toContain('"BSPUtils"');
  });

  // configure_exposure actorName "PostProcess" on a level with no volume failed ACTOR_NOT_FOUND,
  // while the same call with no name spawned one: a name that matches nothing now resolves alike.
  it('a post-process call naming a volume that does not exist resolves or creates one under that name', () => {
    const support = code('Render', 'McpRenderSupport.h');
    expect(support).not.toContain('PostProcessVolume not found: %s');
    expect(support).toMatch(/if \(!Volume\)\s*\{\s*FString ResolveError;/u);
    const resolver = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'Render', 'McpPostProcessVolumeResolution.cpp'), 'utf8');
    expect(resolver).toContain('PPV->SetActorLabel(Reference);');
  });

  // Seven instances of one parent cost seven describes and seven consents.
  it('create_material_instance makes a palette under one consent, each entry through the single handler', () => {
    const create = code('MaterialAuthoring', 'Creation', 'McpMatCreateMatInst.cpp');
    expect(create).toContain('Payload->TryGetArrayField(TEXT("instances"), Instances)');
    expect(create).toMatch(/FMcpResponseCaptureRegistry::Get\(\)\.Begin\(StepId\);\s*HandleCreateMaterialInstance\(Bridge, StepId,/u);
    expect(create).toContain('Item->RemoveField(TEXT("instances"));');
    expect(create).toContain('TEXT("INSTANCE_BATCH_INCOMPLETE")');
  });

  it('a resized or combined texture keeps its source colour space', () => {
    expect(code('Texture', 'McpTextureResize.cpp')).toContain('NewTexture->SRGB = SourceTexture->SRGB;');
    expect(code('Texture', 'McpTextureCombine.cpp')).toContain('OutputTexture->SRGB = BaseTex->SRGB;');
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
    expect(code('AssetWorkflow', 'Analysis', 'McpAssetMatGraph.cpp'))
      .toContain('McpAssetExists(SafeAssetPath, &AssetData)');
  }, 60_000);

  // A GameMode whose Audio component plays MS_Music gave 0 matches for "MS_Music": object
  // references were skipped outright.
  it('find_text matches a reference to another asset by its path', () => {
    const source = code('AssetQuery', 'McpAssetQueryFindText.cpp');
    expect(source).toMatch(/else if \(const FObjectPropertyBase\* Ref = CastField<FObjectPropertyBase>\(Property\)\)/u);
    expect(source).toContain('Soft->GetPropertyValue(Data).ToString()');
    expect(source).toContain('!Path.StartsWith(TEXT("/Script/"))');
    expect(source).toContain('FPackageName::ObjectPathToPackageName(Path) != FPackageName::ObjectPathToPackageName(Asset)');
  });

  // {Direction: 0} on an enemy whose variable is Dir answered success "Variables updated", set
  // nothing, and dirtied the level. {Text, Size} on a sign with Label and Size set Size alone and
  // still answered "Variables updated", the miss only in a warning.
  it('set_blueprint_variables refuses the whole call when any name is not a variable, and names the real ones', () => {
    const source = code('ControlActor', 'McpActorAdvanced.cpp');
    expect(source).toContain('if (Missing.Num() > 0) {');
    expect(source).toContain('TEXT("PROPERTY_NOT_FOUND")');
    expect(source.indexOf('TEXT("PROPERTY_NOT_FOUND")')).toBeLessThan(source.indexOf('Found->Modify();'));
    expect(source).toContain('*McpBlueprintVariableList(ActorClass)');
    expect(source).not.toContain('Property not found');
  });

  // A cart corral spawned around its carts read "sunk 97 units": the carts' tops were taken for the ground under it.
  it('the placement ground probe skips what the actor encloses', () => {
    const source = code('ControlActor', 'McpActorPlacementCheck.cpp');
    // The box of what is judged there: the actor's own, or one section of a spline mesh.
    const enclosed = source.indexOf('Sample.Box.ExpandBy(1.0).IsInside(FBox::BuildAABB(HitOrigin, HitExtent))');
    expect(enclosed).toBeGreaterThan(-1);
    expect(enclosed).toBeLessThan(source.indexOf('OutGroundZ = Candidate.ImpactPoint.Z;'));
  });

  // A single spawn refused folder, which only a batch item took, and no action could file an existing actor.
  it('a single spawn takes folder and tags before its placement is judged, and set_folder files placed actors', () => {
    for (const file of ['McpActorSpawn.cpp', 'McpActorBPSpawn.cpp']) {
      const source = code('ControlActor', file);
      const organise = source.indexOf('McpApplySpawnOrganisation(Spawned, Payload);');
      expect(organise, file).toBeGreaterThan(-1);
      expect(organise, file).toBeLessThan(source.indexOf('McpDescribeActorPlacement(RequestId, Spawned,'));
    }
    expect(code('ControlActor', 'McpActorSpawnBatch.cpp')).toContain('void McpApplySpawnOrganisation(AActor *Actor, const TSharedPtr<FJsonObject> &Payload) {');
    const tags = code('ControlActor', 'McpActorTags.cpp');
    expect(tags).toContain('bool UEngineWireSubsystem::HandleControlActorSetFolder(');
    expect(tags).toContain('Actor->SetFolderPath(FolderPath);');
    expect(code('ControlActor', 'McpActorDispatch.cpp')).toContain('if (LowerSub == TEXT("set_folder"))');
  });

  // A spawn node's Class pin default added its ExposeOnSpawn pins, but the create step listed the pins from before.
  it('a create step lists the node\'s pins as they are after its pinDefaults', () => {
    const steps = code('BlueprintGraph', 'McpBPGraphBatchSteps.cpp');
    const refresh = steps.indexOf('Entry->SetStringField(TEXT("pins"), DescribeNodePins(FindBatchNode(Context.Blueprint, Guid)));');
    expect(refresh).toBeGreaterThan(steps.indexOf('ApplyPinDefaults(Context, Payload, Step, Guid, StepId, Entry)'));
  });

  // GateL = "S6_GateL" (a label, as every other call names actors) failed to load as a path, and the call still
  // answered "Variables updated" with the miss only in a warning.
  it('set_blueprint_variables takes an actor variable by label and fails the call when a value does not take', () => {
    const source = code('ControlActor', 'McpActorAdvanced.cpp');
    const support = code('ControlActor', 'McpActorSupport.h');
    expect(source).toContain('McpActorRefsToPaths(Pair.Value, Property, [this](const FString &Name) { return FindActorByName(Name); });');
    expect(support).toContain('Reference->PropertyClass->IsChildOf(AActor::StaticClass())');
    expect(support).toContain('return Actor ? MakeShared<FJsonValueString>(Actor->GetPathName()) : Item;');
    // An array of actors (FireFx) took paths only.
    expect(support).toContain('CastField<FObjectProperty>(Array ? Array->Inner : Property)');
    expect(support).toContain('for (const TSharedPtr<FJsonValue> &Item : Value->AsArray())');
    expect(source).toContain('ApplyJsonValueToProperty(Found, Property, Value, ApplyError)');
    const refusal = source.indexOf('TEXT("VARIABLE_NOT_SET")');
    expect(refusal).toBeGreaterThan(-1);
    expect(refusal).toBeLessThan(source.indexOf('TEXT("Variables updated")'));
    expect(source, 'no success reply carries the failures as warnings').not.toContain('TEXT("Variables updated"), Data, Warnings');
  });

  it('get_blueprint with a property that misses names the variables the Blueprint has', () => {
    expect(code('Blueprint', 'Queries', 'McpBPGet.cpp'))
      .toMatch(/Its variables: %s\."\), \*PropertyName, \*McpBlueprintVariableList\(Generated\)\),\s*Resp, TEXT\("PROPERTY_NOT_FOUND"\)\);/u);
  });

  // {"RotOscillation": {"Pitch": {"Amplitude": 0.8}}} on a LegacyCameraShake became empty text and
  // vanished; create still answered "Blueprint created".
  it('create sets properties through the shared writer and names each one it did not set', () => {
    const writer = code('BlueprintCreation', 'McpBPCreateProps.cpp');
    expect(writer).toContain('ApplyJsonValueToProperty(TargetObject, Property, Pair.Value, Error)');
    expect(writer).not.toContain('McpJsonScalarToString');
    expect(writer).toMatch(/if \(!Property\) \{\s*OutFailed\.Add\(/u);
    const reply = code('BlueprintCreation', 'McpBPCreateAssets.cpp');
    expect(reply).toContain('ResultPayload->SetArrayField(TEXT("failedProperties")');
    expect(reply).toContain('of its properties were not set: %s');
    // Neither list is sent empty: every clean create carried "failedProperties": [].
    expect(reply).toContain('if (AppliedProperties.Num() > 0) {');
    expect(reply).not.toContain('AppliedProperties.Num() + FailedProperties.Num() > 0');
  });

  it('a loot entry added after a removal takes a new key', () => {
    expect(code('Inventory', 'McpInventoryLootTables.cpp')).toContain('NextIndexedPropertyIndex(');
  });

  // get_property "FadeAmount" on a PlayerCameraManager said only "not found in scope".
  it('a property path miss names the similar properties of that scope', () => {
    const resolver = code('..', 'Foundation', 'BridgeHelpers', 'Properties', 'McpNestedPropPath.h');
    expect(resolver).toContain('static inline FString McpSimilarPropertyNames(const UStruct *Scope, const FString &Wanted)');
    expect(resolver).toMatch(/McpSimilarPropertyNames\(CurrentTypeScope, Segment\);\s*OutError = FString::Printf\(/u);
    expect(resolver).toContain('TEXT("; similar: ")');
  });

  // set_property "PivotOffset" on a Niagara mesh renderer answered applied for PivotOffset_DEPRECATED, which UHT
  // registers under the bare name: the engine ignores it, so the cars never moved.
  it('a deprecated property is refused, naming where a member of that name lives now', () => {
    const resolver = code('..', 'Foundation', 'BridgeHelpers', 'Properties', 'McpNestedPropPath.h');
    expect(resolver).toMatch(/if \(CurrentProperty && CurrentProperty->HasAnyPropertyFlags\(CPF_Deprecated\)\) \{\s*OutError = McpDeprecatedPropertyError\(CurrentTypeScope, CurrentProperty\);\s*return nullptr;/u);
    expect(resolver).toContain('const FStructProperty *Inner = CastField<FStructProperty>(Array ? Array->Inner : *It);');
  });

  // create_audio_actor named its new component QuietComp, then get_property "QuietComp.bAutoActivate" on the actor
  // missed it: an instance component has no property behind it.
  it('a middle path segment may name an actor component exactly', () => {
    const resolver = code('..', 'Foundation', 'BridgeHelpers', 'Properties', 'McpNestedPropPath.h');
    expect(resolver).toContain('!CurrentProperty && !bIsLastSegment ? Cast<AActor>(CurrentObject) : nullptr');
    expect(resolver).toContain('Component->GetName().Equals(Segment, ESearchCase::IgnoreCase)');
    expect(resolver, 'a struct member is no actor').toMatch(/ContainerPtrToValuePtr<void>\(CurrentContainer\);\s*CurrentObject = nullptr;/u);
  });

  // A scale key without an {x,y} pair created the binding and an empty track, then answered
  // INVALID_ARGUMENT; the later valid key reported createdTrack false.
  it('a refused widget animation key or track leaves nothing behind', () => {
    const keys = code('WidgetAuthoring', 'Support', 'McpWidgetAnimKeys.cpp');
    const author = keys.slice(keys.indexOf('bool McpAuthorWidgetAnimationKey('));
    expect(author.indexOf('OutError = KeyValueError(Kind, TrackType, ValueField);')).toBeGreaterThan(-1);
    expect(author.indexOf('OutError = KeyValueError(Kind, TrackType, ValueField);')).toBeLessThan(author.indexOf('FindOrCreateBinding('));
    const track = keys.slice(keys.indexOf('bool McpAddWidgetAnimationTrack('));
    expect(track.indexOf('!IsTransformKind(Kind)')).toBeLessThan(track.indexOf('FindOrCreateBinding('));
  });

  it('widget animation track and key replies name the Widget Blueprint for the receipt', () => {
    expect(code('WidgetAuthoring', 'Animation', 'McpWidgetAnimKeyframe.cpp'))
      .toContain('ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBP));');
    const core = code('WidgetAuthoring', 'Animation', 'McpWidgetAnimCore.cpp');
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
    const settings = code('Render', 'McpRenderSupportSettings.h');
    expect(settings).toMatch(/if \(Unknown\.Num\(\) > 0 && OutApplied\.Num\(\) == AppliedBefore\)[\s\S]*?return false;/u);
    expect(settings).toContain('It->GetName().Contains(Key)');
    expect(settings, 'bOverride_ flags are no suggestion').toContain('!It->GetName().StartsWith(TEXT("bOverride_"))');
  });

  // A custom node got its input before the wire feeding it; that mid-batch compile's
  // "missing input" stayed on a receipt whose final compile was clean.
  it('build_material_graph drops compile lines its final compile supersedes', () => {
    const batch = code('MaterialAuthoring', 'McpMatGraphBatch.cpp');
    expect(batch.indexOf('Bridge->ForgetCapturedMessages(')).toBeGreaterThan(-1);
    expect(batch.indexOf('Bridge->ForgetCapturedMessages(')).toBeLessThan(batch.indexOf('RunStep(CompileId, Compile);'));
  });

  // The engine names the asset in a compile failure by the file it is saved in ("[AssetLog] <disk path>.uasset: Failed to
  // compile Material ..."), and by the object path only while no file exists. The forget matched the package name alone,
  // so every line of a saved material stayed (live: M_Toon, three "missing input" lines in warnings and engineWarnings
  // beside "compiles and was saved"); the receipt shows the disk path as its package path, which hid the mismatch.
  it('the compile lines it forgets are found by the asset file the engine prints for a saved material, not only by its object path', () => {
    const batch = code('MaterialAuthoring', 'McpMatGraphBatch.cpp');

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
    const header = code('MaterialAuthoring', 'McpMatPrivate.h');
    const batch = code('MaterialAuthoring', 'McpMatGraphBatch.cpp');
    const compile = code('MaterialAuthoring', 'Properties', 'McpMatCompileMat.cpp');

    expect(header).toMatch(/inline void McpFinishMaterialEdit\(const FString &RequestId, UObject \*Host\) \{\s*if \(Host && !FMcpResponseCaptureRegistry::Get\(\)\.IsBatchStep\(RequestId\)\) \{ Host->PostEditChange\(\); \}\s*if \(Host\) \{ Host->MarkPackageDirty\(\); \}\s*\}/u);
    expect(header).toContain('#define FINALIZE_HOST() McpFinishMaterialEdit(RequestId, Material ? static_cast<UObject *>(Material) : static_cast<UObject *>(Function))');
    for (const file of [
      ['Nodes', 'McpMatAddMatNode.cpp'],
      ['Nodes', 'McpMatUseMatFunction.cpp'],
      ['Nodes', 'McpMatFunctionInputsOutputs.cpp'],
      ['Properties', 'McpMatSetMatEnum.cpp'],
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
      ['Nodes', 'McpMatUpdateCustomExpr.cpp'],
      ['Nodes', 'McpMatDeleteNode.cpp'],
      ['Parameters', 'McpMatAddParam.cpp'],
    ] as const) {
      expect(code('MaterialAuthoring', ...file)).toContain('McpMaterialHostResult(HostOuter)');
    }
    expect(code('MaterialAuthoring', 'Connections', 'McpMatConnectNodes.cpp').split('McpMaterialHostResult(HostOuter)').length).toBe(5);
  });

  // update_custom_expression ended in the edit helper (a recompile and a dirty mark) and answered "Custom expression updated.":
  // live, the material stayed in unsavedPackages and a broken edit went unseen. Called on its own it compiles, saves and
  // reports as compile_material does; as a step of build_material_graph it leaves both to the batch.
  it('update_custom_expression on its own saves the material and says whether it compiles, as compile_material does', () => {
    const source = code('MaterialAuthoring', 'Nodes', 'McpMatUpdateCustomExpr.cpp');
    const own = source.slice(source.indexOf('if (!FMcpResponseCaptureRegistry::Get().IsBatchStep(RequestId)) {'));

    expect(source.indexOf('FINALIZE_HOST();'), 'after the edit').toBeLessThan(source.indexOf('IsBatchStep(RequestId)'));
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
    const compile = code('MaterialAuthoring', 'Properties', 'McpMatCompileMat.cpp');

    expect(compile, 'every shader, not the on-demand few a post-edit recompile asks for').toMatch(/Material->ForceRecompileForRendering\(\);\s*FMaterialResource \*Resource = MCP_GET_MATERIAL_RESOURCE\(Material\);/u);
    expect(compile).toMatch(/Resource->FinishCompilation\(\);\s*TArray<FString> Errors = Resource->GetCompileErrors\(\);/u);
    expect(compile, 'a failed shader map leaves none behind').toContain('if (Errors.Num() == 0 && !Resource->GetGameThreadShaderMap()) {');
    expect(compile).toContain('const TArray<FString> CompileErrors = McpMaterialCompileErrors(Material);');
    expect(code('MaterialAuthoring', 'McpMatGraphBatch.cpp'), 'the batch compiles through compile_material')
      .toContain('Compile->SetStringField(TEXT("subAction"), TEXT("compile_material"));');
  });

  // set_material_parameter with a parameters list answered {parameters, applied} and no assetPath, so the receipt
  // named nothing for a call that wrote an instance; every single-parameter setter answers it through AddVerification.
  it('set_material_parameter names the asset its parameters list wrote, as the single-parameter setters do', () => {
    const list = code('MaterialAuthoring', 'Parameters', 'McpMatSetMatParam.cpp');
    const setter = code('MaterialAuthoring', 'Parameters', 'McpMatParamValue.cpp');

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
    const list = code('MaterialAuthoring', 'Parameters', 'McpMatSetMatParam.cpp');
    const create = code('MaterialAuthoring', 'Creation', 'McpMatCreateMatInst.cpp');
    const setter = code('MaterialAuthoring', 'Parameters', 'McpMatParamValue.cpp');

    expect(setter).toContain('Payload->TryGetBoolField(TEXT("save"), bSave);');
    // The reply says whether the write reached disk; it did not, so a refused save read as done.
    expect(setter).toContain('const bool bSaved = bSave && McpSafeAssetSave(BaseMaterial);');
    expect(setter).toContain('const bool bSaved = bSave && McpSafeAssetSave(Instance);');
    expect(setter.split('Result->SetBoolField(TEXT("saved"), bSaved);'), 'both branches').toHaveLength(3);
    expect(list).toMatch(
      /One->SetStringField\(TEXT\("assetPath"\), AssetPath\);\s*if \(Shared\.IsValid\(\) && !One->HasField\(TEXT\("save"\)\) && Shared->HasField\(TEXT\("save"\)\)\) \{\s*One->SetField\(TEXT\("save"\), Shared->TryGetField\(TEXT\("save"\)\)\);\s*\}/u
    );
    expect(list).toMatch(/ApplyMaterialParameterList\(Bridge, RequestId, GetJsonStringField\(Payload, TEXT\("assetPath"\)\), \*Entries, Payload, Socket,/u);
    expect(create).toMatch(/ApplyMaterialParameterList\(Bridge, RequestId, NewInstance->GetOutermost\(\)->GetName\(\), \*Entries, Payload, Socket,/u);
  });

  // get_material_info listed the UMaterialExpressionParameter kinds only, so a material built from
  // TextureObjectParameters (BaseColor, Normal, ORM) answered its scalars and vectors and no textures.
  it('get_material_info lists texture parameters with their default texture and sampler type, and find_node matches them by name', () => {
    const info = code('MaterialAuthoring', 'Queries', 'McpMatGetMatInfo.cpp');
    const find = code('MaterialAuthoring', 'Queries', 'McpMatFindNode.cpp');

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

  // A mesh renderer's Meshes written on a Niagara system changed nothing on screen: placed effects kept the render data
  // they started with until each was assigned its system again.
  it('set_property on an object inside a Niagara system re-initializes the effects running it', () => {
    const access = code('Property', 'McpPropActorAccess.cpp');
    const set = code('Property', 'McpPropObjectSet.cpp');
    expect(access).toContain('UNiagaraSystem* System = Edited ? Edited->GetTypedOuter<UNiagaraSystem>() : nullptr;');
    expect(access).toContain('FNiagaraSystemUpdateContext ReInit(System, true);');
    expect(set.indexOf('RefreshNiagaraHostAfterEdit(RootObject)')).toBeGreaterThan(-1);
    expect(set.indexOf('RefreshNiagaraHostAfterEdit(RootObject)')).toBeLessThan(set.indexOf('McpPropertyTarget::SaveAfterWrite('));
  });

  // set_property on a material expression (<material>.<material>:<nodeName>) ran only the expression's own PostEditChange,
  // which never reaches its material: the cached expression data every instance reads kept the old ParameterName until
  // some other call rebuilt the material.
  it('set_property on an object inside a material or material function rebuilds that material, as compile_material does', () => {
    const access = code('Property', 'McpPropActorAccess.cpp');
    const set = code('Property', 'McpPropObjectSet.cpp');
    const compile = code('MaterialAuthoring', 'Properties', 'McpMatCompileMat.cpp');

    expect(access).toMatch(/bool RefreshMaterialHostAfterEdit\(UObject\* Edited\)\s*\{\s*UObject\* Host = Edited \? Edited->GetTypedOuter<UMaterial>\(\) : nullptr;\s*if \(!Host && Edited\)\s*\{\s*Host = Edited->GetTypedOuter<UMaterialFunction>\(\);\s*\}\s*if \(!Host\)\s*\{\s*return false;\s*\}\s*Host->PreEditChange\(nullptr\);\s*Host->PostEditChange\(\);\s*return true;\s*\}/u);
    expect(compile, 'the same two calls compile_material makes').toMatch(/Host->PreEditChange\(nullptr\);\s*Host->PostEditChange\(\);/u);
    expect(set).toMatch(/RefreshK2NodeTitleCacheIfNeeded\(RootObject\);\s*const bool bMaterialRebuilt = McpPropertyActorAccess::RefreshMaterialHostAfterEdit\(RootObject\);/u);
    expect(set.indexOf('RefreshMaterialHostAfterEdit(RootObject)'), 'before the save, so the saved material is the rebuilt one').toBeLessThan(set.indexOf('McpPropertyTarget::SaveAfterWrite('));
    expect(set).toMatch(/if \(bMaterialRebuilt\) \{\s*ResultPayload->SetBoolField\(TEXT\("materialRebuilt"\), true\);\s*\}/u);

    const properties = capabilityIndex().byId.get('inspect.set_property')?.schemas.output.properties;
    const rebuilt = isRecord(properties) ? properties.materialRebuilt : undefined;
    expect(isRecord(rebuilt) ? rebuilt.description : '').toMatch(/inside a material or material function .* that material was rebuilt/u);
  });

  // disconnect_nodes answered "Disconnect operation completed." with nothing unplugged.
  it('disconnect_nodes fails when no pin matched and unplugs a custom node input by label', () => {
    const source = code('MaterialAuthoring', 'Connections', 'McpMatDisconnectNodes.cpp');
    expect(source).not.toContain('Disconnect operation completed');
    expect(source).toContain('Target = TargetExpr->GetInput(InputIndex);');
  });

  // A TextureObjectParameter added by type stayed named "None" and had no texture, so no
  // instance could set it; add_texture_sample dropped a path that did not load.
  it('texture nodes take their name and texture, and a texture that does not load fails', () => {
    const generic = code('MaterialAuthoring', 'Nodes', 'McpMatAddMatNode.cpp');
    expect(generic).toContain('if (NewExpr->HasAParameterName()) {');
    expect(generic).toContain('TextureNode->AutoSetSampleType();');
    const sample = code('MaterialAuthoring', 'Nodes', 'McpMatAddTextureSample.cpp');
    expect(sample).toMatch(/if \(!ResolvedTexture\) \{\s*Bridge->SendAutomationError\([\s\S]*?TEXT\("ASSET_NOT_FOUND"\)\);/u);
    expect(sample).toContain('CreatedExpr->AutoSetSampleType();');
  });

  // revolve turned the same fixed vase at the origin whatever profile, name or place was asked.
  it('revolve turns the given profile, placed and named as asked', () => {
    const source = code('Geometry', 'Primitives', 'McpGeometryPrimitivesShapes.cpp');
    expect(source).toContain('Payload->TryGetArrayField(TEXT("profile"), Profile)');
    expect(source).toContain('RevolveOptions.RevolveDegrees = ');
    expect(source).toContain('ProfilePoints, RevolveOptions, Steps, bCap, nullptr);');
    expect(source).toContain('SpawnPrimitiveOrReply(Self, RequestId, Socket, ReadTransformFromPayload(Payload), Name, DynMesh, Result)');
    expect(source).toContain('Result->SetBoolField(TEXT("usedDefaultProfile"), bDefaultProfile);');
  });

  // Tuning one emitter took a call, a compile request and a save per value: twelve calls for one emitter.
  it('set_parameter_value writes a parameters list in one pass, reports every entry and saves once', () => {
    const source = code('NiagaraAuthoring', 'McpNiagaraParamValues.cpp');
    const between = (from: string, to?: string): string => source.slice(source.indexOf(from), to === undefined ? undefined : source.indexOf(to));
    const writer = between('static FParameterWrite WriteParameter(', 'static bool SetParameterValueList(');
    const list = between('static bool SetParameterValueList(', 'bool SetParameterValue(FActionContext& Context)');
    const single = between('bool SetParameterValue(FActionContext& Context)');

    expect(writer, 'one value written: no reply, no compile request, no save').not.toMatch(/SendError|SendSuccess|MarkDirtyAndVerify|RequestCompile/u);
    expect(list).toContain('Write = WriteParameter(System, Emitter, Name, Entry);');
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
    expect(code('NiagaraAuthoring', 'McpNiagaraParams.cpp')).toContain('if (SubAction == TEXT("set_parameter_value")) return SetParameterValue(Context);');
  });

  // bind refused "ShapeLocation.Box Size" (a space is not an identifier character) although get_niagara_info lists it and
  // set_parameter_value writes it, and it never looked past module calls, so "RandomRangeFloat.Minimum" was not an input.
  it('bind_parameter_to_source takes the module and dynamic-input input names get_niagara_info lists', () => {
    const params = code('NiagaraAuthoring', 'McpNiagaraParams.cpp');
    expect(params).toContain('Function = FindDynamicInputCallInput(Graph, ParamName, Input);');
    expect(params, 'only a variable the call creates is held to identifier characters').toContain(
      'if (!bModuleInput && !ValidateNiagaraIdentifier(Context, ParamName, TEXT("parameterName"), true))');
    expect(params).not.toContain('ValidateNiagaraIdentifier(Context, ParamName, TEXT("parameterName"), true) ||');
    expect(code('NiagaraAuthoring', 'McpNiagaraDynamicInputResolve.cpp')).toContain('Call->GetCalledUsage() == ENiagaraScriptUsage::DynamicInput');
    // get_niagara_info showed a bound dynamic-input input as its stale literal: only module calls' overrides were read.
    const info = code('NiagaraAuthoring', 'McpNiagaraModuleInfo.cpp');
    expect(info).toContain('if (Usage != ENiagaraScriptUsage::Module && Usage != ENiagaraScriptUsage::DynamicInput)');
    expect(info).not.toContain('Module->GetCalledUsage() != ENiagaraScriptUsage::Module');
  });

  // A Ribbon Width written while InitializeParticle.Ribbon Width Mode was Unset changed nothing, and no call could see or flip the
  // switch: a static switch is a pin of the module's call node, not a rapid-iteration parameter.
  it('set_parameter_value sets a module static switch as the stack does, and get_niagara_info lists them', () => {
    const values = code('NiagaraAuthoring', 'McpNiagaraParamValues.cpp');
    const graph = code('NiagaraAuthoring', 'McpNiagaraStackGraph.cpp');

    expect(values).toContain('SetModuleStaticSwitch(System, EmitterName, ParamName, Entry->TryGetField(TEXT("parameterValue")), SwitchError)');
    expect(code('NiagaraAuthoring', 'McpNiagaraContext.h')).toContain('Pin->Direction == EGPD_Input && Pin->bNotConnectable && !Pin->bOrphanedPin');
    expect(graph).toMatch(/Pin->DefaultValue = Default;\s*Node->MarkNodeRequiresSynchronization\([^;]*, true\);/u);
    expect(graph, 'an enum pin holds the entry name the type editor writes').toContain('Enum->GetNameStringByValue(EntryValue)');
    const info = code('NiagaraAuthoring', 'McpNiagaraInfoValidate.cpp');
    expect(info).toContain('EmitterObj->SetObjectField(TEXT("staticSwitches"), CollectModuleStaticSwitches(Handle));');
    expect(info, 'and each renderer by the object path set_property writes').toContain('RendererObj->SetStringField(TEXT("objectPath"), Renderer->GetPathName());');
  });

  // A module for the particle spawn stack of an emitter with interpolated spawning found no stack: its script
  // runs as ParticleSpawnScriptInterpolated while the stack output says ParticleSpawnScript.
  it('add_niagara_module finds the spawn stack of an emitter with interpolated spawning', () => {
    const source = code('NiagaraGraph', 'McpNiagaraGraphHandlers.cpp');
    expect(source).toContain('UNiagaraScript::IsEquivalentUsage(Candidate->GetUsage(), TargetScript->GetUsage())');
    expect(source).not.toContain('Candidate->GetUsage() == TargetScript->GetUsage()');
  });

  // A template module that worked against the values set (a size curve, wind) could not be switched off.
  it('set_emitter_properties turns stack modules on or off by name and refuses a name it cannot find', () => {
    const systems = code('NiagaraAuthoring', 'McpNiagaraSystems.cpp');
    expect(systems).toMatch(/TryGetObjectField\(TEXT\("moduleEnabled"\), ModulesObj\)/u);
    expect(systems).toMatch(/Module->SetEnabledState\(bOn \? ENodeEnabledState::Enabled : ENodeEnabledState::Disabled, false\);\s*Module->MarkNodeRequiresSynchronization\(/u);
    expect(systems).toContain('TEXT("MODULE_NOT_FOUND")');
    expect(code('NiagaraAuthoring', 'McpNiagaraModuleInfo.cpp')).toContain('ModuleObj->SetBoolField(TEXT("enabled"), Module->IsNodeEnabled());');
  });

  // Every placed effect fired once at level start, whatever cue it was placed for.
  it('a Niagara effect can be placed switched off', () => {
    const spawn = code('Effect', 'McpEffectNiagaraSpawn.cpp');
    // SetAutoActivate is refused after registration (it only logs), so the flag is written directly.
    expect(spawn).toMatch(/GetJsonBoolField\(Context\.Payload, TEXT\("autoActivate"\), true\);\s*NiagaraComponent->bAutoActivate = bAutoActivate;/u);
    expect(spawn).not.toContain('SetAutoActivate(');
    // Deactivate only winds a system down, and an editor world never ticks it to the end.
    expect(spawn).toMatch(/if \(bAutoActivate\)\s*\{\s*NiagaraComponent->Activate\(true\);\s*\}\s*else\s*\{\s*NiagaraComponent->DeactivateImmediate\(\);/u);
  });

  // An activate key restarted a finished one-shot every frame of its section: a spark burst fired again and again.
  it('a particle track can fire a one-shot burst once with a trigger key', () => {
    const tracks = code('Sequence', 'Cinematics', 'McpSeqCineTracks.cpp');
    expect(tracks).toMatch(/KeyName == TEXT\("trigger"\) \? EParticleKey::Trigger/u);
    const handler = tracks.slice(tracks.indexOf('bool HandleAddParticleTrack'));
    expect(handler.indexOf('particleKey must be activate, deactivate or trigger')).toBeLessThan(handler.indexOf('CreateBoundSection('));
  });

  // A module added, wired or removed ran the old scripts and listed no inputs until something else compiled.
  it('a Niagara graph edit compiles the system before saving it', () => {
    const save = code('NiagaraGraph', 'McpNiagaraGraphPrivate.h');
    expect(save).toMatch(/inline void SaveNiagaraGraphEdit\([^)]*\)\s*\{\s*System->RequestCompile\(false\);\s*System->MarkPackageDirty\(\);/u);
  });

  // create_niagara_ribbon and create_particle_trail authored LocationBasedRibbon, which spawns only on another emitter's location events.
  it('a default ribbon or trail effect spawns on its own', () => {
    const source = code('Effect', 'McpEffectNiagaraAuthoring.cpp');

    expect(source).toContain('TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate")');
    expect(source, 'at a trail rate, not the module\'s 1 a second').toMatch(/Rate->SetNumberField\(TEXT\("parameterValue"\), 60\.0\);[\s\S]*?SetModuleInputValue\(&System, EmitterName, TEXT\("SpawnRate\.SpawnRate"\), Rate,/u);
    expect(source).toContain('TEXT("InitializeParticle.Ribbon Width Mode"), MakeShared<FJsonValueString>(TEXT("Direct Set"))');
    expect(source, 'and keeps trailing past the template\'s single 5 s loop').toContain('TEXT("EmitterState.Loop Behavior"), MakeShared<FJsonValueString>(TEXT("Infinite"))');
    expect(source).toMatch(/if \(Template && !bExplicitTemplate && TemplatePath\.Contains\(TEXT\("LocationBasedRibbon"\)\)\)\s*\{\s*OutDetails->SetBoolField\(TEXT\("spawnRateAdded"\), MakeRibbonStandalone\(\*System, EmitterName\)\);/u);
  });

  // Niagara objects made without RF_Transactional skipped undo and made every later edit's stack check warn.
  it('Niagara systems, emitters and renderers the tools make are transactional, and older systems are repaired on load', () => {
    expect(code('NiagaraAuthoring', 'McpNiagaraSystems.cpp').split('RF_Public | RF_Standalone | RF_Transactional')).toHaveLength(3);
    expect(code('Effect', 'McpEffectNiagaraAuthoring.cpp')).toContain('NewObject<UNiagaraSystem>(Package, FName(*Name), RF_Public | RF_Standalone | RF_Transactional)');
    expect(code('NiagaraAuthoring', 'McpNiagaraRenderers.cpp')).toContain('NewObject<TRenderer>(Target.Emitter, NAME_None, RF_Transactional)');
    expect(code('NiagaraAuthoring', 'McpNiagaraContext.cpp')).toMatch(/System->SetFlags\(RF_Transactional\);\s*return System;/u);
  });

  // Every effect a creator authored also dropped a preview actor at the world origin of whatever level was open.
  it('an authored effect is placed in the level only when location asks for it', () => {
    const source = code('Effect', 'McpEffectProceduralEffects.cpp');

    expect(source).toMatch(/if \(!Context\.Payload->HasField\(TEXT\("location"\)\)\)\s*\{\s*Details->SetStringField\(TEXT\("systemPath"\), AuthoredSystemPath\);\s*Details->SetBoolField\(TEXT\("placed"\), false\);/u);
    expect(source.indexOf('HasField(TEXT("location"))'), 'checked before the placing call').toBeLessThan(source.lastIndexOf('return CreateNiagaraEffectFromPayload(Context, EffectName, AuthoredSystemPath, Details);'));
  });

  // The editor sets a missing usage flag only outside PIE and never saves it, so a renderer material drew as the default material.
  it('a sprite or ribbon renderer material gets its Niagara usage flag, set and saved', () => {
    const source = code('NiagaraAuthoring', 'McpNiagaraRenderers.cpp');

    expect(source).toContain('EnsureNiagaraUsage(Material, bSprite ? TEXT("bUsedWithNiagaraSprites") : TEXT("bUsedWithNiagaraRibbons"));');
    expect(source).toMatch(/Flag->SetPropertyValue_InContainer\(Base, true\);\s*Base->PostEditChange\(\);\s*McpSafeOperations::McpSafeAssetSave\(Base\);/u);
  });

  it('the record declares the list, one of the two ways to name a value, and what the reply carries', () => {
    const record = capabilityIndex().byId.get('manage_effect.edit_niagara_system');
    const input = record?.schemas.input;
    const output = record?.schemas.output.properties;

    // A list is read by the system form and by a placed effect's form (with actorName, which only set_parameter reads,
    // so a list sent with actorName and no edit still picks it).
    expect(record?.routing.dispatchBy?.declaredBy?.parameters).toEqual(['set_parameter_value', 'set_parameter']);
    expect(record?.routing.dispatchBy?.declaredBy?.actorName).toEqual(['set_parameter']);
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
    const load = (): string => code('Level', 'Lifecycle', 'McpLevelLoad.cpp');
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
      expect(compact()).toContain('TEXT("Level loaded"), LoadReply(true), bWasGameView);');
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
    [['Level', 'Lifecycle', 'McpLevelLoad.cpp'], 'const bool bLoaded = McpSafeLoadMap('],
    [['Level', 'Lifecycle', 'McpLevelCreate.cpp'], 'McpSafeLoadMap(SavePath, true)'],
    [['ControlEditor', 'McpEditorLevel.cpp'], 'McpSafeLoadMap(MapPathToLoad)'],
    [['LevelStructure', 'McpLevelStructureLevelCreation.cpp'], 'LoadIfRequested(Result,'],
  ];

  it('is refused in every mode by one shared check, which lists what would be lost', () => {
    const safety = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Safety', 'McpSafeOperationsMapLoad.h'), 'utf8');
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
    const source = code('Level', 'Lifecycle', 'McpLevelLoad.cpp').split(/\s+/u).join(' ');
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
    const source = editorControl('Session', 'McpEditorEject.cpp');
    expect(source).toContain('GEditor->RequestToggleBetweenPIEandSIE();');
    expect(source, 'there is no Eject console command').not.toMatch(/Exec\([^)]*Eject/u);
    expect(source).toContain('GetEjectedPieViewportClientForMcp()');
    expect(source).toContain('TEXT("EJECT_FAILED")');
    expect(editorControl('McpEditorPlay.cpp')).not.toContain('HandleControlEditorEject');
    expect(outputNames('control_editor.play')).toEqual(expect.arrayContaining(['ejected', 'alreadyEjected', 'view']));
  });

  it('set_camera refuses until the player is ejected, and says which view it moved', () => {
    const camera = editorControl('McpEditorCamera.cpp');
    const setCamera = camera.slice(camera.indexOf('HandleControlEditorSetCamera('));
    const refusal = setCamera.indexOf('RefuseCameraMoveWhilePieFollowsPawnForMcp(');
    expect(refusal).toBeGreaterThan(-1);
    expect(refusal, 'the refusal comes before a viewport is picked').toBeLessThan(setCamera.indexOf('GetActiveEditorViewportClientForMcp()'));
    expect(setCamera).toContain('TEXT("pie_ejected")');
    const support = editorControl('McpEditorViewportSupport.cpp');
    expect(support).toContain('TEXT("PIE_VIEW_NOT_EJECTED")');
    expect(support, 'the shared viewport lookup answers the ejected view first').toContain('if (FEditorViewportClient *Ejected = GetEjectedPieViewportClientForMcp()) {');
    expect(outputNames('control_editor.set_camera')).toEqual(expect.arrayContaining(['view', 'cameraLocation', 'cameraRotation']));
  });

  it('a screenshot of an ejected game is taken from the editor viewport that draws it, and a camera it cannot move is refused', () => {
    const shot = editorControl('McpEditorScreenshot.cpp');
    expect(shot).toContain('Mode == TEXT("game_viewport") && !bEjectedView');
    expect(shot).toContain('GEditor->PlayWorld != nullptr && !bEjectedView && GEditor->GetPIEViewport() != nullptr');
    expect(shot).toContain('RefuseCameraMoveWhilePieFollowsPawnForMcp(this, Socket, RequestId,');
    expect(shot, 'the note that said location and rotation were ignored is gone').not.toContain('cameraNote');
    expect(code('Ui', 'McpUiScreenshot.cpp')).toContain('Mode == TEXT("game_viewport") && GetEjectedPieViewportClientForMcp()');
    expect(outputNames('control_editor.screenshot')).toContain('view');
  });

  it('possess hands the player controller the pawn, and with no pawn brings an ejected player back', () => {
    const session = editorControl('Session', 'McpEditorEject.cpp');
    const possess = session.slice(session.indexOf('HandleControlEditorPossess('));
    expect(possess).toContain('Controller->Possess(Pawn);');
    expect(possess, 'the way back from eject is the same toggle').toContain('GEditor->RequestToggleBetweenPIEandSIE();');
    expect(possess, 'there is no POSSESS console command').not.toMatch(/Exec\([^)]*POSSESS/u);
    expect(editorControl('McpEditorPlay.cpp')).not.toContain('HandleControlEditorPossess(');
    expect(outputNames('control_editor.play')).toEqual(expect.arrayContaining(['possessed', 'returnedFromEject']));
  });

  it('the other level-viewport moves refuse while the player plays, and the view settings reach the game on screen', () => {
    expect(editorControl('Camera', 'McpEditorCameraFocus.cpp'))
      .toContain('RefuseCameraMoveWhilePieFollowsPawnForMcp(this, Socket, RequestId, TEXT("focus_actor"))');
    const toggles = editorControl('McpEditorViewportToggles.cpp');
    const gameView = toggles.slice(toggles.indexOf('HandleControlEditorSetGameView('));
    const refusal = gameView.indexOf('TEXT("PIE_VIEW_NOT_EJECTED")');
    expect(refusal).toBeGreaterThan(-1);
    expect(refusal, 'refused before the hidden level viewport is toggled').toBeLessThan(gameView.indexOf('Client->SetGameView('));
    const viewMode = editorControl('McpEditorViewMode.cpp');
    expect(viewMode).toMatch(/ApplyViewMode\(ViewModeIndex,\s*true,\s*GEditor->GameViewport->EngineShowFlags\)/u);
    expect(viewMode).toContain('Controller->PlayerCameraManager->SetFOV(static_cast<float>(Fov));');
  });
});

// A mesh ASSET's material slots: what each one holds, and which part of the mesh it covers.
describe('the material slots of a mesh asset', () => {
  const outputNames = (id: string): readonly string[] => Object.keys(capabilityIndex().byId.get(id)?.schemas.output.properties ?? {});

  it('set_mesh_materials loads each material strictly, fails on a refused entry and never saves engine content', () => {
    const slots = code('AssetWorkflow', 'Materials', 'McpAssetMeshMatSlots.h');
    expect(slots).toContain('McpLoadAsset(SafePath)');
    expect(slots, 'a material that does not load is refused, never replaced by a default').not.toContain('McpLoadMaterialWithFallback');
    const handler = code('AssetWorkflow', 'Materials', 'McpAssetMeshMats.cpp');
    expect(handler).toContain('TEXT("MATERIAL_SLOTS_PARTIAL")');
    expect(handler).toContain('TEXT("MATERIAL_SLOTS_REFUSED")');
    expect(handler).toContain('TEXT("engine content is not saved")');
    expect(code('AssetWorkflow', 'McpAssetHandlers.cpp')).toContain('McpMeshMaterials::HandleSetMeshMaterials(');
    expect(outputNames('asset.process_asset')).toEqual(expect.arrayContaining(['materialSlots', 'applied', 'refused', 'saved']));
  });

  it('mesh details attribute each LOD0 section to the slot its MaterialIndex names', () => {
    const mesh = code('Environment', 'Inspection', 'McpEnvInspectAssetMesh.cpp');
    expect(mesh).toContain('Slots[Section.MaterialIndex]');
    expect(mesh).toContain('TEXT("slotBoundsAvailable")');
    expect(mesh).toContain('TEXT("triangles")');
    expect(outputNames('inspect.inspect_object')).toEqual(expect.arrayContaining(['materialSlots', 'slotBoundsAvailable']));
  });
});

describe('the main inputs of a material', () => {
  const inputs = code('MaterialAuthoring', 'McpMatMainInputs.h');

  it('take the names other shading models show for the same pins', () => {
    expect(inputs).toContain('PinName == TEXT("Cloth") || PinName == TEXT("CustomData0")) return TEXT("ClearCoat");');
    expect(inputs).toContain('PinName == TEXT("CustomData1")) return TEXT("ClearCoatRoughness");');
    expect(inputs).toContain('PinName == TEXT("FuzzColor")) return TEXT("SubsurfaceColor");');
    expect(inputs).toContain('const FString Canonical = CanonicalMainInputName(PinName);');
  });

  it('list every accepted pin when connect or disconnect does not know the one asked for', () => {
    const connect = code('MaterialAuthoring', 'Connections', 'McpMatConnectNodes.cpp');
    expect(connect).toContain('Unknown input on main node: %s. Main inputs: %s."), *InputName, *ListMainMaterialInputs(Material)');
    const disconnect = code('MaterialAuthoring', 'Connections', 'McpMatDisconnectNodes.cpp');
    expect(disconnect).toContain('GetMainMaterialInput(Material, NormalizeMaterialInputName(PinName))');
    expect(disconnect).toContain('*ListMainMaterialInputs(Material)');
  });
});

describe('object and class references in a property bag', () => {
  const objects = readFileSync(
    join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'BridgeHelpers', 'Properties', 'McpPropApplyObjects.h'),
    'utf8',
  ).replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

  it('null, "" and "None" clear a hard or soft reference (edit_scs ChildActorClass "None" was refused)', () => {
    expect(objects).toContain('ValueField->AsString().Equals(TEXT("None"), ESearchCase::IgnoreCase)');
    expect(objects).toMatch(/if \(bClearReference\) \{\s*return Assign\(nullptr\);/u);
    expect(objects).toMatch(/if \(!MeshOwner\) \{\s*OP->SetObjectPropertyValue_InContainer\(TargetContainer, Value\);/u);
    expect(objects).toMatch(/if \(bClearReference\) \{\s*\*SoftPtr = FSoftObjectPtr\(\);/u);
  });

  it('an object of the wrong class is refused, not stored; a class must derive from the property\'s meta class', () => {
    expect(objects).toContain('CP ? !(AsClass && AsClass->IsChildOf(Wanted)) : !Res->IsA(Wanted)');
  });

  // A null error device crashed the editor: the enum import logs a name it refuses through it.
  it('export text is imported with an error device', () => {
    expect(objects).toContain('Property->ImportText_Direct(*Text, Scratch, nullptr, PPF_None) != nullptr');
    expect(objects).not.toMatch(/ImportText_Direct\([^;]*PPF_None, nullptr\)/u);
  });
});

describe('container values', () => {
  const arrays = readFileSync(
    join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'BridgeHelpers', 'Properties', 'McpPropApplyArrays.h'),
    'utf8',
  ).replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

  // Per-type casts stored an enum name as 0, cut fractions and wrapped bytes, all reported as written.
  it('every array element goes through the shared importer', () => {
    expect(arrays).toContain('ApplyJsonValueToProperty(ElemPtr, AP->Inner, Src[i], Error)');
    expect(arrays).not.toContain('FCString::Ato');
  });

  it('an array, map or set is filled in scratch, so a refused entry leaves the property as it was', () => {
    expect(arrays.match(/McpWriteThroughScratch\(/gu)).toHaveLength(2);
  });
});

describe('Nanite bakes and rebuilds', () => {
  it('convert_to_nanite turns Nanite on in the settings the new mesh copies and replies with what the mesh holds', () => {
    // From 5.1 the asset takes NaniteSettings whole and its bEnabled defaults to false, so bEnableNanite alone
    // baked plain meshes while the reply echoed naniteEnabled: true.
    const conversion = code('Geometry', 'Support', 'McpGeometryAssetConversion.cpp');
    expect(conversion).toContain('CreateOptions.NaniteSettings.bEnabled = bNanite;');
    expect(conversion).toContain('Result->SetBoolField(TEXT("naniteEnabled"), bNaniteOn);');
    expect(conversion).toContain('TEXT("NANITE_NOT_ENABLED")');
  });

  it('nanite_rebuild_mesh rebuilds and saves on every version, reads the result back, and both routes share it', () => {
    const nanite = code('AssetWorkflow', 'Optimization', 'McpAssetNanite.cpp');
    expect(nanite).toContain('StaticMesh->Build(true);');
    expect(nanite).toMatch(/if \(!McpSafeAssetSave\(StaticMesh\)\) \{[\s\S]*?TEXT\("SAVE_FAILED"\)/u);
    expect(nanite).toContain('Resp->SetBoolField(TEXT("naniteEnabled"), After.bEnabled);');
    expect(nanite).not.toContain('PositionPrecision = 8');
    expect(code('Render', 'McpRenderHandlers.cpp')).toContain('return HandleNaniteRebuildMesh(RequestId, SubAction, Payload, RequestingSocket);');
    // assetPaths does a whole pack under one consent: one row per mesh, and a failed mesh fails the call.
    expect(nanite).toContain('TEXT("assetPaths")');
    expect(nanite).toContain('TEXT("NANITE_PARTIAL")');
    // A glass slot would draw the default material under Nanite, so such a mesh is refused before it changes.
    expect(nanite.indexOf('TEXT("NANITE_UNSUPPORTED_MATERIAL")')).toBeGreaterThan(-1);
    expect(nanite.indexOf('TEXT("NANITE_UNSUPPORTED_MATERIAL")')).toBeLessThan(nanite.indexOf('StaticMesh->Build(true);'));
  });
});

describe('graph batches, PIE spawns and trigger volumes', () => {
  it('a build_graph pre-check knows the custom events earlier create_node steps make', () => {
    // A batch that created a custom event and then called it was refused with FUNCTION_NOT_FOUND before anything ran.
    const batch = code('BlueprintGraph', 'McpBPGraphBatch.cpp');
    expect(batch).toMatch(/NodeType\.Equals\(TEXT\("CustomEvent"\), ESearchCase::IgnoreCase\)[\s\S]{0,200}?Declared\.Add\(/u);
    // ...and the event exists on the skeleton class by then, as add_event's does, so the call node resolves.
    const events = code('BlueprintGraph', 'McpBPGraphCustomEvents.cpp');
    expect(events).toMatch(/EventNode->CustomFunctionName = FName\(\*EventName\);\s*Context\.FinalizeNode\(NodeCreator, EventNode, X, Y\);\s*FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified\(Context\.Blueprint\);/u);
  });

  it('a spawned static mesh actor is made movable before its mesh is set, so a spawn during PIE keeps the mesh', () => {
    const spawn = code('ControlActor', 'McpActorSpawn.cpp');
    expect(spawn.indexOf('MeshComponent->SetMobility(EComponentMobility::Movable);'))
      .toBeLessThan(spawn.indexOf('MeshComponent->SetStaticMesh(ResolvedStaticMesh);'));
  });

  it('the placement check leaves trigger and pickup volumes out of what an actor intersects', () => {
    // A pawn standing in a trigger box read as intersecting the trigger's actor.
    const shapes = code('ControlActor', 'Placement', 'McpPlacementShapes.cpp');
    expect(shapes).toContain('GetCollisionResponseToChannel(Channel) == ECR_Block');
    expect(shapes).toContain('!McpBlocksSolids(A) || !McpBlocksSolids(B)');
  });
});

describe('receipts of input edits and deletes', () => {
  // map_action saved the mapping context but its receipt listed no change: the context was named only inside nested
  // verification objects, which changes[] never reads.
  it('every Enhanced Input edit names the asset it saved in changedAssets', () => {
    const mappings = code('Input', 'McpInputMappings.cpp');
    expect(mappings).toContain('const bool bSaved = SaveLoadedAssetThrottled(Context, true);');
    expect(mappings).toContain('SetInputChangedAsset(Result, Context, bSaved);');
    expect(mappings).toContain('SetInputChangedAsset(Result, bChanged ? Context : nullptr, bSaved);');
    expect(mappings).toMatch(/Changed\.Add\(MakeShared<FJsonValueString>\(Asset->GetOutermost\(\)->GetName\(\)\)\);[\s\S]{0,120}?Result->SetArrayField\(TEXT\("changedAssets"\), Changed\);/u);
  });

  it('a trigger or modifier already there, or an input asset that already exists, is reported as no change', () => {
    const triggers = code('Input', 'McpInputTriggerModifiers.cpp');
    expect(triggers).toContain('SetInputChangedAsset(Result, bAlreadyPresent ? nullptr : InAction, bSaved);');
    expect(triggers).toContain('SetInputChangedAsset(Result, bAlreadyPresent ? nullptr : ModifiedAsset, bSaved);');
    const creation = code('Input', 'McpInputCreation.cpp');
    expect(creation).toContain('SetInputChangedAsset(Result, bChanged ? ExistingAsset : nullptr, bSaved);');
    expect(creation).toContain('bUpgrade, bSaved);');
    // Enabling a context changes the running game, not the asset.
    expect(code('Input', 'McpInputRuntimeQueries.cpp')).toMatch(/AddVerification\(Result, Context\);\s*McpHandlerUtils::MarkNoAssetsChanged\(Result\);/u);
  });

  it('an asset delete and an environment delete name what they removed', () => {
    // asset.delete of an input action answered deletedCount 1 with an empty changes[].
    const mutation = code('AssetWorkflow', 'Operations', 'McpAssetAssetMutation.cpp');
    expect(mutation).toContain('DeletedPaths.Add(FPackageName::ObjectPathToPackageName(SafePath));');
    expect(mutation).toContain('Resp->SetArrayField(TEXT("deleted"), DeletedArray);');
    expect(code('Environment', 'McpEnvBuildDeletion.cpp'))
      .toContain('McpAddStringArrayField(Context.Resp, TEXT("deleted"), DeletedTargets);');
  });

  // A folder move and a legacy input mapping did their work while their receipts listed no change: the
  // reply named the folders and the ini only in fields changes[] never reads.
  it('a folder move names both folders in modifiedPaths, once something moved', () => {
    expect(code('AssetWorkflow', 'Rename', 'McpAssetFolderMove.cpp')).toMatch(
      /if \(bMoved\)\s*\{[\s\S]{0,200}?Folders\.Add\(MakeShared<FJsonValueString>\(SourceFolder\)\);\s*Folders\.Add\(MakeShared<FJsonValueString>\(DestinationFolder\)\);\s*Result->SetArrayField\(TEXT\("modifiedPaths"\), Folders\);/u);
  });

  it('a legacy input mapping names DefaultInput.ini when it changed it, not for a mapping already there', () => {
    expect(code('Input', 'McpInputLegacyMappings.cpp')).toMatch(
      /if \(bUpdatedDefaultConfig && \(bRemove \|\| BeforeCount == 0\)\)\s*\{[\s\S]{0,200}?Paths\.Add\(MakeShared<FJsonValueString>\(TEXT\("Config\/DefaultInput\.ini"\)\)\);\s*Result->SetArrayField\(TEXT\("modifiedPaths"\), Paths\);/u);
  });
});

describe('widget input on a minimized editor', () => {
  // A minimized editor paints no widget, so a widget graph's Delay never fires: a timed widget stayed up and
  // widget_list kept listing it with no word on why.
  it('widget_list and widget_click warn while the window showing Play In Editor is minimized', () => {
    const source = code('ControlEditor', 'McpEditorWidgetInput.cpp');
    // Once a play session: every click through a menu repeated the same paragraph.
    expect(source).toContain('static TWeakObjectPtr<UWorld> WarnedWorld;');
    expect(source).toMatch(/if \(Viewport\) \{\s*Window = Viewport->GetWindow\(\);\s*\}[\s\S]{0,200}?if \(Window\.IsValid\(\) && Window->IsWindowMinimized\(\) && WarnedWorld\.Get\(\) != PlayWorld\) \{\s*WarnedWorld = PlayWorld;/u);
    expect(source).toContain('Resp->SetArrayField(TEXT("warnings"), Warnings);');
    expect(source).toMatch(/FString &Message\) \{\s*WarnWhenPieWindowMinimizedForMcp\(Resp\);\s*if \(InputType == TEXT\("widget_list"\)\)/u);
  });
});

describe('foliage edit receipts', () => {
  // Painting foliage answered changes: []: the reply named the foliage actor only as
  // foliageActorPath, which a read (get_foliage_instances) reports too.
  it('a foliage paint, add or remove names the foliage actor it changed', () => {
    expect(code('Foliage', 'McpFoliagePrivate.h')).toContain('Resp->SetArrayField(TEXT("affectedActors"), Changed);');
    expect(code('Foliage', 'McpFoliagePaint.cpp')).toContain('McpFoliageHandlers::SetFoliageActorChanged(Resp, IFA);');
    expect(code('Foliage', 'McpFoliageInsts.cpp')).toContain('McpFoliageHandlers::SetFoliageActorChanged(Resp, IFA);');
    // A removal can touch several foliage actors (one per loaded level or World Partition cell): it names each.
    expect(code('Foliage', 'McpFoliageQueries.cpp')).toMatch(/if \(Changed\.Num\(\) > 0\) \{\s*Resp->SetArrayField\(TEXT\("affectedActors"\), Changed\);\s*Transaction\.DescribeInto\(Resp\);\s*\}/u);
    expect(code('Foliage', 'McpFoliageGetInsts.cpp')).not.toContain('SetFoliageActorChanged');
  });
});

describe('Blueprint create with an unresolved parent class', () => {
  // A camera shake asked for under the wrong module (/Script/GameplayCameras.LegacyCameraShake) came out an Actor
  // Blueprint answering "Blueprint created", with only a list of shake properties that did not exist on it.
  it('a create whose named parent class resolves to nothing is refused and names classes with that name', () => {
    const parents = code('BlueprintCreation', 'McpBPCreateParentClasses.cpp');
    expect(parents).toMatch(/if \(!ResolvedParent && !Context\.ParentClassSpec\.IsEmpty\(\)\) \{\s*const FString Candidates = ClassesNamedLikeForMcp\(Context\.ParentClassSpec\);[\s\S]{0,600}?return nullptr;\s*\}/u);
    expect(code('BlueprintCreation', 'McpBPCreateAssets.cpp')).toMatch(
      /UFactory \*Factory = CreateBlueprintFactory\(Context, ParentError\);\s*if \(!Factory\) \{[\s\S]{0,200}?TEXT\("CLASS_NOT_FOUND"\)\);\s*return true;\s*\}/u);
  });
});

describe('mesh view of a placed actor', () => {
  // inspect_object objectKind mesh declares actorName, yet refused a placed StaticMeshActor as "not a static or
  // skeletal mesh"; reading a placed prop's mesh took a second lookup for its path.
  it('the mesh view follows a placed actor or component to the one mesh it draws, and names each when there are several', () => {
    const source = code('Environment', 'Inspection', 'McpEnvInspectObject.cpp');
    expect(source).toContain('UObject *Mesh = McpMeshDrawnBy(TargetObject, Several);');
    expect(source).toMatch(/if \(!Several\.IsEmpty\(\)\)\s*\{[\s\S]{0,300}?TEXT\("AMBIGUOUS_TARGET"\)\);\s*return true;\s*\}/u);
    expect(source).toContain('Resp->SetStringField(TEXT("meshOf"), MeshOf);');
    expect(source).toMatch(/#if ENGINE_MINOR_VERSION >= 1\s*EachMesh = Skeletal->GetSkeletalMeshAsset\(\);\s*#else\s*EachMesh = Skeletal->SkeletalMesh;\s*#endif/u);
  });
});

describe('multi-property reads of dotted paths', () => {
  // get_property with propertyNames TitleText.Text and SubtitleText.Text answered two rows both named "Text".
  it('a multi-property read echoes each dotted path as asked under propertyPath, keeping the resolved propertyName', () => {
    const source = code('Property', 'McpPropObjectGet.cpp');
    expect(source).toContain('Row->SetStringField(TEXT("propertyName"), Resolved);');
    expect(source).toMatch(/if \(!Wanted\.Equals\(Resolved, ESearchCase::CaseSensitive\)\) \{\s*Row->SetStringField\(TEXT\("propertyPath"\), Wanted\);\s*\}/u);
  });
});

describe('one read on several objects', () => {
  // Eight platforms' state took eight get_property calls, or a list filtered by a shared name.
  it('objectPaths runs the read on each object under a captured id and answers every one, misses included', () => {
    const source = code('Property', 'McpPropObjectGet.cpp');
    expect(source).toContain('for (const TCHAR *Field : {TEXT("objectPaths"), TEXT("actorName"), TEXT("name"), TEXT("blueprintPath")}) One->RemoveField(Field);');
    expect(source).toContain('FString::Printf(TEXT("%s#target%d"), *RequestId, Targets.Num())');
    expect(source).toContain('{TEXT("propertyName"), TEXT("value"), TEXT("properties"), TEXT("missingProperties")}');
    expect(source).toMatch(/if \(Reply\.bSuccess\) \+\+Read;\s*else Row->SetStringField\(TEXT\("error"\), Reply\.Message\);/u);
    expect(source).toContain('Data->SetArrayField(TEXT("targets"), Targets);');
  });
  // A missing object answered "Unable to find object" once per property name, each under missingProperties.
  it('an object that is not there is named once, before any property is read', () => {
    const source = code('Property', 'McpPropObjectGet.cpp');
    expect(source).toMatch(/if \(!McpHandlerUtils::ResolveObjectFromPath\(Path\)\) \{\s*Row->SetStringField\(TEXT\("error"\), McpHandlerUtils::DescribeObjectNotFound\(Path\)\);\s*Targets\.Add\(MakeShared<FJsonValueObject>\(Row\)\);\s*continue;\s*\}/u);
  });
});

describe('list reads struct members of an actor', () => {
  // list propertyNames reported Settings.BloomIntensity "missing" on a post process volume while get_property read it:
  // list only followed Component.Property, never a struct member of the actor itself.
  it('list falls back to the get_property resolver for struct members and deeper paths', () => {
    const source = code('ControlActor', 'List', 'McpActorList.cpp');
    expect(source).toMatch(/if \(FProperty \*Nested = McpResolvePropertyPath\(Actor, Wanted, Container, Resolved, Error\)\) \{\s*Properties->SetField\(Wanted, McpPropertyReflection::ExportPropertyToJsonValueOrText\(Container, Nested\)\);/u);
  });
});

describe('multi-property writes on class defaults', () => {
  // set_property properties {CharMoveComp.bCanWalkOffLedges, CharMoveComp.PerchRadiusThreshold} on a Character Blueprint recompiled and
  // saved the Blueprint while its receipt listed no change: the batch kept only actor fields from each write.
  it('a multi-property write names the asset it saved and how many placed copies took the new default', () => {
    const source = code('Property', 'McpPropObjectSetBatch.cpp');
    expect(source).toContain('TEXT("assetPath"), TEXT("materialRebuilt"), TEXT("saved"), TEXT("saveSkippedReason")}) {');
    expect(source).toContain('if (InstancesUpdated >= 0.0) Data->SetNumberField(TEXT("instancesUpdated"), InstancesUpdated);');
  });
});

describe('removing a widget subtree', () => {
  // remove_widget RootCanvas moved only the canvas out of the WidgetTree: its descendants kept their names (and the
  // class kept their variables), so adding the same names back was refused with a bare "Name is already in use."
  it('removing a widget takes its whole subtree out of the tree and refreshes the class', () => {
    const source = code('WidgetAuthoring', 'Support', 'McpWidgetManipulation.cpp');
    expect(source).toMatch(/TArray<UWidget\*> Removed\{TargetWidget\};\s*UWidgetTree::GetChildWidgets\(TargetWidget, Removed\);/u);
    expect(source).toMatch(/for \(UWidget\* Widget : Removed\)\s*\{\s*Widget->Rename\(nullptr, GetTransientPackage\(\), REN_DontCreateRedirectors \| REN_DoNotDirty\);\s*Widget->MarkAsGarbage\(\);\s*\}/u);
    expect(source).toMatch(/MarkWidgetBlueprintModifiedAndSave\(WidgetBP\);\s*WidgetAuthoringHelpers::RefreshWidgetBlueprintClass\(WidgetBP\);\s*ResultJson->SetBoolField\(TEXT\("success"\), true\);\s*ResultJson->SetStringField\(TEXT\("widgetPath"\), WidgetPath\);\s*ResultJson->SetStringField\(TEXT\("removedWidget"\)/u);
  });

  it('a name the Blueprint refuses for a new widget is named in the refusal', () => {
    const source = code('WidgetAuthoring', 'Support', 'McpWidgetValidate.cpp');
    expect(source).toContain('FString Why = INameValidatorInterface::GetErrorString(Name.ToString(), Result);');
    expect(source, 'its full stop goes, since every refusal goes on after it').toContain('Why.RemoveFromEnd(TEXT("."));');
    expect(source).toMatch(/return FString::Printf\(TEXT\("'%s': %s"\), \*Name\.ToString\(\), \*Why\);/u);
  });
});

describe('simulated keys and focused PIE widgets', () => {
  // simulate_input key_tap Enter in PIE went straight to the viewport client, so a focused menu button never saw it
  // (handledByPIE false, handledBySlate false) while a real Enter pressed it.
  it('a key goes through the Slate focus path while a widget inside the PIE viewport holds focus', () => {
    const widgetInput = code('ControlEditor', 'McpEditorWidgetInput.cpp');
    expect(widgetInput).toMatch(/if \(!ViewportWidget\.IsValid\(\) \|\| !SlateApp\.HasUserFocusedDescendants\(ViewportWidget\.ToSharedRef\(\), 0\)\) \{\s*return false;\s*\}/u);
    expect(widgetInput).toContain('bOutHandled = InputEvent == IE_Released ? SlateApp.ProcessKeyUpEvent(KeyEvent) : SlateApp.ProcessKeyDownEvent(KeyEvent);');
    const routing = code('ControlEditor', 'McpEditorInputRouting.cpp');
    expect(routing).toMatch(/bRoutedToPIE = !bWorldGone && Target != EMcpKeyTarget::Editor &&\s*\(RouteKeyToFocusedPieWidgetForMcp\(InputKey, InputEvent, bHandledBySlate, FocusNote\) \|\|\s*RouteKeyToPIEForMcp\(InputKey, InputEvent, bHandledByPIE\)\);/u);
  });

  // A key no play session took went to the focused editor panel, sample_motion's inputs (declared read) too, where
  // Delete or End runs that panel's command; a key held into the game and released after Stop let go there as well.
  it('hands a key to the editor only when the caller allows it, never from sample_motion or a game key\'s release', () => {
    expect(code('ControlEditor', 'McpEditorInputRouting.cpp')).toMatch(/const bool bToEditor = !bRoutedToPIE && Target != EMcpKeyTarget::Game &&\s*!\(Target == EMcpKeyTarget::Editor && PieViewportHasFocusForMcp\(\)\);\s*if \(bToEditor\) \{/u);
    expect(code('ControlEditor', 'McpEditorSupport.h')).toContain('FString &Message, EMcpKeyTarget Target = EMcpKeyTarget::Any);');
    const motion = code('ControlActor', 'McpActorMotionInputs.cpp');
    expect(motion).toContain('MakeShared<FJsonObject>(), bOk, bRouted, bPIE, bSlate, Message, EMcpKeyTarget::Game);');
    expect(motion).toContain('Input.bRouted = McpSendMotionKey(Input.Key, true);');
    expect(motion).toContain('Entry->SetBoolField(TEXT("routedToPIE"), false);');
    expect(code('ControlEditor', 'McpEditorInput.cpp')).toContain('ScheduleKeyReleaseForMcp(Key, HoldSeconds, bRoutedToPIE);');
  });

  // A release went to whatever took keys when it fired: a key pressed in the editor let go into a game started since,
  // and a key pressed into a game let go into the world that replaced it (a level reload).
  it('lets a held key go only where it went down, and not at all once that play world is gone', () => {
    const input = code('ControlEditor', 'McpEditorInput.cpp');
    const drop = input.indexOf('if (!bPressRouted || !bWorldGone) {');
    expect(drop, 'a game press whose world stopped or was replaced sends no release').toBeGreaterThan(-1);
    expect(input.indexOf('bPressRouted ? EMcpKeyTarget::Game : EMcpKeyTarget::Editor', drop)).toBeGreaterThan(drop);
    // An editor release never reaches the game through Slate once the play viewport has focus.
    const routing = code('ControlEditor', 'McpEditorInputRouting.cpp');
    expect(routing).toContain('!(Target == EMcpKeyTarget::Editor && PieViewportHasFocusForMcp())');
    expect(code('ControlEditor', 'McpEditorWidgetInput.cpp')).toContain('ViewportWidget->HasAnyUserFocusOrFocusedDescendants()');
    // A key_up the caller sends itself names no target: it goes where that key went down, chosen before routing.
    const recall = routing.indexOf('const FPress Press = bRecall ? PressedTo.FindRef(Key) : FPress{Target, PlayWorld};');
    expect(recall).toBeGreaterThan(-1);
    expect(routing).toContain('const bool bRecall = InputEvent == IE_Released && Target == EMcpKeyTarget::Any;');
    expect(recall, 'the press destination is applied before the key is routed').toBeLessThan(routing.indexOf('bRoutedToPIE = !bWorldGone && Target != EMcpKeyTarget::Editor &&'));
    // A game press keeps its play world: after Stop and a new Play, its release reaches neither session.
    expect(routing).toContain('const bool bWorldGone = Press.Target == EMcpKeyTarget::Game && Press.World.Get() != PlayWorld;');
    expect(routing).toContain('PressedTo.Add(Key, bRoutedToPIE ? FPress{EMcpKeyTarget::Game, PlayWorld} : FPress{EMcpKeyTarget::Editor});');
    const motion = code('ControlActor', 'McpActorMotionInputs.cpp');
    expect(motion).toContain('Input.PressWorld = GEditor ? GEditor->PlayWorld.Get() : nullptr;');
    expect(motion).toMatch(/if \(Input\.bRouted && Input\.PressWorld\.IsValid\(\) && GEditor && Input\.PressWorld\.Get\(\) == GEditor->PlayWorld\.Get\(\)\) \{\s*McpSendMotionKey\(Input\.Key, false\);/u);
  });
});

describe('Blueprints that depend on a changed Widget Blueprint', () => {
  // After a widget was removed and added back, a game mode focusing it (Set Input Mode UI Only) failed every play
  // with "Attempted to access missing property 'none'" while it read up to date: the engine only relinks a
  // dependent by name, and the removal left it pointing at nothing.
  it('a widget class refresh compiles every Blueprint that depends on it in full', () => {
    const source = code('WidgetAuthoring', 'Support', 'McpWidgetLoading.cpp');
    expect(source).toMatch(/const bool bCompiled = McpSafeCompileBlueprint\(WidgetBP\);\s*TArray<UBlueprint\*> Dependents;\s*FBlueprintEditorUtils::GetDependentBlueprints\(WidgetBP, Dependents\);\s*for \(UBlueprint\* Dependent : Dependents\)\s*\{\s*McpSafeCompileBlueprint\(Dependent\);\s*\}\s*return bCompiled;/u);
  });
});

describe('connect_pins on pins added on demand', () => {
  // "FromNode 'K2Node_CallFunction_3' pins: execute, ..., Key, . ToNode ...": both nodes, hidden pins, a dangling comma.
  it('PIN_NOT_FOUND names the pin that missed and joins the visible pins of its node', () => {
    const pins = code('BlueprintGraph', 'McpBPGraphPinMutations.cpp');
    expect(pins).toContain('TEXT("No %s pin \'%s\' on \'%s\'. Its pins: %s.")');
    expect(pins).toContain('if (Pin && !Pin->bHidden)');
    expect(pins, 'no list with a dangling separator, no log copy').not.toContain('FromPinsList');
  });

  // A build_graph step connecting a Sequence's then_2 stopped the batch with PIN_NOT_FOUND ("pins: execute, then_0,
  // then_1"): the editor adds that pin on demand, the tool never did.
  it('connect_pins grows a node that adds pins on demand to the pin it names, and takes back pins added in vain', () => {
    const growth = code('BlueprintGraph', 'PinMutations', 'McpBPGraphPinGrowth.cpp');
    expect(growth).toMatch(/IK2Node_AddPinInterface\* Growable = Cast<IK2Node_AddPinInterface>\(Node\)/u);
    expect(growth).toMatch(/Growable->AddInputPin\(\);\s*Pin = Context\.FindPin\(Node, PinName\);/u);
    expect(growth).toMatch(/if \(!Pin && !Before\.Contains\(Extra\)\) Growable->RemoveInputPin\(Extra\);/u);
    const source = code('BlueprintGraph', 'McpBPGraphPinMutations.cpp');
    expect(source).toContain('UEdGraphPin* FromPin = FindOrGrowPin(Context, FromNode, FromPinName);');
    expect(source).toContain('UEdGraphPin* ToPin = FindOrGrowPin(Context, ToNode, ToPinName);');
  });

  // A batch link the engine made through an int-to-float conversion node answered connected false, as if it failed.
  it('counts a link through the inserted conversion node as connected', () => {
    const source = code('BlueprintGraph', 'McpBPGraphPinMutations.cpp');
    expect(source).toContain('Result->SetBoolField(TEXT("connected"), FromPin->LinkedTo.Contains(ToPin) || (LinkedToNode(FromPin) && LinkedToNode(ToPin)));');
  });

  // nodeType GetAllActorsOfClass, listed by create_node, answered NODE_TYPE_NOT_FOUND: it was aliased to a K2Node
  // class Unreal does not have, while the node is a plain call of the GameplayStatics function.
  it('GetAllActorsOfClass and GetActorOfClass create function calls', () => {
    const catalog = code('BlueprintGraph', 'McpBPGraphNodeCatalog.cpp');
    expect(catalog).toMatch(/\{TEXT\("GetAllActorsOfClass"\),\s*MakeTuple\(TEXT\("UGameplayStatics"\), TEXT\("GetAllActorsOfClass"\)\)\}/u);
    expect(catalog).toMatch(/\{TEXT\("GetActorOfClass"\),\s*MakeTuple\(TEXT\("UGameplayStatics"\), TEXT\("GetActorOfClass"\)\)\}/u);
    expect(catalog).not.toContain('K2Node_GetAllActorsOfClass');
  });

  // A Switch on Int made over MCP had only its Default pin: a link to case 17 answered PIN_NOT_FOUND.
  it('a Switch on Int grows to the case a link names, and takes back cases added in vain', () => {
    const growth = code('BlueprintGraph', 'PinMutations', 'McpBPGraphPinGrowth.cpp');
    expect(growth).toMatch(/Switch->AddPinToSwitchNode\(\);\s*Pin = Context\.FindPin\(Node, PinName\);/u);
    expect(growth).toMatch(/if \(!Pin && !Before\.Contains\(Extra\)\) Switch->RemovePinFromSwitchNode\(Extra\);/u);
  });
});

describe('a spawnable added from a class', () => {
  // add_spawnable stored the class default object as the template; a recompile of the Blueprint replaced it and
  // the binding spawned nothing ("does not have a valid object template").
  it('gets a template the sequence owns', () => {
    const source = code('Sequence', 'McpSeqSpawnables.cpp');
    expect(source).toContain('static_cast<UMovieSceneSequence *>(LevelSeq)->CreateSpawnable(ResolvedClass)');
    expect(source).not.toContain('GetDefaultObject()');
  });
});

describe('sequence duplicates and the render queue', () => {
  // A duplicated sequence stayed in memory only, while a created one was saved: an editor exit lost the copies.
  it('a duplicated sequence is saved like a created one', () => {
    const source = code('Sequence', 'McpSeqAssetLibrary.cpp');
    expect(source).toMatch(/if \(DuplicatedSeq\) \{[^}]*McpSafeAssetSave\(DuplicatedSeq\);/u);
  });

  // A render whose game loaded another level hung past its deadline and kept the queue busy until the editor
  // closed: once the cancel wait runs out, its Play In Editor session is ended.
  it('a render still running after the cancel wait has its Play In Editor session ended', () => {
    const source = code('Sequence', 'MovieRender', 'McpSeqMRQCompletion.cpp');
    expect(source).toMatch(/State->bCancellationDeadlineExpired && Cast<UMoviePipelinePIEExecutor>\(Executor\) &&\s*GEditor && GEditor->PlayWorld\)\s*GEditor->RequestEndPlayMap\(\);/u);
  });

  // The queue keeps every job (a finished one renders again with the next unrestricted start) and refuses
  // new ones past its limit; nothing could take a job out, so a long session hit the limit for good.
  it('render jobs can be removed, one or all, but not mid-render', () => {
    const source = code('Sequence', 'MovieRender', 'McpSeqMRQJobCreation.cpp');
    expect(source).toContain('Queue->DeleteAllJobs();');
    expect(source).toContain('Queue->DeleteJob(Job);');
    expect(source).toMatch(/QueueSubsystem->GetActiveExecutor\(\) \|\| QueueSubsystem->IsRendering\(\)\)\s*return SendError/u);
    const routing = code('Sequence', 'MovieRender', 'McpSeqMRQRouting.cpp');
    expect(routing).toContain('{TEXT("remove_render_job"), &HandleRemoveRenderJob}');
  });

  // "MRQ render pass configured." named neither the pass nor the job, so a caller could not tell the ui pass landed.
  it('add_render_pass names the passes it added and the job', () => {
    const source = code('Sequence', 'MovieRender', 'McpSeqMRQPasses.cpp');
    expect(source).toContain('TEXT("Render pass %s added to %s."),');
    expect(source).toContain('*FString::Join(Passes, TEXT(", ")), *Job->JobName)');
  });

  // create took path as a folder only, while every other sequence action reads it as the sequence's own path: a
  // full path with no name answered "sequence_create requires name".
  it('create reads a path that is no folder as the new sequence itself', () => {
    const source = code('Sequence', 'McpSeqAssetCreation.cpp');
    expect(source).toMatch(/if \(!Probe\.IsEmpty\(\) && !DoesAssetDirectoryExistOnDisk\(Probe\)\) \{\s*Name = FPackageName::GetShortName\(Probe\);\s*Folder = FPackageName::GetLongPackagePath\(Probe\);/u);
    expect(source.indexOf('DoesAssetDirectoryExistOnDisk(Probe)')).toBeLessThan(source.indexOf('sequence_create requires name'));
  });
});

describe('tapped keys while the game is paused', () => {
  // key_tap Enter reached a focused menu button that never clicked: the release waited on game time, which a pause
  // menu stops, so the key stayed down for the 600 s grace.
  it('a tapped key is released on wall time while the game paused itself', () => {
    const source = code('ControlEditor', 'McpEditorInput.cpp');
    expect(source).toContain('const bool bGameClock = bGameTime && !bWorldGone && !(Settings && Settings->GetPauserPlayerState());');
    expect(source).toContain('(bGameClock ? Live->GetTimeSeconds() < EndGameTime : FPlatformTime::Seconds() < EndHoldWall)');
  });

  // A jump held for 0.2 s across stepped frames let go on the wall clock before the first step: the editor's pause,
  // which step_frame runs game time through, was read as a menu's.
  it('a held key counts game time under the editor pause and says so', () => {
    const source = code('ControlEditor', 'McpEditorInput.cpp');
    expect(source).not.toContain('!Live->IsPaused()');
    expect(source).toContain('PlayWorld && PlayWorld->bDebugPauseExecution');
    expect(source).toContain('editor\'s pause until step_frame or resume runs it');
  });
});

describe('tapped keys with no play session', () => {
  // A key_tap sent with no PIE running crashed the editor on the next tick: its release read the world settings of a
  // play world that was null, since a key pressed outside PIE never counts game time.
  it('the release of an editor key reads no play world', () => {
    // Up to the function's own closing brace: code() strips the "// namespace" after it.
    const source = sliceBetween(code('ControlEditor', 'McpEditorInput.cpp'), 'void ScheduleKeyReleaseForMcp(', '\n}');
    expect(countPureLines(source)).toBeLessThan(60); // the function alone, never the rest of the file
    expect(source).toContain('const AWorldSettings *Settings = (bGameTime && !bWorldGone) ? Live->GetWorldSettings() : nullptr;');
    expect(source).not.toContain('bWorldGone ? nullptr : Live->GetWorldSettings()');
  });
});

describe('actors moved from code', () => {
  // After set_transform moved and scaled a ground plane, foliage painted past its old end found no ground and
  // raycast_world missed it there: in the editor world a body moved from code kept its collision queries on the old
  // spot, so only where the old and new footprints overlapped did a trace hit.
  it('a move made from code ends as the editor ends its own moves, with its bodies rebuilt where it stands', () => {
    const helper = code('..', 'Foundation', 'BridgeHelpers', 'Actors', 'McpActorMove.h');
    expect(helper).toMatch(/if \(!World \|\| World->IsGameWorld\(\)\)\s*return;\s*Actor->PostEditMove\(true\);/u);
    expect(helper).toMatch(/if \(Primitive->IsPhysicsStateCreated\(\)\)\s*Primitive->RecreatePhysicsState\(\);/u);
    expect(code('ControlActor', 'McpActorTransform.cpp'))
      .toMatch(/Found->SetActorScale3D\(Scale\);\s*McpFinishEditorMove\(Found\);/u);
    expect(code('ControlActor', 'McpActorSnapshots.cpp'))
      .toMatch(/Found->SetActorTransform\(SavedTransform\);\s*McpFinishEditorMove\(Found\);/u);
    expect(code('ControlActor', 'Placement', 'McpCoplanarFix.cpp'))
      .toMatch(/ETeleportType::TeleportPhysics\);\s*McpFinishEditorMove\(Actor\);/u);
    const property = code('Property', 'McpPropActorAccess.cpp');
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
    const source = code('MaterialAuthoring', 'Connections', 'McpMatConnectNodes.cpp');
    expect(source).not.toContain('CustomSource->AdditionalOutputs');
    expect(source).toMatch(/if \(SourceOutputIndex == INDEX_NONE && Outputs\.Num\(\) > 0 && ParseChannelMask\(SourcePin, Channels\)\)/u);
  });

  // add_widget_tree took a texture but no material, so a panel drawn by a UI material needed a second call to set
  // its brush by reflection.
  it('a widget tree node takes a material as an Image or Border brush', () => {
    const tree = code('WidgetAuthoring', 'Templates', 'McpWidgetTreeBuild.cpp');
    expect(tree).toContain('TEXT("shadowColor"), TEXT("material")};');
    expect(tree).toMatch(/if \(!Material\.IsEmpty\(\) && Type != TEXT\("Image"\) && Type != TEXT\("Border"\)\)/u);
    expect(tree).toContain('if (!Material.IsEmpty() && !McpLoadSpecMaterial(Material))');
    const props = code('WidgetAuthoring', 'Templates', 'McpWidgetSpecProps.cpp');
    expect(props).toMatch(/Brush\.SetResourceObject\(Material\);\s*Brush\.DrawAs = ESlateBrushDrawType::Image;/u);
    expect(props).toContain('if (UBorder* Border = Cast<UBorder>(Widget)) { Border->SetBrush(Brush); Border->SetBrushColor(FLinearColor::White); }');
    expect(props).toContain('if (UImage* MaterialImage = Cast<UImage>(Widget)) { MaterialImage->SetBrush(Brush); }');
  });

  // create_material with materialDomain "UserInterface" (the Details panel says "User Interface") was refused,
  // and the refused call had already made the material, so the retry with "UI" hit "already exists".
  it('create_material checks its enum settings before it makes anything, and takes the Details panel labels', () => {
    const create = code('MaterialAuthoring', 'Creation', 'McpMatCreateMat.cpp');
    const check = create.indexOf('if (!ApplyMaterialEnumFields(nullptr, Payload, EnumError))');
    expect(check).toBeGreaterThan(-1);
    expect(check).toBeLessThan(create.indexOf('CreatePackage('));
    expect(check).toBeLessThan(create.indexOf('PrepareNewMaterialAsset('));
    const setters = code('MaterialAuthoring', 'Properties', 'McpMatSetMatEnum.cpp');
    expect(setters.match(/if \(Material\) Material->/gu)).toHaveLength(3);
    const parse = code('MaterialAuthoring', 'Properties', 'McpMatEnumParsing.cpp');
    expect(parse).toContain('Enum->GetDisplayNameTextByIndex(Index).ToString().Replace(TEXT(" "), TEXT(""))');
    expect(parse).toContain('Wanted.Equals(Label, ESearchCase::IgnoreCase)');
  });

  // A widget animation took one call per key.
  it('add_animation_keyframe writes a keys batch, checking every key before writing any', () => {
    const handler = code('WidgetAuthoring', 'Animation', 'McpWidgetAnimKeyframe.cpp');
    expect(handler).toContain('Payload->TryGetArrayField(TEXT("keys"), KeyList)');
    expect(handler).toContain('Merged->RemoveField(TEXT("keys"));');
    expect(handler).toContain('FString(Pair.Key.Len(), *Pair.Key)');
    const checked = handler.indexOf(': KeyRefusal(Keys[Index]);');
    expect(checked).toBeGreaterThan(-1);
    expect(checked).toBeLessThan(handler.indexOf('McpAuthorWidgetAnimationKey('));
    expect(handler).toContain('ResultJson->SetNumberField(TEXT("keysAdded"), Keys.Num());');
    const keys = code('WidgetAuthoring', 'Support', 'McpWidgetAnimKeys.h');
    expect(keys).toContain('FString KeyValueError(const FString& Kind, const FString& TrackType, const TSharedPtr<FJsonValue>& Value);');
  });

  // Wiring a new animation into PlayAnimation stopped with "Variable not found": the animation was only a property
  // of the generated class after the next compile.
  it('create_widget_animation compiles the Widget Blueprint so a graph can read the animation at once', () => {
    const core = code('WidgetAuthoring', 'Animation', 'McpWidgetAnimCore.cpp');
    expect(core).toMatch(/RegisterAnimationGuid\(WidgetBP, NewAnim\);\s*FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified\(WidgetBP\);\s*RefreshWidgetBlueprintClass\(WidgetBP\);\s*ResultJson->SetBoolField\(TEXT\("saved"\), McpSafeAssetSave\(WidgetBP\)\);/u);
  });

  // Re-keying a frame answered "4 keys in the track" for a 3-key track: the new key went in beside the old one on
  // the same frame.
  it('a widget animation key on an occupied frame replaces the key there', () => {
    const keys = code('WidgetAuthoring', 'Support', 'McpWidgetAnimKeysInternal.h');
    expect(keys).toMatch(/for \(int32 Existing = Channel->GetData\(\)\.FindKey\(Frame\); Existing != INDEX_NONE; Existing = Channel->GetData\(\)\.FindKey\(Frame\)\)\s*\{\s*Channel->GetData\(\)\.RemoveKey\(Existing\);\s*\}\s*const float FloatValue/u);
  });
});

describe('Make and Break nodes of structs with native functions', () => {
  // Breaking a velocity with create_node BreakStruct (structPath Vector) compiled with "The structure cannot be
  // broken using generic 'break' node Break Vector": the editor breaks such a struct with its native function.
  it('a Make or Break of a struct with a native make or break builds that function node', () => {
    const dynamic = code('BlueprintGraph', 'McpBPGraphNodeCreationDynamic.cpp');
    expect(dynamic).toContain('NodeClass->IsChildOf(UK2Node_MakeStruct::StaticClass()) ? TEXT("HasNativeMake") : TEXT("HasNativeBreak")');
    expect(dynamic).toMatch(/FindObject<UFunction>\(nullptr, \*Native\)\)\s*\{\s*FGraphNodeCreator<UK2Node_CallFunction> NativeCreator\(\*Context\.TargetGraph\);/u);
    expect(dynamic.indexOf('HasNativeBreak')).toBeLessThan(dynamic.indexOf('NewObject<UEdGraphNode>(Context.TargetGraph, NodeClass)'));
  });
});

describe('opening a level answers once the level has settled', () => {
  // A screenshot right after open_level showed placeholder textures and editor icons: the reply came the moment the
  // load returned, and the load had switched the viewport's game view off.
  it('both load paths reply through the settle wait and keep game view', () => {
    const open = code('ControlEditor', 'McpEditorLevel.cpp');
    expect(open).toContain('McpReplyWhenLevelSettles(WeakThis, Socket, RequestId, TEXT("Level opened"), Resp, bWasGameView);');
    expect(open).toContain('const int32 Streaming = IStreamingManager::Get().GetNumWantingResources();');
    expect(open).toContain('View->SetGameView(true);');
    const load = code('Level', 'Lifecycle', 'McpLevelLoad.cpp');
    expect(load).toContain('McpReplyWhenLevelSettles(TWeakObjectPtr<UEngineWireSubsystem>(&Subsystem), RequestingSocket, RequestId,');
  });
});

describe('get_widget_animation reads what an animation does', () => {
  // A widget animation could be built key by key but read back by name only, and its track count read 0: the
  // tracks live on the widgets the animation binds, not in its unbound list.
  it('walks the bindings for the widget, its tracks and every channel key', () => {
    const queries = code('WidgetAuthoring', 'Animation', 'McpWidgetAnimQueries.cpp');
    expect(queries).toContain('for (const FMovieSceneBinding& Binding : Anim->MovieScene->GetBindings())');
    expect(queries).toContain('TrackCount += Binding.GetTracks().Num();');
    expect(queries).toContain('Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(Index)');
    expect(queries).toContain('TEXT("translation.x"), TEXT("translation.y"), TEXT("angle"), TEXT("scale.x")');
  });
});

describe('configure_rendering', () => {
  // set_project_setting took raw property names and enum spellings and never said a change waited for a restart.
  it('checks every value before writing any, applies the console variable live and names restart-only changes', () => {
    const render = code('Ui', 'McpUiRenderSettings.cpp');
    expect(render.indexOf('Writes.Add(Write);')).toBeLessThan(render.indexOf('McpWriteSetting(Write.Property, Settings, Write.Value);'));
    expect(render).toContain('McpSetConsoleVariable(*Variable, LexToString(Write.Value), ECVF_SetByProjectSetting)');
    expect(render).toContain('Write.Property->HasMetaData(TEXT("ConfigRestartRequired"))');
    expect(render).toContain('Settings->TryUpdateDefaultConfigFile(FString(), false)');
    // The settings object's own change handler can raise dialogs no one is there to answer.
    expect(render).not.toContain('PostEditChangeProperty');
  });
});

describe('skeletal mesh import', () => {
  // A skeleton could only take an animation alone: a character could not be imported onto an existing skeleton,
  // nor its physics asset or morph targets chosen, and the reply named whichever asset the importer listed first.
  it('binds the mesh to a named skeleton with importMesh and takes the physics and morph choices', () => {
    const options = code('AssetWorkflow', 'Operations', 'McpAssetFbxImportOptions.h');
    expect(options).toContain('const bool bTakeAlone = Skel != nullptr && !Options.bImportMesh;');
    expect(options).toContain('Fbx->ImportUI->bCreatePhysicsAsset = Physics == nullptr && Options.CreatePhysicsAsset.Get(true);');
    expect(options).toContain('Fbx->ImportUI->SkeletalMeshImportData->bImportMorphTargets = Options.ImportMorphTargets.GetValue();');
    expect(options).toContain('OutErrorCode = TEXT("PHYSICS_ASSET_NOT_FOUND");');
  });

  it('answers with the mesh and what it is bound to, not the first asset listed', () => {
    const options = code('AssetWorkflow', 'Operations', 'McpAssetFbxImportOptions.h');
    expect(options).toContain('for (UClass *Kind : {USkeletalMesh::StaticClass(), UStaticMesh::StaticClass(), UAnimSequence::StaticClass(), UObject::StaticClass()})');
    const handler = code('AssetWorkflow', 'Operations', 'McpAssetImportDuplicate.cpp');
    expect(handler).toContain('UObject *Asset = McpPickPrimaryImport(ImportedAssets);');
    expect(handler).toContain('McpDescribeImport(ImportedAssets, Asset, ImportedAsset, Resp);');
  });
});

describe('analyze_animation reads a sequence from its own frames', () => {
  // get_animation_info gave length, notifies and curves only; how a clip moves could not be read without playing it.
  it('samples every frame up to the cap, the first and last always in, with the root motion left on the root', () => {
    const analysis = code('AnimationAuthoring', 'Analysis', 'McpAnimAnalysis.cpp');
    expect(analysis).toContain('const int32 Frame = FMath::RoundToInt(double(Sample) * (Keys - 1) / (Count - 1));');
    expect(analysis).toContain('Options.bExtractRootMotion = false;');
    expect(analysis).toContain('UAnimPoseExtensions::GetAnimPoseAtFrame(Sequence, Frame, Options, Pose);');
    // A foot is planted within contactHeight of its lowest point; a montage is refused, not half-read.
    expect(analysis).toContain('Path[Index].Z <= Lowest + ContactHeight');
    expect(analysis).toContain('TEXT("NOT_A_SEQUENCE")');
    expect(analysis).toContain('McpHandlerUtils::MarkNoAssetsChanged(Response);');
  });
});

describe('animation graph nodes by class', () => {
  // create_node took only a full AnimGraphNode_ class name, and put a Sequence Player in an EventGraph, where it
  // compiled clean and never ran.
  it('a short name resolves to the animation node after every Blueprint spelling', () => {
    const catalog = code('BlueprintGraph', 'McpBPGraphNodeCatalog.cpp');
    expect(catalog).toContain('return Found ? Found : FindClass(TArray<FString>{Compact, FString::Printf(TEXT("AnimGraphNode_%s"), *Compact)});');
    expect(catalog).toContain('!DisplayName.Replace(TEXT(" "), TEXT("")).Contains(Filter)');
  });

  it('both create doors refuse a node the graph cannot hold', () => {
    const dynamic = code('BlueprintGraph', 'McpBPGraphNodeCreationDynamic.cpp');
    expect(dynamic).toContain('NodeClass->GetDefaultObject<UEdGraphNode>()->CanCreateUnderSpecifiedSchema(Schema)');
    expect(dynamic.indexOf('DescribeGraphMismatch(NodeClass, Context.TargetGraph)')).toBeLessThan(dynamic.indexOf('NewObject<UEdGraphNode>(Context.TargetGraph, NodeClass)'));
    const addNode = code('Blueprint', 'Graph', 'McpBPAddNodeGraph.cpp');
    expect(addNode).toContain('McpBlueprintGraphHandlers::DescribeGraphMismatch(NodeClass, TargetGraph)');
    expect(addNode).toContain('OutErrorCode = TEXT("NODE_NOT_ALLOWED_IN_GRAPH");');
  });
});

describe('asset queries point at what they found', () => {
  // Searching for a sound found nothing although a PlaySound2D node played it: an asset picked on a pin is
  // DefaultObject, which the scan never read.
  it('find_text matches the asset picked on a graph pin by its path', () => {
    const scan = code('AssetQuery', 'McpAssetQueryFindText.cpp');
    expect(scan).toContain('Reference(Asset, Where, Pin->PinName.ToString(), Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : FString());');
    expect(scan).toMatch(/void Reference\(const FString& Asset, const FString& Where, const FString& Field, const FString& Path\)\s*\{\s*if \(!Path\.IsEmpty\(\) && !Path\.StartsWith\(TEXT\("\/Script\/"\)\)/u);
  });

  // "Which Blueprints call StartCameraFade" found nothing: node titles were never searched. A call node's list title is
  // its function's name, so "Play Sound at Location" found no call until the title the graph shows was matched too.
  it('find_text matches a graph node by its title', () => {
    const scan = code('AssetQuery', 'McpAssetQueryFindText.cpp');
    expect(scan).toContain('const FString ListTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();');
    expect(scan).toContain('Hit(Asset, Where, TEXT("node"), ListTitle.Contains(Query, Case) ? ListTitle : Node->GetNodeTitle(ENodeTitleType::FullTitle)');
  });

  // lookup=graph on a MetaSound answered "Asset does not have a graph structure".
  it('analyze_graph names the action that reads a MetaSound, Niagara or Behavior Tree graph', () => {
    const report = code('AssetWorkflow', 'Analysis', 'McpAssetGraphReport.cpp');
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
    const add = code('Blueprint', 'Functions', 'McpBPAddFunction.cpp');
    expect(add).toMatch(/AddFunctionGraph<UClass>\(Blueprint, NewGraph,\s*OverrideClass == nullptr, OverrideClass\);/u);
    expect(add).not.toContain('CreateFunctionGraph<UFunction>');
    const resolve = code('Blueprint', 'Functions', 'McpBPAddFunctionResult.cpp');
    expect(resolve).toContain('Owner == Blueprint->GeneratedClass || !UEdGraphSchema_K2::CanKismetOverrideFunction(Parent)');
    expect(resolve).toContain('if (UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Parent))');
    expect(resolve).toContain('Resp->SetStringField(TEXT("overrides"), OverrideClass->GetName() + TEXT("::") + FuncName);');
  });
});

describe('start minimized', () => {
  // The editor came up already minimized, so the start-minimized ticker never saw it on screen, kept waiting, and
  // put away the first restore made over the bridge minutes later.
  it('the start-minimized hold begins the first time the window is on screen, minimized or not', () => {
    const lifecycle = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Core', 'Subsystem',
      'EngineWireSubsystemLifecycle.cpp'), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
    expect(lifecycle).toMatch(/Root->IsVisible\(\)\)\s*\{\s*if \(!Root->IsWindowMinimized\(\)\)\s*\{\s*MinimizeWindowForMcp\(Root\.ToSharedRef\(\)\);\s*\}\s*if \(\*HoldUntil == 0\.0\)/u);
  });

  // A restore asked for 3 minutes after launch was put away again within seconds; one 23 s later stayed up.
  it('a restore made over the bridge ends the hold, and a timed run leaves a window restored meanwhile up', () => {
    const lifecycle = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Core', 'Subsystem',
      'EngineWireSubsystemLifecycle.cpp'), 'utf8');
    expect(lifecycle).toMatch(/CreateLambda\(\[GiveUpAt, HoldUntil\]\(float\)\s*\{\s*if \(McpWindowRestoreCount\(\) != 0\)\s*\{\s*return false;/u);
    const windows = code('ControlEditor', 'McpEditorScreenshotWindows.cpp');
    expect(windows).toMatch(/::SetWindowPlacement\(Hwnd, &Placement\);\s*\}\s*\+\+GMcpWindowRestoreCount;/u);
    expect(windows).toMatch(/\} else \{\s*\+\+GMcpWindowRestoreCount;\s*RestoreWindowForCaptureForMcp\(Root\.ToSharedRef\(\)\);/u);
    expect(windows).toContain('Hold.RestoreCount = GMcpWindowRestoreCount;');
    expect(windows).toContain('if (Hold.bWindowRestored && Hold.RestoreCount == GMcpWindowRestoreCount) {');
  });
});

describe('save game slots', () => {
  // The slot name becomes the .sav file name, so a separator or ".." would reach a file outside SaveGames.
  it('a slot name cannot leave Saved/SaveGames', () => {
    const saves = code('SystemControl', 'McpSystemSaveGames.cpp');
    for (const refused of ['TEXT("..")', 'TEXT("/")', 'TEXT("\\\\")', 'TEXT(":")']) {
      expect(saves).toContain(`OutSlot.Contains(${refused})`);
    }
    expect(saves.match(/ReadSaveSlot\(Payload, Slot, User, Error\)/gu)).toHaveLength(2);
  });

  // A slot is written only after every requested property resolved and converted, and the reply is read back from disk.
  it('edit_save_game writes after the properties apply and answers what it reloads', () => {
    const saves = code('SystemControl', 'McpSystemSaveGames.cpp');
    expect(saves.indexOf('ApplySavedProperties(Self, RequestId, RequestingSocket, Save, *Props)'))
      .toBeLessThan(saves.indexOf('UGameplayStatics::SaveGameToSlot(Save, Slot, User)'));
    expect(saves).toContain('USaveGame* Reloaded = bSaved ? UGameplayStatics::LoadGameFromSlot(Slot, User) : nullptr;');
    expect(saves).toContain('NewClass->HasAnyClassFlags(CLASS_Abstract)');
  });
});

describe('search scope and mesh-particle materials', () => {
  // find_text narrowed to /Game/UI still read every actor of the open level elsewhere: 148 hits, 3 of them asked for.
  it('find_text reads the open level only under the searched paths unless includeLevel says otherwise', () => {
    expect(code('AssetQuery', 'McpAssetQueryFindText.cpp'))
      .toContain('if (World && FindTextIncludesLevel(Payload, Filter.PackagePaths, World))');
    const scope = code('AssetQuery', 'McpAssetQueryPrivate.h');
    expect(scope).toContain('if (Payload->TryGetBoolField(TEXT("includeLevel"), bInclude))');
    expect(scope).toContain('Level.StartsWith(Path.ToString() + TEXT("/"))');
  });

  // Sprite and ribbon materials got their Niagara usage flag saved; a mesh renderer's mesh materials did not.
  it('add_mesh_renderer_module flags the mesh materials for mesh particles', () => {
    expect(code('NiagaraAuthoring', 'McpNiagaraRenderers.cpp'))
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
    const resolver = readFileSync(join(DOMAINS, 'MaterialAuthoring', 'McpMatBatchSteps.h'), 'utf8');
    const named = Object.fromEntries([...resolver.matchAll(/Kind == TEXT\("(\w+)"\)\)\s*\{\s*return TEXT\("(\w+)"\);/gu)].map((m) => [m[1], m[2]]));
    const resolve = (kind: string): string => (kind === 'node' ? 'add_material_node' : kind === 'batch' ? 'build_material_graph' : named[kind] ?? `add_${kind}`);
    expect(resolver).toMatch(/if \(Kind == TEXT\("node"\)\)\s*\{\s*return Edit;/u);
    expect(resolver).toContain('return Kind == TEXT("batch") ? FString(TEXT("build_material_graph")) : TEXT("add_") + Kind;');
    expect(Object.keys(members).length).toBeGreaterThan(15);
    for (const [kind, action] of Object.entries(members)) {
      expect(resolve(kind)).toBe(action);
    }
    expect(code('MaterialAuthoring', 'McpMatGraphBatch.cpp'))
      .toContain('!IsBatchableMaterialEdit(Edit = McpMaterialBatchStepEdit(Edit, *StepPtr))');
  });
});

describe('widget previews', () => {
  // A design-mode preview drew every widget, Collapsed ones too: a hidden controls card or NEW RECORD pill showed.
  it('preview_widget gives each widget the visibility the game creates it with', () => {
    const preview = code('WidgetAuthoring', 'Support', 'McpWidgetPreview.cpp');
    // GetVisibility() reads the Slate widget, which design mode forces visible: the saved property is read instead.
    expect(preview).toContain('*VisibilityProperty->ContainerPtrToValuePtr<ESlateVisibility>(Child);');
    expect(preview).toContain('Cached->SetVisibility(UWidget::ConvertSerializedVisibilityToRuntime(Saved));');
    expect(preview).not.toContain('Child->GetVisibility()');
    expect(preview.indexOf('Widget->TakeWidget()')).toBeLessThan(preview.indexOf('ConvertSerializedVisibilityToRuntime'));
  });

  // The first preview after the editor started drew material brushes blank, unannounced: the editor compiles a
  // material's Slate shaders when it is first drawn, so the wait has to follow a draw and be followed by another.
  it('preview_widget waits for shaders on request and always reports them', () => {
    const preview = code('WidgetAuthoring', 'Support', 'McpWidgetPreview.cpp');
    expect(preview.indexOf('McpDrawWidget(WidgetBP')).toBeLessThan(preview.indexOf('McpDeferForShaderCompile(Payload'));
    expect(preview).toContain('McpAddShaderCompileState(ResultJson, Payload);');
  });

  // The same lesson for audit_widget: a wait checked before anything was drawn found nothing compiling, so the audit
  // judged contrast on blank material brushes.
  it('audit_widget draws before it waits for shaders, and reports them', () => {
    const audit = code('WidgetAuthoring', 'Audit', 'McpWidgetAudit.cpp');
    expect(audit.indexOf('McpDrawWidget(WidgetBP')).toBeGreaterThan(-1);
    expect(audit.indexOf('McpDrawWidget(WidgetBP')).toBeLessThan(audit.indexOf('McpDeferForShaderCompile(Payload'));
    expect(audit).toContain('McpAddShaderCompileState(ResultJson, Payload);');
  });
});

// The meter is read once per editor frame: at about 5 fps two recordings read 20-30 dB apart that were not.
describe('sound measurement', () => {
  it('a Sound Wave also reports the level of its own samples', () => {
    const measure = code('Audio', 'McpAudioMeasure.cpp');
    expect(measure).toContain('Wave->GetImportedSoundWaveData(Pcm, SampleRate, Channels)');
    expect(measure).toContain('McpAddWaveLevels(Run->Wave.Get(), Data);');
    const properties = capabilityIndex().byId.get('manage_audio.play_sound')?.schemas.output.properties;
    expect(isRecord(properties) && 'wavePeakDb' in properties && 'waveRmsDb' in properties && 'waveSeconds' in properties).toBe(true);
  });

  // A batch that removed a voice's synth nodes, then named a node class the registry did not hold, saved the voice gutted.
  it('a MetaSound batch looks up every add_node class before any step runs', () => {
    const batch = code('AudioAuthoring', 'MetaSound', 'McpAudioMetaSoundBatch.cpp');
    expect(batch.indexOf('ResolveMetaSoundAddNodeClass(*StepObj)')).toBeGreaterThan(-1);
    expect(batch.indexOf('ResolveMetaSoundAddNodeClass(*StepObj)')).toBeLessThan(batch.indexOf('HandleMetaSoundNodeActions(StepSubAction'));
    expect(code('AudioAuthoring', 'McpAudioMetaSoundNodes.cpp'))
      .toContain('const FMcpMetaSoundNodeClassRequest Request = ResolveMetaSoundAddNodeClass(Params);');
  });
});

// Removing DefaultSceneRoot answered "removed from SCS" while the compile put it straight back with the mesh still under
// it, so a mesh meant to be the physics root never was.
describe('removing a Blueprint\'s root component hands its place to a child', () => {
  it('remove_scs_component promotes the removed node\'s children and reads the tree back', () => {
    const source = code('SCS', 'McpSCSRemoveComponent.cpp');
    expect(source).toContain('SCS->RemoveNodeAndPromoteChildren(NodeToRemove);');
    expect(source).toContain('TEXT("SCS_ROOT_REQUIRED")');
    expect(source).toContain('TEXT("SCS_REMOVE_REVERTED")');
    expect(source).not.toContain('SCS->RemoveNode(');
  });

  // Removing a Buoyancy component answered newRoot "WaterContact": actor components are SCS roots too.
  it('names a new root only when the scene root went, and only a scene component', () => {
    const source = code('SCS', 'McpSCSRemoveComponent.cpp');
    expect(source).toContain('Node->ComponentTemplate->IsA<USceneComponent>()');
    expect(source).toContain('const bool bWasRoot = IsSceneRootNode(NodeToRemove) && SCS->GetRootNodes().Contains(NodeToRemove);');
    expect(source).toContain('SCS->GetRootNodes().FindByPredicate(IsSceneRootNode)');
  });
});

// set_default changed the class default while every placed actor kept the old value under a success reply.
describe('a class default reaches the actors already placed', () => {
  it('set_default carries the new value to placed actors that still held the old default', () => {
    const source = code('Blueprint', 'Graph', 'McpBPSetDefaultLiteral.cpp');
    expect(source).toContain('CDO->GetArchetypeInstances(Instances);');
    expect(source).toContain('Property->Identical(Property->ContainerPtrToValuePtr<void>(InstanceContainer), OldValue)');
    expect(source).toContain('Result->SetNumberField(TEXT("instancesUpdated"), InstancesUpdated);');
  });
});

// A BuoyancyComponent added to a Blueprint never asked for the begin-overlap a water body sends at level load, so every
// placed floater sank.
describe('a Blueprint given buoyancy floats where it is placed', () => {
  it('asks for the water\'s begin-overlap at level load', () => {
    expect(code('SCS', 'McpSCSHandlers.cpp')).toContain('McpEnableWaterOverlaps(CDO);');
    expect(code('Blueprint', 'Components', 'McpBPModifyScsFinalize.cpp'))
      .toContain('McpSCSHandlers::EnableLoadOverlapsForBuoyancy(LocalBP);');
  });

  // A StaticMeshActor-based floater sank with no warning: its mesh makes no overlap events, so the water's
  // begin-overlap never reached the buoyancy component.
  it('turns overlap events on for the root, on the Blueprint path and the water tool alike', () => {
    const lookup = code('..', 'Foundation', 'BridgeHelpers', 'Properties', 'McpComponentLookup.h');
    expect(lookup).toContain('Actor->bGenerateOverlapEventsDuringLevelStreaming = true;');
    expect(lookup).toContain('Root->SetGenerateOverlapEvents(true);');
    expect(code('Environment', 'Runtime', 'McpEnvTimeWater.cpp')).toContain('McpEnableWaterOverlaps(TargetActor);');
  });
});

// A struct read left out every field that was zero (BuoyancyData without BuoyancyDamp2 = 0, its default 1), so the
// reader could not tell a zero from a field the read did not show.
describe('struct reads write every field', () => {
  it('exports a struct against itself, so no field is compared away', () => {
    const source = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'Reflection', 'McpPropertyReflection.cpp'), 'utf8');
    expect(source).toContain('StructProp->Struct->ExportText(Exported, StructValue, StructValue, nullptr, 0, nullptr, true);');
  });
});

// Six frame steps at speed 0.1 answered "Stepped 6 frame(s)" for 0.02 s of game time.
describe('a frame step says the speed it ran at', () => {
  it('names the game speed and the game time it advanced', () => {
    const source = code('ControlEditor', 'McpEditorPlay.cpp');
    expect(source).toContain('Resp->SetNumberField(TEXT("timeDilation"), Dilation);');
    expect(source).toContain('Resp->SetNumberField(TEXT("gameSeconds"), Stepped * FApp::GetFixedDeltaTime() * Dilation);');
  });
});

// A capture that restored the minimized editor photographed the frame drawn before it was minimized.
describe('a full-window screenshot shows the view as it is now', () => {
  it('a capture that restored the editor redraws and waits before taking the picture', () => {
    const source = code('ControlEditor', 'McpEditorScreenshot.cpp');
    expect(source).toContain('Payload->SetBoolField(TEXT("_windowRestoredForCapture"), true);');
    expect(source).toContain('GEditor->RedrawAllViewports(true);');
  });
});

// An expression with a unary minus at step 7 stopped a build_graph batch after 7 nodes were already made.
describe('a graph batch checks its expressions before any step runs', () => {
  it('describes every Expression step in the pre-check, bools the batch declares included', () => {
    const steps = code('BlueprintGraph', 'McpBPGraphBatchSteps.cpp');
    expect(steps).toContain('McpBlueprintMathExpression::DescribeProblems(Blueprint, Expression, DeclaredBools)');
    expect(steps).toContain('DeclaredBools.Add(FName(*Name));');
    const batch = code('BlueprintGraph', 'McpBPGraphBatch.cpp');
    expect(batch.indexOf('DescribeExpressionStep(Context.Blueprint, **Step, DeclaredBools)')).toBeGreaterThan(-1);
    expect(batch.indexOf('DescribeExpressionStep(')).toBeLessThan(batch.indexOf('RunBatchStep(Context, State'));
  });

  // A true/false value used as a number parses, and only the Blueprint compile refused it, 134 steps later.
  it('names a comparison or a bool variable used as an operand of + - * / %, in the batch and the single set', () => {
    const math = code('BlueprintGraph', 'Expression', 'McpBPGraphMathExpr.cpp');
    expect(math).toMatch(/DescribeOperatorMisuse\(Blueprint, Expression, ExtraBools\) \+ TEXT\(" "\) \+\s*DescribeBoolArithmetic\(Blueprint, Expression, ExtraBools\);/u);
    const types = code('BlueprintGraph', 'Expression', 'McpBPGraphMathExprTypes.cpp');
    expect(types).toContain('static const TArray<FString> Arithmetic = {TEXT("+"), TEXT("-"), TEXT("*"), TEXT("/"), TEXT("%")};');
    expect(types).toContain('if (Depth == 0 && TrueFalse.Contains(Parts[At]))');
    expect(types, 'a call result is never judged').toContain('LeftFirst = LeftFirst > 0 && IsNameToken(Parts[LeftFirst - 1]) ? INDEX_NONE : LeftFirst;');
    const mutations = code('BlueprintGraph', 'McpBPGraphNodeMutations.cpp');
    expect(mutations.indexOf('McpBlueprintMathExpression::DescribeBoolArithmetic(Context.Blueprint, Value)'))
      .toBeLessThan(mutations.indexOf('bHandled = McpTrySetNodeAssetPropertyForMcp(TargetNode, PropertyName, Value);'));
  });

  // An arrange_nodes after 69 steps stopped the batch there, the 69 applied.
  it('refuses a step the batch does not run in the pre-check, before any step', () => {
    const batch = code('BlueprintGraph', 'McpBPGraphBatch.cpp');
    expect(batch).toContain('const bool bRuns = IsBatchableEdit(Edit);');
    expect(batch).toContain(': DescribeUnbatchableEdit(Edit); !Bad.IsEmpty())');
    expect(batch.indexOf('const bool bRuns = IsBatchableEdit(Edit);')).toBeLessThan(batch.indexOf('RunBatchStep(Context, State'));
    expect(code('BlueprintGraph', 'McpBPGraphBatchSteps.cpp')).toContain('return DescribeUnbatchableEdit(Edit);');
  });

  // "T - Next < 0 || T - Next >= 0.07" was refused as "'T' is a number": the check read the token beside || alone.
  it('judges each whole operand of && and ||, up to the next && || comma or enclosing bracket', () => {
    const math = code('BlueprintGraph', 'Expression', 'McpBPGraphMathExpr.cpp');
    expect(math).toContain('Notes.Append(DescribeNumberLogic(Blueprint, Parts, ExtraBools));');
    expect(math, 'no verdict from the neighbouring token').not.toContain('Comparisons.Contains(Prev)');
    const types = code('BlueprintGraph', 'Expression', 'McpBPGraphMathExprTypes.cpp');
    expect(types).toContain('if (Depth < 0 || (Depth == 0 && (Parts[At] == TEXT("&&") || Parts[At] == TEXT("||") || Parts[At] == TEXT(","))))');
    expect(types).toContain('const TPair<int32, int32> Operand = LogicalOperand(Parts, Index, Step);');
    expect(types, 'a comparison on the operand level makes it true/false').toContain('bNumber = !bCall && !(Depth == 0 && NotJudged.Contains(Parts[At]));');
  });

  // "A != (B > 0 && C)" with A no bool variable failed as a bare "does not parse": A had become a number input.
  it('names a lone number input compared by == or != with a true/false value, and says what an input is', () => {
    const types = code('BlueprintGraph', 'Expression', 'McpBPGraphMathExprTypes.cpp');
    expect(types).toContain('const bool bEquality = Parts[Index] == TEXT("==") || Parts[Index] == TEXT("!=");');
    expect(types, 'only a name that is the whole operand under C precedence').toContain('IsEqualityEdge(Parts, RightLast + 1)');
    expect(types, 'the literals true and false are bools').toContain('Name.Equals(TEXT("true"), ESearchCase::IgnoreCase)');
    const mutations = code('BlueprintGraph', 'McpBPGraphNodeMutations.cpp');
    expect(mutations).toContain('A name that is not a variable is a number input, never true/false and never a vector: ');
  });

  // Every caller wrote vec(-170, 10, z) or 0.5 * -x first and got a refusal: the node has no unary minus.
  it('writes a unary minus as a subtraction from zero before the node parses it, in the single set and the batch', () => {
    const minus = code('BlueprintGraph', 'Expression', 'McpBPGraphMathExprMinus.cpp');
    expect(minus).toContain('FString RewriteUnaryMinus(const FString& Expression, int32 Depth)');
    expect(minus).toContain('TEXT("(0 - %s)")');
    expect(minus, 'only after a bracket, a comma or an operator').toContain('FCString::Strchr(TEXT("(,+-*/%<>=!&|"), Prev)');
    expect(minus, '1e-5 keeps its exponent').toContain("FChar::ToLower(Text[End - 1]) == TEXT('e')");
    const mutations = code('BlueprintGraph', 'McpBPGraphNodeMutations.cpp');
    const rewrite = mutations.indexOf('Value = McpBlueprintMathExpression::RewriteUnaryMinus(Value);');
    expect(rewrite).toBeGreaterThan(-1);
    expect(rewrite).toBeLessThan(mutations.indexOf('McpBlueprintMathExpression::DescribeBoolArithmetic(Context.Blueprint, Value)'));
    const steps = code('BlueprintGraph', 'McpBPGraphBatchSteps.cpp');
    const stepRewrite = steps.indexOf('Expression = McpBlueprintMathExpression::RewriteUnaryMinus(Expression);');
    expect(stepRewrite).toBeGreaterThan(-1);
    expect(stepRewrite).toBeLessThan(steps.indexOf('DescribeProblems(Blueprint, Expression, DeclaredBools)'));
  });
});

// No capability added code to a project: a C++ class meant leaving the tool for the editor's wizard or an IDE.
describe('create_cpp_class adds a class the way the editor\'s New C++ Class does, without its dialogs', () => {
  it('drives GameProjectUtils::AddCodeToProject unattended, after refusing unsafe input', () => {
    const source = code('SystemControl', 'McpSystemCppClass.cpp');
    const guard = source.indexOf('TGuardValue<bool> Unattended(GIsRunningUnattendedScript, true);');
    expect(guard, 'a wizard message box would block the game thread').toBeGreaterThan(-1);
    expect(guard).toBeLessThan(source.indexOf('GameProjectUtils::AddCodeToProject('));
    // A subfolder of a module without Public/Private got #include "Sub/X.h" from the engine, which did not compile.
    for (const check of ['if (!IsCppIdentifier(ClassName))', 'if (!IsSafeSubfolder(Subfolder))', 'GameProjectUtils::IsValidBaseClassForCreation(ParentClass, Module)', '!ParentClass->HasAnyClassFlags(CLASS_Native)', 'if (!bSplit && !Subfolder.IsEmpty())']) {
      expect(source.indexOf(check), check).toBeGreaterThan(-1);
      expect(source.indexOf(check), check).toBeLessThan(guard);
    }
    expect(source, 'the files written but not compiled is a failure, not a success').toContain('TEXT("COMPILE_FAILED")');
    // Adding the class can reload the project file and empty the engine's module list: a pointer into it read freed
    // strings ("All source code for '<garbage>' must exist within ...").
    const copy = source.indexOf('const FModuleContextInfo Module = *Found;');
    expect(copy).toBeGreaterThan(-1);
    expect(copy).toBeLessThan(guard);
    expect(source).toContain('GameProjectUtils::AddCodeToProject(ClassName, ClassPath, Module, Parent,');
    const handlers = code('SystemControl', 'McpSystemHandlers.cpp');
    expect(handlers).toContain('McpSystemControlHandlers::HandleCreateCppClass(this, RequestId, Payload,');
    expect(handlers).toContain('Lower != TEXT("create_cpp_class")');
  });
});

// A duplicated Blueprint's variables got new GUIDs, and its Math Expressions, which store member references as the
// source's {GUID}, failed to compile until each expression was set again by name.
describe('a duplicated Blueprint keeps its Math Expressions bound to its own variables', () => {
  it('rewrites each source GUID to the variable name and rebuilds the node, before the copy is saved', () => {
    const copy = code('BlueprintGraph', 'Expression', 'McpBPGraphMathExprCopy.cpp');
    expect(copy).toContain('FBlueprintEditorUtils::FindMemberVariableNameByGuid(&Source, Guid)');
    expect(copy).toContain('Node->ReconstructNode();');
    // The engine compiled the copy before the rebind; those 20 errors stayed on the reply and the copy stayed dirty.
    expect(copy).toContain('Bridge->ForgetCapturedMessages([&CopyPath](const FString &Line) { return Line.Contains(CopyPath); });');
    expect(copy).toContain('FKismetEditorUtilities::CompileBlueprint(&Copy);');
    const handler = code('AssetWorkflow', 'Operations', 'McpAssetImportDuplicate.cpp');
    expect(handler).toContain('McpBlueprintMathExpression::RebindDuplicatedExpressions(*Source, *CopyBlueprint);');
    expect(handler.indexOf('PrepareCopy(SourcePath, NewAsset);')).toBeGreaterThan(-1);
    expect(handler.indexOf('PrepareCopy(SourcePath, NewAsset);')).toBeLessThan(handler.indexOf('McpSafeAssetSave(NewAsset)'));
    expect(handler.indexOf('PrepareCopy(SourceAssetPath, Copy);')).toBeGreaterThan(-1);
    expect(handler.indexOf('PrepareCopy(SourceAssetPath, Copy);')).toBeLessThan(handler.indexOf('McpSafeAssetSave(Copy)'));
  });
});

// get_component_details sent every property the class declares (195 for a Niagara component) on every call.
describe('a component read sends property values only when asked', () => {
  it('dumps them for propertyNames or detailed, as inspect_object does', () => {
    expect(code('Environment', 'Inspection', 'McpEnvInspectComponent.cpp')).toContain(
      'if (PropertyNames.Num() > 0 || GetJsonBoolField(Payload, TEXT("detailed"), false))');
  });
});

// An emitter built in one Niagara system had to be rebuilt module by module in the next.
describe('add_emitter copies another system\'s emitter as the editor paste does', () => {
  it('adds through the engine utility, cuts the source as parent and brings the user parameters it reads', () => {
    const copy = code('NiagaraAuthoring', 'McpNiagaraEmitterCopy.cpp');
    expect(copy).toContain('FNiagaraEditorUtilities::AddEmitterToSystem(*System, *SourceEmitter');
    expect(copy).toContain('MCP_GET_EMITTER_DATA(*Added)->RemoveParent();');
    expect(copy).toContain('Source->GetExposedParameters().CopyParameterData(Target, Param);');
    expect(copy).toContain('TEXT("EMITTER_INHERITS")');
    expect(code('NiagaraAuthoring', 'McpNiagaraSystems.cpp')).toContain('return CopyEmitterFromSystem(Context);');
  });
});

// get_niagara_info sent every emitter's inputs and switches, about 15 KB per system.
describe('a Niagara read narrows to one emitter and the named modules', () => {
  it('skips other emitters and keeps only the named modules\' entries', () => {
    const info = code('NiagaraAuthoring', 'McpNiagaraInfoValidate.cpp');
    expect(info).toContain('Handle.GetName().ToString() != OnlyEmitter');
    expect(info).toContain('KeepModuleEntries(EmitterObj, ModuleNames);');
    expect(info).toContain('TEXT("EMITTER_NOT_FOUND")');
  });

  // A placed effect's tint read back as the raw bytes of its override store, and user parameters had no values at all.
  it('reads user parameter defaults, and with actorName the placed component\'s values and overrides', () => {
    const info = code('NiagaraAuthoring', 'McpNiagaraInfoValidate.cpp');
    expect(info).toContain('ReadNiagaraValue(System->GetExposedParameters(), Param)) ParamObj->SetField(TEXT("value"), Value);');
    expect(info).toContain('if (System && !AddPlacedUserParameters(Context, System)) return true;');
    const user = code('NiagaraAuthoring', 'McpNiagaraUserParams.cpp');
    expect(user).toContain('const TSharedPtr<FJsonValue> Value = ReadNiagaraValue(Component->GetOverrideParameters(), Param);');
    expect(user).toContain('Actor ? TEXT("COMPONENT_NOT_FOUND") : TEXT("ACTOR_NOT_FOUND")');
  });
});

// A dozen fire lights each took an edit_component call for one property.
describe('a component property goes onto many actors in one call', () => {
  it('resolves actorNames, writes the named component of each in one transaction, and rows only what did not take', () => {
    expect(code('ControlActor', 'McpActorComponentProps.cpp')).toContain(
      'McpSetComponentPropertiesOnActors(*this, Socket, RequestId, Actors, Missing, ComponentName, PropertiesObject);');
    const many = code('ControlActor', 'McpActorComponentDetails.cpp');
    expect(many).toContain('FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Component Properties")),');
    expect(many).toContain('Data->SetNumberField(TEXT("updatedActors"), Updated.Num());');
    expect(many).toContain('Rows.Num() == 0 ? FString() : FString(TEXT("PARTIAL_FAILURE"))');
  });
});

// A force added to an update stack landed after Solve Forces and Velocity, so the system failed "The module has unmet
// dependencies." (on the generic add, with no word of it in the reply), and that message named neither the module nor a fix.
describe('a Niagara module goes where its dependencies hold', () => {
  it('inserts before the first module providing a post-dependency, on both add paths, and names the issue in full', () => {
    const order = code('NiagaraGraph', 'McpNiagaraModuleOrder.h');
    expect(order).toContain('Dependency.Type == ENiagaraModuleDependencyType::PostDependency && OtherData->ProvidedDependencies.Contains(Dependency.Id)');
    expect(order, 'a missing provider goes before the module that asked for it').toContain(
      'Dependency.Type == ENiagaraModuleDependencyType::PreDependency && Data->ProvidedDependencies.Contains(Dependency.Id)');
    expect(order, 'the engine GetOrderedModuleNodes is not exported').not.toContain('GetOrderedModuleNodes(');
    const generic = code('NiagaraGraph', 'McpNiagaraGraphHandlers.cpp');
    expect(generic).toContain('McpNiagaraModuleOrder::DependencyInsertIndex(*ModuleScript, *OutputNode)');
    expect(generic).toContain('ModuleScript, *OutputNode, StackIndex);');
    expect(generic).toContain('McpNiagaraAuthoringHandlers::CollectNiagaraSystemStackIssues(System, StackErrors, StackWarnings);');
    expect(code('NiagaraAuthoring', 'McpNiagaraStackGraph.cpp')).toContain('McpNiagaraModuleOrder::DependencyInsertIndex(*ModuleScript, *TargetOutput)');
    const issues = code('NiagaraAuthoring', 'McpNiagaraStackIssues.cpp');
    expect(issues).toContain('FString Message = Entry->GetDisplayName().ToString() + TEXT(": ") + Short;');
    expect(issues).toContain('for (const UNiagaraStackEntry::FStackIssueFix& Fix : Issue.GetFixes())');
    // Each error was in stackErrors, again in warnings and the receipt, with a count, a flag and a dependency list.
    const context = code('NiagaraAuthoring', 'McpNiagaraContext.cpp');
    expect(context).toContain('Context.Result->SetArrayField(TEXT("stackErrors"), Errors);');
    expect(context).not.toContain('hasUnmetDependencies');
    expect(code('NiagaraAuthoring', 'McpNiagaraContext.h')).not.toContain('dependencyHint');
    expect(code('..', 'MCP', 'Execute', 'McpNativeReceiptEnrichment.cpp')).not.toContain('TEXT("stackErrors")');
  });
});

// set_parameter on a placed effect answered "Niagara parameter set" for a name its system lacks (the component's
// setter adds any name), wrote a Vector parameter as Float unless told, reached only the actor's first component,
// and left the level unmarked, so the value was lost at the next start unless another edit had dirtied it.
describe('a placed effect takes only the user parameters its system has', () => {
  it('resolves each name and type from the system, names the component, and marks the level', () => {
    const params = code('Effect', 'McpEffectNiagaraRuntimeParams.cpp');
    expect(params).toContain('System->GetExposedParameters().ReadParameterVariables()');
    expect(params).toContain('Write.Code = TEXT("PARAM_NOT_FOUND");');
    expect(params).toContain('const FString Kind = UserTypeKind(Var->GetType());');
    expect(params).toContain('Component->GetName().Equals(ComponentName, ESearchCase::IgnoreCase)');
    expect(params).toContain('Component->Modify();');
    expect(params).toContain('TEXT("PARAMETER_BATCH_INCOMPLETE")');
    expect(params).not.toContain('FindComponentByClass<UNiagaraComponent>()');
  });
});

// A duplicated Niagara system could not start until something compiled it: every component playing the copy logged
// "Error initializing data interfaces. Completing system." and stopped.
describe('a duplicated Niagara system can play at once', () => {
  it('compiles the copy and waits for it before the copy is saved', () => {
    const handler = code('AssetWorkflow', 'Operations', 'McpAssetImportDuplicate.cpp');
    expect(handler).toContain('if (UNiagaraSystem *System = Cast<UNiagaraSystem>(Copy)) {');
    expect(handler).toContain('System->RequestCompile(false);');
    expect(handler).toContain('System->WaitForCompilationComplete(false, false);');
  });
});

// set_default answered instancesUpdated 2 for a class with no placed copies: it counted editor preview actors.
describe('a class default counts only the actors placed in a level', () => {
  it('skips preview copies when counting instancesUpdated', () => {
    const source = code('Blueprint', 'Graph', 'McpBPSetDefaultLiteral.cpp');
    expect(source).toContain('World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::PIE');
  });
});

// A listed BuoyancyData left out BuoyancyDamp2 = 0: the list read exported structs against zero defaults.
describe('listed struct values show their zero fields', () => {
  it('exports a value against itself in the shared property reader and the list fallback', () => {
    const reader = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'Reflection', 'McpPropertyReflectionUtilities.cpp'), 'utf8');
    expect(reader).toContain('MCP_PROPERTY_EXPORT_TEXT(Property, Result, Value, Value, nullptr, PPF_None);');
    const reflection = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'Reflection', 'McpPropertyReflection.cpp'), 'utf8');
    expect(reflection).toContain('MCP_PROPERTY_EXPORT_TEXT(Property, Text, Value, Value, nullptr, PPF_None);');
    expect(reflection).toContain('StructProp->Struct->ExportText(Exported, StructValue, StructValue, nullptr, 0, nullptr, true);');
  });
});

// A paused motion run at interval 0 answered 74 copies of the same t=0 sample.
describe('a paused motion run repeats no sample', () => {
  it('samples only when game time moved since the last sample', () => {
    const source = code('ControlActor', 'McpActorMotionSample.cpp');
    expect(source).toContain('const bool bAdvanced = Run.Samples.Num() == 0 || Now > Run.LastSampleGame;');
    expect(source).toContain('Run.LastSampleGame = GameTime;');
  });
});

// edit_scs modify added a component with no attachTo under DefaultSceneRoot and said nothing about where it went.
describe('a batch-added component says where it was attached', () => {
  it('reports attachedTo when the operation named no parent, on both add paths', () => {
    const resolve = code('Blueprint', 'Components', 'McpBPScsParentResolve.h');
    expect(resolve).toContain('OpSummary->SetStringField(TEXT("attachedTo"), Where);');
    const ops = code('Blueprint', 'Components', 'McpBPModifyScsComponentOps.cpp');
    expect(ops.match(/McpScsParent::ReportParent\(/gu)?.length).toBe(2);
  });
});

// A child Blueprint's edit_scs could not touch a part of its parent Blueprint: get_scs listed it as inherited, while
// modify and attachTo answered that it was neither the Blueprint's own component nor one it inherits.
describe('a child Blueprint edits and attaches to its parent Blueprint parts', () => {
  it('modify edits the child override of a parent part, undone when nothing lands', () => {
    const ops = code('Blueprint', 'Components', 'McpBPModifyScsComponentOps.cpp');
    expect(ops).toContain('Template = McpPropertyCdoComponents::FindCdoComponent(LocalBP, LocalBP->GeneratedClass->GetDefaultObject(), ComponentName, true, &Created.Handler, &Created.Key);');
    expect(ops).toMatch(/if \(!bAnySuccess\) \{\s*Created\.Rollback\(\);/u);
  });
  it('attachTo names a parent Blueprint part by variable, as the Components panel does', () => {
    const resolve = code('Blueprint', 'Components', 'McpBPScsParentResolve.h');
    expect(resolve).toMatch(/if \(bInherited && Node->GetVariableName\(\)\.ToString\(\)\.Equals\(Name, ESearchCase::IgnoreCase\)\) Found = Node;/u);
    expect(resolve).toContain('if (USCS_Node *InheritedNode = FindInheritedScsSceneNode(Blueprint, Trimmed)) {');
    expect(resolve).toContain('Child->SetParent(InheritedNode);');
  });
  // add_scs_component parentComponent "Hanger" on a child Blueprint answered "Parent component not found" while
  // edit_scs attachTo took the same part.
  it('add_scs_component parentComponent takes a parent Blueprint part too, and lists them when nothing matches', () => {
    const add = code('SCS', 'McpSCSAddComponent.cpp');
    expect(add).toContain('InheritedParent = ParentNode ? nullptr : McpScsParent::FindInheritedScsSceneNode(Blueprint, ParentComponentName);');
    expect(add).toMatch(/\} else if \(InheritedParent\) \{\s*NewNode->SetParent\(InheritedParent\);/u);
    expect(add).toContain('Names.AddUnique(GetSCSNodeName(Node) + TEXT(" (inherited)"));');
  });
  // get_blueprint_details listed a restyled gate's Tag as the parent's price tag while every placed copy drew the
  // child's cube: the listings read the node's own template, never the child's override of it.
  it('reads list a parent part as the child override has it, marked overridden', () => {
    const header = code('Property', 'McpPropCdoComponents.h');
    expect(header).toMatch(/if \(Bp->SimpleConstructionScript && Bp->SimpleConstructionScript->GetAllNodes\(\)\.Contains\(Node\)\)\s*\{\s*break;/u);
    expect(header).toContain('Handler->GetOverridenComponentTemplate(FComponentKey(Node))');
    expect(code('Environment', 'Inspection', 'McpEnvInspectBP.cpp')).toContain('McpPropertyCdoComponents::EffectiveTemplate(Blueprint, Node, &bOverridden)');
    expect(code('Property', 'McpPropCdoInspection.cpp')).toContain('McpPropertyCdoComponents::EffectiveTemplate(Blueprint, Node, &bOverridden)');
    expect(code('Blueprint', 'Components', 'McpBPScsGet.cpp')).toContain('McpPropertyCdoComponents::EffectiveTemplate(Blueprint, Node, &bOverridden);');
    expect(code('Blueprint', 'Queries', 'McpBPGet.cpp')).toContain('UObject *Template = Node ? McpPropertyCdoComponents::EffectiveTemplate(BP, Node) : nullptr;');
  });
});

// add_scs_component with a properties bag answered success without saying which properties took their value.
describe('a component add names the properties it set', () => {
  it('echoes appliedProperties on the single add path', () => {
    const source = code('Blueprint', 'Components', 'McpBPScsAddComponent.cpp');
    expect(source).toContain('AppliedNames.Add(MakeShared<FJsonValueString>(PropName));');
    expect(source).toContain('Result->SetArrayField(TEXT("appliedProperties"), AppliedNames);');
  });
});

// Setting a Float member's default to 2.5 answered success with default 0: the text was read from the struct's first
// bytes (a String) instead of the member's own offset.
describe('a struct member default lands on its own member', () => {
  it('converts in a scratch copy and reads the value back at the member offset', () => {
    const source = code('AssetWorkflow', 'Structs', 'McpAssetStructsHelpers.cpp');
    expect(source).toContain('FStructOnScope Scratch(S);');
    expect(source).toContain('const void* Value = Prop->ContainerPtrToValuePtr<void>(Container);');
    expect(source).not.toContain('const_cast<uint8*>(DefaultInstance)');
  });
});

// fix_coplanar left twelve flush pier sections as a staircase after four passes: the smaller piece of every pair moved,
// so each section of a row moved the same way as its neighbour and stayed in its plane.
describe('a row of flush pieces is parted in one pass', () => {
  it('2-colours the touching faces and moves only one colour', () => {
    const plan = code('ControlActor', 'Placement', 'McpCoplanarPlan.cpp');
    expect(plan).toContain('const TMap<FMcpFaceKey, bool> Moves = McpCoplanarColour(Hits, Sides);');
    expect(plan).toMatch(/Moves\.Add\(Next, !bMoves\);/u);
    expect(plan).toMatch(/Swap\(Mover, Still\);\s*Shift = -Shift;/u);
    expect(code('ControlActor', 'Placement', 'McpCoplanarFix.cpp'))
      .toContain('const TMap<AActor*, FMcpCoplanarPlan> Plans = PlanCoplanarPass(Hits, Distance, Inside);');
  });

  // A variable added but not compiled lives only on the Blueprint's list, so diff_asset called the edit identical.
  it('diff_asset compares a Blueprint\'s variable list as well as its class defaults', () => {
    const diff = code('AssetWorkflow', 'Analysis', 'McpAssetDiffBlueprint.cpp');
    expect(diff).toContain('AssetDiffVariables(Diff, Live, Other);');
    expect(diff).toContain('UEdGraphSchema_K2::TypeToText(Var.VarType).ToString()');
  });

  // A Blueprint runtime error kept every variable's value for the session, a large array or string exported whole.
  it('keeps a script error\'s variable snapshot small', () => {
    const errors = code('Log', 'McpScriptErrors.cpp');
    expect(errors).toContain('if (Elements > 16)');
    expect(errors).toContain('Text.Len() > 200) Value = MakeShared<FJsonValueString>(Text.Left(200) + TEXT("..."));');
  });

  // edit_scs add, remove, reparent, set_property and set_transform saved the Blueprint though the schema took save false.
  it('edit_scs holds every SCS edit to the payload\'s save', () => {
    expect(code('SCS', 'McpSCSHandlers.cpp')).toMatch(/if \(!bSaveScsChanges\) \{\s*return;\s*\}\s*bOutSaved = McpSafeAssetSave\(Blueprint\);/u);
    for (const file of ['McpBPScsWrappers.cpp', 'McpBPScsAddComponent.cpp']) {
      expect(code('Blueprint', 'Components', file), file).toContain('TGuardValue<bool> SaveScope(FSCSHandlers::bSaveScsChanges, GetJsonBoolField(Payload, TEXT("save"), true));');
    }
  });

  // A carpet laid over six floor tiles lay partly inside each and was sent under the floor.
  it('judges a piece by its overlaps with all its neighbours together', () => {
    const plan = code('ControlActor', 'Placement', 'McpCoplanarPlan.cpp');
    expect(plan).toContain('Covered.FindOrAdd(FMcpFaceKey(Hits[Index].Actor, Sides[Index])) += Hits[Index].OverlapU * Hits[Index].OverlapV;');
    expect(plan).toContain('const bool bApplied = Hit.FaceArea > 0.0 && Overlap >= 0.95 * Hit.FaceArea;');
  });

  // An ocean was reported as a coplanar pair of its two water info meshes, which no view ever draws.
  it('skips meshes kept out of the main view', () => {
    const faces = code('ControlActor', 'Placement', 'McpCoplanarFaces.cpp');
    expect(faces).toContain('return Component->bRenderInMainPass && Component->GetClass()->GetFName() != TEXT("WaterBodyInfoMeshComponent");');
    expect(faces).toContain('!McpCoplanarDrawnInView(Component))');
  });
});

// A delete listed a struct as "still on disk because the package is still loaded" after the package was unloaded and
// the system had accepted the file delete; the file was gone at the next editor start.
describe('a file delete the system accepted counts as done', () => {
  it('trusts the accepted delete over a file that still shows while a handle is open', () => {
    const source = readFileSync(join(DOMAINS, '..', 'Safety', 'McpSafeOperationsAssetDelete.h'), 'utf8');
    expect(source).toContain('const bool bDeleted = IFileManager::Get().Delete(');
    expect(source).toContain('return bDeleted || !FPackageName::DoesPackageExist(PackageName);');
  });
});

// A Niagara set_parameter that landed answered OUTPUT_SCHEMA_VIOLATION: it sent applied: true while the capability's
// applied is the count of a parameters batch. It also refused LinearColor, the type add_user_parameter takes.
describe('a Niagara parameter write answers in the capability shape', () => {
  it('sends no boolean applied and takes LinearColor as Color', () => {
    const source = code('Effect', 'McpEffectNiagaraRuntimeParams.cpp');
    expect(source, 'only a parameters entry says applied: true or false').not.toContain('Response->SetBoolField(TEXT("applied")');
    expect(source).toContain('Response->SetNumberField(TEXT("applied"), Results.Num() - Failed.Num());');
    expect(source).toContain('(Kind == TEXT("Color") && TypeHint.Equals(TEXT("LinearColor"), ESearchCase::IgnoreCase))');
  });
});

// A set_property batch on an asset never said whether the package reached disk; the single write did.
describe('a property batch says whether it saved', () => {
  it('carries saved and saveSkippedReason from the writes', () => {
    const source = code('Property', 'McpPropObjectSetBatch.cpp');
    expect(source).toMatch(/TEXT\("materialRebuilt"\), TEXT\("saved"\), TEXT\("saveSkippedReason"\)\}\)/u);
  });
});

// set_property markDirty false still left the level unsaved: Modify() marks the package dirty unless told not to.
describe('a property write with markDirty false leaves the package clean', () => {
  it('passes markDirty to Modify', () => {
    const source = code('Property', 'McpPropObjectSet.cpp');
    expect(source).toContain('RootObject->Modify(bMarkDirty);');
    expect(source).not.toContain('RootObject->Modify();');
  });
});

// save_all with assetPaths said "left 1 other dirty package(s) unsaved" without saying which.
describe('save_all names the packages it left unsaved', () => {
  it('lists them under leftDirtyPackages', () => {
    const source = code('ControlEditor', 'McpEditorAssets.cpp');
    // Named, except a package outside a path-restricted caller's prefixes: counted, never named to it.
    expect(source).toMatch(/LeftDirty\+\+;\s*if \(!bOthers\) LeftDirtyPackages\.Add\(Package->GetName\(\)\);/u);
    expect(source).toContain('Resp->SetArrayField(TEXT("leftDirtyPackages"), MakeStringArray(LeftDirtyPackages));');
  });

  // resume answered "PIE session resumed" while the game held its own pause (a title screen): nothing ticked.
  it('resume fails GAME_PAUSED naming the pauser unless unpauseGame lifts the game pause', () => {
    const play = code('ControlEditor', 'McpEditorPlay.cpp');
    expect(play).toContain('if (Settings && Settings->GetPauserPlayerState() && GetJsonBoolField(Payload, TEXT("unpauseGame"))) {');
    expect(play).toContain('UGameplayStatics::SetGamePaused(GEditor->PlayWorld, false);');
    expect(play).toMatch(/Paused->SetStringField\(TEXT\("pausedBy"\), Pauser->GetName\(\)\);\s*SendStandardErrorResponse\(this, Socket, RequestId, TEXT\("GAME_PAUSED"\)/u);
    // play answered "started" while the minimized editor froze the game's title screen, which held the game paused.
    expect(play).toContain('if (bStarted && Root.IsValid() && Root->IsWindowMinimized()) {');
  });

  // A material given a usage flag while a mesh was loading was never marked dirty: assetPaths naming it saved 0.
  it('writes a named package the editor holds even when it is not marked dirty', () => {
    const source = code('ControlEditor', 'McpEditorAssets.cpp');
    expect(source).toMatch(/for \(const FString& Name : OnlyPackages\) \{\s*ProcessPackage\(FindPackage\(nullptr, \*Name\)\);/u);
  });
});

// raycast_world ignoreActors refused PlayerPawn, a name control_actor and set_property take while PIE runs.
describe('the shared actor lookup takes the running game\'s role names', () => {
  it('falls back to ResolveRuntimeRole when no actor has the name', () => {
    const source = code('..', 'Foundation', 'HandlerUtils', 'McpHandlerUtilsActors.cpp');
    expect(source).toMatch(/if \(AActor\* Actor = FindActorByNameInWorldForMcp\(World, ActorName, true\)\)\s*\{\s*return Actor;\s*\}\s*return Cast<AActor>\(ResolveRuntimeRole\(ActorName\)\);/u);
  });
});

// update_custom_expression answered with the whole HLSL it was sent.
describe('a custom expression update does not echo its code', () => {
  it('leaves code out of the reply', () => {
    expect(code('MaterialAuthoring', 'Nodes', 'McpMatUpdateCustomExpr.cpp')).not.toContain('SetStringField(TEXT("code")');
  });
});

// get_splines_info with actorName gave local points while its listing gave world ones; an ocean shoreline read as world
// was off by the actor's location.
describe('a spline read names both point spaces', () => {
  it('adds worldLocation beside the local location', () => {
    const source = code('Spline', 'McpSplineQueries.cpp');
    expect(source).toContain('PointObj->SetObjectField(TEXT("worldLocation"), McpHandlerUtils::VectorToJson(SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World)));');
  });
});

// add_scs_component of a buoyancy component said parent "(root)" while its verification said it attaches to nothing.
describe('an actor component added to a Blueprint is not reported under the root', () => {
  it('names no parent for a component that is not a scene component', () => {
    const source = code('SCS', 'McpSCSAddComponent.cpp');
    expect(source).toContain('const bool bSceneComponent = CompClass->IsChildOf(USceneComponent::StaticClass());');
    expect(source).toContain('!bSceneComponent ? TEXT("(none - not a scene component)")');
  });
});

// A spawn batch item with "label" kept the class label and the reply never mentioned the key.
describe('a spawn batch names the item keys it ignored', () => {
  it('lists unread keys per item and keeps those items under report failures', () => {
    const source = code('ControlActor', 'McpActorSpawnBatch.cpp');
    expect(source).toContain('Entry->SetStringField(TEXT("ignoredKeys"), FString::Join(Ignored, TEXT(", ")));');
    expect(source).toContain('!Entry->HasField(TEXT("ignoredKeys"));');
  });
});

// A boat's or a ship's roll over time could not be sampled: motion samples carried location and velocity only.
describe('motion samples carry rotation when it changes', () => {
  it('writes rotation on the first sample and on each one where it moved', () => {
    const source = code('ControlActor', 'McpActorMotionSample.cpp');
    expect(source).toContain('if (!Run.LastRotation.IsSet() || !Rotation.Equals(Run.LastRotation.GetValue(), 0.05)) {');
    expect(source).toContain('Sample->SetArrayField(TEXT("rotation"), McpPointVec(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));');
  });
});

// connect_pins missed a cast's "AsPrimitive Component" when it was written "AsPrimitiveComponent".
describe('a pin named without its spaces still connects', () => {
  it('compares pin names with spaces and underscores dropped after the exact tries', () => {
    const source = code('BlueprintGraph', 'Context', 'McpBPGraphContextEditor.cpp');
    expect(source).toContain('auto Squash = [](FString S) { S.ReplaceInline(TEXT(" "), TEXT("")); S.ReplaceInline(TEXT("_"), TEXT("")); return S; };');
    expect(source).toContain('Squash(Pin->PinName.ToString()).Equals(Squashed, ESearchCase::IgnoreCase)');
  });
});

// A StaticMeshActor's mesh took its mesh's default collision back when it next registered, and a Blueprint's
// CollisionProfileName change never reached its placed copies.
describe('a static mesh collision change holds', () => {
  it('a static mesh set to a collision mode stops using its mesh default collision, and its placed copies follow', () => {
    const lookup = readFileSync(join(DOMAINS, '..', 'Foundation', 'BridgeHelpers', 'Properties', 'McpComponentLookup.h'), 'utf8');
    expect(lookup).toMatch(/Primitive->SetCollisionEnabled\(static_cast<ECollisionEnabled::Type>\(Enabled\)\);[\s\S]*?Mesh->bUseDefaultCollision = false;/u);
    expect(code('Blueprint', 'Components', 'McpBPScsPropagate.h')).toContain('TEXT("bUseDefaultCollision")})');
  });
});

// set_property of CollisionEnabled on a placed mesh answered success while the collision profile had put the old value back.
describe('a property write the object put back fails', () => {
  it('reads the value again after the change handling and fails when it is the value from before the write', () => {
    const source = code('Property', 'McpPropObjectSet.cpp');
    const before = source.indexOf('const FString BeforeText = bCompare ? McpPropertyTarget::ValueText(Property, TargetContainer) : FString();');
    const after = source.indexOf('const FString AfterText = bCompare && !bDefaultObject ? McpPropertyTarget::ValueText(Property, TargetContainer) : WrittenText;');
    expect(before, 'taken before the write').toBeGreaterThan(-1);
    expect(before).toBeLessThan(source.indexOf('McpPropertyTarget::WriteValue(RootObject, EffectivePropertyName, Property, TargetContainer, ValueField, ConversionError)'));
    expect(after, 'after the object\'s own change handling').toBeGreaterThan(source.indexOf('RootObject->PostEditChangeProperty(Changed);'));
    expect(after, 'and before a Blueprint compile replaces the object').toBeLessThan(source.indexOf('FKismetEditorUtilities::CompileBlueprint(ResolvedBlueprint);'));
    expect(source).toMatch(/if \(bAdjusted && AfterText == BeforeText\)\s*\{\s*CreatedOverride\.Rollback\(\);/u);
    expect(source).toMatch(/the object works it out from another of its settings, so change that "\s*"one instead\./u);
    expect(source, 'an adjusted value is a success that names it').toContain('object\'s own change handling made it \'%s\'.');
  });

  it('saves through the shared helper, with the same skip reasons', () => {
    const target = code('Property', 'McpPropTarget.cpp');
    expect(target).toContain('bool SaveAfterWrite(UObject* Target, UBlueprint* Blueprint, bool bMarkDirty, FString& OutSkipReason)');
    expect(target).toContain('} else if (McpSafeAssetSave(OwningPackage)) {');
    expect(target).toContain('OutSkipReason = TEXT("level content is saved with its level");');
  });
});

// set_property of "StaticMeshComponent0.StaticMesh" on a placed actor wrote the raw property, and the engine's
// KnownStaticMesh ensure fired; a raw CollisionEnabled was put back by the mesh's default collision.
describe('a component key with a setter is written through the setter', () => {
  it('set_property routes a setter key of a component, or of the component a placed actor path names first', () => {
    const target = code('Property', 'McpPropTarget.cpp');
    expect(target).toContain('if (!Component || !McpIsComponentSetterKey(Component, Key)) {');
    expect(target).toContain('McpApplyComponentProperties(Component, Bag, Applied, Warnings);');
    expect(target, 'the actor property holding it, or its own name').toContain('Held->GetObjectPropertyValue_InContainer(Actor)');
    expect(code('ControlActor', 'McpActorComponentProps.cpp'))
      .toContain('bool McpIsComponentSetterKey(const UActorComponent *Component, const FString &Name) {');
    expect(code('Property', 'McpPropCdoPropagation.cpp'), 'placed copies take it the same way')
      .toContain('McpPropertyTarget::WriteValue(Instance, Path, Property, Container, Value, Error)');
  });
});

// edit_scs set_property StaticMesh wrote the template's raw property (and the push to placed copies copied it raw), so
// the engine's KnownStaticMesh ensure fired on the template.
describe('a component mesh goes through SetStaticMesh', () => {
  it('the shared JSON writer and the push to placed copies set a StaticMeshComponent mesh through its setter', () => {
    const writer = readFileSync(join(DOMAINS, '..', 'Foundation', 'BridgeHelpers', 'Properties', 'McpPropApplyObjects.h'), 'utf8');
    expect(writer, 'a struct member or function parameter named StaticMesh is not the component\'s')
      .toContain('Cast<UClass>(Property->GetOwnerUField())');
    expect(writer).toContain('Owner->IsChildOf(UStaticMeshComponent::StaticClass())');
    expect(writer).toContain('MeshOwner->SetStaticMesh(Cast<UStaticMesh>(Value));');
    expect(writer, 'set and clear alike').toContain('return Assign(Res);');
    expect(writer).toContain('return Assign(nullptr);');
    expect(code('Blueprint', 'Components', 'McpBPScsPropagate.h'))
      .toContain('Mesh->SetStaticMesh(McpStaticMeshOwner(TemplateProp, TemplateContainer)->GetStaticMesh());');
  });
});

// A parent Blueprint's class default reached its own placed copies, not a child Blueprint's default or the child's copies.
describe('a class default reaches derived Blueprints that never overrode it', () => {
  it('collects derived defaults and their copies whose whole archetype chain still holds the old value', () => {
    const source = code('Property', 'McpPropCdoPropagation.cpp');
    expect(source).toContain('for (UObject* Each = Instance; Each && Each != Template; Each = Each->GetArchetype())');
    expect(source).toContain('Instance->IsTemplate() && !McpPropertyTarget::IsSupersededTarget(Instance)');
    expect(source).not.toContain('Instance->GetArchetype() != Template');
  });
});

// A water body's wave generator took MaxAmplitude while the waves kept their old height: only the generator heard it.
describe('an instanced subobject write tells its owner', () => {
  it('walks up the owners through the instanced property holding each one', () => {
    const target = code('Property', 'McpPropTarget.cpp');
    expect(target).toContain('if (It->HasAnyPropertyFlags(CPF_InstancedReference) && It->GetObjectPropertyValue_InContainer(Owner) == Edited) {');
    expect(target).toContain('Owner->PostEditChangeProperty(Changed);');
    const set = code('Property', 'McpPropObjectSet.cpp');
    expect(set.indexOf('McpPropertyTarget::NotifyOwners(RootObject);')).toBeGreaterThan(set.indexOf('RootObject->PostEditChangeProperty(Changed);'));
  });
});

// attachTo "Root" on a Blueprint with a component named Root landed on the DefaultSceneRoot above it.
describe('a component named like the root alias is the parent it names', () => {
  it('treats Root, RootComponent and DefaultSceneRoot as the root only when no component has that name', () => {
    const scs = code('SCS', 'McpSCSHandlers.cpp');
    expect(scs).toContain('bool IsSCSRootAlias(USimpleConstructionScript *SCS, const FString &Name) {');
    expect(scs).toContain('!FindSCSNodeByVariableName(SCS, Name);');
    expect(code('SCS', 'McpSCSAddComponent.cpp')).toContain('IsSCSRootAlias(SCS, ParentComponentName)');
    expect(code('SCS', 'McpSCSReparent.cpp')).toContain('IsSCSRootAlias(SCS, NewParentName)');
  });
});

// A Blueprint component's properties bag refused SimulatePhysics: the field is BodyInstance.bSimulatePhysics.
describe('a bool named without its b prefix resolves', () => {
  it('tries the b-prefixed bool for a last segment and for a bare nested name', () => {
    const resolver = code('..', 'Foundation', 'BridgeHelpers', 'Properties', 'McpNestedPropPath.h');
    expect(resolver).toContain('CastField<FBoolProperty>(FindFProperty<FProperty>(CurrentTypeScope, FName(*(TEXT("b") + Segment))));');
    expect(resolver).toContain('McpCollectNestedMemberPaths(RootObject->GetClass(), FName(*(TEXT("b") + PropertyPath)), FString(), Candidates);');
  });
});

// Writing four buoyancy pontoons echoed every runtime field of each one (LocalForce, WaterHeight ...): 4 KB per call.
describe('a written struct echoes its editable fields', () => {
  it('leaves runtime fields out of a write echo, for a struct and an array of them, and reads stay whole', () => {
    const reflection = code('..', 'Foundation', 'Reflection', 'McpPropertyReflection.cpp');
    expect(reflection).toContain('if (!It->HasAnyPropertyFlags(CPF_Edit))');
    expect(reflection).toContain('return bLeftOut && !Fields.IsEmpty() ?');
    expect(reflection).toContain('FStructProperty* StructProp = CastField<FStructProperty>(ArrayProp ? ArrayProp->Inner : Property);');
    expect(code('Property', 'McpPropObjectSet.cpp')).toContain('McpPropertyReflection::ExportWrittenValueToJson(TargetContainer, Property)');
    expect(code('Property', 'McpPropObjectGet.cpp')).not.toContain('ExportWrittenValueToJson');
  });
});

// A 108-step graph build answered some 70 rows of {"index":N,"edit":"connect_pins","connected":true,"success":true}.
describe('a batch lists the steps that have something to say', () => {
  it('counts plain successful steps in succeeded and lists the rest, in the graph, material and MetaSound batches', () => {
    const fields = code('..', 'Foundation', 'BridgeHelpers', 'Responses', 'McpJsonFields.h');
    expect(fields).toContain('static inline TArray<TSharedPtr<FJsonValue>> McpListTellingSteps(const TArray<TSharedPtr<FJsonValue>>& Results)');
    expect(fields).toContain('Step->Values.Num() == Plain');
    // Through McpSetBatchSteps, which also leaves out an empty results or nodeIds.
    expect(fields).toMatch(/const TArray<TSharedPtr<FJsonValue>> Telling = McpListTellingSteps\(Results\);\s*if \(Telling\.Num\(\) > 0\)/u);
    expect(fields).toContain('if (NodeIds.IsValid() && NodeIds->Values.Num() > 0)');
    expect(code('BlueprintGraph', 'McpBPGraphBatch.cpp')).toContain('McpSetBatchSteps(Result, Results, NodeIds);');
    expect(code('MaterialAuthoring', 'McpMatGraphBatch.cpp')).toContain('McpSetBatchSteps(Result, Results, NodeIds);');
    expect(code('AudioAuthoring', 'MetaSound', 'McpAudioMetaSoundBatch.cpp')).toContain('McpSetBatchSteps(Response, Results, NodeIds);');
  });

  it('lists the failing step last on a failed batch, alone once the steps before it were undone', () => {
    const batch = code('BlueprintGraph', 'McpBPGraphBatch.cpp');
    expect(batch).toContain('Details->SetArrayField(TEXT("results"), Results);');
    expect(batch.indexOf('McpBlueprintBehaviour::UndoBatch(')).toBeLessThan(batch.indexOf('Details->SetArrayField(TEXT("results"), Results);'));
    expect(code('BlueprintGraph', 'Behaviour', 'McpBPBehaviourSnapshot.cpp')).toContain('Results.RemoveAt(0, FailedIndex);');
    // The message is cut at 512 characters: what happened to the Blueprint goes before the step's own error.
    expect(batch).toContain('TEXT("build_graph stopped at operations[%d]. %s %s"), Index,');
  });

  // A type error at step 5 left the two variables and three nodes the steps before it made.
  it('undoes the steps before a failing one, from a snapshot taken before step 0', () => {
    const batch = code('BlueprintGraph', 'McpBPGraphBatch.cpp');
    expect(batch).toContain('bCompile ? McpBlueprintBehaviour::TakeSnapshot(Context.Blueprint) : McpBlueprintBehaviour::FSnapshot();');
    expect(batch.indexOf('McpBlueprintBehaviour::TakeSnapshot(')).toBeLessThan(batch.indexOf('RunBatchStep(Context, State'));
    expect(batch).toContain('McpBlueprintBehaviour::UndoBatch(Context.Blueprint, Before, *Steps, Results)');
    expect(batch, 'a failed batch names no removed node').not.toContain('Details->SetObjectField(TEXT("nodeIds"), NodeIds);');
    const snapshot = code('BlueprintGraph', 'Behaviour', 'McpBPBehaviourSnapshot.cpp');
    expect(snapshot).toMatch(/Detail::Restore\(Blueprint, Before, MakeShared<FJsonObject>\(\)\);\s*if \(!Before\.bPackageDirty\)/u);
    expect(snapshot, 'what it cannot put back is named').toContain('set on a node that was already there');
    // Removing a new Math Expression took its sub-graph along; removing that graph's tunnel node crashed the editor.
    expect(snapshot).toContain('FBlueprintEditorUtils::FindBlueprintForGraph(Graph) != Blueprint');
    expect(code('BlueprintGraph', 'Behaviour', 'McpBPBehaviourOwnership.cpp')).toContain('FBlueprintEditorUtils::FindBlueprintForGraph(Graph) != Blueprint');
  });

  // A created node's row repeated its id beside the batch's nodeIds map.
  it('leaves the node id out of a row whose step has an id', () => {
    const fields = code('..', 'Foundation', 'BridgeHelpers', 'Responses', 'McpJsonFields.h');
    expect(fields).toMatch(/if \(Step\.IsValid\(\) && Step->HasField\(TEXT\("id"\)\) && \(Step->HasField\(TEXT\("nodeGuid"\)\) \|\| Step->HasField\(TEXT\("nodeId"\)\)\)\)\s*\{\s*Step = MakeShared<FJsonObject>\(\*Step\);\s*Step->RemoveField\(TEXT\("nodeGuid"\)\);\s*Step->RemoveField\(TEXT\("nodeId"\)\);/u);
  });
});

// manage_blueprint create sent { path } answered only "Missing required parameter 'savePath'", and path looked accepted.
describe('a missing required parameter names the keys sent in its place', () => {
  it('adds the keys the action does not take to the native refusal', () => {
    const source = code('..', 'MCP', 'Execute', 'McpNativeGatewaySchemaValidation.cpp');
    expect(source).toContain('if (!(*Properties)->HasField(Entry.Key)) Stray.Add(Entry.Key);');
    expect(source).toContain('TEXT("; sent %s, which this action does not take")');
    expect(source).toContain('McpSchemaKeywords::JoinPointer(Pointer, Missing[0]), Message + StrayNote);');
  });

  // spawn sent { assetPath } answered only "At least one of [classPath, ...] must be provided".
  it('adds them to a missing at-least-one-of group too', () => {
    const source = code('..', 'MCP', 'Execute', 'McpNativeGatewaySchemaValidation.cpp');
    expect(source).toMatch(/CheckRequiredOneOf\(Object, Schema, Pointer, OutViolation\)\)\s*\{\s*OutViolation\.Message \+= StrayNote;/u);
  });
});

// get_stats in Play In Editor read a 21 ms game thread, 8 ms of it the editor's own UI, while the world ticked in 10.5.
describe('performance stats give the game\'s own share of the frame', () => {
  it('times each world\'s tick from the subsystem\'s start and reports the measured world\'s as worldTickMs', () => {
    const lifecycle = code('..', 'Core', 'Subsystem', 'EngineWireSubsystemLifecycle.cpp');
    expect(lifecycle).toContain('McpWorldTickTimer::Start();');
    expect(lifecycle).toContain('McpWorldTickTimer::Stop();');
    const timer = code('..', 'Foundation', 'Diagnostics', 'McpWorldTickTimer.cpp');
    expect(timer).toContain('FWorldDelegates::OnWorldTickStart.AddLambda(');
    expect(timer).toContain('FWorldDelegates::OnWorldPostActorTick.AddLambda(');
    expect(timer).toContain('FWorldDelegates::OnWorldTickStart.Remove(GMcpWorldTickStartHandle);');
    const stats = code('Environment', 'Inspection', 'McpEnvInspectStats.cpp');
    expect(stats).toContain('if (WorldTickMs >= 0.0) Resp->SetNumberField(TEXT("worldTickMs"), WorldTickMs);');
  });

  // With the editor minimized the render and RHI times read the same on every call and the GPU time 150-750 ms.
  it('says when the editor window is minimized, so the render times are stale', () => {
    const stats = code('Environment', 'Inspection', 'McpEnvInspectStats.cpp');
    expect(stats).toMatch(/if \(Root\.IsValid\(\) && Root->IsWindowMinimized\(\)\)\s*\{\s*Resp->SetBoolField\(TEXT\("windowMinimized"\), true\);/u);
  });

  // A game holding most of the GPU slowed every frame; only the viewport's "Video memory has been exhausted" line said so.
  it('says how far video memory is over budget, from the renderer\'s own figure', () => {
    const stats = code('Environment', 'Inspection', 'McpEnvInspectStats.cpp');
    expect(stats).toContain('const double VideoOverMB = double(GDemotedLocalMemorySize) / (1024.0 * 1024.0);');
    expect(stats).toContain('if (VideoOverMB >= 1.0) Resp->SetNumberField(TEXT("videoMemoryOverBudgetMB"), FMath::RoundToDouble(VideoOverMB));');
  });
});

// get_splines_info with actorName answered the spline at the top level and again under splines; save_all carried every
// count its message gives as a field.
describe('a reply says each thing once', () => {
  it('answers one spline only under splines', () => {
    const source = code('Spline', 'McpSplineQueries.cpp');
    expect(source).not.toContain('Result->SetNumberField(TEXT("pointCount")');
    expect(source).not.toContain('Result->SetArrayField(TEXT("points")');
  });

  it('reads a new variable back once, without the request echoed beside it or metadata copying its name', () => {
    const add = code('Blueprint', 'Variables', 'McpBPAddVariable.cpp');
    expect(add).not.toContain('Response->SetStringField(TEXT("variableType"), VarType);');
    expect(add).not.toContain('Variable already exists; no changes applied.');
    const vars = code('Blueprint', 'Variables', 'McpBPVariableIntrospection.cpp');
    expect(vars).toContain('Metadata->RemoveField(TEXT("DisplayName"));');
  });

  it('names a compiled Blueprint once', () => {
    expect(code('Blueprint', 'Queries', 'McpBPCompile.cpp')).not.toContain('Out->SetStringField(TEXT("blueprintPath"), Path);');
  });

  it('says a compile cleared undo history without the paragraph on why', () => {
    expect(code('..', 'Foundation', 'BridgeHelpers', 'Blueprints', 'McpBPDiag.h')).not.toContain('undoBufferResetReason');
  });

  it('lets save_all\'s message carry the counts', () => {
    const source = code('ControlEditor', 'McpEditorAssets.cpp');
    expect(source).not.toContain('Resp->SetNumberField(TEXT("savedCount")');
    expect(source).toContain('if (FailedPackages.Num() > 0) Resp->SetArrayField(TEXT("failedPackages"), MakeStringArray(FailedPackages));');
  });
});

// There was no way to make a Material Parameter Collection or give it parameters, so a level-wide value (wetness, wind
// strength) could not drive several materials at once.
describe('a Material Parameter Collection can be made and read', () => {
  it('creates or updates a collection, works on copies so a refused row changes nothing, and never removes a parameter', () => {
    const create = code('MaterialAuthoring', 'Creation', 'McpMatCreateParamCollection.cpp');
    expect(create).toContain('TArray<FCollectionScalarParameter> Scalars = Collection ? Collection->ScalarParameters : TArray<FCollectionScalarParameter>();');
    expect(create).toContain('return FString::Printf(TEXT("\'%s\' is already a parameter of the other kind, and a collection\'s names are shared"), *Name);');
    expect(create.indexOf('Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s; nothing was changed."), *Error), TEXT("INVALID_ARGUMENT"));'))
      .toBeLessThan(create.indexOf('NewObject<UMaterialParameterCollection>('));
    expect(create).toContain('Collection->PostEditChange();');
    // A new vector started all zero, so an [r, g, b] default came out with alpha 0.
    expect(create).toContain('bAdded ? 1.f : Param.DefaultValue.A');
    expect(create).not.toMatch(/ScalarParameters\.Remove|VectorParameters\.Remove|\.Empty\(\)/u);
    const handlers = code('MaterialAuthoring', 'McpMatHandlers.cpp');
    expect(handlers).toContain('McpMaterialAuthoringHandlers::HandleCreateParameterCollection(this, RequestId, SubAction, Payload, Socket)');
    const routing = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'MCP', 'Routing',
      'McpConsolidatedActionRoutingAssets.h'), 'utf8');
    expect(routing).toContain('TEXT("create_parameter_collection")');
  });

  it('a CollectionParameter node takes its collection, refusing one that lacks the parameter before the node exists', () => {
    const node = code('MaterialAuthoring', 'Nodes', 'McpMatAddMatNode.cpp');
    expect(node.indexOf('!NodeCollection->GetParameterId(Wanted).IsValid()')).toBeLessThan(node.indexOf('NewObject<UMaterialExpression>('));
    expect(node).toMatch(/CastChecked<UMaterialExpressionCollectionParameter>\(NewExpr\)->Collection = NodeCollection;\s*NewExpr->PostEditChange\(\);/u);
  });
});

// A merged mesh (a library pack imported as one asset) could only be placed whole.
describe('split_mesh makes one static mesh per part', () => {
  it('groups the triangles itself, checks every name and the object count before it creates anything, and keeps each part to the slots it uses', () => {
    const split = code('Geometry', 'Support', 'McpGeometrySplitMesh.cpp');
    // An imported mesh came apart into ten thousand loose faces, and the pooled Geometry Script split raised an ensure
    // past a thousand pieces and let them all go.
    expect(split).not.toContain('UDynamicMeshPool');
    expect(split).not.toContain('SplitMeshByComponents');
    expect(split).toContain('Components.FindConnectedTriangles();');
    expect(split, 'swept along X').toContain('for (int32 J = I + 1; J < Order.Num() && Pieces[Order[J]].Box.Min.X <= Reach.Max.X; ++J)');
    expect(split).toContain('UE::Geometry::FDynamicSubmesh3 Submesh(&Mesh, Group.Triangles);');
    expect(split.indexOf('McpAssetExists(Path)')).toBeLessThan(split.indexOf('CreateNewStaticMeshAssetFromMesh('));
    expect(split.indexOf('Kept.Num() > MaxParts')).toBeLessThan(split.indexOf('CreateNewStaticMeshAssetFromMesh('));
    // dryRun answers the meshes and creates none, so a split can be tuned first.
    expect(split.indexOf('if (bDryRun)')).toBeLessThan(split.indexOf('CreateNewStaticMeshAssetFromMesh('));
    expect(split).toContain('UGeometryScriptLibrary_MeshMaterialFunctions::CompactMaterialIDs(Part, SourceMaterials, PartMaterials);');
    expect(split, 'small objects go after grouping, and the largest first, so a repeat of a larger one is the one dropped')
      .toContain('Groups.Sort([](const FMcpTriangleGroup& A, const FMcpTriangleGroup& B) { return A.Triangles.Num() > B.Triangles.Num(); });');
    const handlers = code('Geometry', 'McpGeometryHandlers.cpp');
    expect(handlers).toContain('if (SubAction == TEXT("split_mesh")) return HandleSplitMesh(this, RequestId, Payload, RequestingSocket);');
  });
});

// validate_assets only loaded each asset: one its project or engine validators reject passed as valid.
describe('validate_assets runs Data Validation on request', () => {
  it('asks the editor validator subsystem for each loaded asset and turns a rejected one invalid', () => {
    const check = code('SystemControl', 'McpSystemAssetValidate.cpp');
    expect(check).toContain('const bool bDataValidation = GetJsonBoolField(Payload, TEXT("dataValidation"));');
    expect(check).toContain('UEditorValidatorSubsystem* Validator = bDataValidation && GEditor');
    expect(check).toContain('Validator->IsAssetValid(FAssetData(Asset), Errors, Warnings, EDataValidationUsecase::Script);');
    expect(check).toContain('if (Asset && Validator && McpRunDataValidation(Validator, Asset, Row) == EDataValidationResult::Invalid) {');
    expect(check).toContain('if (Counts[0] > 0) {');
  });
});

// run_tests typed "automation RunTests" into the console and answered "check the Output Log".
describe('run_tests runs each test and answers with its result', () => {
  const tests = code('SystemControl', 'McpSystemTests.cpp');

  it('drives each test as the engine automation worker does and reads what it reported', () => {
    expect(tests).not.toContain('automation RunTests');
    expect(tests).toContain('Framework.StartTestByName(Test.GetTestName(), 0);');
    expect(tests).toContain('if (!bTimedOut && !(Framework.ExecuteLatentCommands() && Framework.ExecuteNetworkCommands())) {');
    expect(tests).toContain('Run.Current->GetExecutionInfo(Info);');
    expect(tests).toContain('const bool bPassed = Run.Current->GetLastExecutionSuccessState() && !bTimedOut;');
    expect(tests).toContain('for (const FAutomationExecutionEntry& Entry : Info.GetEntries()) {');
  });

  // StopTest and ExecuteLatentCommands check() that a test is running: a test the framework refused to start
  // (Play In Editor, a slow task, an unknown name) must never reach them.
  it('only stops a test the framework really started, and never runs beside another runner or a game', () => {
    expect(tests).toMatch(/Framework\.StartTestByName\(Test\.GetTestName\(\), 0\);\s*Run\.Current = GIsAutomationTesting \? Framework\.GetCurrentTest\(\) : nullptr;\s*if \(Run\.Current\) \{/u);
    // The editor's automation worker may finish the test first: only a test still running, and still ours, is stopped here.
    expect(tests).toContain('if (GIsAutomationTesting && Framework.GetCurrentTest() == Run.Current) {');
    expect(tests).toContain('const bool bBlocked = GIsAutomationTesting || (GEditor && GEditor->PlayWorld);');
    expect(tests).toContain('TEXT("PIE_RUNNING")');
    expect(tests).toContain('TEXT("AUTOMATION_BUSY")');
    // A test still running at its timeout has its queued commands dropped before it is stopped.
    expect(tests).toMatch(/if \(bTimedOut\) \{\s*Framework\.DequeueAllCommands\(\);/u);
  });
});

// A model cannot see a mesh: a hole, a crack, a fin or a face turned inside out passed every other check.
describe('check_mesh reads the soundness of a mesh from its source topology', () => {
  it('counts open, non-manifold and flipped edges from the faces each source edge joins', () => {
    const check = code('Environment', 'Inspection', 'McpEnvInspectMeshCheck.cpp');
    expect(check).toContain('const FMeshDescription *Description = Mesh ? Mesh->GetMeshDescription(0) : nullptr;');
    expect(check).toContain('const TArrayView<const FTriangleID> Faces = Description->GetEdgeConnectedTriangleIDs(Edge);');
    expect(check).toContain('Flipped += First.From != First.To && First.From == Runs(Faces[1], From, To).From ? 1 : 0;');
    // Faces split along a seam (corners at one place, not shared) are joined, not a hole, and still checked for winding.
    expect(check).toContain('Flipped += Seen.Count == 1 && Seen.From == Run.From ? 1 : 0;');
    expect(check).toContain('Weld[Vertex.GetValue()] = Found ? *Found : WeldIds.Add(Key, WeldIds.Num());');
    // Only a closed surface reports a volume.
    expect(check).toContain('const bool bClosed = TriangleCount > 0 && Open == 0 && NonManifold == 0;');
    expect(check).toContain('McpHandlerUtils::MarkNoAssetsChanged(Result);');
    const inspect = code('Environment', 'Inspection', 'McpEnvInspect.cpp');
    expect(inspect).toContain('return HandleInspectMeshCheckAction(*this, RequestId, Payload, RequestingSocket);');
  });
});

// A particle system advanced (or a component edited) right before a capture drew its old state: its render data is
// only sent at the end of the next frame, so the picture missed it.
describe('an editor viewport capture shows what changed since the last frame', () => {
  it('sends the world\'s pending render updates before every frame it draws', () => {
    const windows = code('ControlEditor', 'McpEditorScreenshotWindows.cpp');
    expect(windows).toMatch(/if \(World\) \{\s*World->SendAllEndOfFrameUpdates\(\);\s*\}\s*Viewport->Draw\(\);\s*FlushRenderingCommands\(\);/u);
    const screenshot = code('ControlEditor', 'McpEditorScreenshot.cpp');
    expect(screenshot).toContain('DrawViewportFramesForMcp(Viewport, CaptureClient ? CaptureClient->GetWorld() : nullptr, bCameraMoved ? 3 : 1);');
  });

  // A just-built Nanite mesh photographed at once showed blobs of its coarsest clusters: its pages stream in over engine
  // ticks, which no drawing inside one call waits out.
  it('takes the picture again a beat later until the frame matches the one before, at most five passes', () => {
    const screenshot = code('ControlEditor', 'McpEditorScreenshot.cpp');
    const settle = code('ControlEditor', 'Camera', 'McpViewSettle.cpp');
    expect(screenshot).toMatch(/bCameraMoved \? 3 : 1\);\s*if \(!ViewFrameSettledForMcp\(Viewport, Payload\)\) \{ RetryLater\(\); return true; \}/u);
    expect(settle).toContain('constexpr int32 MaxSettleRounds = 5;');
    expect(settle).toContain('if (SignaturesMatch(Signature, GetJsonStringField(Payload, TEXT("_settleFrame"))) || Round >= MaxSettleRounds) return true;');
  });
});

// advance_simulation answered "advanced" for a finished, paused or never-started system, which the engine steps
// without a word; activate answered active true while its system was still waiting to start.
describe('a Niagara advance or activate says what the system did', () => {
  it('reads the age reached, and refuses when the system did not move', () => {
    const lifecycle = code('Effect', 'McpEffectNiagaraLifecycle.cpp');
    expect(lifecycle).toContain('const double AgeBefore = McpNiagaraAge(NiagaraComponent);');
    expect(lifecycle).toMatch(/if \(Age <= AgeBefore\)\s*\{[\s\S]*?TEXT\("NOT_ADVANCED"\)\);\s*return true;/u);
    expect(lifecycle).toContain('Response->SetBoolField(TEXT("complete"), bComplete);');
    expect(lifecycle).toMatch(/if \(GetJsonBoolField\(Context\.Payload, TEXT\("reset"\)\)\)\s*\{\s*NiagaraComponent->ResetSystem\(\);/u);
    expect(lifecycle).toContain('Response->SetBoolField(TEXT("active"), bActive);');
    expect(lifecycle).not.toContain('SetBoolField(TEXT("active"), true)');
  });
});

// A mesh only spawned in play drew the default material there and in a cooked game, with only a log warning: the
// editor sets a material's usage flag while it draws the mesh outside play.
describe('validate_assets materialUsage lists the usage flags a cooked game would miss', () => {
  it('checks Nanite, skeletal, instanced and spline uses on meshes and Blueprint components, and fails the row', () => {
    const usage = code('SystemControl', 'McpSystemMaterialUsage.cpp');
    expect(usage).toContain('const FMcpUsageNeed McpNaniteNeed{MATUSAGE_Nanite, TEXT("Nanite"), TEXT("bUsedWithNanite")};');
    expect(usage).toContain('Base->MaterialDomain != MD_Surface || Base->bUsedAsSpecialEngineMaterial)');
    // A placement preview set bUsedWithNanite in memory and left the material unsaved: the check passed and the
    // cooked game would still have missed the flag.
    expect(usage, 'a flag set but unsaved is reported').toContain(': McpUsagePending(Owner, Need.Usage) ? static_cast<UObject *>(Owner)');
    // A mesh's asynchronous build finishing set the flag while the editor could not mark the material dirty: nothing
    // was unsaved and the file still lacked it. The engine keeps its own record until the material is saved.
    expect(usage, 'the engine record of flags awaiting a save').toContain('return Material->IsUsageFlagDirty(Usage);');
    expect(usage, 'a 5.8 instance owns its flags').toContain('UMaterialInterface *Owner = Interface->IsA<UMaterialInstanceDynamic>() ? Base : Interface;');
    // ...which 5.8 sets on an instance outside that record, with only a log line.
    expect(usage).toContain('const TCHAR *Marker = TEXT(" needed to set usage flag ");');
    expect(usage, 'kept until the file is written').toContain('IFileManager::Get().GetTimeStamp(*FPaths::ConvertRelativePathToFull(File)) < Given;');
    expect(usage, 'whole seconds, as a file time is').toContain('FDateTime(Now.GetYear(), Now.GetMonth(), Now.GetDay(), Now.GetHour(), Now.GetMinute(), Now.GetSecond())');
    const lifecycle = code('..', 'Core', 'Subsystem', 'EngineWireSubsystemLifecycle.cpp');
    expect(lifecycle).toContain('McpSystemControlHandlers::McpWatchMaterialUsage(true);');
    expect(lifecycle).toContain('McpSystemControlHandlers::McpWatchMaterialUsage(false);');
    expect(usage, 'the parent\'s native components and the construction script\'s').toContain('TInlineComponentArray<UPrimitiveComponent *> Parts(Defaults);');
    expect(usage).toContain('Cast<UPrimitiveComponent>(Node->ComponentTemplate)');
    const validate = code('SystemControl', 'McpSystemAssetValidate.cpp');
    expect(validate).toContain('const bool bUsage = GetJsonBoolField(Payload, TEXT("materialUsage"));');
    expect(validate).toMatch(/Row->SetArrayField\(TEXT\("materialUsage"\), Missing\);\s*FailRow\(Row\);/u);
    expect(validate).toContain('Row->SetNumberField(TEXT("materialUsageCount"), UsageCount);');
  });

  // The edit_scs placement preview drew a Nanite part, which set the flag and left the material dirty (or, while the
  // mesh was still building, not even dirty); saving only the Blueprint left the flag off on disk.
  it('saves the project materials a placement preview gave a flag, once the preview world is gone', () => {
    const placement = code('ControlActor', 'Placement', 'McpPartPlacement.cpp');
    for (const audit of ['FPartAudit AuditBlueprintParts(', 'FPartAudit AuditBlueprintPartsUsingMesh(']) {
      const body = placement.slice(placement.indexOf(audit));
      expect(body.indexOf('const FMcpPreviewMaterialSaver Saver(Blueprint);'), audit).toBeGreaterThan(-1);
      expect(body.indexOf('const FMcpPreviewMaterialSaver Saver(Blueprint);'), audit).toBeLessThan(body.indexOf('FPreviewScene Scene;'));
    }
    const saver = code('ControlActor', 'Placement', 'McpPlacementMaterials.cpp');
    // The compile can set the flag before the preview: the saver asks which needed flags are set but unsaved.
    expect(saver).toContain('McpSystemControlHandlers::McpMissingMaterialUsage(Blueprint.Get())');
    expect(saver, 'project content only').toContain('Material->GetOutermost()->GetName().StartsWith(TEXT("/Game/")) && McpSafeOperations::McpSafeAssetSave(Material)');
  });
});

// A model that cannot see a picture could not tell what a shot shows.
describe('describe_view counts what the game draws in the camera view', () => {
  it('reads a fresh id map, leaves editor-only drawing out, and refuses a running game', () => {
    const view = code('Environment', 'Inspection', 'McpEnvInspectViewContents.cpp');
    // The capture is shared with analyze_view.
    const capture = code('Environment', 'Inspection', 'McpEnvInspectViewPoints.cpp');
    expect(view).toContain('McpCaptureView(CameraPayload, TEXT("describe_view"), bDiff, View, CaptureError, CaptureCode)');
    expect(capture).toMatch(/Viewport->InvalidateHitProxy\(\);\s*Out\.Ids = Viewport->GetRawHitProxyData\(/u);
    expect(view).toContain('const bool bGameDraws = Actor && !(Component && (Component->IsEditorOnly() || Component->bHiddenInGame));');
    expect(capture).toContain('if (GEditor->PlayWorld && !GEditor->bIsSimulatingInEditor)');
    expect(capture).toContain('DrawViewportFramesForMcp(Viewport, Out.Client->GetWorld(), bMoved || bSwitch ? 3 : 1);');
    expect(view).toContain('McpHandlerUtils::MarkNoAssetsChanged(Result);');
    const inspect = code('Environment', 'Inspection', 'McpEnvInspect.cpp');
    expect(inspect).toContain('return HandleInspectViewContentsAction(*this, RequestId, Payload, RequestingSocket);');
  });

  // Checking one prop from a camera meant reading every row of a 40-actor frame, and a missing one said nothing.
  it('lists only the actors named and says why each unseen one is not drawn', () => {
    const view = code('Environment', 'Inspection', 'McpEnvInspectViewContents.cpp');
    expect(view).toContain('Payload->TryGetArrayField(TEXT("actors"), Requested);');
    expect(view).toContain('NotVisible.Add(MakeShared<FJsonValueObject>(McpDescribeUnseenActor(Actor, *Client, Size, Ids, Proxies)));');
    expect(view).toContain('if (Requested && !Named.Contains(Row.Actor))');
    const unseen = code('Environment', 'Inspection', 'McpEnvInspectViewUnseen.cpp');
    expect(unseen, 'the level editor keeps the horizontal field of view').toContain('FMath::Tan(FMath::DegreesToRadians(Client.ViewFOV * 0.5))');
    expect(unseen).toContain('TEXT("outside the frame")');
    expect(unseen, 'what is drawn over its box, frontmost id per pixel').toContain('Covers.FindOrAdd(Proxy->Kind == EMcpViewKind::Actor ? Proxy->Actor : nullptr) += 1;');
    // A storefront behind a balloon cluster at the frame edge was named as what hid it.
    expect(unseen, 'ground under it, or an actor centred farther, cannot hide it').toContain('const bool bGround = Other.Max.Z < Middle.Z && (bUnder || Other.GetSize().Z < 0.25 * Bounds.GetSize().Z);');
    // An editor-only sprite made an 84 cm robot a 256 cm cube.
    expect(unseen, 'the box of what the game draws').toContain('const FBox Bounds = McpPlacement::DrawnBounds(*Actor);');
    expect(code('ControlActor', 'Placement', 'McpPlacementMount.cpp')).toContain(
      'return Part->IsRegistered() && Part->IsVisible() && !Part->bHiddenInGame && !Part->IsEditorOnly();',
    );
    expect(unseen).toContain('Row->SetNumberField(TEXT("inFrame"), InFrame);');
    expect(unseen).toContain('InFrame < 50.0 ? TEXT("at the edge of the frame, mostly outside it")');
  });

  // A spot of a picture could not be turned into the actor and the world point under it, and a picture could not
  // point at an actor by number.
  it('answers points from the id drawn there and a line through the same camera, and numbers the listed boxes', () => {
    const view = code('Environment', 'Inspection', 'McpEnvInspectViewContents.cpp');
    expect(view).toMatch(/!McpDescribeViewPoints\(\*Client, Size, Ids, Proxies, \*Points, FMath::Clamp\(GetJsonIntField\(Payload, TEXT\("tolerance"\), 0\), 0, 16\),\s*PointRows, PointError\)/u);
    expect(view).toContain('Result->SetStringField(TEXT("imageBase64"), McpLabelledViewPng(Colors, Size, ListedBoxes));');
    const points = code('Environment', 'Inspection', 'McpEnvInspectViewPoints.cpp');
    expect(points, 'the inverse of the unseen projection, horizontal FOV').toContain('const double TanY = TanX * Size.Y / Size.X;');
    expect(points).toContain('Axes.GetScaledAxis(EAxis::Z) * ((1.0 - 2.0 * Y) * TanY)).GetSafeNormal();');
    expect(points).toContain('World->LineTraceSingleByChannel(Hit, Camera, Camera + Direction * McpViewTraceLength, ECC_Visibility, Params)');
    expect(points).toContain('if (Client.IsOrtho())');
    expect(points, 'one digit glyph per 3 x 5 bitmask').toContain('constexpr uint16 McpDigits[10] = {0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF};');
    expect(points, 'the labelled frame stays far under the reply cap').toContain('constexpr int32 McpLabelWidth = 960;');
  });

  it('draws every label digit as its 3 x 5 shape', () => {
    const points = code('Environment', 'Inspection', 'McpEnvInspectViewPoints.cpp');
    const masks = /McpDigits\[10\] = \{([^}]*)\}/u.exec(points)?.[1].split(',').map((hex) => Number.parseInt(hex.trim(), 16)) ?? [];
    const shapes = masks.map((mask) => mask.toString(2).padStart(15, '0').match(/.{3}/gu)?.join(' '));
    expect(shapes).toEqual([
      '111 101 101 101 111', '010 110 010 010 111', '111 001 111 100 111', '111 001 111 001 111', '101 101 111 001 001',
      '111 100 111 001 111', '111 100 111 101 111', '111 001 001 001 001', '111 101 111 101 111', '111 101 111 001 111'
    ]);
  });
});

// A model asked to fix "this node" or "the Blueprint I have open" could not tell what that was.
describe('get_editor_focus reads what the user is working on', () => {
  it('lists the open editors newest first and each Blueprint editor\'s graph and selected nodes', () => {
    const focus = code('Environment', 'Inspection', 'McpEnvInspectEditorFocus.cpp');
    expect(focus).toContain('Editors->GetAllEditedAssets()');
    expect(focus).toContain('const double When = Editor->GetLastActivationTime();');
    // The Kismet module lists only editors it made as FBlueprintEditor, so the cast is the one the engine relies on.
    expect(focus).toContain('for (const TSharedRef<IBlueprintEditor> &Each : Kismet.GetBlueprintEditors())');
    expect(focus).toContain('FBlueprintEditor &Editor = static_cast<FBlueprintEditor &>(Each.Get());');
    expect(focus).toContain('Item->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());');
    expect(focus).toContain('FMcpLogHistory::Get().WarningsSince(FMcpLogHistory::Now() - 60.0, 10)');
    expect(focus).toContain('FMcpScriptErrors::Get().ToJson()');
    expect(focus).toContain('McpAddShaderCompileState(Result, nullptr);');
    const inspect = code('Environment', 'Inspection', 'McpEnvInspect.cpp');
    expect(inspect).toContain('return HandleInspectEditorFocusAction(*this, RequestId, RequestingSocket);');
  });
});

// A frame could only be judged by eye: no pass said which actor a region belongs to, how far it is or which way it
// faces.
describe('capture_passes writes aligned id, depth and normal passes of the level view', () => {
  it('captures depth and normal at the view\'s own camera, field of view and shape, and paints ids per actor', () => {
    const passes = code('Environment', 'Inspection', 'McpEnvInspectCapturePasses.cpp');
    expect(passes).toContain('McpCapturePass(Client->GetWorld(), Camera, Facing, Client->ViewFOV, Size, SCS_SceneDepth)');
    expect(passes).toContain('McpCapturePass(Client->GetWorld(), Camera, Facing, Client->ViewFOV, Size, SCS_Normal)');
    expect(passes).toContain('const FIntPoint Size(Width, FMath::Max(1, FMath::RoundToInt(double(Width) * ViewSize.Y / ViewSize.X)));');
    expect(passes).toMatch(/Viewport->InvalidateHitProxy\(\);\s*const TArray<FColor> &Ids = Viewport->GetRawHitProxyData\(/u);
    expect(passes).toContain('Capture->DestroyComponent();');
    expect(passes).toContain('if (Files->Values.Num() < Passes.Num())');
    const ids = code('Environment', 'Inspection', 'McpEnvInspectViewContents.cpp');
    expect(ids).toContain('FMcpViewProxy McpViewProxyOf(const FColor &Id)');
  });
});

// A mesh could not leave the editor; the exporters' option dialogs and message log would also have stopped or
// pulled up the editor.
describe('export_mesh writes a file through the editor exporters, unattended', () => {
  it('runs the export task automated, with dialogs and the log window held off, and checks the file', () => {
    const exporter = code('AssetWorkflow', 'Export', 'McpAssetMeshExport.cpp');
    expect(exporter).toContain('Task->bAutomated = true;');
    expect(exporter).toContain('Task->bPrompt = false;');
    expect(exporter).toMatch(/TGuardValue<bool> Unattended\(GIsAutomationTesting, true\);\s*bExported = UExporter::RunAssetExportTask\(Task\);/u);
    expect(exporter).toContain('if (!McpResolveProjectFilePath(Requested, File, PathError))');
    expect(exporter).toContain('if (!bExported || Bytes <= 0)');
    // Files written beside it are the ones new or changed since the export began, not every recent file there.
    expect(exporter).toContain('if (Path != File && (!Was || *Was != Now.Value) && Written.Num() < McpMaxListedFiles)');
    const assets = code('AssetWorkflow', 'McpAssetHandlers.cpp');
    expect(assets).toContain('return McpMeshExport::HandleExportMesh(this, RequestId, Payload, RequestingSocket);');
  });

  // The glTF bake ticks the core ticker while it waits for mips; nested in a handler's ticker callback, that asserted.
  it('runs a glTF export outside the core ticker, with no other request dispatched in the middle of it', () => {
    const exporter = code('AssetWorkflow', 'Export', 'McpAssetMeshExport.cpp');
    expect(exporter).toMatch(/if \(Format == TEXT\("glb"\) \|\| Format == TEXT\("gltf"\)\)\s*\{[^}]*Bridge->RunOutsideTicker\(/u);
    const outside = code('..', 'Core', 'Requests', 'McpRunOutsideTicker.cpp');
    expect(outside).toContain('GEditor->GetTimerManager()->SetTimerForNextTick(');
    expect(outside).toMatch(/TGuardValue<bool> Drain\(Self->QueueFairness\.bDraining, true\);\s*Self->RunLatentReply\(RequestId, Action, Work\);/u);
  });
});

// A Control Rig could be created but its graph and hierarchy could not be edited or read.
describe('edit_control_rig edits a rig through its own controllers and keeps what a batch applied', () => {
  it('stops at the first failing step, then compiles and saves the steps before it', () => {
    const edit = code('AnimationAuthoring', 'ControlRig', 'McpControlRigGraphEdit.cpp');
    expect(edit).toContain('for (int32 Index = 0; Index < Steps.Num() && Failed == INDEX_NONE; ++Index)');
    expect(edit).toMatch(/if \(Applied > 0\)\s*\{\s*FCompilerResultsLog Results;\s*FKismetEditorUtilities::CompileBlueprint\(Edit\.Rig, EBlueprintCompileOptions::None, &Results\);/u);
    expect(edit).toContain('Edit.Graph->AddUnitNodeFromStructPath(Struct->GetPathName(), TEXT("Execute"), Position, GetJsonStringField(Step, TEXT("name")), true);');
    // A set pin answers the value as stored, read back off the pin.
    expect(edit).toContain('Extra->SetStringField(TEXT("value"), Pin->GetDefaultValue());');
    const hierarchy = code('AnimationAuthoring', 'ControlRig', 'McpControlRigHierarchyEdit.cpp');
    expect(hierarchy).toContain('Edit.Hierarchy->ImportBones(Skeleton, NAME_None, true, true, false, true);');
    expect(hierarchy).toContain('Extra->SetNumberField(TEXT("bones"), Skeleton->GetReferenceSkeleton().GetNum());');
    const read = code('AnimationAuthoring', 'ControlRig', 'McpControlRigGraphRead.cpp');
    expect(read).toContain('McpHandlerUtils::MarkNoAssetsChanged(Response);');
  });
});

// A game camera's FOV of 50 drew 64 degrees across on a wider viewport (the project kept the vertical FOV), so points
// placed on the frame from the reported FOV landed well off.
describe('a game screenshot says the field of view the frame was drawn with', () => {
  it('reads both angles off the local player projection, beside the camera setting', () => {
    const shot = code('Ui', 'McpUiScreenshot.cpp');
    expect(shot).toContain('LocalPlayer->GetProjectionData(Viewport, Projection)');
    expect(shot, 'an orthographic camera has no field of view').toContain('Projection.ProjectionMatrix.M[3][3] == 0.0f');
    expect(shot).toContain('RenderedFov.X = FMath::RadiansToDegrees(2.0 * FMath::Atan(1.0 / Projection.ProjectionMatrix.M[0][0]));');
    const support = code('Ui', 'McpUiScreenshotSupport.cpp');
    expect(support).toContain('Resp->SetNumberField(TEXT("horizontalFov")');
    expect(support).toContain('Resp->SetNumberField(TEXT("verticalFov")');
  });
});

// stop answered "Play in Editor stopped" as it queued the request; the session ended on a later editor tick (10 s later
// in a minimized editor), so the next call could still land in the play world.
describe('stop answers once the play session has ended', () => {
  it('waits on a ticker for the play world to go, and says when it did not within 20 s', () => {
    const play = code('ControlEditor', 'McpEditorPlay.cpp');
    const stop = play.slice(play.indexOf('HandleControlEditorStop('), play.indexOf('HandleControlEditorSetGameSpeed('));
    expect(stop).toContain('GEditor->RequestEndPlayMap();');
    expect(stop).toContain('const bool bStopped = !GEditor || !GEditor->PlayWorld;');
    expect(stop).toContain('TEXT("PIE_STOP_TIMEOUT")');
    expect(stop.indexOf('RequestEndPlayMap'), 'no reply before the ticker').toBeLessThan(stop.indexOf('SendAutomationResponse(Socket, RequestId, bStopped'));
  });
});

// A Fab pack of 32 meshes with default-material slots took one call and one consent per mesh.
describe('mesh_materials sets one slot map on many meshes', () => {
  it('passes over a slot a mesh lacks, fails on an entry no mesh has, and loads every material first', () => {
    const many = code('AssetWorkflow', 'Materials', 'McpAssetMeshMatsMany.cpp');
    expect(many).toContain('if (!ResolveSlot(Slots, Wanted[Entry].Key, Assignment.Index, Reason, Code)) continue;');
    expect(many).toContain('if (!Used[Entry]) Problems.Add(');
    expect(many.indexOf('Material = LoadMaterial(MaterialText, Reason, Code);'), 'materials load before any mesh changes')
      .toBeLessThan(many.indexOf('SetOnOneOfMany(PathValue.IsValid()'));
    const single = code('AssetWorkflow', 'Materials', 'McpAssetMeshMats.cpp');
    expect(single).toMatch(/if \(AssetPath\.IsEmpty\(\) && Payload->TryGetArrayField\(TEXT\("assetPaths"\), Paths\) && Paths && Paths->Num\(\) > 0\)\s*\{\s*return SetOnManyMeshes\(/u);
  });
});

// A material batch's auto-placed nodes, 400 apart, were reported as overlapping: the width estimate read the object
// name ("MaterialExpressionTextureSampleParameter2D_0", 408 px) instead of the title the editor draws.
describe('material node extents follow the title the editor draws', () => {
  it('measures the caption lines, not the object name', () => {
    const header = code('MaterialAuthoring', 'McpMatPrivate.h');
    const estimate = header.slice(header.indexOf('inline void EstimateMaterialNodeExtent('), header.indexOf('inline FString AddMaterialNodePlacementFields('));
    expect(estimate).toContain('Expr->GetCaption(Captions);');
    expect(estimate).not.toContain('Expr->GetName()');
  });
});

describe('a sound that never ends by itself', () => {
  // A steam hiss built as a one-shot MetaSound, its On Finished never wired, went on PlaySoundAtLocation with no word:
  // the node returns nothing to stop, so every play ran until the level unloaded, one more copy per call.
  it('every compile names a PlaySound2D or PlaySoundAtLocation node whose own sound never ends', () => {
    const check = readFileSync(
      join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'BridgeHelpers', 'Blueprints', 'McpBPSoundCheck.cpp'),
      'utf8',
    );
    expect(check).toContain('const bool bForgotten = Name == TEXT("PlaySoundAtLocation") || Name == TEXT("PlaySound2D");');
    expect(check).toContain('const FString Endless = bForgotten ? McpAudioAuthoring::DescribeEndlessSound(Sound) : FString();');
    const ends = code('AudioAuthoring', 'MetaSound', 'McpAudioMetaSoundGraphRead.cpp');
    expect(ends).toMatch(/if \(Sound->IsLooping\(\)\)\s*\{\s*return TEXT\("loops"\);/u);
    expect(ends).toContain('const FName OnFinished(TEXT("UE.Source.OneShot.OnFinished"));');
    expect(ends).toMatch(/Page\.Edges\.ContainsByPredicate\(\s*\[Finished\]\(const FMetasoundFrontendEdge& Edge\) \{ return Edge\.ToNodeID == Finished->NodeID; \}\)/u);
    expect(ends).toMatch(/if \(!bFired\)\s*\{\s*return TEXT\("is a one-shot MetaSound that never fires On Finished"\);/u);
  });
});

describe('a batch names a literal only when the engine stored something else', () => {
  // A MetaSound batch of 13 set_default steps answered 13 rows of {appliedValue: "380.000000"} for 380, and a graph
  // batch echoed every pin default it set, so the rows that mattered drowned in repeats.
  it('MetaSound and Blueprint graph batches drop an appliedValue equal to the literal asked for', () => {
    const fields = readFileSync(
      join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'BridgeHelpers', 'Responses', 'McpJsonFields.h'),
      'utf8',
    );
    expect(fields).toContain('static inline bool McpAppliedValueDiffers(const TSharedPtr<FJsonValue>& Requested, const FString& Applied)');
    expect(fields).toMatch(/if \(Requested->Type == EJson::Boolean\)[\s\S]*?if \(Requested->TryGetNumber\(Number\)\)/u);
    expect(code('AudioAuthoring', 'MetaSound', 'McpAudioMetaSoundBatch.cpp'))
      .toContain('McpAppliedValueDiffers((*StepObj)->TryGetField(TEXT("defaultValue")), Applied)');
    const steps = code('BlueprintGraph', 'McpBPGraphBatchSteps.cpp');
    expect(steps).toContain('McpAppliedValueDiffers(Pair.Value, AppliedValue)');
    expect(steps).toMatch(/if \(Applied->Values\.Num\(\) > 0\)\s*\{?\s*Entry->SetObjectField\(TEXT\("pinDefaults"\), Applied\);/u);
    expect(steps).toContain('Entry->RemoveField(TEXT("appliedValue"));');
  });
});

describe('MetaSound node classes can be listed', () => {
  // A fire crackle needed a random trigger filter, and nothing listed which node classes exist: the class name was a
  // guess until add_node refused it.
  it('list_metasound_nodes searches class name, display name and category, sorted, cut at limit', () => {
    const search = code('AudioAuthoring', 'MetaSound', 'McpAudioMetaSoundNodeSearch.cpp');
    expect(search).toContain('Metasound::Frontend::ISearchEngine::Get().FindAllClasses(false)');
    expect(search).toContain('SquashName(Dotted + TEXT("|") + Meta.GetDisplayName().ToString() + TEXT("|") + Category).Contains(Filter, ESearchCase::IgnoreCase)');
    // Ranked by where the filter hits the class's own name, the engine's UE nodes first within a tier, then by name:
    // "Trigger" was led by enum conversions, then by Crossfade "Trigger Route" variants.
    expect(search).toMatch(/const int32 Tier = Name\.Equals\(Filter, ESearchCase::IgnoreCase\) \? 0 : Name\.StartsWith\(Filter, ESearchCase::IgnoreCase\) \? 1\s*: Name\.Contains\(Filter, ESearchCase::IgnoreCase\) \? 2 : 3;/u);
    expect(search).toContain('Rank.Add(Dotted, Tier * 2 + (Meta.GetClassName().Namespace.ToString().Equals(TEXT("UE"), ESearchCase::IgnoreCase) ? 0 : 1));');
    expect(search).toContain('return Rank[A] != Rank[B] ? Rank[A] < Rank[B] : A.Compare(B, ESearchCase::CaseSensitive) < 0;');
    expect(search).toContain('Response->SetBoolField(TEXT("truncated"), Names.Num() > Limit);');
    expect(code('AudioAuthoring', 'MetaSound', 'McpAudioMetaSoundGraphRead.cpp')).toMatch(/if \(SubAction == TEXT\("list_metasound_nodes"\)\)\s*\{\s*return ListMetaSoundNodeClasses\(Params, Response\);/u);
  });
});

describe('a component under a parent Blueprint part verifies', () => {
  // add_scs_component parentComponent "Hanger" (a parent Blueprint's part) attached the node, then failed
  // SCS_VERIFICATION_FAILED: the check accepted a parent recorded on the node only when it was native.
  it('a parent recorded on the node matches whether it is native or a parent Blueprint part', () => {
    const scs = code('SCS', 'McpSCSHandlers.cpp');
    expect(scs).toMatch(/if \(Node->ParentComponentOrVariableName\.IsNone\(\)\) \{\s*return false;/u);
    expect(scs).toContain('const bool bNativeParent = !ParentNode && !Node->ParentComponentOrVariableName.IsNone();');
    expect(scs).not.toContain('!Node->bIsParentComponentNative || Node->ParentComponentOrVariableName.IsNone()');
  });
  it('a MetaSound class with no variant is named without ".None"', () => {
    expect(code('AudioAuthoring', 'MetaSound', 'McpAudioMetaSoundNodeSearch.cpp'))
      .toContain('const FString Variant = ClassName.Variant.IsNone() ? FString() : ClassName.Variant.ToString();');
  });
});

describe('lists that only repeated the request are gone', () => {
  // create_material_instance with nine parameters answered nine {parameterName, applied: true} rows, and every edit_scs
  // operation carried its milliseconds and the subsystem call that made it.
  it('a parameter list names only the values that did not apply', () => {
    const params = code('MaterialAuthoring', 'Parameters', 'McpMatSetMatParam.cpp');
    expect(params).not.toContain('Entry->SetBoolField(TEXT("applied"), Reply.bSuccess);');
    expect(params).toMatch(/Entry->SetStringField\(TEXT\("error"\), Why\);\s*OutResults\.Add\(MakeShared<FJsonValueObject>\(Entry\)\);/u);
    expect(params).toContain('Data->SetNumberField(TEXT("applied"), Entries->Num() - Failed.Num());');
    expect(code('MaterialAuthoring', 'Creation', 'McpMatCreateMatInst.cpp')).toMatch(/if \(Failed\.Num\(\) > 0\) \{\s*Result->SetArrayField\(TEXT\("parameters"\), Results\);/u);
  });
  it('edit_scs operations carry no timing or subsystem detail', () => {
    expect(code('Blueprint', 'Components', 'McpBPModifyScs.cpp')).not.toContain('TEXT("durationMs")');
    expect(code('Blueprint', 'Components', 'McpBPModifyScsComponentOps.cpp')).not.toContain('TEXT("additionMethod")');
  });
});

describe('a console command returns what it logged on other threads', () => {
  // au.DumpActiveSounds listed every sound but the last: the audio thread logged it after Exec returned, and the log
  // passes other threads' lines on only when it flushes, by which time the capture was gone.
  it('flushes threaded logs until the log is quiet before the capture is removed', () => {
    const console = code('ConsoleCommand', 'McpConsoleHandlers.cpp');
    expect(console).toMatch(/for \(int32 Quiet = 0, Waited = 0; Quiet < 3 && Waited < 25; \+\+Waited\)\s*\{\s*const int32 Before = LogCapture\.Lines\.Num\(\);\s*FPlatformProcess::Sleep\(0\.02f\);\s*GLog->FlushThreadedLogs\(\);/u);
    expect(console.indexOf('GLog->FlushThreadedLogs();')).toBeLessThan(console.indexOf('GLog->RemoveOutputDevice(&LogCapture);'));
  });
});

describe('a parameter no pin takes is refused before anything is made', () => {
  // add_function inputs [{name: "Speed", type: "flaot"}] made a wildcard pin and answered success, and a nameless entry
  // was skipped; a graph custom event dropped a type it could not build and removed the old event before checking.
  it('add_function and add_event check every {name, type} first', () => {
    const resolver = code('..', 'Foundation', 'Blueprint', 'McpBlueprintUtilsTypeResolver.cpp');
    expect(resolver).toMatch(/if \(Name\.TrimStartAndEnd\(\)\.IsEmpty\(\)\)\s*\{\s*return FString::Printf/u);
    expect(resolver).toMatch(/const FTypeResolutionResult Resolved = ResolvePinType\(Type\);\s*if \(!Resolved\.bSuccess\)/u);
    const fn = code('Blueprint', 'Functions', 'McpBPAddFunction.cpp');
    expect(fn).toContain('McpBlueprintUtils::DescribeInvalidParam(Outputs, TEXT("outputs"))');
    expect(fn.indexOf('McpBlueprintUtils::DescribeInvalidParam(Inputs, TEXT("inputs"))')).toBeGreaterThan(0);
    expect(fn.indexOf('DescribeInvalidParam(Inputs')).toBeLessThan(fn.indexOf('FBlueprintEditorUtils::CreateNewGraph('));
    const event = code('Blueprint', 'Events', 'McpBPAddEventCustom.cpp');
    expect(event.indexOf('DescribeInvalidParam(Params, TEXT("parameters"))')).toBeGreaterThan(0);
    expect(event.indexOf('DescribeInvalidParam(Params')).toBeLessThan(event.indexOf('FGraphNodeCreator<UK2Node_CustomEvent>'));
  });
  it('a graph custom event refuses a type it cannot build and keeps the old event', () => {
    const events = code('BlueprintGraph', 'McpBPGraphCustomEvents.cpp');
    expect(events).toContain('Context.SendError(Error, TEXT("TYPE_RESOLUTION_FAILED"));');
    expect(events.indexOf('RemoveExistingCustomEvents(Context.Blueprint, EventName);'))
      .toBeGreaterThan(events.indexOf('if (!AddParameters(Context, TemporaryFunction, *Parameters))'));
    const params = code('BlueprintGraph', 'McpBPGraphCustomEventParams.cpp');
    expect(params).toMatch(/PinType\.PinCategory == UEdGraphSchema_K2::PC_Byte\)\s*\{\s*FByteProperty/u);
    expect(params).toContain('ObjectProperty->PropertyClass = ReferenceClass;');
    expect(params).toContain('ClassProperty->MetaClass = ReferenceClass;');
  });
});

describe('a pin descriptor says when it holds many values', () => {
  // An Array<Int> event input read back as pinType "int", the same as one int, and an enum or soft reference pin named
  // no sub-type: get_node_details and every pin summary only named one for an object, class or struct.
  it('both pin serializers add pinSubType, container and a map value type through one helper', () => {
    const queries = code('BlueprintGraph', 'McpBPGraphQueries.cpp');
    expect(queries).toMatch(/if \(PinType\.PinSubCategoryObject\.IsValid\(\)\)\s*\{\s*PinObject->SetStringField\(TEXT\("pinSubType"\)/u);
    expect(queries).toContain('PinObject->SetStringField(TEXT("container"), PinType.IsArray() ? TEXT("array") : PinType.IsSet() ? TEXT("set") : TEXT("map"));');
    expect(queries).toContain('PinObject->SetStringField(TEXT("valueType"), Value.TerminalSubCategoryObject.IsValid()');
    for (const file of ['McpBPGraphQueries.cpp', 'McpBPGraphDetails.cpp']) {
      const source = code('BlueprintGraph', file);
      expect(source).toContain('AddPinTypeFields(PinObject, Pin->PinType);');
      expect(source).not.toContain('Pin->PinType.PinCategory == TEXT("struct")) &&');
    }
  });
});

describe('an enum type keeps its enum when compiled', () => {
  // add_variable Enum:/Script/Engine.EMovementMode compiled to an int32 property: the resolver made a PC_Enum pin, and
  // the compiler has no PC_Enum case ("Failed to resolve the type-subtype, create a generic property"). A custom event
  // with that parameter failed to compile as "out-of-date". The editor's type picker makes a byte pin naming the enum.
  it('the shared resolver makes byte pins for enums, and struct summaries read them as enums', () => {
    const base = code('..', 'Foundation', 'Blueprint', 'McpBlueprintUtilsBaseTypes.cpp');
    expect(base).not.toContain('MakePin(K2::PC_Enum');
    expect(base.split('MakePin(K2::PC_Byte, NAME_None, Enum)').length - 1).toBe(2);
    // A bare enum name (EMovementMode) is no path FindObject takes; both resolvers scan for it by name.
    expect(base).toContain('for (TObjectIterator<UEnum> It; It && !Enum; ++It)');
    expect(code('BlueprintGraph', 'McpBPGraphPinTypes.cpp')).toContain('for (TObjectIterator<UEnum> It; It && !Enum; ++It)');
    const summary = code('AssetWorkflow', 'Structs', 'McpAssetStructsTypeHelpers.cpp');
    expect(summary).toMatch(/Pin\.PinCategory == UEdGraphSchema_K2::PC_Byte && Cast<UEnum>\(Pin\.PinSubCategoryObject\.Get\(\)\)/u);
    expect(summary).toContain('Vt.TerminalCategory == UEdGraphSchema_K2::PC_Enum || Vt.TerminalCategory == UEdGraphSchema_K2::PC_Byte');
  });
});

describe('remove_variable takes a list under one consent', () => {
  // Removing eight variables took sixteen calls: one name per call, and a describe for each consent.
  it('reads variableNames like set_variable_metadata and checks every name before removing any', () => {
    const remove = code('Blueprint', 'Variables', 'McpBPVariableRemovalRename.cpp');
    expect(remove).toContain('TArray<FString> VarNames = McpReadVariableNames(LocalPayload);');
    expect(remove.indexOf('McpMatchVariableNames(Blueprint, VarNames, Missing, Known);'))
      .toBeLessThan(remove.indexOf('FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, FName(*Name));'));
    expect(remove).toContain('Nothing was removed.');
    const metadata = code('Blueprint', 'Variables', 'McpBPVariableMetadata.cpp');
    expect(metadata).toContain('TArray<FString> VarNames = McpReadVariableNames(LocalPayload);');
    expect(metadata).toContain('McpMatchVariableNames(Blueprint, VarNames, Missing, Known);');
  });
});

describe('a graph filter finds what reads or moves a variable', () => {
  // A Tick that set the customs boom every frame froze the Set Relative Rotation that raised it; filter "Arm" listed
  // the getters and setters but not that node, so the competing writer had to be found by hand.
  it('matches a node whose input is wired from a variable or component getter of that name', () => {
    const queries = code('BlueprintGraph', 'McpBPGraphQueries.cpp');
    expect(queries).toContain('#include "K2Node_VariableGet.h"');
    expect(queries).toMatch(/Pin->Direction == EGPD_Input && Linked \? Cast<UK2Node_VariableGet>\(Linked->GetOwningNode\(\)\) : nullptr;\s*if \(Getter && Matches\(Getter->GetVarNameString\(\)\)\)/u);
  });
});

describe('a motion sample reports the velocity an actor moves at', () => {
  // A runaway cart moved by SetActorLocation crossed 570 cm/s while every sample said velocity [0, 0, 0].
  it('falls back to the velocity the sampled positions show when the engine reports none', () => {
    const sample = code('ControlActor', 'McpActorMotionSample.cpp');
    expect(sample).toContain('(Location - Run.Last) / (GameTime - Run.LastSampleGame)');
    expect(sample).toContain('McpPointVec(Actor->GetVelocity().IsNearlyZero() ? Moved : Actor->GetVelocity())');
  });
});

describe('a motion read waits for an actor the game spawns', () => {
  // A thrown block lived two seconds: by the time a call could name it, it was gone or not born yet.
  it('spawnOf listens to the world spawn event and hands the run the first actor of the class', () => {
    const sample = code('ControlActor', 'McpActorMotionSample.cpp');
    // The handler is a private member: the run starts through a callback the handler hands over.
    expect(sample).toMatch(/if \(TargetName\.IsEmpty\(\) && Payload->HasField\(TEXT\("spawnOf"\)\)\) \{[\s\S]*?McpSampleMotionOfNextSpawn\(this, RequestId, Payload, Socket, \[Weak, RequestId, Socket\][\s\S]*?Weak->HandleControlActorSampleMotion\(RequestId, Named, Socket\);\s*\}\);\s*return true;/u);
    const watch = code('ControlActor', 'Motion', 'McpMotionSpawnWatch.cpp');
    expect(watch).toContain('World->AddOnActorSpawnedHandler(');
    expect(watch).toContain('Now->RemoveOnActorSpawnedHandler(Watch->Handle);');
    expect(watch).toContain('Actor->IsA(Pinned->Class.Get())');
    expect(watch).toContain('Named->SetNumberField(TEXT("maxRealSeconds"), FMath::Max(1.0, RealLeft));');
    expect(watch).toContain('Run(Named);');
  });
});

describe('a graph read lists the nodes that do nothing', () => {
  // Deleting the MoveComponentTo nodes a physics rework replaced left two unlinked Get Arm getters behind; the
  // compiler says nothing about such leftovers, so nothing found them.
  it('unusedOnly keeps an unreached call or an unread pure node, never a chain start', () => {
    const queries = code('BlueprintGraph', 'McpBPGraphQueries.cpp');
    expect(queries).toContain('return bExecIn ? !bExecInLinked : (!bExecOut && bDataOut && !bDataOutLinked);');
    expect(queries).toContain('Context.Payload->TryGetBoolField(TEXT("unusedOnly"), bUnusedOnly);');
    expect(queries).toContain('(bUnusedOnly && !NodeDoesNothing(Node))');
  });
});

describe('a Niagara module add names the module as its inputs go by', () => {
  // The reply said CurlForce and Scale Sprite Size, while set_parameter_value wanted CurlNoiseForce.Noise Strength
  // and ScaleSpriteSize.Uniform Curve Scale.
  it('reads the name back from the module the stack made', () => {
    const modules = code('NiagaraAuthoring', 'McpNiagaraParticleModules.cpp');
    expect(modules).toContain('const UNiagaraNodeFunctionCall* Added = AddModuleToEmitterStack(Handle, ModulePath, Module->Usage, ModuleName);');
    expect(modules).toContain('ModuleName = Added->GetFunctionName();');
  });
});

describe('a listed or sampled property reads as a JSON value', () => {
  // list propertyNames answered "False", "2.200000" and an array of actors as one quoted export string, while
  // get_property answered the same properties as false, 2.2 and an array of paths.
  it('list and motion samples use the exporter get_property uses, with export text only where it has no form', () => {
    const reflection = code('..', 'Foundation', 'Reflection', 'McpPropertyReflection.cpp');
    expect(reflection).toMatch(/ExportPropertyToJsonValueOrText\(void\* TargetContainer, FProperty\* Property\)\s*\{\s*TSharedPtr<FJsonValue> Json = ExportPropertyToJsonValue\(TargetContainer, Property\);/u);
    const list = code('ControlActor', 'List', 'McpActorList.cpp');
    expect(list).toContain('McpPropertyReflection::ExportPropertyToJsonValueOrText(Owner, Property)');
    expect(list).toContain('McpPropertyReflection::ExportPropertyToJsonValueOrText(Container, Nested)');
    expect(list).not.toContain('GetPropertyValueAsString');
    expect(code('ControlActor', 'McpActorMotionSample.cpp'))
      .toContain('Values->SetField(Watched.Label, McpPropertyReflection::ExportPropertyToJsonValueOrText(Owner, Property));');
  });
});

describe('a folder delete counts the assets it held', () => {
  // Deleting a folder of two assets answered "deletedCount": 1, the one path asked for.
  it('reads the folder from the asset registry before it goes, and counts a single asset as one', () => {
    const mutation = code('AssetWorkflow', 'Operations', 'McpAssetAssetMutation.cpp');
    expect(mutation).toMatch(/GetAssetsByPath\(FName\(\*SafePath\), InFolder,\s*true\);/u);
    expect(mutation).toContain('DeletedAssets += InFolder.Num();');
    expect(mutation).toContain('++DeletedAssets;');
    expect(mutation).toContain('Resp->SetNumberField(TEXT("deletedCount"), DeletedAssets);');
  });
});

describe('a level viewport screenshot says when the viewport is not realtime', () => {
  // A fire placed in the level showed no flame in an editor screenshot: nothing ticks in a viewport that is not realtime.
  it('warns from the client it photographed', () => {
    expect(code('ControlEditor', 'McpEditorScreenshot.cpp')).toContain('ViewCaptureWarningsForMcp(CaptureClient);');
    const warnings = code('ControlEditor', 'Camera', 'McpViewSettle.cpp');
    expect(warnings).toContain('if (Client && !Client->IsRealtime())');
    expect(warnings).toContain('The level viewport is not realtime');
  });

  // Over its video memory budget the editor took 27 s for a soft capture, and only the line drawn into it said why.
  it('says when video memory is over budget', () => {
    expect(code('ControlEditor', 'Camera', 'McpViewSettle.cpp')).toMatch(/const double OverMB = double\(GDemotedLocalMemorySize\) \/ \(1024\.0 \* 1024\.0\);\s*if \(OverMB >= 1\.0\)/u);
  });
});

describe('a float property reads as it was typed', () => {
  // LightDecay 2.2 read back as 2.200000047683716: the float was widened to double before it was written out.
  it('writes a float through its seven significant digits, in arrays, maps, sets and Niagara module inputs too', () => {
    const reflection = code('..', 'Foundation', 'Reflection', 'McpPropertyReflection.cpp');
    expect(reflection).toContain('return FCString::Atod(*FString::Printf(TEXT("%.7g"), Value));');
    expect(reflection).toContain('ReadableFloat(FloatProp->GetPropertyValue_InContainer(TargetContainer))');
    const niagara = code('NiagaraAuthoring', 'McpNiagaraInfoValidate.cpp');
    expect(niagara.match(/McpPropertyReflection::ReadableFloat\(/gu)).toHaveLength(2);
    const arrays = code('..', 'Foundation', 'Reflection', 'McpPropertyReflectionArrays.cpp');
    expect(arrays).toContain('return ExportPropertyToJsonValueOrText(ElemPtr, Inner);');
  });
});

describe('a world edit in a paused game says nothing will move', () => {
  // A character moved into a game its title card had paused stayed in the air where it was put: it read as broken gravity.
  it('flags the paused play world on every reply and warns on world edits', () => {
    const responses = code('..', 'Core', 'Subsystem', 'EngineWireSubsystemResponses.cpp');
    expect(responses).toMatch(/ContextWorld->IsPlayInEditor\(\) && ContextWorld->IsPaused\(\)\)\s*\{\s*EffectiveResult->SetBoolField\(TEXT\("gamePaused"\), true\);/u);
    const native = code('..', 'MCP', 'Execute', 'McpNativeReceiptEnrichment.cpp');
    const warning = 'The game is paused (a title screen, a pause menu or control_editor pause): nothing it moves or simulates advances until play resumes (control_editor resume with unpauseGame).';
    expect(native).toContain(warning);
  });
});

describe('an undeclared key that is a declared parameter value names that parameter', () => {
  // get_transform with motion: true answered "allowed: ..., readMode, ..." and never said motion was a readMode value.
  it('builds the hint natively', () => {
    const hints = code('..', 'MCP', 'Execute', 'Request', 'McpNativeGatewayParameterHints.h');
    expect(hints).toContain('return FString::Printf(TEXT("did you mean %s: \'%s\'; "), *Name, *Text);');
    const keywords = code('..', 'MCP', 'Execute', 'McpNativeGatewaySchemaKeywords.cpp');
    expect(keywords).toContain('const FString EnumHint = McpParameterHints::EnumValueHint(Key, Properties, Declared);');
    expect(keywords).toContain('EnumHint.IsEmpty() ? *Hint : *EnumHint');
  });
});

describe('a transform reads as it was typed', () => {
  // A 90 degree yaw read 89.99999999999999 and a camera y 1710.5776600334307: noise from the transform math.
  it('rounds every vector and rotator of a reply to ten significant digits', () => {
    const transforms = code('..', 'Foundation', 'HandlerUtils', 'McpHandlerUtilsTransforms.h');
    expect(transforms).toContain('FCString::Atod(*FString::Printf(TEXT("%.10g"), Value))');
    for (const field of ['Vector.X', 'Vector.Y', 'Vector.Z', 'Rotator.Pitch', 'Rotator.Yaw', 'Rotator.Roll']) {
      expect(transforms.split(`ReadableDouble(${field})`).length - 1, field).toBe(2);
    }
  });
});

describe('a material dead branch lists as orphaned', () => {
  // A Multiply left behind when the node it fed was rewired still read its live input, so the both-ways walk
  // marked it live and the material carried it unseen.
  it('walks from the outputs through what each node reads only', () => {
    const subgraph = code('MaterialAuthoring', 'Queries', 'McpMatGetConnectedSubgraph.cpp');
    expect(subgraph).toContain('Reads.Add(Expr, Input.Expression);');
    expect(subgraph).toContain('Reads.MultiFind(Cur, Neighbors);');
    expect(subgraph).toContain('Cast<UMaterialExpressionCustomOutput>(Expr)');
  });
});

describe('a key a focused game widget takes names that widget', () => {
  // A title screen left up with its Play button focused took every jump key as a press, and the replies only
  // said the pawn never moved.
  it('names the focused widget in the key reply, the resume warning and the motion warning', () => {
    const routing = code('ControlEditor', 'McpEditorInputRouting.cpp');
    expect(routing).toContain('TEXT("%s: %s (delivered to PIE)%s"), Verb, *Key, *FocusNote)');
    const play = code('ControlEditor', 'McpEditorPlay.cpp');
    expect(play).toContain('const FString Focused = FocusedPieWidgetForMcp();');
    expect(play).toContain('Resp->SetArrayField(TEXT("warnings"), Warnings);');
    // Read as each key goes down: a pressed menu button can close its menu before the run ends.
    const motion = code('ControlActor', 'McpActorMotionInputs.cpp');
    expect(motion.indexOf('Input.Focused = FocusedPieWidgetForMcp();')).toBeGreaterThan(0);
    expect(motion.indexOf('Input.Focused = FocusedPieWidgetForMcp();')).toBeLessThan(
      motion.indexOf('McpSendMotionKey(Input.Key, true);'));
    expect(motion).toContain('*Taken->Focused, *Taken->Key');
    const widgets = code('ControlEditor', 'McpEditorWidgetInput.cpp');
    expect(widgets).toContain('if (Owner->HasKeyboardFocus())');
    expect(widgets).toContain('Child->HasKeyboardFocus()');
    // Named before the key lands: a pressed button can close its menu and take the focus with it.
    expect(widgets.indexOf('const FString Focused = FocusedPieWidgetForMcp();')).toBeLessThan(
      widgets.indexOf('const FKeyEvent KeyEvent(Key'));
  });
});

describe("a Set wired to another object is not this one's write", () => {
  // Two Set HomeX nodes wired to spawned copies, beside a Get of this one's HomeX, read as this object's HomeX set
  // twice, and a compile audit flagged a graph that was right.
  it('reads a variable node as acting on this object only while its target pin is unwired', () => {
    const checks = code('..', 'Foundation', 'BridgeHelpers', 'Blueprints', 'McpBPGraphChecks.cpp');
    expect(checks).toContain('(!Target || Target->LinkedTo.Num() == 0)');
    expect(checks).toContain('return McpVariableOnSelf(Set) ? Set->GetVarName() : NAME_None;');
    expect(checks).toContain('return Get->GetVarName() == Variable && McpVariableOnSelf(Get);');
    expect(checks).toContain('if (Get && McpVariableOnSelf(Get))');
    const wait = code('..', 'Foundation', 'BridgeHelpers', 'Blueprints', 'McpBPWaitRereadCheck.cpp');
    expect(wait).toContain('Set && McpVariableOnSelf(Set) && Set->GetVarName() == Get->GetVarName()');
  });
});

describe('a compiler warning reaches the receipt', () => {
  // "DoesSaveGameExist was pruned because its Exec pin is not connected" sat in diagnostics under "the blueprint
  // compiles": the node never ran and nothing said so.
  it('carries the compiler warnings with the graph checks and names them in a batch headline', () => {
    const diag = code('..', 'Foundation', 'BridgeHelpers', 'Blueprints', 'McpBPDiag.h');
    expect(diag).toContain('if (Index >= FirstCheck || Entry->GetStringField(TEXT("severity")) == TEXT("warning")) {');
    expect(diag).toContain('if (Warnings.Num() > Before) {');
    const batch = code('BlueprintGraph', 'McpBPGraphBatch.cpp');
    expect(batch).toContain('TEXT(", with warnings (see the receipt\'s warnings)")');
  });
});

describe('a Blueprint struct reads as an object under its authored names', () => {
  // A leaderboard of F_ScoreEntry read back as "(PlayerName_2_0AD1D94C...=\"Di\",Score_4_D0AA...=70)".
  it('exports each member under GetAuthoredName, before the native export text', () => {
    const reflection = code('..', 'Foundation', 'Reflection', 'McpPropertyReflection.cpp');
    expect(reflection).toContain('Cast<UUserDefinedStruct>(StructProp->Struct)');
    expect(reflection).toContain('Members->SetField(It->GetAuthoredName(), ExportPropertyToJsonValueOrText(StructMemory, *It));');
    expect(reflection.indexOf('Cast<UUserDefinedStruct>(StructProp->Struct)')).toBeLessThan(
      reflection.indexOf('StructProp->Struct->ExportText(Exported'));
  });
});

describe('a function call lists outputs and unset parameters only when it has some', () => {
  it('leaves both out of a call with no return value and every argument given', () => {
    for (const source of [code('ControlActor', 'McpActorAdvanced.cpp'), code('ControlEditor', 'McpEditorReflectedInvoke.cpp')]) {
      expect(source).toMatch(/if \(Outputs\.IsValid\(\) && Outputs->Values\.Num\(\) > 0\) \{\s*\w+->SetObjectField\(TEXT\("outputs"\), Outputs\);/u);
      expect(source).toMatch(/if \(Unset\.Num\(\) > 0\) \{\s*\w+->SetArrayField\(TEXT\("unsetParameters"\), Unset\);/u);
    }
  });
});

describe('a shop takes its own items', () => {
  // Every card read "Item 1" and "100" under "Gold: 0": two text edits a card before the shop said anything.
  it('names each card and its price from items, and the header from currency', () => {
    const screens = code('WidgetAuthoring', 'Templates', 'McpWidgetScreensMore.cpp');
    expect(screens).toContain('Payload->TryGetArrayField(TEXT("items"), Values)');
    expect(screens).toContain('Listed.Emplace(Name, FText::AsNumber(Price).ToString());');
    expect(screens).toContain('GetJsonStringField(Payload, TEXT("currency"), TEXT("Gold"))');
    expect(screens).toContain('Items = Listed.Num();');
  });
});

describe('a Math Expression lists inputPins only when it made pins', () => {
  it('leaves the list out for a formula that reads only member variables', () => {
    const expression = code('BlueprintGraph', 'Expression', 'McpBPGraphMathExpr.cpp');
    expect(expression).toMatch(/if \(Inputs\.Num\(\) > 0\)\s*\{\s*Result->SetArrayField\(TEXT\("inputPins"\), Inputs\);/u);
  });
});

describe('a frame step says what the game logged during it', () => {
  // A Blueprint turning a Stationary light stepped on silently: "has to be 'Movable'" went only to the log.
  it('reads the warnings and errors logged since the step began into the reply', () => {
    const play = code('ControlEditor', 'McpEditorPlay.cpp');
    expect(play.indexOf('const double Since = FMcpLogHistory::Now();')).toBeGreaterThan(-1);
    expect(play.indexOf('const double Since = FMcpLogHistory::Now();')).toBeLessThan(play.indexOf('AdvanceOneFrame(GEditor->PlayWorld);'));
    expect(play).toContain('FMcpLogHistory::Get().WarningsSince(Since, 5)');
    expect(play).toContain('Resp->SetArrayField(TEXT("warnings"), McpHandlerUtils::ToJsonStringArray(Logged));');
    const history = code('Log', 'McpLogHistory.cpp');
    expect(history).toContain('Line.Verbosity <= ELogVerbosity::Warning && Line.Category != OwnCategory');
    expect(history).toContain('EngineWireSubsystemResponse::SanitizeEngineErrorForResponse(Raw)');
    // A motion read runs game time the same way, from the moment its run is made.
    const motion = code('ControlActor', 'McpActorMotionSample.cpp');
    expect(motion).toContain('LogSince = FMcpLogHistory::Now();');
    expect(motion).toContain('Warnings.Append(FMcpLogHistory::Get().WarningsSince(Run->LogSince, 5));');
  });

  // An empty Raise Script Error message matched every line and hid the whole window's warnings; ten script errors
  // pushed every other warning off the list with no sign anything was cut.
  it('ignores a blank script error message and keeps half the lines for other warnings, naming a cut', () => {
    const history = code('Log', 'McpLogHistory.cpp');
    expect(history).toContain('ScriptMessages.RemoveAll([](const FString& Text) { return Text.TrimStartAndEnd().IsEmpty(); });');
    expect(history).toContain('FMath::Max(MaxDistinct - Messages.Num(), MaxDistinct / 2)');
    expect(history).toContain('more distinct warnings or Blueprint errors not listed');
  });
});

describe('setup_physics_simulation makes the actor or mesh it is given simulate', () => {
  // It saved an empty Physics Asset (no bodies, nothing could simulate) and answered "setup completed".
  it('makes bodies for the skeletal mesh and refuses when none could be made', () => {
    const setup = code('Animation', 'Physics', 'McpAnimSetupPhysicsSimulation.cpp');
    expect(setup).toContain('FPhysicsAssetUtils::CreateFromSkeletalMesh(PhysicsAsset, TargetMesh, FPhysAssetCreateParams(),');
    expect(setup).toContain('PhysicsAsset->SkeletalBodySetups.Num() == 0) {');
    expect(setup).toContain('TEXT("BODY_GENERATION_FAILED")');
    expect(setup.indexOf('PhysicsAsset->MarkAsGarbage();'), 'the empty asset is never left behind').toBeGreaterThan(-1);
  });

  // A crate given by actorName was refused for having no skeletal mesh, though search offers this call for a pushable object.
  it('makes a placed prop simulate, as one undo step, and says when the part is not the root', () => {
    const setup = code('Animation', 'Physics', 'McpAnimSetupPhysicsSimulation.cpp');
    expect(setup).toMatch(/\} else if \(SimulatePlacedProp\(Context, FoundActor\)\) \{\s*return false;/u);
    const prop = code('Animation', 'Physics', 'McpAnimPhysicsProp.cpp');
    expect(prop).toContain('EMcpMutationDurability::EditorStateOnly, {Body});');
    expect(prop).toContain('Body->SetSimulatePhysics(true);');
    expect(prop).toContain('!Mesh->GetStaticMesh()->GetBodySetup()');
    expect(prop).toContain('if (Body != Actor->GetRootComponent()) {');
    expect(prop).toContain('Transaction.DescribeInto(Context.Resp);');
  });
});

describe('widget_list says what a widget reads and where a nested one sits', () => {
  // An inventory filled at runtime listed its three rows with no text and no link to the panel holding them.
  it('lists the visible texts that are no button label, and the owner and panel of a widget added to another', () => {
    const input = code('ControlEditor', 'McpEditorWidgetInput.cpp');
    expect(input).toContain('Text->IsVisible() && !Cast<UButton>(Text->GetParent()) && !Text->GetText().IsEmpty() && Texts.Num() < 20');
    expect(input).toContain('Entry->SetArrayField(TEXT("texts"), McpHandlerUtils::ToJsonStringArray(Texts));');
    expect(input).toContain('const UPanelWidget *Panel = Owner->IsInViewport() ? nullptr : Owner->GetParent();');
    expect(input).toContain('Entry->SetStringField(TEXT("in"), Outer->GetName() + TEXT(".") + Panel->GetName());');
  });
});

describe('a mouse move in a running game moves the game\'s own cursor', () => {
  // A top-down aim read no mouse at all: the move was a Slate event at desktop coordinates, which never reached the
  // viewport the game reads its cursor from.
  it('hands the game viewport its own move in viewport pixels, before the editor path, without the system cursor', () => {
    const routing = code('ControlEditor', 'McpEditorInputRouting.cpp');
    const move = routing.indexOf('if (McpSimulateGameMouseMove(X, Y, bSuccess, bRoutedToPIE, bHandledByPIE, Message)) {');
    expect(move).toBeGreaterThan(-1);
    expect(move, 'before the Slate event at desktop coordinates').toBeLessThan(routing.indexOf('bHandledBySlate = SlateApp.ProcessMouseMoveEvent(MoveEvent);'));
    const cursor = code('ControlEditor', 'Input', 'McpEditorGameCursor.cpp');
    expect(cursor).toContain('Geometry.LocalToAbsolute(FVector2D(X * Local.X / Pixels.X, Y * Local.Y / Pixels.Y))');
    expect(cursor).toContain('Viewport->OnMouseMove(Geometry, Move);');
    expect(cursor).toContain('X >= Pixels.X || Y >= Pixels.Y');
    expect(cursor).not.toContain('SetPosition(');
  });
});

describe('a game started in a minimized editor says what that does to physics', () => {
  // A crate fell off a moving platform: at 3 fps each physics step is capped at 1/30 s, so physics runs a tenth as
  // fast as the game clock that moves the platform. The warning named only frame rate and widgets.
  it('names the slowed physics and asks for the window before physics or timing is tested', () => {
    const play = code('ControlEditor', 'McpEditorPlay.cpp');
    expect(play).toContain('physics at a tenth of the game clock');
    expect(play).toContain('before testing physics or timing');
  });
});

describe('a packaged game can take its own executable name', () => {
  // A packaged game was always the project's name (its launcher <Project>.exe): the launcher, the binaries and an Android
  // package are named after the game target built, and nothing in the tool could name or choose that target.
  it('copies the project game target under the new name and builds that target', () => {
    const target = code('SystemControl', 'McpSystemGameTarget.cpp');
    expect(target).toContain('Text.ReplaceInline(*OldClass, *(Name + TEXT("Target")), ESearchCase::CaseSensitive);');
    expect(target, 'only a game target is copied or reused').toContain('Text.Contains(TEXT("TargetType.Game"))');
    expect(target, 'a name that is not an identifier writes nothing').toContain('OutCode = TEXT("INVALID_ARGUMENT");');
    const pack = code('SystemControl', 'McpSystemPackage.cpp');
    expect(pack).toContain('Line += FString::Printf(TEXT(" -target=%s"), *Target);');
    expect(pack).toContain('Result->SetBoolField(TEXT("targetCreated"), bTargetCreated);');
    const launch = code('SystemControl', 'McpSystemLaunchBuild.cpp');
    expect(launch, 'launch_build finds a game packaged under another name').toContain('FPaths::Combine(BinDir, TEXT("*.exe"))');
  });
});

describe('a widget rename refuses a name the class already holds', () => {
  // A widget renamed to DisplayLabel (a UWidget property) answered "Renamed widget" and the Widget Blueprint stopped
  // compiling: the add paths checked McpWidgetNameConflict, the rename did not.
  it('checks the new name like the add paths do, before anything is renamed', () => {
    const rename = code('WidgetAuthoring', 'Support', 'McpWidgetManipulation.cpp');
    const check = rename.indexOf('McpWidgetNameConflict(WidgetBP, FName(*NewName), TargetWidget->GetClass())');
    expect(check).toBeGreaterThan(-1);
    expect(check, 'before the widget is renamed').toBeLessThan(rename.indexOf('TargetWidget->Rename(*NewName, nullptr, REN_DontCreateRedirectors);'));
    expect(rename).toContain('TEXT("NAME_CONFLICT")');
  });
});

describe('a clean final compile settles compiler lines logged before it', () => {
  // A batch whose add_function made an OnKeyDown override compiled part-way, and the reply kept that half-built
  // graph's "Return Value should not be empty" warning although its final compile was clean (warningCount 0).
  it('drops captured [Compiler] lines when the reply says compilerStatus UpToDate', () => {
    const enrich = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Core', 'Subsystem', 'EngineWireSubsystemResponseEnrichment.h'), 'utf8');
    expect(enrich).toContain('Result->TryGetStringField(TEXT("compilerStatus"), CompilerStatus)');
    expect(enrich).toContain('CompilerStatus == TEXT("UpToDate")');
    expect(enrich).toContain('WarningMessages.RemoveAll(IsCompilerLine)');
    expect(enrich).toContain('ErrorMessages.RemoveAll(IsCompilerLine)');
  });
});

describe('a smoke run that wrote no log does not read as a clean run', () => {
  // A Shipping game logs nothing, and package_status answered errorCount 0 and mapsLoaded [] for its launch_build run,
  // which read as "reached no level, hit no error" although nothing had been seen; configuration came back empty.
  it('omits the log counts and warns when the game wrote no log', () => {
    const launch = code('SystemControl', 'McpSystemLaunchBuild.cpp');
    const empty = launch.indexOf('if (Text.IsEmpty())');
    expect(empty).toBeGreaterThan(-1);
    expect(empty, 'before any count is set').toBeLessThan(launch.indexOf('Result->SetNumberField(TEXT("errorCount"), Errors);'));
    expect(launch).toContain('a Shipping build logs nothing');
    expect(launch, 'the configuration comes from the executable name').toContain('FPaths::GetBaseFilename(Exe).Split(TEXT("-Win64-"), nullptr, &Job.Configuration);');
  });
});

describe('an asset delete says nothing of its own routine', () => {
  // Every Blueprint loaded from disk leaves its file behind after the engine delete, and the cleanup that removes it
  // logged a warning each time, so a 62-asset delete reply carried that line for each asset as if something failed.
  it('logs the routine leftover-file cleanup at Log and warns only when the package stays loaded', () => {
    const safety = readFileSync(join(DOMAINS, '..', 'Safety', 'McpSafeOperationsAssetDelete.h'), 'utf8');
    expect(safety).toMatch(/UE_LOG\(LogMcpSafeOperations, Log,\s*TEXT\("McpRemoveLeftoverPackageFile: '%s' was deleted in memory/u);
    expect(safety).toMatch(/UE_LOG\(LogMcpSafeOperations, Warning,\s*TEXT\("McpRemoveLeftoverPackageFile: '%s' is still loaded/u);
  });
});

describe('a plugin can be kept to the editor, and a smoke run can open a given level', () => {
  // enable_plugin could only switch a plugin on or off, so an editor tool and every plugin it brought in went into the
  // packaged game; the engine's SetPluginEnabled also drops a reference that matches the plugin's default state,
  // whatever targets it lists, so the reference is written here.
  it('writes the reference with its targets, reads it back, then reloads the project', () => {
    const plugins = code('SystemControl', 'McpSystemPlugins.cpp');
    expect(plugins).toContain('Ref->TargetAllowList = Targets;');
    expect(plugins, 'a file that does not load back never reaches LoadProjectFile').toContain('!Written.Load(ProjectFile, FailReason)');
    expect(plugins).toContain('IProjectManager::Get().LoadProjectFile(ProjectFile);');
    expect(plugins, 'a plugin on only through another plugin gets its own reference')
      .toContain('!Plugin->IsEnabledByDefault(true) && !McpProjectEnablesPlugin(PluginName)');
    expect(plugins, 'a plugin left out of the game whose folder is always cooked fails the next package: say so')
      .toContain('!Targets.Contains(EBuildTargetType::Game) && McpAlwaysCooksContentOf(Plugin)');
  });

  it('checks the level before any build is looked up and warns when the game never loaded it', () => {
    const launch = code('SystemControl', 'McpSystemLaunchBuild.cpp');
    const check = launch.indexOf('if (!Map.IsEmpty() && !bMapIsName)');
    expect(check).toBeGreaterThan(-1);
    expect(check, 'before any build is looked up').toBeLessThan(launch.indexOf('FindGameExecutable(ArchiveDir)'));
    expect(launch).toContain('const FString MapArg = Map.IsEmpty() ? FString() : Map + TEXT(" ");');
    expect(launch).toContain('The game never logged loading %s');
  });
});

describe('a Blueprint runtime error names the node that raised it', () => {
  // An Accessed None reached a frame step only as a LogScript line with a byte offset, and from 5.8 once per object.
  it('reads each script exception of a play session back to its node, graph and Blueprint', () => {
    const errors = code('Log', 'McpScriptErrors.cpp');
    expect(errors).toContain('FBlueprintCoreDelegates::OnScriptException.AddRaw(this, &FMcpScriptErrors::OnScriptException);');
    expect(errors).toContain('FEditorDelegates::BeginPIE.AddLambda');
    expect(errors).toContain('static_cast<int32>(StackFrame.Code - StackFrame.Node->Script.GetData()) - 1');
    expect(errors).toContain('FKismetDebugUtilities::FindSourceNodeForCodeLocation(ActiveObject, StackFrame.Node, Offset, true)');
    expect(errors).toContain('Error.NodeId = Node->NodeGuid.ToString();');
    expect(errors).toMatch(/Type == EBlueprintExceptionType::Breakpoint \|\| Type == EBlueprintExceptionType::Tracepoint \|\|\s*Type == EBlueprintExceptionType::WireTracepoint/u);
    expect(errors).toContain('World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game');
    // BlueprintExceptionInfo.h exists from 5.4 on; earlier engines declare the struct in UObject/Script.h.
    expect(errors).toMatch(/#if ENGINE_MAJOR_VERSION > 5 \|\| \(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4\)\s*#include "Blueprint\/BlueprintExceptionInfo\.h"/u);
    const lifecycle = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Core', 'Subsystem',
      'EngineWireSubsystemLifecycle.cpp'), 'utf8');
    expect(lifecycle).toContain('FMcpScriptErrors::Get().Watch(true);');
    expect(lifecycle).toContain('FMcpScriptErrors::Get().Watch(false);');
  });

  it('frame steps and motion reads lead with the error and drop the log lines repeating it', () => {
    // Both the LogScript warning and the PIE message log's "Blueprint Runtime Error" quote the error's text.
    const history = code('Log', 'McpLogHistory.cpp');
    expect(history).toContain('TArray<FString> Result = FMcpScriptErrors::Get().LinesSince(SinceSeconds, ScriptMessages);');
    expect(history).toMatch(/Line\.Category != OwnCategory &&\s*!ScriptMessages\.ContainsByPredicate\(\[&Line\]\(const FString& Text\) \{ return Line\.Message\.Contains\(Text\); \}\)/u);
    expect(history).toContain('Result.Append(Messages);');
  });

  it('runtime_report lists the session errors, kept after stop until the next play', () => {
    const runtime = code('Environment', 'Inspection', 'McpEnvInspectRuntime.cpp');
    expect(runtime).toMatch(/const TArray<TSharedPtr<FJsonValue>> ScriptErrors = FMcpScriptErrors::Get\(\)\.ToJson\(\);\s*if \(ScriptErrors\.Num\(\) > 0\)\s*\{\s*Report->SetArrayField\(TEXT\("scriptErrors"\), ScriptErrors\);/u);
  });
});

describe('runtime_report filter matches a path only whole', () => {
  // Every actor's path holds its level's name, so a filter naming the level listed every actor in it.
  it('matches name, label and class by part and the object path only exactly', () => {
    const runtime = code('Environment', 'Inspection', 'McpEnvInspectRuntime.cpp');
    expect(runtime).toContain('Actor->GetPathName().Equals(Filter, ESearchCase::IgnoreCase);');
    expect(runtime).not.toContain('Actor->GetPathName().Contains(Filter)');
  });
});

describe('the plugin links on UE 5.0', () => {
  // UE 5.0 declares AddScriptModuleToStack without exporting it, and 5.0 and 5.1 inline CopyParameterData with a
  // LogNiagara line the Niagara module does not export: either call left the 5.0 build with unresolved externals.
  it('calls AddScriptModuleToStack only where the engine exports it', () => {
    for (const file of [['NiagaraAuthoring', 'McpNiagaraStackGraph.cpp'], ['NiagaraGraph', 'McpNiagaraGraphHandlers.cpp']]) {
      const source = code(...file);
      const calls = source.split('FNiagaraStackGraphUtilities::AddScriptModuleToStack(').length - 1;
      expect(calls, file.join('/')).toBeGreaterThan(0);
      expect(source.match(/#if MCP_HAS_NIAGARA_STACK_GRAPH_UTILITIES[^#]*FNiagaraStackGraphUtilities::AddScriptModuleToStack\(/gu)?.length, file.join('/')).toBe(calls);
    }
  });

  it('copies a user parameter without the inline CopyParameterData on 5.0 and 5.1', () => {
    const copy = code('NiagaraAuthoring', 'McpNiagaraEmitterCopy.cpp');
    expect(copy).toMatch(/#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 2[^#]*Target\.SetParameterData\(Data, Param\);\s*\}\s*#else\s*Source->GetExposedParameters\(\)\.CopyParameterData\(Target, Param\);\s*#endif/u);
  });
});

describe('what the native smoke run found', () => {
  const privateCode = (...segments: readonly string[]): string =>
    readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', ...segments), 'utf8')
      .replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

  // IPlatformFile::MoveFile is a plain MoveFileW on Windows: every rewrite of a diagnostics record after the first
  // failed and left the temp beside a stale file.
  it('a diagnostics record replaces the file it rewrites', () => {
    const snapshot = privateCode('Foundation', 'Diagnostics', 'McpDiagnosticsSnapshot.cpp');
    expect(snapshot).toContain('return IFileManager::Get().Move(*TargetPath, *TempPath,  true);');
    expect(snapshot).not.toContain('PlatformFile.MoveFile(*TargetPath, *TempPath)');
  });

  // start_render's record and handler say renderTimeoutMs; the request timeout read timeoutMs and cut an hour-long
  // render off after 335 s.
  it('a render request lives as long as the render it asked for', () => {
    const policy = privateCode('MCP', 'Transport', 'McpNativeTransportTimeoutPolicy.cpp');
    expect(policy).toContain('Arguments->TryGetNumberField(TEXT("renderTimeoutMs"), RequestedTimeoutMs)');
  });
});

describe('get_frame_cost names what bounds the frame', () => {
  const source = code('Environment', 'Inspection', 'Performance', 'McpEnvInspectFrameCost.cpp');

  it('is answered by its own handler', () => {
    expect(code('Environment', 'Inspection', 'McpEnvInspect.cpp'))
      .toMatch(/LowerSubAction\.Equals\(TEXT\("get_frame_cost"\)\)\)\s*\{\s*return HandleInspectFrameCostAction\(/u);
  });

  // An unfocused editor waits 333 ms between frames while its busiest thread works 4 ms: that is no bottleneck.
  it('says the frame waits when no thread fills it, and keeps Nanite meshes out of the heavy list', () => {
    expect(source).toMatch(/if \(Ms < T\.Frame \* 0\.5\)/u);
    expect(source).toMatch(/if \(Asset->HasValidNaniteData\(\)\)\s*\{\s*\+\+Nanite;\s*\}\s*else/u);
  });

  // An RHI-bound frame read "Render-thread-bound", and in Play a game thread the editor filled read as a slow game.
  it('names each thread it can pick, and the editor share of the game thread in Play', () => {
    expect(source).toContain('TEXT("RHI-thread")');
    expect(source).toMatch(/bGame && T\.bPie && T\.WorldTick >= 0\.0 && T\.WorldTick < Ms \* 0\.5/u);
  });

  it('declares every field it sends', () => {
    const record = capabilityIndex().records.find((entry) => String(entry.id) === 'inspect.get_stats');
    const declared = Object.keys(record?.schemas.output.properties ?? {});
    const sent = [...source.matchAll(/Resp->Set\w+Field\(TEXT\("(\w+)"\)/gu)].map((match) => String(match[1]));
    expect(sent.length).toBeGreaterThan(10);
    for (const field of sent.filter((name) => name !== 'warnings')) expect(declared, field).toContain(field);
  });
});

describe('diff_asset compares without writing', () => {
  const diff = code('AssetWorkflow', 'Analysis', 'McpAssetDiff.cpp');

  // The editor's own diff loads a revision this way; the copy is released and deleted after the read.
  it('loads a copy of the saved file for diffing and releases it', () => {
    expect(diff).toContain('LoadPackage(nullptr, *TempPackageName, LOAD_ForDiff | LOAD_DisableCompileOnLoad)');
    expect(diff).toMatch(/ResetLoaders\(Package\);[\s\S]*?ClearFlags\(RF_Standalone\)[\s\S]*?DeleteDirectory\(\*Dir, false, true\)/u);
    expect(diff).not.toMatch(/McpSafeAssetSave|SaveLoadedAsset|SavePackage/u);
  });

  it('leaves out what a package does not keep', () => {
    expect(diff).toContain('CPF_Transient | CPF_DuplicateTransient | CPF_Deprecated');
  });

  it('is answered by its own handler', () => {
    expect(code('AssetWorkflow', 'McpAssetHandlers.cpp'))
      .toMatch(/Lower == TEXT\("diff_asset"\)\)\s*return McpAssetDiff::HandleDiffAsset\(/u);
  });
});

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');
const privateCode = (...segments: readonly string[]): string =>
  readFileSync(join(PRIVATE, ...segments), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('long cinematic work: tracks, settings, batches, rate limits and renders', () => {
  // MovieScene3DTransformTrack_1 also matched _10 to _19, and remove_track could take the first of those.
  it('a whole track name picks that track before any that contains it', () => {
    const support = code('Sequence', 'McpSeqEditorSupport.h');
    expect(support).toContain('for (int32 Step = 0; Step < 3; ++Step)');
    expect(support).toMatch(/Step == 0\s*\? Name\.Equals\(TrackName, ESearchCase::IgnoreCase\)/u);
    expect(support).toContain('Out.RemoveAll([&Whole](UMovieSceneTrack *Track) { return !Whole(Track); });');
  });

  it('environment settings take objects and soft references through the shared writer', () => {
    expect(code('Environment', 'Runtime', 'McpEnvReflectSettings.cpp')).toContain('if (::ApplyJsonValueToProperty(Target, Property, Pair.Value, ApplyError))');
  });

  it('the pre-queue scans reach every value of a full batch', () => {
    expect(privateCode('Foundation', 'McpCapabilityAuthorization.h')).toContain('inline constexpr int32 McpMaxPayloadScanNodes = 262144;');
    for (const file of [['Foundation', 'McpCapabilityPathScan.cpp'], ['Core', 'Security', 'McpPrequeueGate.cpp']]) {
      expect(privateCode(...file), file.join('/')).toContain('constexpr int32 MaxScanNodes = McpMaxPayloadScanNodes;');
    }
    expect(privateCode('Core', 'Security', 'McpPrequeueGate.cpp')).not.toContain('Key.ToLower()');
  });

  it('a rate-limited request names its wait and sends Retry-After', () => {
    expect(privateCode('MCP', 'Protocol', 'McpJsonRpc.cpp')).toContain('Data->SetNumberField(TEXT("retryAfterSeconds"), RetryAfterSeconds);');
    for (const file of [['MCP', 'Transport', 'McpNativeTransportConnection.cpp'], ['MCP', 'Protocol', 'McpNativeTransportModern.cpp']]) {
      expect(privateCode(...file), file.join('/')).toContain('FMcpJsonRpc::RetryAfterHeader(RetryAfter)');
    }
  });

  it('saves and restarts wait for a running render, which cancel_render stops', () => {
    expect(code('ControlEditor', 'McpEditorAssets.cpp'))
      .toMatch(/if \(McpMovieRenderRunning\(\)\) \{\s*SendAutomationError\(Socket, RequestId, McpMovieRenderRunningMessage\(\), TEXT\("RENDER_IN_PROGRESS"\)\);/u);
    expect(code('ControlEditor', 'McpEditorRestart.cpp')).toContain('TEXT("RENDER_IN_PROGRESS"), McpMovieRenderRunningMessage()');
    expect(code('Sequence', 'MovieRender', 'McpSeqMRQRouting.cpp')).toContain('{TEXT("cancel_render"), &HandleCancelRender}');
    expect(code('Sequence', 'MovieRender', 'McpSeqMRQJobCreation.cpp')).toContain('Executor->CancelAllJobs();');
  });
});

describe('coplanar faces no camera can see', () => {
  // Carpet trims whose bottoms lay in the carpet's plane on the floor were reported, and fixed, as flicker.
  it('leaves out two undersides with something solid right below them', () => {
    const faces = code('ControlActor', 'Placement', 'McpCoplanarFaces.cpp');
    expect(faces).toContain('if (Normal.Z > -0.99)');
    expect(faces).toContain('if (Best.OverlapU <= 0.0 || McpCoplanarHiddenUnderside(World, Best.Normal, SmallerCenter, A.Actor, B.Actor))');
  });
});

describe('describe_view names what covers part of a named actor', () => {
  // A sign half behind a row of blocks still drew pixels, so it read as seen and nothing named what covered it.
  it('measures nearer actors over a visible actor box as it does for an unseen one', () => {
    expect(code('Environment', 'Inspection', 'McpEnvInspectViewContents.cpp')).toContain('McpCoveringActors(Row.Actor, *Client, Size, Ids, Proxies)');
    const unseen = code('Environment', 'Inspection', 'McpEnvInspectViewUnseen.cpp');
    expect(unseen).toContain('Covers.Remove(Actor);');
    expect(unseen).toContain('McpNearerCovers(Actor, Client, Size, Ids, Proxies, 5.0, InFrame, Covers);');
  });
});

describe('set_viewport_realtime takes effect over another override', () => {
  // A minimized editor holds the viewport off realtime: the toggle said enabled while every capture still found it not.
  it('puts its own override on top when another system holds the viewport, and reports the state reached', () => {
    const mode = code('ControlEditor', 'McpEditorViewMode.cpp');
    expect(mode).toContain('ViewportClient.RemoveRealtimeOverride(McpOverride, false);');
    expect(mode).toContain('if (!bBackgroundHold) ViewportClient.AddRealtimeOverride(bRealtime, McpOverride);');
    expect(mode).toContain('Resp->SetBoolField(TEXT("realtime"), bReached);');
  });

  // The editor re-adds its background override on every tick, so an override of ours was buried on the next frame.
  it('says the background throttle holds the viewport instead of claiming realtime', () => {
    expect(code('ControlEditor', 'McpEditorViewMode.cpp'))
      .toContain('const bool bBackgroundHold = bRealtime && !FApp::HasFocus() && Performance && Performance->bThrottleCPUWhenNotForeground;');
  });
});

describe('audit_placement finds unfinished content', () => {
  // A level full of grid-checker meshes, "Text" labels and actors pasted twice read as slop, and only a camera fly-by found it.
  it('reports default materials, placeholder text and pasted duplicates beside placement problems', () => {
    const audit = code('ControlActor', 'McpActorPlacementAudit.cpp');
    expect(audit).toContain('McpContentAudit::MergeContent(World, NameFilter, Findings, FindingIndex);');
    expect(audit).toContain('Object->SetArrayField(TEXT("contentIssues"), Finding.ContentIssues);');
    const content = code('ControlActor', 'Placement', 'McpContentAudit.cpp');
    expect(content).toContain('Material->GetMaterial() == Default');
    expect(content).toContain('Text->Text.ToString() == TEXT("Text")');
  });
});

describe('every call that changes actors is checked on the spot', () => {
  const core = (...segments: readonly string[]): string =>
    readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', ...segments), 'utf8');

  // An actor duplicated, re-meshed or spawned by any capability but spawn and set_transform answered a bare success, so a
  // caller that never ran audit_placement left it sunk, floating, z-fighting or on the default grid material.
  it('watches what a request spawns or modifies and checks it in the reply', () => {
    const dispatch = core('Core', 'Requests', 'McpProcessRequest.cpp');
    expect(dispatch).toContain('McpChangeWatch::Begin(RequestId);');
    expect(dispatch).toContain('McpChangeWatch::End();');
    expect(core('Core', 'Subsystem', 'EngineWireSubsystemResponses.cpp')).toContain('McpChangeWatch::AddWarnings(RequestId, EffectiveResult);');
    const watch = code('ControlActor', 'Placement', 'McpChangeWatch.cpp');
    expect(watch).toContain('FCoreUObjectDelegates::OnObjectModified.AddStatic(&WatchOnModified)');
    expect(watch).toContain('GEngine->OnLevelActorAdded().AddStatic(&WatchOnAdded)');
    expect(watch).toContain('McpPlacement::DescribePlacement(Actor, Rows[Actor]);');
    expect(watch).toContain('McpContentAudit::FindUnfinished(Actor, Issues);');
    expect(watch).toContain('McpContentAudit::FindTwin(Actor)');
  });

  // A Blueprint compile respawns every placed instance and a construction script rebuilds parts: neither moved anything.
  it('skips respawns and compares assets, not component objects', () => {
    const watch = code('ControlActor', 'Placement', 'McpChangeWatch.cpp');
    expect(watch).toContain('GIsReinstancing');
    expect(watch).toContain('Actor->IsChildActor()');
    expect(code('ControlActor', 'Placement', 'McpContentAudit.cpp')).toContain('OutDrawn += Drawn;');
  });

  // A part moved in a Blueprint moved in every placed copy, and the compile replaced each copy with a new object.
  it('fingerprints the placed copies of an edited Blueprint by path and judges what the edit changed in them', () => {
    const watch = code('ControlActor', 'Placement', 'McpChangeWatch.cpp');
    expect(watch).toContain('WatchBlueprintCopies(Object);');
    expect(watch).toContain('for (TActorIterator<AActor> It(World, Class); It; ++It)');
    expect(watch).toContain('AActor* Copy = FindObject<AActor>(nullptr, *Pair.Key);');
  });

  it('leaves an actor whose reply carries its placement to that reply', () => {
    expect(code('ControlActor', 'McpActorPlacementCheck.cpp')).toContain('McpChangeWatch::NotePlacementReported(Actor);');
  });
});

describe('every find stops at its limit', () => {
  // A class or tag most of a level shares answered every actor (206 rows for one tag), while mesh and material finds
  // stopped at 200 and said so.
  it('class, name and tag finds cut their rows at limit and say how many matched', () => {
    expect(code('ControlActor', 'McpActorSupport.h')).toContain('inline int32 McpCapFindRows(');
    expect(code('ControlActor', 'McpActorQueryFindByClass.cpp')).toContain('McpCapFindRows(Payload, ActorsArray, Data)');
    expect(code('ControlActor', 'McpActorQuery.cpp')).toContain('McpCapFindRows(Payload, Matches, Data);');
    expect(code('ControlActor', 'McpActorTags.cpp')).toContain('McpCapFindRows(Payload, Matches, Data)');
  });
});

describe('a player start is judged where a pawn spawns', () => {
  // Its capsule collides with nothing, so a start buried in the floor or a wall was skipped like a volume.
  it('measures a player start by its capsule instead of skipping it', () => {
    const check = code('ControlActor', 'McpActorPlacementCheck.cpp');
    expect(check).toContain('const ANavigationObjectBase *Start = Cast<ANavigationObjectBase>(Actor);');
    expect(check).toContain('if ((!StartCapsule && IsBoundsOnlyActor(Actor))');
    expect(check).toContain('Extent = StartCapsule->Bounds.BoxExtent;');
  });
});

describe('animation and character creators say what they did', () => {
  // A Blueprint that is not a Character was compiled, saved unchanged and answered "Movement configured";
  // every character edit loads through one loader, which now refuses it.
  it('character edits refuse a Blueprint with no character movement and report the values they set', () => {
    const loader = code('Character', 'McpCharacterBPSupport.cpp');
    expect(loader).toContain('if (!CharacterDefaults || !CharacterDefaults->GetCharacterMovement())');
    expect(loader).toContain('TEXT("NOT_A_CHARACTER")');
    const movement = code('Character', 'McpCharacterMovementCore.cpp');
    expect(movement).toContain('Result->SetNumberField(TEXT("maxWalkSpeed"), Movement->MaxWalkSpeed);');
    expect(movement).toContain('Result->SetNumberField(TEXT("navAgentRadius"), Applied->NavAgentProps.AgentRadius);');
    expect(movement).not.toContain('configure_sprint');
  });

  // A nav agent size on the defaults was replaced by the capsule size when the character spawned, and an Animation
  // Blueprint for another skeleton left the character in its rest pose; both answered "configured".
  it('nav agent sizes stick and an Animation Blueprint must fit the mesh skeleton', () => {
    const movement = code('Character', 'McpCharacterMovementCore.cpp');
    expect(movement).toContain('Movement->SetUpdateNavAgentWithOwnersCollisions(false);');
    expect(movement).toContain('Result->SetBoolField(TEXT("navAgentFollowsCapsule"), Applied->ShouldUpdateNavAgentWithOwnersCollision());');
    const components = code('Character', 'McpCharacterComponents.cpp');
    expect(components).toContain('MeshSkeleton->IsCompatibleForEditor(AnimSkeleton)');
    expect(components).toContain('TEXT("SKELETON_MISMATCH")');
  });

  // "Group.Slot" went into the Slot node while montages name the bare slot, so no montage ever played through it;
  // neither node creator named the node it made, so it could not be wired.
  it('slot nodes hold the bare slot name and both anim node creators name their node', () => {
    const layers = code('AnimationAuthoring', 'McpAnimBPSlotLayers.cpp');
    expect(layers).toContain('SlotNode->Node.SlotName = Slot;');
    expect(layers).not.toContain('TEXT("%s.%s"), *GroupName, *SlotName');
    expect(layers).toContain('TEXT("SLOT_IN_OTHER_GROUP")');
    expect(layers).toContain('Response->SetStringField(TEXT("nodeId"), SlotNode->NodeGuid.ToString());');
    expect(layers).toContain('Response->SetStringField(TEXT("nodeId"), BlendNode->NodeGuid.ToString());');
  });

  // The editor-world actor does not tick, so a montage "playing" there stood still.
  it('play_montage outside play reports that nothing moves', () => {
    const montage = code('Animation', 'Montages', 'McpAnimPlayAnimMontage.cpp');
    expect(montage).toContain('Resp->SetBoolField(TEXT("playing"), PieWorld != nullptr);');
  });

  // A failed save answered like a saved asset, and the new package was not even dirty for save_all.
  it('skeleton, AI controller and MetaHuman creators report saved and leave an unsaved asset dirty', () => {
    expect(code('Skeleton', 'Bones', 'McpSkeletonStructureCreation.cpp')).toContain('Result->SetBoolField(TEXT("saved"), bSave && bSaveOk);');
    const controller = code('AI', 'Controllers', 'McpAIControllerSetup.cpp');
    expect(controller).toContain('Blueprint->MarkPackageDirty();');
    expect(controller).toContain('Result->SetBoolField(TEXT("saved"), bSaved);');
    const metahuman = code('MetaHuman', 'McpMetaHumanCreate.cpp');
    expect(metahuman).toContain('Created->MarkPackageDirty();');
    expect(metahuman).toContain('Result->SetBoolField(TEXT("saved"), bSaved);');
  });
});

describe('native search renders a signature with fractions', () => {
  // A confident first hit carries its example; playRate 1.25 failed canonical rendering, and with it the whole search.
  it('renders search rows with fractions allowed', () => {
    const search = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'MCP', 'Gateway', 'McpNativeGatewaySearch.cpp'), 'utf8');
    expect(search).toContain('McpCanonicalJsonObject(View, Rendered, /*bAllowFractions=*/true)');
  });
});

describe('edits that skipped undo, and replies that said too little', () => {
  const priv = (...segments: readonly string[]): string =>
    readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', ...segments), 'utf8');

  // Modify alone recorded nothing: detach, light channels and spline points could not be undone, and a light channel
  // set without Modify was not even unsaved, so a save left it out.
  it('detach, light channels and spline point edits run in a transaction', () => {
    expect(code('ControlActor', 'McpActorAttachment.cpp')).toContain('FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Detach Actor"))');
    const lighting = code('Render', 'McpRenderLighting.cpp');
    expect(lighting).toContain('TEXT("Set Light Channel")');
    expect(lighting).toContain('TEXT("Set Actor Light Channel")');
    const spline = code('Spline', 'McpSplinePointEdit.cpp');
    for (const title of ['Add Spline Point', 'Remove Spline Point', 'Move Spline Point']) expect(spline).toContain(`TEXT("${title}")`);
  });

  it('open_level takes an object path, and a relative .t3d export is the project\'s', () => {
    expect(code('ControlEditor', 'McpEditorLevel.cpp')).toContain('LevelPath = FPackageName::ObjectPathToPackageName(LevelPath);');
    expect(code('Level', 'ImportExport', 'McpLevelExport.cpp')).toContain('FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ExportPath)');
  });

  it('local player refusals carry codes, native reset answers enabled and a widget animation delete reports saved', () => {
    const players = code('Sessions', 'McpSessionsLocalPlayers.cpp');
    for (const errorCode of ['NOT_IN_PLAY', 'PRIMARY_PLAYER', 'PLAYER_NOT_FOUND', 'ADD_PLAYER_FAILED']) expect(players).toContain(`TEXT("${errorCode}")`);
    expect(priv('MCP', 'DynamicTools', 'McpDynamicToolManagerQueries.cpp')).toContain('Result->SetNumberField(TEXT("enabled"), Enabled);');
    expect(code('WidgetAuthoring', 'Animation', 'McpWidgetAnimQueries.cpp')).toContain('ResultJson->SetBoolField(TEXT("saved"), bSaved);');
    expect(code('NiagaraGraph', 'McpNiagaraGraphHandlers.cpp')).toContain('.Equals(TEXT("Update"), ESearchCase::IgnoreCase)');
  });
});

describe('review findings on renders, bone keys, play rate and paging', () => {
  const priv = (...segments: readonly string[]): string =>
    readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', ...segments), 'utf8');

  // A track add_bone_track just made holds no key, and the read answered identity: an omitted rotation snapped the bone.
  it('set_bone_key on 5.2+ uses a track value only where a key is', () => {
    expect(code('AnimationAuthoring', 'McpAnimSequenceTracks.cpp')).toContain('if (!Keyed.Equals(FTransform::Identity, 0.0))');
  });

  // A frozen clip became one tick long, and a fixed rate replaced a loop or curve warp.
  it('set_section playRate keeps a frozen clip and refuses a time warp', () => {
    const state = code('Sequence', 'McpSeqTrackState.cpp');
    expect(state).toContain('bFixedRate && OldRate > 0.0');
    expect(state).toContain('if (bRate && !bFixedRate) {');
  });

  it('restart validateOnly names a running render, a blank cursor is refused natively, the cut note is inside the bound', () => {
    expect(code('ControlEditor', 'McpEditorRestart.cpp')).toContain('a Movie Render Queue render is running.');
    expect(priv('MCP', 'Execute', 'McpNativeGatewayExecuteRequest.cpp')).toContain('Cursor->AsString().TrimStartAndEnd().IsEmpty()');
    expect(priv('MCP', 'Execute', 'McpNativeResultPages.cpp')).toContain('!Entry->Issued.Contains(Offset)');
    expect(code('Log', 'McpLogHistory.cpp')).toContain('if (Unlisted > 0 && MaxDistinct > 0)');
  });
});

describe('spawned lights undo, render targets say what was saved, fades change the sound playing', () => {
  it('create_light spawns inside a transaction and reports it', () => {
    const spawn = code('Lighting', 'McpLightingLightSpawn.cpp');
    expect(spawn).toMatch(/FMcpScopedEditorTransaction Transaction\(FText::FromString\(TEXT\("Spawn Light"\)\)[\s\S]*?SpawnActorDeferred/u);
    expect(spawn).toContain('Transaction.DescribeInto(Resp);');
  });

  it('create_render_target answers the save it made and flags an existing target', () => {
    const target = code('Texture', 'McpTextureRenderTarget.cpp');
    expect(target).toContain('const bool bSaved = bSave && McpSafeAssetSave(RenderTarget);');
    expect(target).toContain('Response->SetBoolField(TEXT("saved"), bSaved);');
    expect(target.match(/SetBoolField\(TEXT\("alreadyExists"\), true\)/gu)).toHaveLength(2);
  });

  it('all three fades share one handler: a volume change adjusts, a silent sound or unknown component is refused', () => {
    const fade = code('Audio', 'McpAudioFadeAndReverb.cpp');
    expect(fade).toContain('AudioComp->AdjustVolume((float)FadeTime, (float)TargetVolume);');
    expect(fade).toContain('TEXT("SOUND_NOT_PLAYING")');
    expect(fade).toContain('TEXT("COMPONENT_NOT_FOUND")');
    expect(fade).toMatch(/for \(UWorld\* World : \{GEditor->PlayWorld\.Get\(\), GEditor->GetEditorWorldContext\(\)\.World\(\)\}\)/u);
    expect(fade).toContain('Lower == TEXT("fade_sound_in") || Lower == TEXT("fade_sound_out")');
    expect(code('Audio', 'McpAudioComponentsAndFades.cpp')).not.toContain('fade_sound');
  });
});

describe('a write never deletes, engine content is never rebuilt, a listing can be paged', () => {
  it('ensure_single_sky_light refuses several sky lights instead of destroying them, and refuses during Play', () => {
    const sky = code('Lighting', 'McpLightingSky.cpp');
    expect(sky).not.toContain('DestroyActor');
    expect(sky).toContain('TEXT("MULTIPLE_SKY_LIGHTS")');
    expect(sky).toContain('TEXT("PIE_ACTIVE")');
    expect(sky).toContain('ExtractVectorField(Payload, TEXT("location"), FVector(0.0, 0.0, 500.0))');
    expect(sky.match(/TEXT\("SPAWN_FAILED"\)/gu)).toHaveLength(2);
  });

  it('list_data_table_rows lists rows once, at the root, so the gateway can page them', () => {
    const rows = code('AssetWorkflow', 'DataTables', 'Rows.cpp');
    expect(rows).not.toContain('Details->SetArrayField(TEXT("rows")');
    expect(rows).toContain('constexpr int32 MaxListedRows = 10000;');
    expect(rows).toContain('OutResult->SetBoolField(TEXT("truncated"), true);');
  });

  it('LOD edits refuse engine meshes and say whether they saved', () => {
    const lods = code('Geometry', 'Mesh', 'McpGeometryLodOperations.cpp');
    expect(lods).toContain('TEXT("ENGINE_CONTENT")');
    expect(lods.match(/Result->SetBoolField\(TEXT\("saved"\), bSaved\);/gu)).toHaveLength(2);
    expect(lods).toContain('TrianglePercent <= 0.0 || TrianglePercent > 100.0');
    const assetLods = code('AssetWorkflow', 'Optimization', 'McpAssetLODs.cpp');
    expect(assetLods).toMatch(/StartsWith\(TEXT\("\/Engine\/"\)\)\) \{\s*EnginePaths\.Add\(Path\);\s*continue;/u);
    expect(assetLods).toContain('ErrorCode = TEXT("ENGINE_CONTENT");');
  });
});

describe('thumbnails are stored, found assets are not changes, a new audio actor is named', () => {
  it('create_thumbnail caches the render into the package and dirties it only then', () => {
    const thumbs = code('AssetWorkflow', 'Operations', 'McpAssetThumbnails.cpp');
    expect(thumbs).toMatch(/if \(bCached\) \{\s*ThumbnailTools::CacheThumbnail\(Asset->GetFullName\(\), &ObjectThumbnail, Package\);\s*Package->MarkPackageDirty\(\);/u);
    expect(thumbs.match(/MarkPackageDirty\(\)/gu)).toHaveLength(1);
    expect(thumbs).toContain('Width > 4096 || Height > 4096');
  });

  it('an existing render target and a no-op sky light check claim no change', () => {
    expect(code('Texture', 'McpTextureRenderTarget.cpp').match(/McpHandlerUtils::MarkNoAssetsChanged\(Response\);/gu)).toHaveLength(2);
    expect(code('Lighting', 'McpLightingSky.cpp')).toMatch(/if \(bSpawned \|\| bRelabelled\)\s*\{\s*Transaction\.Emplace/u);
  });

  it('create_audio_component names the actor the component is on', () => {
    expect(code('Audio', 'McpAudioComponentsAndFades.cpp')).toContain('Resp->SetStringField(TEXT("actorName"), McpActorRef(Owner));');
  });
});

describe('material property setters edit an instance by override and leave engine content alone', () => {
  it('blend mode, shading model and two-sided override on an instance; the domain and engine content are refused', () => {
    const setters = code('MaterialAuthoring', 'Properties', 'McpMatSetMatEnum.cpp');
    expect(setters).toContain('Instance->BasePropertyOverrides.bOverride_BlendMode = true;');
    expect(setters).toContain('Instance->BasePropertyOverrides.bOverride_ShadingModel = true;');
    expect(setters).toContain('Instance->BasePropertyOverrides.bOverride_TwoSided = true;');
    expect(setters).toContain('TEXT("set_material_domain"), TEXT("materialDomain"), TEXT("Material domain"), &ApplyMaterialDomain, &ValidMaterialDomains, nullptr}');
    expect(setters).toContain('TEXT("MATERIAL_INSTANCE")');
    expect(setters).toContain('TEXT("ENGINE_CONTENT")');
    expect(setters).toContain('Result->SetBoolField(TEXT("saved"), bSaved);');
    expect(setters.indexOf('if (!Setter->Apply(nullptr, Value))')).toBeLessThan(setters.indexOf('Target->Modify();'));
  });
});

describe('break_pin_links names what it disconnected and touches nothing when it refuses', () => {
  it('validates the node and pin before the transaction, and a pin with no links changes nothing', () => {
    const pins = code('BlueprintGraph', 'McpBPGraphPinMutations.cpp');
    const block = pins.slice(pins.indexOf('TEXT("break_pin_links")'));
    expect(block.indexOf('TEXT("PIN_NOT_FOUND")')).toBeLessThan(block.indexOf('TEXT("Break Blueprint Pin Links")'));
    expect(block).toContain('Result->SetArrayField(TEXT("disconnectedFrom"), McpHandlerUtils::ToJsonStringArray(Linked));');
    expect(block).toMatch(/if \(Linked\.Num\(\) == 0\)\s*\{[\s\S]*?McpHandlerUtils::MarkNoAssetsChanged\(Result\);/u);
  });
});

describe('sequence create, duplicate and rename write only /Game sequences and say what they did', () => {
  it('duplicate and rename go through one guard; create keeps only a level sequence it finds', () => {
    const library = code('Sequence', 'McpSeqAssetLibrary.cpp');
    expect(library.match(/McpResolveSequenceMove\(this, Socket, RequestId, /gu)).toHaveLength(2);
    expect(library).toContain('TEXT("SEQUENCE_PATH_NOT_WRITABLE")');
    expect(library).toContain('TEXT("ASSET_ALREADY_EXISTS")');
    expect(library).toContain('Resp->SetBoolField(TEXT("saved"), bSaved);');
    const create = code('Sequence', 'McpSeqAssetCreation.cpp');
    expect(create).toMatch(/if \(!Cast<ULevelSequence>\(Existing\)\)[\s\S]*?TEXT\("ASSET_ALREADY_EXISTS"\)/u);
    expect(create).toContain('Resp->SetBoolField(TEXT("alreadyExists"), true);');
    expect(create).toContain('Resp->SetBoolField(TEXT("saved"), McpSafeAssetSave(NewSequence));');
  });
});

describe('enum values keep their stored meaning: renames change the shown name, duplicates are refused', () => {
  it('rename uses the display name, metadata goes on the entry, and every lookup takes either name', () => {
    const values = code('AssetWorkflow', 'Enums', 'Values.cpp');
    expect(values).toContain('FEnumEditorUtils::SetEnumeratorDisplayName(Enum, Index, FText::FromString(NewValueName))');
    expect(values).toContain('Enum->SetMetaData(*Key, *Value, Index);');
    expect(values).not.toContain('Value_%s_%s');
    expect(values).toContain('TEXT("VALUE_EXISTS")');
    expect(code('AssetWorkflow', 'Enums', 'Shared.h')).toContain('Enum->GetDisplayNameTextByIndex(Index).ToString() == Requested');
    expect(code('AssetWorkflow', 'Enums', 'Split.cpp')).toContain('McpFindEnumValue(Enum, V->AsString())');
    const lifecycle = code('AssetWorkflow', 'Enums', 'LifecycleEnums.cpp');
    expect(lifecycle.indexOf('if (ValueNames.Contains(ValName))')).toBeLessThan(lifecycle.indexOf('FEnumEditorUtils::CreateUserDefinedEnum('));
    expect(lifecycle).toContain('V->SetStringField(TEXT("displayName"), Enum->GetDisplayNameTextByIndex(i).ToString());');
  });
});

describe('an import replaces what is at its destination only when told to', () => {
  it('every import checks the destination; only an FBX with options deletes the old asset first', () => {
    const header = code('AssetWorkflow', 'Operations', 'McpAssetFbxImportOptions.h');
    expect(header).toMatch(/if \(!bDeleteExisting\) \{\s*continue;\s*\}/u);
    const importer = code('AssetWorkflow', 'Operations', 'McpAssetImportDuplicate.cpp');
    expect(importer).toMatch(/if \(SetupError\.IsEmpty\(\)\) \{\s*McpClearImportTarget\(DestPath, DestName, ResolvedSourcePath,\s*bOverwrite, SetupError, SetupCode, ImportData->Factory != nullptr\);/u);
  });
});

describe('audio spawns undo, a used sound is no change, widget style edits keep what they are not given', () => {
  it('the ambient sound and the reverb zone spawn inside a transaction', () => {
    const ambient = code('Audio', 'McpAudioAmbient.cpp');
    expect(ambient).toMatch(/FMcpScopedEditorTransaction Transaction\(FText::FromString\(TEXT\("Create Ambient Sound"\)\)[\s\S]*?SpawnActor<AAmbientSound>/u);
    expect(ambient).toContain('McpHandlerUtils::MarkNoAssetsChanged(Resp);');
    expect(code('Audio', 'McpAudioFadeAndReverb.cpp')).toMatch(/FMcpScopedEditorTransaction Transaction\(FText::FromString\(TEXT\("Create Reverb Zone"\)\)[\s\S]*?SpawnActor<AAudioVolume>/u);
  });

  it('a Rich Text font starts from its own override, and set_margin reports the save', () => {
    const styling = code('WidgetAuthoring', 'Styling', 'McpWidgetAdvancedStyling.cpp');
    expect(styling).toContain('FindFProperty<FStructProperty>(URichTextBlock::StaticClass(), TEXT("DefaultTextStyleOverride"))');
    expect(styling.match(/SetBoolField\(TEXT\("saved"\), WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave\(WidgetBP\)\)/gu)).toHaveLength(2);
  });
});

describe('material function pins refuse what would break the function', () => {
  it('an unknown input type and a taken pin name are refused, not swapped for Vector3 or duplicated', () => {
    const io = code('MaterialAuthoring', 'Nodes', 'McpMatFunctionInputsOutputs.cpp');
    expect(io).toContain('TEXT("NAME_TAKEN")');
    expect(io).toMatch(/if \(!InputType\.IsEmpty\(\) && !Type\) \{[\s\S]*?TEXT\("INVALID_ARGUMENT"\)/u);
    expect(io).toContain('MCP_GET_FUNCTION_EXPRESSIONS(Func)');
  });
});

describe('creating an Animation Blueprint never deletes what is in the way', () => {
  it('any asset other than an Animation Blueprint at the path is refused, not force-deleted', () => {
    const assets = code('AnimationAuthoring', 'McpAnimBPAssets.cpp');
    expect(assets).not.toContain('UEditorAssetLibrary::DeleteAsset');
    expect(assets).not.toContain('LEGACY_ASSET_DELETE_FAILED');
    expect(assets).toContain('Response->SetBoolField(TEXT("saved"), bSaved);');
  });
});

describe('a landscape layer blend refuses a blend type it does not know', () => {
  it('an unknown blendType is refused before the node joins the material, and saved is the save result', () => {
    const blend = code('MaterialAuthoring', 'Properties', 'McpMatConfigLayerBlend.cpp');
    expect(blend.indexOf('if (!IsKnownBlendType(BlendType))')).toBeLessThan(blend.indexOf('MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(BlendNode);'));
    expect(blend).toContain('Result->SetBoolField(TEXT("saved"), bSaved);');
  });
});

describe('texture edits in place leave engine content alone and say whether they saved', () => {
  it('the in-place loader refuses engine content before the grayscale conversion, and each editor reports saved', () => {
    const shared = code('Texture', 'McpTextureShared.cpp');
    expect(shared.indexOf('*OutErrorCode = TEXT("ENGINE_CONTENT");')).toBeLessThan(shared.indexOf('if (bConvertToBGRA8 && Format == TSF_G8)'));
    for (const file of ['McpTextureColor.cpp', 'McpTextureCurves.cpp', 'McpTextureFilters.cpp']) {
      const source = code('Texture', file);
      expect(source, file).toMatch(/AssetPath, Error,\s*true, &ErrorCode\);/u);
      expect(source, file).toContain('Response->SetBoolField(TEXT("saved"), GetJsonBoolField(Params, TEXT("save"), true) && McpSafeAssetSave(Texture));');
    }
  });
});

describe('RPCs are made where they can be called, and only flags a Blueprint runs are set', () => {
  it('create checks the name and the class before it makes an editable function graph', () => {
    const create = code('Networking', 'McpNetRpcCreation.cpp');
    expect(create.indexOf('CheckFreeMemberNameOrReply(Context, Blueprint, FunctionName')).toBeLessThan(create.indexOf('FBlueprintEditorUtils::CreateNewGraph('));
    expect(create).toContain('TEXT("INVALID_TARGET")');
    expect(create).toContain('AddFunctionGraph<UFunction>(Blueprint, NewGraph, true,');
    expect(create).toContain('McpBlueprintUtils::DescribeInvalidParam(Inputs, TEXT("inputs"))');
  });

  it('flags reach replicated custom events, refuse a non-RPC, report no change, and validation is refused', () => {
    const flags = code('Networking', 'McpNetRpcFlags.cpp');
    expect(flags).toContain('Cast<UK2Node_CustomEvent>(Node)');
    expect(flags).toContain('TEXT("NOT_AN_RPC")');
    expect(flags).toMatch(/if \(!bChanged\)\s*\{\s*McpHandlerUtils::MarkNoAssetsChanged\(ResultJson\);/u);
    expect(flags.indexOf('TEXT("NOT_SUPPORTED")')).toBeLessThan(flags.indexOf('return SetRpcFlag(Context, FUNC_NetValidate'));
  });

  it('a RepNotify function is created when missing, and one with parameters is refused', () => {
    const repNotify = code('Networking', 'McpNetSerialize.cpp');
    expect(repNotify).toContain('(FuncGraph && HasParametersOrResult(FuncGraph))');
    expect(repNotify).toContain('AddFunctionGraph<UClass>(Blueprint, FuncGraph, false, nullptr);');
    expect(repNotify).toContain('ResultJson->SetBoolField(TEXT("functionCreated"), bCreate);');
  });
});

describe('hitboxes, attachments, inventory receipts and the Ability System Component do what they report', () => {
  it('hit detection refuses a shape it does not know before adding anything', () => {
    const damage = code('Combat', 'McpCombatDamageExecution.cpp');
    const detection = damage.slice(damage.indexOf('SubAction == TEXT("configure_hit_detection")'));
    expect(detection.indexOf('HitboxType != TEXT("Capsule") && HitboxType != TEXT("Box") && HitboxType != TEXT("Sphere")'))
      .toBeLessThan(detection.indexOf('GetOrCreateSCSComponent<UCapsuleComponent>'));
  });

  it('attachments refuse no slot, and apply the socket and the allowed types they report', () => {
    const attach = code('Combat', 'McpCombatWeaponEquipment.cpp');
    expect(attach.indexOf('if (Slots.Num() == 0 || !SCS)')).toBeLessThan(attach.indexOf('GetOrCreateSCSComponent<USceneComponent>'));
    expect(attach).toContain('PointNode->AttachToName = FName(*Slot.Socket);');
    expect(attach).toContain('AttachPoint->ComponentTags.AddUnique(FName(*Type));');
  });

  it('every inventory configure reply names the asset it changed', () => {
    for (const file of ['McpInventoryItemPresentation.cpp', 'McpInventoryCategories.cpp', 'McpInventoryCraftingRecipes.cpp',
      'McpInventoryLootTables.cpp', 'McpInventoryLootDrops.cpp', 'McpInventoryInventoryReplication.cpp']) {
      expect(code('Inventory', file), file).toContain('McpHandlerUtils::AddVerification(Result,');
    }
  });

  it('the Ability System Component needs an Actor Blueprint and is never doubled; base values need a value and an attribute', () => {
    const components = code('GAS', 'McpGASComponents.cpp');
    expect(components.indexOf('if (!SCS)')).toBeLessThan(components.indexOf('SCS->CreateNode('));
    expect(components.indexOf('TEXT("alreadyExists"), true')).toBeLessThan(components.indexOf('SCS->CreateNode('));
    const values = code('GAS', 'Authoring', 'McpGASAttributeValues.cpp');
    expect(values).toContain('if (!Payload->HasField(TEXT("baseValue")))');
    expect(values).toContain('TEXT("INVALID_TYPE")');
    expect(code('GAS', 'McpGASAttributes.cpp')).toContain('Blueprint->ParentClass->IsChildOf(UAttributeSet::StaticClass())');
  });
});

describe('no save rewrites engine content', () => {
  it('McpSafeAssetSave refuses every /Engine/ package, not only /Engine/Transient', () => {
    const save = code('..', 'Safety', 'McpSafeOperationsAssetSave.h');
    expect(save).toMatch(/if \(PackageName\.StartsWith\(TEXT\("\/Engine\/"\)\)\)\s*\{[\s\S]*?return false;/u);
    expect(save.indexOf('PackageName.StartsWith(TEXT("/Engine/"))')).toBeLessThan(save.indexOf('Package->SetDirtyFlag(true);'));
  });
});

describe('blend spaces, skeletons and ragdolls answer for what they did', () => {
  it('a refused blend sample is an error, and axis edits need a known axis, a field and a forward range', () => {
    const samples = code('AnimationAuthoring', 'McpAnimBlendSpaceSamples.cpp');
    expect(samples).toContain('const int32 SampleIndex = BlendSpace->AddSample(Animation, SampleValue);');
    expect(samples.indexOf('if (SampleIndex == INDEX_NONE)')).toBeLessThan(samples.indexOf('BlendSpace->PostEditChange();'));
    expect(samples).toContain('(bVertical && BlendSpace->IsA<UBlendSpace1D>()) || !bAnyField');
    expect(samples.indexOf('if (Updated.Min >= Updated.Max || Updated.GridNum < 1)')).toBeLessThan(samples.indexOf('BlendParams[bVertical ? 1 : 0] = Updated;'));
  });

  it('skeleton edits report the save and name their asset; reparenting refuses a descendant and skips its own parent', () => {
    expect(code('Skeleton', 'Assets', 'McpSkeletonPayload.cpp')).toContain('Result->SetBoolField(TEXT("saved"), bSave && McpSafeAssetSave(Asset));');
    for (const file of [['Bones', 'McpSkeletonStructureCreation.cpp'], ['Bones', 'McpSkeletonStructureEditing.cpp'], ['Bones', 'McpSkeletonVirtualBones.cpp'],
      ['Bones', 'McpSkeletonSocketRemoval.cpp'], ['Bones', 'McpSkeletonBoneTransforms.cpp'], ['Physics', 'McpSkeletonPhysicsBodyRemoval.cpp'], ['Physics', 'McpSkeletonCloth.cpp']]) {
      expect(code('Skeleton', ...file), file.join('/')).toMatch(/SaveAndReport\(\w+, Payload, (Outcome\.)?Result\);/u);
    }
    const editing = code('Skeleton', 'Bones', 'McpSkeletonStructureEditing.cpp');
    expect(editing).toContain('RefSkeleton.BoneIsChildOf(NewParentIndex, BoneIndex)');
    expect(editing.indexOf('RefSkeleton.GetParentIndex(BoneIndex) == NewParentIndex')).toBeLessThan(editing.indexOf('McpSkeletonBoneEdits::SetBoneParent('));
    expect(code('Skeleton', 'Bones', 'McpSkeletonVirtualBones.cpp')).toContain('VirtualBoneNameHelpers::AddVirtualBonePrefix(');
  });

  it('a ragdoll needs a physics asset before it is told to simulate', () => {
    const ragdoll = code('Animation', 'Physics', 'McpAnimSetupRagdoll.cpp');
    expect(ragdoll.indexOf('TEXT("NO_PHYSICS_ASSET")')).toBeLessThan(ragdoll.indexOf('SkelMeshComp->SetSimulatePhysics(true);'));
  });
});

describe('a create never rebuilds an asset already at its path', () => {
  it('the shared package helper refuses an occupied path before CreatePackage', () => {
    const helper = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'BridgeHelpers', 'Assets', 'McpAssetCreation.h'), 'utf8');
    const body = helper.slice(helper.indexOf('McpCreatePackageForNewAsset('));
    expect(body.indexOf('McpAssetExists(PackagePath)')).toBeLessThan(body.indexOf('CreatePackage(*PackagePath)'));
    expect(body).toContain('OutCode = TEXT("ASSET_ALREADY_EXISTS");');
  });

  it('creators of a caller-named asset go through it', () => {
    for (const file of [['Animation', 'Rigging', 'McpAnimSkinMesh.cpp'], ['AnimationAuthoring', 'McpAnimAimOffsets.cpp'], ['AnimationAuthoring', 'McpAnimControlRig.cpp'],
      ['AnimationAuthoring', 'McpAnimIKRetargeting.cpp'], ['AssetWorkflow', 'Enums', 'Split.cpp'], ['AssetWorkflow', 'Structs', 'McpAssetStructsImport.cpp'],
      ['AudioAuthoring', 'McpAudioAttenuation.cpp'], ['AudioAuthoring', 'McpAudioSoundClasses.cpp'], ['AudioAuthoring', 'McpAudioSoundCueAssets.cpp'],
      ['AudioAuthoring', 'McpAudioSoundMixes.cpp'], ['AudioAuthoring', 'McpAudioDialogue.cpp'], ['AudioAuthoring', 'McpAudioEffects.cpp'],
      ['Environment', 'Runtime', 'McpEnvFoliageAssets.cpp'], ['LevelStructure', 'McpLevelStructureDataLayerCreation.cpp'], ['LevelStructure', 'McpLevelStructureHlod.cpp'],
      ['MaterialAuthoring', 'Nodes', 'McpMatAddLandscapeLayer.cpp'], ['Skeleton', 'Bones', 'McpSkeletonStructureCreation.cpp'], ['Skeleton', 'Physics', 'McpSkeletonPhysicsAssets.cpp'],
      ['Texture', 'McpTextureExtract.cpp'], ['Audio', 'McpAudioOcclusionAndAttenuation.cpp'], ['Foliage', 'McpFoliageProcedural.cpp'],
      ['AI', 'McpAIContext.h'], ['BehaviorTree', 'McpBTAssets.cpp'], ['Landscape', 'McpLandscapeGrassType.cpp']]) {
      expect(code(...file), file.join('/')).toContain('McpCreatePackageForNewAsset(');
    }
  });

  it('a source effect preset is an asset in its own package, not a transient object', () => {
    const support = code('AudioAuthoring', 'McpAudioAssetSupport.cpp');
    expect(support).toContain('NewObject<UObject>(Outer, PresetClass, PresetName, RF_Public | RF_Standalone)');
    expect(support).not.toContain('NewObject<UObject>(GetTransientPackage(), PresetClass');
    expect(code('AudioAuthoring', 'McpAudioEffects.cpp')).toContain('Response->SetStringField(TEXT("createdAssetPath"), PresetPkg);');
  });
});

describe('AI and navigation writes say what they did', () => {
  it('a Blackboard key is refused on a taken name instead of shadowing the first', () => {
    const keys = code('AI', 'Blackboard', 'McpAIBlackboardKeys.cpp');
    expect(keys.indexOf('Blackboard->GetKeyID(FName(*KeyName)) != FBlackboard::InvalidKey')).toBeLessThan(keys.indexOf('Blackboard->Keys.Add(Entry)'));
    expect(keys).toContain('TEXT("NAME_TAKEN")');
    expect(keys).not.toContain('NewObject<UBlackboardKeyType_Object>(Blackboard)');
    expect(keys).toContain('NotifyBlackboardKeysChanged(Blackboard);');
  });

  it('a Blackboard default refuses what it cannot hold instead of answering valueSet false', () => {
    const values = code('AI', 'Blackboard', 'McpAIBlackboardValues.cpp');
    expect(values).not.toContain('valueSet');
    expect(values.indexOf('WriteBlackboardKeyDefault(')).toBeLessThan(values.indexOf('McpSafeAssetSave(Blackboard)'));
    const defaults = code('AI', 'Blackboard', 'McpAIBlackboardDefaults.cpp');
    expect(defaults).toContain('TEXT("NOT_SUPPORTED")');
    expect(defaults).toContain('ENGINE_MINOR_VERSION >= 4');
  });

  it('a nav link proxy is one kind of link and counts for navigation as soon as it is made', () => {
    const links = code('Navigation', 'McpNavLinks.cpp');
    expect(links).toContain('NavLink->PointLinks.Reset();');
    expect(links).toContain('SmartComp->SetNavigationRelevancy(NavLink->bSmartLinkIsRelevant);');
    expect(links).toContain('FNavigationSystem::UpdateActorData(*NavLink);');
    expect(links).not.toContain('PointLinks.Add(NewLink)');
    expect(code('Navigation', 'McpNavPrivate.h')).toContain('return Direction.Equals(TEXT("BothWays"), ESearchCase::IgnoreCase);');
  });

  it('smart link settings check every area class before changing anything', () => {
    const smart = code('Navigation', 'McpNavSmartLinks.cpp');
    expect(smart.indexOf('TEXT("INVALID_AREA_CLASS")')).toBeLessThan(smart.indexOf('SmartComp->SetEnabledArea('));
    expect(smart).toContain('NavLinkCustomField<float>(SmartComp, TEXT("BroadcastRadius"))');
  });

  it('create_landscape refuses a taken label and a material that does not load before it spawns', () => {
    const create = code('Landscape', 'McpLandscapeCreation.cpp');
    expect(create).toContain('TEXT("NAME_TAKEN")');
    expect(create).toContain('TEXT("ASSET_NOT_FOUND")');
    expect(create).toContain('static_cast<int32>(Value) - 1');
    // A spawned Landscape keeps the engine's old 128 x 128 x 256; the editor builds at 100.
    expect(code('Landscape', 'McpLandscapeCreationTask.cpp')).toContain('Landscape->SetActorRelativeScale3D(FVector(100.0f, 100.0f, 100.0f));');
  });

  it('State Trees get a schema and are compiled after every edit', () => {
    const feature = code('AI', 'StateTree', 'McpAIStateTreeFeature.h');
    expect(feature).toContain('/Script/GameplayStateTreeModule.StateTreeComponentSchema');
    expect(feature).not.toContain('__has_include("Components/StateTreeComponentSchema.h")');
    expect(feature).toContain('UStateTreeEditingSubsystem::CompileStateTree(Tree, Log)');
    // Before 5.5 the editor module compiles: a compiler object here put its destructor (and its call into StructUtils,
    // which the plugin does not link) into the plugin on Mac.
    expect(feature).toContain('UE::StateTree::Delegates::OnRequestCompile.Execute(*Tree)');
    expect(feature).not.toContain('FStateTreeCompiler Compiler(');
    const assets = code('AI', 'StateTree', 'McpAIStateTreeAssets.cpp');
    expect(assets).toContain('CreateAIAssetInPackage<UStateTree>(');
    expect(assets).toContain('TEXT("NAME_TAKEN")');
    expect(assets.match(/McpCompileStateTreeInto\(StateTree, Result\);/gu)).toHaveLength(2);
    expect(code('AI', 'StateTree', 'McpAIStateTreeTransitions.cpp')).toContain('TriggerValue == INDEX_NONE || TriggerValue == 0');
    expect(code('AI', 'StateTree', 'McpAIStateTreeTasks.cpp')).not.toContain('recompile the asset in the StateTree editor');
  });

  it('Smart Object, Mass and EQS writes refuse what they cannot do', () => {
    expect(code('AI', 'SmartObjects', 'McpAISmartObjectsDefs.cpp')).toContain('CreateAIAssetOfClass(DefinitionClass, ');
    const components = code('AI', 'SmartObjects', 'McpAISmartObjectsComponents.cpp');
    expect(components).toContain('TEXT("ALREADY_EXISTS")');
    expect(components).toContain('Nothing to set: send activityTags or enabled.');
    const mass = code('AI', 'Runtime', 'McpAIMass.cpp');
    expect(mass).toContain('CreateAIAssetInPackage<UMassEntityConfigAsset>(');
    expect(mass).toContain('a config cannot be its own');
    const context = code('AI', 'EQS', 'McpAIEQSContextTests.cpp');
    expect(context).toContain('Property->SetObjectPropertyValue_InContainer(Target, ContextClass);');
    expect(context).not.toContain('Context %s configured');
    const scoring = code('AI', 'EQS', 'McpAIEQSScoring.cpp');
    expect(scoring.indexOf('Nothing to set: send scoringEquation')).toBeLessThan(scoring.indexOf('TargetTest->Modify();'));
  });

  it('perception senses keep what a call does not send, and no team is claimed', () => {
    expect(code('AI', 'Perception', 'McpAIPerceptionConfigs.h')).toContain('Cast<TConfig>(*It)');
    for (const file of ['McpAIPerceptionSight.cpp', 'McpAIPerceptionHearingDamage.cpp', 'McpAIPerceptionSetup.cpp']) {
      const source = code('AI', 'Perception', file);
      expect(source, file).toContain('FindOrAddSenseConfig<');
      expect(source, file).not.toMatch(/NewObject<UAISenseConfig_\w+>\(PerceptionComp\)/u);
    }
    expect(code('AI', 'Perception', 'McpAIPerceptionTeam.cpp')).not.toContain('SetGenericTeamId(FGenericTeamId');
  });

  it('set_ai_movement takes 0, refuses a negative value and a call that sets nothing', () => {
    const movement = code('AI', 'Navigation', 'McpAIMovement.cpp');
    expect(movement).toContain('Nothing to set: send maxWalkSpeed');
    expect(movement).toContain('if (Value < 0.0)');
    expect(movement).not.toContain('> 0.0f');
    expect(movement.indexOf('MovementComp->Modify();')).toBeLessThan(movement.indexOf('SetNumber(TEXT("maxWalkSpeed")'));
  });

  it('add_mass_spawner places a spawner that spawns, and a Mass config can gain a trait', () => {
    const spawner = code('AI', 'Runtime', 'McpAIMassSpawner.cpp');
    expect(spawner).toContain('World->SpawnActor(F.Spawner,');
    expect(spawner).toContain('AppendElement(F.Generators, Spawner)');
    expect(spawner).toContain('F.QueryTemplate->SetObjectPropertyValue(');
    expect(code('AI', 'Runtime', 'McpAIMass.cpp')).not.toContain('Spawner configured');
    expect(code('AI', 'Runtime', 'McpAIMass.cpp')).toContain('Config.AddTrait(*NewObject<UMassEntityTraitBase>(ConfigAsset, TraitClass');
  });

  it('a Smart Object slot gets the setup the editor gives a new slot', () => {
    const defs = code('AI', 'SmartObjects', 'McpAISmartObjectsDefs.cpp');
    expect(defs).toContain('FPropertyChangedEvent Added(SlotsProp, EPropertyChangeType::ArrayAdd);');
    expect(defs).toContain('NotifySlotAdded(Definition, SlotsProp, SlotIndex);');
  });

  it('Smart Objects are reached through reflection, never through their headers', () => {
    // Up to 5.2 SmartObjects' headers include MassEntityTypes.h, whose explicitly instantiated bitsets call into
    // MassEntity; a file including them needs MassEntity linked, which a build without Mass does not do.
    for (const file of ['McpAISmartObjectsFeature.h', 'McpAISmartObjectsDefs.cpp', 'McpAISmartObjectsComponents.cpp']) {
      expect(code('AI', 'SmartObjects', file)).not.toMatch(/#\s*include\s*["<](SmartObject|Mass)/);
    }
    expect(code('AI', 'SmartObjects', 'McpAISmartObjectsFeature.h')).toContain('TEXT("/Script/SmartObjectsModule.")');
  });

  it('add_eqs_generator refuses settings its generator does not read, before it adds anything', () => {
    const assets = code('AI', 'EQS', 'McpAIEQSAssets.cpp');
    expect(assets).toContain('GeneratorSettingKeys(GeneratorClass)');
    expect(assets).toContain('Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Query));');
    expect(assets.indexOf('does not apply to %s')).toBeLessThan(assets.indexOf('Query->Modify();'));
  });

  it('sound class and mix edits refuse empty calls and keep one modifier per class', () => {
    const classes = code('AudioAuthoring', 'McpAudioSoundClasses.cpp');
    expect(classes).toContain('Nothing to set: send volume, pitch');
    expect(classes).toContain('or an empty string to clear the parent.');
    expect(code('AudioAuthoring', 'McpAudioSoundMixes.cpp')).toContain('Mix->SoundClassEffects.FindByPredicate(');
    expect(code('AudioAuthoring', 'McpAudioSoundMixEq.cpp')).toContain('Nothing to set: send applyEQ');
  });

  it('perception replies and reads use the names the configure actions take', () => {
    const configs = code('AI', 'Perception', 'McpAIPerceptionConfigs.h');
    expect(configs).toContain('Out->SetObjectField(TEXT("detectionByAffiliation"), Affiliation);');
    expect(configs).not.toContain('LoSHearingRange =');
    for (const file of ['McpAIPerceptionSight.cpp', 'McpAIPerceptionHearingDamage.cpp']) {
      expect(code('AI', 'Perception', file), file).toContain('AddSenseSettings(');
    }
    expect(code('AI', 'Runtime', 'McpAIInfoBP.cpp')).toContain('AddSenseSettings(Config, Sense);');
  });
});

describe('widget renames and reparents keep what depends on them', () => {
  it('a rename carries animation bindings, property bindings and navigation to the new name', () => {
    const refs = code('WidgetAuthoring', 'Support', 'McpWidgetRenameRefs.cpp');
    expect(refs).toContain('AnimBinding.WidgetName = NewName;');
    expect(refs).toContain('Binding.ObjectName = NewName.ToString();');
    expect(refs).toContain('TryToRenameBinding(OldName, NewName)');
    expect(code('WidgetAuthoring', 'Support', 'McpWidgetManipulation.cpp')).toContain('McpRenameWidgetReferences(WidgetBP, OldFName, NewFName)');
  });

  it('a reparent loads a Widget Blueprint path and refuses a cycle', () => {
    const creation = code('WidgetAuthoring', 'Support', 'McpWidgetCreation.cpp');
    expect(creation).not.toContain('FindFirstObject<UClass>(*ParentClass');
    expect(creation).toContain('TEXT("CYCLIC_PARENT")');
  });
});

describe('lighting setup does not destroy work and builds what it names', () => {
  it('a lit level refuses to replace unsaved levels and lights its sky', () => {
    const level = code('Lighting', 'McpLightingLevelBootstrap.cpp');
    expect(level.indexOf('McpRefuseLoadOverUnsavedLevels(')).toBeLessThan(level.indexOf('GEditor->NewMap();'));
    expect(level.indexOf('TEXT("PIE_ACTIVE")')).toBeLessThan(level.indexOf('GEditor->NewMap();'));
    expect(level).toContain('/Script/Engine.SkyAtmosphere');
    expect(level).toContain('bRealTimeCapture = true;');
  });

  it('merge_actors places the merged mesh before it removes the sources, and skips no name', () => {
    const merge = code('Performance', 'McpPerfActorMerge.cpp');
    expect(merge).toContain('TEXT("ACTOR_NOT_FOUND")');
    expect(merge).toContain('TEXT("ASSET_ALREADY_EXISTS")');
    expect(merge.indexOf('SetStaticMesh(MergedMesh)')).toBeLessThan(merge.indexOf('ActorSubsystem->DestroyActor(Actor)'));
    expect(merge).not.toContain('Actor->Destroy();');
  });

  it('component creators refuse a name the actor already has before NewObject collides', () => {
    const media = code('Sequence', 'Media', 'McpSeqMediaComponents.cpp');
    expect(media.indexOf('TEXT("NAME_TAKEN")')).toBeLessThan(media.indexOf('NewObject<UActorComponent>('));
    const pcg = code('PCG', 'McpPCGComponentActions.cpp');
    expect(pcg.indexOf('TEXT("NAME_TAKEN")')).toBeLessThan(pcg.indexOf('CreatePCGComponent(Actor, ComponentName)'));
  });

  it('a Lightmass volume gets a box brush in the editor level', () => {
    const build = code('Lighting', 'McpLightingBuild.cpp');
    expect(build).toContain('VolumeHelpers::SpawnVolumeActor<ALightmassImportanceVolume>(World,');
    expect(build).not.toContain('SetActorScale3D(Size / 200.0f)');
  });
});

describe('physics bodies, heightmaps and geometry edits do what they say', () => {
  it('a body mass is written, and simulatePhysics sets the body physics type', () => {
    const bodies = code('Skeleton', 'Physics', 'McpSkeletonPhysicsBodies.cpp');
    expect(bodies).toContain('SetMassOverride(static_cast<float>(Mass), true)');
    expect(bodies).toContain('BodySetup->PhysicsType = bSimulatePhysics');
    expect(bodies).not.toContain('DefaultInstance.bSimulatePhysics = bSimulatePhysics');
  });

  it('modify_heightmap refuses an unknown operation and a heightData count that fits neither form, and smooths', () => {
    const heightmap = code('Landscape', 'McpLandscapeHeightmap.cpp');
    expect(heightmap).toContain('operation must be set, raise (add), lower (subtract), flatten or smooth.');
    expect(heightmap).toContain('takes 1 for all of');
    expect(heightmap).toContain('Operation == TEXT("smooth")');
  });

  it('a geometry edit of a placed dynamic mesh records one undo step', () => {
    expect(code('Geometry', 'McpGeometryHandlers.cpp')).toContain('const FMcpGeometryUndoScope Undo(Payload);');
    expect(code('Geometry', 'Support', 'McpGeometryUndo.cpp')).toContain('GUndo->StoreUndo(Target, MakeUnique<FMeshReplacementChange>(Before, After))');
  });
});

describe('landscape edits reach the merged landscape, physics and texture settings refuse what they cannot set', () => {
  it('every height and paint edit writes an edit layer, uploads its region and merges', () => {
    const scope = code('Landscape', 'McpLandscapeLookup.cpp');
    expect(scope).toContain('Landscape->SetEditingLayer(LandscapeWriteLayer(Landscape));');
    expect(scope).toContain('return Editing.IsValid() ? Editing : FirstEditLayerGuid(Landscape);');
    expect(scope).toContain('Landscape->ForceUpdateLayersContent();');
    for (const file of [['Landscape', 'McpLandscapeHeightmap.cpp'], ['Landscape', 'McpLandscapeSculpt.cpp'],
      ['Landscape', 'McpLandscapeLayerPaint.cpp'], ['Environment', 'Landscape', 'McpEnvLandscapeHeightmap.cpp']]) {
      const source = code(...file);
      expect(source, file.join('/')).toContain('McpLandscapeHandlers::FMcpLandscapeEditScope Edit(Landscape,');
      expect(source, file.join('/')).toMatch(/FLandscapeEditDataInterface \w+\(LandscapeInfo\);/u);
      expect(source, file.join('/')).toContain('Edit.DescribeInto(');
    }
  });

  // With edit layers the engine applies landscape splines only to the layer reserved for them: without one (or the
  // update request) a spline said raiseTerrain true and left every height as it was.
  it('landscape splines shape the terrain through the edit layer reserved for them, in one undo step', () => {
    const handler = code('Environment', 'Landscape', 'McpEnvLandscapeSplines.cpp');
    expect(handler).toContain('MakeUnique<FMcpScopedEditorTransaction>(');
    expect(handler).toContain('McpApplyLandscapeSplinesToTerrain(Landscape, Resp, bMerge)');
    // The layers merge after the undo step closes, and a replace never records the landscape actor: an undo would
    // restore its record of where the splines were drawn, and the rebuild would clear only that.
    expect(handler).toMatch(/Undo->DescribeInto\(Resp\);\s*Undo\.Reset\(\);\s*if \(bMerge\)\s*\{\s*Landscape->ForceUpdateLayersContent\(\);/u);
    expect(handler).not.toMatch(/Landscape->Modify\(\);\s*SplinesComponent->Modify\(\);/u);
    // Control points are in the splines component's unscaled space, not the landscape's (scaled 100).
    expect(handler).toContain('ControlPoint->Location = SplineToWorld.InverseTransformPosition(WorldPoint);');
    expect(handler).not.toContain('Landscape->GetTransform().InverseTransformPosition(WorldPoint)');
    const terrain = code('Environment', 'Landscape', 'McpEnvLandscapeSplinePayload.cpp');
    expect(terrain).toContain('Landscape->RequestSplineLayerUpdate();');
    expect(terrain).toContain('CreateLayer(FName(TEXT("Splines")), ULandscapeEditLayerSplines::StaticClass());');
    expect(terrain).toContain('Landscape->SetLandscapeSplinesReservedLayer(LayerIndex);');
    expect(terrain).toContain('!Landscape->IsMaxLayersReached()');
    expect(terrain).toContain('Info->ApplySplines(false, &Shaped)');
  });

  it('physics asset edits report the save they made and refuse a shape they do not know', () => {
    const bodies = code('Skeleton', 'Physics', 'McpSkeletonPhysicsBodies.cpp');
    const constraints = code('Skeleton', 'Physics', 'McpSkeletonPhysicsConstraints.cpp');
    const assets = code('Skeleton', 'Physics', 'McpSkeletonPhysicsAssets.cpp');
    for (const source of [bodies, constraints, assets]) {
      expect(source).not.toMatch(/^\s*SaveIfRequested\(PhysicsAsset, Payload\);/mu);
      expect(source).toContain('SaveAndReport(');
    }
    expect(bodies).toContain('is not a body shape; use Capsule (the default), ');
    expect(assets).toContain('Params.GeomType = EFG_TaperedCapsule;');
    expect(assets).toMatch(/if \(!ApplyCreateParams\(Payload, Params, GeomTypeName\)\)[\s\S]*?McpCreatePackageForNewAsset/u);
  });

  it('texture compression and group names are engine enum names, and the streaming flags are required', () => {
    const settings = code('Texture', 'McpTextureSettings.cpp');
    expect(settings).toContain('ParseEngineEnum(CompressionSettingsStr, TEXT("TC_"), Compression)');
    expect(settings).toContain('ParseEngineEnum(TextureGroup, TEXT("TEXTUREGROUP_"), Group)');
    expect(settings).not.toContain('ParseCompressionSetting');
    expect(settings).toContain('if (!Params->HasField(TEXT("virtualTextureStreaming")))');
    expect(settings).toContain('if (!Params->HasField(TEXT("neverStream")))');
    expect(settings).not.toContain('if (bSave) McpSafeAssetSave(Texture);');
  });
});

describe('sounds, landscape receipts, skeleton saves and UV projections answer what happened', () => {
  it('play_sound refuses when the engine started no sound', () => {
    const playback = code('Audio', 'McpAudioPlayback.cpp');
    expect(playback).toMatch(/if \(!Preview\) \{[\s\S]*?TEXT\("NOT_PLAYED"\)\);\s*return true;/u);
  });

  it('sculpt and paint name the landscape for the receipt; an export names the file, not the landscape', () => {
    for (const file of ['McpLandscapeSculpt.cpp', 'McpLandscapeLayerPaint.cpp']) {
      expect(code('Landscape', file), file).toContain('McpHandlerUtils::AddVerification(Resp, Landscape);');
    }
    const heightmapIo = code('Environment', 'Landscape', 'McpEnvLandscapeHeightmap.cpp');
    const exportBody = heightmapIo.slice(heightmapIo.indexOf('McpExportLandscapeHeightmap'), heightmapIo.indexOf('McpImportLandscapeHeightmap'));
    expect(exportBody).toContain('TEXT("modifiedPaths")');
    expect(exportBody).not.toContain('AddVerification(Resp, Landscape)');
  });

  it('skeleton edits report the save they made', () => {
    for (const file of [['Bones', 'McpSkeletonSockets.cpp'], ['MorphTargets', 'McpSkeletonMorphTargetEditing.cpp'],
      ['SkinWeights', 'McpSkeletonWeightProfiles.cpp']]) {
      const source = code('Skeleton', ...file);
      expect(source, file.join('/')).not.toMatch(/^\s*SaveIfRequested\(\w+, Payload\);/mu);
      expect(source, file.join('/')).toContain('SaveAndReport(');
    }
  });

  it('a texture group refusal stays short and carries every valid name', () => {
    const settings = code('Texture', 'McpTextureSettings.cpp');
    expect(settings).toContain('Response->SetArrayField(TEXT("validValues")');
    expect(settings).toContain('EngineEnumNames<::TextureGroup>(TEXT("TEXTUREGROUP_"))');
  });

  it('project_uv sizes its box (tileSize, else the mesh bounds), and transform_uvs refuses a call with nothing to apply', () => {
    const uv = code('Geometry', 'UV', 'McpGeometryUvGen.cpp');
    expect(uv).not.toContain('FTransform::Identity');
    expect(uv).toContain('TryGetNumberField(TEXT("tileSize"), TileSize)');
    expect(uv).toContain('Nothing to apply: send uvOffset, uvScale or rotation.');
  });
});

describe('Blueprint creates report the save and the Blueprint already there, receipts name assets, not class defaults', () => {
  it('a new Blueprint is saved before the reply, which reports the save', () => {
    const assets = code('BlueprintCreation', 'McpBPCreateAssets.cpp');
    const save = assets.indexOf('const bool bSaved = SaveLoadedAssetThrottled(CreatedBlueprint, true);');
    expect(save).toBeGreaterThan(-1);
    expect(save).toBeLessThan(assets.indexOf('Message, ResultPayload, FString());'));
    expect(code('BlueprintCreation', 'McpBPCreateCompletion.cpp')).not.toContain('SetBoolField(TEXT("saved"), true)');
  });

  it('a Blueprint already at the path is flagged, and one of another parent class is refused', () => {
    const assets = code('BlueprintCreation', 'McpBPCreateAssets.cpp');
    expect(assets).toContain('ResultPayload->SetBoolField(TEXT("alreadyExists"), true);');
    expect(assets).toMatch(/Blueprint->ParentClass != Wanted->ParentClass[\s\S]*?TEXT\("ASSET_ALREADY_EXISTS"\)\);/u);
  });

  it('a class default object answers as its asset, and create_skeleton names its skeleton', () => {
    const resolution = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'HandlerUtils',
      'McpHandlerUtilsObjectResolution.cpp'), 'utf8');
    expect(resolution).toContain('!AsActor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)');
    expect(code('Skeleton', 'Bones', 'McpSkeletonStructureCreation.cpp')).toContain('McpHandlerUtils::AddVerification(Result, NewSkeleton);');
  });
});

describe('spline point edits and Blueprint variable sets undo, and refuse what they would guess', () => {
  it('an unknown point type is refused, not turned into Curve, and the reply names the type set', () => {
    const config = code('Spline', 'McpSplineValueAndConfig.cpp');
    expect(config).toContain('return OutType != ESplinePointType::Curve || LowerStr == TEXT("curve");');
    for (const file of ['McpSplineActor.cpp', 'McpSplinePointEdit.cpp', 'McpSplinePointTransform.cpp']) {
      expect(code('Spline', file), file).toContain('RefuseSplinePointType(Self, RequestId, Socket,');
    }
    expect(code('Spline', 'McpSplinePointTransform.cpp')).toContain('SetStringField(TEXT("splineType"), SplinePointTypeToString(PointType))');
  });

  it('a point edit needs its index and value, and is one undo step', () => {
    const transform = code('Spline', 'McpSplinePointTransform.cpp');
    expect(transform).toContain('if (!Payload->HasField(TEXT("pointIndex")) || (ValueField && !Payload->HasField(ValueField)))');
    expect(transform.match(/FMcpScopedEditorTransaction Transaction\(/gu)?.length).toBe(4);
    const edit = code('Spline', 'McpSplinePointEdit.cpp');
    expect(edit).toContain('pointIndex and position are required. Nothing was changed.');
    expect(edit).toContain('position is required. Nothing was changed.');
  });

  it('set_blueprint_variables is one undo step, a batch included', () => {
    const advanced = code('ControlActor', 'McpActorAdvanced.cpp');
    expect(advanced).toContain('FMcpScopedEditorTransaction BatchTransaction(FText::FromString(TEXT("Set Blueprint Variables"))');
    expect(advanced).toMatch(/IsBatchStep\(RequestId\)\s*\?\s*nullptr\s*:\s*MakeUnique<FMcpScopedEditorTransaction>/u);
  });
});

describe('lighting settings refuse an empty call before spawning anything, undo, and say how long they last', () => {
  it('exposure, ambient occlusion and fog check their settings before a volume or fog actor can be spawned', () => {
    const post = code('Lighting', 'McpLightingPostProcess.cpp');
    for (const handler of ['HandleSetExposure', 'HandleSetAmbientOcclusion']) {
      const body = post.slice(post.indexOf(`bool ${handler}(`));
      expect(body.indexOf('RefuseIfNothingSet('), handler).toBeLessThan(body.indexOf('ResolvePostProcessOrReply('));
      expect(body.indexOf('FMcpScopedEditorTransaction Transaction('), handler).toBeLessThan(body.indexOf('ResolvePostProcessOrReply('));
    }
    const fog = code('Lighting', 'McpLightingAtmosphere.cpp');
    expect(fog.indexOf('Nothing to set: pass enabled or viewDistance.')).toBeLessThan(fog.indexOf('SpawnActorInActiveWorld<AActor>(AExponentialHeightFog'));
    expect(fog).toContain('bool bEnabled = FogComp->bEnableVolumetricFog;');
  });

  it('GI refuses RayTraced from 5.4, where method 3 is Plugin, and saves to the project only when asked', () => {
    const gi = code('Lighting', 'McpLightingGlobalIllumination.cpp');
    expect(gi).toMatch(/#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 4[\s\S]*?TEXT\("RayTraced"\), 3[\s\S]*?#else(?:(?!RayTraced"\), 3)[\s\S])*?#endif/u);
    expect(gi).toContain('if (GetJsonBoolField(Payload, TEXT("saveToProject"), false))');
    expect(gi).toContain('Renderer->TryUpdateDefaultConfigFile()');
  });

  it('spline components carry RF_Transactional, so their point edits reach the undo buffer', () => {
    const config = code('Spline', 'McpSplineValueAndConfig.cpp');
    expect(config).toContain('NewObject<USplineComponent>(Actor, TEXT("SplineComponent"), RF_Transactional)');
    expect(config).toContain('Spline->SetFlags(RF_Transactional);');
  });
});

describe('run_benchmark measures what it ran', () => {
  it('samples every frame and reports the times, opens no GPU Visualizer, and refuses an unknown type', () => {
    const profiling = code('Performance', 'McpPerfProfiling.cpp');
    const benchmark = profiling.slice(profiling.indexOf('if (Context.Lower == TEXT("run_benchmark"))'), profiling.indexOf('if (Context.Lower == TEXT("enable_gpu_timing"))'));
    expect(benchmark).not.toContain('profilegpu');
    expect(benchmark).not.toContain('stat startfile');
    expect(benchmark).toContain('Resp->SetNumberField(TEXT("averageFps")');
    expect(benchmark).toContain('is not a benchmark type; use cpu, gpu or all.');
    expect(benchmark).toMatch(/if \(Samples->Elapsed < BenchmarkDuration\)\s*\{\s*return true;/u);
  });
});

describe('add_scs_component adds nothing it cannot mesh', () => {
  it('loads meshPath and materialPath before the node exists and refuses what does not load or show', () => {
    const add = code('SCS', 'McpSCSAddComponent.cpp');
    const refusal = add.indexOf('does not load as a %s; nothing was added.');
    expect(refusal).toBeGreaterThan(-1);
    expect(refusal).toBeLessThan(add.indexOf('SCS->CreateNode('));
    expect(add).toContain('shows no %s; send %s only for a %s. Nothing was added.');
    expect(add).toContain('bCanHold ? TEXT("ASSET_NOT_FOUND") : TEXT("INVALID_ARGUMENT")');
  });
});

describe('material parameter setters refuse a missing value', () => {
  it('scalar, vector and static switch refuse instead of writing 0, white or false', () => {
    const scalar = code('MaterialAuthoring', 'Parameters', 'McpMatSetScalarParamValue.cpp');
    expect(scalar).toMatch(/if \(!Payload->TryGetNumberField\(TEXT\("value"\), Value\)\) \{\s*Bridge->SendAutomationError/u);
    const toggle = code('MaterialAuthoring', 'Parameters', 'McpMatSetStaticSwitchParamValue.cpp');
    expect(toggle).toMatch(/if \(!Payload->TryGetBoolField\(TEXT\("value"\), Value\)\) \{\s*Bridge->SendAutomationError/u);
    const vector = code('MaterialAuthoring', 'Parameters', 'McpMatSetVectorParamValue.cpp');
    expect(vector).not.toContain('else if (Payload->HasField(TEXT("value")))');
    expect(vector).toMatch(/\} else \{\s*Bridge->SendAutomationError\(\s*Socket, RequestId,\s*TEXT\("'value' must be an object/u);
  });
});

describe('static lighting edits undo and a build that cannot run says so', () => {
  it('Lightmass settings and the indirect lighting cache write inside a transaction', () => {
    const lighting = code('Render', 'McpRenderLighting.cpp');
    const lightmass = lighting.slice(lighting.indexOf('"configure_lightmass_settings"'), lighting.indexOf('ReadActorReference(Payload, Reference)'));
    expect(lightmass).toContain('FMcpScopedEditorTransaction Transaction(');
    expect(lightmass).toContain('Transaction.DescribeInto(Result);');
    const cache = lighting.slice(lighting.lastIndexOf('if (SubAction == TEXT("configure_indirect_lighting_cache"))'));
    expect(cache).toContain('FMcpScopedEditorTransaction Transaction(');
    expect(cache).toContain('Transaction.DescribeInto(Result);');
  });

  it('foliage reads and removals cover every foliage actor, and a removal undoes', () => {
    const remove = code('Foliage', 'McpFoliageQueries.cpp');
    expect(remove).toContain('McpFoliageHandlers::AllFoliageActors(World)');
    expect(remove).not.toContain('GetOrCreateFoliageActorForWorldSafe');
    expect(remove).toContain('Transaction.DescribeInto(Resp);');
    expect(remove).toContain('Nothing matched: no foliage instances were removed.');
    const read = code('Foliage', 'McpFoliageGetInsts.cpp');
    expect(read).toContain('McpFoliageHandlers::AllFoliageActors(World)');
    expect(read).not.toContain('GetOrCreateFoliageActorForWorldSafe');
  });

  it('a water body takes its material as WaterMaterial, undoes, and refuses a call that sets nothing', () => {
    const surface = code('Environment', 'Runtime', 'McpEnvActorSurface.cpp');
    expect(surface).toContain('FindFProperty<FObjectProperty>(Component->GetClass(), TEXT("WaterMaterial"))');
    expect(surface).toContain('Component->PostEditChangeProperty(Changed);');
    const water = code('Environment', 'Runtime', 'McpEnvWeatherActors.cpp');
    const configure = water.slice(water.indexOf('bool McpConfigureWaterBodyActor('));
    expect(configure).toContain('Nothing to set: send materialPath, collisionEnabled or settings.');
    expect(configure).toContain('Transaction.DescribeInto(Resp);');
    expect(configure).toMatch(/if \(Applied == 0\)/u);
  });

  it('build lighting refuses a level that forces no precomputed lighting', () => {
    const build = code('Lighting', 'McpLightingBuild.cpp');
    expect(build).toContain('TEXT("PRECOMPUTED_LIGHTING_DISABLED")');
    expect(build).not.toContain('TEXT("skipped")');
    expect(build).not.toContain('TEXT("buildApi")');
  });
});

describe('socket edits and mesh bakes refuse what they cannot do', () => {
  it('configure_socket refuses a call naming nothing to change before it loads anything', () => {
    const sockets = code('Skeleton', 'Bones', 'McpSkeletonSockets.cpp');
    const configure = sockets.slice(sockets.indexOf('HandleConfigureSocket('));
    const refusal = configure.indexOf('Nothing to change: send attachBoneName');
    expect(refusal).toBeGreaterThan(-1);
    expect(refusal).toBeLessThan(configure.indexOf('LoadPayloadSkeletonOrReply'));
  });

  it('a bake refuses a path another kind of asset holds and says when it replaced a static mesh', () => {
    const conversion = code('Geometry', 'Support', 'McpGeometryAssetConversion.cpp');
    expect(conversion).toContain('McpAssetExists(OutAssetPath) && !Cast<UStaticMesh>(McpLoadAsset(OutAssetPath))');
    expect(conversion).toContain('OutCode = TEXT("ASSET_EXISTS");');
    expect(conversion).toContain('Result->SetBoolField(TEXT("replaced"), bReplacing);');
    expect(code('Geometry', 'Mesh', 'McpGeometryLodOperations.cpp')).toContain('Result->SetBoolField(TEXT("replaced"), bReplacing);');
  });
});

describe('montage edits refuse what they cannot do and report their save', () => {
  it('sections, links, slots, notifies and blends refuse a missing, taken, overlapping or empty request', () => {
    const assets = code('AnimationAuthoring', 'McpAnimMontageAssets.cpp');
    expect(assets).toContain('TEXT("SECTION_EXISTS")');
    expect(assets).toContain('TEXT("SEGMENT_OVERLAP")');
    expect(assets).toContain('TEXT("SKELETON_MISMATCH")');
    expect(assets).toContain('Montage->SetCompositeLength(Montage->CalculateSequenceLength());');
    expect(assets).toContain('startTime (the time the section moves to) is required; nothing was moved.');
    const notifyBlend = code('AnimationAuthoring', 'McpAnimMontageNotifyBlend.cpp');
    expect(notifyBlend).toContain('TEXT("SECTION_NOT_FOUND")');
    expect(notifyBlend).toContain('Nothing to set: send blendTime');
    expect(notifyBlend).toContain("(the montage's length) and trackIndex be 0 or more; nothing was added.");
    expect(notifyBlend).not.toContain('SaveAnimAsset(Montage, bSave);');
  });

  it('save false leaves an animation edit as an unsaved change instead of dropping it', () => {
    const support = code('AnimationAuthoring', 'McpAnimSupport.cpp');
    const save = support.slice(support.indexOf('bool SaveAnimAsset('));
    expect(save.indexOf('Asset->MarkPackageDirty();')).toBeLessThan(save.indexOf('if (!bShouldSave)'));
  });

  it('a projectile refuses a mesh that does not load before it makes anything', () => {
    const projectiles = code('Combat', 'McpCombatProjectiles.cpp');
    const create = projectiles.slice(projectiles.indexOf('"create_projectile_blueprint"'));
    expect(create.indexOf('projectileMeshPath is not a static mesh')).toBeLessThan(create.indexOf('CreateActorBlueprint('));
  });
});

describe('level Blueprint wiring goes through the graph schema', () => {
  it('connect_level_blueprint_nodes links through TryCreateConnection and refuses with the schema reason', () => {
    const connect = code('LevelStructure', 'McpLevelStructureBPConnect.cpp');
    expect(connect).not.toContain('SourcePin->MakeLinkTo(TargetPin)');
    expect(connect).toContain('Schema->TryCreateConnection(SourcePin, TargetPin)');
    expect(connect).toContain('Response.Message.ToString()');
  });
});

describe('a sequence camera is placed only when it can be bound', () => {
  it('add_camera refuses an asset that is not a Level Sequence before spawning, and removes a camera it could not bind', () => {
    const bindings = code('Sequence', 'McpSeqBindings.cpp');
    const camera = bindings.slice(bindings.indexOf('HandleSequenceAddCamera('), bindings.indexOf('HandleSequenceAddActor('));
    expect(camera.indexOf('is not a Level Sequence; nothing was added.')).toBeLessThan(camera.indexOf('SpawnActorInActiveWorld'));
    expect(camera).toContain('TEXT("BIND_FAILED")');
    expect(camera).toContain('->DestroyActor(Spawned)');
  });
});

describe('cinematic camera and rig edits undo and read back', () => {
  it('camera settings modify the camera before writing, re-derive the lens and read back what it kept', () => {
    const cameras = code('Sequence', 'Cinematics', 'McpSeqCineCameras.cpp');
    const apply = cameras.slice(cameras.indexOf('int32 ApplyCameraSettings('));
    expect(apply.indexOf('Camera->Modify();')).toBeLessThan(apply.indexOf('ApplyNumberAliases('));
    expect(apply).toContain('Camera->PostEditChange();');
    expect(apply).toContain('Result->SetObjectField(TEXT("values"), Kept);');
    expect(cameras).toContain('FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Configure Camera Settings"))');
  });

  it('a camera rig edit is one transaction opened before the rig is spawned or changed', () => {
    const rigs = code('Sequence', 'Cinematics', 'McpSeqCineCameraRigs.cpp');
    expect(rigs.indexOf('FMcpScopedEditorTransaction Transaction(')).toBeLessThan(rigs.indexOf('SpawnActorInActiveWorld'));
    expect(rigs).toContain('Transaction.DescribeInto(OutResult);');
  });
});

describe('dialogue contexts are whole and audio edits keep save false', () => {
  it('set_dialogue_context requires a speaker and refuses a context already on the wave unless replace', () => {
    const dialogue = code('AudioAuthoring', 'McpAudioDialogue.cpp');
    expect(dialogue).toContain('speakerPath is required');
    expect(dialogue).toContain('Mapping.Context == NewMapping.Context');
    expect(dialogue).toContain('TEXT("CONTEXT_EXISTS")');
    expect(dialogue).toContain('Response->SetBoolField(TEXT("saved"), bSaved);');
  });

  it('save false leaves an audio edit as an unsaved change', () => {
    const support = code('AudioAuthoring', 'McpAudioAssetSupport.cpp');
    const save = support.slice(support.indexOf('bool SaveAudioAsset('));
    expect(save.indexOf('Asset->MarkPackageDirty();')).toBeLessThan(save.indexOf('if (!bShouldSave)'));
  });
});

describe('PCG node edits change what they say', () => {
  // A node found by nodeName was re-titled with that name, and an edit that changed nothing answered "updated".
  it('set_pcg_node_settings finds its node by nodeName and refuses an edit that changes nothing', () => {
    const nodes = code('PCG', 'McpPCGNodeActions.cpp');
    expect(nodes).toContain('if (bEdit && AppliedSettings + AppliedConvenienceSettings == 0 && !bMoves && !Payload->HasField(TEXT("title")))');
    expect(nodes).toContain('ApplyNodeMetadata(Node, Payload, !bEdit);');
    expect(code('PCG', 'McpPCGGraph.cpp')).toContain(': GetJsonStringField(Payload, TEXT("title"));');
  });
});

describe('audio actions act on the sound being heard', () => {
  // During Play the editor world is muted, so mixes and plays aimed at it answered success with nothing heard.
  it('mix and play actions use the running game while one plays', () => {
    expect(code('Audio', 'McpAudioPrivate.h')).toContain('inline UWorld* McpAudibleWorld()');
    const mixes = code('Audio', 'McpAudioMixes.cpp');
    expect(mixes).toContain('UWorld *World = McpAudibleWorld();');
    expect(mixes).toContain('Resp->SetBoolField(TEXT("inGame"), World->IsPlayInEditor());');
    expect(mixes).not.toContain('GetEditorWorldContext');
    expect(code('Audio', 'McpAudioPlayback.cpp')).not.toContain('GetEditorWorldContext');
  });
});

describe('landscape LOD edits reach the drawn landscape', () => {
  // The writes fired no edit event and recorded no undo: World Partition proxies kept the old LOD until a reload.
  it('configure_landscape_lod is one undo step and fires the engine edit event for each property it sets', () => {
    const lod = code('Environment', 'McpEnvBuildLandscapeFoliage.cpp');
    expect(lod).toContain('FText::FromString(TEXT("Configure Landscape LOD"))');
    expect(lod).toContain('Landscape->PostEditChangeProperty(Event);');
    expect(lod).toContain('Context.Resp->SetObjectField(TEXT("values"), Values);');
  });
});

describe('source control passes caller text to git safely and says what it skipped', () => {
  // A quote in the commit message or identity ended the quoted argument, so the rest became git options.
  it('the commit message goes through a file and the identity refuses quotes', () => {
    const repo = code('AssetWorkflow', 'SourceControl', 'McpAssetSourceControlRepo.cpp');
    expect(repo).toContain('TEXT("commit -F \\"%s\\""), *MessageFile');
    expect(repo).not.toContain('commit -m');
    expect(repo).toContain("if (Char == TEXT('\"') || Char < 32)");
  });

  it('checkout and submit list the paths that named no asset, and enable switches providers', () => {
    const control = code('AssetWorkflow', 'SourceControl', 'McpAssetSourceControl.cpp');
    expect(control.split('SetArrayField(TEXT("skipped"), Skipped)').length - 1).toBe(4);
    expect(control).toContain('SourceControlModule.GetProvider().GetName().ToString().Equals(Provider, ESearchCase::IgnoreCase)');
    expect(control).not.toContain('Please configure provider in Editor preferences');
  });

  // The explanation sat only in the error field, which some clients never show.
  it('each refusal message says why on its own', () => {
    const control = code('AssetWorkflow', 'SourceControl', 'McpAssetSourceControl.cpp');
    expect(control).not.toContain('TEXT("Source control disabled")');
    expect(control).not.toContain('TEXT("No valid assets")');
    expect(control.split('*FString::Join(AssetPaths, TEXT(", "))').length - 1).toBe(2);
    expect(control).toContain('Result->GetStringField(TEXT("error")), Result,');
    expect(control).not.toContain('TEXT("Source control is not enabled")');
  });
});

describe('spline templates draw their own shape and refuse what does not load', () => {
  // Every template drew the same flat ribbon (a wall or a pipe came out as a road), nothing at all without a
  // meshPath, and a mesh or material that failed to load still answered success with a bare spline.
  it('each kind carries a stock mesh and cross-section, and everything loads before the actor spawns', () => {
    const templates = code('Spline', 'McpSplineTemplates.cpp');
    expect(templates).toContain('{TEXT("Wall"), StockCube, ESplineMeshAxis::X, 20.0, 300.0, false, true}');
    expect(templates).toContain('{TEXT("Pipe"), StockCylinder, ESplineMeshAxis::Z, 50.0, 50.0, true, false}');
    expect(templates).not.toContain('0.08f)');
    const spawn = templates.indexOf('SpawnSplineActor(');
    expect(templates.indexOf('TEXT("MESH_NOT_FOUND")')).toBeLessThan(spawn);
    expect(templates.indexOf('TEXT("MATERIAL_NOT_FOUND")')).toBeLessThan(spawn);
    expect(templates).toContain('SegmentComp->SetStartOffset(SegmentOffset, false);');
    // A spline mesh flags its material Used with Spline Meshes, which dirties an /Engine material nobody can save.
    expect(templates).not.toContain('BasicShapeMaterial');
  });

  // A bare AActor spawns with no root, so the spawn location and rotation went nowhere.
  it('a spline actor lands where it was asked', () => {
    const spawn = code('Spline', 'McpSplineValueAndConfig.cpp');
    expect(spawn.indexOf('Actor->SetActorLocationAndRotation(Location, Rotation);')).toBeGreaterThan(spawn.indexOf('Actor->SetRootComponent(Spline);'));
  });
});

describe('render console settings say what took', () => {
  // A console variable held by the console or read-only was listed as applied; a settings key the action does not
  // read was dropped; nothing taking still answered success; ray tracing off for the project went unmentioned.
  it('reads each variable back, refuses unread keys and empty results, and warns when ray tracing is off', () => {
    const helper = code('..', 'Foundation', 'Render', 'McpConsoleVariableWrite.h');
    expect(helper).toContain('Variable.GetFloat(), Wanted');
    const console = code('Render', 'McpRenderConsole.cpp');
    expect(console).toContain('does not read settings %s; it reads %s');
    expect(console).toContain('TEXT("CVAR_UNAVAILABLE")');
    expect(console).toContain('FindConsoleVariable(TEXT("r.RayTracing"))');
    expect(console).not.toContain('Render console settings not available.');
  });

  // A denoiser name no plugin registered left the renderer on whichever denoiser it had, without a word.
  it('configure_path_tracing takes only a denoiser a loaded plugin registers, refused before any variable is set', () => {
    const console = code('Render', 'McpRenderConsole.cpp');
    expect(console).toContain('TArray<FString> RenderLoadedDenoisers()');
    expect(console.indexOf('is not a denoiser this editor')).toBeLessThan(console.indexOf('TArray<FBoundedConsoleSetting> Values;'));
    expect(console).toContain('SetConsoleVariable(TEXT("r.PathTracing.Denoiser.Name"), DenoiserName, Applied, Unsupported);');
  });

  // set_tonemapper_type set r.TonemapperFilm, which no 5.x engine has, and answered applied.
  it('the tonemapper type drives the tone curve and refuses an unknown type', () => {
    const color = code('Render', 'McpRenderPostProcessColor.cpp');
    expect(color).not.toContain('r.TonemapperFilm');
    expect(color).toContain('{TEXT("ToneCurveAmount"), TEXT("ExpandGamut")}');
    expect(color).toContain('is not a tonemapper');
  });

  // Every post-process call wrote the live volume piecemeal with no transaction: undo took nothing back, and a refusal
  // halfway (a bad value after good ones) left the volume half changed.
  it('post-process values go on in one undo step once every one is taken', () => {
    const support = code('Render', 'McpRenderSupport.h');
    expect(support).toMatch(/inline void CommitPostProcessVolume[\s\S]*?FMcpScopedEditorTransaction Transaction[\s\S]*?Volume->Settings = Staged;/u);
    for (const file of ['McpRenderPostProcessColor.cpp', 'McpRenderPostProcessLens.cpp', 'McpRenderReflections.cpp']) {
      const handler = code('Render', file);
      expect(handler).toContain('FPostProcessSettings Staged = Volume->Settings;');
      expect(handler).toMatch(/CommitPostProcessVolume\(Volume, Staged, Payload, (true|false|bBlend), Applied\);/u);
      expect(handler).not.toContain('ApplyVolumeBlendFields(Volume, Payload, Applied);');
    }
  });

  // A motion blur amount of 50 or an f-stop of 0.5 was written and answered applied; two exposure and depth of field
  // names wrote values no engine has; bMobileHQGaussian's override (bOverride_MobileHQGaussian) was never turned on.
  it('values are held to the engine limits and to names the engine has', () => {
    const settings = code('Render', 'McpRenderSupportSettings.h');
    expect(settings).toContain('Property->GetMetaData(TEXT("ClampMin"))');
    expect(settings).toContain('Property->GetMetaData(TEXT("ClampMax"))');
    expect(settings).toContain('TEXT("bOverride_") + Pair.Key.Mid(1)');
    const enums = code('Render', 'McpRenderSupportEnums.h');
    expect(enums).not.toContain('AEM_Recovery');
    expect(enums).not.toContain('DOFM_');
    expect(code('Render', 'McpRenderPostProcessLens.cpp')).toContain('TEXT("bMobileHQGaussian")');
  });

  // A misspelt actorName went on to the level's unbound volume, so it edited another volume than the one named.
  it('a volume name that matches none is refused while the level has an unbound volume', () => {
    const resolver = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'Render', 'McpPostProcessVolumeResolution.cpp'), 'utf8');
    expect(resolver).toContain('if (!Reference.IsEmpty() && (Unbound.Num() > 0 || !bAllowSpawn))');
  });

  // A capture offset call without captureOffset, and a capture resolution call without resolution (which resized the
  // render target to 512), answered success.
  it('capture offset and resolution need their value and read it back', () => {
    const reflections = code('Render', 'McpRenderReflections.cpp');
    expect(reflections).toContain('if (!ReadJsonTriple(Payload->TryGetField(TEXT("captureOffset")), Keys, Offset))');
    expect(reflections).toContain('McpHandlerUtils::VectorToJson(Capture->CaptureOffset)');
    const capture = code('Render', 'McpRenderSceneCapture.cpp');
    expect(capture).toContain('ReadBoundedIntField(Payload, TEXT("resolution"), 0, 1, 4096, Resolution, Error)');
    expect(capture).toContain('Result->SetNumberField(TEXT("resolution"), Capture2D->TextureTarget->SizeX);');
  });
});

describe('console variable writes are read back', () => {
  // IConsoleVariable::Set at code priority loses to any console set and to read-only variables with only a log line,
  // and every site listed the variable as applied.
  it('one helper sets at console priority, refuses read-only variables and reads the value back', () => {
    const helper = code('..', 'Foundation', 'Render', 'McpConsoleVariableWrite.h');
    expect(helper).toContain('EConsoleVariableFlags Priority = ECVF_SetByConsole');
    expect(helper).toContain('Variable.TestFlags(ECVF_ReadOnly)');
    expect(code('Render', 'McpRenderSupportSettings.h')).toContain('McpSetConsoleVariable(*Variable, Value)');
    expect(code('ControlEditor', 'McpEditorPreferences.cpp')).toContain('McpSetConsoleVariable(*CVar, Value)');
    const shadows = code('Lighting', 'McpLightingAtmosphere.cpp');
    expect(shadows).toContain('McpSetConsoleVariable(*CVar, Value)');
    expect(shadows).not.toContain('CVar->Set(');
    const gi = code('Lighting', 'McpLightingGlobalIllumination.cpp');
    expect(gi).not.toContain('CVar->Set(');
    expect(gi).not.toContain('GICVar->Set(');
    // A project setting a console value outranks only takes effect after a restart, and says so.
    expect(code('Ui', 'McpUiRenderSettings.cpp')).toContain('McpSetConsoleVariable(*Variable, LexToString(Write.Value), ECVF_SetByProjectSetting).IsEmpty()');
  });
});

describe('view modes resolve through the engine enum', () => {
  // Ten names were known; the rest went to a console command the editor does not run.
  it('any EViewModeIndex resolves by name, and an unknown one is refused with the list', () => {
    const viewMode = code('ControlEditor', 'McpEditorViewMode.cpp');
    expect(viewMode).toContain('StaticEnum<EViewModeIndex>()');
    expect(viewMode).toContain('is not a view mode. Common ones:');
    expect(viewMode).not.toContain('TEXT("viewmode %s")');
  });
});

describe('effect lifecycle refusals name what was not found', () => {
  // "Niagara system not found." did not say whether the actor, its component or a component playing assetPath missed.
  it('each miss names the actor, its component or the asset', () => {
    const lifecycle = code('Effect', 'McpEffectNiagaraLifecycle.cpp');
    expect(lifecycle).not.toContain('TEXT("Niagara system not found.")');
    expect(lifecycle.split('LifecycleMiss(Actor, SystemName, AssetPath)').length - 1).toBe(3);
  });
});

describe('Gameplay Effect, data interface and framework creates say what they did', () => {
  // An unknown modifier operation became Additive; a duration alone made an Instant effect; an unknown policy made an
  // Instant effect that answered with the policy asked for.
  it('refuses unknown ops and policies and reads a lone duration as HasDuration', () => {
    const modifiers = code('GAS', 'Authoring', 'McpGASEffectModifiers.cpp');
    expect(modifiers).toContain('is not a modifier "');
    expect(modifiers).toContain('ModOp >= EGameplayModOp::Max');
    const magnitude = code('GAS', 'McpGASEffectsMagnitude.cpp');
    expect(magnitude.split('Payload->HasField(TEXT("duration")) ? TEXT("HasDuration") : TEXT("Instant")').length - 1).toBe(2);
    expect(magnitude).toContain('Nothing was created.');
  });

  // A second data interface under a taken name was built by NewObject over the first.
  it('a data interface name the system has is refused, and the object name is unique', () => {
    const interfaces = code('NiagaraAuthoring', 'McpNiagaraDataInterfaces.cpp');
    expect(interfaces).toContain('TEXT("NAME_TAKEN")');
    expect(code('NiagaraAuthoring', 'McpNiagaraUserParams.cpp')).toContain('MakeUniqueObjectName(System, DataInterfaceClass, FName(*ParamName))');
  });

  it('a framework class create reports whether it saved', () => {
    expect(code('GameFramework', 'McpGameFrameworkCreation.cpp')).toContain('Response->SetBoolField(TEXT("saved"), bSaved);');
  });
});

describe('sublevels and minimap volumes land where they say', () => {
  // parentLevel was checked to exist and then ignored, so the sublevel streamed into whatever level was open.
  it('create_sublevel refuses a parent that is not the open level and never builds a /Game//Temp path', () => {
    const sublevels = code('LevelStructure', 'McpLevelStructureSublevels.cpp');
    expect(sublevels).toContain('TEXT("PARENT_LEVEL_NOT_LOADED")');
    expect(sublevels).toContain('OpenLevel.StartsWith(TEXT("/Game/")) ? FPaths::GetPath(OpenLevel) : FString(TEXT("/Game/Maps"))');
    expect(sublevels).not.toContain('TEXT("Parent level not found: %s")');
  });

  // A volume from SpawnActor has no brush: scaling it left bounds of zero, so the minimap captured nothing.
  it('the minimap volume gets a real box brush', () => {
    const hlod = code('LevelStructure', 'McpLevelStructureHlod.cpp');
    expect(hlod).toContain('VolumeHelpers::CreateBoxBrushForVolume(MiniMapVolume, VolumeExtent);');
    expect(hlod).not.toContain('SetActorScale3D(DesiredScale)');
  });

  // Same empty brush for the streaming volume, which also stayed unbound from its level until the level was reloaded.
  it('the streaming volume gets a box brush and is bound to its level at once', () => {
    const distance = code('LevelStructure', 'McpLevelStructureStreamingDistance.cpp');
    expect(distance).toContain('VolumeHelpers::CreateBoxBrushForVolume(NewVolume, FVector(StreamingDistance));');
    expect(distance).toContain('FoundLevel->PostEditChangeProperty(VolumesChanged);');
    expect(distance).not.toContain('SetActorScale3D(DesiredScale)');
    expect(distance).toContain('is not one: use Loading, LoadingAndVisibility');
  });

  // Anything but an exact "Runtime" made an Editor data layer; an unknown HLOD layer type answered success unapplied.
  it('data layer and HLOD layer types are refused when unknown, before anything is created', () => {
    const dataLayer = code('LevelStructure', 'McpLevelStructureDataLayerCreation.cpp');
    expect(dataLayer).toContain('DataLayerType.Equals(TEXT("Runtime"), ESearchCase::IgnoreCase)');
    expect(dataLayer).not.toContain('if (DataLayerType == TEXT("Runtime"))');
    const hlod = code('LevelStructure', 'McpLevelStructureHlod.cpp');
    expect(hlod.indexOf('StaticEnum<EHLODLayerType>()')).toBeLessThan(hlod.indexOf('McpCreatePackageForNewAsset('));
  });

  // Setting one grid value reset the others to their defaults; a hash set row with no layer answered success.
  it('grid edits keep the fields left out, and a partition row with no layer is refused', () => {
    const grid = code('LevelStructure', 'McpLevelStructureWorldPartition.cpp');
    expect(grid).toContain('if (Payload->HasField(TEXT("loadingRange"))) { Grid->LoadingRange = LoadingRange; }');
    expect(grid.indexOf('SpatialHash->Modify();')).toBeLessThan(grid.indexOf('Grid->CellSize = GridCellSize;'));
    expect(code('LevelStructure', 'McpLevelStructureRuntimeHashSetGrid.cpp')).toContain('TEXT("PARTITION_HAS_NO_LAYER")');
  });
});

describe('gameplay debugger categories must be registered', () => {
  // A mistyped name became a new config entry nothing reads, and the call answered "enabled".
  it('an unregistered category is refused with the registered ones', () => {
    const debug = code('Debug', 'McpDebugHandlers.cpp');
    expect(debug).toContain('TEXT("CATEGORY_NOT_FOUND")');
    expect(debug).not.toContain('Config->Categories.AddDefaulted_GetRef()');
  });
});

describe('open_level_blueprint says when its editor did not open', () => {
  // OpenEditorForAsset returning false still answered "Opened Level Blueprint".
  it('an editor that did not open is EDITOR_NOT_OPENED', () => {
    expect(code('LevelStructure', 'McpLevelStructureBPOpen.cpp')).toContain('TEXT("EDITOR_NOT_OPENED")');
  });
});

describe('procedural foliage places instances', () => {
  // The volume had no brush, the simulation ran with a callback that added nothing, and density set the painting
  // value the simulation never reads, yet the call answered "Procedural foliage created".
  it('builds the volume brush, resimulates through the editor library and reads back what spawned', () => {
    const procedural = code('Foliage', 'McpFoliageProcedural.cpp');
    expect(procedural).toContain('VolumeHelpers::CreateBoxBrushForVolume(Volume, Size * 0.5);');
    expect(procedural).toContain('TEXT("/Script/FoliageEdit.ProceduralFoliageEditorLibrary")');
    expect(procedural).toContain('McpResimulateInEditor(Volume) && ProcComp->HasSpawnedAnyInstances()');
    expect(procedural).not.toContain('[](const TArray<FDesiredFoliageInstance> &) {}');
    expect(procedural).toContain('FT->InitialSeedDensity =');
    expect(procedural.indexOf('TEXT("MESH_NOT_FOUND")')).toBeLessThan(procedural.indexOf('McpCreatePackageForNewAsset('));
  });
});

describe('performance settings take, or say why not', () => {
  // Set at code priority with no read-back, a value typed in the console, a read-only variable and one this engine
  // version lacks all answered success.
  it('every console write goes through the read-back helper', () => {
    const perfPrivate = code('Performance', 'McpPerfPrivate.h');
    expect(perfPrivate).toContain('McpSetConsoleVariable(*CVar, Value)');
    expect(perfPrivate).toContain('TEXT("CVAR_UNAVAILABLE")');
    for (const file of ['McpPerfRenderingSettings.cpp', 'McpPerfAdvancedOptimization.cpp', 'McpPerfProfiling.cpp']) {
      expect(code('Performance', file)).not.toContain('SetCVarIfExists');
    }
  });

  it('vsync reaches the editor windows and the frame cap is set as the console does', () => {
    const rendering = code('Performance', 'McpPerfRenderingSettings.cpp');
    expect(rendering).toContain('SetPerfCVar(TEXT("r.VSyncEditor")');
    expect(rendering).toContain('SetPerfCVar(TEXT("t.MaxFPS")');
    expect(rendering).not.toContain('GEngine->SetMaxFPS(');
    expect(rendering).not.toContain('boostPlayerLocation');
  });

  it('scalability is saved where the editor loads it at startup', () => {
    for (const file of ['McpPerfRenderingSettings.cpp', 'McpPerfAdvancedOptimization.cpp']) {
      const source = code('Performance', file);
      expect(source).toContain('Scalability::SaveState(GEditorSettingsIni);');
      expect(source).not.toContain('Scalability::SaveState(GEditorIni)');
    }
  });

  it('writes no console variable the engine lacks, and refuses an unknown profile or shader mode', () => {
    const advanced = code('Performance', 'McpPerfAdvancedOptimization.cpp');
    for (const missing of ['r.OcclusionSlop', 'r.OcclusionCullMinScreenRadius', 'wp.Runtime.EnableStreaming', 'wp.Runtime.RuntimeCellSize', 'wp.Runtime.RuntimeStreamingRange']) {
      expect(advanced).not.toContain(`TEXT("${missing}")`);
    }
    expect(advanced).toContain('WorldPartition->SetEnableStreaming(bEnabled);');
    expect(advanced).toContain('TEXT("STREAMING_NOT_CHANGED")');
    expect(advanced).toContain('McpLevelStructure::HandleConfigureGridSize(');
    expect(advanced).toContain('"must be performance, balanced or quality, got \'%s\'. Nothing was changed."');
    expect(advanced).toContain('TEXT("EXEC_FAILED")');
  });

  it('a grid edit answers with the grid as it now stands and changes only what was sent', () => {
    const grid = code('LevelStructure', 'McpLevelStructureWorldPartition.cpp');
    expect(grid).toContain('ResponseJson->SetNumberField(TEXT("cellSize"), Edited->CellSize);');
    expect(grid).not.toContain('ResponseJson->SetNumberField(TEXT("cellSize"), GridCellSize);');
    const hashSet = code('LevelStructure', 'McpLevelStructureRuntimeHashSetGrid.cpp');
    expect(hashSet).toContain('bLoadingRangeApplied = bSetRange && SetField(');
    expect(hashSet).toContain('bCellSizeApplied = bSetCellSize && SetField(');
    // The unsaved-level note is its own sentence after a message that lacks a closing stop.
    expect(code('LevelStructure', 'McpLevelStructureEditorWorld.cpp')).toContain('(Message.EndsWith(TEXT(".")) ? Message : Message + TEXT("."))');
  });
});

describe('ability edits are measured on the compiled class and saved, or refused', () => {
  // A policy or effect class written on a class without the property, or lost by the recompile, answered "set".
  it('policies and effect classes are read back before the save', () => {
    const commit = code('GAS', 'McpGASRequestContext.h');
    expect(commit).toContain('TFunctionRef<bool()> IsPersisted = [] { return true; }');
    expect(commit.indexOf('if (!IsPersisted())')).toBeLessThan(commit.lastIndexOf('McpSafeOperations::McpSafeAssetSave(Blueprint)'));
    const policies = code('GAS', 'McpGASAbilityPolicies.cpp');
    expect(policies).toContain('This ability class has no NetExecutionPolicy property. Nothing was changed.');
    expect(policies).toContain('Result->SetStringField(TEXT("instancingPolicy"), GASEnumName(InstPolicy));');
    expect(code('GAS', 'McpGASAbilityBasics.cpp')).toContain('ReadCompiledAbilityValue(Blueprint, ClassProperty, Held) && Held.Get() == EffectClass');
  });

  it('a set that cannot be written is SAVE_FAILED', () => {
    const sets = code('GAS', 'McpGASAbilitySets.cpp');
    expect(sets).toContain('if (!McpSafeAssetSave(SetAsset))');
    expect(sets).not.toContain('const bool bSaved = McpSafeAssetSave(SetAsset);');
  });
});

describe('edits answer with what the engine holds', () => {
  it('movement scalars refuse negatives and read the compiled class back', () => {
    const modes = code('Character', 'McpCharacterMovementModes.cpp');
    expect(modes).toContain('%s cannot be negative. Nothing was changed.');
    expect(modes).toContain('Result->SetNumberField(FString(ResponseField), Movement->*Property);');
    expect(modes).toMatch(/&UCharacterMovementComponent::GravityScale,\s+true\);/u);
  });

  it('projectile edits change only what was sent and report the component values', () => {
    const projectiles = code('Combat', 'McpCombatProjectiles.cpp');
    expect(projectiles).toContain('TEXT("INVALID_BLUEPRINT")');
    expect(projectiles).toContain('Result->SetNumberField(TEXT("projectileSpeed"), MovementComp->InitialSpeed);');
    expect(projectiles).not.toContain('Result->SetNumberField(TEXT("collisionRadius"), GetJsonNumberField(');
    expect(projectiles).toContain('if (Payload->HasField(TEXT("bounceEnabled")))');
  });

  it('game speed, attenuation, morph weights and spline axes report the held value', () => {
    expect(code('ControlEditor', 'McpEditorPlay.cpp')).toContain('Resp->SetNumberField(TEXT("speed"), World->GetWorldSettings()->TimeDilation);');
    expect(code('Audio', 'McpAudioOcclusionAndAttenuation.cpp')).toContain('Resp->SetNumberField(TEXT("falloffDistance"), Atten->Attenuation.FalloffDistance);');
    const morph = code('Skeleton', 'MorphTargets', 'McpSkeletonMorphTargetQueries.cpp');
    expect(morph).not.toContain('addMissing');
    expect(morph).toContain('SkelMeshComp->GetMorphTargetCurves().Find(FName(*MorphTargetName))');
    expect(code('Spline', 'McpSplineMeshAsset.cpp')).toContain('SplineMeshAxisName(TargetComp->GetForwardAxis())');
    expect(code('Geometry', 'Support', 'McpGeometryArrays.cpp')).toContain('Result->SetNumberField(TEXT("triangleCount"), Target.Mesh->GetTriangleCount());');
  });
});

describe('names the engine does not know are refused before anything changes', () => {
  it('switch types, spline axes, attenuation shapes, deform axes and animation asset types', () => {
    expect(code('Interaction', 'McpInteractionSwitch.cpp')).toContain('switchType must be button, lever or "');
    const axis = code('Spline', 'McpSplineValueAndConfig.cpp');
    expect(axis).toContain('bool ParseSplineMeshAxis(const FString& ForwardAxis, ESplineMeshAxis::Type& OutAxis)');
    const bp = code('Spline', 'McpSplineMeshBP.cpp');
    expect(bp.indexOf('RefuseSplineMeshAxis(')).toBeLessThan(bp.indexOf('SCS->CreateNode('));
    const atten = code('Audio', 'McpAudioOcclusionAndAttenuation.cpp');
    expect(atten.indexOf('attenuationShape must be Sphere, Capsule, Box or Cone')).toBeLessThan(atten.lastIndexOf('McpCreatePackageForNewAsset('));
    expect(code('Geometry', 'McpGeometryHandlers.cpp')).toContain('axis must be X, Y or Z, got \'%s\'. Nothing was changed.');
    const anim = code('Animation', 'Assets', 'McpAnimCreateAnimAsset.cpp');
    expect(anim).toContain('assetType must be sequence or montage');
    expect(anim).toContain('!bExisting && !McpSafeOperations::McpSafeAssetSave(Asset)');
  });

  it('an IK Rig path must load before the retargeter is made', () => {
    const ik = code('AnimationAuthoring', 'McpAnimIKRetargeting.cpp');
    expect(ik.indexOf('TEXT("IK_RIG_NOT_FOUND")')).toBeLessThan(ik.indexOf('McpCreatePackageForNewAsset('));
  });
});

describe('calls that would change nothing are refused', () => {
  it('sequence properties, scatter settings, spatialization and smoothing', () => {
    const props = code('Sequence', 'McpSeqProps.cpp');
    expect(props).toContain('Pass frameRate, playbackStart, playbackEnd or lengthInFrames. Nothing was changed.');
    expect(props).not.toContain('TEXT("NOT_IMPLEMENTED")');
    expect(props).not.toContain('Resp->SetNumberField(TEXT("frameRate"), 0.0);');
    const ranges = code('Sequence', 'McpSeqRanges.cpp');
    expect(ranges).toContain('start and end are required, in seconds, with end after start');
    expect(ranges.indexOf('MovieScene->Modify();')).toBeLessThan(ranges.indexOf('MovieScene->SetViewRange(Start, End);'));
    const config = code('Spline', 'McpSplineConfig.cpp');
    expect(config).toContain('Pass spacing, useRandomOffset or randomOffsetRange. Nothing was stored.');
    expect(config).toContain('GetConfiguredSplineNumber(Target, World, TEXT("meshSpacing"), 100.0)');
    const spatial = code('AudioAuthoring', 'McpAudioAttenuation.cpp');
    expect(spatial).toContain('Pass spatialize and/or spatialization. Nothing was changed.');
    expect(spatial).toContain('if (bHasSpatialize)');
    expect(code('Geometry', 'Deformers', 'McpGeometryDeformersSmoothing.cpp')).toContain('if (Iterations < 1 || Step <= 0.0)');
  });
});

describe('saves and runtime state are reported as they are', () => {
  it('networking Blueprint edits that cannot be written are SAVE_FAILED', () => {
    const net = code('Networking', 'McpNetValueAndActorResolve.cpp');
    expect(net).toContain('if (!McpSafeAssetSave(Blueprint))');
    expect(net).not.toContain('SetBoolField(TEXT("saved"), McpSafeAssetSave(Blueprint))');
  });

  it('a mapping context already on the player is re-added at the new priority', () => {
    const input = code('Input', 'McpInputRuntimeQueries.cpp');
    expect(input.indexOf('InputSubsystem->RemoveMappingContext(Context);')).toBeLessThan(input.indexOf('InputSubsystem->AddMappingContext(Context, Priority);'));
  });
});

describe('sequence timing and track state edits move nothing they should not', () => {
  // A new tick resolution was set over keys stored in ticks, so every key moved in time; 24000/1001 was read as 24000.
  it('a tick resolution change migrates keys and parses a ratio', () => {
    const rates = code('Sequence', 'McpSeqSetDisplayRate.cpp');
    expect(rates).toContain('UE::MovieScene::TimeHelpers::MigrateFrameTimes(Previous, NewResolution, MovieScene);');
    expect(rates).not.toContain('SetTickResolutionDirectly');
    expect(rates).not.toContain('Contains(TEXT("24000"))');
  });

  it('mute, solo and lock are one undo step each', () => {
    const state = code('Sequence', 'McpSeqTrackState.cpp');
    for (const title of ['Mute Sequence Track', 'Solo Sequence Track', 'Lock Sequence Track']) {
      expect(state).toContain(`FText::FromString(TEXT("${title}"))`);
    }
  });

  // stop put the playhead on frame 0, before a playback range that starts later, and answered "reset to start".
  it('stop goes to the start of the playback range and seeks land on whole ticks', () => {
    const playback = code('Sequence', 'McpSeqPlayback.cpp');
    const stop = playback.slice(playback.indexOf('HandleSequenceStop('));
    expect(stop).toContain('McpSeekPaused(Sequencer, FFrameTime(MovieScene->GetPlaybackRange().GetLowerBoundValue()));');
    expect(stop).not.toContain('FFrameTime(0)');
    expect(playback).toContain('AsFrameTime(Seconds).RoundToFrame()');
    expect(playback).toContain('FText::FromString(TEXT("Set Sequence Playback Speed"))');
  });

  // "Transform" with two actors bound muted, or removed, whichever track came first; a name add_track gave a track
  // answered TRACK_NOT_FOUND to mute, solo, lock and remove_track.
  it('a track name several tracks fit is refused, and display names are found', () => {
    const support = code('Sequence', 'McpSeqEditorSupport.h');
    const lookup = support.slice(support.indexOf('inline UMovieSceneTrack *FindTrackByName'), support.indexOf('inline void CollectTracksByName'));
    expect(lookup).toContain('if (Found.Num() > 1) {');
    expect(lookup).not.toMatch(/if \(Matches\(Track\)\) \{\s*return Track;/);
    expect(code('Sequence', 'McpSeqTrackState.cpp')).toContain('FindTrackByName(OutMovieScene, TrackName, true, FString(), &Ambiguous)');
    expect(code('Sequence', 'McpSeqRegistry.cpp')).toContain('TEXT("AMBIGUOUS_TRACK")');
  });

  // open opened any asset at the path in its own editor and answered "Sequence opened" whether or not one opened.
  it('open and add_track answer with what the editor did', () => {
    const creation = code('Sequence', 'McpSeqAssetCreation.cpp');
    const open = creation.slice(creation.indexOf('HandleSequenceOpen('));
    expect(open).toContain('!AssetEditorSS->OpenEditorForAsset(Sequence)');
    expect(open).toContain('McpSequence::LoadOrReply(');
    const addTrack = code('Sequence', 'McpSeqTrackCreation.cpp');
    expect(addTrack).toContain('FText::FromString(TEXT("Add Sequence Track"))');
    expect(addTrack).toContain('Resp->SetStringField(TEXT("trackName"), NewTrack->GetDisplayName().ToString());');
  });
});

describe('animation, audio, data table and input edits answer with what the engine holds', () => {
  // createIfMissing false on a missing curve, and a key the controller refused, answered "Curve key set"; 5.0 wrote none.
  it('a curve key is refused when it was not set, and read back when it was', () => {
    const events = code('AnimationAuthoring', 'McpAnimSequenceEvents.cpp');
    expect(events).toContain('if (!Controller.SetCurveKey(CurveId, FRichCurveKey(FrameTime, Value)))');
    expect(events).toContain('TEXT("CURVE_NOT_FOUND")');
    expect(events).toContain('VerifySmartName(USkeleton::AnimCurveMappingName, SmartCurveName)');
    expect(events).toContain('Curve->FloatCurve.Eval(FrameTime)');
  });

  it('bone tracks, cached poses, blend spaces and aim offsets report what they hold and whether they saved', () => {
    const tracks = code('AnimationAuthoring', 'McpAnimSequenceTracks.cpp');
    expect(tracks).toContain('Response->SetBoolField(TEXT("alreadyExists"), bAlreadyHadTrack);');
    const nodes = code('AnimationAuthoring', 'McpAnimBPBlendNodes.cpp');
    expect(nodes).toContain('TEXT("CACHE_NAME_TAKEN")');
    expect(nodes).toContain('Response->SetStringField(TEXT("cacheName"), CachedPoseNode->CacheName);');
    expect(code('AnimationAuthoring', 'McpAnimAimOffsets.cpp')).toContain('McpSetBlendSpaceAxis(NewAimOffset, 0, TEXT("Yaw"), -180.f, 180.f)');
    const spaces = code('AnimationAuthoring', 'McpAnimBlendSpaceAssets.cpp');
    expect(spaces).toContain('McpBlendSpaceAxesJson(Cast<UBlendSpace>(ExistingAsset), 1)');
    expect(spaces).not.toMatch(/^\s*SaveAnimAsset\(NewBlendSpace, bSave\);/mu);
    expect(spaces).toContain('TEXT("MISSING_SKELETON")');
    const samples = code('AnimationAuthoring', 'McpAnimBlendSpaceSamples.cpp');
    expect(samples).toMatch(/BlendSpace->ValidateSampleData\(\);\s*BlendSpace->PostEditChange\(\);\s*BlendSpace->ResampleData\(\);/u);
  });

  // The modifier rebuilt the skeleton when it went out of scope, after the save had written it; no step was undoable.
  it('skeleton bone edits are one undo step, rebuilt before the save', () => {
    const creation = code('Skeleton', 'Bones', 'McpSkeletonStructureCreation.cpp');
    expect(creation).toMatch(/FMcpScopedEditorTransaction Transaction\(FText::FromString\(TEXT\("Add Bone"\)\)[\s\S]*?Transaction\.DescribeInto\(Result\);\s*\}[\s\S]*?SaveAndReport\(Skeleton, Payload, Result\);/u);
    expect(creation).toContain('ApplyTransformFieldsFromJson(Payload, BoneTransform);');
    const editing = code('Skeleton', 'Bones', 'McpSkeletonStructureEditing.cpp');
    expect(editing).toContain('FText::FromString(TEXT("Remove Bone"))');
    expect(editing).toContain('FText::FromString(TEXT("Set Bone Parent"))');
    expect(editing).toContain('RootCount <= 1');
  });

  it('occlusion, data table rows, PIE pause and legacy bindings say what changed', () => {
    expect(code('AudioAuthoring', 'McpAudioAttenuation.cpp')).toContain('if (Params->HasField(TEXT("enable")) || Params->HasField(TEXT("enableOcclusion")))');
    const rows = code('AssetWorkflow', 'DataTables', 'Rows.cpp');
    for (const title of ['Add Data Table Row', 'Update Data Table Row', 'Delete Data Table Row']) {
      expect(rows).toContain(`FText::FromString(TEXT("${title}"))`);
    }
    const play = code('ControlEditor', 'McpEditorPlay.cpp');
    expect(play).toContain('Resp->SetBoolField(TEXT("alreadyPaused"), bWasPaused);');
    expect(play).toContain('Resp->SetBoolField(TEXT("wasPaused"), bWasPaused);');
    expect(code('Input', 'McpInputLegacyMappings.cpp')).toContain('bRemove || BeforeCount == 0');
  });
});

describe('Niagara authoring builds what it reports', () => {
  // NewObject under a taken name rebuilt a system in place (its emitters gone) or, for another class, stopped the editor.
  it('creating a system or emitter refuses a taken name and compiles the new system', () => {
    const systems = code('NiagaraAuthoring', 'McpNiagaraSystems.cpp');
    expect(systems).not.toContain('CreatePackage(*PackageName)');
    expect(systems.match(/McpCreatePackageForNewAsset\(PackageName, CreateError, CreateCode\)/gu)?.length).toBe(2);
    expect(systems).toContain('NewSystem->RequestCompile(false);');
    expect(code('NiagaraAuthoring', 'McpNiagaraContext.cpp')).toMatch(/void MarkDirtyAndVerify[\s\S]*?System->RequestCompile\(false\);/u);
  });

  // An unknown velocityMode, forceType, eventType or iteration source became the default while the reply echoed it.
  it('selectors refuse names the engine has no module or source for', () => {
    const modules = code('NiagaraAuthoring', 'McpNiagaraParticleModules.cpp');
    expect(modules).toContain('velocityMode must be Linear, Cone or FromPoint');
    expect(modules).toContain('forceType must be Gravity');
    const events = code('NiagaraAuthoring', 'McpNiagaraEvents.cpp');
    expect(events).toContain('eventType must be Location, Collision or Death');
    expect(events).toContain('GeneratedEventName(NewModule)');
    const stages = code('NiagaraAuthoring', 'McpNiagaraSimulation.cpp');
    expect(stages).toContain('ENiagaraIterationSource::DirectSet');
    expect(stages).toContain('FindPropertyByName(TEXT("MergeId"))');
    const renderers = code('NiagaraAuthoring', 'McpNiagaraRenderers.cpp');
    expect(renderers).toContain('meshPath is required');
    expect(renderers).toContain('Context.Result->SetBoolField(TEXT("rendererCreated"), bCreated);');
  });
});
