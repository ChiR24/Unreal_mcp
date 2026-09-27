import { DEFAULT_REQUEST_TIMEOUT_MS, requestTimeoutOverrideMs } from '../config.js';
import { bridgeNotConnectedMessage } from './bridge-config.js';
import { McpRequestCancelledError } from './request-cancellation-error.js';
import { ConnectionLifecycle } from './connection-lifecycle.js';
import { RequestCorrelation } from './request-correlation.js';
import type { RequestTracker } from './request-tracker.js';
import type {
    AutomationBridgeMessage,
    AutomationBridgeResponseMessage,
    AutomationRequestOptions,
    ConnectionControlDependencies,
    NaturalTimeoutNotification,
    QueuedRequestItem
} from './types.js';


export interface AutomationRequestDispatcherDependencies extends ConnectionControlDependencies {
    readonly enabled: boolean;
    readonly maxQueuedRequests: number;
    readonly connectionTimeoutMs: number;
    readonly requestTracker: RequestTracker;
    readonly isConnected: () => boolean;
    readonly send: (payload: AutomationBridgeMessage) => boolean;
    /** Connection id of the socket the next send will use, for owner stamping. */
}

export class AutomationRequestDispatcher {
    private queuedRequestItems: QueuedRequestItem[] = [];
    private readonly correlation = new RequestCorrelation();
    private readonly connection: ConnectionLifecycle;

    constructor(private readonly deps: AutomationRequestDispatcherDependencies) {
        this.connection = new ConnectionLifecycle({
            enabled: deps.enabled,
            connectionTimeoutMs: deps.connectionTimeoutMs,
            log: deps.log,
            startClient: deps.startClient,
            abortPendingConnection: deps.abortPendingConnection,
            describeTarget: deps.describeTarget,
            once: deps.once,
            off: deps.off
        });
        deps.requestTracker.setNaturalTimeoutObserver((notification) => this.handleNaturalTimeout(notification));
    }

    /** The lazy connect every request runs, on demand. Rejects when Unreal is unreachable. */
    public connect(): Promise<void> {
        return this.connection.ensureConnected();
    }

    public async sendAutomationRequest<T = AutomationBridgeResponseMessage>(
        action: string,
        payload: Record<string, unknown> = {},
        options: AutomationRequestOptions = {}
    ): Promise<T> {
        if (!this.deps.isConnected()) {
            await this.connection.ensureConnected();
        }

        if (!this.deps.isConnected()) {
            throw new Error(bridgeNotConnectedMessage(this.deps.describeTarget?.()));
        }

        if (this.deps.requestTracker.getPendingCount() >= this.deps.requestTracker.getMaxPendingRequests()) {
            if (this.queuedRequestItems.length >= this.deps.maxQueuedRequests) {
                throw new Error(`Automation bridge request queue is full (max: ${this.deps.maxQueuedRequests}). Please retry later.`);
            }

            return new Promise<T>((resolve, reject) => {
                const item: QueuedRequestItem = {
                    resolve: resolve as (value: unknown) => void,
                    reject: reject as (reason: unknown) => void,
                    action,
                    payload,
                    options,
                    mcpRequestId: options.mcpRequestId
                };
                this.queuedRequestItems.push(item);
                this.correlation.registerQueued(options.mcpRequestId, item);
            });
        }

        return this.sendRequestInternal<T>(action, payload, options);
    }

    /** The MCP requests awaiting an automation id, for progress fan-out. */
    public mcpRequestIdsForAuto(autoId: string): string[] {
        return this.correlation.mcpRequestIdsForAuto(autoId);
    }

    public stop(reason: Error): void {
        this.connection.abort(reason);
        this.rejectQueuedRequests(reason);
        this.deps.requestTracker.rejectAll(reason);
        this.correlation.clear();
    }

    public rejectQueuedRequests(error: Error): void {
        for (const item of this.queuedRequestItems.splice(0)) {
            item.reject(error);
        }
    }

    public rejectPendingRequests(error: Error): void {
        this.deps.requestTracker.rejectAll(error);
    }

    /**
     * Cancel every automation request correlated to an MCP request id.
     *
     * Rejects queued items that never left the bridge and, for each inflight
     * automation request, sends a targeted `cancel_request` frame to Unreal and
     * rejects the caller's promise. Convergence point for both SDK AbortSignal cancellation
     * and explicit `notifications/cancelled` handling. Non-throwing and
     * idempotent: a second call for the same id is a no-op once torn down.
     */
    public cancelMcpRequest(mcpRequestId: string, reason: string): void {
        if (!mcpRequestId) return;

        const queued = this.correlation.takeQueued(mcpRequestId);
        if (queued.length > 0) {
            this.queuedRequestItems = this.queuedRequestItems.filter((it) => !queued.includes(it));
            const error = new McpRequestCancelledError(`MCP request cancelled: ${reason}`, reason);
            for (const it of queued) it.reject(error);
        }

        this.correlation.cancel(
            mcpRequestId,
            {
                sendFrame: (autoId) => this.deps.send({ type: 'cancel_request', requestId: autoId, reason }),
                rejectUnderlying: (autoId) => this.deps.requestTracker.rejectRequest(
                    autoId,
                    new McpRequestCancelledError(`MCP request cancelled: ${reason}`, reason)
                ),
                log: this.deps.log
            }
        );
    }

    /**
     * Terminal natural-timeout settlement installed on the tracker. Runs inside
     * the tracker's timer callback after the pending entry and its timers are
     * already cleared. Settles correlation BEFORE the best-effort advisory
     * cancel frame so an explicit-cancel race can never emit a second frame
     * for this automation id; the shared-promise finalizer's settle then
     * becomes an idempotent no-op.
     */
    private handleNaturalTimeout(notification: NaturalTimeoutNotification): void {
        // Settled first, so an explicit cancel racing this timeout cannot emit
        // a second frame. Best-effort: a failed send is logged, never thrown
        // into the tracker's timer callback.
        this.correlation.settle(notification.requestId);
        try {
            this.deps.send({ type: 'cancel_request', requestId: notification.requestId, reason: `natural timeout (${notification.kind})` });
        } catch {
            this.deps.log.warn('Failed to deliver natural-timeout cancel_request frame to Unreal', {
                requestId: notification.requestId,
                kind: notification.kind
            });
        }
    }

    private async sendRequestInternal<T>(
        action: string,
        payload: Record<string, unknown>,
        options: AutomationRequestOptions
    ): Promise<T> {
        const timeoutMs = options.timeoutMs ?? requestTimeoutOverrideMs() ?? DEFAULT_REQUEST_TIMEOUT_MS;
        // Mirror of the plugin authority's reconciliation
        // (McpConnectionManagerAuthority.cpp): the payload may legitimately carry
        // `action` and `subAction` with different values (handler aliases rewrite
        // `subAction` to the native name), and the plugin resolves subAction
        // first. Normalizing here keeps both transports applying the same rule,
        // so a decoy `action` can never steer a dispatcher away from what the
        // plugin authorized. A shallow copy avoids mutating a caller-owned args
        // object that a queued retry may reuse.
        const hasAction =
            typeof payload.action === 'string' && payload.action.length > 0;
        const hasSubAction =
            typeof payload.subAction === 'string' && payload.subAction.length > 0;
        const reconciledPayload =
            hasSubAction && (!hasAction || payload.action !== payload.subAction)
                ? { ...payload, action: payload.subAction }
                : hasAction && !hasSubAction
                  ? { ...payload, subAction: payload.action }
                  : payload;
        const { requestId, promise } = this.deps.requestTracker.createRequest({ action, payload: reconciledPayload, timeoutMs });
        const resultPromise = promise;
        void resultPromise
            .then(() => this.processRequestQueue(), () => this.processRequestQueue())
            .finally(() => this.correlation.settle(requestId))
            .catch(() => undefined);

        const envelope: AutomationBridgeMessage = { type: 'automation_request', requestId, action, payload: reconciledPayload };
        if (options.correlationId !== undefined) envelope.correlationId = options.correlationId;
        if (options.consent !== undefined) envelope.consent = options.consent;
        if (options.expectedRevisions !== undefined) envelope.expectedRevisions = options.expectedRevisions;
        if (this.deps.send(envelope)) {
            this.deps.requestTracker.updateLastRequestSentAt();
            this.correlation.register(options.mcpRequestId, requestId);
            return resultPromise as Promise<T>;
        }

        this.deps.requestTracker.rejectRequest(requestId, new Error('Failed to send request'));
        throw new Error('Failed to send request');
    }

    private processRequestQueue(): void {
        if (this.queuedRequestItems.length === 0) return;
        if (!this.deps.isConnected()) {
            this.rejectQueuedRequests(new Error('Connection lost'));
            return;
        }

        while (
            this.queuedRequestItems.length > 0 &&
            this.deps.requestTracker.getPendingCount() < this.deps.requestTracker.getMaxPendingRequests()
        ) {
            const item = this.queuedRequestItems.shift();
            if (!item) continue;

            this.correlation.detachQueued(item);
            try {
                const requestPromise = this.sendRequestInternal(item.action, item.payload, item.options);
                requestPromise.then(item.resolve, item.reject);
            } catch (error) {
                // Synchronous setup failure (e.g. tracker at capacity): reject the
                // dequeued caller and keep draining the remaining items.
                item.reject(error);
            }
        }
    }
}
