/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const steps = readFileSync(
  resolve(
    process.cwd(),
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersBatchSteps.cpp',
  ),
  'utf8',
);

const batch = readFileSync(
  resolve(
    process.cwd(),
    'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersBatch.cpp',
  ),
  'utf8',
);

describe('graph batch variable contracts', () => {
  it('names the graph entry node "$entry" so a construction script can be wired without a lookup', () => {
    expect(batch).toMatch(/if \(Existing->IsA<UK2Node_FunctionEntry>\(\)\)\s*\{\s*State\.Aliases\.Add\(TEXT\("entry"\), Existing->NodeGuid\.ToString\(\)\);/);
  });

  it('checks every function and variable name before applying any step', () => {
    const precheckAt = batch.indexOf('const FString Precheck = PrecheckSteps(Context, *Steps, BadIndex, BadCode);');
    const firstStepAt = batch.indexOf('FBatchState State;');
    expect(precheckAt).toBeGreaterThan(-1);
    expect(firstStepAt).toBeGreaterThan(precheckAt);
    expect(batch).toContain('!ResolveGraphCallFunction(Context.Blueprint, Member, MemberClass, ResolvedClass)');
    expect(batch).toContain('Nothing was applied.');
    // A variable declared by an earlier add_variable step in the same batch is not a miss.
    expect(batch).toContain('!Declared.Contains(Variable)');
  });

  it('lets one graph batch declare the variables its nodes use', () => {
    const isBatchable = steps.slice(steps.indexOf('bool IsBatchableEdit'), steps.indexOf('void ExpandEndpoint'));

    expect(isBatchable).toContain('Edit == TEXT("add_variable")');
    // The step runs the ordinary handler, which compiles, so later Get/Set steps resolve.
    expect(steps).toMatch(
      /if \(Edit == TEXT\("add_variable"\)\)\s*\{[\s\S]*?McpBlueprintHandlers::HandleBlueprintAddVariable\(McpBlueprintHandlers::BuildBlueprintActionContext\(/,
    );
  });
});

const privateRoot = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private';
const source = (rel: string): string => readFileSync(resolve(process.cwd(), privateRoot, rel), 'utf8');

// A build_graph batch hung the editor for good: every step saved the Blueprint,
// and one save opened a modal dialog nobody could close.
describe('batch saves and unattended saves', () => {
  it('saves a build_graph batch once, and still saves the applied steps when it stops early', () => {
    expect(batch).toMatch(/FMcpDeferAssetSaves DeferSave;\s*Error = RunBatchStep\(/);
    const stopped = batch.slice(batch.indexOf('if (Error.IsEmpty())'), batch.indexOf('build_graph stopped at operations'));
    expect(stopped).toContain('SaveLoadedAssetThrottled(Context.Blueprint);');
  });

  it('skips a deferred save in the one throttled-save funnel, shared across translation units', () => {
    const registry = source('Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h');
    expect(registry).toMatch(/^inline int32 &McpAssetSaveDeferralDepth\(\) \{/m);
    expect(registry).toContain('if (!bForce && McpAssetSaveDeferralDepth() > 0)');
  });

  it('never lets a save open a modal dialog', () => {
    const assetSave = source('Safety/McpSafeOperationsAssetSave.h');
    const guardAt = assetSave.indexOf('TGuardValue<bool> UnattendedSave(GIsRunningUnattendedScript, true);');
    expect(guardAt).toBeGreaterThan(-1);
    expect(guardAt).toBeLessThan(assetSave.indexOf('UPackageTools::SavePackagesForObjects('));
    expect(source('Safety/McpSafeOperationsLevelSave.h')).toMatch(
      /TGuardValue<bool> UnattendedSave\(GIsRunningUnattendedScript, true\);\s*bSaveSucceeded = FEditorFileUtils::SaveLevel\(/,
    );
  });
});
