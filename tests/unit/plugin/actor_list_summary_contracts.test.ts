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

  it('puts class, tag and folder names in values, never in JSON keys that redaction reads as field names', () => {
    // {"Level/Stage/Secrets": 1} lost its count to [REDACTED]; a {name, count} row keeps it.
    expect(lookup).toContain('Row->SetStringField(TEXT("name"),');
    expect(lookup).toContain('Row->SetNumberField(TEXT("count"), Pair.Value);');
    expect(lookup).not.toMatch(/Out->SetNumberField\(Pair\.Key/);
    for (const field of ['byClass', 'byTag', 'byFolder']) expect(lookup).toContain(`Data->SetArrayField(TEXT("${field}")`);
  });

  it('narrows by tag, class and outliner folder before counting or paging', () => {
    for (const field of ['tag', 'className', 'folder']) expect(lookup).toContain(`Payload->TryGetStringField(TEXT("${field}"),`);
    const filterAt = lookup.indexOf('if (!McpActorMatchesListFilters(Actor, TagFilter, ClassFilter, FolderFilter))');
    expect(filterAt).toBeGreaterThan(-1);
    expect(filterAt).toBeLessThan(lookup.indexOf('++TotalCount;'));
    const support = readFileSync(
      resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h'),
      'utf8',
    );
    // A class filter matches subclasses too (Light finds every light type).
    expect(support).toMatch(/Class = Class->GetSuperClass\(\)/);
    expect(support).toContain('Folder == TEXT("(none)")');
  });
});

describe('actor delete contracts', () => {
  const lifecycle = readFileSync(
    resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/ControlActor/McpAutomationBridge_ControlActorLifecycle.cpp'),
    'utf8',
  );

  it('wraps each delete call in one transaction, so a single undo restores every actor', () => {
    for (const title of ['"Delete Actors"', '"Delete Actors by Tag"']) {
      expect(lifecycle).toContain(`FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT(${title}))`);
    }
    expect(lifecycle.match(/Transaction\.DescribeInto\(/g)).toHaveLength(2);
    // The transaction opens before the first DestroyActor, not after.
    const byTag = lifecycle.slice(lifecycle.indexOf('HandleControlActorDeleteByTag'));
    expect(byTag.indexOf('FMcpScopedEditorTransaction')).toBeLessThan(byTag.indexOf('DestroyActor('));
  });
});
