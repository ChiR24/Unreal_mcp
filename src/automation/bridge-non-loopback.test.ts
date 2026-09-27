import { describe, it, expect } from 'vitest';
import { AutomationBridge } from './bridge.js';

// With allowNonLoopback, valid IPs and hostnames pass through (IPv6 brackets
// trimmed); anything malformed falls back to loopback.
describe('AutomationBridge non-loopback host validation', () => {
    it.each([
        ['0.0.0.0', '0.0.0.0'],
        ['192.168.1.100', '192.168.1.100'],
        ['10.0.0.1', '10.0.0.1'],
        ['::', '::'],
        ['fe80::1', 'fe80::1'],
        ['2001:db8::1', '2001:db8::1'],
        ['[fe80::1]', 'fe80::1'],
        ['example.com', 'example.com'],
        ['unreal-server.local', 'unreal-server.local'],
        ['mcp.unreal.internal', 'mcp.unreal.internal'],
        ['dev-pc', 'dev-pc'],
        ['-invalid-hostname', '127.0.0.1'],
        ['256.1.1.1', '127.0.0.1'],
        ['example..com', '127.0.0.1'],
    ])('host %s resolves to %s', (host, expected) => {
        expect(new AutomationBridge({ host, port: 8091, allowNonLoopback: true }).getStatus().host).toBe(expected);
    });
});
