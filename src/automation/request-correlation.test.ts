import { describe, it, expect, vi } from 'vitest';
import { RequestCorrelation } from './request-correlation.js';
import { McpRequestCancelledError } from './request-cancellation-error.js';
import { canonicalizeMcpRequestId } from './request-context.js';
import { RequestTracker } from './request-tracker.js';
import type { QueuedRequestItem } from './types.js';
import { Logger } from '../utils/logging/logger.js';

function silentLogger(): Logger {
    return new Logger('request-correlation-test', 'error');
}

/**
 * A subscriber whose resolve/reject settle a real promise. `outcome` is updated
 * synchronously when either is invoked, and is idempotent: a late resolve after a
 * reject cannot resurrect a rejected subscriber (the guard keeps the first state).
 */
interface Tracked {
    resolve: (value: unknown) => void;
    reject: (reason: unknown) => void;
    readonly getOutcome: () => 'pending' | 'resolved' | 'rejected';
    readonly promise: Promise<unknown>;
}

function tracked(): Tracked {
    let resolveFn: (value: unknown) => void = () => undefined;
    let rejectFn: (reason: unknown) => void = () => undefined;
    let outcome: 'pending' | 'resolved' | 'rejected' = 'pending';
    const promise = new Promise<unknown>((res, rej) => {
        resolveFn = (value: unknown) => {
            if (outcome === 'pending') outcome = 'resolved';
            res(value);
        };
        rejectFn = (reason: unknown) => {
            if (outcome === 'pending') outcome = 'rejected';
            rej(reason);
        };
    });
    // Swallow the expected cancel() rejection; settlement state is tracked synchronously via getOutcome().
    void promise.catch(() => undefined);
    return {
        resolve: resolveFn,
        reject: rejectFn,
        getOutcome: () => outcome,
        promise
    };
}

function delivery() {
    const sendFrame = vi.fn();
    const rejectUnderlying = vi.fn();
    return {
        sendFrame,
        rejectUnderlying,
        log: silentLogger()
    };
}

function realCoalesceKey(action: string, payload: Record<string, unknown>): string {
    const tracker = new RequestTracker(10);
    const key = tracker.createCoalesceKey(action, payload);
    expect(key.length).toBeGreaterThan(0);
    return key;
}

describe('RequestCorrelation — A. baseline characterization', () => {
    it('register() with a canonicalized mcp id records the subscriber and leaves an unrelated coalesce key empty', () => {
        const correlation = new RequestCorrelation();
        const sub = tracked();
        const mcpRequestId = canonicalizeMcpRequestId('abc');
        correlation.register(mcpRequestId, 'auto_a1', sub.resolve, sub.reject);

        expect(correlation.getAutoIdForCoalesceKey('not-noted')).toBeUndefined();
    });

    it('cancel() of a single subscriber rejects it with McpRequestCancelledError and emits exactly one frame + one underlying reject', async () => {
        const correlation = new RequestCorrelation();
        const sub = tracked();
        const mcpRequestId = canonicalizeMcpRequestId('abc');
        correlation.register(mcpRequestId, 'auto_a2', sub.resolve, sub.reject);

        const d = delivery();
        correlation.cancel(mcpRequestId, 'user-abort', d);

        expect(sub.getOutcome()).toBe('rejected');
        expect(d.sendFrame).toHaveBeenCalledTimes(1);
        expect(d.sendFrame).toHaveBeenCalledWith('auto_a2');
        expect(d.rejectUnderlying).toHaveBeenCalledTimes(1);
        expect(d.rejectUnderlying).toHaveBeenCalledWith('auto_a2');

        const err = await sub.promise.catch((e) => e);
        expect(err).toBeInstanceOf(McpRequestCancelledError);
        if (err instanceof McpRequestCancelledError) {
            expect(err.code).toBe('MCP_REQUEST_CANCELLED');
            expect(err.cancelled).toBe(true);
            expect(err.reason).toBe('user-abort');
        }
    });
});

describe('RequestCorrelation — FINDING 1: stale coalesce map on send-failure settle', () => {
    it('clears autoByCoalesceKey when settle is called for an unregistered autoId that had a coalesce key noted', () => {
        const correlation = new RequestCorrelation();
        const coalesceKey = realCoalesceKey('get_asset', { path: '/Game/Foo.foo' });
        const autoId = 'auto_fail_1';

        correlation.noteCoalesceKey(coalesceKey, autoId);
        expect(correlation.getAutoIdForCoalesceKey(coalesceKey)).toBe(autoId);

        correlation.settle(autoId);

        expect(correlation.getAutoIdForCoalesceKey(coalesceKey)).toBeUndefined();
    });

    it('does not coalesce a same-tick retry onto the rejected promise after a send-failure settle', () => {
        const correlation = new RequestCorrelation();
        const coalesceKey = realCoalesceKey('get_asset', { path: '/Game/Bar.bar' });
        const failedAutoId = 'auto_fail_2';
        const retryAutoId = 'auto_retry_2';

        correlation.noteCoalesceKey(coalesceKey, failedAutoId);
        correlation.settle(failedAutoId);

        const resolved = correlation.getAutoIdForCoalesceKey(coalesceKey);
        expect(resolved).not.toBe(failedAutoId);

        correlation.noteCoalesceKey(coalesceKey, retryAutoId);
        expect(correlation.getAutoIdForCoalesceKey(coalesceKey)).toBe(retryAutoId);
    });

    it('keeps normal-path coalesce cleanup working', () => {
        const correlation = new RequestCorrelation();
        const coalesceKey = realCoalesceKey('get_asset', { path: '/Game/Baz.baz' });
        const autoId = 'auto_ok_1';

        const sub = tracked();
        correlation.register('mcp_1', autoId, sub.resolve, sub.reject);
        correlation.noteCoalesceKey(coalesceKey, autoId);

        correlation.settle(autoId);
        expect(correlation.getAutoIdForCoalesceKey(coalesceKey)).toBeUndefined();
    });
});

describe('RequestCorrelation — B. coalesced subscribers (reference-counted teardown)', () => {
    it('cancelling a non-last coalesced subscriber rejects only it and emits no frame; the other stays pending', () => {
        const correlation = new RequestCorrelation();
        const coalesceKey = realCoalesceKey('get_asset', { path: '/Game/Coalesced.coalesced' });
        const autoId = 'auto_coalesced';
        const mcpFirst = canonicalizeMcpRequestId('first');
        const mcpSecond = canonicalizeMcpRequestId('second');

        const first = tracked();
        const second = tracked();
        correlation.register(mcpFirst, autoId, first.resolve, first.reject);
        correlation.register(mcpSecond, autoId, second.resolve, second.reject);
        correlation.noteCoalesceKey(coalesceKey, autoId);

        const d = delivery();
        correlation.cancel(mcpFirst, 'cancel-first', d);

        expect(first.getOutcome()).toBe('rejected');
        expect(second.getOutcome()).toBe('pending');
        expect(d.sendFrame).not.toHaveBeenCalled();
        expect(d.rejectUnderlying).not.toHaveBeenCalled();
    });

    it('cancelling the last coalesced subscriber emits exactly one frame + one underlying reject across both cancels', () => {
        const correlation = new RequestCorrelation();
        const coalesceKey = realCoalesceKey('get_asset', { path: '/Game/Coalesced.coalesced' });
        const autoId = 'auto_coalesced';
        const mcpFirst = canonicalizeMcpRequestId('first');
        const mcpSecond = canonicalizeMcpRequestId('second');

        const first = tracked();
        const second = tracked();
        correlation.register(mcpFirst, autoId, first.resolve, first.reject);
        correlation.register(mcpSecond, autoId, second.resolve, second.reject);
        correlation.noteCoalesceKey(coalesceKey, autoId);

        const d = delivery();
        correlation.cancel(mcpFirst, 'cancel-first', d);
        correlation.cancel(mcpSecond, 'cancel-second', d);

        expect(first.getOutcome()).toBe('rejected');
        expect(second.getOutcome()).toBe('rejected');
        expect(d.sendFrame).toHaveBeenCalledTimes(1);
        expect(d.sendFrame).toHaveBeenCalledWith(autoId);
        expect(d.rejectUnderlying).toHaveBeenCalledTimes(1);
    });

    it('suppresses a late result after settle: a second cancel is a no-op and emits no frame', () => {
        const correlation = new RequestCorrelation();
        const coalesceKey = realCoalesceKey('get_asset', { path: '/Game/Late.late' });
        const autoId = 'auto_late';
        const mcpRequestId = canonicalizeMcpRequestId('late-owner');

        const sub = tracked();
        correlation.register(mcpRequestId, autoId, sub.resolve, sub.reject);
        correlation.noteCoalesceKey(coalesceKey, autoId);

        correlation.settle(autoId);
        expect(correlation.getAutoIdForCoalesceKey(coalesceKey)).toBeUndefined();

        const d = delivery();
        correlation.cancel(mcpRequestId, 'after-settle', d);
        expect(d.sendFrame).not.toHaveBeenCalled();
        expect(sub.getOutcome()).toBe('pending');
    });

    it('drops the coalesce lookup after the underlying settles so a retry cannot resolve onto a stale id', () => {
        const correlation = new RequestCorrelation();
        const coalesceKey = realCoalesceKey('get_asset', { path: '/Game/Stale.stale' });
        const autoId = 'auto_stale';

        const sub = tracked();
        correlation.register('mcp_x', autoId, sub.resolve, sub.reject);
        correlation.noteCoalesceKey(coalesceKey, autoId);
        correlation.settle(autoId);

        expect(correlation.getAutoIdForCoalesceKey(coalesceKey)).toBeUndefined();
    });
});

describe('RequestCorrelation — C. duplicate / unknown cancel (idempotency, interruption)', () => {
    it('a second cancel of the same mcp id is a no-op: frame + underlying reject fire exactly once', () => {
        const correlation = new RequestCorrelation();
        const autoId = 'auto_dup';
        const mcpRequestId = canonicalizeMcpRequestId('dup');

        const sub = tracked();
        correlation.register(mcpRequestId, autoId, sub.resolve, sub.reject);

        const d = delivery();
        correlation.cancel(mcpRequestId, 'first', d);
        correlation.cancel(mcpRequestId, 'second', d);

        expect(sub.getOutcome()).toBe('rejected');
        expect(d.sendFrame).toHaveBeenCalledTimes(1);
        expect(d.rejectUnderlying).toHaveBeenCalledTimes(1);
    });

    it('cancel of an unknown mcp id throws nothing and emits no frame / underlying reject', () => {
        const correlation = new RequestCorrelation();
        const d = delivery();
        expect(() => correlation.cancel(canonicalizeMcpRequestId('ghost'), 'r', d)).not.toThrow();
        expect(d.sendFrame).not.toHaveBeenCalled();
        expect(d.rejectUnderlying).not.toHaveBeenCalled();
    });

    it('cancelling a numeric-id mcp request never affects a string-id request with the same textual value', () => {
        const correlation = new RequestCorrelation();
        const numId = canonicalizeMcpRequestId(1);
        const strId = canonicalizeMcpRequestId('1');
        const subNum = tracked();
        const subStr = tracked();

        correlation.register(numId, 'auto_num', subNum.resolve, subNum.reject);
        correlation.register(strId, 'auto_str', subStr.resolve, subStr.reject);

        const d = delivery();
        correlation.cancel(numId, 'r', d);

        expect(subNum.getOutcome()).toBe('rejected');
        expect(subStr.getOutcome()).toBe('pending');
        expect(d.sendFrame).toHaveBeenCalledTimes(1);
    });
});

describe('RequestCorrelation — D3. queued register / take / detach (drained exactly once)', () => {
    function item(mcpRequestId: string, action: string): QueuedRequestItem {
        return {
            resolve: () => undefined,
            reject: () => undefined,
            action,
            payload: {},
            options: {},
            mcpRequestId
        };
    }

    it('takeQueued returns every owned item once and empties the bucket', () => {
        const correlation = new RequestCorrelation();
        const a = item('m', 'get_asset');
        const b = item('m', 'get_asset');
        correlation.registerQueued('m', a);
        correlation.registerQueued('m', b);

        const taken = correlation.takeQueued('m');
        expect(taken).toHaveLength(2);
        expect(correlation.takeQueued('m')).toHaveLength(0);
    });

    it('detachQueued removes a single item so the next take returns the remainder', () => {
        const correlation = new RequestCorrelation();
        const a = item('m', 'get_asset');
        const b = item('m', 'get_asset');
        correlation.registerQueued('m', a);
        correlation.registerQueued('m', b);
        correlation.detachQueued(a);

        const taken = correlation.takeQueued('m');
        expect(taken).toHaveLength(1);
        expect(taken[0]).toBe(b);
    });

    it('registerQueued with undefined mcp id is a no-op (nothing to correlate to an id)', () => {
        const correlation = new RequestCorrelation();
        const a = item('m', 'get_asset');
        correlation.registerQueued(undefined, a);
        expect(correlation.takeQueued('m')).toHaveLength(0);
    });
});

describe('RequestCorrelation — FINDING 5: cancel must not desync when a subscriber reject throws', () => {
    it('stays non-throwing and still tears down byMcp when a subscriber reject throws', () => {
        const correlation = new RequestCorrelation();
        const mcpRequestId = canonicalizeMcpRequestId('cancel-1');
        const autoId = 'auto_cancel_1';

        const sub = tracked();
        sub.reject = () => {
            throw new Error('boom');
        };

        correlation.register(mcpRequestId, autoId, sub.resolve, sub.reject);
        correlation.noteCoalesceKey('k', autoId);

        const d = delivery();
        expect(() => correlation.cancel(mcpRequestId, 'test', d)).not.toThrow();

        correlation.cancel(mcpRequestId, 'test', d);
        expect(d.sendFrame).toHaveBeenCalledTimes(1);
        expect(d.rejectUnderlying).toHaveBeenCalledTimes(1);
    });

    it('cancels remaining subscribers even when an earlier subscriber reject throws', () => {
        const correlation = new RequestCorrelation();
        const mcpRequestId = canonicalizeMcpRequestId('cancel-2');
        const autoIdA = 'auto_A';
        const autoIdB = 'auto_B';

        const throwingSub = tracked();
        throwingSub.reject = () => {
            throw new Error('boom');
        };
        const safeSub = tracked();

        correlation.register(mcpRequestId, autoIdA, throwingSub.resolve, throwingSub.reject);
        correlation.register(mcpRequestId, autoIdB, safeSub.resolve, safeSub.reject);

        const d = delivery();
        expect(() =>
            correlation.cancel(mcpRequestId, 'test', d)
        ).not.toThrow();

        expect(safeSub.getOutcome()).toBe('rejected');
        expect(d.sendFrame).toHaveBeenCalledTimes(2);
        expect(d.rejectUnderlying).toHaveBeenCalledTimes(2);
    });
});

describe('RequestCorrelation — F. map teardown exactly once (acceptance core)', () => {
    it('cancelling a multi-autoId mcp request drains each underlying exactly once and leaves no coalesce residue', () => {
        const correlation = new RequestCorrelation();
        const mcpRequestId = canonicalizeMcpRequestId('multi');
        const autoA = 'auto_f_a';
        const autoB = 'auto_f_b';
        const coalesceKeyA = realCoalesceKey('get_asset', { path: '/Game/F_A.f_a' });
        const coalesceKeyB = realCoalesceKey('list', { path: '/Game/F_B.f_b' });

        const a = tracked();
        const b = tracked();
        correlation.register(mcpRequestId, autoA, a.resolve, a.reject);
        correlation.register(mcpRequestId, autoB, b.resolve, b.reject);
        correlation.noteCoalesceKey(coalesceKeyA, autoA);
        correlation.noteCoalesceKey(coalesceKeyB, autoB);

        const d = delivery();
        correlation.cancel(mcpRequestId, 'multi', d);

        expect(a.getOutcome()).toBe('rejected');
        expect(b.getOutcome()).toBe('rejected');
        expect(d.sendFrame).toHaveBeenCalledTimes(2);
        expect(d.sendFrame).toHaveBeenNthCalledWith(1, autoA);
        expect(d.sendFrame).toHaveBeenNthCalledWith(2, autoB);
        expect(d.rejectUnderlying).toHaveBeenCalledTimes(2);

        expect(correlation.getAutoIdForCoalesceKey(coalesceKeyA)).toBeUndefined();
        expect(correlation.getAutoIdForCoalesceKey(coalesceKeyB)).toBeUndefined();

        const again = delivery();
        correlation.cancel(mcpRequestId, 'multi', again);
        expect(again.sendFrame).not.toHaveBeenCalled();
    });
});

describe('RequestCorrelation — G. adversarial probes (dirty/stale/flaky/misleading/interruption)', () => {
    it('dirty: register with undefined mcp id lands only in byAuto and never in byMcp', () => {
        const correlation = new RequestCorrelation();
        const sub = tracked();
        correlation.register(undefined, 'auto_dirty', sub.resolve, sub.reject);

        const d = delivery();
        expect(() => correlation.cancel(canonicalizeMcpRequestId('nope'), 'r', d)).not.toThrow();
        expect(d.sendFrame).not.toHaveBeenCalled();

        correlation.noteCoalesceKey('dirty-k', 'auto_dirty');
        expect(correlation.getAutoIdForCoalesceKey('dirty-k')).toBe('auto_dirty');
        correlation.settle('auto_dirty');
        expect(correlation.getAutoIdForCoalesceKey('dirty-k')).toBeUndefined();
    });

    it('dirty: a new subscriber registered after settle is independent and cancelled on its own', () => {
        const correlation = new RequestCorrelation();
        const first = tracked();
        correlation.register('m1', 'auto_rereg', first.resolve, first.reject);
        correlation.settle('auto_rereg');

        const second = tracked();
        correlation.register('m2', 'auto_rereg', second.resolve, second.reject);

        const d = delivery();
        correlation.cancel('m2', 'r', d);
        expect(second.getOutcome()).toBe('rejected');
        expect(first.getOutcome()).toBe('pending');
        expect(d.sendFrame).toHaveBeenCalledTimes(1);
    });

    it('stale: settle is idempotent and does not throw on a second call', () => {
        const correlation = new RequestCorrelation();
        correlation.register('m', 'auto_stale2', () => undefined, () => undefined);
        expect(() => {
            correlation.settle('auto_stale2');
            correlation.settle('auto_stale2');
        }).not.toThrow();
    });

    it('flaky: 1000 registers produce collision-free subscriber ids', () => {
        const correlation = new RequestCorrelation();
        const ids: string[] = [];
        for (let i = 0; i < 1000; i++) {
            ids.push(correlation.register(`m_${i}`, `auto_${i}`, () => undefined, () => undefined));
        }
        expect(new Set(ids).size).toBe(1000);
    });

    it('flaky: teardown counts are independent of subscriber insertion order', () => {
        const correlation = new RequestCorrelation();
        const autoShared = 'auto_order';
        const subscribers = [tracked(), tracked(), tracked()];
        for (const s of subscribers) {
            correlation.register(canonicalizeMcpRequestId('ord'), autoShared, s.resolve, s.reject);
        }
        const d = delivery();
        correlation.cancel(canonicalizeMcpRequestId('ord'), 'r', d);
        expect(d.sendFrame).toHaveBeenCalledTimes(1);
        for (const s of subscribers) expect(s.getOutcome()).toBe('rejected');
    });

    it('misleading: total frames equal the number of distinct auto ids with no duplicates', () => {
        const correlation = new RequestCorrelation();
        const mcpRequestId = canonicalizeMcpRequestId('mis');
        const autoIds = ['auto_m1', 'auto_m2', 'auto_m3'];
        for (const autoId of autoIds) {
            correlation.register(mcpRequestId, autoId, () => undefined, () => undefined);
        }
        const d = delivery();
        correlation.cancel(mcpRequestId, 'r', d);
        expect(d.sendFrame).toHaveBeenCalledTimes(autoIds.length);
        const called = new Set(d.sendFrame.mock.calls.map((c) => c[0]));
        expect(called.size).toBe(autoIds.length);
    });

    it('interruption: a late underlying resolve cannot resurrect a cancelled subscriber', () => {
        const correlation = new RequestCorrelation();
        const shared = tracked();
        const sub = tracked();
        shared.promise.then(sub.resolve, sub.reject);
        correlation.register('m', 'auto_late2', sub.resolve, sub.reject);

        const d = delivery();
        correlation.cancel('m', 'r', d);
        expect(sub.getOutcome()).toBe('rejected');

        shared.resolve('late-result');
        expect(sub.getOutcome()).toBe('rejected');
    });

    it('interruption: cancel-then-settle and settle-then-cancel are both safe and never double-reject', () => {
        const correlation = new RequestCorrelation();
        const sendFrame = vi.fn();
        const rejectUnderlying = vi.fn();
        const d = { sendFrame, rejectUnderlying, log: silentLogger() };

        const subCancelFirst = tracked();
        correlation.register('m_cf', 'auto_cf', subCancelFirst.resolve, subCancelFirst.reject);
        correlation.cancel('m_cf', 'r', d);
        correlation.settle('auto_cf');
        expect(subCancelFirst.getOutcome()).toBe('rejected');
        expect(sendFrame).toHaveBeenCalledTimes(1);

        const subSettleFirst = tracked();
        correlation.register('m_sf', 'auto_sf', subSettleFirst.resolve, subSettleFirst.reject);
        correlation.settle('auto_sf');
        expect(subSettleFirst.getOutcome()).toBe('pending');
        correlation.cancel('m_sf', 'r', d);
        expect(subSettleFirst.getOutcome()).toBe('pending');
        expect(sendFrame).toHaveBeenCalledTimes(1);
    });
});
