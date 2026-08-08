import { randomUUID } from 'node:crypto';
import type { DebugCorrelationContext, DebugDiagnostic, DebugJobRecord } from './types.js';

type JobExecutor = (signal: AbortSignal) => Promise<unknown>;

export class DebugJobManager {
  private readonly jobs = new Map<string, DebugJobRecord>();
  private readonly controllers = new Map<string, AbortController>();

  constructor(private readonly capacity = 256) {}

  start(kind: string, context: DebugCorrelationContext, executor: JobExecutor): DebugJobRecord {
    const now = new Date().toISOString();
    const job: DebugJobRecord = {
      jobId: randomUUID(),
      kind,
      state: 'queued',
      createdAt: now,
      updatedAt: now,
      context
    };
    const controller = new AbortController();
    this.jobs.set(job.jobId, job);
    this.controllers.set(job.jobId, controller);
    this.trim();
    queueMicrotask(() => void this.run(job.jobId, executor, controller));
    return { ...job };
  }

  get(jobId: string): DebugJobRecord | undefined {
    const job = this.jobs.get(jobId);
    return job ? { ...job } : undefined;
  }

  list(): DebugJobRecord[] {
    return Array.from(this.jobs.values(), (job) => ({ ...job }));
  }

  cancel(jobId: string): DebugJobRecord | undefined {
    const job = this.jobs.get(jobId);
    if (!job) return undefined;
    if (job.state === 'queued' || job.state === 'running') {
      this.controllers.get(jobId)?.abort();
      job.state = 'cancelled';
      job.updatedAt = new Date().toISOString();
    }
    return { ...job };
  }

  private async run(jobId: string, executor: JobExecutor, controller: AbortController): Promise<void> {
    const job = this.jobs.get(jobId);
    if (!job || job.state === 'cancelled') return;
    job.state = 'running';
    job.updatedAt = new Date().toISOString();
    try {
      const result = await executor(controller.signal);
      if (!controller.signal.aborted) {
        job.result = result;
        job.state = this.resultPassed(result) ? 'passed' : 'failed';
      } else {
        job.state = 'cancelled';
      }
    } catch (error) {
      if (controller.signal.aborted) {
        job.state = 'cancelled';
      } else {
        const message = error instanceof Error ? error.message : String(error);
        job.state = message.includes('timed out') ? 'timed_out' : 'failed';
        job.diagnostic = this.diagnostic(message);
      }
    } finally {
      job.updatedAt = new Date().toISOString();
      this.controllers.delete(jobId);
    }
  }

  private resultPassed(result: unknown): boolean {
    if (typeof result !== 'object' || result === null || Array.isArray(result)) return true;
    const value = result as Record<string, unknown>;
    return value.success !== false && value.passed !== false;
  }

  private diagnostic(message: string): DebugDiagnostic {
    return {
      code: 'DEBUG_JOB_FAILED',
      severity: 'error',
      component: 'sidecar',
      phase: 'job_execution',
      retriable: true,
      message
    };
  }

  private trim(): void {
    while (this.jobs.size > this.capacity) {
      const completed = Array.from(this.jobs.values()).find((job) => !['queued', 'running'].includes(job.state));
      if (!completed) return;
      this.jobs.delete(completed.jobId);
      this.controllers.delete(completed.jobId);
    }
  }
}
