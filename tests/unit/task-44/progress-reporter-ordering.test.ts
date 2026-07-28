// Task 44 lane A (RED first) — monotonic, bounded, active-only progress.
//
// Asserted on what a CLIENT would observe (the notification objects handed to
// the transport), never on internal counters:
//   1. monotonic   - a notification never reports progress <= the one before it
//   2. bounded     - a chatty source cannot emit an unbounded stream
//   3. active-only - nothing is emitted once the operation reached its terminal
//                    state, so a late frame can never trail a returned result
//
// Delivery is drained with an explicit `drain()` await, never a sleep or timer,
// so no case here depends on wall-clock timing.

import { describe, expect, it, vi } from 'vitest';
import {
  createProgressReporter,
  MAX_PROGRESS_NOTIFICATIONS,
  type ProgressNotification,
} from '../../../src/server/mcp-primitives/progress/progress-reporter.js';

function recorder() {
  const sent: ProgressNotification[] = [];
  const notify = vi.fn(async (notification: ProgressNotification) => {
    sent.push(notification);
  });
  return { sent, notify };
}

const progresses = (sent: readonly ProgressNotification[]): number[] =>
  sent.map((n) => n.params.progress);

describe('Task 44 — emitted progress carries the client token verbatim', () => {
  it('stamps every notification with the exact string token', async () => {
    const { sent, notify } = recorder();
    const reporter = createProgressReporter({ token: 'client-token-1', notify });

    reporter.report({ progress: 10, total: 100 });
    reporter.report({ progress: 20, total: 100 });
    await reporter.drain();

    expect(sent).toHaveLength(2);
    for (const notification of sent) {
      expect(notification.method).toBe('notifications/progress');
      expect(notification.params.progressToken).toBe('client-token-1');
      expect(typeof notification.params.progressToken).toBe('string');
    }
  });

  it('stamps a number token as a number, never stringified', async () => {
    const { sent, notify } = recorder();
    const reporter = createProgressReporter({ token: 7, notify });

    reporter.report({ progress: 1 });
    await reporter.drain();

    expect(sent[0]?.params.progressToken).toBe(7);
    expect(typeof sent[0]?.params.progressToken).toBe('number');
  });

  it('emits NOTHING when the client sent no token', async () => {
    // Absent token must not be back-filled with an invented id: the correct
    // behaviour is silence, not a notification the client cannot correlate.
    const { sent, notify } = recorder();
    const reporter = createProgressReporter({ token: undefined, notify });

    reporter.report({ progress: 1 });
    reporter.report({ progress: 2 });
    await reporter.drain();

    expect(sent).toEqual([]);
    expect(notify).not.toHaveBeenCalled();
    expect(reporter.sent).toBe(0);
  });
});

describe('Task 44 — progress is monotonic', () => {
  it('drops a regressing update instead of emitting it', async () => {
    const { sent, notify } = recorder();
    const reporter = createProgressReporter({ token: 'tok', notify });

    reporter.report({ progress: 10 });
    reporter.report({ progress: 5 });
    reporter.report({ progress: 20 });
    await reporter.drain();

    expect(progresses(sent)).toEqual([10, 20]);
  });

  it('drops a repeated identical value (strictly increasing)', async () => {
    const { sent, notify } = recorder();
    const reporter = createProgressReporter({ token: 'tok', notify });

    reporter.report({ progress: 10 });
    reporter.report({ progress: 10 });
    await reporter.drain();

    expect(progresses(sent)).toEqual([10]);
  });

  it('accepts an initial zero and keeps it strictly increasing after', async () => {
    const { sent, notify } = recorder();
    const reporter = createProgressReporter({ token: 'tok', notify });

    reporter.report({ progress: 0 });
    reporter.report({ progress: 0 });
    reporter.report({ progress: 0.5 });
    await reporter.drain();

    expect(progresses(sent)).toEqual([0, 0.5]);
  });

  it('delivers notifications in report order', async () => {
    // Serialized delivery: a reporter that fires without chaining could let the
    // transport resolve out of order and show the client progress going
    // backwards even though each value was individually monotonic.
    const { sent, notify } = recorder();
    const reporter = createProgressReporter({ token: 'tok', notify });

    for (let value = 1; value <= 8; value += 1) reporter.report({ progress: value });
    await reporter.drain();

    expect(progresses(sent)).toEqual([1, 2, 3, 4, 5, 6, 7, 8]);
  });

  it('drops non-finite progress values', async () => {
    const { sent, notify } = recorder();
    const reporter = createProgressReporter({ token: 'tok', notify });

    reporter.report({ progress: Number.NaN });
    reporter.report({ progress: Number.POSITIVE_INFINITY });
    await reporter.drain();

    expect(sent).toEqual([]);
  });
});

describe('Task 44 — progress is bounded', () => {
  it('stops emitting after the cap regardless of how chatty the source is', async () => {
    const { sent, notify } = recorder();
    const reporter = createProgressReporter({ token: 'tok', notify });

    for (let value = 1; value <= MAX_PROGRESS_NOTIFICATIONS + 250; value += 1) {
      reporter.report({ progress: value });
    }
    await reporter.drain();

    expect(sent).toHaveLength(MAX_PROGRESS_NOTIFICATIONS);
    expect(progresses(sent).at(-1)).toBe(MAX_PROGRESS_NOTIFICATIONS);
  });

  it('honours an explicit lower cap', async () => {
    const { sent, notify } = recorder();
    const reporter = createProgressReporter({ token: 'tok', notify, max: 3 });

    for (let value = 1; value <= 50; value += 1) reporter.report({ progress: value });
    await reporter.drain();

    expect(progresses(sent)).toEqual([1, 2, 3]);
  });

  it('bounds the message string it forwards', async () => {
    const { sent, notify } = recorder();
    const reporter = createProgressReporter({ token: 'tok', notify });

    reporter.report({ progress: 1, message: 'x'.repeat(10_000) });
    await reporter.drain();

    const message = sent[0]?.params.message ?? '';
    expect(message.length).toBeLessThanOrEqual(512);
  });
});

describe('Task 44 — progress is emitted only while the operation is active', () => {
  it('emits nothing after close(), so no frame trails the terminal result', async () => {
    const { sent, notify } = recorder();
    const reporter = createProgressReporter({ token: 'tok', notify });

    reporter.report({ progress: 1 });
    reporter.close();
    reporter.report({ progress: 2 });
    reporter.report({ progress: 3 });
    await reporter.drain();

    expect(progresses(sent)).toEqual([1]);
    expect(reporter.active).toBe(false);
  });

  it('treats close() as idempotent — exactly one terminal transition', async () => {
    const { notify } = recorder();
    const reporter = createProgressReporter({ token: 'tok', notify });

    reporter.close();
    reporter.close();
    reporter.close();

    expect(reporter.active).toBe(false);
    await expect(reporter.drain()).resolves.toBeUndefined();
  });

  it('starts active and reports its own emitted count', async () => {
    const { notify } = recorder();
    const reporter = createProgressReporter({ token: 'tok', notify });

    expect(reporter.active).toBe(true);
    reporter.report({ progress: 1 });
    reporter.report({ progress: 2 });
    await reporter.drain();

    expect(reporter.sent).toBe(2);
  });

  it('keeps a transport failure from breaking the caller or the chain', async () => {
    const failures: unknown[] = [];
    const notify = vi.fn(async () => {
      throw new Error('transport closed');
    });
    const reporter = createProgressReporter({
      token: 'tok',
      notify,
      onError: (error) => failures.push(error),
    });

    expect(() => reporter.report({ progress: 1 })).not.toThrow();
    await expect(reporter.drain()).resolves.toBeUndefined();
    expect(failures).toHaveLength(1);
  });
});
