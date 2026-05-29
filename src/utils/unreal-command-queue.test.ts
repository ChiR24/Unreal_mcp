import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { UnrealCommandQueue } from './unreal-command-queue.js';

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
});
