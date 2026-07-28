// tests/eval/task48-known-breaches.ts
// The reviewed, documented breach ledger for Task 48.
//
// The ledger is currently EMPTY: every declared budget passes, so nothing is
// excused. It never weakens a threshold and never makes a failing gate pass.
// What it adds is the distinction between "a breach we investigated and cannot
// close honestly" and "a breach that just appeared", which a bare red suite
// cannot express.
//
// It is deliberately double-sided, so it cannot rot into an excuse:
//   - a breach NOT listed here fails the gate (regression detection intact);
//   - a listed breach that starts PASSING also fails the gate, forcing the
//     entry to be deleted rather than left behind as stale cover.
//
// Adding an entry here is a review decision, not a workaround. Each one must
// carry the remedies that were actually measured and why each was rejected.

export type KnownBreach = {
  readonly id: string;
  readonly requirement: string;
  /** What the ranking is actually doing, in product terms. */
  readonly rootCause: string;
  /**
   * Why closing it would require an action the task forbids. If this cannot be
   * stated concretely, the breach does not belong in this ledger.
   */
  readonly whyNotClosable: string;
  /** Remedies that were implemented and measured before this was accepted. */
  readonly measuredRemedies: readonly string[];
};

export const TASK48_KNOWN_BREACHES: readonly KnownBreach[] = Object.freeze([]);

export function knownBreachIds(): readonly string[] {
  return Object.freeze([...TASK48_KNOWN_BREACHES.map((breach) => breach.id)].sort());
}

export function knownBreachFor(id: string): KnownBreach | undefined {
  return TASK48_KNOWN_BREACHES.find((breach) => breach.id === id);
}

export function unreviewedBreaches(failedBudgetIds: readonly string[]): readonly string[] {
  const reviewed = new Set(knownBreachIds());
  return Object.freeze([...failedBudgetIds].filter((id) => !reviewed.has(id)).sort());
}

/**
 * Ledger entries whose budget now passes. These are not good news to be
 * ignored: leaving one behind would let a future breach of the same budget hide
 * inside an entry that no longer describes reality, so the gate demands the
 * entry be deleted.
 */
export function staleLedgerEntries(failedBudgetIds: readonly string[]): readonly string[] {
  const failing = new Set(failedBudgetIds);
  return Object.freeze(knownBreachIds().filter((id) => !failing.has(id)));
}
