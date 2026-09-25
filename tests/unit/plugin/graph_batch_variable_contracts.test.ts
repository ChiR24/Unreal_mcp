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

describe('graph batch variable contracts', () => {
  it('lets one graph batch declare the variables its nodes use', () => {
    const isBatchable = steps.slice(steps.indexOf('bool IsBatchableEdit'), steps.indexOf('void ExpandEndpoint'));

    expect(isBatchable).toContain('Edit == TEXT("add_variable")');
    // The step runs the ordinary handler, which compiles, so later Get/Set steps resolve.
    expect(steps).toMatch(
      /if \(Edit == TEXT\("add_variable"\)\)\s*\{[\s\S]*?McpBlueprintHandlers::HandleBlueprintAddVariable\(McpBlueprintHandlers::BuildBlueprintActionContext\(/,
    );
  });
});
