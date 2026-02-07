import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { AutomationBridge } from './bridge.js';

describe('AutomationBridge Host Validation', () => {
    const originalEnv = process.env;

    beforeEach(() => {
        vi.resetModules();
        process.env = { ...originalEnv };
        delete process.env.MCP_AUTOMATION_ALLOW_NON_LOOPBACK;
        delete process.env.MCP_AUTOMATION_HOST;
        delete process.env.MCP_AUTOMATION_WS_HOST;
    });

    afterEach(() => {
        process.env = originalEnv;
    });

    describe('Loopback addresses (always allowed)', () => {
        it('should accept 127.0.0.1 by default', () => {
            const bridge = new AutomationBridge({ host: '127.0.0.1', port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should normalize localhost to 127.0.0.1', () => {
            const bridge = new AutomationBridge({ host: 'localhost', port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should normalize LOCALHOST to 127.0.0.1 (case insensitive)', () => {
            const bridge = new AutomationBridge({ host: 'LOCALHOST', port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should accept IPv6 loopback ::1', () => {
            const bridge = new AutomationBridge({ host: '::1', port: 8091 });
            expect(bridge.getStatus().host).toBe('::1');
        });

        it('should accept bracketed IPv6 loopback [::1]', () => {
            const bridge = new AutomationBridge({ host: '[::1]', port: 8091 });
            expect(bridge.getStatus().host).toBe('::1');
        });
    });

    describe('Non-loopback addresses (default: rejected)', () => {
        it('should reject 0.0.0.0 by default and fallback to 127.0.0.1', () => {
            const bridge = new AutomationBridge({ host: '0.0.0.0', port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should reject LAN IP by default and fallback to 127.0.0.1', () => {
            const bridge = new AutomationBridge({ host: '192.168.1.100', port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should reject public IP by default and fallback to 127.0.0.1', () => {
            const bridge = new AutomationBridge({ host: '8.8.8.8', port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should reject IPv6 all-interfaces :: by default and fallback to 127.0.0.1', () => {
            const bridge = new AutomationBridge({ host: '::', port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should reject IPv6 LAN address by default and fallback to 127.0.0.1', () => {
            const bridge = new AutomationBridge({ host: 'fe80::1', port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });
    });

    describe('Non-loopback IPv4 addresses with allowNonLoopback=true (option)', () => {
        it('should accept 0.0.0.0 when allowNonLoopback is true', () => {
            const bridge = new AutomationBridge({ 
                host: '0.0.0.0', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('0.0.0.0');
        });

        it('should accept LAN IP when allowNonLoopback is true', () => {
            const bridge = new AutomationBridge({ 
                host: '192.168.1.100', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('192.168.1.100');
        });

        it('should accept any valid IPv4 when allowNonLoopback is true', () => {
            const bridge = new AutomationBridge({ 
                host: '10.0.0.1', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('10.0.0.1');
        });
    });

    describe('Non-loopback IPv6 addresses with allowNonLoopback=true (option)', () => {
        it('should accept :: (all interfaces) when allowNonLoopback is true', () => {
            const bridge = new AutomationBridge({ 
                host: '::', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('::');
        });

        it('should accept fe80::1 (link-local) when allowNonLoopback is true', () => {
            const bridge = new AutomationBridge({ 
                host: 'fe80::1', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('fe80::1');
        });

        it('should accept 2001:db8::1 (global) when allowNonLoopback is true', () => {
            const bridge = new AutomationBridge({ 
                host: '2001:db8::1', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('2001:db8::1');
        });

        it('should accept bracketed IPv6 [fe80::1] when allowNonLoopback is true', () => {
            const bridge = new AutomationBridge({ 
                host: '[fe80::1]', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('[fe80::1]');
        });
    });

    describe('Invalid addresses with allowNonLoopback=true', () => {
        it('should reject invalid hostname format', () => {
            const bridge = new AutomationBridge({ 
                host: '-invalid-hostname', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should reject out-of-range IPv4 octets', () => {
            const bridge = new AutomationBridge({ 
                host: '256.1.1.1', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should reject invalid hostname format', () => {
            const bridge = new AutomationBridge({ 
                host: '-invalid-hostname', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });
    });

    describe('Domain names/hostnames with allowNonLoopback=true', () => {
        it('should accept domain name when allowNonLoopback is true', () => {
            const bridge = new AutomationBridge({ 
                host: 'example.com', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('example.com');
        });

        it('should accept local hostname when allowNonLoopback is true', () => {
            const bridge = new AutomationBridge({ 
                host: 'unreal-server.local', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('unreal-server.local');
        });

        it('should accept subdomain when allowNonLoopback is true', () => {
            const bridge = new AutomationBridge({ 
                host: 'mcp.unreal.internal', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('mcp.unreal.internal');
        });

        it('should accept simple hostname when allowNonLoopback is true', () => {
            const bridge = new AutomationBridge({ 
                host: 'dev-pc', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('dev-pc');
        });
    });

    describe('MCP_AUTOMATION_ALLOW_NON_LOOPBACK env var', () => {
        it('should accept 0.0.0.0 when env var is "true"', () => {
            process.env.MCP_AUTOMATION_ALLOW_NON_LOOPBACK = 'true';
            const bridge = new AutomationBridge({ host: '0.0.0.0', port: 8091 });
            expect(bridge.getStatus().host).toBe('0.0.0.0');
        });

        it('should accept LAN IP when env var is "TRUE" (case insensitive)', () => {
            process.env.MCP_AUTOMATION_ALLOW_NON_LOOPBACK = 'TRUE';
            const bridge = new AutomationBridge({ host: '192.168.1.50', port: 8091 });
            expect(bridge.getStatus().host).toBe('192.168.1.50');
        });

        it('should accept IPv6 :: when env var is "true"', () => {
            process.env.MCP_AUTOMATION_ALLOW_NON_LOOPBACK = 'true';
            const bridge = new AutomationBridge({ host: '::', port: 8091 });
            expect(bridge.getStatus().host).toBe('::');
        });

        it('should reject non-loopback when env var is "false"', () => {
            process.env.MCP_AUTOMATION_ALLOW_NON_LOOPBACK = 'false';
            const bridge = new AutomationBridge({ host: '0.0.0.0', port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should reject non-loopback when env var is empty', () => {
            process.env.MCP_AUTOMATION_ALLOW_NON_LOOPBACK = '';
            const bridge = new AutomationBridge({ host: '0.0.0.0', port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });
    });

    describe('Edge cases', () => {
        it('should handle empty host string', () => {
            const bridge = new AutomationBridge({ host: '', port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should handle whitespace-only host', () => {
            const bridge = new AutomationBridge({ host: '   ', port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should trim whitespace from valid host', () => {
            const bridge = new AutomationBridge({ host: '  127.0.0.1  ', port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should use default when no host provided', () => {
            const bridge = new AutomationBridge({ port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should handle null host', () => {
            const bridge = new AutomationBridge({ host: null as unknown as string, port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('should handle undefined host', () => {
            const bridge = new AutomationBridge({ host: undefined, port: 8091 });
            expect(bridge.getStatus().host).toBe('127.0.0.1');
        });

        it('option allowNonLoopback should override env var false', () => {
            process.env.MCP_AUTOMATION_ALLOW_NON_LOOPBACK = 'false';
            const bridge = new AutomationBridge({ 
                host: '0.0.0.0', 
                port: 8091, 
                allowNonLoopback: true 
            });
            expect(bridge.getStatus().host).toBe('0.0.0.0');
        });
    });

    describe('IPv6 zone IDs', () => {
        it('should accept link-local with zone ID when allowNonLoopback is true', () => {
            const bridge = new AutomationBridge({ 
                host: 'fe80::1%eth0', 
                port: 8091, 
                allowNonLoopback: true 
            });
            // Zone ID is preserved in the returned value
            expect(bridge.getStatus().host).toBe('fe80::1%eth0');
        });
    });
});
