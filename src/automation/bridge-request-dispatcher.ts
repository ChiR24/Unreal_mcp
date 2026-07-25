import { config } from '../config.js';
import { McpRequestCancelledError } from './request-cancellation-error.js';
import { ConnectionLifecycle } from './connection-lifecycle.js';
import { RequestCorrelation } from './request-correlation.js';
import { ConsentGrantSchema } from '../tools/catalog/capabilities/semantic/authorization.js';
import type { Logger } from '../utils/logging/logger.js';
import type { RequestTracker } from './request-tracker.js';
import type {
    AutomationBridgeEvents,
    AutomationBridgeMessage,
    AutomationBridgeResponseMessage,
    QueuedRequestItem
} from './types.js';

type AutomationRequestOptions = { timeoutMs?: number; mcpRequestId?: string; correlationId?: string; consent?: { capability: string; acknowledge: 'explicit' | 'elevated' } };

export interface AutomationRequestDispatcherDependencies {
    readonly enabled: boolean;
    readonly maxQueuedRequests: number;
    readonly connectionTimeoutMs: number;
    readonly requestTracker: RequestTracker;
    readonly log: Logger;
    readonly isConnected: () => boolean;
    readonly send: (payload: AutomationBridgeMessage) => boolean;
    readonly startClient: () => void;
    readonly abortPendingConnection: (reason: Error) => void;
    readonly once: <K extends keyof AutomationBridgeEvents>(
        event: K,
        listener: AutomationBridgeEvents[K]
    ) => void;
    readonly off: <K extends keyof AutomationBridgeEvents>(
        event: K,
        listener: AutomationBridgeEvents[K]
    ) => void;
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
            once: deps.once,
            off: deps.off
        });
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
            throw new Error('Automation bridge not connected');
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
     * subscriber, rejects the caller-local promise and (when it is the last
     * subscriber for a given automation id) sends a targeted `cancel_request`
     * frame to Unreal. Convergence point for both SDK AbortSignal cancellation
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
            reason,
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

    private async sendRequestInternal<T>(
        action: string,
        payload: Record<string, unknown>,
        options: AutomationRequestOptions
    ): Promise<T> {
        const timeoutMs = options.timeoutMs ?? config.MCP_REQUEST_TIMEOUT_MS;
        const coalesceKey = this.deps.requestTracker.createCoalesceKey(action, payload);
        if (coalesceKey) {
            const existing = this.deps.requestTracker.getCoalescedRequest(coalesceKey);
            const autoId = this.correlation.getAutoIdForCoalesceKey(coalesceKey);
            if (existing && autoId) {
                return this.createSubscriberPromise<T>(existing, options.mcpRequestId, autoId);
            }
        }

        const { requestId, promise } = this.deps.requestTracker.createRequest(action, payload, timeoutMs);
        if (coalesceKey) {
            this.deps.requestTracker.setCoalescedRequest(coalesceKey, promise);
            this.correlation.noteCoalesceKey(coalesceKey, requestId);
        }

        const resultPromise = promise.then(castAutomationResponse);
        void resultPromise
            .then(() => this.processRequestQueue(), () => this.processRequestQueue())
            .finally(() => this.correlation.settle(requestId))
            .catch(() => undefined);

        const envelope: AutomationBridgeMessage = { type: 'automation_request', requestId, action, payload };
        if (options.correlationId !== undefined) envelope.correlationId = options.correlationId;
        if (options.consent !== undefined) envelope.consent = options.consent;
        if (this.deps.send(envelope)) {
            this.deps.requestTracker.updateLastRequestSentAt();
            return this.createSubscriberPromise<T>(resultPromise, options.mcpRequestId, requestId);
        }

        this.deps.requestTracker.rejectRequest(requestId, new Error('Failed to send request'));
        throw new Error('Failed to send request');
    }

    /**
     * Wrap an underlying automation request promise in a per-caller subscriber
     * promise. The subscriber follows the shared promise but can be rejected
     * independently on cancellation, which is what lets coalesced callers (who
     * share one underlying automation id) cancel without tearing each other down.
     */
    private createSubscriberPromise<T>(
        shared: Promise<AutomationBridgeResponseMessage>,
        mcpRequestId: string | undefined,
        autoId: string
    ): Promise<T> {
        return new Promise<T>((resolve, reject) => {
            shared.then(
                (value) => resolve(value as T),
                (error) => reject(error)
            );
            this.correlation.register(
                mcpRequestId,
                autoId,
                resolve as (value: unknown) => void,
                reject as (reason: unknown) => void
            );
        });
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
                const requestPromise = this.sendRequestInternal(item.action, item.payload, getQueuedOptions(item.options));
                requestPromise.then(item.resolve, item.reject);
            } catch (error) {
                // Synchronous setup failure (e.g. tracker at capacity): reject the
                // dequeued caller and keep draining the remaining items.
                item.reject(error);
            }
        }
    }
}

function castAutomationResponse(response: AutomationBridgeResponseMessage): AutomationBridgeResponseMessage {
    return response;
}

function getQueuedOptions(options: Record<string, unknown>): AutomationRequestOptions {
    const result: AutomationRequestOptions = {};
    if (typeof options.timeoutMs === 'number') result.timeoutMs = options.timeoutMs;
    if (typeof options.mcpRequestId === 'string') result.mcpRequestId = options.mcpRequestId;
    if (typeof options.correlationId === 'string') result.correlationId = options.correlationId;
    // Re-parsed rather than asserted: this arrives as an untyped record, and a
    // malformed grant must be dropped here rather than forwarded to the plugin.
    const consent = ConsentGrantSchema.safeParse(options.consent);
    if (consent.success) {
        result.consent = { capability: consent.data.capability, acknowledge: consent.data.acknowledge };
    }
    return result;
}
