import { Logger } from './logger.js';
import crypto from 'crypto';

const log = new Logger('TaskQueue');

export type TaskStatus = 'pending' | 'running' | 'completed' | 'failed' | 'cancelled';

export interface TaskEntry {
  id: string;
  status: TaskStatus;
  action: string;
  params: Record<string, unknown>;
  createdAt: number;
  startedAt?: number;
  completedAt?: number;
  result?: unknown;
  error?: string;
  progress?: number;
}

export interface TaskQueueOptions {
  maxConcurrent?: number;
  maxQueueSize?: number;
  defaultTimeoutMs?: number;
}

export class TaskQueue {
  private tasks: Map<string, TaskEntry> = new Map();
  private runningCount = 0;
  private readonly _maxConcurrent: number;
  private readonly maxQueueSize: number;
  private readonly _defaultTimeoutMs: number;

  constructor(options: TaskQueueOptions = {}) {
    this._maxConcurrent = options.maxConcurrent ?? 5;
    this.maxQueueSize = options.maxQueueSize ?? 100;
    this._defaultTimeoutMs = options.defaultTimeoutMs ?? 300000;
  }

  submitTask(action: string, params: Record<string, unknown>): string {
    if (this.tasks.size >= this.maxQueueSize) {
      throw new Error(`Task queue full (max ${this.maxQueueSize})`);
    }
    const id = crypto.randomUUID();
    const task: TaskEntry = {
      id,
      status: 'pending',
      action,
      params,
      createdAt: Date.now(),
    };
    this.tasks.set(id, task);
    log.info(`Task submitted: ${id} (${action})`);
    return id;
  }

  getTaskStatus(taskId: string): TaskEntry | null {
    return this.tasks.get(taskId) ?? null;
  }

  getTaskResult(taskId: string): { status: TaskStatus; result?: unknown; error?: string } | null {
    const task = this.tasks.get(taskId);
    if (!task) return null;
    return {
      status: task.status,
      result: task.status === 'completed' ? task.result : undefined,
      error: task.status === 'failed' ? task.error : undefined,
    };
  }

  startTask(taskId: string): boolean {
    const task = this.tasks.get(taskId);
    if (!task || task.status !== 'pending') return false;
    task.status = 'running';
    task.startedAt = Date.now();
    this.runningCount++;
    return true;
  }

  completeTask(taskId: string, result: unknown): boolean {
    const task = this.tasks.get(taskId);
    if (!task || task.status !== 'running') return false;
    task.status = 'completed';
    task.result = result;
    task.completedAt = Date.now();
    this.runningCount--;
    log.info(`Task completed: ${taskId}`);
    return true;
  }

  failTask(taskId: string, error: string): boolean {
    const task = this.tasks.get(taskId);
    if (!task || task.status !== 'running') return false;
    task.status = 'failed';
    task.error = error;
    task.completedAt = Date.now();
    this.runningCount--;
    log.error(`Task failed: ${taskId} - ${error}`);
    return true;
  }

  cancelTask(taskId: string): boolean {
    const task = this.tasks.get(taskId);
    if (!task || task.status !== 'pending') return false;
    task.status = 'cancelled';
    task.completedAt = Date.now();
    return true;
  }

  getStats(): { total: number; pending: number; running: number; completed: number; failed: number } {
    const stats = { total: 0, pending: 0, running: 0, completed: 0, failed: 0 };
    for (const task of this.tasks.values()) {
      stats.total++;
      if (task.status === 'pending') stats.pending++;
      else if (task.status === 'running') stats.running++;
      else if (task.status === 'completed') stats.completed++;
      else if (task.status === 'failed') stats.failed++;
    }
    return stats;
  }

  get maxConcurrent(): number {
    return this._maxConcurrent;
  }

  get defaultTimeoutMs(): number {
    return this._defaultTimeoutMs;
  }
}

export const taskQueue = new TaskQueue();
