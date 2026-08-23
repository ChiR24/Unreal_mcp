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
        const { requestId, promise } = tracker.createRequest({
            action: 'manage_sequence',
            payload: {
                action: 'create_master_sequence',
                subAction: 'create_master_sequence'
            },
            timeoutMs: 10000
        });

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
        const { requestId, promise } = tracker.createRequest({
            action: 'control_actor',
            payload: {
                action: 'spawn'
            },
            timeoutMs: 10000
        });

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
        expect(events).toEqual([
            {
                type: 'automation_event',
                event: 'asset_saved',
                requestId: 'orphan-request',
                message: 'Saved /Game/Maps/Arena',
                payload: { assetPath: '/Game/Maps/Arena' }
            }
        ]);
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
});
