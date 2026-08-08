import { describe, expect, it } from 'vitest';
import { automationMessageSchema } from './message-schema.js';

describe('automationMessageSchema', () => {
    it('preserves unknown top-level bridge payload fields', () => {
        const message = {
            type: 'bridge_ack',
            serverName: 'UnrealMCP',
            heartbeatIntervalMs: 15000,
            futureCapability: {
                modes: ['native', 'bridge']
            }
        };

        expect(automationMessageSchema.parse(message)).toEqual(message);
    });

    it('rejects negative bridge heartbeat intervals', () => {
        expect(() => automationMessageSchema.parse({
            type: 'bridge_ack',
            heartbeatIntervalMs: -1
        })).toThrow();
    });

    it('rejects fractional protocol versions', () => {
        expect(() => automationMessageSchema.parse({
            type: 'bridge_ack',
            protocolVersion: 1.5
        })).toThrow();
    });

    it('accepts protocol v2 correlated diagnostics', () => {
        expect(automationMessageSchema.parse({
            type: 'automation_event',
            event: 'blueprint_exception',
            sequence: 12,
            timestamp: '2026-07-28T00:00:00.000Z',
            context: { traceId: 'trace-1', debugSessionId: 'session-1', frame: 42 },
            diagnostic: {
                code: 'BLUEPRINT_EXCEPTION',
                severity: 'error',
                component: 'unreal_bridge',
                phase: 'runtime',
                retriable: false,
                message: 'Accessed None'
            }
        })).toMatchObject({ event: 'blueprint_exception', sequence: 12 });
    });
});
