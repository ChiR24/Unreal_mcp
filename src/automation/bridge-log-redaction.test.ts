import { afterEach, describe, expect, it, vi } from 'vitest';
import { WebSocketServer } from 'ws';
import { Logger } from '../utils/logging/logger.js';
import { AutomationBridge } from './bridge.js';
import { redactAutomationLogValue } from './log-redaction.js';

async function closeWebSocketServer(server: WebSocketServer): Promise<void> {
    for (const client of server.clients) {
        client.terminate();
    }

    await new Promise<void>((resolve, reject) => {
        server.close((error) => {
            if (error) {
                reject(error);
                return;
            }
            resolve();
        });
    });
}

describe('AutomationBridge log redaction', () => {
    afterEach(() => {
        vi.restoreAllMocks();
    });

    it('keeps the session id in connection state without writing credentials to logs', async () => {
        // Given
        const loggedArguments: unknown[][] = [];
        for (const level of ['debug', 'info', 'warn', 'error'] as const) {
            vi.spyOn(Logger.prototype, level).mockImplementation((...args: unknown[]) => {
                loggedArguments.push(args);
            });
        }

        const server = new WebSocketServer({ host: '127.0.0.1', port: 0 });
        server.on('connection', (socket) => {
            socket.on('message', (data) => {
                const message: unknown = JSON.parse(data.toString('utf8'));
                if (
                    typeof message === 'object'
                    && message !== null
                    && 'type' in message
                    && message.type === 'bridge_hello'
                ) {
                    socket.send(
                        JSON.stringify({
                            type: 'bridge_ack',
                            sessionId: 'internal-session-id',
                            capabilityToken: 'server-capability-token',
                            message: 'Echoed server-capability-token'
                        })
                    );
                }
            });
        });

        await new Promise<void>((resolve, reject) => {
            server.once('error', reject);
            server.once('listening', resolve);
        });

        const address = server.address();
        if (!address || typeof address === 'string') {
            await closeWebSocketServer(server);
            throw new Error('Failed to bind test WebSocket server');
        }

        const bridge = new AutomationBridge({
            capabilityToken: 'client-capability-token',
            clientHost: '127.0.0.1',
            clientPort: address.port,
            connectionTimeoutMs: 1000,
            heartbeatIntervalMs: 0
        });
        let connectedMetadata: Record<string, unknown> | undefined;
        const inboundMessage = new Promise<void>((resolve) => {
            bridge.once('message', () => resolve());
        });
        bridge.once('connected', (info) => {
            connectedMetadata = info.metadata;
            for (const client of server.clients) {
                client.send(
                    JSON.stringify({
                        type: 'bridge_goodbye',
                        sessionId: 'message-session-id',
                        capabilityToken: 'message-capability-token',
                        message: 'Echoed message-capability-token'
                    })
                );
            }
        });

        try {
            // When
            bridge.start();
            await inboundMessage;

            // Then
            const status = bridge.getStatus();
            expect(status.connections[0]?.sessionId).toBe('internal-session-id');
            expect(status.lastHandshakeMetadata?.sessionId).toBe('internal-session-id');
            expect(status.lastHandshakeMetadata?.message).toBe('Echoed [REDACTED]');
            expect(connectedMetadata?.sessionId).toBe('[REDACTED]');
            expect(connectedMetadata?.capabilityToken).toBe('[REDACTED]');

            const serializedLogs = JSON.stringify(loggedArguments);
            expect(serializedLogs).not.toContain('internal-session-id');
            expect(serializedLogs).not.toContain('server-capability-token');
            expect(serializedLogs).not.toContain('client-capability-token');
            expect(serializedLogs).not.toContain('message-session-id');
            expect(serializedLogs).not.toContain('message-capability-token');
            expect(serializedLogs).toContain('[REDACTED]');
        } finally {
            bridge.stop();
            await closeWebSocketServer(server);
        }
    });

    it('redacts nested credential fields and labeled credential text without mutating input', () => {
        // Given
        const source = {
            sessionId: 'top-level-session',
            nested: {
                session_id: 'nested-session',
                capabilityToken: 'embedded-capability',
                note: 'Echoed embedded-capability'
            },
            headers: {
                'X-MCP-Capability': 'header-capability'
            }
        };

        // When
        const redacted = redactAutomationLogValue(source);

        // Then
        const serialized = JSON.stringify(redacted);
        expect(serialized).not.toContain('top-level-session');
        expect(serialized).not.toContain('nested-session');
        expect(serialized).not.toContain('embedded-capability');
        expect(serialized).not.toContain('header-capability');
        expect(serialized).toContain('[REDACTED]');
        expect(source.sessionId).toBe('top-level-session');
        expect(source.nested.session_id).toBe('nested-session');
    });

    it('redacts a reflected capability token from disconnect diagnostics and pending errors', async () => {
        const capabilityToken = 'reflected-close-capability';
        const loggedArguments: unknown[][] = [];
        for (const level of ['debug', 'info', 'warn', 'error'] as const) {
            vi.spyOn(Logger.prototype, level).mockImplementation((...args: unknown[]) => {
                loggedArguments.push(args);
            });
        }

        const server = new WebSocketServer({ host: '127.0.0.1', port: 0 });
        server.on('connection', (socket) => {
            socket.on('message', (data) => {
                const message: unknown = JSON.parse(data.toString('utf8'));
                if (
                    typeof message === 'object'
                    && message !== null
                    && 'type' in message
                    && message.type === 'bridge_hello'
                ) {
                    socket.send(JSON.stringify({
                        type: 'bridge_ack',
                        sessionId: 'disconnect-redaction-session'
                    }));
                }
            });
        });
        await new Promise<void>((resolve, reject) => {
            server.once('error', reject);
            server.once('listening', resolve);
        });
        const address = server.address();
        if (!address || typeof address === 'string') {
            await closeWebSocketServer(server);
            throw new Error('Failed to bind test WebSocket server');
        }

        const bridge = new AutomationBridge({
            capabilityToken,
            clientHost: '127.0.0.1',
            clientPort: address.port,
            connectionTimeoutMs: 1000,
            heartbeatIntervalMs: 0
        });
        const connected = new Promise<void>((resolve) => {
            bridge.once('connected', () => resolve());
        });
        const disconnected = new Promise<{ reason: string }>((resolve) => {
            bridge.once('disconnected', (info) => resolve(info));
        });

        try {
            bridge.start();
            await connected;
            const pendingRequest = bridge.sendAutomationRequest(
                'inspect',
                { action: 'get_status' },
                { timeoutMs: 5000 }
            );
            for (const client of server.clients) {
                client.close(4001, `peer echoed ${capabilityToken}`);
            }

            const disconnectInfo = await disconnected;
            await expect(pendingRequest).rejects.toThrow(
                'peer echoed [REDACTED]'
            );
            expect(disconnectInfo.reason).toBe('peer echoed [REDACTED]');
            expect(bridge.getStatus().lastDisconnect?.reason).toBe(
                'peer echoed [REDACTED]'
            );

            const serializedDiagnostics = JSON.stringify({
                logs: loggedArguments,
                disconnectInfo,
                status: bridge.getStatus()
            });
            expect(serializedDiagnostics).not.toContain(capabilityToken);
            expect(serializedDiagnostics).toContain('[REDACTED]');
        } finally {
            bridge.stop();
            await closeWebSocketServer(server);
        }
    });
});
