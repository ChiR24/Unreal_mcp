// tests/eval/native-gate.test.ts
// The retrieval gate for the native gateway door (the plugin's HTTP /mcp search),
// measured through its TypeScript copy in native-ranker.ts. The copy is pinned to
// the C++ source first, so a weight or word list changed on one side fails here.
// ponytail: only constants are pinned; a change to the C++ scoring LOGIC must be
// copied into native-ranker.ts by hand.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { RETRIEVAL_FUNCTION_WORDS } from '../../src/tools/catalog/capabilities/retrieval/constants.js';
import { measureRetrieval } from './measure-retrieval.js';
import {
  NATIVE_ACTION_COVERED_BONUS, NATIVE_DELETE_INTENT_BONUS, NATIVE_READ_INTENT_BONUS, NATIVE_RULE_WEIGHTS,
  NATIVE_WORD_COVERAGE_BONUS, nativeSearchRanker,
} from './native-ranker.js';

const GATEWAY = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'MCP', 'Gateway');
const nativeMatch = readFileSync(join(GATEWAY, 'McpNativeGatewaySearchMatch.cpp'), 'utf8');
const nativeHeader = readFileSync(join(GATEWAY, 'McpNativeGatewaySearch.h'), 'utf8');

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
    expect(headerConstant('McpSearchReadIntentBonus')).toBe(NATIVE_READ_INTENT_BONUS);
    expect(headerConstant('McpSearchDeleteIntentBonus')).toBe(NATIVE_DELETE_INTENT_BONUS);
  });

  it('drops the same function words', () => {
    const start = nativeMatch.indexOf('FunctionWords[]');
    const block = nativeMatch.slice(start, nativeMatch.indexOf('};', start));
    const words = [...block.matchAll(/TEXT\("([a-z]+)"\)/gu)].map((match) => String(match[1])).sort();
    expect(words).toEqual([...RETRIEVAL_FUNCTION_WORDS].sort());
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
