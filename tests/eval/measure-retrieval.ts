// tests/eval/measure-retrieval.ts
// Top-1 and top-K retrieval, measured through the real gateway `search`.

import { searchGatewayCapabilities } from '../../src/server/gateway/gateway-search.js';
import { GATEWAY_DEFAULT_SEARCH_LIMIT, retrievalCases } from './fixtures.js';

/** Injection seam: tests substitute a ranker to prove a breach fails the gate. */
export type GatewayRanker = (intent: string, limit: number) => readonly string[];

export const gatewaySearchRanker: GatewayRanker = (intent, limit) => {
  const response = searchGatewayCapabilities({ operation: 'search', query: intent, limit });
  const rows = Array.isArray(response.results) ? response.results : [];
  return rows.map((row) =>
    typeof row === 'object' && row !== null && 'capability' in row
      ? String((row as Record<string, unknown>).capability)
      : '',
  );
};

export function measureRetrieval(rank: GatewayRanker = gatewaySearchRanker): {
  readonly top1Accuracy: number;
  readonly topKRecall: number;
  readonly misses: readonly string[];
} {
  const cases = retrievalCases();
  let top1 = 0;
  let recall = 0;
  const misses: string[] = [];
  for (const entry of cases) {
    const ranked = rank(entry.intent, GATEWAY_DEFAULT_SEARCH_LIMIT);
    const top = ranked[0];
    const top1Correct = top !== undefined && entry.acceptedCapabilityIds.includes(top);
    const recallCorrect = ranked.includes(entry.expectedCapabilityId);
    if (top1Correct) top1 += 1;
    if (recallCorrect) recall += 1;
    if (!top1Correct || !recallCorrect) misses.push(`${entry.id}: got ${String(top)}`);
  }
  const total = Math.max(1, cases.length);
  return { top1Accuracy: top1 / total, topKRecall: recall / total, misses };
}
