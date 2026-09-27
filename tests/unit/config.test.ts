import { describe, expect, it, vi } from 'vitest';
import { EnvSchema, requestTimeoutOverrideMs } from '../../src/config.js';

describe('EnvSchema env var defaults', () => {
    it('parse({}) returns the documented defaults', () => {
        const result = EnvSchema.parse({});
        expect(result.NODE_ENV).toBe('development');
        expect(result.MCP_CONNECTION_TIMEOUT_MS).toBe(5000);
        expect(result.MCP_ADDITIONAL_PATH_PREFIXES).toBe('');
    });

    it('respects user-set number strings', () => {
        expect(EnvSchema.parse({ MCP_CONNECTION_TIMEOUT_MS: '1000' }).MCP_CONNECTION_TIMEOUT_MS).toBe(1000);
    });

    it('maps the legacy connection-timeout alias, and the canonical name wins', () => {
        expect(EnvSchema.parse({ UNREAL_CONNECTION_TIMEOUT: '7000' }).MCP_CONNECTION_TIMEOUT_MS).toBe(7000);
        expect(EnvSchema.parse({ MCP_CONNECTION_TIMEOUT_MS: '8000', UNREAL_CONNECTION_TIMEOUT: '7000' }).MCP_CONNECTION_TIMEOUT_MS).toBe(8000);
    });

    it('reads the request-timeout pin, its alias, and prefers the canonical name', () => {
        expect(requestTimeoutOverrideMs({})).toBeUndefined();
        expect(requestTimeoutOverrideMs({ MCP_REQUEST_TIMEOUT_MS: '60000' })).toBe(60000);
        expect(requestTimeoutOverrideMs({ MCP_AUTOMATION_REQUEST_TIMEOUT_MS: '90000' })).toBe(90000);
        expect(requestTimeoutOverrideMs({ MCP_REQUEST_TIMEOUT_MS: '100000', MCP_AUTOMATION_REQUEST_TIMEOUT_MS: '90000' })).toBe(100000);
    });

    it.each(['abc', '8092abc', '0x1f9b', '-1', '0', '1000.5'])('treats a malformed pin %j as absent', (value) => {
        expect(requestTimeoutOverrideMs({ MCP_REQUEST_TIMEOUT_MS: value })).toBeUndefined();
    });

    it('deduplicates and validates additional path prefixes', async () => {
        vi.stubEnv('MCP_ADDITIONAL_PATH_PREFIXES', 'ProjectObject,/ProjectObject/,../Bad,/Plugin//Bad,/ProjectAnimation');
        vi.resetModules();
        const mod = await import('../../src/config.js');
        expect(mod.getAdditionalPathPrefixes()).toEqual(['/ProjectObject/', '/ProjectAnimation/']);
        vi.unstubAllEnvs();
        vi.resetModules();
    });
});
