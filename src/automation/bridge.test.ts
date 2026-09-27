import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { AutomationBridge } from './bridge.js';
import { DEFAULT_AUTOMATION_PORT } from '../constants.js';

const hostOf = (options: ConstructorParameters<typeof AutomationBridge>[0]) => new AutomationBridge(options).getStatus().host;

describe('AutomationBridge host validation', () => {
    beforeEach(() => {
        for (const name of ['MCP_AUTOMATION_ALLOW_NON_LOOPBACK', 'MCP_AUTOMATION_HOST', 'MCP_AUTOMATION_WS_HOST', 'MCP_AUTOMATION_CLIENT_HOST',
            'MCP_AUTOMATION_PORT', 'MCP_AUTOMATION_WS_PORT', 'MCP_AUTOMATION_CLIENT_PORT', 'UE_PROJECT_PATH']) vi.stubEnv(name, undefined);
    });

    afterEach(() => {
        vi.unstubAllEnvs();
    });

    // Loopback is always allowed (normalized); anything else falls back to
    // loopback unless non-loopback access is opted into.
    it.each([
        ['127.0.0.1', '127.0.0.1'],
        ['localhost', '127.0.0.1'],
        ['LOCALHOST', '127.0.0.1'],
        ['::1', '::1'],
        ['[::1]', '::1'],
        ['  127.0.0.1  ', '127.0.0.1'],
        ['', '127.0.0.1'],
        ['   ', '127.0.0.1'],
        [null, '127.0.0.1'],
        [undefined, '127.0.0.1'],
        ['0.0.0.0', '127.0.0.1'],
        ['192.168.1.100', '127.0.0.1'],
        ['8.8.8.8', '127.0.0.1'],
        ['::', '127.0.0.1'],
        ['fe80::1', '127.0.0.1'],
    ])('host %j resolves to %s by default', (host, expected) => {
        expect(hostOf({ host, port: 8091 })).toBe(expected);
    });

    it.each([
        ['true', '0.0.0.0', '0.0.0.0'],
        ['TRUE', '192.168.1.50', '192.168.1.50'],
        ['true', '::', '::'],
        ['false', '0.0.0.0', '127.0.0.1'],
        ['', '0.0.0.0', '127.0.0.1'],
    ])('MCP_AUTOMATION_ALLOW_NON_LOOPBACK=%j maps %s to %s', (flag, host, expected) => {
        vi.stubEnv('MCP_AUTOMATION_ALLOW_NON_LOOPBACK', flag);
        expect(hostOf({ host, port: 8091 })).toBe(expected);
    });

    it('lets the allowNonLoopback option override the env var', () => {
        vi.stubEnv('MCP_AUTOMATION_ALLOW_NON_LOOPBACK', 'false');
        expect(hostOf({ host: '0.0.0.0', port: 8091, allowNonLoopback: true })).toBe('0.0.0.0');
    });

    it('keeps an IPv6 zone ID', () => {
        expect(hostOf({ host: 'fe80::1%eth0', port: 8091, allowNonLoopback: true })).toBe('fe80::1%eth0');
    });

    it.each(['8092abc', '0x1f9b'])('rejects the malformed env port %s', (port) => {
        vi.stubEnv('MCP_AUTOMATION_WS_PORT', port);
        expect(new AutomationBridge({}).getStatus().port).toBe(DEFAULT_AUTOMATION_PORT);
    });

    it('accepts MCP_AUTOMATION_PORT and prefers the websocket-specific port', () => {
        vi.stubEnv('MCP_AUTOMATION_PORT', '8097');
        expect(new AutomationBridge({}).getStatus().port).toBe(8097);
        vi.stubEnv('MCP_AUTOMATION_WS_PORT', '8098');
        expect(new AutomationBridge({}).getStatus().port).toBe(8098);
    });

    it('dials the resolved IPv6 host from an option or MCP_AUTOMATION_WS_HOST', () => {
        expect(new AutomationBridge({ host: '::1', port: 8098, enabled: false }).getClientUrl()).toBe('ws://[::1]:8098');
        vi.stubEnv('MCP_AUTOMATION_WS_HOST', '::1');
        const fromEnv = new AutomationBridge({ port: 8098, enabled: false });
        expect(fromEnv.getStatus().host).toBe('::1');
        expect(fromEnv.getClientUrl()).toBe('ws://[::1]:8098');
    });
});
