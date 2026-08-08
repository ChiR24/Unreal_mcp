import { describe, expect, it, vi } from 'vitest';
import { DebugJobManager } from './job-manager.js';

const context = { traceId: 'trace', timestamp: '2026-07-28T00:00:00.000Z' };

describe('DebugJobManager', () => {
  it('transitions queued jobs to passed', async () => {
    const manager = new DebugJobManager();
    const job = manager.start('test', context, async () => ({ success: true }));
    expect(job.state).toBe('queued');
    await vi.waitFor(() => expect(manager.get(job.jobId)?.state).toBe('passed'));
  });

  it('captures failed results and cancellation', async () => {
    const manager = new DebugJobManager();
    const failed = manager.start('test', context, async () => ({ success: false }));
    await vi.waitFor(() => expect(manager.get(failed.jobId)?.state).toBe('failed'));

    const waiting = manager.start('test', context, async (signal) => {
      await new Promise<void>((resolve) => signal.addEventListener('abort', () => resolve(), { once: true }));
      return { success: true };
    });
    expect(manager.cancel(waiting.jobId)?.state).toBe('cancelled');
  });

  it('distinguishes adapter timeouts from ordinary failures', async () => {
    const manager = new DebugJobManager();
    const timedOut = manager.start('adapter', context, async () => {
      throw new Error('Debug adapter request timed out after 30000ms');
    });

    await vi.waitFor(() => expect(manager.get(timedOut.jobId)?.state).toBe('timed_out'));
  });
});
