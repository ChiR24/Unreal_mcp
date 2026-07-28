// src/server/mcp-primitives/progress/progress-sink-registry.ts
// Task 44: routes upstream progress to the ONE in-flight request that asked.
//
// Progress observations come back from Unreal keyed by the canonical MCP
// request id (`num:1` / `str:1`, already collision-free across the string and
// number id spaces). This table maps that id to the reporter holding THAT
// client's token, which is what keeps one request's progress — and one
// request's token — from ever reaching another concurrent request.
//
// Bounded by construction: `register` evicts oldest-first at the cap, so a
// client that opens requests faster than they settle cannot grow the table
// without limit. Lookups for an unknown id are a silent no-op, so a frame that
// arrives after its request settled is dropped rather than throwing inside the
// transport or resurrecting a closed reporter.

import type { ProgressReporter, ProgressUpdate } from './progress-reporter.js';

/** Maximum concurrently tracked in-flight requests. */
export const MAX_TRACKED_PROGRESS_REQUESTS = 256;

export interface ProgressSinkRegistryOptions {
  readonly max?: number;
}

export class ProgressSinkRegistry {
  private readonly sinks = new Map<string, ProgressReporter>();
  private readonly max: number;

  constructor(options: ProgressSinkRegistryOptions = {}) {
    this.max = options.max ?? MAX_TRACKED_PROGRESS_REQUESTS;
  }

  register(mcpRequestId: string, reporter: ProgressReporter): void {
    if (mcpRequestId.length === 0) return;

    this.sinks.delete(mcpRequestId);
    // Map iteration is insertion-ordered, so the first key is the oldest
    // tracked request and eviction is deterministic rather than arbitrary.
    while (this.sinks.size >= this.max) {
      const oldest = this.sinks.keys().next();
      if (oldest.done === true) break;
      this.sinks.delete(oldest.value);
    }
    this.sinks.set(mcpRequestId, reporter);
  }

  report(mcpRequestId: string, update: ProgressUpdate): void {
    this.sinks.get(mcpRequestId)?.report(update);
  }

  unregister(mcpRequestId: string): void {
    this.sinks.delete(mcpRequestId);
  }

  /**
   * End the stream for a request the client gave up on. Distinct from
   * `unregister`: it also drives the reporter to its terminal state, so an
   * update already resolved elsewhere cannot re-open the stream afterwards.
   */
  close(mcpRequestId: string): void {
    this.sinks.get(mcpRequestId)?.close();
    this.sinks.delete(mcpRequestId);
  }

  /**
   * Drain on shutdown. Every tracked reporter is driven terminal before the
   * table is emptied: dropping the references alone would leave live reporters
   * still holding their request's notify closure, free to emit onto a transport
   * that is closing.
   */
  clear(): void {
    for (const reporter of this.sinks.values()) reporter.close();
    this.sinks.clear();
  }

  get size(): number {
    return this.sinks.size;
  }
}
