// Task 44 lane A — the upstream half: Unreal's `progress_update` frames must
// actually reach the MCP request that owns them.
//
// Before this task the automation layer consumed `progress_update` purely to
// extend the request timeout and then DROPPED it, so a long editor operation
// produced progress that no client ever saw. These cases pin the forwarding and
// the identity resolution that decides which request it belongs to.

import { describe, expect, it, vi } from 'vitest';
import { MessageHandler } from '../../../src/automation/message-handler.js';
import { RequestCorrelation } from '../../../src/automation/request-correlation.js';
import type { RequestTracker } from '../../../src/automation/request-tracker.js';
import type {
  AutomationProgressUpdate,
  PendingRequest,
  ProgressUpdateMessage,
} from '../../../src/automation/types.js';

function trackerWithPending(present: boolean, extendResult = true) {
  const pending = { action: 'build_lighting' } as PendingRequest;
  return {
    getPendingRequest: vi.fn(() => (present ? pending : undefined)),
    extendTimeout: vi.fn(() => extendResult),
  } as unknown as RequestTracker;
}

function frame(overrides: Partial<ProgressUpdateMessage> = {}): ProgressUpdateMessage {
  return {
    type: 'progress_update',
    requestId: 'auto-1',
    percent: 42,
    ...overrides,
  } as ProgressUpdateMessage;
}

function handle(
  message: ProgressUpdateMessage,
  tracker: RequestTracker,
): Array<{ requestId: string; update: AutomationProgressUpdate }> {
  const seen: Array<{ requestId: string; update: AutomationProgressUpdate }> = [];
  const handler = new MessageHandler(tracker, undefined, (requestId, update) => {
    seen.push({ requestId, update });
  });
  handler.handleMessage(message);
  return seen;
}

describe('Task 44 — Unreal progress reaches the MCP layer instead of being dropped', () => {
  it('forwards a percent update for a live request', () => {
    const seen = handle(frame({ percent: 42, message: 'Building' }), trackerWithPending(true));

    expect(seen).toHaveLength(1);
    expect(seen[0]?.requestId).toBe('auto-1');
    expect(seen[0]?.update.progress).toBe(42);
    expect(seen[0]?.update.total).toBe(100);
    expect(seen[0]?.update.message).toBe('Building');
  });

  it('forwards even when the timeout extension is refused', () => {
    // A refused extension is a deadlock signal, not a reason to also deny the
    // client the progress Unreal already produced.
    const seen = handle(frame(), trackerWithPending(true, false));

    expect(seen).toHaveLength(1);
  });

  it('omits the message field when Unreal sent none', () => {
    const seen = handle(frame({ percent: 5 }), trackerWithPending(true));

    expect(seen[0]?.update.message).toBeUndefined();
  });

  it('forwards nothing when there is no matching pending request', () => {
    const seen = handle(frame(), trackerWithPending(false));

    expect(seen).toEqual([]);
  });

  it('forwards nothing when the frame carries no percent', () => {
    const seen = handle(frame({ percent: undefined }), trackerWithPending(true));

    expect(seen).toEqual([]);
  });

  it('forwards nothing for a non-finite percent', () => {
    const seen = handle(frame({ percent: Number.NaN }), trackerWithPending(true));

    expect(seen).toEqual([]);
  });
});

describe('Task 44 — progress resolves to the requests that actually own it', () => {
  const noop = () => undefined;

  it('maps an automation id to its subscribing MCP request', () => {
    const correlation = new RequestCorrelation();
    correlation.register('num:1', 'auto-1', noop, noop);

    expect(correlation.mcpRequestIdsForAuto('auto-1')).toEqual(['num:1']);
  });

  it('fans out to every subscriber of a COALESCED automation request', () => {
    const correlation = new RequestCorrelation();
    correlation.register('num:1', 'auto-1', noop, noop);
    correlation.register('str:abc', 'auto-1', noop, noop);

    expect(correlation.mcpRequestIdsForAuto('auto-1').sort()).toEqual(['num:1', 'str:abc']);
  });

  it('deduplicates a request that opened several subscribers on one id', () => {
    const correlation = new RequestCorrelation();
    correlation.register('num:1', 'auto-1', noop, noop);
    correlation.register('num:1', 'auto-1', noop, noop);

    expect(correlation.mcpRequestIdsForAuto('auto-1')).toEqual(['num:1']);
  });

  it('NEVER reports a request subscribed to a different automation id', () => {
    const correlation = new RequestCorrelation();
    correlation.register('num:1', 'auto-1', noop, noop);
    correlation.register('num:2', 'auto-2', noop, noop);

    expect(correlation.mcpRequestIdsForAuto('auto-1')).toEqual(['num:1']);
    expect(correlation.mcpRequestIdsForAuto('auto-2')).toEqual(['num:2']);
  });

  it('reports nobody for an unknown or already-settled automation id', () => {
    const correlation = new RequestCorrelation();
    correlation.register('num:1', 'auto-1', noop, noop);

    expect(correlation.mcpRequestIdsForAuto('auto-unknown')).toEqual([]);

    correlation.settle('auto-1');
    expect(correlation.mcpRequestIdsForAuto('auto-1')).toEqual([]);
  });

  it('skips an anonymous subscriber that has no MCP request id', () => {
    const correlation = new RequestCorrelation();
    correlation.register(undefined, 'auto-1', noop, noop);

    expect(correlation.mcpRequestIdsForAuto('auto-1')).toEqual([]);
  });
});
