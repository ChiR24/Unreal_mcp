import { EventEmitter } from 'node:events';
import { AutomationBridgeClient } from './bridge-client.js';
import { type AutomationBridgeResolvedConfig, resolveAutomationBridgeConfig } from './bridge-config.js';
import { AutomationRequestDispatcher } from './bridge-request-dispatcher.js';
import type { AutomationBridgeRuntimeState } from './bridge-state.js';
import { buildAutomationBridgeStatus } from './bridge-status.js';
import { ConnectionManager } from './connection-manager.js';
import { HandshakeHandler } from './handshake.js';
import { AutomationLogger } from './log-redaction.js';
import { MessageHandler } from './message-handler.js';
import { RequestTracker } from './request-tracker.js';
import type {
    AutomationBridgeEvents,
    AutomationBridgeMessage,
    AutomationBridgeOptions,
    AutomationBridgeResponseMessage,
    AutomationBridgeStatus
} from './types.js';

export class AutomationBridge extends EventEmitter {
    private readonly config: AutomationBridgeResolvedConfig;
    private readonly state: AutomationBridgeRuntimeState = {};
    private readonly connectionManager: ConnectionManager;
    private readonly requestTracker: RequestTracker;
    private readonly handshakeHandler: HandshakeHandler;
    private readonly messageHandler: MessageHandler;
    private readonly client: AutomationBridgeClient;
    private readonly requestDispatcher: AutomationRequestDispatcher;
    private readonly log = new AutomationLogger('AutomationBridge');

    constructor(options: AutomationBridgeOptions = {}) {
        super();

        this.config = resolveAutomationBridgeConfig(options, this.log);
        this.connectionManager = new ConnectionManager(
            this.config.heartbeatIntervalMs,
            this.config.maxInboundMessagesPerMinute,
            this.config.maxInboundAutomationRequestsPerMinute
        );
        this.requestTracker = new RequestTracker(this.config.maxPendingRequests);
        this.handshakeHandler = new HandshakeHandler(this.config.capabilityToken);
        this.messageHandler = new MessageHandler(
            this.requestTracker,
            (event) => this.emitAutomation('automationEvent', event)
        );
        this.client = new AutomationBridgeClient({
            config: this.config,
            state: this.state,
            connectionManager: this.connectionManager,
            handshakeHandler: this.handshakeHandler,
            messageHandler: this.messageHandler,
            log: this.log,
            emit: (event, ...args) => this.emitAutomation(event, ...args),
            rejectQueuedRequests: (error) => this.requestDispatcher.rejectQueuedRequests(error),
            rejectPendingRequests: (error) => this.requestDispatcher.rejectPendingRequests(error)
        });
        this.requestDispatcher = new AutomationRequestDispatcher({
            enabled: this.config.enabled,
            maxQueuedRequests: this.config.maxQueuedRequests,
            connectionTimeoutMs: this.config.connectionTimeoutMs,
            requestTracker: this.requestTracker,
            log: this.log,
            isConnected: () => this.isConnected(),
            send: (payload) => this.client.send(payload),
            startClient: () => this.client.startClient(),
            abortPendingConnection: () => this.client.abortPendingConnection(),
            once: (event, listener) => {
                this.once(event, listener);
            },
            off: (event, listener) => {
                this.off(event, listener);
            }
        });
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
        if (!this.config.enabled) {
            this.log.info('Automation bridge disabled by configuration.');
            return;
        }

        this.log.info(`Automation bridge connecting to Unreal server at ${this.getClientUrl()}`);
        this.client.startClient();
    }

    stop(): void {
        if (this.isConnected()) {
            this.client.broadcast({
                type: 'bridge_shutdown',
                timestamp: new Date().toISOString(),
                reason: 'Server shutting down'
            });
        }

        const stopError = new Error('Automation bridge server stopped');
        this.requestDispatcher.stop(stopError);
        this.connectionManager.closeAll(1001, 'Server shutdown');
        this.state.lastHandshakeAck = undefined;
    }

    isConnected(): boolean {
        return this.connectionManager.isConnected();
    }

    getStatus(): AutomationBridgeStatus {
        return buildAutomationBridgeStatus({
            config: this.config,
            state: this.state,
            connectionManager: this.connectionManager,
            requestTracker: this.requestTracker,
            connected: this.isConnected()
        });
    }

    getClientUrl(): string {
        return this.client.getClientUrl();
    }

    async sendAutomationRequest<T = AutomationBridgeResponseMessage>(
        action: string,
        payload: Record<string, unknown> = {},
        options: { timeoutMs?: number; mcpRequestId?: string; correlationId?: string } = {}
    ): Promise<T> {
        return this.requestDispatcher.sendAutomationRequest<T>(action, payload, options);
    }

    /**
     * Cancel every automation request correlated to a canonicalized MCP request
     * id. Convergence point for both SDK AbortSignal cancellation and explicit
     * `notifications/cancelled` handling. Idempotent.
     */
    cancelMcpRequest(requestId: string, reason: string): void {
        this.requestDispatcher.cancelMcpRequest(requestId, reason);
    }

    send(payload: AutomationBridgeMessage): boolean {
        return this.client.send(payload);
    }

    private emitAutomation<K extends keyof AutomationBridgeEvents>(
        event: K,
        ...args: Parameters<AutomationBridgeEvents[K]>
    ): void {
        this.emit(event, ...args);
    }
}
