import { describe, expect, it } from 'vitest';
import { Logger } from '../utils/logging/logger.js';
import {
    AutomationRequestDispatcher,
    type AutomationRequestDispatcherDependencies,
} from './bridge-request-dispatcher.js';
import { McpRequestCancelledError } from './request-cancellation-error.js';
import { RequestTracker } from './request-tracker.js';
import type { AutomationBridgeMessage } from './types.js';

function createDispatcher(
    tracker: RequestTracker,
    overrides: Partial<AutomationRequestDispatcherDependencies> = {},
) {
    const sent: Array<Record<string, unknown>> = [];
    const deps: AutomationRequestDispatcherDependencies = {
        enabled: true,
        maxQueuedRequests: 5,
        connectionTimeoutMs: 1000,
        requestTracker: tracker,
        log: new Logger('test'),
        isConnected: () => true,
        send: (payload: AutomationBridgeMessage) => {
            sent.push(payload as Record<string, unknown>);
            return true;
        },
        startClient: () => {},
        abortPendingConnection: () => {},
        once: () => {},
        off: () => {},
        ...overrides,
    };
    return { dispatcher: new AutomationRequestDispatcher(deps), sent };
}

const automationRequests = (sent: Array<Record<string, unknown>>) =>
    sent.filter((m) => m.type === 'automation_request');
const cancelRequests = (sent: Array<Record<string, unknown>>) =>
    sent.filter((m) => m.type === 'cancel_request');

describe('AutomationRequestDispatcher cancellation', () => {
    it('correlates one MCP id to multiple spawned automation request ids', () => {
        const { dispatcher, sent } = createDispatcher(new RequestTracker(50));

        void dispatcher.sendAutomationRequest('get_actor', { a: 1 }, { mcpRequestId: 'mcp:1' });
        void dispatcher.sendAutomationRequest('get_actor', { a: 2 }, { mcpRequestId: 'mcp:1' });

        const ids = automationRequests(sent).map((m) => m.requestId as string);
        expect(ids).toHaveLength(2);
        expect(cancelRequests(sent)).toHaveLength(0);
    });

    it('rejects inflight correlated requests and sends a cancel frame per automation id', async () => {
        const { dispatcher, sent } = createDispatcher(new RequestTracker(50));

        const p1 = dispatcher.sendAutomationRequest('get_actor', { a: 1 }, { mcpRequestId: 'mcp:1' });
        const p2 = dispatcher.sendAutomationRequest('get_actor', { a: 2 }, { mcpRequestId: 'mcp:1' });

        const ids = automationRequests(sent).map((m) => m.requestId as string);
        dispatcher.cancelMcpRequest('mcp:1', 'client cancelled');

        await expect(p1).rejects.toBeInstanceOf(McpRequestCancelledError);
        await expect(p2).rejects.toBeInstanceOf(McpRequestCancelledError);

        const cancels = cancelRequests(sent);
        expect(cancels).toHaveLength(2);
        expect(cancels.map((c) => c.requestId).sort()).toEqual(ids.slice().sort());
        expect(cancels.every((c) => c.reason === 'client cancelled')).toBe(true);
    });

    it('rejects a queued (never-sent) request without emitting a cancel frame', async () => {
        const tracker = new RequestTracker(1); // only 1 inflight allowed
        const { dispatcher, sent } = createDispatcher(tracker);

        const inflight = dispatcher.sendAutomationRequest('get_actor', { a: 1 }, { mcpRequestId: 'mcp:0' });
        const queued = dispatcher.sendAutomationRequest('get_actor', { a: 2 }, { mcpRequestId: 'mcp:2' });

        expect(automationRequests(sent)).toHaveLength(1); // only the inflight one was sent

        dispatcher.cancelMcpRequest('mcp:2', 'client cancelled');
        await expect(queued).rejects.toBeInstanceOf(McpRequestCancelledError);
        expect(cancelRequests(sent)).toHaveLength(0);

        // The unrelated inflight request is untouched.
        tracker.resolveRequest(
            automationRequests(sent)[0].requestId as string,
            {
                type: 'automation_response',
                requestId: automationRequests(sent)[0].requestId as string,
                success: true,
            },
        );
        await expect(inflight).resolves.toBeDefined();
    });

    it('is idempotent: a second cancel is a no-op and emits no extra frame', async () => {
        const { dispatcher, sent } = createDispatcher(new RequestTracker(50));

        const p = dispatcher.sendAutomationRequest('get_actor', {}, { mcpRequestId: 'mcp:1' });
        dispatcher.cancelMcpRequest('mcp:1', 'reason');
        dispatcher.cancelMcpRequest('mcp:1', 'reason');
        await expect(p).rejects.toBeInstanceOf(McpRequestCancelledError);

        expect(cancelRequests(sent)).toHaveLength(1);
    });

    it('tears down correlation on resolve so a late cancel is harmless', async () => {
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker);

        const p = dispatcher.sendAutomationRequest('get_actor', {}, { mcpRequestId: 'mcp:1' });
        const autoId = automationRequests(sent)[0].requestId as string;
        tracker.resolveRequest(autoId, { type: 'automation_response', requestId: autoId, success: true });
        await expect(p).resolves.toBeDefined();

        // biome-ignore lint/complexity/useLiteralKeys: test intentionally inspects private cleanup state
        expect(dispatcher['correlation']['byMcp'].size).toBe(0);

        // Cancelling after settlement must not send a frame or throw.
        dispatcher.cancelMcpRequest('mcp:1', 'late');
        expect(cancelRequests(sent)).toHaveLength(0);
    });

    it('is a no-op for an unknown MCP id', () => {
        const { dispatcher, sent } = createDispatcher(new RequestTracker(50));
        expect(() => dispatcher.cancelMcpRequest('mcp:unknown', 'reason')).not.toThrow();
        expect(cancelRequests(sent)).toHaveLength(0);
    });
});

describe('AutomationRequestDispatcher correlation metadata (Task 39)', () => {
    it('stamps the client-facing correlation id onto the outbound automation_request envelope (gateway -> bridge -> queue hop)', () => {
        const { dispatcher, sent } = createDispatcher(new RequestTracker(50));
        void dispatcher.sendAutomationRequest('get_actor', { a: 1 }, { correlationId: 'gw-42' });
        const reqs = automationRequests(sent);
        expect(reqs).toHaveLength(1);
        expect(reqs[0].correlationId).toBe('gw-42');
    });

    it('omits correlationId from the envelope when none is supplied', () => {
        const { dispatcher, sent } = createDispatcher(new RequestTracker(50));
        void dispatcher.sendAutomationRequest('get_actor', { a: 1 }, {});
        const reqs = automationRequests(sent);
        expect(reqs).toHaveLength(1);
        expect('correlationId' in reqs[0]).toBe(false);
    });
});

describe('AutomationRequestDispatcher live-state metadata (Task 42)', () => {
    it('stamps expected revisions onto the outbound automation_request envelope', () => {
        const { dispatcher, sent } = createDispatcher(new RequestTracker(50));

        void dispatcher.sendAutomationRequest(
            'manage_asset',
            { action: 'rename_asset' },
            { expectedRevisions: { selection: 7, package: 11 } },
        );

        expect(automationRequests(sent)[0]?.expectedRevisions).toEqual({ selection: 7, package: 11 });
    });

    it('preserves expected revisions while a request waits in the backpressure queue', async () => {
        const tracker = new RequestTracker(1);
        const { dispatcher, sent } = createDispatcher(tracker);
        const inflight = dispatcher.sendAutomationRequest('inspect', { action: 'get_object_details' });
        const queued = dispatcher.sendAutomationRequest(
            'manage_level',
            { action: 'save_level' },
            { expectedRevisions: { level: 5 } },
        );
        const firstId = automationRequests(sent)[0]?.requestId;
        expect(typeof firstId).toBe('string');
        if (typeof firstId !== 'string') throw new Error('missing first automation request id');

        tracker.resolveRequest(firstId, { type: 'automation_response', requestId: firstId, success: true });
        await inflight;
        await Promise.resolve();

        const requests = automationRequests(sent);
        expect(requests[1]?.expectedRevisions).toEqual({ level: 5 });
        const secondId = requests[1]?.requestId;
        expect(typeof secondId).toBe('string');
        if (typeof secondId !== 'string') throw new Error('missing queued automation request id');
        tracker.resolveRequest(secondId, { type: 'automation_response', requestId: secondId, success: true });
        await queued;
    });
});

describe('AutomationRequestDispatcher disconnect recovery (Task 45)', () => {
    it('never re-sends an inflight mutation when the connection drops', async () => {
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker);

        const inflight = dispatcher.sendAutomationRequest('spawn_actor', { name: 'Hero' });
        expect(automationRequests(sent)).toHaveLength(1);

        dispatcher.rejectPendingRequests(new Error('Connection lost'));

        await expect(inflight).rejects.toThrow('Connection lost');
        expect(automationRequests(sent)).toHaveLength(1);
    });

    it('rejects a queued mutation instead of draining it after the connection drops', async () => {
        const tracker = new RequestTracker(1);
        let connected = true;
        const { dispatcher, sent } = createDispatcher(tracker, { isConnected: () => connected });

        const inflight = dispatcher.sendAutomationRequest('spawn_actor', { name: 'Hero' });
        const queued = dispatcher.sendAutomationRequest('spawn_actor', { name: 'Villain' });
        expect(automationRequests(sent)).toHaveLength(1);

        connected = false;
        const autoId = automationRequests(sent)[0].requestId as string;
        tracker.rejectRequest(autoId, new Error('Connection lost'));

        await expect(inflight).rejects.toThrow('Connection lost');
        await expect(queued).rejects.toThrow('Connection lost');
        expect(automationRequests(sent)).toHaveLength(1);
    });

    it('never re-sends a timed-out mutation', async () => {
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker);

        const inflight = dispatcher.sendAutomationRequest('delete_asset', { path: '/Game/Hero' });
        const autoId = automationRequests(sent)[0].requestId as string;

        tracker.rejectRequest(autoId, new Error(`Request ${autoId} timed out after 30000ms`));

        await expect(inflight).rejects.toThrow('timed out');
        expect(automationRequests(sent)).toHaveLength(1);
    });
});

describe('AutomationRequestDispatcher send-failure and cancellation edge cases', () => {
    it('rejects every queued caller and keeps draining when send() fails during drain', async () => {
        const tracker = new RequestTracker(1);
        let sendCalls = 0;
        const { dispatcher, sent } = createDispatcher(tracker, {
            send: (payload) => {
                if (payload.type !== 'automation_request') return true;
                sendCalls += 1;
                const ok = sendCalls === 1;
                if (ok) sent.push(payload as Record<string, unknown>);
                return ok;
            }
        });

        const inflight = dispatcher.sendAutomationRequest('spawn', { a: 1 }, { mcpRequestId: 'mcp:0' });
        const q1 = dispatcher.sendAutomationRequest('spawn', { a: 2 }, { mcpRequestId: 'mcp:1' });
        const q2 = dispatcher.sendAutomationRequest('spawn', { a: 3 }, { mcpRequestId: 'mcp:2' });

        const autoId = automationRequests(sent)[0].requestId as string;
        tracker.resolveRequest(autoId, { type: 'automation_response', requestId: autoId, success: true });

        await expect(inflight).resolves.toBeDefined();
        await expect(q1).rejects.toBeInstanceOf(Error);
        await expect(q2).rejects.toBeInstanceOf(Error);
    });

    it('does not throw when the cancel frame send() throws', async () => {
        const { dispatcher, sent } = createDispatcher(new RequestTracker(50), {
            send: (payload) => {
                sent.push(payload as Record<string, unknown>);
                if (payload.type === 'cancel_request') throw new Error('delivery boom');
                return true;
            }
        });
        const p1 = dispatcher.sendAutomationRequest('spawn', { a: 1 }, { mcpRequestId: 'mcp:1' });
        const p2 = dispatcher.sendAutomationRequest('spawn', { a: 2 }, { mcpRequestId: 'mcp:1' });

        expect(() => dispatcher.cancelMcpRequest('mcp:1', 'reason')).not.toThrow();
        await expect(p1).rejects.toBeInstanceOf(McpRequestCancelledError);
        await expect(p2).rejects.toBeInstanceOf(McpRequestCancelledError);
    });

    it('attempts all cancel frames when one send() throws', async () => {
        const tracker = new RequestTracker(50);
        let cancelCalls = 0;
        const { dispatcher, sent } = createDispatcher(tracker, {
            send: (payload) => {
                if (payload.type === 'cancel_request') {
                    cancelCalls += 1;
                    if (cancelCalls === 1) throw new Error('boom on first');
                    sent.push(payload as Record<string, unknown>);
                    return true;
                }
                sent.push(payload as Record<string, unknown>);
                return true;
            }
        });
        const p1 = dispatcher.sendAutomationRequest('spawn', { a: 1 }, { mcpRequestId: 'mcp:1' });
        const p2 = dispatcher.sendAutomationRequest('spawn', { a: 2 }, { mcpRequestId: 'mcp:1' });

        expect(() => dispatcher.cancelMcpRequest('mcp:1', 'reason')).not.toThrow();
        await expect(p1).rejects.toBeInstanceOf(McpRequestCancelledError);
        await expect(p2).rejects.toBeInstanceOf(McpRequestCancelledError);
        expect(cancelCalls).toBe(2);
        expect(cancelRequests(sent)).toHaveLength(1);
    });

    it('lets a coalesced follower cancel independently without tearing down the origin subscriber', async () => {
        const tracker = new RequestTracker(50);
        const { dispatcher, sent } = createDispatcher(tracker);

        const p1 = dispatcher.sendAutomationRequest('get_actor', { a: 1 }, { mcpRequestId: 'mcp:1' });
        const p2 = dispatcher.sendAutomationRequest('get_actor', { a: 1 }, { mcpRequestId: 'mcp:2' });

        expect(automationRequests(sent)).toHaveLength(1);

        dispatcher.cancelMcpRequest('mcp:1', 'origin cancelled');
        await expect(p1).rejects.toBeInstanceOf(McpRequestCancelledError);

        const outcome = await Promise.race([
            p2.then(() => 'resolved'),
            new Promise<string>((resolve) => setTimeout(() => resolve('pending'), 50))
        ]);
        expect(outcome).toBe('pending');
        expect(cancelRequests(sent)).toHaveLength(0);

        dispatcher.cancelMcpRequest('mcp:2', 'follower cancelled');
        await expect(p2).rejects.toBeInstanceOf(McpRequestCancelledError);
        expect(cancelRequests(sent)).toHaveLength(1);
    });
});
