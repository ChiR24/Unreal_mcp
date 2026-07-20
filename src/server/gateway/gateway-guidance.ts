// src/server/gateway/gateway-guidance.ts
// Closest-match suggestion + executable nextCall construction for guided
// self-correction. An AI client can copy `nextCall` verbatim as its next
// gateway request, removing trial-and-error discovery.

export const MAX_SUGGESTIONS = 3;

function levenshtein(a: string, b: string): number {
  const m = a.length;
  const n = b.length;
  if (m === 0) return n;
  if (n === 0) return m;
  let previous = Array.from({ length: n + 1 }, (_, index) => index);
  let current = new Array<number>(n + 1).fill(0);
  for (let i = 1; i <= m; i++) {
    current[0] = i;
    for (let j = 1; j <= n; j++) {
      const cost = a[i - 1] === b[j - 1] ? 0 : 1;
      current[j] = Math.min(previous[j] + 1, current[j - 1] + 1, previous[j - 1] + cost);
    }
    [previous, current] = [current, previous];
  }
  return previous[n];
}

function commonPrefixLength(left: string, right: string): number {
  const limit = Math.min(left.length, right.length);
  let shared = 0;
  while (shared < limit && left[shared] === right[shared]) shared += 1;
  return shared;
}

// Edit distance alone ties candidates a caller would never confuse: `manage_asts`
// is 3 edits from `manage_asset`, `manage_ai`, and `manage_gas` alike, so the top
// suggestion fell to whichever the catalog listed first. A typo keeps the prefix
// it was mistyped from, so the longer shared prefix is the better correction; the
// trailing name comparison makes the order total rather than catalog-dependent.
export function closestMatches(target: string, candidates: string[], limit: number = MAX_SUGGESTIONS): string[] {
  if (limit <= 0) return [];
  const normalized = target.trim().toLowerCase();
  if (normalized.length === 0) return candidates.slice(0, limit);
  return candidates
    .map((candidate) => {
      const lower = candidate.toLowerCase();
      let score = levenshtein(lower, normalized);
      if (lower.includes(normalized) || normalized.includes(lower)) score -= 4;
      return { candidate, score, prefix: commonPrefixLength(lower, normalized) };
    })
    .sort((left, right) =>
      left.score - right.score
      || right.prefix - left.prefix
      || left.candidate.localeCompare(right.candidate))
    .slice(0, limit)
    .map((entry) => entry.candidate);
}

/** Build a directly-invokable gateway request payload. Omitted parts stay absent. */
export function buildNextCall(parts: {
  operation: string;
  tool?: string;
  action?: string;
  param?: string;
}): Record<string, unknown> {
  const call: Record<string, unknown> = { operation: parts.operation };
  if (parts.tool !== undefined) call.tool = parts.tool;
  if (parts.action !== undefined) call.action = parts.action;
  if (parts.param !== undefined) call.param = parts.param;
  return call;
}
