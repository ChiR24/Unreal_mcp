import { randomUUID } from 'node:crypto';
import { EventEmitter } from 'node:events';
import { WebSocket } from 'ws';
import { AutomationLogger } from './log-redaction.js';
import type { SocketInfo } from './types.js';

const RATE_LIMIT_WINDOW_MS = 60_000;

type AutomationSocketListener =
    | (() => void)
    | ((data: Buffer | string) => void)
    | ((error: Error) => void);

export interface AutomationSocket {
    readonly protocol: string;
    readonly readyState: number;
    ping(): void;
    send(data: string): void;
    close(code?: number, data?: string): void;
    removeAllListeners(): this;
    on(eventName: string | symbol, listener: AutomationSocketListener): this;
    once(eventName: string | symbol, listener: AutomationSocketListener): this;
    off(eventName: string | symbol, listener: AutomationSocketListener): this;
}

/**
 * The one WebSocket this client holds to the plugin: its identity, the
 * heartbeat that keeps it alive, and the inbound message-rate limit.
 */
export class ConnectionManager extends EventEmitter {
    private socket?: AutomationSocket;
    private info?: SocketInfo;
    private heartbeatTimer?: NodeJS.Timeout;
    private lastMessageAt?: Date;
    private log = new AutomationLogger('ConnectionManager');
    private rateWindowStartMs = 0;
    private rateMessageCount = 0;

    constructor(
        private heartbeatIntervalMs: number,
        private maxMessagesPerMinute: number
    ) {
        super();
    }

    /** The configured heartbeat interval in milliseconds, or 0 when disabled. */
    public getHeartbeatIntervalMs(): number {
        return this.heartbeatIntervalMs;
    }

    public registerSocket(
        socket: AutomationSocket,
        port: number,
        metadata?: Record<string, unknown>,
        remoteAddress?: string,
        remotePort?: number
    ): void {
        this.socket = socket;
        this.info = {
            connectionId: randomUUID(),
            port,
            connectedAt: new Date(),
            protocol: socket.protocol || undefined,
            sessionId: metadata && typeof metadata.sessionId === 'string' ? metadata.sessionId : undefined,
            remoteAddress: remoteAddress ?? undefined,
            remotePort: typeof remotePort === 'number' ? remotePort : undefined
        };
        this.rateWindowStartMs = Date.now();
        this.rateMessageCount = 0;

        socket.on('pong', () => {
            this.lastMessageAt = new Date();
        });
        socket.once('close', () => {
            this.removeSocket(socket);
        });
        socket.once('error', (error: Error) => {
            this.log.error('Socket error in ConnectionManager', error);
            this.removeSocket(socket);
        });
    }

    /** Forget `socket` if it is the registered one; returns what was known about it. */
    public removeSocket(socket: AutomationSocket): SocketInfo | undefined {
        if (socket !== this.socket) return undefined;
        const info = this.info;
        this.socket = undefined;
        this.info = undefined;
        this.stopHeartbeat();
        return info;
    }

    /** False when the socket is not ours or has exceeded the per-minute message limit. */
    public recordInboundMessage(socket: AutomationSocket): boolean {
        if (socket !== this.socket) return false;
        if (this.maxMessagesPerMinute <= 0) return true;

        const nowMs = Date.now();
        if (nowMs - this.rateWindowStartMs >= RATE_LIMIT_WINDOW_MS) {
            this.rateWindowStartMs = nowMs;
            this.rateMessageCount = 0;
        }
        this.rateMessageCount += 1;
        if (this.rateMessageCount > this.maxMessagesPerMinute) {
            this.log.warn(`Inbound message rate exceeded (${this.rateMessageCount}/${this.maxMessagesPerMinute} per minute).`);
            return false;
        }
        return true;
    }

    public getSocket(): AutomationSocket | undefined {
        return this.socket;
    }

    public getSocketInfo(): SocketInfo | undefined {
        return this.info;
    }

    public isConnected(): boolean {
        return this.socket !== undefined;
    }

    public startHeartbeat(): void {
        if (this.heartbeatIntervalMs <= 0) return;
        if (this.heartbeatTimer) clearInterval(this.heartbeatTimer);

        this.heartbeatTimer = setInterval(() => {
            const socket = this.socket;
            if (!socket) {
                this.stopHeartbeat();
                return;
            }
            if (socket.readyState !== WebSocket.OPEN) return;
            try {
                socket.ping();
            } catch (error) {
                this.log.error('Failed to send heartbeat', error instanceof Error ? error : String(error));
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

    public close(code?: number, reason?: string): void {
        this.stopHeartbeat();
        const socket = this.socket;
        this.socket = undefined;
        this.info = undefined;
        if (socket) {
            socket.removeAllListeners();
            socket.close(code, reason);
        }
    }
}
