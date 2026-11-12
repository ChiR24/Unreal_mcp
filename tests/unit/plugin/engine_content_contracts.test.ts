/**
 * Source contracts for the engine-content rule.
 *
 * McpSafeAssetSave refused a package under /Engine/ because engine content is shared by every project on the install,
 * while the content of every engine plugin (/Niagara, /ControlRig, a plugin installed into the engine) sits in that
 * install too and was saved in place. One predicate, McpSafeOperations::IsEngineContentPackage, answers for both, and
 * the save rule and every handler that refuses to change engine content ask it.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const privateRoot = resolve(process.cwd(), 'plugins/EngineWire/Source/EngineWire/Private');
const code = (file: string): string =>
  readFileSync(resolve(privateRoot, file), 'utf8')
    .replace(/\/\*[\s\S]*?\*\//gu, '')
    .replace(/^[ \t]*\/\/.*$/gmu, '');

const refusals = [
  'Domains/AI/Blackboard/McpAIBlackboardDefaults.cpp',
  'Domains/AssetWorkflow/Materials/McpAssetMeshMats.cpp',
  'Domains/AssetWorkflow/Materials/McpAssetMeshMatsMany.cpp',
  'Domains/AssetWorkflow/Materials/McpMaterialParent.cpp',
  'Domains/AssetWorkflow/Operations/McpAssetThumbnails.cpp',
  'Domains/AssetWorkflow/Optimization/McpAssetLODs.cpp',
  'Domains/AssetWorkflow/Optimization/McpAssetMeshCollision.cpp',
  'Domains/AssetWorkflow/Optimization/McpAssetNanite.cpp',
  'Domains/AssetWorkflow/Reimport/McpAssetReimport.cpp',
  'Domains/AssetWorkflow/Structs/McpAssetStructsAssetOpsRename.cpp',
  'Domains/AudioAuthoring/McpAudioLooping.cpp',
  'Domains/AudioAuthoring/McpAudioLoudness.cpp',
  'Domains/AudioAuthoring/McpAudioSubtitles.cpp',
  'Domains/Geometry/Mesh/McpGeometryLodOperations.cpp',
  'Domains/MaterialAuthoring/Properties/McpMatSetMatEnum.cpp',
  'Domains/Property/McpPropTarget.cpp',
  'Domains/Texture/McpTextureShared.cpp',
];

describe('engine content is one rule', () => {
  it('the predicate takes /Engine and every package whose file is under the engine directory', () => {
    const classification = code('Safety/McpSafeOperationsAssetClassification.h');
    expect(classification).toContain('inline bool IsEngineContentPackage(const FString& PackageName)');
    expect(classification).toContain('FPackageName::TryConvertLongPackageNameToFilename(PackageName, File)');
    expect(classification).toContain('FPaths::EngineDir()');
  });

  it('the save rule asks it', () => {
    expect(code('Safety/McpSafeOperationsAssetSave.h')).toContain('if (IsEngineContentPackage(PackageName))');
  });

  it('every engine-content refusal asks it instead of testing the /Engine/ prefix', () => {
    for (const file of refusals) {
      const source = code(file);
      expect(source, file).toContain('McpSafeOperations::IsEngineContentPackage(');
      expect(source, file).not.toContain('StartsWith(TEXT("/Engine/"))');
    }
  });
});
