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
    const library = code('Domains', 'AssetWorkflow', 'Operations', 'McpAssetFabLibrary.cpp');
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
    expect(valid).toBeLessThan(helper.indexOf('CreatePackage(*PackagePath)'));
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
    // The one bare CreatePackage left in the audio effects takes a name built from a known effect type.
    const effects = code('Domains', 'AudioAuthoring', 'McpAudioEffects.cpp');
    const known = effects.indexOf('if (!SourceEffectPresetClass(EffectType))');
    expect(known).toBeGreaterThan(-1);
    expect(known).toBeLessThan(effects.indexOf('CreatePackage(*PresetPkg)'));
  });

  it('a spawn that names its actor asks for the name, so a taken one is renamed and not fatal', () => {
    const named: string[] = [];
    for (const entry of readdirSync(PRIVATE, { recursive: true, encoding: 'utf8' })) {
      if (!/\.(?:cpp|h)$/u.test(entry) || /Tests?\.cpp$/u.test(entry)) continue;
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
