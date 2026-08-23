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

// Every Interaction file that mutates a Blueprint asset. RuntimeActors and
// RuntimeComponents are excluded on purpose: they operate on world actors, hold
// no UBlueprint and never save, so they need ACTOR identity rather than this
// asset helper. Info.cpp is a read. Both belong to Todo 18 (BB-007/008/009).
const ASSET_MUTATING = [
  'Chest', 'Components', 'Destruction', 'Door', 'Interface',
  'Lever', 'Switch', 'Triggers', 'WidgetEvents'
] as const;

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

describe('todo16 BB-008: every asset-mutating Interaction handler stamps evidence', () => {
  it.each(ASSET_MUTATING)('%s routes through the shared evidence helper', (name) => {
    const source = interaction(name);

    expect(source).toContain('McpAutomationBridgeHelpersMutationEvidence.h');
    expect(source).toMatch(/AddMutationEvidence\(/u);
  });

  it.each(ASSET_MUTATING)('%s captures every save result, on any line', (name) => {
    const source = interaction(name);
    const saves = [...source.matchAll(/McpSafeAssetSave\(/gu)];

    expect(saves.length).toBeGreaterThanOrEqual(1);
    // Not line-start anchored: a fire-and-forget save appended to a shared line
    // is still a fire-and-forget save.
    for (const match of saves) {
      const before = source.slice(Math.max(0, (match.index ?? 0) - 60), match.index);
      expect(before, `${name}: every save must bind its result`).toMatch(/const bool b\w*Saved = $/u);
    }
  });

  it.each(ASSET_MUTATING)('%s gates EVERY saved entry, not just one of them', (name) => {
    const source = interaction(name);
    const saved = [...source.matchAll(/\w*Changes\.Add\(TEXT\("saved"\)\)/gu)];

    expect(saved.length).toBeGreaterThanOrEqual(1);
    // Per call site, not per file: in a two-path file a single surviving gate
    // must not license an ungated sibling.
    for (const match of saved) {
      const before = source.slice(Math.max(0, (match.index ?? 0) - 40), match.index);
      const gate = /if \((b\w*Saved)\)\s*\{\s*$/u.exec(before);
      expect(gate, `${name}: each "saved" must sit behind its own flag`).not.toBeNull();
      // The flag has to be the save's own result. Matching only its NAME let a
      // hand-written `const bool bAlwaysSaved = true;` claim a save that the
      // handler never performed.
      expect(source, `${name}: the gate flag must be bound by a real save`)
        .toMatch(new RegExp(`const bool ${gate?.[1] ?? 'bSaved'} = McpSafeAssetSave\\(`, 'u'));
    }
  });

  it.each(ASSET_MUTATING)('%s never stamps evidence against a null asset', (name) => {
    const source = interaction(name);
    const calls = [...source.matchAll(/AddMutationEvidence\(\s*(\w+)\s*,\s*(\w+)\s*,/gu)];
    const total = (source.match(/AddMutationEvidence\(/gu) ?? []).length;

    expect(calls.length).toBeGreaterThanOrEqual(1);
    // Counts must agree, or a call whose asset argument is not a bare
    // identifier (a cast, a member, a call) is skipped by the pattern above
    // and inspected by nothing at all.
    expect(calls.length, `${name}: every evidence call must expose an inspectable asset argument`).toBe(total);
    // A null asset makes AddAssetVerification early-return, producing exactly
    // the evidence-free receipt BB-008 is about.
    for (const match of calls) {
      expect(match[2], `${name}: evidence needs a real asset`).not.toBe('nullptr');
    }
  });

  it('create_door_actor keeps its pre-existing verification call', () => {
    expect(interaction('Door')).toContain('McpHandlerUtils::AddVerification(Result, DoorBP)');
  });

  it('files with two mutation paths carry two evidence call sites', () => {
    for (const name of ['Chest', 'Door', 'Switch', 'Triggers', 'WidgetEvents', 'Components']) {
      const calls = (interaction(name).match(/AddMutationEvidence\(/gu) ?? []).length;
      expect(calls, `${name} should stamp both of its mutation paths`).toBeGreaterThanOrEqual(2);
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
