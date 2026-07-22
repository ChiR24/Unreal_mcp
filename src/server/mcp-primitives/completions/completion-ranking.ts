// src/server/mcp-primitives/completions/completion-ranking.ts
// Task 33: deterministic ranking plus item/byte budget capping. Ranking is a
// closed tier ladder (exact-prefix, substring, subsequence, single-edit typo)
// with the candidate value as the stable lexicographic tiebreak, so identical
// inputs always yield identical ordered output. Matching is case-insensitive;
// the original value is preserved for output. Native mirror:
// Private/MCP/Primitives/McpCompletionProvider.{h,cpp}.

import {
  MAX_COMPLETION_BYTES,
  MAX_COMPLETION_ITEMS,
  type CompletionCandidate,
  type CompletionResult,
} from './completion-types.js';

// Ranking tiers, best (lowest) first; TIER_NONE candidates are dropped.
const TIER_EXACT_PREFIX = 0;
const TIER_SUBSTRING = 1;
const TIER_SUBSEQUENCE = 2;
const TIER_TYPO = 3;
const TIER_NONE = 99;

// True when `a` is within edit distance 1 of `b` (one insert, delete, or
// substitute). Bounded and allocation-free so a typo match never costs a scan.
function withinOneEdit(a: string, b: string): boolean {
  if (a === b) return true;
  const la = a.length;
  const lb = b.length;
  if (Math.abs(la - lb) > 1) return false;
  let i = 0;
  let j = 0;
  let edits = 0;
  while (i < la && j < lb) {
    if (a[i] === b[j]) {
      i += 1;
      j += 1;
      continue;
    }
    edits += 1;
    if (edits > 1) return false;
    if (la > lb) i += 1;
    else if (lb > la) j += 1;
    else {
      i += 1;
      j += 1;
    }
  }
  if (i < la || j < lb) edits += 1;
  return edits <= 1;
}

// True when every char of `needle` appears in `haystack` in order (loose match).
function isSubsequence(needle: string, haystack: string): boolean {
  let n = 0;
  for (let h = 0; h < haystack.length && n < needle.length; h += 1) {
    if (haystack[h] === needle[n]) n += 1;
  }
  return n === needle.length;
}

// The best tier a candidate value earns against the (already lower-cased) prefix.
function tierFor(value: string, prefix: string): number {
  if (prefix.length === 0 || value.startsWith(prefix)) return TIER_EXACT_PREFIX;
  if (value.includes(prefix)) return TIER_SUBSTRING;
  if (isSubsequence(prefix, value)) return TIER_SUBSEQUENCE;
  if (withinOneEdit(prefix, value.slice(0, prefix.length)) || withinOneEdit(prefix, value.slice(0, prefix.length + 1))) {
    return TIER_TYPO;
  }
  return TIER_NONE;
}

/**
 * Rank a candidate pool against a prefix. Non-matching candidates are dropped;
 * the rest are ordered by tier, then by value lexicographically. The result is
 * fully deterministic and independent of the input order.
 */
export function rankCandidates(
  pool: readonly CompletionCandidate[],
  prefix: string,
): readonly CompletionCandidate[] {
  const lowered = prefix.toLowerCase();
  const scored: { candidate: CompletionCandidate; tier: number }[] = [];
  let hasStrongMatch = false;
  for (const candidate of pool) {
    const tier = tierFor(candidate.value.toLowerCase(), lowered);
    if (tier === TIER_NONE) continue;
    if (tier < TIER_TYPO) hasStrongMatch = true;
    scored.push({ candidate, tier });
  }
  // Typos are a fallback: keep them only when no prefix/substring/subsequence matched.
  const matched = hasStrongMatch ? scored.filter((entry) => entry.tier < TIER_TYPO) : scored;
  matched.sort((a, b) => {
    if (a.tier !== b.tier) return a.tier - b.tier;
    if (a.candidate.value < b.candidate.value) return -1;
    if (a.candidate.value > b.candidate.value) return 1;
    return 0;
  });
  return matched.map((entry) => entry.candidate);
}

/**
 * Cap a ranked list to the item and serialized-byte budgets. `total` is the full
 * ranked count; `hasMore` is true when either budget truncated the list. The
 * first value is always kept even if it alone exceeds the byte budget, so a
 * single long value never produces a misleading empty result.
 */
export function applyBudget(ranked: readonly CompletionCandidate[]): CompletionResult {
  const values: string[] = [];
  let bytes = 0;
  let truncated = false;
  for (const candidate of ranked) {
    if (values.length >= MAX_COMPLETION_ITEMS) {
      truncated = true;
      break;
    }
    const size = Buffer.byteLength(candidate.value, 'utf8');
    if (values.length > 0 && bytes + size > MAX_COMPLETION_BYTES) {
      truncated = true;
      break;
    }
    values.push(candidate.value);
    bytes += size;
  }
  return { values, total: ranked.length, hasMore: truncated || values.length < ranked.length };
}
