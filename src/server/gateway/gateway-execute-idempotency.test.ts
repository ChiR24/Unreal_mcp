// Task 41: the execute-path idempotency seam.
//
// This is the layer between the gateway and the ledger. It decides whether a
// request participates in dedup at all (only the `idempotency-key` behaviour
// class, and only when the client actually supplied a key), computes the
// canonical POST-normalization fingerprint, and settles the slot exactly once.
//
// The load-bearing negative is here too: a dispatch that throws OR that returns
// a non-cacheable (error) receipt must release the slot, so a failure is never
// replayed as a success and the key stays retryable.

import { describe, expect, it, vi } from 'vitest';

import { IdempotencyLedger } from './idempotency-ledger.js';
import { capabilityIndex } from './gateway-capability-index.js';
import {
  canonicalFingerprint,
  runWithIdempotency,
  type IdempotentRequest
} from './gateway-execute-idempotency.js';

const ledger = (): IdempotencyLedger => new IdempotencyLedger({ clock: () => 1_000 });

const req = (over: Partial<IdempotentRequest> = {}): IdempotentRequest => ({
  capabilityId: 'asset.import_asset',
  principal: 'scoped:qawriter',
  params: { sourcePath: '/tmp/a.fbx' },
  idempotencyKey: 'key-1',
  ...over,
});

const ok = (): Record<string, unknown> => ({ ok: true, value: 'created' });
const errorReceipt = (): Record<string, unknown> => ({ ok: false, errorCode: 'EXEC_FAILED' });
const cacheable = (r: Record<string, unknown>): boolean => r.ok === true;
const conflict = (reason: string): Record<string, unknown> => ({ ok: false, errorCode: 'CONFLICT', reason });

describe('canonicalFingerprint', () => {
  it('is stable across key ordering', () => {
    expect(canonicalFingerprint('c', { a: 1, b: 2 })).toBe(canonicalFingerprint('c', { b: 2, a: 1 }));
  });

  it('changes when a value changes', () => {
    expect(canonicalFingerprint('c', { a: 1 })).not.toBe(canonicalFingerprint('c', { a: 2 }));
  });

  it('changes when the capability changes', () => {
    expect(canonicalFingerprint('c1', { a: 1 })).not.toBe(canonicalFingerprint('c2', { a: 1 }));
  });

  it('distinguishes nested reorderings from real changes', () => {
    const same = canonicalFingerprint('c', { o: { x: 1, y: 2 } }) === canonicalFingerprint('c', { o: { y: 2, x: 1 } });
    const differs = canonicalFingerprint('c', { o: { x: 1 } }) !== canonicalFingerprint('c', { o: { x: 9 } });
    expect([same, differs]).toEqual([true, true]);
  });
});

describe('runWithIdempotency — opt-out cases dispatch straight through', () => {
  // Not supplying a key is the ONLY way out. This used to also list "the
  // capability's declared class is not idempotency-key", which no record ever
  // declares — see the reachability block at the foot of this file.
  it('does not engage for an empty key, which is not a client opting in', async () => {
    const l = ledger();
    const dispatch = vi.fn(async () => ok());

    await runWithIdempotency(req({ idempotencyKey: '' }), l, dispatch, cacheable, conflict);

    expect([dispatch.mock.calls.length, l.size()]).toEqual([1, 0]);
  });

  it('does not engage when the client supplied no key', async () => {
    const l = ledger();
    const dispatch = vi.fn(async () => ok());

    await runWithIdempotency(req({ idempotencyKey: undefined }), l, dispatch, cacheable, conflict);

    expect([dispatch.mock.calls.length, l.size()]).toEqual([1, 0]);
  });
});

describe('runWithIdempotency — dedup', () => {
  it('dispatches once and replays the recorded receipt on an identical retry', async () => {
    const l = ledger();
    const dispatch = vi.fn(async () => ok());

    const first = await runWithIdempotency(req(), l, dispatch, cacheable, conflict);
    const second = await runWithIdempotency(req(), l, dispatch, cacheable, conflict);

    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(second).toEqual(first);
  });

  it('refuses a key re-used with different effective params', async () => {
    const l = ledger();
    const dispatch = vi.fn(async () => ok());
    await runWithIdempotency(req(), l, dispatch, cacheable, conflict);

    const clash = await runWithIdempotency(req({ params: { sourcePath: '/tmp/OTHER.fbx' } }), l, dispatch, cacheable, conflict);

    expect(dispatch).toHaveBeenCalledTimes(1);
    expect(clash.errorCode).toBe('CONFLICT');
  });

  it('isolates identical keys across principals', async () => {
    const l = ledger();
    const dispatch = vi.fn(async () => ok());

    await runWithIdempotency(req({ principal: 'scoped:alice' }), l, dispatch, cacheable, conflict);
    await runWithIdempotency(req({ principal: 'scoped:bob' }), l, dispatch, cacheable, conflict);

    expect(dispatch).toHaveBeenCalledTimes(2);
  });
});

describe('runWithIdempotency — failures are never cached', () => {
  it('releases the slot when dispatch throws, and rethrows', async () => {
    const l = ledger();
    const boom = vi.fn(async () => { throw new Error('bridge died'); });

    await expect(runWithIdempotency(req(), l, boom, cacheable, conflict)).rejects.toThrow('bridge died');

    expect(l.size()).toBe(0);
  });

  it('lets the same key be retried after a thrown dispatch', async () => {
    const l = ledger();
    const boom = async (): Promise<Record<string, unknown>> => { throw new Error('bridge died'); };
    await runWithIdempotency(req(), l, boom, cacheable, conflict).catch(() => undefined);

    const retry = await runWithIdempotency(req(), l, async () => ok(), cacheable, conflict);

    expect(retry).toEqual(ok());
  });

  it('releases the slot when dispatch returns a non-cacheable error receipt', async () => {
    const l = ledger();

    const result = await runWithIdempotency(req(), l, async () => errorReceipt(), cacheable, conflict);

    expect(result).toEqual(errorReceipt());
    expect(l.size()).toBe(0);
  });

  it('re-dispatches after an error receipt rather than replaying it', async () => {
    const l = ledger();
    await runWithIdempotency(req(), l, async () => errorReceipt(), cacheable, conflict);
    const dispatch = vi.fn(async () => ok());

    await runWithIdempotency(req(), l, dispatch, cacheable, conflict);

    expect(dispatch).toHaveBeenCalledTimes(1);
  });
});

describe('runWithIdempotency — concurrency', () => {
  it('refuses a duplicate that arrives while the first is still in flight', async () => {
    const l = ledger();
    let release = (): void => undefined;
    const slow = async (): Promise<Record<string, unknown>> => {
      await new Promise<void>((r) => { release = r; });
      return ok();
    };

    const inFlight = runWithIdempotency(req(), l, slow, cacheable, conflict);
    const duplicate = await runWithIdempotency(req(), l, async () => ok(), cacheable, conflict);
    release();
    await inFlight;

    expect(duplicate.errorCode).toBe('CONFLICT');
  });

  it('names the in-flight case distinctly from a fingerprint clash', async () => {
    const l = ledger();
    let release = (): void => undefined;
    const slow = async (): Promise<Record<string, unknown>> => {
      await new Promise<void>((r) => { release = r; });
      return ok();
    };

    const inFlight = runWithIdempotency(req(), l, slow, cacheable, conflict);
    const duplicate = await runWithIdempotency(req(), l, async () => ok(), cacheable, conflict);
    release();
    await inFlight;

    expect(duplicate.reason).toBe('IN_FLIGHT');
  });
});

// Task 46 F4. Every case above hands the seam a hand-written
// `idempotencyClass: 'idempotency-key'`, which no capability record declares -
// so they proved the ledger works without proving any request can reach it.
// These are anchored to the classes the REAL registry emits instead.
//
// Parity anchor: McpNativeTransportGatewayExecute.cpp engages its ledger on
// `if (!Context.IdempotencyId.IsEmpty())` - a client key, any capability.
// One real capability per declared class, lexicographically first so the sample
// moves only when the contract does — never a hand-picked id whose behavior the
// author already believed.
const SAMPLE_PER_DECLARED_CLASS: readonly (readonly [string, string])[] = [
  ...capabilityIndex().records
    .slice()
    .sort((a, b) => (a.id < b.id ? -1 : a.id > b.id ? 1 : 0))
    .reduce(
      (byClass, record) => byClass.has(record.behavior.idempotency)
        ? byClass
        : byClass.set(record.behavior.idempotency, record.id),
      new Map<string, string>()
    )
].sort();

describe('runWithIdempotency — reachable for every class the registry actually emits', () => {
  it('emits classes, and none of them is the one the old gate demanded', () => {
    expect(SAMPLE_PER_DECLARED_CLASS.length).toBeGreaterThan(0);
    expect(SAMPLE_PER_DECLARED_CLASS.map(([declaredClass]) => declaredClass)).not.toContain('idempotency-key');
  });

  it.each(SAMPLE_PER_DECLARED_CLASS)(
    'dispatches ONCE for a %s capability (%s) called twice with the same key and params',
    async (_declaredClass, capabilityId) => {
      const l = ledger();
      const dispatch = vi.fn(async () => ok());

      const first = await runWithIdempotency(req({ capabilityId }), l, dispatch, cacheable, conflict);
      const second = await runWithIdempotency(req({ capabilityId }), l, dispatch, cacheable, conflict);

      expect(dispatch).toHaveBeenCalledTimes(1);
      expect(second).toEqual(first);
    }
  );

  it.each(SAMPLE_PER_DECLARED_CLASS)(
    'refuses a %s key (%s) re-used with different params as a typed conflict',
    async (_declaredClass, capabilityId) => {
      const l = ledger();
      const dispatch = vi.fn(async () => ok());
      await runWithIdempotency(req({ capabilityId }), l, dispatch, cacheable, conflict);

      const clash = await runWithIdempotency(
        req({ capabilityId, params: { sourcePath: '/tmp/OTHER.fbx' } }),
        l,
        dispatch,
        cacheable,
        conflict
      );

      expect(dispatch).toHaveBeenCalledTimes(1);
      expect(clash).toMatchObject({ errorCode: 'CONFLICT', reason: 'FINGERPRINT_MISMATCH' });
    }
  );
});
