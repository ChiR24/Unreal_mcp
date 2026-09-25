/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

const lookup = readFileSync(
  resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlActor/McpAutomationBridge_ControlActorLookup.cpp'),
  'utf8',
);

describe('actor list summary contracts', () => {
  it('counts every matching actor by class, tag and folder before paging applies', () => {
    const summaryAt = lookup.indexOf('if (bSummary) {\n      ++ByClass.FindOrAdd(Actor->GetClass()->GetName());'.replace(/\n/g, lookup.includes('\r\n') ? '\r\n' : '\n'));
    const pagingAt = lookup.indexOf('if (TotalCount <= Offset || (Limit > 0 && ActorsArray.Num() >= Limit))');
    expect(summaryAt).toBeGreaterThan(-1);
    expect(pagingAt).toBeGreaterThan(summaryAt);
    for (const field of ['byClass', 'byTag', 'byFolder']) expect(lookup).toContain(`TEXT("${field}")`);
  });

  it('reports a name the class lacks instead of dropping it', () => {
    expect(lookup).toContain('Entry->SetArrayField(TEXT("missingProperties"), Missing);');
  });
});
