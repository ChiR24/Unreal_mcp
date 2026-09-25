/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const dir = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/BlueprintGraph/';
const views = ['McpAutomationBridge_BlueprintGraphHandlersDetails.cpp', 'McpAutomationBridge_BlueprintGraphHandlersQueries.cpp'];

describe('graph pin link contracts', () => {
  it.each(views)('%s names the node at the far end of every link', (file) => {
    const source = readFileSync(resolve(process.cwd(), dir + file), 'utf8');
    expect(source).toMatch(/TEXT\("nodeTitle"\),\s*LinkedPin->GetOwningNode\(\)->GetNodeTitle\(ENodeTitleType::ListView\)\.ToString\(\)\);/);
  });
});
