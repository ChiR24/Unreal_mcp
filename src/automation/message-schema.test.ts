import { describe, expect, it } from 'vitest';
import { automationMessageSchema, cancelRequestSchema, readBridgeAuthority } from './message-schema.js';

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

    it('captures the additive bridge_ack authority descriptor and strips secrets/unknowns', () => {
        const parsed = automationMessageSchema.parse({
            type: 'bridge_ack',
            authority: {
                profile: 'scoped:reader',
                scopes: ['read'],
                deprecated: false,
                tokenRequired: true,
                pathRestricted: true,
                projectRestricted: false,
                capabilityToken: 'super-secret',
                allowedPathPrefixes: ['/Game/'],
                maxRequestsPerMinute: 60
            }
        }) as { authority: Record<string, unknown> };

        expect(parsed.authority).toEqual({
            profile: 'scoped:reader',
            scopes: ['read'],
            deprecated: false,
            tokenRequired: true,
            pathRestricted: true,
            projectRestricted: false
        });
        expect(parsed.authority.capabilityToken).toBeUndefined();
        expect(parsed.authority.allowedPathPrefixes).toBeUndefined();
        expect(parsed.authority.maxRequestsPerMinute).toBeUndefined();
    });

    it('accepts a bridge_ack with no authority descriptor (old plugin)', () => {
        const parsed = automationMessageSchema.parse({ type: 'bridge_ack' }) as { authority?: unknown };
        expect(parsed.authority).toBeUndefined();
    });
});

describe('readBridgeAuthority', () => {
    it('returns the stripped authority descriptor from handshake metadata', () => {
        expect(
            readBridgeAuthority({ authority: { profile: 'legacy', scopes: ['admin'], deprecated: true, capabilityToken: 'x' } })
        ).toEqual({ profile: 'legacy', scopes: ['admin'], deprecated: true });
    });

    it('returns undefined when metadata or authority is absent (old plugin)', () => {
        expect(readBridgeAuthority(undefined)).toBeUndefined();
        expect(readBridgeAuthority({})).toBeUndefined();
    });

    it('returns undefined for a malformed authority value', () => {
        expect(readBridgeAuthority({ authority: 'not-an-object' })).toBeUndefined();
    });
});
