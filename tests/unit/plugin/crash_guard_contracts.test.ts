// A value the engine answers with a Fatal log or a failed check is refused before the engine sees it. Each of
// these took the editor down, so the wiring is pinned by reading the code; the refusals themselves are
// integration cases.

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Block and line comments removed, so no assertion can be satisfied (or tripped) by prose. */
const strip = (text: string): string => text.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const code = (...segments: readonly string[]): string => strip(readFileSync(join(PRIVATE, ...segments), 'utf8').replace(/\r\n/gu, '\n'));
/** Every source file under Private with its text, read once: several cases below look over the whole tree. */
let tree: Map<string, string> | undefined;
const sources = (): Map<string, string> =>
  (tree ??= new Map(readdirSync(PRIVATE, { recursive: true, encoding: 'utf8' })
    .filter((entry) => /\.(?:cpp|h)$/u.test(entry))
    .map((entry) => [entry, readFileSync(join(PRIVATE, entry), 'utf8')])));
/** The source files under Private that name a word at all: a plain look that spares the comment strip for the rest. */
const filesNaming = (word: string): string[] =>
  [...sources()].filter(([, text]) => text.includes(word)).map(([entry]) => entry);

describe('a value the engine answers with a fatal error is refused first', () => {
  it('a montage notify and a Behavior Tree node are made only from a class of their base that can be instanced', () => {
    const notify = code('Domains', 'AnimationAuthoring', 'McpAnimMontageNotifyBlend.cpp');
    expect(notify).toContain('ResolveNotifyClassByName(NotifyClass, TEXT("AnimNotify_"), UAnimNotify::StaticClass(), Tried)');
    expect(notify).not.toContain('FindFirstObject<UClass>');
    expect(code('Domains', 'AI', 'McpAIContext.h')).toContain('Class->IsChildOf(Base) && !Class->HasAnyClassFlags(CLASS_Abstract)');
    const tasks = code('Domains', 'AI', 'BehaviorTree', 'McpAIBehaviorTreeAssets.cpp');
    expect(tasks).toContain('FindBTNodeClass(TEXT("BTTask_"), TaskType, UBTTaskNode::StaticClass(), true)');
    expect(tasks).not.toContain('NewObject<UObject>(BT, TaskClass)');
    const others = code('Domains', 'AI', 'BehaviorTree', 'McpAIBehaviorTreeDecorators.cpp');
    expect(others).toContain('FindBTNodeClass(TEXT("BTDecorator_"), DecoratorType, UBTDecorator::StaticClass())');
    expect(others).toContain('FindBTNodeClass(TEXT("BTService_"), ServiceType, UBTService::StaticClass())');
    expect(`${tasks}${others}`).not.toContain('/Script/AIModule.');
  });

  it('the Fab library listing resolves a column type without asserting on an unknown one', () => {
    const library = code('Domains', 'AssetWorkflow', 'Fab', 'McpAssetFabLibrary.cpp');
    expect(library).toContain('Path.StartsWith(TEXT("/")) ? TypeOptional(FTopLevelAssetPath(Path)) : nullptr');
    expect(library).not.toMatch(/(?<!\w)Type\(FTopLevelAssetPath/u);
  });

  it('a revision control provider is set only when the editor has registered it', () => {
    for (const file of ['McpAssetSourceControl.cpp', 'McpAssetSourceControlRepo.cpp']) {
      const source = code('Domains', 'AssetWorkflow', 'SourceControl', file);
      const sets = [...source.matchAll(/SetProvider\(/gu)];
      expect(sets, file).toHaveLength(1);
      const asked = source.indexOf('GetProviderNames(Registered)');
      expect(asked, file).toBeGreaterThan(-1);
      expect(source.slice(asked, sets[0]?.index), file).toContain('Registered.Contains(');
    }
  });

  it('a new asset package is made only under a package name the engine takes and no asset holds', () => {
    const helper = code('Foundation', 'BridgeHelpers', 'Assets', 'McpAssetCreation.h');
    const valid = helper.indexOf('FPackageName::IsValidLongPackageName(PackagePath, true, &InvalidWhy)');
    expect(valid).toBeGreaterThan(-1);
    expect(helper).toContain('PackagePath.Len() >= NAME_SIZE');
    expect(valid).toBeLessThan(helper.indexOf('UPackage *Package = McpCreatePackageOrNull(PackagePath);'));
    // Every bare create refuses a loaded package that already holds an asset, whatever its own check missed.
    expect(helper).toContain('return Loaded && Loaded->FindAssetInPackage() ? nullptr : CreatePackage(*PackagePath);');
    for (const file of [
      ['AssetWorkflow', 'Structs', 'McpAssetStructsLifecycle.cpp'],
      ['AssetWorkflow', 'Structs', 'McpAssetStructsAssetOpsRename.cpp'],
      ['AssetWorkflow', 'DataTables', 'McpAssetDataTableLifecycle.cpp'],
      ['AssetWorkflow', 'Enums', 'McpAssetEnumLifecycle.cpp'],
      ['Foliage', 'McpFoliageWorld.cpp'],
      ['Texture', 'McpTextureAssets.cpp'],
    ]) {
      const source = code('Domains', ...file);
      expect(source, file.join('/')).toContain('McpCreatePackageForNewAsset(');
      expect(source.match(/(?<!\w)CreatePackage\(/gu), file.join('/')).toBeNull();
    }
    // The audio effects make their preset package under a name built from a known effect type.
    const effects = code('Domains', 'AudioAuthoring', 'McpAudioEffects.cpp');
    const known = effects.indexOf('if (!SourceEffectPresetClass(EffectType))');
    expect(known).toBeGreaterThan(-1);
    expect(known).toBeLessThan(effects.indexOf('McpCreatePackageOrNull(PresetPkg)'));
  });

  // CreatePackage is a Fatal log for a name holding "//" (a folder ending in a slash joined to a name), so the engine
  // call sits in one function that answers null for such a name, and every creator's own null branch reports it.
  it('no handler hands the engine a package name it stops the editor on', () => {
    // Read as written: the comment strip would cut the line at the "//" the check names.
    expect(readFileSync(join(PRIVATE, 'Foundation', 'BridgeHelpers', 'Assets', 'McpAssetCreation.h'), 'utf8'))
      .toContain('if (PackagePath.Contains(TEXT("//")) || PackagePath.Len() >= NAME_SIZE)');
    const bare = filesNaming('CreatePackage(')
      .filter((file) => !/Tests?\.cpp$/u.test(file) && !file.endsWith('McpAssetCreation.h'))
      .filter((file) => /(?<!\w)CreatePackage\(/u.test(strip(readFileSync(join(PRIVATE, file), 'utf8'))));
    expect(bare).toEqual([]);
  });

  it('a Blueprint component is added only from a component class that can be instanced', () => {
    const helper = code('Foundation', 'BridgeHelpers', 'Blueprints', 'McpScsLookup.h');
    expect(helper).toContain('!Class->IsChildOf(UActorComponent::StaticClass())');
    expect(helper).toContain('Class->HasAnyClassFlags(CLASS_Abstract)');
    const batch = code('Domains', 'Blueprint', 'Components', 'McpBPModifyScsComponentOps.cpp');
    const refused = batch.indexOf('McpScsComponentClassRefusal(ComponentClass, ComponentClassPath)');
    expect(refused).toBeGreaterThan(-1);
    expect(refused).toBeLessThan(batch.indexOf('AddNewSubobject('));
    expect(refused).toBeLessThan(batch.indexOf('LocalSCS->CreateNode('));
    const single = code('Domains', 'SCS', 'McpSCSAddComponent.cpp');
    const singleRefused = single.indexOf('McpScsComponentClassRefusal(CompClass, ComponentClass)');
    expect(singleRefused).toBeGreaterThan(-1);
    expect(singleRefused).toBeLessThan(single.indexOf('SCS->CreateNode('));
  });

  it('a component is added to an actor only under a name nothing on it holds', () => {
    const add = code('Domains', 'ControlActor', 'McpActorComponents.cpp');
    const taken = add.indexOf('FindObject<UObject>(Found, *ComponentName)');
    expect(taken).toBeGreaterThan(-1);
    expect(taken).toBeLessThan(add.indexOf('NewObject<UActorComponent>('));
    expect(add).toContain('MakeUniqueObjectName(Found, ComponentClass, ComponentClass->GetFName())');
    expect(add).not.toContain('FMath::Rand()');
    expect(add).toContain('ComponentClass->HasAnyClassFlags(CLASS_Abstract)');
    const shared = code('Domains', 'Environment', 'Runtime', 'McpEnvActorComponents.cpp');
    const renamed = shared.indexOf('FindObject<UObject>(Actor, *Name.ToString())');
    expect(renamed).toBeGreaterThan(-1);
    expect(renamed).toBeLessThan(shared.indexOf('NewObject<UActorComponent>(Actor, ComponentClass, Name'));
    expect(shared).toContain('MakeUniqueObjectName(Actor, ComponentClass, Name)');
  });

  it('stairs are checked against the mesh budget before the engine builds them', () => {
    const stairs = code('Domains', 'Geometry', 'Primitives', 'McpGeometryPrimitivesSurfaces.cpp');
    expect(stairs).toContain('bFloating ? 16 * Steps - 4 : 2 * Steps * Steps + 10 * Steps');
    for (const append of ['AppendLinearStairs(', 'AppendCurvedStairs(']) {
      const at = stairs.indexOf(append);
      expect(at, append).toBeGreaterThan(-1);
      const guarded = stairs.lastIndexOf('if (!GuardStairBudget(Self, RequestId, Socket, NumSteps, bFloating)) return true;', at);
      expect(guarded, append).toBeGreaterThan(stairs.lastIndexOf('bool Handle', at));
    }
  });

  it('a UV channel is held to 0 to 7 by the record and by the one helper that makes the layer', () => {
    const helper = code('Domains', 'Geometry', 'Support', 'McpGeometrySafetySupport.cpp');
    expect(helper.indexOf('Channel < 0 || Channel > 7')).toBeGreaterThan(-1);
    expect(helper.indexOf('Channel < 0 || Channel > 7')).toBeLessThan(helper.indexOf('SetNumUVLayers('));
    for (const file of ['McpGeometryUvEditing.cpp', 'McpGeometryUvGen.cpp']) {
      const source = code('Domains', 'Geometry', 'UV', file);
      expect(source, file).toContain('EnsureUvLayerOrReply(Self, RequestId, Socket, Mesh, UVChannel)');
      expect(source, file).not.toContain('SetNumUVLayers(');
    }
    const record = readFileSync(join('src', 'tools', 'catalog', 'capabilities', 'records', 'world', 'properties.ts'), 'utf8');
    expect(record).toContain("uvChannel: { type: 'integer', minimum: 0, maximum: 7,");
  });

  it('a new landscape is held to the component count the editor allows before its arrays are sized', () => {
    const create = code('Domains', 'Landscape', 'McpLandscapeCreation.cpp');
    const limit = create.indexOf('const int32 MaxComponents = FMath::Min(256, 8191 / Request.QuadsPerComponent);');
    expect(limit).toBeGreaterThan(create.indexOf('Components(TEXT("y"), TEXT("sizeY"), Request.ComponentsY);'));
    expect(create.slice(limit, limit + 900)).toMatch(/Request\.ComponentsX > MaxComponents \|\| Request\.ComponentsY > MaxComponents[\s\S]*TEXT\("INVALID_ARGUMENT"\)\);\s*return true;/u);
    expect(create).not.toContain('static_cast<int32>(Value)');
  });

  it('landscape paint and sculpt refuse a landscape with no extent before they size a region from the request', () => {
    for (const [file, sized] of [['McpLandscapeLayerPaint.cpp', 'RegionSizeX * RegionSizeY'], ['McpLandscapeSculpt.cpp', 'HeightData.SetNumZeroed(']] as const) {
      const source = code('Domains', 'Landscape', file);
      const refused = source.indexOf('if (!LandscapeInfo->GetLandscapeExtent(LMinX, LMinY, LMaxX, LMaxY)) {');
      expect(refused, file).toBeGreaterThan(-1);
      expect(source.slice(refused, refused + 260), file).toMatch(/TEXT\("INVALID_LANDSCAPE"\)\);\s*return true;/u);
      expect(refused, file).toBeLessThan(source.indexOf(sized));
    }
    const sculpt = code('Domains', 'Landscape', 'McpLandscapeSculpt.cpp');
    expect(sculpt).toContain('FMath::RoundToInt(FMath::Min(BrushRadius / ScaleX, 1.0e6))');
    expect(sculpt).toContain('FMath::RoundToInt(FMath::Clamp(LocalPos.X, -1.0e8, 1.0e8))');
  });

  it('a data layer asset and a sublevel are made only under a path their package can take', () => {
    const layer = code('Domains', 'LevelStructure', 'McpLevelStructureDataLayerCreation.cpp');
    const validated = layer.indexOf('McpBuildValidatedAssetPath(DataLayerAssetPath, DataLayerName, TEXT("data layer"), FullAssetPath, PathError, PathCode)');
    expect(validated).toBeGreaterThan(-1);
    expect(validated).toBeLessThan(layer.indexOf('McpCreatePackageForNewAsset(FullAssetPath'));
    expect(layer).toContain('McpSafeOperations::IsEngineContentPackage(FullAssetPath)');
    expect(layer).not.toContain('TEXT("/Game/") + FullAssetPath');
    const sublevel = code('Domains', 'LevelStructure', 'McpLevelStructureSublevels.cpp');
    const refused = sublevel.indexOf('!FullSublevelPath.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(FullSublevelPath)');
    expect(refused).toBeGreaterThan(-1);
    expect(refused).toBeLessThan(sublevel.indexOf('CreateLevelWorld(SublevelPackageName'));
    expect(sublevel).not.toContain('TEXT("/Game/") + FullSublevelPath');
  });

  it('a material function call names its pins from the names saved on it, not through its transient pointers', () => {
    for (const file of [['Queries', 'McpMatGetNodeProps.cpp'], ['McpMatMainInputs.h'], ['Connections', 'McpMatConnectNodes.cpp']]) {
      const source = code('Domains', 'MaterialAuthoring', ...file);
      expect(source, file.join('/')).not.toMatch(/ExpressionInput->InputName|ExpressionOutput->OutputName/u);
    }
    const props = code('Domains', 'MaterialAuthoring', 'Queries', 'McpMatGetNodeProps.cpp');
    expect(props).toContain('FI.Input.InputName.ToString()');
    expect(props).toContain('FO.Output.OutputName.ToString()');
  });

  it('no walk over a material expression\'s inputs ends on a null GetInput', () => {
    // Before UE 5.5 seven engine overrides of GetInput never return null, so such a walk hangs or asserts there.
    const folder = join(PRIVATE, 'Domains', 'MaterialAuthoring');
    let walks = 0;
    for (const entry of readdirSync(folder, { recursive: true, encoding: 'utf8' })) {
      if (!/\.(?:cpp|h)$/u.test(entry)) continue;
      for (const line of code('Domains', 'MaterialAuthoring', entry).split('\n')) {
        if (!/^\s*(?:for|while)\s*\(.*GetInput\(/u.test(line)) continue;
        walks += 1;
        expect(line, entry).toContain('McpCountMaterialInputs(');
      }
    }
    expect(walks).toBeGreaterThanOrEqual(1);
    const helper = code('Domains', 'MaterialAuthoring', 'McpMatMainInputs.h');
    expect(helper).toMatch(/return Expr->CountInputs\(\);\s*#elif MCP_HAS_MATERIAL_EXPRESSION_INPUTS_VIEW\s*return Expr->GetInputsView\(\)\.Num\(\);\s*#else\s*return Expr->GetInputs\(\)\.Num\(\);/u);
    expect(code('Domains', 'MaterialAuthoring', 'McpMatPrivate.h')).toContain('const int32 InputCount = McpCountMaterialInputs(Expr);');
  });

  it('nav mesh numbers are checked against the engine\'s limits before any is written', () => {
    const nav = code('Domains', 'Navigation', 'McpNavNavMeshSettings.cpp');
    expect(nav).toContain('McpPropertyReflection::CheckRange(Property, Value, Field, RangeError)');
    const refused = nav.indexOf('if (!RangeError.IsEmpty())');
    expect(refused).toBeGreaterThan(-1);
    expect(refused).toBeLessThan(nav.indexOf('bCheckOnly = false;'));
    expect(nav).toContain('NavMesh->PostEditChangeProperty(Changed);');
    expect(code('Foundation', 'Reflection', 'McpPropertyReflection.h')).toContain('Property->GetMetaData(TEXT("ClampMin"))');
    expect(code('Domains', 'Render', 'McpRenderSupportSettings.h')).toContain('McpPropertyReflection::CheckRange(Property, Number, FString(*Pair.Key), OutError)');
  });

  it('a bone edit that indexes the raw bones finds its bone among the raw bones, never a virtual one', () => {
    const create = code('Domains', 'Skeleton', 'Bones', 'McpSkeletonStructureCreation.cpp');
    expect(create).toContain('ParentIndex = RefSkeleton.FindRawBoneIndex(FName(*ParentName));');
    const transform = code('Domains', 'Skeleton', 'Bones', 'McpSkeletonBoneTransforms.cpp');
    expect(transform).toContain('const int32 BoneIndex = RefSkeleton.FindRawBoneIndex(FName(*BoneName));');
    expect(transform).toContain('TEXT("VIRTUAL_BONE")');
    expect(transform.indexOf('FindRawBoneIndex(')).toBeLessThan(transform.indexOf('Modifier.UpdateRefPoseTransform(BoneIndex'));
  });

  it('a material is set only in a slot the component has', () => {
    // UMeshComponent::SetMaterial grows its override list up to the index it is given.
    const spline = code('Domains', 'Spline', 'McpSplineMeshRuntime.cpp');
    const refused = spline.indexOf('MaterialIndex < 0 || MaterialIndex >= TargetComp->GetNumMaterials()');
    expect(refused).toBeGreaterThan(-1);
    expect(refused).toBeLessThan(spline.indexOf('TargetComp->SetMaterial(MaterialIndex, Material)'));
    const surface = code('Domains', 'Environment', 'Runtime', 'McpEnvActorSurface.cpp');
    expect(surface).toMatch(/if \(Component && MaterialIndex >= 0 && MaterialIndex < Component->GetNumMaterials\(\)\)\s*\{\s*Component->Modify\(\);\s*Component->SetMaterial\(MaterialIndex, Material\);/u);
  });

  it('a spline scatter refuses a count past its ceiling before it makes a component', () => {
    const scatter = code('Domains', 'Spline', 'McpSplineScatter.cpp');
    const refused = scatter.indexOf('if (Steps + 1.0 > MaxScatteredMeshes)');
    expect(refused).toBeGreaterThan(-1);
    expect(refused).toBeLessThan(scatter.indexOf('NewObject<UStaticMeshComponent>('));
    expect(scatter).not.toContain('FMath::FloorToInt(SplineLength / Spacing)');
  });

  it('a widget animation is made only under a name the Blueprint holds nothing under', () => {
    const create = code('Domains', 'WidgetAuthoring', 'Animation', 'McpWidgetAnimCore.cpp');
    const held = create.indexOf('Validity == EValidatorResult::AlreadyInUse || FindObject<UObject>(WidgetBP, *AnimationName) != nullptr');
    expect(held).toBeGreaterThan(-1);
    // Made by the shared creation (Support/McpWidgetGuidReg.cpp), called after the check.
    expect(held).toBeLessThan(create.indexOf('McpCreateWidgetAnimation(WidgetBP, AnimationName,'));
    expect(code('Domains', 'WidgetAuthoring', 'Support', 'McpWidgetGuidReg.cpp')).toContain('NewObject<UWidgetAnimation>(WidgetBP, FName(*AnimationName)');
    expect(create).toContain('INameValidatorInterface::GetErrorString(AnimationName, Validity)');
  });

  it('a widget relation names at most 64 widgets', () => {
    // Each member goes through a linear, case-insensitive AddUnique before any is looked up: an unbounded list ran
    // quadratic on the game thread.
    const record = readFileSync(join('src', 'tools', 'catalog', 'capabilities', 'records', 'manage-blueprint', 'widget-layout.ts'), 'utf8');
    expect(record).toContain("widgets: { type: 'array', items: { type: 'string' }, minItems: 2, maxItems: 64,");
  });

  it('a sequence is not added to a track of a sequence it already plays', () => {
    const assets = code('Domains', 'Sequence', 'Cinematics', 'McpSeqCineAssets.cpp');
    expect(assets).toContain('Subs->ContainsSequence(*Outer, true)');
    expect(assets).toContain('Shots->ContainsSequence(*Outer, true)');
    for (const [guard, add] of [['WouldPlayItself(Master, Sub)', 'Sub, GetFrame('], ['WouldPlayItself(Master, ShotSeq)', 'ShotSeq,']] as const) {
      const at = assets.indexOf(guard);
      expect(at, guard).toBeGreaterThan(-1);
      expect(at, guard).toBeLessThan(assets.indexOf(add, at + guard.length));
    }
    expect(assets.split('TEXT("CYCLE_DETECTED")')).toHaveLength(3);
  });

  it('the auto edit refuses a cut table past its bound, and no shot is longer than the range', () => {
    const plan = code('Domains', 'Sequence', 'Planner', 'McpAutoEditPlan.cpp');
    expect(plan).toContain('constexpr int64 EditMostCells = 4000000;');
    expect(plan).toContain('static_cast<int64>(Plan.Segments) * Cameras * (static_cast<int64>(Plan.MaxRun) + 1)');
    expect(plan).toContain('FMath::FloorToInt(FMath::Min(Longest / Plan.SegmentSeconds + 1e-6, Whole))');
    expect(plan).toContain('FMath::CeilToInt(FMath::Min(Shortest / Plan.SegmentSeconds - 1e-6, Whole))');
    expect(code('Domains', 'Sequence', 'Planner', 'McpAutoEdit.cpp')).toContain('ReadTiming(Payload, *Sequence, Cameras.Num(), Plan)');
  });

  it('run_ubt polls its build from the core ticker and answers when it ends', () => {
    // Waiting for the build on the game thread stopped the editor ticking and draining its queue for the whole build.
    const build = code('Domains', 'SystemControl', 'McpSystemBuild.cpp');
    expect(build).toContain('FTSTicker::GetCoreTicker().AddTicker(');
    expect(build).toContain('Subsystem->RunLatentReply(Run->RequestId, TEXT("system_control run_ubt")');
    expect(build).not.toContain('FPlatformProcess::Sleep(');
    expect(build).not.toMatch(/while \(FPlatformProcess::IsProcRunning/u);
  });

  it('the package and world of a new level are made in one place', () => {
    const made = filesNaming('UWorld::CreateWorld(').filter((entry) => code(entry).includes('UWorld::CreateWorld(')).map((entry) => entry.split('\\').join('/'));
    expect(made).toEqual(['Domains/LevelStructure/McpLevelStructureLevelCreationCleanup.cpp']);
    for (const file of ['McpLevelStructureLevelCreation.cpp', 'McpLevelStructureSublevels.cpp']) {
      const source = code('Domains', 'LevelStructure', file);
      expect(source, file).toContain('CreateLevelWorld(');
      expect(source, file).toContain('SaveLevelWorld(');
    }
  });

  it('a generated texture is not changed while its first build is still running', () => {
    // A colour space changed under the build fails a check in the engine's texture build task: PreEditChange waits.
    const assets = code('Domains', 'Texture', 'McpTextureAssets.cpp');
    const create = assets.slice(assets.indexOf('UTexture2D* CreateEmptyTexture('), assets.indexOf('bool UpdateTextureBGRA8('));
    const started = create.indexOf('NewTexture->PostEditChange();');
    expect(started).toBeGreaterThan(-1);
    expect(create.indexOf('NewTexture->PreEditChange(nullptr);', started)).toBeGreaterThan(started);
    expect(create.indexOf('NewTexture->PreEditChange(nullptr);')).toBeLessThan(create.indexOf('GetPlatformData()'));
  });

  it('an occlusion bake refuses a ray count that would hold the editor for minutes', () => {
    const bake = code('Domains', 'Texture', 'McpTextureAmbientOcclusion.cpp');
    expect(bake).toContain('constexpr int64 MaxAoBakeRays = 128ll * 1024 * 1024;');
    const refused = bake.indexOf('if (Rays > MaxAoBakeRays)');
    expect(refused).toBeGreaterThan(-1);
    expect(refused).toBeLessThan(bake.indexOf('Baker.Bake();'));
    expect(refused).toBeLessThan(bake.indexOf('LoadObject<UStaticMesh>'));
  });

  it('a spawn that names its actor asks for the name, so a taken one is renamed and not fatal', () => {
    const named: string[] = [];
    for (const entry of filesNaming('FActorSpawnParameters')) {
      if (/Tests?\.cpp$/u.test(entry)) continue;
      const source = code(entry);
      for (const [, params] of source.matchAll(/FActorSpawnParameters (\w+);/gu)) {
        const name = new RegExp(`(?<!\\w)${params}\\.Name = (?!MakeUniqueObjectName)`, 'u');
        if (!name.test(source)) continue;
        named.push(entry);
        expect(source, entry).toContain(`${params}.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;`);
      }
    }
    expect(named.length).toBeGreaterThanOrEqual(4);
  });
});
