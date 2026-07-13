import { describe, expect, it } from 'vitest';
import { automationMessageSchema, cancelRequestSchema } from './message-schema.js';

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

    it('accepts a cancel_request frame carrying an automation request id', () => {
        const message = {
            type: 'cancel_request',
            requestId: 'f47ac10b-58cc-4372-a567-0e02b2c3d479',
            reason: 'client cancelled'
        };
        expect(automationMessageSchema.parse(message)).toEqual(message);
        expect(cancelRequestSchema.parse(message)).toEqual(message);
    });

    it('rejects a cancel_request frame with an empty requestId', () => {
        expect(() => automationMessageSchema.parse({
            type: 'cancel_request',
            requestId: ''
        })).toThrow();
    });
});
