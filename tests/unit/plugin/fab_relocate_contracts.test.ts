/**
 * Source contracts for moving and naming what a Fab import created.
 *
 * Fab lands a Megascans add under machine names (/Game/Fab/Megascans/3D/<name>_<id>/High/<id>_tier_1/...),
 * and its first import also installs master materials that every later import uses. The add can now name a
 * /Game folder and an asset name, applied after the import settles. What must stay true: the shared Fab
 * folders and everything outside /Game/Fab are never touched, the move goes through the machinery
 * asset.move uses, it is all or nothing, and nothing here saves, deletes or moves a file behind the
 * editor's back.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const fabDir = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow/Fab';
const read = (file: string): string => readFileSync(resolve(process.cwd(), fabDir, file), 'utf8');
const handlers = (file: string): string =>
  readFileSync(resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AssetWorkflow', file), 'utf8');

/** Comment bodies explain the rules, so rule checks ignore them. */
const code = (text: string): string =>
  text.replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');

const rules = code(read('McpAutomationBridge_FabRelocateRules.h'));
const relocate = code(read('McpAutomationBridge_FabRelocate.cpp'));

describe('what a relocation may touch', () => {
  it('considers only assets in a folder under /Game/Fab/, and never the shared ones', () => {
    expect(rules).toContain('return TEXT("/Game/Fab/");');
    expect(relocate).toContain('if (!Folder.StartsWith(FabFolderPrefix()) || IsSharedFolder(Folder))');
    // The folders Fab installs once, for every Megascans import and every later one.
    expect(rules).toMatch(/Child == TEXT\("Materials"\) \|\| Child == TEXT\("MaterialFunctions"\) \|\| Child == TEXT\("Textures"\)/u);
    // What sits directly in /Game/Fab has no trailing slash to match the prefix, so it is never considered.
    expect(rules).toContain('Folder.StartsWith(FabFolderPrefix())');
  });

  it('judges the shared folders by the first folder under /Game/Fab, so a listing\'s own Materials folder is not one', () => {
    expect(rules).toMatch(/Rest\.FindChar\(TEXT\('\/'\), Slash\) \? Rest\.Left\(Slash\) : Rest/u);
  });

  it('leaves a unreal-engine pack, which lands outside /Game/Fab, where Fab put it, and says so', () => {
    expect(relocate).toMatch(/Nothing was moved: this import created nothing in a folder of its own under \/Game\/Fab/u);
    expect(relocate).toContain('asset.move relocates a folder.');
  });

  it('never removes a folder every import shares', () => {
    // /Game/Fab itself, /Game/Fab/Megascans and its type buckets (3D, Surfaces, ...) are not listing folders.
    expect(rules).toMatch(/Parts\[1\] == TEXT\("Fab"\) && Parts\[2\] == TEXT\("Megascans"\)/u);
    expect(rules).toContain('Parts.Num() > (bMegascans ? 4 : 2)');
    expect(relocate).toMatch(/for \(FString Current = Folder; IsListingFolder\(Current\); Current = FPaths::GetPath\(Current\)\)/u);
    expect(relocate).toMatch(/Left\.Num\(\) > 0 \|\| !McpSafeOperations::McpSafeDeleteFolder\(Current\)/u);
  });
});

describe('how a relocation moves things', () => {
  it('goes through the rename and redirector fix-up asset.move uses, and no other way', () => {
    expect(relocate).toContain('McpAssetRename::RenameWithSettingsFollow(Renames, MakeShared<FJsonObject>(), Failure)');
    expect(relocate).toContain('McpAssetRename::FixupRedirectorsIn(OldRoot, Found, Fixed);');
    for (const banned of ['SavePackage', 'DeleteAsset', 'DeleteObjects', 'ObjectTools', 'IFileManager', 'MoveFile', 'DeleteDirectory', 'RenameAssets']) {
      expect(relocate, `${banned} would bypass the safe machinery`).not.toContain(banned);
    }
  });

  it('is all or nothing: a taken or doubled target stops the move before anything is renamed', () => {
    const plan = relocate.slice(relocate.indexOf('bool Plan('), relocate.indexOf('void PruneEmptyFolders('));
    expect(plan).toMatch(/\(!Leaving\.Contains\(Target\) && McpAssetExists\(Target\)\) \|\| Arriving\.Contains\(Target\)/u);
    expect(plan).toMatch(/return false;/u);
    const apply = relocate.slice(relocate.indexOf('void Apply('));
    expect(apply.indexOf('if (!Plan(')).toBeLessThan(apply.indexOf('McpAssetRename::RenameWithSettingsFollow('));
  });

  it('moves only what the caller asked for, and keeps the layout under the new folder', () => {
    expect(relocate).toContain('Move.NewFolder = Destination + Move.OldFolder.Mid(OldRoot.Len());');
    expect(relocate).toContain('if (Move.NewPackage() != Move.OldPackage())');
  });

  it('rewrites the paths it reports, and the saved packages, to where the assets are now', () => {
    expect(relocate).toContain('for (TArray<FString>* Paths : {&ImportedPaths, &Result.SamplePaths})');
    expect(relocate).toContain('Result.RootPath = Destination;');
    const post = code(read('McpAutomationBridge_FabPostImport.cpp'));
    expect(post).toMatch(/McpFabRelocate::Apply\(DestinationFolder, AssetName, Result, Paths\);\s*\}\s*SaveImported\(Result, Paths\);/u);
  });
});

describe('how an asset is named', () => {
  it('names the one mesh SM_ or SK_, or for a surface the one material instance MI_, and nothing when there is no single one', () => {
    expect(rules).toMatch(/Role == ERole::SkeletalMesh \? TEXT\("SK_"\) : Role == ERole::MaterialInstance \? TEXT\("MI_"\) : TEXT\("SM_"\)/u);
    expect(relocate).toMatch(/\(OutMeshes == 1 && Move\.IsMesh\(\)\) \|\| \(OutMeshes == 0 && OutInstances == 1 && Move\.Role == ERole::MaterialInstance\)/u);
    expect(relocate).toContain('assetName was not applied');
  });

  it('lets materials and textures follow the stem of the anchor, and leaves any other name alone', () => {
    // ubitfhtfa_tier_1 -> stem ubitfhtfa; MI_ubitfhtfa -> MI_<name>; T_ubitfhtfa_4K_ORD -> T_<name>_4K_ORD.
    expect(rules).toContain('AnchorName.Find(TEXT("_tier_"), ESearchCase::IgnoreCase)');
    expect(rules).toMatch(/Rest != Stem && !Rest\.StartsWith\(Stem \+ TEXT\("_"\)\)/u);
    expect(rules).toContain('return Name.Left(Underscore + 1) + AssetName + Rest.Mid(Stem.Len());');
  });

  it('takes only an ASCII name an asset can have', () => {
    expect(relocate).toMatch(/Name\.Len\(\) > 64 \|\| FChar::IsDigit\(Name\[0\]\)/u);
    expect(relocate).toMatch(/\(Ch >= TEXT\('a'\) && Ch <= TEXT\('z'\)\)[\s\S]*Ch == TEXT\('_'\)/u);
  });
});

describe('the add and the status read', () => {
  const add = code(handlers('Operations/McpAutomationBridge_AssetWorkflowFabAdd.cpp'));

  it('refuses a destination outside /Game, and a name that is not one, before Fab is asked', () => {
    expect(add).toContain('SanitizeProjectRelativePath(RequestedDestination)');
    // A refused folder says why, with the helper's own wording, before the /Game test below.
    expect(add).toContain('McpPathRefusalMessage(TEXT("destinationPath"), RequestedDestination)');
    expect(add).toMatch(/Destination != TEXT\("\/Game"\) && !Destination\.StartsWith\(TEXT\("\/Game\/"\)\)/u);
    expect(add).toContain('!McpFabRelocate::IsValidAssetName(AssetName)');
    expect(add.indexOf('McpFabRelocate::IsValidAssetName(AssetName)')).toBeLessThan(add.indexOf('Provider->AddToProject('));
  });

  it('hands the validated values to the post-import step, which only the settled import reaches', () => {
    expect(add).toContain('McpFabPostImport::Run(Result, Paths, Destination, AssetName);');
    // A cancelled import skips the post-import step, so nothing is moved or saved for it.
    expect(code(readFileSync(resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridgeFab/Private/Import/McpFabImportWatcher.cpp'), 'utf8')))
      .toContain('if (PostImport && Count > 0 && !bCancelled)');
  });

  it('reports what the relocation did, and only when it ran', () => {
    const status = code(read('McpAutomationBridge_AssetWorkflowFabImportStatus.cpp'));
    expect(status).toMatch(/if \(Result\.bRelocateRan\) \{\s*Data->SetBoolField\(TEXT\("relocated"\), Result\.MovedCount > 0\);/u);
    expect(status).toContain('TEXT("movedCount")');
    expect(status).toContain('TEXT("relocationNote")');
  });
});
