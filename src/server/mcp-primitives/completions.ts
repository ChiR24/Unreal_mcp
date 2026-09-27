// src/server/mcp-primitives/completions.ts
// completion/complete over a closed set of slots: capability ids for
// ue://capability/{capabilityId}, knowledge topics, content roots for
// ue://asset/{assetPath}, and the enum-valued workflow prompt arguments.
// Every pool is in-memory; nothing scans the editor or the host filesystem.

import { knowledgeTopics } from '../../resources/knowledge-resources.js';
import { dynamicToolManager } from '../../tools/dynamic/dynamic-tool-manager.js';
import { HOST_PATH_PATTERN, UE_CONTENT_ROOTS, isTraversalPath } from '../../utils/paths/content-path-policy.js';
import { capabilityIndex } from '../gateway/gateway-capability-index.js';
import { WORKFLOW_PROMPTS } from './prompts.js';

const MAX_ITEMS = 100;
const MAX_BYTES = 8192;
const MAX_PREFIX = 128;

export interface CompletionResult {
  readonly values: string[];
  readonly total: number;
  readonly hasMore: boolean;
}

const EMPTY: CompletionResult = { values: [], total: 0, hasMore: false };

/** Capability ids plus their alias and legacy tool.action names, for enabled tools only. */
function capabilityCandidates(): string[] {
  const values = new Set<string>();
  for (const record of capabilityIndex().records) {
    if (!dynamicToolManager.isToolEnabled(record.routing.parentTool)) continue;
    values.add(String(record.id));
    for (const alias of record.aliases) values.add(String(alias));
    for (const legacy of record.legacyIds) values.add(`${String(legacy.tool)}.${String(legacy.action)}`);
  }
  return [...values];
}

function candidatesFor(ref: { type: string; name?: string; uri?: string }, argument: string): readonly string[] | undefined {
  if (ref.type === 'ref/prompt') {
    return WORKFLOW_PROMPTS.find((prompt) => prompt.id === ref.name)?.arguments.find((spec) => spec.name === argument)?.allowed;
  }
  if (ref.uri === 'ue://capability/{capabilityId}' && argument === 'capabilityId') return capabilityCandidates();
  if (ref.uri === 'ue://knowledge/{topic}' && argument === 'topic') return knowledgeTopics();
  if (ref.uri === 'ue://asset/{assetPath}' && argument === 'assetPath') return [...UE_CONTENT_ROOTS];
  return undefined;
}

function withinOneEdit(a: string, b: string): boolean {
  if (Math.abs(a.length - b.length) > 1) return false;
  let i = 0;
  let j = 0;
  let edits = 0;
  while (i < a.length && j < b.length) {
    if (a[i] === b[j]) { i += 1; j += 1; continue; }
    edits += 1;
    if (edits > 1) return false;
    if (a.length > b.length) i += 1;
    else if (b.length > a.length) j += 1;
    else { i += 1; j += 1; }
  }
  return edits + (i < a.length || j < b.length ? 1 : 0) <= 1;
}

function isSubsequence(needle: string, haystack: string): boolean {
  let n = 0;
  for (let h = 0; h < haystack.length && n < needle.length; h += 1) if (haystack[h] === needle[n]) n += 1;
  return n === needle.length;
}

/** 0 prefix, 1 substring, 2 subsequence, 3 one typo, undefined no match. */
function tierFor(value: string, prefix: string): number | undefined {
  if (prefix.length === 0 || value.startsWith(prefix)) return 0;
  if (value.includes(prefix)) return 1;
  if (isSubsequence(prefix, value)) return 2;
  if (withinOneEdit(prefix, value.slice(0, prefix.length)) || withinOneEdit(prefix, value.slice(0, prefix.length + 1))) return 3;
  return undefined;
}

/** Rank the pool against the prefix (typo matches only when nothing better matched) and cap it. */
export function complete(ref: { type: string; name?: string; uri?: string }, argument: string, prefix: string): CompletionResult {
  if (prefix.length > MAX_PREFIX || HOST_PATH_PATTERN.test(prefix) || isTraversalPath(prefix)) return EMPTY;
  const pool = candidatesFor(ref, argument);
  if (pool === undefined) return EMPTY;

  const lowered = prefix.toLowerCase();
  const scored = pool.flatMap((value) => {
    const tier = tierFor(value.toLowerCase(), lowered);
    return tier === undefined ? [] : [{ value, tier }];
  });
  const matched = scored.some((entry) => entry.tier < 3) ? scored.filter((entry) => entry.tier < 3) : scored;
  matched.sort((a, b) => a.tier - b.tier || (a.value < b.value ? -1 : a.value > b.value ? 1 : 0));

  const values: string[] = [];
  let bytes = 0;
  for (const { value } of matched) {
    const size = Buffer.byteLength(value, 'utf8');
    if (values.length >= MAX_ITEMS || (values.length > 0 && bytes + size > MAX_BYTES)) break;
    values.push(value);
    bytes += size;
  }
  return { values, total: matched.length, hasMore: values.length < matched.length };
}
