import { WebSocket } from 'ws';
import { MAX_WS_MESSAGE_SIZE_BYTES } from '../constants.js';
import { redactImagePayloadTextForLog } from '../utils/logging/log-redaction.js';
import type { Logger } from '../utils/logging/logger.js';
import { type AutomationBridgeResolvedConfig, formatHostForUrl } from './bridge-config.js';
import { getRawDataByteLength, rawDataToUtf8String } from './bridge-frame.js';
import type { AutomationBridgeRuntimeState } from './bridge-state.js';
import type { ConnectionManager } from './connection-manager.js';
import type { HandshakeHandler } from './handshake.js';
import {
    REDACTED_AUTOMATION_CREDENTIAL,
    redactAutomationLogRecord,
    redactKnownAutomationCredentials
} from './log-redaction.js';
import type { MessageHandler } from './message-handler.js';
import { automationMessageSchema } from './message-schema.js';
import type { AutomationBridgeEvents, AutomationBridgeMessage } from './types.js';

type WebSocketWithInternalSocket = WebSocket & {
    _socket?: { remoteAddress?: string; remotePort?: number };
    socket?: { remoteAddress?: string; remotePort?: number };
};

interface AutomationBridgeClientDependencies {
    readonly config: AutomationBridgeResolvedConfig;
    readonly state: AutomationBridgeRuntimeState;
    readonly connectionManager: ConnectionManager;
    readonly handshakeHandler: HandshakeHandler;
    readonly messageHandler: MessageHandler;
    readonly log: Logger;
    readonly emit: <K extends keyof AutomationBridgeEvents>(
        event: K,
        ...args: Parameters<AutomationBridgeEvents[K]>
    ) => void;
    readonly rejectQueuedRequests: (error: Error) => void;
    readonly rejectPendingRequests: (error: Error) => void;
    readonly rejectOwnedRequests: (ownerId: string, error: Error) => number;
}

export class AutomationBridgeClient {
    private pendingConnectionSocket?: WebSocket;
    private readonly abortedConnectionSockets = new WeakSet<WebSocket>();

    constructor(private readonly deps: AutomationBridgeClientDependencies) { }

    public getClientUrl(): string {
        const scheme = this.deps.config.useTls ? 'wss' : 'ws';
        return `${scheme}://${formatHostForUrl(this.deps.config.clientHost)}:${this.deps.config.clientPort}`;
    }

    public startClient(): void {
        try {
            const url = this.getClientUrl();
            this.deps.log.info(`Connecting to Unreal Engine automation server at ${url}`);
            this.deps.log.debug(`Negotiated protocols: ${JSON.stringify(this.deps.config.negotiatedProtocols)}`);

            const protocols = this.deps.config.negotiatedProtocols.length === 1
                ? this.deps.config.negotiatedProtocols[0]
                : this.deps.config.negotiatedProtocols;
            this.deps.log.debug(`Using WebSocket protocols arg: ${JSON.stringify(protocols)}`);

            const headers: Record<string, string> | undefined = this.deps.config.capabilityToken
                ? {
                    'X-MCP-Capability': this.deps.config.capabilityToken,
                    'X-MCP-Capability-Token': this.deps.config.capabilityToken
                }
                : undefined;

            const socket = new WebSocket(url, protocols, { headers, perMessageDeflate: false });
            this.pendingConnectionSocket = socket;
            this.handleClientConnection(socket);
        } catch (error) {
            const errorObj = this.redactPeerError(error);
            this.deps.state.lastError = { message: errorObj.message, at: new Date() };
            this.deps.log.error('Failed to create WebSocket client connection', errorObj);
            this.deps.emit('error', Object.assign(errorObj, { port: this.deps.config.clientPort }));
        }
    }

    public abortPendingConnection(): void {
        const socket = this.pendingConnectionSocket;
        this.pendingConnectionSocket = undefined;

        if (socket && socket.readyState !== WebSocket.CLOSED && socket.readyState !== WebSocket.CLOSING) {
            this.abortedConnectionSockets.add(socket);
            try {
                socket.terminate();
            } catch (error) {
                this.deps.log.debug('Failed to terminate timed-out automation bridge socket', error instanceof Error ? error : String(error));
            }
        }
    }

    public send(payload: AutomationBridgeMessage): boolean {
        const primarySocket = this.deps.connectionManager.getPrimarySocket();
        if (!primarySocket || primarySocket.readyState !== WebSocket.OPEN) {
            this.deps.log.warn('Attempted to send automation message without an active primary connection');
            return false;
        }

        try {
            primarySocket.send(JSON.stringify(payload));
            return true;
        } catch (error) {
            this.deps.log.error('Failed to send automation message', error);
            const errorObj = error instanceof Error ? error : new Error(String(error));
            const primaryInfo = this.deps.connectionManager.getActiveSockets().get(primarySocket);
            this.deps.emit('error', Object.assign(errorObj, { port: primaryInfo?.port }));
            return false;
        }
    }

    public broadcast(payload: AutomationBridgeMessage): boolean {
        const sockets = this.deps.connectionManager.getActiveSockets();
        if (sockets.size === 0) {
            this.deps.log.warn('Attempted to broadcast automation message without any active connections');
            return false;
        }

        let sentCount = 0;
        for (const [socket] of sockets) {
            if (socket.readyState !== WebSocket.OPEN) {
                continue;
            }

            try {
                socket.send(JSON.stringify(payload));
                sentCount++;
            } catch (error) {
                this.deps.log.error('Failed to broadcast automation message to socket', error instanceof Error ? error : String(error));
            }
        }
        return sentCount > 0;
    }

    private async handleClientConnection(socket: WebSocket): Promise<void> {
        socket.on('open', async () => {
            this.deps.log.info('Automation bridge client connected, starting handshake');
            try {
                const metadata = await this.deps.handshakeHandler.initiateHandshake(socket, this.deps.config.connectionTimeoutMs);
                this.clearPendingConnection(socket);
                this.recordHandshakeSuccess(socket, metadata);
                this.installMessageHandler(socket);
            } catch (error) {
                this.clearPendingConnection(socket);
                const err = this.redactPeerError(error);
                this.deps.state.lastHandshakeFailure = { reason: err.message, at: new Date() };
                this.deps.emit('handshakeFailed', { reason: err.message, port: this.deps.config.clientPort });
            }
        });

        socket.on('error', (error) => {
            this.clearPendingConnection(socket);
            if (this.abortedConnectionSockets.has(socket)) {
                this.deps.log.debug('Ignoring error from aborted automation bridge socket', error);
                return;
            }

            const errorObj = this.redactPeerError(error);
            this.deps.log.error('Automation bridge client socket error', errorObj);
            this.deps.state.lastError = { message: errorObj.message, at: new Date() };
            this.deps.emit('error', Object.assign(errorObj, { port: this.deps.config.clientPort }));
        });

        socket.on('close', (code, reasonBuffer) => {
            this.clearPendingConnection(socket);
            this.abortedConnectionSockets.delete(socket);
            const reason = this.redactPeerText(reasonBuffer.toString('utf8'));
            const socketInfo = this.deps.connectionManager.removeSocket(socket);

            if (!socketInfo) {
                return;
            }

            this.deps.state.lastDisconnect = { code, reason, at: new Date() };
            this.deps.emit('disconnected', {
                code,
                reason,
                port: socketInfo.port,
                protocol: socketInfo.protocol || null
            });
            this.deps.log.info(`Automation bridge client socket closed (code=${code}, reason=${reason})`);

            // Owner-scoped settlement when a connection survives: exactly the
            // requests this socket carried, once each (entries are deleted),
            // never a cancel_request frame (disconnect is an explicit
            // non-notify class), idempotent for a secondary close that owns
            // nothing. When no socket remains, the full teardown below rejects
            // everything (owned included) and keeps the redacted close reason.
            if (this.deps.connectionManager.isConnected()) {
                this.deps.rejectOwnedRequests(socketInfo.connectionId, new Error('Automation bridge primary connection lost'));
            }

            if (!this.deps.connectionManager.isConnected()) {
                const error = new Error(reason || 'Connection lost');
                this.deps.rejectQueuedRequests(error);
                this.deps.rejectPendingRequests(error);
            }
        });
    }

    private recordHandshakeSuccess(socket: WebSocket, metadata: Record<string, unknown>): void {
        this.deps.state.lastHandshakeAt = new Date();
        this.deps.state.lastHandshakeMetadata = metadata;
        this.deps.state.lastHandshakeFailure = undefined;
        this.deps.connectionManager.updateLastMessageTime();

        const socketWithInternal = socket as WebSocketWithInternalSocket;
        const underlying = socketWithInternal._socket || socketWithInternal.socket;
        this.deps.connectionManager.registerSocket(
            socket,
            this.deps.config.clientPort,
            metadata,
            underlying?.remoteAddress ?? undefined,
            underlying?.remotePort ?? undefined
        );
        this.deps.connectionManager.startHeartbeat();

        this.deps.emit('connected', {
            socket,
            metadata: redactAutomationLogRecord(metadata),
            port: this.deps.config.clientPort,
            protocol: socket.protocol || null
        });
    }

    private installMessageHandler(socket: WebSocket): void {
        socket.on('message', (data) => {
            try {
                const byteLength = getRawDataByteLength(data);
                if (byteLength > MAX_WS_MESSAGE_SIZE_BYTES) {
                    this.deps.log.error(`Received oversized message (${byteLength} bytes, max: ${MAX_WS_MESSAGE_SIZE_BYTES}). Dropping.`);
                    return;
                }

                const text = rawDataToUtf8String(data, byteLength);
                this.deps.log.debug(`[AutomationBridge Client] Received message: ${redactImagePayloadTextForLog(text).substring(0, 1000)}`);
                const parsed = JSON.parse(text);
                if (!this.deps.connectionManager.recordInboundMessage(socket, false)) {
                    this.deps.log.warn('Inbound message rate limit exceeded; closing connection.');
                    socket.close(4008, 'Rate limit exceeded');
                    return;
                }

                const validation = automationMessageSchema.safeParse(parsed);
                if (!validation.success) {
                    this.deps.log.warn('Dropped invalid automation message', validation.error.issues);
                    return;
                }

                this.deps.connectionManager.updateLastMessageTime();
                this.deps.messageHandler.handleMessage(validation.data);
                this.deps.emit('message', validation.data);
            } catch (error) {
                this.deps.log.error('Error handling message', error instanceof Error ? error : String(error));
            }
        });
    }

    private clearPendingConnection(socket: WebSocket): void {
        if (this.pendingConnectionSocket === socket) {
            this.pendingConnectionSocket = undefined;
        }
    }

    private redactPeerText(value: string): string {
        const knownCredentials = this.deps.config.capabilityToken
            ? [this.deps.config.capabilityToken]
            : [];
        const redacted = redactKnownAutomationCredentials(value, knownCredentials);
        return typeof redacted === 'string'
            ? redacted
            : REDACTED_AUTOMATION_CREDENTIAL;
    }

    private redactPeerError(value: unknown): Error {
        const source = value instanceof Error ? value : new Error(String(value));
        const redacted = new Error(this.redactPeerText(source.message));
        redacted.name = source.name;
        return redacted;
    }
}
