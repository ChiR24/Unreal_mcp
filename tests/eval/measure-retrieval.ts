// tests/eval/measure-retrieval.ts
// Top-1 and top-K retrieval of a ranker over the corpus.

import { GATEWAY_DEFAULT_SEARCH_LIMIT, retrievalCases } from './fixtures.js';

/** The capability ids a search answers, best first. */
export type GatewayRanker = (intent: string, limit: number) => readonly string[];

export function measureRetrieval(rank: GatewayRanker): {
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
