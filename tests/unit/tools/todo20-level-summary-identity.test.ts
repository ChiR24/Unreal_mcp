// Todo 20 BB-018 — level get_summary identity projection source contract.
//
// A loaded level's get_summary must be identifiable as a map asset without a
// follow-up list_levels call: the loaded branch must emit the full identity set
// (packageName/assetName/objectPath/assetClass/tagsAndValues) and derive levelName
// from the package short name (FPackageName::GetShortName), never from
// ULevel::GetName() (which yields the instance name "PersistentLevel").
//
// Live proof (levelName == map asset short name on a loaded ULW level) runs at
// Todo 39 against a live editor.

import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE_ROOT = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private'
);
const RECORDS_ROOT = resolve(
  process.cwd(),
  'src/tools/catalog/capabilities/records/manage-level'
);

function read(path: string): string {
  expect(existsSync(path), `missing file: ${path}`).toBe(true);
  return readFileSync(path, 'utf8');
}

/** Strip comments so a claim in prose cannot satisfy a code contract. */
function code(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
}

const levelHandlersInfo = () =>
  read(resolve(PRIVATE_ROOT, 'Domains', 'Level', 'World', 'McpAutomationBridge_LevelHandlersInfo.cpp'));
const levelOperationsRecord = () => resolve(RECORDS_ROOT, 'operations.data.ts');

describe('Todo 20 BB-018 loaded-branch get_summary emits full map-asset identity', () => {
  it('the loaded branch emits packageName, assetName, objectPath, assetClass and tagsAndValues', () => {
    const source = code(levelHandlersInfo());
    // Scope to the loaded branch (TargetLevel non-null short-circuit).
    const loadedStart = source.indexOf('if (TargetLevel)');
    expect(loadedStart).toBeGreaterThan(-1);
    const loadedBranch = source.slice(
      loadedStart,
      source.indexOf('SendAutomationResponse', loadedStart)
    );

    expect(loadedBranch).toContain('SetStringField(TEXT("packageName")');
    expect(loadedBranch).toContain('SetStringField(TEXT("assetName")');
    expect(loadedBranch).toContain('SetStringField(TEXT("objectPath")');
    expect(loadedBranch).toContain('SetStringField(TEXT("assetClass")');
    expect(loadedBranch).toContain('SetObjectField(TEXT("tagsAndValues")');
  });

  it('levelName is derived from the package short name, not ULevel::GetName()', () => {
    const source = code(levelHandlersInfo());
    const loadedStart = source.indexOf('if (TargetLevel)');
    const loadedBranch = source.slice(
      loadedStart,
      source.indexOf('SendAutomationResponse', loadedStart)
    );

    // The bug: levelName = TargetLevel->GetName() yields "PersistentLevel".
    expect(loadedBranch, 'levelName must not be the ULevel instance name').not.toContain(
      'SetStringField(TEXT("levelName"), TargetLevel->GetName())'
    );
    // The fix: levelName mirrors the asset short name already computed via
    // FPackageName::GetShortName (AssetName).
    expect(loadedBranch, 'levelName must be the package short name (AssetName)').toContain(
      'SetStringField(TEXT("levelName"), AssetName)'
    );
  });

  it('the loaded branch derives the short name via FPackageName::GetShortName', () => {
    const source = code(levelHandlersInfo());
    const loadedStart = source.indexOf('if (TargetLevel)');
    const loadedBranch = source.slice(
      loadedStart,
      source.indexOf('SendAutomationResponse', loadedStart)
    );
    expect(loadedBranch).toContain('FPackageName::GetShortName');
  });
});

describe('Todo 20 BB-018 the get_summary record still declares the full identity set', () => {
  // Guard against a Wave-2 record shrink: the native projection depends on the
  // record advertising all 9 output keys.
  it('manage_level.get_summary outputProps declares all 9 identity keys', () => {
    const source = read(levelOperationsRecord());
    const summaryStart = source.indexOf("action: 'get_summary'");
    expect(summaryStart).toBeGreaterThan(-1);
    // Slice to the next record boundary so the full outputProps object is in scope.
    const summaryBlock = source.slice(
      summaryStart,
      source.indexOf('buildCoreRecord({', summaryStart)
    );
    const outputStart = summaryBlock.indexOf('outputProps:');
    expect(outputStart).toBeGreaterThan(-1);
    const outputBlock = summaryBlock.slice(
      outputStart,
      summaryBlock.indexOf('normalizationClass', outputStart)
    );

    for (const key of [
      'levelPath',
      'levelName',
      'actorCount',
      'loaded',
      'packageName',
      'assetName',
      'objectPath',
      'assetClass',
      'tagsAndValues',
    ]) {
      expect(outputBlock, `outputProps must declare ${key}`).toContain(key);
    }
  });
});
