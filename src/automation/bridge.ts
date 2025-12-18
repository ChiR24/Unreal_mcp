import { EventEmitter } from 'node:events';
import { WebSocket } from 'ws';
import { Logger } from '../utils/logger.js';
import {
    DEFAULT_AUTOMATION_HOST,
    DEFAULT_AUTOMATION_PORT,
    DEFAULT_NEGOTIATED_PROTOCOLS,
    DEFAULT_HEARTBEAT_INTERVAL_MS,
    DEFAULT_MAX_PENDING_REQUESTS
} from '../constants.js';
import { createRequire } from 'node:module';
import {
    AutomationBridgeOptions,
    AutomationBridgeStatus,
    AutomationBridgeMessage,
    AutomationBridgeResponseMessage,
    AutomationBridgeEvents
} from './types.js';
import { ConnectionManager } from './connection-manager.js';
import { RequestTracker } from './request-tracker.js';
import { HandshakeHandler } from './handshake.js';
import { MessageHandler } from './message-handler.js';

const require = createRequire(import.meta.url);
const packageInfo: { name?: string; version?: string } = (() => {
    try {
        return require('../../package.json');
    } catch (error) {
        const log = new Logger('AutomationBridge');
        log.debug('Unable to read package.json for version info', error);
        return {};
    }
})();

export class AutomationBridge extends EventEmitter {
    private readonly host: string;
    private readonly port: number;
    private readonly ports: number[];
    private readonly negotiatedProtocols: string[];
    private readonly capabilityToken?: string;
    private readonly enabled: boolean;
    private readonly serverName: string;
    private readonly serverVersion: string;
    private readonly clientHost: string;
    private readonly clientPort: number;
    private readonly serverLegacyEnabled: boolean;
    private readonly maxConcurrentConnections: number;

    private connectionManager: ConnectionManager;
    private requestTracker: RequestTracker;
    private handshakeHandler: HandshakeHandler;
    private messageHandler: MessageHandler;
    private log = new Logger('AutomationBridge');

    private lastHandshakeAt?: Date;
    private lastHandshakeMetadata?: Record<string, unknown>;
    private lastHandshakeAck?: AutomationBridgeMessage;
    private lastHandshakeFailure?: { reason: string; at: Date };
    private lastDisconnect?: { code: number; reason: string; at: Date };
    private lastError?: { message: string; at: Date };
    private queuedRequestItems: Array<{ resolve: (v: any) => void; reject: (e: any) => void; action: string; payload: any; options: any }> = [];
    private connectionPromise?: Promise<void>;
    private connectionLock = false;

    constructor(options: AutomationBridgeOptions = {}) {
        super();
        this.host = options.host ?? process.env.MCP_AUTOMATION_WS_HOST ?? DEFAULT_AUTOMATION_HOST;

        const sanitizePort = (value: unknown): number | null => {
            if (typeof value === 'number' && Number.isInteger(value)) {
                return value > 0 && value <= 65535 ? value : null;
            }
            if (typeof value === 'string' && value.trim().length > 0) {
                const parsed = Number.parseInt(value.trim(), 10);
                return Number.isInteger(parsed) && parsed > 0 && parsed <= 65535 ? parsed : null;
            }
            return null;
        };

        const defaultPort = sanitizePort(options.port ?? process.env.MCP_AUTOMATION_WS_PORT) ?? DEFAULT_AUTOMATION_PORT;
        const configuredPortValues: Array<number | string> | undefined = options.ports
            ? options.ports
            : process.env.MCP_AUTOMATION_WS_PORTS
                ?.split(',')
                .map((token) => token.trim())
                .filter((token) => token.length > 0);

        const sanitizedPorts = Array.isArray(configuredPortValues)
            ? configuredPortValues
                .map((value) => sanitizePort(value))
                .filter((port): port is number => port !== null)
            : [];

        if (!sanitizedPorts.includes(defaultPort)) {
            sanitizedPorts.unshift(defaultPort);
        }
        if (sanitizedPorts.length === 0) {
            sanitizedPorts.push(DEFAULT_AUTOMATION_PORT);
        }

        this.ports = Array.from(new Set(sanitizedPorts));
        const defaultProtocols = DEFAULT_NEGOTIATED_PROTOCOLS;
        const userProtocols = Array.isArray(options.protocols)
            ? options.protocols.filter((proto) => typeof proto === 'string' && proto.trim().length > 0)
            : [];
        const envProtocols = process.env.MCP_AUTOMATION_WS_PROTOCOLS
            ? process.env.MCP_AUTOMATION_WS_PROTOCOLS.split(',')
                .map((token) => token.trim())
                .filter((token) => token.length > 0)
            : [];
        this.negotiatedProtocols = Array.from(new Set([...userProtocols, ...envProtocols, ...defaultProtocols]));
        this.port = this.ports[0];
        this.serverLegacyEnabled =
            options.serverLegacyEnabled ?? process.env.MCP_AUTOMATION_SERVER_LEGACY !== 'false';
        this.capabilityToken =
            options.capabilityToken ?? process.env.MCP_AUTOMATION_CAPABILITY_TOKEN ?? undefined;
        this.enabled = options.enabled ?? process.env.MCP_AUTOMATION_BRIDGE_ENABLED !== 'false';
        this.serverName = options.serverName
            ?? process.env.MCP_SERVER_NAME
            ?? packageInfo.name
            ?? 'unreal-engine-mcp';
        this.serverVersion = options.serverVersion
            ?? process.env.MCP_SERVER_VERSION
            ?? packageInfo.version
            ?? process.env.npm_package_version
            ?? '0.0.0';

        const heartbeatIntervalMs = (options.heartbeatIntervalMs ?? DEFAULT_HEARTBEAT_INTERVAL_MS) > 0
            ? (options.heartbeatIntervalMs ?? DEFAULT_HEARTBEAT_INTERVAL_MS)
            : 0;

        const maxPendingRequests = Math.max(1, options.maxPendingRequests ?? DEFAULT_MAX_PENDING_REQUESTS);
        const maxConcurrentConnections = Math.max(1, options.maxConcurrentConnections ?? 10);

        this.clientHost = options.clientHost ?? process.env.MCP_AUTOMATION_CLIENT_HOST ?? DEFAULT_AUTOMATION_HOST;
        this.clientPort = options.clientPort ?? sanitizePort(process.env.MCP_AUTOMATION_CLIENT_PORT) ?? DEFAULT_AUTOMATION_PORT;
        this.maxConcurrentConnections = maxConcurrentConnections;

        // Initialize components
        this.connectionManager = new ConnectionManager(heartbeatIntervalMs);
        this.requestTracker = new RequestTracker(maxPendingRequests);
        this.handshakeHandler = new HandshakeHandler(this.capabilityToken);
        this.messageHandler = new MessageHandler(this.requestTracker);

        // Forward events from connection manager
        // Note: ConnectionManager doesn't emit 'connected'/'disconnected' directly in the same way, 
        // we handle socket events here and use ConnectionManager to track state.
    }

    override on<K extends keyof AutomationBridgeEvents>(
        event: K,
        listener: AutomationBridgeEvents[K]
    ): this {
        return super.on(event, listener as (...args: unknown[]) => void);
    }

    override once<K extends keyof AutomationBridgeEvents>(
        event: K,
        listener: AutomationBridgeEvents[K]
    ): this {
        return super.once(event, listener as (...args: unknown[]) => void);
    }

    override off<K extends keyof AutomationBridgeEvents>(
        event: K,
        listener: AutomationBridgeEvents[K]
    ): this {
        return super.off(event, listener as (...args: unknown[]) => void);
    }

    start(): void {
        if (!this.enabled) {
            this.log.info('Automation bridge disabled by configuration.');
            return;
        }

        this.log.info(`Automation bridge connecting to Unreal server at ws://${this.clientHost}:${this.clientPort}`);
        this.startClient();
    }

    private startClient(): void {
        try {
            const url = `ws://${this.clientHost}:${this.clientPort}`;
            this.log.info(`Connecting to Unreal Engine automation server at ${url}`);

            this.log.debug(`Negotiated protocols: ${JSON.stringify(this.negotiatedProtocols)}`);

            // Compatibility fix: If only one protocol, pass as string to ensure ws/plugin compatibility
            const protocols = 'mcp-automation';

            this.log.debug(`Using WebSocket protocols arg: ${JSON.stringify(protocols)}`);

            const socket = new WebSocket(url, protocols, {
                headers: this.capabilityToken ? { 'X-MCP-Capability': this.capabilityToken } : undefined,
                perMessageDeflate: false
            });

            this.handleClientConnection(socket);
        } catch (error) {
            const errorObj = error instanceof Error ? error : new Error(String(error));
            this.lastError = { message: errorObj.message, at: new Date() };
            this.log.error('Failed to create WebSocket client connection', errorObj);
            const errorWithPort = Object.assign(errorObj, { port: this.clientPort });
            this.emitAutomation('error', errorWithPort);
        }
    }

    private async handleClientConnection(socket: WebSocket): Promise<void> {
        socket.on('open', async () => {
            this.log.info('Automation bridge client connected, starting handshake');
            try {
                const metadata = await this.handshakeHandler.initiateHandshake(socket);

                this.lastHandshakeAt = new Date();
                this.lastHandshakeMetadata = metadata;
                this.lastHandshakeFailure = undefined;
                this.connectionManager.updateLastMessageTime();

                // Extract remote address/port from underlying TCP socket
                // Note: WebSocket types don't expose _socket, but it exists at runtime
                const socketWithInternal = socket as unknown as { _socket?: { remoteAddress?: string; remotePort?: number }; socket?: { remoteAddress?: string; remotePort?: number } };
                const underlying = socketWithInternal._socket || socketWithInternal.socket;
                const remoteAddr = underlying?.remoteAddress ?? undefined;
                const remotePort = underlying?.remotePort ?? undefined;

                this.connectionManager.registerSocket(socket, this.clientPort, metadata, remoteAddr, remotePort);
                this.connectionManager.startHeartbeat();

                this.emitAutomation('connected', {
                    socket,
                    metadata,
                    port: this.clientPort,
                    protocol: socket.protocol || null
                });

                // Set up message handling for the authenticated socket
                socket.on('message', (data) => {
                    try {
                        const text = typeof data === 'string' ? data : data.toString('utf8');
                        this.log.debug(`[AutomationBridge Client] Received message: ${text.substring(0, 1000)}`);
                        const parsed = JSON.parse(text) as AutomationBridgeMessage;
                        this.connectionManager.updateLastMessageTime();
                        this.messageHandler.handleMessage(parsed);
                        this.emitAutomation('message', parsed);
                    } catch (error) {
                        this.log.error('Error handling message', error);
                    }
                });

            } catch (error) {
                const err = error instanceof Error ? error : new Error(String(error));
                this.lastHandshakeFailure = { reason: err.message, at: new Date() };
                this.emitAutomation('handshakeFailed', { reason: err.message, port: this.clientPort });
            }
        });

        socket.on('error', (error) => {
            this.log.error('Automation bridge client socket error', error);
            const errObj = error instanceof Error ? error : new Error(String(error));
            this.lastError = { message: errObj.message, at: new Date() };
            const errWithPort = Object.assign(errObj, { port: this.clientPort });
            this.emitAutomation('error', errWithPort);
        });

        socket.on('close', (code, reasonBuffer) => {
            const reason = reasonBuffer.toString('utf8');
            const socketInfo = this.connectionManager.removeSocket(socket);

            if (socketInfo) {
                this.lastDisconnect = { code, reason, at: new Date() };
                this.emitAutomation('disconnected', {
                    code,
                    reason,
                    port: socketInfo.port,
                    protocol: socketInfo.protocol || null
                });
                this.log.info(`Automation bridge client socket closed (code=${code}, reason=${reason})`);

                if (!this.connectionManager.isConnected()) {
                    this.requestTracker.rejectAll(new Error(reason || 'Connection lost'));
                }
            }
        });
    }

    stop(): void {
        if (this.isConnected()) {
            this.broadcast({
                type: 'bridge_shutdown',
                timestamp: new Date().toISOString(),
                reason: 'Server shutting down'
            });
        }
        this.connectionManager.closeAll(1001, 'Server shutdown');
        this.lastHandshakeAck = undefined;
        this.requestTracker.rejectAll(new Error('Automation bridge server stopped'));
    }

    isConnected(): boolean {
        return this.connectionManager.isConnected();
    }

    getStatus(): AutomationBridgeStatus {

        const connectionInfos = Array.from(this.connectionManager.getActiveSockets().entries()).map(([socket, info]) => ({
            connectionId: info.connectionId,
            sessionId: info.sessionId ?? null,
            remoteAddress: info.remoteAddress ?? null,
            remotePort: info.remotePort ?? null,
            port: info.port,
            connectedAt: info.connectedAt.toISOString(),
            protocol: info.protocol || null,
            readyState: socket.readyState,
            isPrimary: socket === this.connectionManager.getPrimarySocket()
        }));

        return {
            enabled: this.enabled,
            host: this.host,
            port: this.port,
            configuredPorts: [...this.ports],
            listeningPorts: [], // We are client-only now
            connected: this.isConnected(),
            connectedAt: connectionInfos.length > 0 ? connectionInfos[0].connectedAt : null,
            activePort: connectionInfos.length > 0 ? connectionInfos[0].port : null,
            negotiatedProtocol: connectionInfos.length > 0 ? connectionInfos[0].protocol : null,
            supportedProtocols: [...this.negotiatedProtocols],
            supportedOpcodes: ['automation_request'],
            expectedResponseOpcodes: ['automation_response'],
            capabilityTokenRequired: Boolean(this.capabilityToken),
            lastHandshakeAt: this.lastHandshakeAt?.toISOString() ?? null,
            lastHandshakeMetadata: this.lastHandshakeMetadata ?? null,
            lastHandshakeAck: this.lastHandshakeAck ?? null,
            lastHandshakeFailure: this.lastHandshakeFailure
                ? { reason: this.lastHandshakeFailure.reason, at: this.lastHandshakeFailure.at.toISOString() }
                : null,
            lastDisconnect: this.lastDisconnect
                ? { code: this.lastDisconnect.code, reason: this.lastDisconnect.reason, at: this.lastDisconnect.at.toISOString() }
                : null,
            lastError: this.lastError
                ? { message: this.lastError.message, at: this.lastError.at.toISOString() }
                : null,
            lastMessageAt: this.connectionManager.getLastMessageTime()?.toISOString() ?? null,
            lastRequestSentAt: this.requestTracker.getLastRequestSentAt()?.toISOString() ?? null,
            pendingRequests: this.requestTracker.getPendingCount(),
            pendingRequestDetails: this.requestTracker.getPendingDetails(),
            connections: connectionInfos,
            webSocketListening: false,
            serverLegacyEnabled: this.serverLegacyEnabled,
            serverName: this.serverName,
            serverVersion: this.serverVersion,
            maxConcurrentConnections: this.maxConcurrentConnections,
            maxPendingRequests: this.requestTracker.getMaxPendingRequests(),
            heartbeatIntervalMs: this.connectionManager.getHeartbeatIntervalMs()
        };
    }

    async sendAutomationRequest<T = AutomationBridgeResponseMessage>(
        action: string,
        payload: Record<string, unknown> = {},
        options: { timeoutMs?: number } = {}
    ): Promise<T> {
        if (!this.isConnected()) {
            if (this.enabled) {
                this.log.info('Automation bridge not connected, attempting lazy connection...');

                // Avoid multiple simultaneous connection attempts using lock
                if (!this.connectionPromise && !this.connectionLock) {
                    this.connectionLock = true;
                    this.connectionPromise = new Promise<void>((resolve, reject) => {
                        const onConnect = () => {
                            cleanup(); resolve();
                        };
                        // We map errors to rejects, but we should be careful about which errors.
                        // A socket error might happen during connection.
                        const onError = (err: any) => {
                            cleanup(); reject(err);
                        };
                        // Also listen for handshake failure
                        const onHandshakeFail = (err: any) => {
                            cleanup(); reject(new Error(`Handshake failed: ${err.reason}`));
                        };

                        const cleanup = () => {
                            this.off('connected', onConnect);
                            this.off('error', onError);
                            this.off('handshakeFailed', onHandshakeFail);
                            // Clear lock and promise so next attempt can try again
                            this.connectionLock = false;
                            this.connectionPromise = undefined;
                        };

                        this.once('connected', onConnect);
                        this.once('error', onError);
                        this.once('handshakeFailed', onHandshakeFail);

                        try {
                            this.startClient();
                        } catch (e) {
                            onError(e);
                        }
                    });
                }

                try {
                    // Wait for connection with a short timeout for the connection itself
                    const connectTimeout = 5000;
                    let timeoutId: ReturnType<typeof setTimeout> | undefined;
                    const timeoutPromise = new Promise<never>((_, reject) => {
                        timeoutId = setTimeout(() => reject(new Error('Lazy connection timeout')), connectTimeout);
                    });

                    try {
                        await Promise.race([this.connectionPromise, timeoutPromise]);
                    } finally {
                        if (timeoutId) clearTimeout(timeoutId);
                    }
                } catch (err: any) {
                    this.log.error('Lazy connection failed', err);
                    // We don't throw here immediately, we let the isConnected check fail below 
                    // or throw a specific error.
                    // Actually, if connection failed, we should probably fail the request.
                    throw new Error(`Failed to establish connection to Unreal Engine: ${err.message}`);
                }
            } else {
                throw new Error('Automation bridge disabled');
            }
        }

        if (!this.isConnected()) {
            throw new Error('Automation bridge not connected');
        }

        // Check if we need to queue (unless it's a priority request which standard ones are not)
        // We use requestTracker directly to check limit as it's the source of truth
        // Note: requestTracker exposes maxPendingRequests via constructor but generic check logic isn't public
        // We assumed getPendingCount() is available
        if (this.requestTracker.getPendingCount() >= this.requestTracker.getMaxPendingRequests()) {
            return new Promise<T>((resolve, reject) => {
                this.queuedRequestItems.push({
                    resolve,
                    reject,
                    action,
                    payload,
                    options
                });
            });
        }

        return this.sendRequestInternal<T>(action, payload, options);
    }

    private async sendRequestInternal<T>(
        action: string,
        payload: Record<string, unknown>,
        options: { timeoutMs?: number }
    ): Promise<T> {
        const timeoutMs = options.timeoutMs ?? 60000; // Increased default timeout to 60s

        // Check for coalescing
        const coalesceKey = this.requestTracker.createCoalesceKey(action, payload);
        if (coalesceKey) {
            const existing = this.requestTracker.getCoalescedRequest(coalesceKey);
            if (existing) {
                return existing as unknown as T;
            }
        }

        const { requestId, promise } = this.requestTracker.createRequest(action, payload, timeoutMs);

        if (coalesceKey) {
            this.requestTracker.setCoalescedRequest(coalesceKey, promise);
        }

        const message: AutomationBridgeMessage = {
            type: 'automation_request',
            requestId,
            action,
            payload
        };

        const resultPromise = promise as unknown as Promise<T>;

        // Ensure we process the queue when this request finishes
        resultPromise.finally(() => {
            this.processRequestQueue();
        }).catch(() => { }); // catch to prevent unhandled rejection during finally chain? no, finally returns new promise

        if (this.send(message)) {
            this.requestTracker.updateLastRequestSentAt();
            return resultPromise;
        } else {
            this.requestTracker.rejectRequest(requestId, new Error('Failed to send request'));
            throw new Error('Failed to send request');
        }
    }

    private processRequestQueue() {
        if (this.queuedRequestItems.length === 0) return;

        // while we have capacity and items
        while (
            this.queuedRequestItems.length > 0 &&
            this.requestTracker.getPendingCount() < this.requestTracker.getMaxPendingRequests()
        ) {
            const item = this.queuedRequestItems.shift();
            if (item) {
                this.sendRequestInternal(item.action, item.payload, item.options)
                    .then(item.resolve)
                    .catch(item.reject);
            }
        }
    }

    send(payload: AutomationBridgeMessage): boolean {
        const primarySocket = this.connectionManager.getPrimarySocket();
        if (!primarySocket || primarySocket.readyState !== WebSocket.OPEN) {
            this.log.warn('Attempted to send automation message without an active primary connection');
            return false;
        }
        try {
            primarySocket.send(JSON.stringify(payload));
            return true;
        } catch (error) {
            this.log.error('Failed to send automation message', error);
            const errObj = error instanceof Error ? error : new Error(String(error));
            const primaryInfo = this.connectionManager.getActiveSockets().get(primarySocket);
            const errorWithPort = Object.assign(errObj, { port: primaryInfo?.port });
            this.emitAutomation('error', errorWithPort);
            return false;
        }
    }

    private broadcast(payload: AutomationBridgeMessage): boolean {
        const sockets = this.connectionManager.getActiveSockets();
        if (sockets.size === 0) {
            this.log.warn('Attempted to broadcast automation message without any active connections');
            return false;
        }
        let sentCount = 0;
        for (const [socket] of sockets) {
            if (socket.readyState === WebSocket.OPEN) {
                try {
                    socket.send(JSON.stringify(payload));
                    sentCount++;
                } catch (error) {
                    this.log.error('Failed to broadcast automation message to socket', error);
                }
            }
        }
        return sentCount > 0;
    }

    private emitAutomation<K extends keyof AutomationBridgeEvents>(
        event: K,
        ...args: Parameters<AutomationBridgeEvents[K]>
    ): void {
        this.emit(event, ...args);
    }
}
