import { describe, expect, it, vi } from 'vitest';
import { Logger } from '../utils/logging/logger.js';
import { createServer } from './server-factory.js';

describe('createServer automation event notifications', () => {
    it('forwards automation bridge events through the MCP notification API', async () => {
        const { server, bridge, automationBridge, metricsServer } = createServer();
        const notificationSpy = vi
            .spyOn(server, 'notification')
            .mockImplementation(async () => undefined);

        try {
            automationBridge.emit('automationEvent', {
                type: 'automation_event',
                event: 'asset_saved',
                requestId: 'orphan-request',
                message: 'Saved /Game/Maps/Arena',
                payload: { assetPath: '/Game/Maps/Arena' }
            });

            expect(notificationSpy).toHaveBeenCalledWith({
                method: 'notifications/unreal/automation_event',
                params: {
                    type: 'automation_event',
                    event: 'asset_saved',
                    requestId: 'orphan-request',
                    message: 'Saved /Game/Maps/Arena',
                    payload: { assetPath: '/Game/Maps/Arena' }
                }
            });
        } finally {
            notificationSpy.mockRestore();
            automationBridge.stop();
            bridge.dispose();
            metricsServer?.close();
        }
    });

    it('redacts session credentials from automation bridge message logs', () => {
        // Given
        const loggedArguments: unknown[][] = [];
        const infoSpy = vi
            .spyOn(Logger.prototype, 'info')
            .mockImplementation((...args: unknown[]) => {
                loggedArguments.push(args);
            });
        const debugSpy = vi
            .spyOn(Logger.prototype, 'debug')
            .mockImplementation((...args: unknown[]) => {
                loggedArguments.push(args);
            });
        const { bridge, automationBridge, metricsServer } = createServer();

        try {
            // When
            automationBridge.emit('message', {
                type: 'bridge_goodbye',
                session_id: 'raw-message-session-id',
                headers: {
                    'X-MCP-Capability-Token': 'raw-message-capability-token'
                }
            });

            // Then
            const serializedLogs = JSON.stringify(loggedArguments);
            expect(serializedLogs).not.toContain('raw-message-session-id');
            expect(serializedLogs).not.toContain('raw-message-capability-token');
            expect(serializedLogs).toContain('[REDACTED]');
        } finally {
            automationBridge.stop();
            bridge.dispose();
            metricsServer?.close();
            infoSpy.mockRestore();
            debugSpy.mockRestore();
        }
    });
});
