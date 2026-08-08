import { describe, expect, it } from 'vitest';
import { RequestTracker } from './request-tracker.js';
import { MessageHandler } from './message-handler.js';

type AutomationEventFixture = {
    readonly type: 'automation_event';
    readonly event: string;
    readonly requestId?: string;
    readonly payload?: unknown;
    readonly result?: unknown;
    readonly message?: string;
};

describe('MessageHandler automation events', () => {
    it('allows manage_sequence responses to echo their native sub-action', async () => {
        const tracker = new RequestTracker(10);
        const handler = new MessageHandler(tracker);
        const { requestId, promise } = tracker.createRequest('manage_sequence', {
            action: 'create_master_sequence',
            subAction: 'create_master_sequence'
        }, 10000);

        handler.handleMessage({
            type: 'automation_response',
            requestId,
            success: true,
            message: 'Master sequence created',
            result: {
                success: true,
                action: 'create_master_sequence',
                sequencePath: '/Game/MCPTest/Cinematics/SEQ_Master'
            }
        });

        const response = await promise;
        expect(response).toMatchObject({
            success: true,
            result: {
                action: 'create_master_sequence',
                sequencePath: '/Game/MCPTest/Cinematics/SEQ_Master'
            }
        });
        expect(response.error).toBeUndefined();
    });

    it('still flags unrelated response action mismatches', async () => {
        const tracker = new RequestTracker(10);
        const handler = new MessageHandler(tracker);
        const { requestId, promise } = tracker.createRequest('control_actor', {
            action: 'spawn'
        }, 10000);

        handler.handleMessage({
            type: 'automation_response',
            requestId,
            success: true,
            message: 'Unexpected success',
            result: {
                success: true,
                action: 'create_master_sequence'
            }
        });

        await expect(promise).resolves.toMatchObject({
            success: false,
            error: 'ACTION_PREFIX_MISMATCH'
        });
    });

    it('emits normalized automation events when no pending request exists', () => {
        // Given
        const events: AutomationEventFixture[] = [];
        const handler = new MessageHandler(
            new RequestTracker(10),
            (event: AutomationEventFixture) => {
                events.push(event);
            }
        );

        // When
        handler.handleMessage({
            type: 'automation_event',
            event: 'asset_saved',
            requestId: 'orphan-request',
            message: 'Saved /Game/Maps/Arena',
            payload: { assetPath: '/Game/Maps/Arena' },
            unexpected: 'not forwarded'
        });

        // Then
        expect(events).toHaveLength(1);
        expect(events[0]).toMatchObject({
                type: 'automation_event',
                event: 'asset_saved',
                requestId: 'orphan-request',
                message: 'Saved /Game/Maps/Arena',
                payload: { assetPath: '/Game/Maps/Arena' }
        });
        expect(Reflect.get(events[0] ?? {}, 'sequence')).toBe(1);
        expect(Reflect.get(events[0] ?? {}, 'timestamp')).toEqual(expect.any(String));
        expect(Reflect.get(events[0] ?? {}, 'context')).toMatchObject({ requestId: 'orphan-request' });
    });

    it('drops malformed automation events without an event name', () => {
        // Given
        const events: AutomationEventFixture[] = [];
        const handler = new MessageHandler(
            new RequestTracker(10),
            (event: AutomationEventFixture) => {
                events.push(event);
            }
        );

        // When
        handler.handleMessage({
            type: 'automation_event',
            message: 'Missing event name',
            payload: { assetPath: '/Game/Maps/Arena' }
        });

        // Then
        expect(events).toEqual([]);
    });

    it('keeps test jobs pending until the completion event', async () => {
        const tracker = new RequestTracker(10);
        const handler = new MessageHandler(tracker);
        const { requestId, promise } = tracker.createRequest('manage_tests', { action: 'run_tests' }, 10000);
        const pending = tracker.getPendingRequest(requestId);
        expect(pending).toBeDefined();
        if (!pending) throw new Error('Expected a pending request');
        pending.waitForEvent = true;

        handler.handleMessage({
            type: 'automation_response', requestId, success: true,
            result: { queued: true }
        });
        handler.handleMessage({
            type: 'automation_event', event: 'automation_test_started', requestId,
            result: { testName: 'Missile.Determinism' }
        });
        expect(tracker.getPendingRequest(requestId)).toBeDefined();

        handler.handleMessage({
            type: 'automation_event', event: 'automation_test_completed', requestId,
            result: { success: false, errors: ['state hash mismatch'] }
        });
        await expect(promise).resolves.toMatchObject({
            success: false,
            result: { errors: ['state hash mismatch'] }
        });
    });
});
