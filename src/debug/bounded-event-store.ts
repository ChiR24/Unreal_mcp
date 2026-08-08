import type { DebugCorrelationContext, DebugEvent } from './types.js';

export interface DebugEventQuery {
  after?: number;
  limit?: number;
  event?: string;
  sessionId?: string;
  requestId?: string;
  severity?: string;
  category?: string;
  regex?: string;
  since?: string;
  until?: string;
}

export class BoundedEventStore {
  private readonly events: DebugEvent[] = [];
  private nextSequence = 1;
  private dropped = 0;

  constructor(private readonly capacity = 10_000) {}

  append(input: {
    event: string;
    context: DebugCorrelationContext;
    payload?: unknown;
    message?: string;
    timestamp?: string;
  }): DebugEvent {
    const stored: DebugEvent = {
      type: 'automation_event',
      event: input.event,
      sequence: this.nextSequence++,
      timestamp: input.timestamp ?? new Date().toISOString(),
      context: { ...input.context },
      ...(input.payload === undefined ? {} : { payload: input.payload }),
      ...(input.message === undefined ? {} : { message: input.message })
    };
    this.events.push(stored);
    if (this.events.length > this.capacity) {
      const overflow = this.events.length - this.capacity;
      this.events.splice(0, overflow);
      this.dropped += overflow;
    }
    return stored;
  }

  query(query: DebugEventQuery = {}): {
    events: DebugEvent[];
    nextCursor: number;
    oldestCursor: number;
    dropped: number;
  } {
    const after = Math.max(0, query.after ?? 0);
    const limit = Math.min(1_000, Math.max(1, query.limit ?? 100));
    const matches = this.matchingEvents(query).slice(0, limit);

    return {
      events: matches,
      nextCursor: matches.at(-1)?.sequence ?? after,
      oldestCursor: this.events.at(0)?.sequence ?? this.nextSequence,
      dropped: this.dropped
    };
  }

  latest(query: Omit<DebugEventQuery, 'limit'> = {}): DebugEvent | undefined {
    return this.matchingEvents(query).at(-1);
  }

  getCursor(): number {
    return this.nextSequence - 1;
  }

  getDroppedCount(): number {
    return this.dropped;
  }

  private matchingEvents(query: Omit<DebugEventQuery, 'limit'>): DebugEvent[] {
    const after = Math.max(0, query.after ?? 0);
    const pattern = query.regex ? new RegExp(query.regex, 'i') : undefined;
    return this.events.filter((entry) => {
      if (entry.sequence <= after) return false;
      if (query.event && entry.event !== query.event) return false;
      if (query.sessionId && entry.context.debugSessionId !== query.sessionId) return false;
      if (query.requestId && entry.context.requestId !== query.requestId) return false;
      if (query.since && entry.timestamp < query.since) return false;
      if (query.until && entry.timestamp > query.until) return false;
      const payload = this.asRecord(entry.payload);
      if (query.severity && payload.severity !== query.severity) return false;
      if (query.category && payload.category !== query.category) return false;
      if (pattern && !pattern.test(`${entry.event} ${entry.message ?? ''} ${JSON.stringify(entry.payload ?? null)}`)) return false;
      return true;
    });
  }

  private asRecord(value: unknown): Record<string, unknown> {
    return typeof value === 'object' && value !== null && !Array.isArray(value)
      ? value as Record<string, unknown>
      : {};
  }
}
