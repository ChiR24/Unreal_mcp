// tests/eval/model-fixture.ts
// Deterministic, offline stand-in for a discovery/model "call". No network, no
// randomness, no real model. Given an intent and the available capability
// universe, it returns a stable ranked candidate list derived from transparent
// lexical overlap. This is the fixture the scorer evaluates; adversarial cases
// may instead supply an explicit ranking override in the corpus.

import type { ManifestModel, ModelRanking, RankedCandidate } from './types.js';
import { TOP_K } from './types.js';

const DESTRUCTIVE_PREFIXES = ['delete', 'destroy'] as const;
const DESTRUCTIVE_EXACT = new Set(['bulk_delete', 'remove_foliage_instances', 'remove_volume']);

export function isDestructive(action: string): boolean {
  if (DESTRUCTIVE_EXACT.has(action)) return true;
  return DESTRUCTIVE_PREFIXES.some((prefix) => action.startsWith(prefix));
}

function tokenize(value: string): readonly string[] {
  const matches = value.toLowerCase().match(/[a-z0-9]+/g);
  return matches === null ? [] : matches;
}

function scoreIntent(
  intentTokens: ReadonlySet<string>,
  actionTokens: ReadonlySet<string>,
  toolTokens: ReadonlySet<string>,
  descTokens: ReadonlySet<string>,
  paramTokens: ReadonlySet<string>,
): number {
  let score = 0;
  for (const tok of intentTokens) {
    if (actionTokens.has(tok)) score += 3;
    if (toolTokens.has(tok)) score += 2;
    if (descTokens.has(tok)) score += 1;
    if (paramTokens.has(tok)) score += 1;
  }
  return score;
}

export function modelCall(
  intent: string,
  model: ManifestModel,
  topK: number = TOP_K,
): ModelRanking {
  const intentTokens = new Set(tokenize(intent));
  const scored: RankedCandidate[] = [];
  for (const tool of model.tools) {
    const toolTokens = new Set(tokenize(tool.name));
    const descTokens = new Set(tokenize(tool.description));
    const paramTokens = new Set(tool.parameterNames.flatMap((p) => tokenize(p)));
    for (const action of tool.actions) {
      const actionTokens = new Set(tokenize(action));
      const score = scoreIntent(intentTokens, actionTokens, toolTokens, descTokens, paramTokens);
      if (score <= 0) continue;
      scored.push({ tool: tool.name, action, score });
    }
  }
  scored.sort((a, b) => {
    if (b.score !== a.score) return b.score - a.score;
    if (a.tool !== b.tool) return a.tool < b.tool ? -1 : 1;
    return a.action < b.action ? -1 : 1;
  });
  return scored.slice(0, topK);
}
