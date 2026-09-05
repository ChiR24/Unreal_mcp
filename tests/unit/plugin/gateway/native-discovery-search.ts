/// <reference types="node" />

// Task 25: normalized `search` reference. Deterministic filter, score, order,
// page and byte budget; mirrored exactly by McpNativeGatewaySearch.cpp and
// McpNativeGatewaySearchMatch.cpp.
//
// Matching is WORD-level, never substring: "move" must not hit `remove_*`, and
// a namespace word must not make every record under it look like a hit. A
// record's declared aliases are its own names, so a verb the action does not
// carry ("move actor" -> control_actor.move_actor) still lands. Function words
// are dropped from the query, and regular plurals/inflections fold on both the
// query and the record text, so "list actors" meets "actor".

import {
  allDomains,
  allFamilies,
  boundedLimit,
  boundedOffset,
  canonicalJson,
  closestMatches,
  guidedError,
  isAvailable,
  loadCanonicalRegistry,
  ordinalCompare,
  utf8Length,
  type DiscoveryInput,
  type DiscoveryRecord,
  type JsonValue,
} from './native-discovery-model.js';

export const SEARCH_DEFAULT_LIMIT = 12;
export const SEARCH_MAX_LIMIT = 25;
// Genuinely binding: the widest 25 results the catalog can produce total 10,263
// bytes, so a 16 KB cap could never fire. 8 KB bounds a full page of typical
// results and is exercised by real queries on both surfaces.
export const MAX_RESULT_BYTES = 24576;
/** Per matched query word, so a record covering more of the query outranks one covering less. */
export const WORD_COVERAGE_BONUS = 5;

// Ordered highest-signal first; `matchReasons` lists the rules that fired in
// this order regardless of which pass fired them.
const MATCH_RULES: readonly { readonly reason: string; readonly weight: number }[] = [
  { reason: 'id-exact', weight: 100 },
  { reason: 'id', weight: 50 },
  { reason: 'family', weight: 20 },
  { reason: 'domain', weight: 15 },
  { reason: 'topic', weight: 12 },
  { reason: 'summary', weight: 8 },
  { reason: 'parent', weight: 5 },
];
const RULE_ID_EXACT = 0;
const RULE_ID = 1;
const RULE_FAMILY = 2;
const RULE_DOMAIN = 3;
const RULE_TOPIC = 4;
const RULE_SUMMARY = 5;
const RULE_PARENT = 6;

/**
 * Closed-class English function words dropped from the QUERY (record text is
 * never filtered; a query word that survives cannot be one of these). Same set
 * as McpSearchIsFunctionWord; a grammatical category, never catalog vocabulary.
 */
export const SEARCH_FUNCTION_WORDS: ReadonlySet<string> = new Set([
  'a', 'an', 'the', 'this', 'that', 'these', 'those', 'of', 'in', 'on', 'at',
  'to', 'for', 'from', 'by', 'with', 'into', 'onto', 'and', 'or', 'but', 'it',
  'its', 'is', 'are', 'be', 'as', 'all', 'every', 'any', 'some', 'my', 'our',
  'their', 'his', 'her', 'you', 'me', 'we', 'i', 'please', 'then', 'so',
  'what', 'which', 'how', 'where', 'who', 'when', 'why', 'do', 'does', 'did',
  'can', 'could', 'should', 'would', 'will', 'want', 'need',
]);

const isAsciiAlnum = (ch: string): boolean => (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9');

/**
 * Regular plurals and the two regular verb inflections, first matching rule
 * wins (McpSearchFoldInflection). Deliberately not a stemmer: every rule is a
 * suffix rewrite so both surfaces reproduce it exactly.
 */
export const foldInflection = (word: string): string => {
  if (word.length > 4 && word.endsWith('ies')) return `${word.slice(0, -3)}y`;
  if (word.length > 4 && (word.endsWith('ses') || word.endsWith('xes') || word.endsWith('ches') || word.endsWith('shes'))) {
    return word.slice(0, -2);
  }
  if (word.length > 3 && word.endsWith('s') && !word.endsWith('ss')) return word.slice(0, -1);
  if (word.length > 5 && word.endsWith('ing')) return word.slice(0, -3);
  if (word.length > 4 && word.endsWith('ed')) return word.slice(0, -2);
  return word;
};

/** Lowercase ASCII alphanumeric runs of `text`, folded, in order (McpSearchWords). */
export const searchWords = (text: string): readonly string[] => {
  const out: string[] = [];
  let current = '';
  for (const ch of text.toLowerCase()) {
    if (isAsciiAlnum(ch)) {
      current += ch;
      continue;
    }
    if (current.length > 0) {
      out.push(foldInflection(current));
      current = '';
    }
  }
  if (current.length > 0) out.push(foldInflection(current));
  return out;
};

const containsWord = (text: string, word: string): boolean => searchWords(text).includes(word);
const actionSegment = (id: string): string => id.slice(id.lastIndexOf('.') + 1);
/** The folded action key of an id or alias: "blueprint.list_blueprint_variables" -> "list_blueprint_variable". */
const actionKey = (id: string): string => searchWords(actionSegment(id)).join('_');

const actionHasWord = (record: DiscoveryRecord, word: string): boolean =>
  containsWord(actionSegment(record.id), word)
  || record.aliases.some((alias) => containsWord(actionSegment(alias), word));

const actionEquals = (record: DiscoveryRecord, key: string): boolean =>
  key.length > 0 && (actionKey(record.id) === key || record.aliases.some((alias) => actionKey(alias) === key));

const anyTopicContainsPhrase = (topics: readonly string[], query: string): boolean =>
  topics.some((topic) => topic.toLowerCase().includes(query));

const anyTopicContainsWord = (topics: readonly string[], word: string): boolean =>
  topics.some((topic) => containsWord(topic, word));

/** Score one record: undefined when nothing fired (McpSearchScoreRecord). */
export const scoreRecord = (
  record: DiscoveryRecord,
  query: string,
  allWords: readonly string[],
  contentWords: readonly string[],
): { readonly score: number; readonly reasons: readonly string[] } | undefined => {
  const fired: boolean[] = MATCH_RULES.map(() => false);
  let score = 0;
  // Phrase pass: the whole query as an exact id, or as the exact action spelling
  // of the id or of a declared alias, with or without its function words.
  if (record.id.toLowerCase() === query
    || actionEquals(record, allWords.join('_'))
    || actionEquals(record, contentWords.join('_'))) {
    fired[RULE_ID_EXACT] = true;
    score += MATCH_RULES[RULE_ID_EXACT].weight;
  }
  // Phrase hits in prose only count for multi-word queries; a single word is
  // already scored by the word pass and must not count twice.
  if (contentWords.length >= 2) {
    if (anyTopicContainsPhrase(record.discovery.topics, query)) {
      fired[RULE_TOPIC] = true;
      score += MATCH_RULES[RULE_TOPIC].weight;
    }
    if (record.discovery.summary.toLowerCase().includes(query)) {
      fired[RULE_SUMMARY] = true;
      score += MATCH_RULES[RULE_SUMMARY].weight;
    }
  }
  // Word pass: whole-word hits per content word. The id rule reads the ACTION
  // segment and the declared aliases; namespace words reach a record only
  // through its domain and parent, at their own weights.
  let matched = 0;
  for (const word of contentWords) {
    const hits: boolean[] = MATCH_RULES.map(() => false);
    hits[RULE_ID] = actionHasWord(record, word);
    hits[RULE_FAMILY] = containsWord(record.discovery.family, word);
    hits[RULE_DOMAIN] = containsWord(record.discovery.domain, word);
    hits[RULE_TOPIC] = anyTopicContainsWord(record.discovery.topics, word);
    hits[RULE_SUMMARY] = containsWord(record.discovery.summary, word);
    hits[RULE_PARENT] = containsWord(record.routing.parentTool, word);
    let any = false;
    for (let rule = 0; rule < MATCH_RULES.length; rule += 1) {
      if (!hits[rule]) continue;
      any = true;
      fired[rule] = true;
      score += MATCH_RULES[rule].weight;
    }
    if (any) matched += 1;
  }
  score += matched * WORD_COVERAGE_BONUS;
  const reasons = MATCH_RULES.filter((_, index) => fired[index]).map((rule) => rule.reason);
  return reasons.length === 0 ? undefined : { score, reasons };
};

export const searchCapabilities = (input: DiscoveryInput): JsonValue => {
  const registry = loadCanonicalRegistry();
  const query = (input.query ?? '').trim().toLowerCase();
  const allWords = searchWords(query);
  // Function words dropped and duplicates collapsed (first occurrence wins), so a
  // repeated noun ("attach actor to another actor") is not scored twice.
  const contentWords = allWords.filter((word, index) => !SEARCH_FUNCTION_WORDS.has(word) && allWords.indexOf(word) === index);
  const limit = boundedLimit(input.limit, SEARCH_DEFAULT_LIMIT, SEARCH_MAX_LIMIT);
  const offset = boundedOffset(input.offset);
  // Mirrors FMcpDiscoveryQuery::MaxBytes: an explicit budget is clamped to 512..262144, else the default.
  const budget = typeof input.maxBytes === 'number' && input.maxBytes > 0 ? Math.min(262144, Math.max(512, Math.floor(input.maxBytes))) : MAX_RESULT_BYTES;

  if (input.domain !== undefined && !allDomains().includes(input.domain)) {
    return guidedError('search', 'UNKNOWN_DOMAIN', `Unknown domain '${input.domain}'.`, {
      nextCall: { operation: 'search' },
      suggestions: closestMatches(input.domain, allDomains()) as readonly JsonValue[],
    });
  }
  if (input.family !== undefined && !allFamilies().includes(input.family)) {
    return guidedError('search', 'UNKNOWN_FAMILY', `Unknown family '${input.family}'.`, {
      nextCall: { operation: 'search' },
      suggestions: closestMatches(input.family, allFamilies()) as readonly JsonValue[],
    });
  }

  const scored: { readonly record: DiscoveryRecord; readonly score: number; readonly reasons: readonly string[] }[] = [];
  for (const record of registry.records) {
    if (input.domain !== undefined && record.discovery.domain !== input.domain) continue;
    if (input.family !== undefined && record.discovery.family !== input.family) continue;
    if (query.length === 0) {
      scored.push({ record, score: 0, reasons: [] });
      continue;
    }
    const match = scoreRecord(record, query, allWords, contentWords);
    if (match === undefined) continue;
    scored.push({ record, score: match.score, reasons: match.reasons });
  }

  scored.sort((left, right) => right.score - left.score || ordinalCompare(left.record.id, right.record.id));

  const total = scored.length;
  const page = scored.slice(offset, offset + limit);
  const results: JsonValue[] = [];
  let bytes = 0;
  let truncated = false;
  for (const entry of page) {
    const view: JsonValue = {
      available: isAvailable(entry.record),
      capability: entry.record.id,
      domain: entry.record.discovery.domain,
      effect: entry.record.behavior.effect,
      family: entry.record.discovery.family,
      matchReasons: entry.reasons as readonly JsonValue[],
      // The public action (id's last segment), never the internal dispatch verb:
      // every manage_audio capability dispatches through 'manage_audio'.
      nextCall: {
        action: entry.record.id.slice(entry.record.id.lastIndexOf('.') + 1),
        operation: 'describe',
        tool: entry.record.routing.parentTool,
      },
      parent: entry.record.routing.parentTool,
      score: entry.score,
      summary: entry.record.discovery.summary,
    };
    const size = utf8Length(canonicalJson(view));
    if (bytes + size > budget && results.length > 0) {
      truncated = true;
      break;
    }
    bytes += size;
    results.push(view);
  }

  const hasMore = offset + results.length < total;
  const byteBudgetTruncated = truncated;
  truncated = byteBudgetTruncated || hasMore;
  const envelope: Record<string, JsonValue> = {
    catalogRevision: registry.catalogRevision,
    effectiveLimit: limit,
    hasMore,
    limit,
    message:
      'Results are capability-level and bounded. Call describe with the exact capability before execute.',
    offset,
    operation: 'search',
    query: input.query ?? '',
    results,
    success: true,
    total,
    truncated,
    truncationReason: byteBudgetTruncated ? 'byte-budget' : hasMore ? 'limit' : 'none',
  };
  // An empty page is where a caller starts inventing names: say what to change
  // and hand over the one call that always works.
  if (results.length === 0 && total === 0) {
    envelope.message = query.length === 0
      ? 'No capability matches these filters. Remove a filter, or call describe with no selector to browse.'
      : `No capability matched '${query}'. Use 2-4 plain words naming the verb and the object (e.g. 'spawn actor'), drop any filter, or call describe with no selector to browse.`;
    envelope.nextCall = { operation: 'describe' };
  }
  if (input.domain !== undefined) envelope.domain = input.domain;
  if (input.family !== undefined) envelope.family = input.family;
  if (hasMore) envelope.nextCursor = String(offset + results.length);
  return envelope;
};
