// tests/eval/native-ranker.ts
// A TypeScript copy of the native gateway's search ranker
// (Private/MCP/Gateway/McpNativeGatewaySearchMatch.cpp, McpNativeGatewaySearch.cpp and Search/),
// so the eval gate measures search without an editor. Its word lists are read from
// the C++ source; native-gate.test.ts pins its weights to it, so a constant changed
// on one side only fails there.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { compareAscii } from '../../src/utils/serialization/ordering.js';
import { finalRegistryRecords } from './fixtures.js';
import type { GatewayRanker } from './measure-retrieval.js';

const GATEWAY = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'MCP', 'Gateway');
const NATIVE_MATCH_FILE = 'McpNativeGatewaySearchMatch.cpp';
const NATIVE_INTENT_FILE = join('Search', 'McpSearchIntent.cpp');

/** One `const TCHAR* const <name>[] = { TEXT("..."), ... };` array of the C++ ranker. */
function nativeWordList(file: string, name: string): ReadonlySet<string> {
  const source = readFileSync(join(GATEWAY, file), 'utf8');
  const start = source.indexOf(`${name}[]`);
  if (start < 0) throw new Error(`${name}[] is not in ${file}`);
  const block = source.slice(start, source.indexOf('};', start));
  return new Set([...block.matchAll(/TEXT\("([a-z]+)"\)/gu)].map((match) => String(match[1])));
}

const RETRIEVAL_FUNCTION_WORDS = nativeWordList(NATIVE_MATCH_FILE, 'FunctionWords');
const RETRIEVAL_READ_INTENT_WORDS = nativeWordList(NATIVE_INTENT_FILE, 'ReadIntentWords');
const RETRIEVAL_DELETE_INTENT_WORDS = nativeWordList(NATIVE_INTENT_FILE, 'DeleteIntentWords');

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

/** McpSearchLeadTied: the record's action, an alias or a topic holds the first content word and another one. */
function leadTied(record: NativeRecord, content: readonly string[]): boolean {
  const holds = (words: readonly string[]): boolean => words.includes(content[0] ?? '') && content.slice(1).some((word) => words.includes(word));
  return content.length < 2 || [record.id, ...record.aliases].some((name) => holds(searchWords(lastSegment(name))))
    || record.topics.some((topic) => holds(searchWords(topic)));
}

function scoreRecord(record: NativeRecord, query: string, all: readonly string[], content: readonly string[], leadIsAction: boolean): number | null {
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
  let leadNamed = false;
  for (const [index, word] of content.entries()) {
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
    if (index === 0) leadNamed = hits.some(([rule, hit]) => hit && (rule === 'id' || rule === 'family' || rule === 'domain'));
  }
  score += matched * NATIVE_WORD_COVERAGE_BONUS;
  const first = query.match(/[a-z0-9]+/u)?.[0] ?? '';
  const tieLead = (leadIsAction && !leadNamed) || (RETRIEVAL_DELETE_INTENT_WORDS.has(first) && record.effect !== 'destructive');
  if (content.length >= 2 && named === content.length && (!tieLead || leadTied(record, content))) score += NATIVE_FULL_COVERAGE_BONUS;
  const own = searchWords(lastSegment(record.id));
  const ownCovered = own.length >= 2 && own.every((word) => content.includes(word));
  const run = ` ${content.join(' ')} `;
  const aliasRun = record.aliases.some((alias) => {
    const words = searchWords(lastSegment(alias));
    return words.length >= 2 && run.includes(` ${words.join(' ')} `);
  });
  if (matched === content.length && (ownCovered || aliasRun)) score += NATIVE_ACTION_COVERED_BONUS;
  if (!fired) return null;
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

let wholeActions: ReadonlySet<string> | undefined;
/** McpSearchIsWholeAction's set: every word that is the whole action of an id or an alias ("move", "delete"). */
export function nativeWholeActions(): ReadonlySet<string> {
  return wholeActions ??= new Set(nativeRecords().flatMap((record) => [record.id, ...record.aliases])
    .map((name) => searchWords(lastSegment(name))).filter((words) => words.length === 1).map((words) => words[0] ?? ''));
}

/** The native door's typo correction (Private/MCP/Gateway/Search/McpSearchTypos.cpp), copied for the eval. */
export const NATIVE_TYPO_SWAP_LENGTH = 4;
export const NATIVE_TYPO_MIN_LENGTH = 5;
export const NATIVE_TYPO_LONG_LENGTH = 8;

const KEY_ROWS = ['qwertyuiop', 'asdfghjkl', 'zxcvbnm'] as const;
function keyPlace(ch: string): readonly [number, number] | undefined {
  for (const [row, keys] of KEY_ROWS.entries()) {
    const col = keys.indexOf(ch);
    if (col >= 0) return [row, col + 0.5 * row];
  }
  return undefined;
}
function neighbourKeys(a: string, b: string): boolean {
  const p = keyPlace(a);
  const q = keyPlace(b);
  return p !== undefined && q !== undefined && Math.abs(p[0] - q[0]) <= 1 && Math.abs(p[1] - q[1]) <= 1;
}

/** Optimal string alignment distance; a substitution between keys that are not neighbours costs 2. */
export function nativeEditDistance(a: string, b: string, limit: number): number {
  if (Math.abs(a.length - b.length) > limit) return limit + 1;
  const cell = (cells: readonly number[], index: number): number => cells[index] ?? 0;
  let older = new Array<number>(b.length + 1).fill(0);
  let prev = Array.from({ length: b.length + 1 }, (_, j) => j);
  let row = new Array<number>(b.length + 1).fill(0);
  for (let i = 1; i <= a.length; i += 1) {
    row[0] = i;
    let rowMin = i;
    for (let j = 1; j <= b.length; j += 1) {
      const substitute = a.charAt(i - 1) === b.charAt(j - 1) ? 0 : neighbourKeys(a.charAt(i - 1), b.charAt(j - 1)) ? 1 : 2;
      let value = Math.min(cell(prev, j) + 1, cell(row, j - 1) + 1, cell(prev, j - 1) + substitute);
      if (i > 1 && j > 1 && a.charAt(i - 1) === b.charAt(j - 2) && a.charAt(i - 2) === b.charAt(j - 1)) value = Math.min(value, cell(older, j - 2) + 1);
      row[j] = value;
      rowMin = Math.min(rowMin, value);
    }
    if (rowMin > limit) return limit + 1;
    [older, prev, row] = [prev, row, older];
  }
  return Math.min(cell(prev, b.length), limit + 1);
}

function isSwapOf(a: string, b: string): boolean {
  if (a.length !== b.length) return false;
  const [first, second, ...rest] = [...a].flatMap((ch, index) => (ch === b.charAt(index) ? [] : [index]));
  return first !== undefined && second !== undefined && rest.length === 0 &&
    a.charAt(first) === b.charAt(second) && a.charAt(second) === b.charAt(first);
}

let typoWords: { readonly known: ReadonlySet<string>; readonly names: ReadonlyMap<string, number> } | undefined;
function typoVocabulary(): { readonly known: ReadonlySet<string>; readonly names: ReadonlyMap<string, number> } {
  if (typoWords) return typoWords;
  const known = new Set<string>();
  const names = new Map<string, number>();
  for (const record of nativeRecords()) {
    const own = new Set<string>([
      ...searchWords(lastSegment(record.id)), ...record.aliases.flatMap((alias) => searchWords(lastSegment(alias))),
      ...record.topics.flatMap((topic) => searchWords(topic)), ...searchWords(record.family), ...searchWords(record.domain),
      ...searchWords(record.parent),
    ]);
    for (const word of own) {
      names.set(word, (names.get(word) ?? 0) + 1);
      known.add(word);
    }
    for (const word of searchWords(record.summary)) known.add(word);
  }
  return typoWords = { known, names };
}

/** The query with each word no record can match replaced by the nearest catalog name, as the native door does. */
export function nativeCorrectQuery(query: string): { readonly query: string; readonly corrected: readonly { from: string; to: string }[] } {
  const { known, names } = typoVocabulary();
  const corrected: { from: string; to: string }[] = [];
  const out = query.replace(/[a-z0-9]+/gu, (raw) => {
    const folded = searchWords(raw)[0] ?? '';
    if (RETRIEVAL_FUNCTION_WORDS.has(folded) || folded.length < NATIVE_TYPO_SWAP_LENGTH || /[0-9]/u.test(raw) || known.has(folded)) return raw;
    const limit = folded.length >= NATIVE_TYPO_LONG_LENGTH ? 2 : 1;
    let best = '';
    let bestDistance = limit + 1;
    let bestCount = 0;
    for (const [name, count] of names) {
      const distance = nativeEditDistance(folded, name, limit);
      if (distance > limit || (folded.length < NATIVE_TYPO_MIN_LENGTH && !isSwapOf(folded, name))) continue;
      if (distance < bestDistance || (distance === bestDistance && (count > bestCount || (count === bestCount && compareAscii(name, best) < 0)))) {
        best = name;
        bestDistance = distance;
        bestCount = count;
      }
    }
    if (best === '') return raw;
    corrected.push({ from: raw, to: best });
    return best;
  });
  return { query: out, corrected };
}

/** Every record the native door ranks for a query, best first, with its score. */
export function nativeScored(intent: string): readonly { readonly id: string; readonly score: number }[] {
  const query = nativeCorrectQuery(intent.trim().toLowerCase()).query;
  const all = searchWords(query);
  const content = [...new Set(all.filter((word) => !RETRIEVAL_FUNCTION_WORDS.has(word)))];
  const leadIsAction = content.length >= 2 && nativeWholeActions().has(content[0] ?? '');
  return nativeRecords()
    .map((record) => ({ id: record.id, score: scoreRecord(record, query, all, content, leadIsAction) }))
    .filter((entry): entry is { id: string; score: number } => entry.score !== null)
    .sort((left, right) => right.score - left.score || compareAscii(left.id, right.id));
}

export const nativeSearchRanker: GatewayRanker = (intent, limit) =>
  nativeScored(intent).slice(0, limit).map((entry) => entry.id);
