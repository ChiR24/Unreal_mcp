import { describe, expect, it } from 'vitest';
import { PACKAGE } from '../constants.js';
import { automationMessageSchema, cancelRequestSchema, pluginVersionMismatch, readBridgeAuthority } from './message-schema.js';

const LIVE_REVISIONS = {
    selection: 2,
    level: 3,
    assetRegistry: 4,
    package: 5
} as const;

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

    it('parses the exact live revision snapshot on an automation response', () => {
        const parsed = automationMessageSchema.parse({
            type: 'automation_response',
            requestId: 'revision-response',
            success: true,
            liveRevisions: LIVE_REVISIONS
        });

        expect(parsed.liveRevisions).toEqual(LIVE_REVISIONS);
    });

    it('rejects an incomplete or extended live revision snapshot', () => {
        expect(automationMessageSchema.safeParse({
            type: 'automation_response',
            requestId: 'missing-package',
            liveRevisions: { selection: 2, level: 3, assetRegistry: 4 }
        }).success).toBe(false);
        expect(automationMessageSchema.safeParse({
            type: 'automation_response',
            requestId: 'extra-key',
            liveRevisions: { ...LIVE_REVISIONS, futureState: 6 }
        }).success).toBe(false);
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

describe('pluginVersionMismatch', () => {
    it('is silent when the plugin is this release', () => {
        expect(pluginVersionMismatch({ pluginVersion: PACKAGE.version })).toBeUndefined();
    });

    it('names both versions, the matching plugin release and the server pin', () => {
        const text = pluginVersionMismatch({ pluginVersion: '0.0.1' }) ?? '';
        expect(text).toContain(`${PACKAGE.name} ${PACKAGE.version}`);
        expect(text).toContain('McpAutomationBridge 0.0.1');
        expect(text).toContain(`https://github.com/ChiR24/Unreal_mcp/releases/tag/v${PACKAGE.version}`);
        expect(text).toContain(`npx -y ${PACKAGE.name}@0.0.1`);
    });

    it('treats a plugin that sends no version as older than this server', () => {
        const text = pluginVersionMismatch({ type: 'bridge_ack' }) ?? '';
        expect(text).toContain('predates version reporting');
        expect(text).not.toContain('npx');
    });
});
