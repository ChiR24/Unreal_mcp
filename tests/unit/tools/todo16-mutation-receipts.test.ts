// Plan Todo 16 (BB-008, BB-045) - a mutation receipt must carry canonical
// identity and concrete changes when something actually changed, and must stay
// truthfully empty when nothing did.
//
// Written after the fixes landed, so non-vacuity is proven by mutation: toggle
// any one fix off and the case naming it fails. The native cases are
// source-contract reads because no engine root exists here to compile the
// plugin.
//
// An independent verifier defeated the first version of these guards three
// ways: comment text satisfied the call-site and compile-gating assertions,
// bare substring containment let the bounds constants be widened 100x, and the
// trim/truncation had no assertion at all. Source is therefore comment-stripped
// before every structural assertion, constants are anchored through their
// terminating semicolon, and each bound is pinned to the expression applying it.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

import { extractChanges, extractHandles } from '../../../src/tools/catalog/capabilities/semantic/receipt-outcome.js';

const PRIVATE = join(
  'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private'
);

/** Block and line comments removed, so no assertion can be satisfied by prose. */
function stripComments(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
}

function nativeSource(...segments: readonly string[]): string {
  return stripComments(readFileSync(join(PRIVATE, ...segments), 'utf8'));
}

function interaction(name: string): string {
  return nativeSource('Domains', 'Interaction', `McpAutomationBridge_InteractionHandlers${name}.cpp`);
}

const compile = (): string =>
  nativeSource('Domains', 'Blueprint', 'Queries', 'McpAutomationBridge_BlueprintHandlersCompile.cpp');
const helper = (): string =>
  nativeSource('Foundation', 'BridgeHelpers', 'Responses', 'McpAutomationBridgeHelpersMutationEvidence.h');

describe('todo16 BB-045: a widget mutation yields an asset handle', () => {
  it('widgetPath is recognised as canonical asset identity', () => {
    const handles = extractHandles({ success: true, widgetPath: '/Game/UI/WBP_HUD' });

    expect(handles).toContainEqual({ kind: 'asset', path: '/Game/UI/WBP_HUD' });
  });

  it('an explicit assetPath still wins over widgetPath', () => {
    const handles = extractHandles({
      success: true,
      assetPath: '/Game/Canonical',
      widgetPath: '/Game/UI/WBP_HUD'
    });

    expect(handles).toContainEqual({ kind: 'asset', path: '/Game/Canonical' });
    expect(handles).not.toContainEqual({ kind: 'asset', path: '/Game/UI/WBP_HUD' });
  });
});

describe('todo16: identity and changes reach the receipt, and nothing is invented', () => {
  it('assetPath plus changedEntities produce handles and changes', () => {
    const result = {
      success: true,
      assetPath: '/Game/ULW_MCP_QA/ChestBP',
      changedEntities: ['created chest blueprint', 'saved']
    };

    expect(extractHandles(result)).toContainEqual({ kind: 'asset', path: '/Game/ULW_MCP_QA/ChestBP' });
    // extractChanges also derives one entry from CHANGE_SINGLE_FIELDS, so the
    // assetPath appears alongside the two the handler actually emitted.
    expect(extractChanges(result)).toEqual(
      expect.arrayContaining(['created chest blueprint', 'saved'])
    );
    expect(extractChanges(result)).toHaveLength(3);
  });

  it('a result carrying no evidence yields empty arrays, never fabricated ones', () => {
    expect(extractHandles({ success: true })).toEqual([]);
    expect(extractChanges({ success: true })).toEqual([]);
  });
});

// Every Interaction Blueprint handler finishes through SendInteractableResult (BlueprintVariables.cpp), which does the
// save, the "saved" gate, the verification and the evidence stamp for all of them; Interface still finishes inline.
const SHARED_FINISH = ['Chest', 'Components', 'Door', 'Lever', 'Switch', 'Triggers'] as const;
const finisher = (): string => {
  const source = interaction('BlueprintVariables');
  const start = source.indexOf('void SendInteractableResult(');
  return source.slice(start, source.indexOf('\n}', start));
};

describe('todo16 BB-008: every asset-mutating Interaction handler stamps evidence', () => {
  it.each(SHARED_FINISH)('%s finishes every mutation through SendInteractableResult and never saves on its own', (name) => {
    const source = interaction(name);

    expect(source).toMatch(/SendInteractableResult\(/u);
    expect(source, `${name}: a save outside the shared finisher would skip its evidence`).not.toMatch(/McpSafeAssetSave\(/u);
    expect(source, `${name}: evidence is stamped by the finisher`).not.toMatch(/AddMutationEvidence\(/u);
  });

  it('the shared finisher saves, gates "saved" on that save, verifies and stamps evidence on the saved asset', () => {
    const source = finisher();

    expect(interaction('BlueprintVariables')).toContain('McpAutomationBridgeHelpersMutationEvidence.h');
    expect(source).toMatch(/if \(McpSafeAssetSave\(Blueprint\)\)\s*\{\s*Changes\.Add\(TEXT\("saved"\)\);/u);
    expect((source.match(/Changes\.Add\(TEXT\("saved"\)\)/gu) ?? []).length, '"saved" is added only behind the save').toBe(1);
    expect(source).toContain('McpHandlerUtils::AddVerification(Result, Blueprint)');
    expect(source).toContain('AddMutationEvidence(Result, Blueprint, Changes)');
  });

  it('Interface binds its save result and gates "saved" on it', () => {
    const source = interaction('Interface');

    expect(source).toContain('McpAutomationBridgeHelpersMutationEvidence.h');
    expect(source).toMatch(/const bool bInterfaceSaved = McpSafeAssetSave\(InterfaceBP\);/u);
    expect(source).toMatch(/if \(bInterfaceSaved\)\s*\{\s*InterfaceChanges\.Add\(TEXT\("saved"\)\);/u);
    expect(source).toContain('AddMutationEvidence(Result, InterfaceBP, InterfaceChanges)');
  });

  it('files with two mutation paths finish both through the shared finisher', () => {
    for (const name of ['Chest', 'Door', 'Switch', 'Components']) {
      const calls = (interaction(name).match(/SendInteractableResult\(/gu) ?? []).length;
      expect(calls, `${name} should finish both of its mutation paths`).toBeGreaterThanOrEqual(2);
    }
  });
});

describe('todo16 BB-045: compile evidence is derived from actual state', () => {
  it('compiled and saved are each gated on their own observed flag', () => {
    const source = compile();

    expect(source).toContain('McpAutomationBridgeHelpersMutationEvidence.h');
    expect(source).toMatch(/if \(bCompiled\)\s*\{\s*CompileChanges\.Add\(TEXT\("compiled"\)\);/u);
    expect(source).toMatch(/if \(bSaved\)\s*\{\s*CompileChanges\.Add\(TEXT\("saved"\)\);/u);
    expect(source).toContain('AddMutationEvidence(Out, BP, CompileChanges)');
  });

  it('nothing is added unconditionally, so a failed compile reports no change', () => {
    const source = compile();
    // Every Add must sit immediately behind its own `if (bFlag) {` guard; a bare
    // or brace-wrapped unconditional Add is rejected.
    const adds = [...source.matchAll(/CompileChanges\.Add\(/gu)];
    expect(adds.length).toBeGreaterThanOrEqual(2);
    // Exactly the two states the handler observes. Requiring only "behind some
    // flag" let an invented third entry ride in behind `if (bAlways)`.
    const literals = [...source.matchAll(/CompileChanges\.Add\(TEXT\("(\w+)"\)\)/gu)].map((m) => m[1]).sort();
    expect(literals, 'compile evidence is exactly compiled + saved').toEqual(['compiled', 'saved']);
    for (const match of adds) {
      const before = source.slice(Math.max(0, (match.index ?? 0) - 40), match.index);
      expect(before, 'each CompileChanges.Add must sit behind its own flag').toMatch(/if \(b\w+\)\s*\{\s*$/u);
    }
  });
});

describe('todo16: the shared helper bounds what a handler can push', () => {
  it('pins the exact cap values, not just their prefix', () => {
    const source = helper();

    // Anchored through the terminator: `= 200` no longer satisfies `= 20`.
    expect(source).toMatch(/McpMaxChangedEntities\s*=\s*20\s*;/u);
    expect(source).toMatch(/McpMaxChangedEntityChars\s*=\s*120\s*;/u);
  });

  it('actually applies the count cap, the length cap, the trim and the dedup', () => {
    const source = helper();

    // Anchored on the closing paren, so `>= McpMaxChangedEntities * 100` fails.
    expect(source).toMatch(/Bounded\.Num\(\) >= McpMaxChangedEntities\s*\)/u);
    // Anchored on the statement that binds the value, so a dead TEXT("...")
    // string mentioning the call cannot stand in for applying it.
    expect(source).toMatch(/const FString Trimmed = Entry\.TrimStartAndEnd\(\);/u);
    // Pinned to the `Capped` binding, which is the value Seen/Bounded below
    // actually consume, so the truncation cannot survive on a dead local while
    // the live path passes the untruncated string through.
    expect(source).toMatch(
      /const FString Capped = Trimmed\.Len\(\) > McpMaxChangedEntityChars\s*\?\s*Trimmed\.Left\(McpMaxChangedEntityChars\)/u
    );
    expect(source).toContain('Seen.Add(Capped, &bAlreadySeen)');
  });

  it('omits the field entirely when there is nothing truthful to report', () => {
    const source = helper();

    expect(source).toMatch(/if \(Bounded\.Num\(\) > 0\)/u);
    expect(source).toContain('AddAssetVerification(Response, Asset)');
  });
});
