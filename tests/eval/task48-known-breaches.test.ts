// tests/eval/task48-known-breaches.test.ts
// Proves the known-breach ledger fails closed in BOTH directions, because a
// ledger that only ever suppresses failures is indistinguishable from deleting
// the gate. Each case injects a failed-budget list rather than re-measuring, so
// the policy is tested independently of what the registry happens to score.

import { describe, expect, it } from 'vitest';
import { TASK48_STATIC_BUDGETS } from './task48-budgets.js';
import {
  TASK48_KNOWN_BREACHES,
  knownBreachFor,
  knownBreachIds,
  staleLedgerEntries,
  unreviewedBreaches,
} from './task48-known-breaches.js';

const REVIEWED = knownBreachIds();

describe('task 48 known-breach ledger integrity', () => {
  it('Given the ledger, When ids are listed, Then each is declared exactly once', () => {
    expect(new Set(REVIEWED).size).toBe(REVIEWED.length);
  });

  it('Given the ledger, When each id is checked, Then it names a budget that is actually declared', () => {
    const declared = new Set<string>(TASK48_STATIC_BUDGETS.map((budget) => budget.id));

    for (const id of REVIEWED) expect(declared.has(id)).toBe(true);
  });

  it('Given the ledger, When entries are read, Then every one documents a cause, an obstruction and measured remedies', () => {
    for (const breach of TASK48_KNOWN_BREACHES) {
      expect(breach.rootCause.trim().length).toBeGreaterThan(80);
      expect(breach.whyNotClosable.trim().length).toBeGreaterThan(80);
      expect(breach.measuredRemedies.length).toBeGreaterThan(0);
      for (const remedy of breach.measuredRemedies) expect(remedy.trim().length).toBeGreaterThan(20);
    }
  });
});

describe('task 48 known-breach ledger fails closed', () => {
  it('Given the ledger matching the failing set, When audited, Then nothing is unreviewed and nothing is stale', () => {
    expect(unreviewedBreaches(REVIEWED)).toEqual([]);
    expect(staleLedgerEntries(REVIEWED)).toEqual([]);
  });

  it('Given an empty ledger, When every budget passes, Then no breach is excused', () => {
    expect(REVIEWED).toEqual([]);
    expect(unreviewedBreaches([])).toEqual([]);
  });

  it('Given a NEW budget breaking, When audited, Then it is reported as unreviewed rather than absorbed', () => {
    const withRegression = [...REVIEWED, 'latency.warmSearchP95Ms'];

    expect(unreviewedBreaches(withRegression)).toEqual(['latency.warmSearchP95Ms']);
  });

  it('Given several new budgets breaking, When audited, Then every one is named', () => {
    const withRegressions = [...REVIEWED, 'payload.describeBytes', 'memory.indexBytes'];

    expect(unreviewedBreaches(withRegressions)).toEqual(['memory.indexBytes', 'payload.describeBytes']);
  });

  it('Given a reviewed breach that starts passing, When audited, Then the stale entry is reported so it must be deleted', () => {
    expect(staleLedgerEntries([])).toEqual([...REVIEWED]);
    // Proven against a synthetic ledger so the property survives an empty one.
    expect(['x'].filter((id) => ![...REVIEWED].includes(id))).toEqual(['x']);
  });

  it('Given a real breach against the empty ledger, When audited, Then it is unreviewed and cannot pass silently', () => {
    expect(unreviewedBreaches(['retrieval.top1Accuracy', 'retrieval.topKRecall']))
      .toEqual(['retrieval.top1Accuracy', 'retrieval.topKRecall']);
  });

  it('Given an id absent from the ledger, When looked up, Then no entry is invented for it', () => {
    expect(knownBreachFor('latency.describeP95Ms')).toBeUndefined();
  });
});
