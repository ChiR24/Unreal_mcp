import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import {
  UnrealCommandQueue,
  UnrealCommandQueueStoppedError,
} from './unreal-command-queue.js';

interface Deferred<T> {
  promise: Promise<T>;
  resolve: (value: T) => void;
  reject: (reason?: unknown) => void;
}

function deferred<T>(): Deferred<T> {
  let resolve!: (value: T) => void;
  let reject!: (reason?: unknown) => void;
  const promise = new Promise<T>((promiseResolve, promiseReject) => {
    resolve = promiseResolve;
    reject = promiseReject;
  });

  return { promise, resolve, reject };
}

describe('UnrealCommandQueue', () => {
  const baseTime = new Date('2026-01-01T00:00:00.000Z');
  let queue: UnrealCommandQueue;

  beforeEach(() => {
    vi.useFakeTimers();
    vi.setSystemTime(baseTime);
    vi.spyOn(Math, 'random').mockReturnValue(0);
    queue = new UnrealCommandQueue();
  });

  afterEach(() => {
    queue.stopProcessor();
    vi.useRealTimers();
    vi.restoreAllMocks();
  });

  it('runs queued commands by priority after the active command completes', async () => {
    const firstCommand = deferred<void>();
    const order: string[] = [];

    const first = queue.execute(async () => {
      order.push('first');
      await firstCommand.promise;
      return 'first';
    }, 7);
    const light = queue.execute(async () => {
      order.push('light');
      return 'light';
    }, 9);
    const heavy = queue.execute(async () => {
      order.push('heavy');
      return 'heavy';
    }, 1);

    expect(order).toEqual(['first']);

    firstCommand.resolve();
    await vi.advanceTimersByTimeAsync(499);
    expect(order).toEqual(['first']);

    await vi.advanceTimersByTimeAsync(1);
    await expect(heavy).resolves.toBe('heavy');
    expect(order).toEqual(['first', 'heavy']);

    await vi.advanceTimersByTimeAsync(99);
    expect(order).toEqual(['first', 'heavy']);

    await vi.advanceTimersByTimeAsync(1);
    await expect(light).resolves.toBe('light');
    await expect(first).resolves.toBe('first');
    expect(order).toEqual(['first', 'heavy', 'light']);
  });

  it('throttles every stat command from the previous stat command start', async () => {
    const firstCommand = deferred<void>();
    const startedAt: number[] = [];

    const first = queue.execute(async () => {
      startedAt.push(Date.now() - baseTime.getTime());
      await firstCommand.promise;
      return 'first';
    }, 8);
    const second = queue.execute(async () => {
      startedAt.push(Date.now() - baseTime.getTime());
      return 'second';
    }, 8);
    const third = queue.execute(async () => {
      startedAt.push(Date.now() - baseTime.getTime());
      return 'third';
    }, 8);

    expect(startedAt).toEqual([0]);

    firstCommand.resolve();
    await vi.advanceTimersByTimeAsync(299);
    expect(startedAt).toEqual([0]);

    await vi.advanceTimersByTimeAsync(1);
    await expect(second).resolves.toBe('second');
    expect(startedAt).toEqual([0, 300]);

    await vi.advanceTimersByTimeAsync(299);
    expect(startedAt).toEqual([0, 300]);

    await vi.advanceTimersByTimeAsync(1);
    await expect(third).resolves.toBe('third');
    await expect(first).resolves.toBe('first');
    expect(startedAt).toEqual([0, 300, 600]);
  });

  it('does not retry an active command after the queue is stopped', async () => {
    const releaseFailure = deferred<void>();
    const command = vi.fn(async () => {
      await releaseFailure.promise;
      throw new Error('Automation bridge not connected');
    });

    const result = queue.execute(command, 8);
    queue.stopProcessor();
    releaseFailure.resolve();

    await expect(result).rejects.toBeInstanceOf(
      UnrealCommandQueueStoppedError,
    );
    await vi.advanceTimersByTimeAsync(2000);
    expect(command).toHaveBeenCalledTimes(1);
  });

  it('never re-runs a non-idempotent command after a transient failure', async () => {
    // A mutating console command (priority 5 is `summon`/`spawn` in
    // CommandValidator.getPriority). A client-side timeout does NOT prove the
    // editor skipped the work, so re-running it can spawn the actor twice.
    const command = vi.fn(async () => {
      throw new Error('Request 8f2c timed out after 30000ms');
    });

    const outcome = queue
      .execute(command, 5)
      .then(() => 'resolved' as const, () => 'rejected' as const);

    await vi.advanceTimersByTimeAsync(10_000);

    expect(await outcome).toBe('rejected');
    expect(command).toHaveBeenCalledTimes(1);
  });

  it('never re-runs a command that a disconnect interrupted', async () => {
    const command = vi.fn(async () => {
      throw new Error('Automation bridge not connected');
    });

    const outcome = queue
      .execute(command, 5)
      .then(() => 'resolved' as const, () => 'rejected' as const);

    await vi.advanceTimersByTimeAsync(10_000);

    expect(await outcome).toBe('rejected');
    expect(command).toHaveBeenCalledTimes(1);
  });

  it('recovers a declared safe read exactly once after a transient failure', async () => {
    let attempts = 0;
    const command = vi.fn(async () => {
      attempts += 1;
      if (attempts === 1) {
        throw new Error('Request 8f2c timed out after 30000ms');
      }
      return 'level-list';
    });

    const result = queue.execute(command, 9, { retryPolicy: 'safe-read' });
    await vi.advanceTimersByTimeAsync(10_000);

    await expect(result).resolves.toBe('level-list');
    expect(command).toHaveBeenCalledTimes(2);
  });

  it('recovers an idempotency-protected mutation exactly once after a transient failure', async () => {
    let attempts = 0;
    const command = vi.fn(async () => {
      attempts += 1;
      if (attempts === 1) {
        throw new Error('Automation bridge not connected');
      }
      return 'imported';
    });

    const result = queue.execute(command, 5, {
      retryPolicy: 'idempotency-protected',
    });
    await vi.advanceTimersByTimeAsync(10_000);

    await expect(result).resolves.toBe('imported');
    expect(command).toHaveBeenCalledTimes(2);
  });

  it('caps recovery of retry-safe work at a single attempt', async () => {
    const command = vi.fn(async () => {
      throw new Error('Request 8f2c timed out after 30000ms');
    });

    const outcome = queue
      .execute(command, 9, { retryPolicy: 'safe-read' })
      .then(() => 'resolved' as const, () => 'rejected' as const);

    await vi.advanceTimersByTimeAsync(10_000);

    expect(await outcome).toBe('rejected');
    expect(command).toHaveBeenCalledTimes(2);
  });
});
