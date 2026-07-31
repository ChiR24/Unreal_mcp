// Task 44 lane A (RED first) — per-request progress routing and isolation.
//
// Progress arrives from Unreal keyed by an automation request id and has to
// reach the ONE in-flight MCP request that asked for it. The security-shaped
// property is that it can reach no other: a concurrent request must never
// observe another request's progress, and must never have its token used.
//
// The registry is also the lifetime seam. A settled request unregisters, so a
// late frame arriving after the terminal result is dropped silently rather than
// resurrecting a closed reporter or throwing inside the transport.

import { describe, expect, it } from 'vitest';
import {
  MAX_TRACKED_PROGRESS_REQUESTS,
  ProgressSinkRegistry,
} from '../../../src/server/mcp-primitives/progress/progress-sink-registry.js';
import {
  createProgressReporter,
  type ProgressNotification,
} from '../../../src/server/mcp-primitives/progress/progress-reporter.js';

function reporterFor(token: string | number) {
  const sent: ProgressNotification[] = [];
  const reporter = createProgressReporter({
    token,
    notify: async (notification) => {
      sent.push(notification);
    },
  });
  return { sent, reporter };
}

describe('Task 44 — progress reaches only the request that asked for it', () => {
  it('routes an update to the matching request', async () => {
    const registry = new ProgressSinkRegistry();
    const a = reporterFor('token-a');
    registry.register('num:1', a.reporter);

    registry.report('num:1', { progress: 25, total: 100 });
    await a.reporter.drain();

    expect(a.sent).toHaveLength(1);
    expect(a.sent[0]?.params.progressToken).toBe('token-a');
    expect(a.sent[0]?.params.progress).toBe(25);
  });

  it('NEVER leaks one request progress or token to another', async () => {
    const registry = new ProgressSinkRegistry();
    const a = reporterFor('token-a');
    const b = reporterFor('token-b');
    registry.register('num:1', a.reporter);
    registry.register('str:other-session-req', b.reporter);

    registry.report('num:1', { progress: 10 });
    await Promise.all([a.reporter.drain(), b.reporter.drain()]);

    expect(a.sent).toHaveLength(1);
    expect(b.sent).toEqual([]);
    expect(a.sent.map((n) => n.params.progressToken)).toEqual(['token-a']);
  });

  it('keeps string and number request ids in distinct slots', async () => {
    const registry = new ProgressSinkRegistry();
    const numeric = reporterFor('for-number-1');
    const textual = reporterFor('for-string-1');
    registry.register('num:1', numeric.reporter);
    registry.register('str:1', textual.reporter);

    registry.report('str:1', { progress: 5 });
    await Promise.all([numeric.reporter.drain(), textual.reporter.drain()]);

    expect(numeric.sent).toEqual([]);
    expect(textual.sent).toHaveLength(1);
  });

  it('drops an update for an unknown request without throwing', () => {
    const registry = new ProgressSinkRegistry();

    expect(() => registry.report('num:999', { progress: 1 })).not.toThrow();
    expect(registry.size).toBe(0);
  });
});

describe('Task 44 — a settled request stops receiving progress', () => {
  it('drops a late frame that arrives after unregister', async () => {
    const registry = new ProgressSinkRegistry();
    const a = reporterFor('token-a');
    registry.register('num:1', a.reporter);

    registry.report('num:1', { progress: 10 });
    registry.unregister('num:1');
    registry.report('num:1', { progress: 20 });
    await a.reporter.drain();

    expect(a.sent.map((n) => n.params.progress)).toEqual([10]);
    expect(registry.size).toBe(0);
  });

  it('treats unregister as idempotent', () => {
    const registry = new ProgressSinkRegistry();
    registry.register('num:1', reporterFor('t').reporter);

    registry.unregister('num:1');
    registry.unregister('num:1');

    expect(registry.size).toBe(0);
  });

  it('leaks no entry once every request settled', () => {
    const registry = new ProgressSinkRegistry();
    for (let index = 0; index < 10; index += 1) {
      registry.register(`num:${index}`, reporterFor(index).reporter);
    }
    expect(registry.size).toBe(10);

    for (let index = 0; index < 10; index += 1) registry.unregister(`num:${index}`);

    expect(registry.size).toBe(0);
  });

  it('clear() drains every tracked request', () => {
    const registry = new ProgressSinkRegistry();
    registry.register('num:1', reporterFor('a').reporter);
    registry.register('num:2', reporterFor('b').reporter);

    registry.clear();

    expect(registry.size).toBe(0);
  });
});

describe('Task 44 — the sink table is bounded', () => {
  it('never grows past the cap even under unbounded registration', () => {
    const registry = new ProgressSinkRegistry();

    for (let index = 0; index < MAX_TRACKED_PROGRESS_REQUESTS + 500; index += 1) {
      registry.register(`num:${index}`, reporterFor(index).reporter);
    }

    expect(registry.size).toBeLessThanOrEqual(MAX_TRACKED_PROGRESS_REQUESTS);
  });

  it('evicts oldest-first so the newest request keeps working', async () => {
    const registry = new ProgressSinkRegistry({ max: 2 });
    const first = reporterFor('first');
    const second = reporterFor('second');
    const third = reporterFor('third');

    registry.register('num:1', first.reporter);
    registry.register('num:2', second.reporter);
    registry.register('num:3', third.reporter);

    registry.report('num:1', { progress: 1 });
    registry.report('num:2', { progress: 2 });
    registry.report('num:3', { progress: 3 });
    await Promise.all([
      first.reporter.drain(),
      second.reporter.drain(),
      third.reporter.drain(),
    ]);

    expect(registry.size).toBe(2);
    expect(first.sent).toEqual([]);
    expect(second.sent).toHaveLength(1);
    expect(third.sent).toHaveLength(1);
  });

  it('re-registering an id replaces rather than duplicates it', () => {
    const registry = new ProgressSinkRegistry();
    registry.register('num:1', reporterFor('a').reporter);
    registry.register('num:1', reporterFor('b').reporter);

    expect(registry.size).toBe(1);
  });
});
