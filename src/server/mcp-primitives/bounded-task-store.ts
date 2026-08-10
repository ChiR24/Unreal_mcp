// src/server/mcp-primitives/bounded-task-store.ts
// Task 44: the per-session BOUNDED task store behind MCP Tasks (2025-11-25).
//
// This implements the MCP SDK's own `TaskStore` contract rather than a parallel
// one, so `new Server(info, { taskStore })` auto-registers the real
// `tasks/get|list|cancel|result` handlers against it — the store is reachable
// from the wire, not a shelf ornament that only a unit test can see.
//
// It deliberately does NOT reuse the SDK's `InMemoryTaskStore`, which is
// unbounded, drives expiry off real `setTimeout` timers, and ignores `sessionId`
// entirely (its parameters are literally named `_sessionId`). Three properties
// this server needs are therefore absent from it:
//   * a hard cap, so a client cannot grow server memory without limit;
//   * a clock that a test can advance, so expiry is provable without sleeping;
//   * session isolation, so one session cannot read, cancel, or evict another's
//     task — the security-shaped property of a multi-session transport.
//
// Native mirror: Private/MCP/Primitives/McpTaskStore.{h,cpp}. The two must agree
// on cap semantics, eviction order, TTL clamping, and the terminal-state rule.

import { randomBytes } from 'node:crypto';
import type { Request, RequestId, Result, Task } from '@modelcontextprotocol/sdk/types.js';
import type { CreateTaskOptions, TaskStore } from '@modelcontextprotocol/sdk/experimental/tasks/interfaces.js';

/** The statuses a task can never leave. Mirrors the SDK's `isTerminal`. */
const TERMINAL_STATUSES: ReadonlySet<Task['status']> = new Set<Task['status']>([
  'completed',
  'failed',
  'cancelled',
]);

export function isTerminalStatus(status: Task['status']): boolean {
  return TERMINAL_STATUSES.has(status);
}

/**
 * A task this store issued. `ttl` narrows the SDK's `number | null` to `number`:
 * a bounded store never grants the unlimited lifetime a `null` TTL asks for, so
 * the unlimited case is made unrepresentable rather than merely undocumented.
 */
export type BoundedTask = Omit<Task, 'ttl'> & { readonly ttl: number };

export interface BoundedTaskStoreOptions {
  /** Hard per-session ceiling. Never exceeded, for any session, ever. */
  readonly maxTasksPerSession: number;
  /** TTL applied when the requestor asks for none. */
  readonly defaultTtlMs: number;
  /** Ceiling a requested TTL is clamped to; also what a `null` TTL becomes. */
  readonly maxTtlMs: number;
  /** Injected clock. Tests advance a fake one; production passes `Date.now`. */
  readonly now: () => number;
}

export const TASK_STORE_DEFAULTS = {
  maxTasksPerSession: 32,
  defaultTtlMs: 5 * 60_000,
  maxTtlMs: 30 * 60_000,
} as const;

/**
 * Refused because the session is full of tasks that are still running.
 *
 * `code` must be a JSON-RPC integer: the SDK maps a thrown error with
 * `Number.isSafeInteger(error.code)` onto the wire and otherwise substitutes
 * InternalError, so the string code this used to carry turned an actionable
 * capacity refusal into an opaque -32603. The string survives as `reason` for
 * callers that switch on it.
 */
export class TaskStoreCapacityError extends Error {
  /** JSON-RPC InvalidRequest: the request cannot be served in this state. */
  readonly code = -32600;
  readonly reason = 'TASK_STORE_AT_CAPACITY';
  constructor(limit: number) {
    super(
      `Task store is at its per-session capacity of ${limit} and every retained task is still running. ` +
        'Wait for a task to settle, cancel one, or retry after a result expires.',
    );
    this.name = 'TaskStoreCapacityError';
    Object.setPrototypeOf(this, TaskStoreCapacityError.prototype);
  }
}

/**
 * No such task IN THIS SESSION. A task owned by a different session is
 * reported exactly the same way as one that never existed, so probing cannot
 * be used to learn that another session's task id is live.
 */
export class TaskStoreNotFoundError extends Error {
  readonly code = 'TASK_NOT_FOUND';
  constructor(taskId: string) {
    super(`Task not found: ${taskId}`);
    this.name = 'TaskStoreNotFoundError';
    Object.setPrototypeOf(this, TaskStoreNotFoundError.prototype);
  }
}

/** A second terminal transition was attempted. Exactly one is allowed. */
export class TaskStoreTerminalError extends Error {
  readonly code = 'TASK_ALREADY_TERMINAL';
  constructor(taskId: string, from: Task['status'], to: Task['status']) {
    super(
      `Task ${taskId} is already in terminal status '${from}' and cannot transition to '${to}'. ` +
        'A terminal result is published once and never replaced.',
    );
    this.name = 'TaskStoreTerminalError';
    Object.setPrototypeOf(this, TaskStoreTerminalError.prototype);
  }
}

interface StoredTask {
  /** Monotonic across the whole store; the ONLY eviction order that is used. */
  readonly seq: number;
  readonly expiresAt: number;
  task: BoundedTask;
  result?: Result;
}

const LIST_PAGE_SIZE = 50;

/**
 * Partition key. Named sessions are prefixed so no client-chosen session id can
 * ever collide with the anonymous partition (a stdio server has no session id).
 */
function partitionKey(sessionId: string | undefined): string {
  return sessionId === undefined ? 'anonymous' : `session:${sessionId}`;
}

function newTaskId(): string {
  return randomBytes(16).toString('hex');
}

export class BoundedTaskStore implements TaskStore {
  private readonly options: BoundedTaskStoreOptions;
  /** partition key -> (taskId -> entry). Insertion order is the eviction order. */
  private readonly partitions = new Map<string, Map<string, StoredTask>>();
  private nextSeq = 0;

  constructor(options: Partial<BoundedTaskStoreOptions> = {}) {
    this.options = {
      maxTasksPerSession: options.maxTasksPerSession ?? TASK_STORE_DEFAULTS.maxTasksPerSession,
      defaultTtlMs: options.defaultTtlMs ?? TASK_STORE_DEFAULTS.defaultTtlMs,
      maxTtlMs: options.maxTtlMs ?? TASK_STORE_DEFAULTS.maxTtlMs,
      now: options.now ?? Date.now,
    };
  }

  // --- TaskStore contract -------------------------------------------------

  async createTask(
    taskParams: CreateTaskOptions,
    _requestId: RequestId,
    _request: Request,
    sessionId?: string,
  ): Promise<Task> {
    const partition = this.partitionFor(sessionId, true);
    this.evictExpired(partition);
    this.makeRoom(partition);

    const now = this.options.now();
    const ttl = this.clampTtl(taskParams.ttl);
    const timestamp = new Date(now).toISOString();
    const task: BoundedTask = {
      taskId: newTaskId(),
      status: 'working',
      ttl,
      createdAt: timestamp,
      lastUpdatedAt: timestamp,
      ...(typeof taskParams.pollInterval === 'number' ? { pollInterval: taskParams.pollInterval } : {}),
    };
    partition.set(task.taskId, { seq: this.nextSeq++, expiresAt: now + ttl, task });
    return task;
  }

  async getTask(taskId: string, sessionId?: string): Promise<Task | null> {
    return this.liveEntry(taskId, sessionId)?.task ?? null;
  }

  async storeTaskResult(
    taskId: string,
    status: 'completed' | 'failed',
    result: Result,
    sessionId?: string,
  ): Promise<void> {
    const entry = this.requireLiveEntry(taskId, sessionId);
    this.assertNotTerminal(entry, status);
    entry.result = result;
    entry.task = { ...entry.task, status, lastUpdatedAt: this.stamp() };
  }

  async getTaskResult(taskId: string, sessionId?: string): Promise<Result> {
    const entry = this.requireLiveEntry(taskId, sessionId);
    // A result exists only once the task has settled; asking earlier is not a
    // "wait", it is a request for something that does not exist yet.
    if (entry.result === undefined) throw new TaskStoreNotFoundError(taskId);
    return entry.result;
  }

  async updateTaskStatus(
    taskId: string,
    status: Task['status'],
    statusMessage?: string,
    sessionId?: string,
  ): Promise<void> {
    const entry = this.requireLiveEntry(taskId, sessionId);
    this.assertNotTerminal(entry, status);
    entry.task = {
      ...entry.task,
      status,
      lastUpdatedAt: this.stamp(),
      ...(statusMessage === undefined ? {} : { statusMessage }),
    };
  }

  async listTasks(cursor?: string, sessionId?: string): Promise<{ tasks: Task[]; nextCursor?: string }> {
    const partition = this.partitionFor(sessionId, false);
    if (!partition) return { tasks: [] };
    this.evictExpired(partition);

    const ordered = [...partition.values()].sort((left, right) => left.seq - right.seq);
    const after = cursor === undefined ? -1 : Number.parseInt(cursor, 10);
    const remaining = ordered.filter((entry) => entry.seq > (Number.isNaN(after) ? -1 : after));
    const page = remaining.slice(0, LIST_PAGE_SIZE);
    const last = page.at(-1);
    const hasMore = last !== undefined && remaining.length > page.length;
    return {
      tasks: page.map((entry) => entry.task),
      ...(hasMore ? { nextCursor: String(last.seq) } : {}),
    };
  }

  // --- Lifecycle beyond the SDK contract -----------------------------------

  /** Drop every task for one session; used on disconnect. Other sessions keep theirs. */
  closeSession(sessionId: string | undefined): void {
    this.partitions.delete(partitionKey(sessionId));
  }

  /** Drop everything; used on shutdown so no retained task or result survives. */
  clear(): void {
    this.partitions.clear();
  }

  /** Live (non-expired) task count for one session. */
  sessionSize(sessionId: string | undefined): number {
    const partition = this.partitionFor(sessionId, false);
    if (!partition) return 0;
    this.evictExpired(partition);
    return partition.size;
  }

  /** Live task count across every session. */
  totalSize(): number {
    let total = 0;
    for (const partition of this.partitions.values()) {
      this.evictExpired(partition);
      total += partition.size;
    }
    return total;
  }

  // --- Internals ------------------------------------------------------------

  private stamp(): string {
    return new Date(this.options.now()).toISOString();
  }

  private clampTtl(requested: number | null | undefined): number {
    // `null` means "unlimited" in the SDK contract. A bounded store cannot offer
    // that, and the contract explicitly allows the implementation to override
    // the request as long as the applied TTL is what it reports back.
    if (requested === null || requested === undefined) {
      return requested === null ? this.options.maxTtlMs : this.options.defaultTtlMs;
    }
    if (!Number.isFinite(requested) || requested <= 0) return this.options.defaultTtlMs;
    return Math.min(requested, this.options.maxTtlMs);
  }

  private partitionFor(sessionId: string | undefined, create: true): Map<string, StoredTask>;
  private partitionFor(sessionId: string | undefined, create: false): Map<string, StoredTask> | undefined;
  private partitionFor(sessionId: string | undefined, create: boolean): Map<string, StoredTask> | undefined {
    const key = partitionKey(sessionId);
    const existing = this.partitions.get(key);
    if (existing || !create) return existing;
    const created = new Map<string, StoredTask>();
    this.partitions.set(key, created);
    return created;
  }

  /**
   * Session-scoped lookup. The task id is only ever resolved INSIDE the caller's
   * own partition, so isolation is structural: there is no global id map that a
   * missing session check could accidentally read through.
   */
  private liveEntry(taskId: string, sessionId: string | undefined): StoredTask | undefined {
    const partition = this.partitionFor(sessionId, false);
    if (!partition) return undefined;
    this.evictExpired(partition);
    return partition.get(taskId);
  }

  private requireLiveEntry(taskId: string, sessionId: string | undefined): StoredTask {
    const entry = this.liveEntry(taskId, sessionId);
    if (!entry) throw new TaskStoreNotFoundError(taskId);
    return entry;
  }

  private assertNotTerminal(entry: StoredTask, next: Task['status']): void {
    if (isTerminalStatus(entry.task.status)) {
      throw new TaskStoreTerminalError(entry.task.taskId, entry.task.status, next);
    }
  }

  /** TTL is measured from creation and applies regardless of status. */
  private evictExpired(partition: Map<string, StoredTask>): void {
    const now = this.options.now();
    for (const [taskId, entry] of partition) {
      if (entry.expiresAt <= now) partition.delete(taskId);
    }
  }

  /**
   * Make one slot available. ONLY a terminal task is ever evicted: dropping a
   * still-running task would make a live handle vanish from under a polling
   * client and silently discard a result that was still going to be produced.
   * When nothing is terminal the creation is refused instead — fail-closed.
   */
  private makeRoom(partition: Map<string, StoredTask>): void {
    const limit = this.options.maxTasksPerSession;
    while (partition.size >= limit) {
      let oldest: { taskId: string; seq: number } | undefined;
      for (const [taskId, entry] of partition) {
        if (!isTerminalStatus(entry.task.status)) continue;
        if (oldest === undefined || entry.seq < oldest.seq) oldest = { taskId, seq: entry.seq };
      }
      if (oldest === undefined) throw new TaskStoreCapacityError(limit);
      partition.delete(oldest.taskId);
    }
  }
}
