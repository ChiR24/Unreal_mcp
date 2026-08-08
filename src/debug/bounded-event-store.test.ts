import { describe, expect, it } from 'vitest';
import { BoundedEventStore } from './bounded-event-store.js';

const context = { traceId: 'trace', timestamp: '2026-07-28T00:00:00.000Z' };

describe('BoundedEventStore', () => {
  it('uses monotonic cursors and reports dropped events', () => {
    const store = new BoundedEventStore(2);
    store.append({ event: 'first', context });
    store.append({ event: 'second', context });
    store.append({ event: 'third', context });

    expect(store.query()).toMatchObject({
      oldestCursor: 2,
      nextCursor: 3,
      dropped: 1
    });
    expect(store.query({ after: 2 }).events.map((event) => event.event)).toEqual(['third']);
  });

  it('filters correlated structured events', () => {
    const store = new BoundedEventStore();
    store.append({
      event: 'log',
      context: { ...context, debugSessionId: 'one' },
      payload: { category: 'Missile', severity: 'error' },
      message: 'guidance divergence'
    });
    store.append({ event: 'log', context: { ...context, debugSessionId: 'two' }, message: 'unrelated' });

    expect(store.query({ sessionId: 'one', category: 'Missile', regex: 'divergence' }).events).toHaveLength(1);
  });

  it('returns the newest matching event without a query page-size bias', () => {
    const store = new BoundedEventStore(2_000);
    for (let index = 0; index < 1_200; index += 1) {
      store.append({ event: 'probe_snapshot', context, payload: { index } });
    }

    expect(store.latest({ event: 'probe_snapshot' })?.payload).toEqual({ index: 1_199 });
  });
});
