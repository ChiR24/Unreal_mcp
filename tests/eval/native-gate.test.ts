// tests/eval/native-gate.test.ts
// The retrieval gate for the native gateway door (the plugin's HTTP /mcp search),
// measured through its TypeScript copy in native-ranker.ts. The copy reads its word
// lists from the C++ source and is pinned to its weights first, so a weight changed
// on one side fails here.
// ponytail: only constants are pinned; a change to the C++ scoring LOGIC must be
// copied into native-ranker.ts by hand.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { measureRetrieval } from './measure-retrieval.js';
import {
  NATIVE_ACTION_COVERED_BONUS, NATIVE_DELETE_INTENT_BONUS, NATIVE_FULL_COVERAGE_BONUS, NATIVE_PHRASE_TOPIC_BONUS, NATIVE_READ_INTENT_BONUS,
  NATIVE_RULE_WEIGHTS, NATIVE_TYPO_LONG_LENGTH, NATIVE_TYPO_MIN_LENGTH, NATIVE_TYPO_SWAP_LENGTH, NATIVE_WORD_COVERAGE_BONUS,
  nativeCorrectQuery, nativeEditDistance, nativeSearchRanker, nativeWholeActions,
} from './native-ranker.js';

const GATEWAY = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'MCP', 'Gateway');
const nativeMatch = readFileSync(join(GATEWAY, 'McpNativeGatewaySearchMatch.cpp'), 'utf8');
const nativeHeader = readFileSync(join(GATEWAY, 'McpNativeGatewaySearch.h'), 'utf8');
const nativeTypos = readFileSync(join(GATEWAY, 'Search', 'McpSearchTypos.cpp'), 'utf8');
const nativeIntent = readFileSync(join(GATEWAY, 'Search', 'McpSearchIntent.cpp'), 'utf8');

const headerConstant = (name: string): number => Number(nativeHeader.match(new RegExp(`constexpr int32 ${name} = (\\d+);`, 'u'))?.[1]);

describe('the TypeScript copy matches the native source', () => {
  it('has the same rule weights', () => {
    const start = nativeMatch.indexOf('MatchRules[RuleCount]');
    const block = nativeMatch.slice(start, nativeMatch.indexOf('};', start));
    const weights = Object.fromEntries([...block.matchAll(/TEXT\("([a-z-]+)"\), (\d+)/gu)].map((match) => [match[1], Number(match[2])]));
    expect(weights).toEqual(NATIVE_RULE_WEIGHTS);
  });

  it('has the same bonuses', () => {
    expect(headerConstant('McpSearchWordCoverageBonus')).toBe(NATIVE_WORD_COVERAGE_BONUS);
    expect(headerConstant('McpSearchActionCoveredBonus')).toBe(NATIVE_ACTION_COVERED_BONUS);
    expect(headerConstant('McpSearchFullCoverageBonus')).toBe(NATIVE_FULL_COVERAGE_BONUS);
    expect(headerConstant('McpSearchPhraseTopicBonus')).toBe(NATIVE_PHRASE_TOPIC_BONUS);
    expect(nativeMatch).toContain('Score += MatchRules[RuleTopic].Weight + McpSearchPhraseTopicBonus;');
    expect(headerConstant('McpSearchReadIntentBonus')).toBe(NATIVE_READ_INTENT_BONUS);
    expect(headerConstant('McpSearchDeleteIntentBonus')).toBe(NATIVE_DELETE_INTENT_BONUS);
  });

  it('corrects typos within the same limits', () => {
    const typoConstant = (name: string): number => Number(nativeTypos.match(new RegExp(`constexpr int32 ${name} = (\\d+);`, 'u'))?.[1]);
    expect(typoConstant('McpTypoSwapLength')).toBe(NATIVE_TYPO_SWAP_LENGTH);
    expect(typoConstant('McpTypoMinLength')).toBe(NATIVE_TYPO_MIN_LENGTH);
    expect(typoConstant('McpTypoLongLength')).toBe(NATIVE_TYPO_LONG_LENGTH);
    expect(nativeTypos).toContain('AreNeighbourKeys(A[I - 1], B[J - 1]) ? 1 : 2');
  });

  it('ties a lead verb to its object under the same conditions', () => {
    expect(nativeMatch).toContain('(!bTieLead || McpSearchLeadTied(Record, ContentWords))');
    expect(nativeMatch).toContain('const bool bTieLead = (bLeadIsAction && !bLeadNamed) ||');
    expect(nativeIntent).toContain('if (Words.Num() == 1)');
  });
});

// A verb that is some record's whole action leads the request; a record that names it only in a topic, or does not
// delete and is asked to, covers the request only where one of its names or topics holds the verb and the object
// (EngineWire.Gateway.SearchIntent holds the C++ to the same).
describe('lead verbs', () => {
  it('reads these words as whole actions: a single-word alias joins the list and moves rankings', () => {
    expect([...nativeWholeActions()].sort()).toEqual([
      'activate', 'attach', 'bend', 'bevel', 'blur', 'bridge', 'chamfer', 'cleanup', 'compile', 'create', 'cylindrify',
      'deactivate', 'delete', 'desaturate', 'detach', 'difference', 'duplicate', 'eject', 'exist', 'explain', 'export',
      'extrude', 'find', 'get', 'import', 'inset', 'invert', 'list', 'load', 'loft', 'mirror', 'morphology', 'move',
      'niagara', 'open', 'outset', 'particle', 'pause', 'play', 'playtest', 'poke', 'possess', 'profile', 'quadrangulate',
      'redo', 'reimport', 'relax', 'rename', 'reset', 'resume', 'revolve', 'save', 'screenshot', 'sculpt', 'set', 'sharpen',
      'shell', 'smooth', 'spawn', 'spherify', 'stop', 'stream', 'stretch', 'subdivide', 'subscribe', 'sweep', 'taper',
      'twist', 'undo', 'unload', 'unsubscribe', 'validate',
    ]);
  });

  it('keeps a verb named in passing from covering another object', () => {
    expect(nativeSearchRanker('move a vertex', 1)).toEqual(['manage_geometry.edit_dynamic_mesh']);
    expect(nativeSearchRanker('delete triangles', 1)).toEqual(['manage_geometry.edit_dynamic_mesh']);
    expect(nativeSearchRanker('delete the crate', 3)).not.toContain('manage_geometry.edit_dynamic_mesh');
  });
});

// The known answers EngineWire.Gateway.SearchTypos holds the C++ to, on the copy.
describe('typo correction', () => {
  it('measures edits as the native door does', () => {
    expect(nativeEditDistance('spwan', 'spawn', 1)).toBe(1);
    expect(nativeEditDistance('delte', 'delete', 1)).toBe(1);
    expect(nativeEditDistance('lighr', 'light', 1)).toBe(1);
    expect(nativeEditDistance('carve', 'curve', 2)).toBe(2);
    expect(nativeEditDistance('abcdef', 'uvwxyz', 1)).toBe(2);
  });

  it('corrects a typo, a four-letter swap, and leaves correct and real words alone', () => {
    expect(nativeCorrectQuery('spwan actor')).toEqual({ query: 'spawn actor', corrected: [{ from: 'spwan', to: 'spawn' }] });
    expect(nativeCorrectQuery('lsit actors').query).toBe('list actors');
    expect(nativeCorrectQuery('spawn actor').corrected).toEqual([]);
    expect(nativeCorrectQuery('carve valleys').corrected).toEqual([]);
  });
});

describe('native retrieval', () => {
  const measured = measureRetrieval(nativeSearchRanker);

  it('top-K recall is at least 98%', () => {
    expect(measured.topKRecall, measured.misses.join('\n')).toBeGreaterThanOrEqual(0.98);
  });

  it('top-1 accuracy is at least 90%', () => {
    expect(measured.top1Accuracy, measured.misses.join('\n')).toBeGreaterThanOrEqual(0.9);
  });
});
