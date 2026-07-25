// Task 41: bounded, principal-scoped idempotency ledger.
//
// RED-first contract. Every case here is a behaviour the plan names at line 423:
// first execution, identical replay, in-progress, changed fingerprint, failed
// execution, TTL/cap eviction, principal isolation and cleanup. The clock is
// injected so TTL is exercised without sleeping, and the raw idempotency key is
// asserted absent from the ledger's own state so a key can never leak into a log
// or an evidence file.

import { describe, expect, it } from 'vitest';

import { IdempotencyLedger, type LedgerScope } from './idempotency-ledger.js';

const scope = (over: Partial<LedgerScope> = {}): LedgerScope => ({
  principal: 'scoped:qawriter',
  capabilityId: 'asset.import_asset',
  key: 'client-key-0001',
  ...over,
});

const receipt = (id: string): Record<string, unknown> => ({ ok: true, capabilityId: id });

/** Fake clock: tests advance time explicitly, nothing sleeps. */
const makeClock = (start = 1_000): { now: () => number; advance: (ms: number) => void } => {
  let t = start;
  return { now: () => t, advance: (ms: number) => { t += ms; } };
};

describe('IdempotencyLedger — A. first execution and replay', () => {
  it('admits the first request for a key as first-execution', () => {
    const ledger = new IdempotencyLedger({ clock: makeClock().now });

    const outcome = ledger.begin(scope(), 'fp-a');

    expect(outcome.kind).toBe('first');
  });

  it('replays the recorded receipt for an identical completed key', () => {
    const ledger = new IdempotencyLedger({ clock: makeClock().now });
    const first = ledger.begin(scope(), 'fp-a');
    if (first.kind !== 'first') throw new Error('expected first');
    ledger.complete(first.handle, receipt('asset.import_asset'));

    const second = ledger.begin(scope(), 'fp-a');

    expect(second.kind).toBe('replay');
    if (second.kind !== 'replay') throw new Error('expected replay');
    expect(second.receipt).toEqual(receipt('asset.import_asset'));
  });

  it('reports an in-flight duplicate while the first execution is still running', () => {
    const ledger = new IdempotencyLedger({ clock: makeClock().now });
    ledger.begin(scope(), 'fp-a');

    const concurrent = ledger.begin(scope(), 'fp-a');

    expect(concurrent.kind).toBe('in-flight');
  });
});

describe('IdempotencyLedger — B. fingerprint conflict', () => {
  it('rejects the same key re-used with a different post-normalization fingerprint', () => {
    const ledger = new IdempotencyLedger({ clock: makeClock().now });
    const first = ledger.begin(scope(), 'fp-a');
    if (first.kind !== 'first') throw new Error('expected first');
    ledger.complete(first.handle, receipt('asset.import_asset'));

    const conflicting = ledger.begin(scope(), 'fp-DIFFERENT');

    expect(conflicting.kind).toBe('conflict');
  });

  it('leaks no prior receipt on a conflict', () => {
    const ledger = new IdempotencyLedger({ clock: makeClock().now });
    const first = ledger.begin(scope(), 'fp-a');
    if (first.kind !== 'first') throw new Error('expected first');
    ledger.complete(first.handle, receipt('asset.import_asset'));

    const conflicting = ledger.begin(scope(), 'fp-DIFFERENT');

    expect(JSON.stringify(conflicting)).not.toContain('asset.import_asset');
  });
});

describe('IdempotencyLedger — C. failures are never cached', () => {
  it('re-admits a key whose first execution failed', () => {
    const ledger = new IdempotencyLedger({ clock: makeClock().now });
    const first = ledger.begin(scope(), 'fp-a');
    if (first.kind !== 'first') throw new Error('expected first');

    ledger.abandon(first.handle);

    expect(ledger.begin(scope(), 'fp-a').kind).toBe('first');
  });

  it('drops the entry entirely on abandon so nothing is replayable', () => {
    const ledger = new IdempotencyLedger({ clock: makeClock().now });
    const first = ledger.begin(scope(), 'fp-a');
    if (first.kind !== 'first') throw new Error('expected first');

    ledger.abandon(first.handle);

    expect(ledger.size()).toBe(0);
  });
});

describe('IdempotencyLedger — D. TTL and cap eviction', () => {
  it('expires a completed entry after the 24-hour TTL', () => {
    const clock = makeClock();
    const ledger = new IdempotencyLedger({ clock: clock.now });
    const first = ledger.begin(scope(), 'fp-a');
    if (first.kind !== 'first') throw new Error('expected first');
    ledger.complete(first.handle, receipt('asset.import_asset'));

    clock.advance(24 * 60 * 60 * 1000 + 1);

    expect(ledger.begin(scope(), 'fp-a').kind).toBe('first');
  });

  it('still replays just inside the TTL boundary', () => {
    const clock = makeClock();
    const ledger = new IdempotencyLedger({ clock: clock.now });
    const first = ledger.begin(scope(), 'fp-a');
    if (first.kind !== 'first') throw new Error('expected first');
    ledger.complete(first.handle, receipt('asset.import_asset'));

    clock.advance(24 * 60 * 60 * 1000 - 1);

    expect(ledger.begin(scope(), 'fp-a').kind).toBe('replay');
  });

  it('evicts oldest-first at the cap, deterministically', () => {
    const ledger = new IdempotencyLedger({ clock: makeClock().now, maxEntries: 2 });
    for (const k of ['k1', 'k2']) {
      const o = ledger.begin(scope({ key: k }), 'fp');
      if (o.kind !== 'first') throw new Error('expected first');
      ledger.complete(o.handle, receipt(k));
    }

    const third = ledger.begin(scope({ key: 'k3' }), 'fp');
    if (third.kind !== 'first') throw new Error('expected first');
    ledger.complete(third.handle, receipt('k3'));

    expect(ledger.size()).toBe(2);
    expect(ledger.begin(scope({ key: 'k1' }), 'fp').kind).toBe('first');
    expect(ledger.begin(scope({ key: 'k2' }), 'fp').kind).toBe('replay');
  });

  it('defaults the cap to 1024 entries', () => {
    expect(new IdempotencyLedger({ clock: makeClock().now }).maxEntries).toBe(1024);
  });
});

describe('IdempotencyLedger — E. isolation', () => {
  it('isolates the same key across principals', () => {
    const ledger = new IdempotencyLedger({ clock: makeClock().now });
    const first = ledger.begin(scope({ principal: 'scoped:alice' }), 'fp-a');
    if (first.kind !== 'first') throw new Error('expected first');
    ledger.complete(first.handle, receipt('alice'));

    expect(ledger.begin(scope({ principal: 'scoped:bob' }), 'fp-a').kind).toBe('first');
  });

  it('isolates the same key across capabilities', () => {
    const ledger = new IdempotencyLedger({ clock: makeClock().now });
    const first = ledger.begin(scope({ capabilityId: 'asset.import_asset' }), 'fp-a');
    if (first.kind !== 'first') throw new Error('expected first');
    ledger.complete(first.handle, receipt('import'));

    expect(ledger.begin(scope({ capabilityId: 'asset.delete_asset' }), 'fp-a').kind).toBe('first');
  });
});

describe('IdempotencyLedger — F. secret safety and cleanup', () => {
  it('never retains the raw idempotency key in its own state', () => {
    const ledger = new IdempotencyLedger({ clock: makeClock().now });
    const first = ledger.begin(scope({ key: 'super-secret-key-value' }), 'fp-a');
    if (first.kind !== 'first') throw new Error('expected first');
    ledger.complete(first.handle, receipt('x'));

    expect(ledger.debugState()).not.toContain('super-secret-key-value');
  });

  it('clears every entry for one principal without touching another', () => {
    const ledger = new IdempotencyLedger({ clock: makeClock().now });
    for (const p of ['scoped:alice', 'scoped:bob']) {
      const o = ledger.begin(scope({ principal: p }), 'fp');
      if (o.kind !== 'first') throw new Error('expected first');
      ledger.complete(o.handle, receipt(p));
    }

    ledger.clearPrincipal('scoped:alice');

    expect(ledger.begin(scope({ principal: 'scoped:alice' }), 'fp').kind).toBe('first');
    expect(ledger.begin(scope({ principal: 'scoped:bob' }), 'fp').kind).toBe('replay');
  });
});
