// Task 33: direct lock for deterministic ranking and budget capping, including
// the fallback-only typo rule discovered via the manual driver: a precise value
// must not drown in single-edit sibling values.

import { describe, expect, it } from 'vitest';

import { applyBudget, rankCandidates } from './completion-ranking.js';
import { MAX_COMPLETION_BYTES, MAX_COMPLETION_ITEMS, type CompletionCandidate } from './completion-types.js';

const cand = (value: string): CompletionCandidate => ({ value, kind: 'enum' });

describe('rankCandidates', () => {
  it('is independent of input order', () => {
    const forward = [cand('a.b'), cand('a.a'), cand('a.c')];
    const reversed = [...forward].reverse();
    expect(rankCandidates(forward, 'a.').map((c) => c.value)).toEqual(rankCandidates(reversed, 'a.').map((c) => c.value));
  });

  it('returns the whole pool sorted for an empty prefix', () => {
    expect(rankCandidates([cand('c'), cand('a'), cand('b')], '').map((c) => c.value)).toEqual(['a', 'b', 'c']);
  });

  it('ranks an exact-prefix match ahead of a substring match', () => {
    const ranked = rankCandidates([cand('x_asset'), cand('asset_x')], 'asset').map((c) => c.value);
    expect(ranked[0]).toBe('asset_x');
  });

  it('suppresses one-edit siblings when an exact value matches (fallback-only typo)', () => {
    const versions = ['5.0', '5.5', '5.7', '5.8'].map(cand);
    expect(rankCandidates(versions, '5.7').map((c) => c.value)).toEqual(['5.7']);
  });

  it('surfaces a substitution typo only when nothing stronger matched', () => {
    expect(rankCandidates([cand('asset')], 'asZet').map((c) => c.value)).toEqual(['asset']);
  });

  it('drops candidates that do not match at all', () => {
    expect(rankCandidates([cand('zzz')], 'asset')).toHaveLength(0);
  });
});

describe('applyBudget', () => {
  it('caps at the item budget and reports total and hasMore', () => {
    const many = Array.from({ length: MAX_COMPLETION_ITEMS + 25 }, (_v, index) => cand(`c${String(index).padStart(3, '0')}`));
    const result = applyBudget(many);
    expect(result.values.length).toBe(MAX_COMPLETION_ITEMS);
    expect(result.total).toBe(MAX_COMPLETION_ITEMS + 25);
    expect(result.hasMore).toBe(true);
  });

  it('caps at the serialized-byte budget', () => {
    const heavy = Array.from({ length: 60 }, (_v, index) => cand(`${'x'.repeat(300)}${String(index)}`));
    const result = applyBudget(heavy);
    const bytes = result.values.reduce((sum, value) => sum + Buffer.byteLength(value, 'utf8'), 0);
    expect(bytes).toBeLessThanOrEqual(MAX_COMPLETION_BYTES);
    expect(result.hasMore).toBe(true);
  });

  it('reports no truncation when everything fits', () => {
    const result = applyBudget([cand('a'), cand('b')]);
    expect(result.values).toEqual(['a', 'b']);
    expect(result.total).toBe(2);
    expect(result.hasMore).toBe(false);
  });
});
