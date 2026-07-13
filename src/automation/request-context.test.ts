import { describe, expect, it } from 'vitest';
import {
    canonicalizeMcpRequestId,
    getMcpRequestContext,
    runWithMcpRequestContext,
} from './request-context.js';

describe('canonicalizeMcpRequestId', () => {
    it('namespaces by type so string and number ids never collide', () => {
        expect(canonicalizeMcpRequestId('1')).toBe('str:1');
        expect(canonicalizeMcpRequestId(1)).toBe('num:1');
        expect(canonicalizeMcpRequestId('1')).not.toBe(canonicalizeMcpRequestId(1));
    });

    it('keeps distinct ids distinct even with colliding text', () => {
        expect(canonicalizeMcpRequestId('num:5')).toBe('str:num:5');
        expect(canonicalizeMcpRequestId(5)).toBe('num:5');
        expect(canonicalizeMcpRequestId('num:5')).not.toBe(canonicalizeMcpRequestId(5));
    });

    it('is stable for the same input', () => {
        expect(canonicalizeMcpRequestId(42)).toBe(canonicalizeMcpRequestId(42));
        expect(canonicalizeMcpRequestId('abc')).toBe(canonicalizeMcpRequestId('abc'));
    });
});

describe('McpRequestContext async-local storage', () => {
    it('is empty outside a request context', () => {
        expect(getMcpRequestContext()).toBeUndefined();
    });

    it('is readable inside runWithMcpRequestContext and cleared after', () => {
        const seen: Array<string | undefined> = [];
        runWithMcpRequestContext({ requestId: 'mcp:7', signal: undefined }, () => {
            seen.push(getMcpRequestContext()?.requestId);
        });
        seen.push(getMcpRequestContext()?.requestId);
        expect(seen).toEqual(['mcp:7', undefined]);
    });

    it('propagates through async boundaries', async () => {
        const seen = await runWithMcpRequestContext({ requestId: 'mcp:9', signal: undefined }, async () => {
            await Promise.resolve();
            return getMcpRequestContext()?.requestId;
        });
        expect(seen).toBe('mcp:9');
    });
});
