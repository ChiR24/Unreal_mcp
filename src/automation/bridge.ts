import { EventEmitter } from 'node:events';
import { AutomationBridgeClient } from './bridge-client.js';
import { type AutomationBridgeResolvedConfig, resolveAutomationBridgeConfig } from './bridge-config.js';
import { AutomationRequestDispatcher } from './bridge-request-dispatcher.js';
import type { AutomationBridgeRuntimeState } from './bridge-state.js';
import { buildAutomationBridgeStatus } from './bridge-status.js';
import { CapabilityTokenProvider } from './capability-token-provider.js';
import { ConnectionManager } from './connection-manager.js';
import { HandshakeHandler } from './handshake.js';
import { AutomationLogger } from './log-redaction.js';
import { readBridgeAuthority, type BridgeAuthority } from './message-schema.js';
import { MessageHandler } from './message-handler.js';
import { RequestTracker } from './request-tracker.js';
import type { ExpectedRevisions } from '../tools/catalog/capabilities/semantic/execution-options.js';
import type {
    AutomationBridgeEvents,
    AutomationBridgeMessage,
    AutomationBridgeOptions,
    AutomationBridgeResponseMessage,
    AutomationBridgeStatus,
    AutomationProgressListener,
    AutomationProgressUpdate
} from './types.js';

export class AutomationBridge extends EventEmitter {
    private readonly config: AutomationBridgeResolvedConfig;
    private readonly state: AutomationBridgeRuntimeState = {};
    private readonly connectionManager: ConnectionManager;
    private readonly requestTracker: RequestTracker;
    private readonly capabilityTokenProvider: CapabilityTokenProvider;
    private readonly handshakeHandler: HandshakeHandler;
    private readonly messageHandler: MessageHandler;
    private readonly client: AutomationBridgeClient;
    private readonly requestDispatcher: AutomationRequestDispatcher;
    private readonly log = new AutomationLogger('AutomationBridge');
    private progressListener: AutomationProgressListener | undefined;
    private cancelledListener: ((mcpRequestId: string) => void) | undefined;

    constructor(options: AutomationBridgeOptions = {}) {
        super();

        this.config = resolveAutomationBridgeConfig(options, this.log);
        this.connectionManager = new ConnectionManager(
            this.config.heartbeatIntervalMs,
            this.config.maxInboundMessagesPerMinute,
            this.config.maxInboundAutomationRequestsPerMinute
        );
        this.requestTracker = new RequestTracker(this.config.maxPendingRequests);
        this.capabilityTokenProvider = new CapabilityTokenProvider(this.config.capabilityToken, this.log);
        this.handshakeHandler = new HandshakeHandler(
            this.config.capabilityToken,
            () => this.capabilityTokenProvider.resolve()
        );
        this.messageHandler = new MessageHandler(
            this.requestTracker,
            (event) => this.emitAutomation('automationEvent', event),
            (autoId, update) => this.forwardAutomationProgress(autoId, update)
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
            rejectPendingRequests: (error) => this.requestDispatcher.rejectPendingRequests(error),
            rejectOwnedRequests: (ownerId, error) => this.requestDispatcher.rejectOwnedRequests(ownerId, error)
        });
        this.requestDispatcher = new AutomationRequestDispatcher({
            enabled: this.config.enabled,
            maxQueuedRequests: this.config.maxQueuedRequests,
            connectionTimeoutMs: this.config.connectionTimeoutMs,
            requestTracker: this.requestTracker,
            log: this.log,
            isConnected: () => this.isConnected(),
            send: (payload) => this.client.send(payload),
            getSendOwnerId: () => this.connectionManager.getPrimaryConnectionId(),
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

    /**
     * Install the server-layer sink that turns resolved progress into MCP
     * `notifications/progress`. The bridge deliberately does not own that
     * translation: it knows automation ids, not client progress tokens.
     */
    setRequestProgressListener(listener: AutomationProgressListener | undefined): void {
        this.progressListener = listener;
    }

    /** Fan-in point for progress already resolved to its owning MCP request. */
    reportRequestProgress(mcpRequestId: string, update: AutomationProgressUpdate): void {
        if (!mcpRequestId) return;
        this.progressListener?.(mcpRequestId, update);
    }

    /**
     * Observe requests the client has given up on. Both cancellation paths
     * (SDK abort and `notifications/cancelled`) converge on cancelMcpRequest,
     * so this is the one place that learns about every cancelled request.
     */
    setRequestCancelledListener(listener: ((mcpRequestId: string) => void) | undefined): void {
        this.cancelledListener = listener;
    }

    private forwardAutomationProgress(autoId: string, update: AutomationProgressUpdate): void {
        for (const mcpRequestId of this.requestDispatcher.mcpRequestIdsForAuto(autoId)) {
            this.reportRequestProgress(mcpRequestId, update);
        }
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

    getAuthority(): BridgeAuthority | undefined {
        return readBridgeAuthority(this.state.lastHandshakeMetadata);
    }

    /**
     * True when an EFFECTIVE capability token is available (explicit option,
     * `MCP_AUTOMATION_CAPABILITY_TOKEN`, or the persisted token file). Routes
     * through the provider so a file-backed token is seen, not just the
     * explicit config option — a token the plugin auto-generated must close
     * the offline admin path on this side too.
     */
    async isCapabilityTokenConfigured(): Promise<boolean> {
        return (await this.capabilityTokenProvider.resolve()) !== undefined;
    }

    async sendAutomationRequest<T = AutomationBridgeResponseMessage>(
        action: string,
        payload: Record<string, unknown> = {},
        options: { timeoutMs?: number; mcpRequestId?: string; correlationId?: string; consent?: { capability: string; acknowledge: 'explicit' | 'elevated' }; expectedRevisions?: ExpectedRevisions } = {}
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
        // Advisory: queued work is dropped and the in-flight response is
        // abandoned, but editor work already dispatched to Unreal runs to
        // completion. The client has stopped listening either way, so its
        // notification stream ends here.
        if (requestId) this.cancelledListener?.(requestId);
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
