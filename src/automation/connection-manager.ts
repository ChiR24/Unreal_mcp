import { WebSocket } from 'ws';
import { Logger } from '../utils/logger.js';
import { randomUUID } from 'node:crypto';
import { SocketInfo } from './types.js';
import { EventEmitter } from 'node:events';

export class ConnectionManager extends EventEmitter {
    private activeSockets = new Map<WebSocket, SocketInfo>();
    private primarySocket?: WebSocket;
    private heartbeatTimer?: NodeJS.Timeout;
    private lastMessageAt?: Date;
    private log = new Logger('ConnectionManager');

    constructor(
        private heartbeatIntervalMs: number
    ) {
        super();
    }

    /**
     * Get the configured heartbeat interval in milliseconds.
     * @returns The heartbeat interval or 0 if disabled
     */
    public getHeartbeatIntervalMs(): number {
        return this.heartbeatIntervalMs;
    }

    public registerSocket(
        socket: WebSocket,
        port: number,
        metadata?: Record<string, unknown>,
        remoteAddress?: string,
        remotePort?: number
    ): void {
        const connectionId = randomUUID();
        const sessionId = metadata && typeof metadata.sessionId === 'string' ? (metadata.sessionId as string) : undefined;
        const socketInfo: SocketInfo = {
            connectionId,
            port,
            connectedAt: new Date(),
            protocol: socket.protocol || undefined,
            sessionId,
            remoteAddress: remoteAddress ?? undefined,
            remotePort: typeof remotePort === 'number' ? remotePort : undefined
        };

        this.activeSockets.set(socket, socketInfo);

        // Set as primary socket if this is the first connection
        if (!this.primarySocket) {
            this.primarySocket = socket;
        }

        // Handle WebSocket pong frames for heartbeat tracking
        socket.on('pong', () => {
            this.lastMessageAt = new Date();
        });

        // Auto-cleanup on close or error
        socket.once('close', () => {
            this.removeSocket(socket);
        });

        socket.once('error', (error) => {
            this.log.error('Socket error in ConnectionManager', error);
            this.removeSocket(socket);
        });
    }

    public removeSocket(socket: WebSocket): SocketInfo | undefined {
        const info = this.activeSockets.get(socket);
        if (info) {
            this.activeSockets.delete(socket);
            if (socket === this.primarySocket) {
                this.primarySocket = this.activeSockets.size > 0 ? this.activeSockets.keys().next().value : undefined;
                if (this.activeSockets.size === 0) {
                    this.stopHeartbeat();
                }
            }
        }
        return info;
    }

    public getActiveSockets(): Map<WebSocket, SocketInfo> {
        return this.activeSockets;
    }

    public getPrimarySocket(): WebSocket | undefined {
        return this.primarySocket;
    }

    public isConnected(): boolean {
        return this.activeSockets.size > 0;
    }

    public startHeartbeat(): void {
        if (this.heartbeatIntervalMs <= 0) return;
        if (this.heartbeatTimer) clearInterval(this.heartbeatTimer);

        this.heartbeatTimer = setInterval(() => {
            if (this.activeSockets.size === 0) {
                this.stopHeartbeat();
                return;
            }

            const pingPayload = JSON.stringify({
                type: 'bridge_ping',
                timestamp: new Date().toISOString()
            });

            for (const [socket] of this.activeSockets) {
                if (socket.readyState === WebSocket.OPEN) {
                    try {
                        socket.ping();
                        socket.send(pingPayload);
                    } catch (error: unknown) {
                        this.log.error('Failed to send heartbeat', error);
                    }
                }
            }
        }, this.heartbeatIntervalMs);
    }

    public stopHeartbeat(): void {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = undefined;
        }
    }

    public updateLastMessageTime(): void {
        this.lastMessageAt = new Date();
    }

    public getLastMessageTime(): Date | undefined {
        return this.lastMessageAt;
    }

    public closeAll(code?: number, reason?: string): void {
        this.stopHeartbeat();
        for (const [socket] of this.activeSockets) {
            socket.removeAllListeners();
            socket.close(code, reason);
        }
        this.activeSockets.clear();
        this.primarySocket = undefined;
    }
}
