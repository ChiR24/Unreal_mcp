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
