/**
 * Source contracts for the unreferenced asset report and the project's config files.
 *
 * An asset only a config file names (a default map or game mode, or the parent materials an importer copies from,
 * kept in its per-user settings) has no referencer the asset registry knows of, so the report listed it as unused; a
 * cleanup that trusted it deleted an importer's parent materials and every later import came in broken. What must
 * stay true: the config files are read through the project-file resolver, an asset one names is never a delete
 * candidate, and nothing from a config file but the /Game paths it names reaches the reply, beside its own file name
 * (a config file can hold a token).
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

import { sliceBetween } from './plugin-contract-fixtures.js';

/** Comment bodies explain the rules, so rule checks ignore them. */
const code = (text: string): string =>
  text.replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');

const report = code(readFileSync(resolve(process.cwd(),
  'plugins/EngineWire/Source/EngineWire/Private/Domains/AssetWorkflow/Analysis/McpAssetReports.cpp'), 'utf8'));
const scan = sliceBetween(report, 'ReportConfigNamedPackages() {', 'ReportAssetRow(');
const handler = sliceBetween(report, 'HandleGenerateReport(', 'bool bFileWritten');

describe('the config files the unreferenced report reads', () => {
  it('are the project\'s own .ini files, Config and Saved/Config and a folder down in each', () => {
    expect(scan).toContain('{TEXT("Config"), TEXT("Saved/Config")}');
    expect(scan).toMatch(/FindFiles\(Subfolders, \*\(Project \/ Root \/ TEXT\("\*"\)\), false, true\)/u);
    expect(scan).toMatch(/FindFiles\(Inis, \*\(Project \/ Folder \/ TEXT\("\*\.ini"\)\), true, false\)/u);
  });

  it('are each read only once the project-file resolver has kept the path inside the project', () => {
    const resolved = scan.indexOf('McpResolveProjectFilePath(Relative, File, PathError)');
    expect(resolved).toBeGreaterThan(-1);
    expect(resolved).toBeLessThan(scan.indexOf('FFileHelper::LoadFileToString(Text, *File)'));
  });

  it('give up nothing but the /Game package names in them: no value is kept, logged or put in a reply', () => {
    expect(scan).toContain('Text.Find(TEXT("/Game/"))');
    // The one thing kept from a file: the package name that follows /Game/, with the file's project-relative name.
    expect(scan.match(/\bNamed\.\w+\(/gu)).toEqual(['Named.FindOrAdd(']);
    expect(scan).toContain('Named.FindOrAdd(Text.Mid(At, End - At)).AddUnique(Relative);');
    expect(scan).not.toMatch(/UE_LOG|Set\w+Field|SendAutomation/u);
  });
});

describe('an asset a config file names', () => {
  it('is taken out of the delete candidates and listed apart, with the files that name it', () => {
    const configPass = handler.indexOf('ReportConfigNamedPackages()');
    expect(configPass).toBeGreaterThan(handler.indexOf('Registry.GetReferencers(Asset.PackageName, Referencers);'));
    expect(handler).toContain('Named.Find(Asset.PackageName.ToString())');
    expect(handler).toContain('Row->SetArrayField(TEXT("configFiles"), FileValues);');
    expect(handler).toMatch(/return Files != nullptr;\s*\}\);/u);
    expect(handler).toContain('Resp->SetArrayField(TEXT("configReferenced"), ConfigRows);');
  });

  it('is only looked for in the unreferenced report', () => {
    expect(handler).toMatch(/if \(bUnreferenced\) \{[\s\S]*ReportConfigNamedPackages\(\)[\s\S]*\}\s*TArray<TSharedPtr<FJsonValue>> AssetsArray;/u);
  });
});
