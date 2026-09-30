// tests/eval/native-ranker.ts
// A TypeScript copy of the native gateway's search ranker
// (Private/MCP/Gateway/McpNativeGatewaySearchMatch.cpp and McpNativeGatewaySearch.cpp),
// so the eval gate measures the door an HTTP client uses as well as the stdio one.
// native-gate.test.ts pins its weights and word lists to the C++ source: a constant
// changed on one side only fails there.

import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { RETRIEVAL_DELETE_INTENT_WORDS, RETRIEVAL_FUNCTION_WORDS, RETRIEVAL_READ_INTENT_WORDS } from '../../src/tools/catalog/capabilities/retrieval/constants.js';
import { compareAscii } from '../../src/utils/serialization/ordering.js';
import { finalRegistryRecords } from './fixtures.js';
import type { GatewayRanker } from './measure-retrieval.js';

export const NATIVE_RULE_WEIGHTS = {
  'id-exact': 100, id: 50, family: 20, domain: 15, topic: 12, summary: 8, parent: 5,
} as const;
export const NATIVE_WORD_COVERAGE_BONUS = 5;
export const NATIVE_ACTION_COVERED_BONUS = 50;
export const NATIVE_FULL_COVERAGE_BONUS = 100;
export const NATIVE_PHRASE_TOPIC_BONUS = 50;
export const NATIVE_READ_INTENT_BONUS = 40;
export const NATIVE_DELETE_INTENT_BONUS = 40;

type NativeRecord = {
  readonly id: string;
  readonly aliases: readonly string[];
  readonly family: string;
  readonly domain: string;
  readonly topics: readonly string[];
  readonly summary: string;
  readonly parent: string;
  readonly effect: string;
};

/** Suffix rewrites only (FoldInflection), then the delete synonyms (FoldSynonym). A function word stays whole. */
function foldWord(word: string): string {
  if (RETRIEVAL_FUNCTION_WORDS.has(word)) return word;
  const length = word.length;
  let folded = word;
  if (length > 4 && folded.endsWith('ies')) folded = `${folded.slice(0, -3)}y`;
  else if (length > 4 && ['ses', 'xes', 'ches', 'shes'].some((suffix) => folded.endsWith(suffix))) folded = folded.slice(0, -2);
  else if (length > 3 && folded.endsWith('s') && !folded.endsWith('ss')) folded = folded.slice(0, -1);
  else if (length > 5 && folded.endsWith('ing')) folded = folded.slice(0, -3);
  else if (length > 4 && folded.endsWith('ed')) folded = folded.slice(0, -2);
  return folded === 'remove' || folded === 'destroy' || folded === 'erase' ? 'delete' : folded;
}

// Record text repeats across every query of a probe; re-tokenizing it per query word took the
// 364-case plain probe past vitest's 10 s budget.
const wordCache = new Map<string, string[]>();
const searchWords = (text: string): string[] => {
  let words = wordCache.get(text);
  if (words === undefined) {
    words = (text.toLowerCase().match(/[a-z0-9]+/gu) ?? []).map(foldWord);
    wordCache.set(text, words);
  }
  return words;
};
const lastSegment = (id: string): string => id.slice(id.lastIndexOf('.') + 1);
const containsWord = (text: string, word: string): boolean => searchWords(text).includes(word);
const actionKey = (id: string): string => searchWords(lastSegment(id)).join('_');

function actionHasWord(record: NativeRecord, word: string): boolean {
  return containsWord(lastSegment(record.id), word) || record.aliases.some((alias) => containsWord(lastSegment(alias), word));
}

function actionEquals(record: NativeRecord, key: string): boolean {
  return key !== '' && (actionKey(record.id) === key || record.aliases.some((alias) => actionKey(alias) === key));
}

function scoreRecord(record: NativeRecord, query: string, all: readonly string[], content: readonly string[]): number | null {
  let score = 0;
  let fired = false;
  const fire = (rule: keyof typeof NATIVE_RULE_WEIGHTS): void => { score += NATIVE_RULE_WEIGHTS[rule]; fired = true; };
  if (record.id.toLowerCase() === query || actionEquals(record, all.join('_')) || actionEquals(record, content.join('_'))) fire('id-exact');
  if (all.length >= 2) {
    if (record.topics.some((topic) => topic.toLowerCase().includes(query))) {
      fire('topic');
      score += NATIVE_PHRASE_TOPIC_BONUS;
    }
    if (record.summary.toLowerCase().includes(query)) fire('summary');
  }
  let matched = 0;
  let named = 0;
  for (const word of content) {
    const hits: [keyof typeof NATIVE_RULE_WEIGHTS, boolean][] = [
      ['id', actionHasWord(record, word)],
      ['family', containsWord(record.family, word)],
      ['domain', containsWord(record.domain, word)],
      ['topic', record.topics.some((topic) => containsWord(topic, word))],
      ['summary', containsWord(record.summary, word)],
      ['parent', containsWord(record.parent, word)],
    ];
    let any = false;
    for (const [rule, hit] of hits) {
      if (!hit) continue;
      any = true;
      fire(rule);
    }
    if (any) matched += 1;
    if (hits.some(([rule, hit]) => hit && rule !== 'summary' && rule !== 'parent')) named += 1;
  }
  score += matched * NATIVE_WORD_COVERAGE_BONUS;
  if (content.length >= 2 && named === content.length) score += NATIVE_FULL_COVERAGE_BONUS;
  const own = searchWords(lastSegment(record.id));
  const ownCovered = own.length >= 2 && own.every((word) => content.includes(word));
  const run = ` ${content.join(' ')} `;
  const aliasRun = record.aliases.some((alias) => {
    const words = searchWords(lastSegment(alias));
    return words.length >= 2 && run.includes(` ${words.join(' ')} `);
  });
  if (matched === content.length && (ownCovered || aliasRun)) score += NATIVE_ACTION_COVERED_BONUS;
  if (!fired) return null;
  const first = query.match(/[a-z0-9]+/u)?.[0] ?? '';
  if (record.effect === 'read' && RETRIEVAL_READ_INTENT_WORDS.has(first)) score += NATIVE_READ_INTENT_BONUS;
  else if (record.effect === 'destructive' && RETRIEVAL_DELETE_INTENT_WORDS.has(first)) score += NATIVE_DELETE_INTENT_BONUS;
  return score;
}

let records: readonly NativeRecord[] | undefined;
function nativeRecords(): readonly NativeRecord[] {
  return records ??= finalRegistryRecords().map((record: CapabilityRecord) => ({
    id: String(record.id),
    aliases: record.aliases.map(String),
    family: record.discovery.family,
    domain: record.discovery.domain,
    topics: record.discovery.topics,
    summary: record.discovery.summary,
    parent: record.routing.parentTool,
    effect: record.behavior.effect,
  }));
}

export const nativeSearchRanker: GatewayRanker = (intent, limit) => {
  const query = intent.trim().toLowerCase();
  const all = searchWords(query);
  const content = [...new Set(all.filter((word) => !RETRIEVAL_FUNCTION_WORDS.has(word)))];
  return nativeRecords()
    .map((record) => ({ id: record.id, score: scoreRecord(record, query, all, content) }))
    .filter((entry): entry is { id: string; score: number } => entry.score !== null)
    .sort((left, right) => right.score - left.score || compareAscii(left.id, right.id))
    .slice(0, limit)
    .map((entry) => entry.id);
};
