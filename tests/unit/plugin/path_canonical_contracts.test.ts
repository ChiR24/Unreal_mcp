/**
 * Source contracts for request paths that skipped the shared canonicalizer.
 *
 * Every asset path a request names goes through SanitizeProjectRelativePath (directly, through a domain's validator
 * built on it, or through a loader that calls it) before anything loads it. These sites handed the caller's text to
 * LoadObject, LoadClass or an engine function that writes assets itself, and the MetaHuman folders could name engine
 * content, which McpSafeAssetSave refuses but those engine functions save on their own.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const privateRoot = resolve(process.cwd(), 'plugins/EngineWire/Source/EngineWire/Private');
const code = (file: string): string =>
  readFileSync(resolve(privateRoot, file), 'utf8')
    .replace(/\/\*[\s\S]*?\*\//gu, '')
    .replace(/^[ \t]*\/\/.*$/gmu, '');

describe('request paths go through the shared canonicalizer before a load', () => {
  it('PCG texture, post-process LUT, scene-capture target and SCS assets', () => {
    const pcg = code('Domains/PCG/McpPCGSpawnerSettings.cpp');
    expect(pcg).toContain('LoadPCGAsset<UTexture>(TexturePath');
    expect(pcg).not.toContain('LoadObject<UTexture>(nullptr, *TexturePath)');
    const lut = code('Domains/Render/McpRenderPostProcessColor.cpp');
    expect(lut).toContain('SanitizeProjectRelativePath(LutPath)');
    expect(lut).not.toContain('LoadObject<UTexture>(nullptr, *LutPath)');
    const capture = code('Domains/Render/McpRenderSceneCapture.cpp');
    expect(capture).toContain('SanitizeProjectRelativePath(Path)');
    expect(capture).not.toContain('LoadObject<UTextureRenderTarget2D>');
    const scs = code('Domains/SCS/McpSCSAddComponent.cpp');
    expect(scs).toContain('SanitizeProjectRelativePath(MeshPath)');
    expect(scs).toContain('SanitizeProjectRelativePath(MaterialPath)');
    expect(scs).not.toMatch(/LoadObject<U(StaticMesh|SkeletalMesh|MaterialInterface)>\(nullptr, \*(MeshPath|MaterialPath)\)/u);
  });

  it('Sequence sub-sequences, shots, animations and camera shakes', () => {
    const assets = code('Domains/Sequence/Cinematics/McpSeqCineAssets.cpp');
    expect(assets).toContain('SanitizeProjectRelativePath(SubPath)');
    expect(assets).toContain('SanitizeProjectRelativePath(ShotPath)');
    expect(assets).not.toContain('LoadObject<ULevelSequence>(nullptr, *SubPath)');
    expect(code('Domains/Sequence/Cinematics/McpSeqCineBindingTracks.cpp')).toContain('SanitizeProjectRelativePath(AnimPath)');
    const shake = code('Domains/Sequence/Cinematics/McpSeqCineCameraTracks.cpp');
    expect(shake).toContain('SanitizeProjectRelativePath(RawPath)');
    expect(shake).not.toContain('LoadObject<UObject>(nullptr, *Path)');
  });

  it('MetaHuman characters and the folders its engine functions write into', () => {
    const shared = code('Domains/MetaHuman/McpMetaHumanShared.cpp');
    expect(shared).toContain('const FString AssetPath = SanitizeProjectRelativePath(RawPath);');
    expect(shared).toContain('McpSafeOperations::IsEngineContentPackage(OutFolder)');
    const build = code('Domains/MetaHuman/McpMetaHumanBuild.cpp');
    expect(build).toContain('ReadContentFolderOrReply(Self, RequestId, Socket, TEXT("buildPath")');
    expect(build).toContain('ReadContentFolderOrReply(Self, RequestId, Socket, TEXT("commonFolderPath")');
    expect(build).toContain('IsValidObjectName');
    expect(code('Domains/MetaHuman/McpMetaHumanExport.cpp')).toContain('ReadContentFolderOrReply(Self, RequestId, Socket, TEXT("projectPath")');
  });

  it('Niagara dynamic inputs, copied emitters and the graph route', () => {
    expect(code('Domains/NiagaraAuthoring/McpNiagaraDynamicInput.cpp')).toContain(
      'ValidateAndSanitizePath(Context, DynamicInputScriptPath, TEXT("dynamicInputScriptPath"))',
    );
    expect(code('Domains/NiagaraAuthoring/McpNiagaraEmitterCopy.cpp')).toContain(
      'ValidateAndSanitizePath(Context, SourcePath, TEXT("sourceSystemPath"))',
    );
    const graph = code('Domains/NiagaraGraph/McpNiagaraGraphHandlers.cpp');
    expect(graph).toContain('McpNiagaraAuthoringHandlers::LoadSystemOrError(Context)');
    expect(graph).toContain('ValidateAndSanitizePath(Context, ModulePath, TEXT("modulePath"))');
    expect(graph).not.toContain('LoadObject<UNiagaraSystem>(nullptr, *AssetPath)');
  });
});
