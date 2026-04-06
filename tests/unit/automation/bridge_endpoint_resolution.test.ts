import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { AutomationBridge } from '../../../src/automation/bridge.js';

describe('AutomationBridge endpoint resolution', () => {
    const originalEnv = process.env;

    beforeEach(() => {
        vi.resetModules();
        process.env = { ...originalEnv };
        delete process.env.MCP_AUTOMATION_HOST;
        delete process.env.MCP_AUTOMATION_WS_HOST;
        delete process.env.MCP_AUTOMATION_PORT;
        delete process.env.MCP_AUTOMATION_WS_PORT;
        delete process.env.MCP_AUTOMATION_WS_PORTS;
        delete process.env.MCP_AUTOMATION_CLIENT_PORT;
    });

    afterEach(() => {
        process.env = originalEnv;
    });

    it('prefers MCP_AUTOMATION_PORT over legacy MCP_AUTOMATION_WS_PORT', () => {
        process.env.MCP_AUTOMATION_PORT = '8091';
        process.env.MCP_AUTOMATION_WS_PORT = '8090';

        const bridge = new AutomationBridge();
        const status = bridge.getStatus();

        expect(status.port).toBe(8091);
        expect(status.configuredPorts).toEqual([8091, 8090]);
    });

    it('uses 8091-first fallback order when no port overrides are present', () => {
        const bridge = new AutomationBridge();
        const status = bridge.getStatus();

        expect(status.port).toBe(8091);
        expect(status.configuredPorts).toEqual([8091, 8090]);
    });

    it('lets MCP_AUTOMATION_CLIENT_PORT override the client target and otherwise follows the resolved canonical port', () => {
        process.env.MCP_AUTOMATION_PORT = '8091';
        process.env.MCP_AUTOMATION_WS_PORT = '8090';

        const defaultClientBridge = new AutomationBridge();
        expect((defaultClientBridge as unknown as { clientPort: number }).clientPort).toBe(8091);

        process.env.MCP_AUTOMATION_CLIENT_PORT = '8123';

        const explicitClientBridge = new AutomationBridge();
        expect((explicitClientBridge as unknown as { clientPort: number }).clientPort).toBe(8123);
    });

    it('reports the resolved public host and ordered port candidates in runtime diagnostics', () => {
        process.env.MCP_AUTOMATION_HOST = 'localhost';
        process.env.MCP_AUTOMATION_WS_HOST = '::1';
        process.env.MCP_AUTOMATION_PORT = '8091';
        process.env.MCP_AUTOMATION_WS_PORTS = '8090';

        const bridge = new AutomationBridge();
        const status = bridge.getStatus();

        expect(status.host).toBe('127.0.0.1');
        expect(status.port).toBe(8091);
        expect(status.configuredPorts).toEqual([8091, 8090]);
    });
});