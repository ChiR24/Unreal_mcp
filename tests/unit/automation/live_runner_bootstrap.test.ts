import { describe, expect, it, vi } from 'vitest';

const loadBridgeBootstrap = () => import('../../lib/bridge-bootstrap.mjs');

describe('live runner bootstrap', () => {
    it('preserves the discovered listener and overlays the child-process env with the resolved host and port', async () => {
        const { waitForAnyPort, prepareLiveServerEnv } = await loadBridgeBootstrap();
        const probePort = vi.fn(async (_host: string, port: number) => port === 8090);

        const listener = await waitForAnyPort('127.0.0.1', [8091, 8090], 25, probePort);
        const serverEnv = prepareLiveServerEnv({ KEEP_ME: '1', MCP_AUTOMATION_CLIENT_PORT: '9000' }, listener);

        expect(listener).toEqual({ host: '127.0.0.1', port: 8090, ports: [8090, 8091] });
        expect(serverEnv.KEEP_ME).toBe('1');
        expect(serverEnv.MCP_AUTOMATION_HOST).toBe('127.0.0.1');
        expect(serverEnv.MCP_AUTOMATION_PORT).toBe('8090');
        expect(serverEnv.MCP_AUTOMATION_WS_HOST).toBe('127.0.0.1');
        expect(serverEnv.MCP_AUTOMATION_WS_PORT).toBe('8090');
        expect(serverEnv.MCP_AUTOMATION_WS_PORTS).toBe('8090,8091');
        expect(serverEnv.MCP_AUTOMATION_CLIENT_HOST).toBe('127.0.0.1');
        expect(serverEnv.MCP_AUTOMATION_CLIENT_PORT).toBe('8090');
        expect(probePort).toHaveBeenCalledTimes(2);
    });

    it('uses a 8091-first probe order by default and preserves compatibility lists from MCP_AUTOMATION_WS_PORTS', async () => {
        const { getBridgeBootstrapConfig } = await loadBridgeBootstrap();

        expect(getBridgeBootstrapConfig({})).toEqual({
            host: '127.0.0.1',
            ports: [8091, 8090]
        });

        expect(getBridgeBootstrapConfig({
            MCP_AUTOMATION_PORT: '8123',
            MCP_AUTOMATION_WS_PORTS: '8100,8090'
        })).toEqual({
            host: '127.0.0.1',
            ports: [8123, 8100, 8090]
        });
    });

    it('throws structured diagnostics when no bridge listener is reachable', async () => {
        const { waitForAnyPort } = await loadBridgeBootstrap();

        await expect(
            waitForAnyPort('127.0.0.1', [8091, 8090], 5, async () => false)
        ).rejects.toMatchObject({
            code: 'AUTOMATION_BRIDGE_UNAVAILABLE',
            host: '127.0.0.1',
            ports: [8091, 8090],
            guidance: expect.stringContaining('MCP_AUTOMATION_PORT')
        });
    });
});