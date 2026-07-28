import type { Logger } from '../utils/logging/logger.js';
import { McpRequestCancelledError } from './request-cancellation-error.js';
import type { QueuedRequestItem } from './types.js';

interface CorrelatedSubscriber {
    readonly subscriberId: string;
    readonly mcpRequestId: string;
    readonly autoId: string;
    resolve: (value: unknown) => void;
    reject: (reason: unknown) => void;
}

type CancelFrameSender = (autoId: string) => void;
type UnderlyingRejecter = (autoId: string) => void;

interface CancelDelivery {
    readonly sendFrame: CancelFrameSender;
    readonly rejectUnderlying: UnderlyingRejecter;
    readonly log: Logger;
}

/**
 * Owns the MCP-request -> automation-request correlation and cancellation state
 * for the bridge request dispatcher.
 *
 * A single MCP request may fan out to several automation requests (one tool
 * call spawns multiple), and a read-only automation request may be *coalesced*
 * so several MCP requests share one underlying automation request id. To let
 * coalesced callers cancel independently, every caller gets its own subscriber
 * entry; the underlying automation request (and its `cancel_request` frame) is
 * only torn down when the last subscriber for that automation id cancels.
 *
 * All maps are bounded: they are cleaned on natural settle (`settle`) and on
 * cancellation (`cancel`/`takeQueued`), so a settled or cancelled request never
 * retains correlation state.
 */
export class RequestCorrelation {
    private readonly subscribers = new Map<string, CorrelatedSubscriber>();
    private readonly byMcp = new Map<string, Set<string>>();
    private readonly byAuto = new Map<string, Set<string>>();
    private readonly queuedByMcp = new Map<string, QueuedRequestItem[]>();
    private readonly autoByCoalesceKey = new Map<string, string>();
    private readonly coalesceKeyByAuto = new Map<string, string>();
    private subscriberSeq = 0;

    public register(
        mcpRequestId: string | undefined,
        autoId: string,
        resolve: (value: unknown) => void,
        reject: (reason: unknown) => void
    ): string {
        const subscriberId = `sub_${++this.subscriberSeq}`;
        this.subscribers.set(subscriberId, {
            subscriberId,
            mcpRequestId: mcpRequestId ?? '',
            autoId,
            resolve,
            reject
        });
        if (mcpRequestId) {
            this.addToSet(this.byMcp, mcpRequestId, subscriberId);
        }
        this.addToSet(this.byAuto, autoId, subscriberId);
        return subscriberId;
    }

    /**
     * The MCP request ids currently subscribed to an automation request.
     *
     * A coalesced read is shared by several MCP requests, so progress observed
     * for one automation id legitimately fans out to each subscriber — and to
     * no one else, which is what keeps an unrelated request from ever seeing
     * another request's progress. Deduplicated: a request that opened several
     * subscribers for the same automation id is reported once.
     */
    public mcpRequestIdsForAuto(autoId: string): string[] {
        const ids = this.byAuto.get(autoId);
        if (!ids) return [];
        const owners = new Set<string>();
        for (const id of ids) {
            const mcpRequestId = this.subscribers.get(id)?.mcpRequestId;
            if (mcpRequestId) owners.add(mcpRequestId);
        }
        return [...owners];
    }

    public noteCoalesceKey(coalesceKey: string, autoId: string): void {
        this.autoByCoalesceKey.set(coalesceKey, autoId);
        this.coalesceKeyByAuto.set(autoId, coalesceKey);
    }

    public getAutoIdForCoalesceKey(coalesceKey: string): string | undefined {
        return this.autoByCoalesceKey.get(coalesceKey);
    }

    public registerQueued(mcpRequestId: string | undefined, item: QueuedRequestItem): void {
        if (!mcpRequestId) return;
        const owned = this.queuedByMcp.get(mcpRequestId) ?? [];
        owned.push(item);
        this.queuedByMcp.set(mcpRequestId, owned);
    }

    public detachQueued(item: QueuedRequestItem): void {
        if (!item.mcpRequestId) return;
        const owned = this.queuedByMcp.get(item.mcpRequestId);
        if (!owned) return;
        const index = owned.indexOf(item);
        if (index >= 0) owned.splice(index, 1);
        if (owned.length === 0) this.queuedByMcp.delete(item.mcpRequestId);
    }

    /**
     * Remove and return the queued items owned by an MCP request id so the
     * dispatcher can drop them from its queue array and reject them. Does not
     * reject; the caller owns the rejection so it can also remove from the queue.
     */
    public takeQueued(mcpRequestId: string): QueuedRequestItem[] {
        const owned = this.queuedByMcp.get(mcpRequestId);
        if (!owned) return [];
        this.queuedByMcp.delete(mcpRequestId);
        return owned;
    }

    /**
     * Tear down every subscriber correlated to an automation id. Called when the
     * underlying request settles (resolve, timeout, or transport failure) so a
     * late cancellation is harmless and state does not retain.
     */
    public settle(autoId: string): void {
        const ids = this.byAuto.get(autoId);
        if (!ids) {
            const key = this.coalesceKeyByAuto.get(autoId);
            if (key) {
                this.coalesceKeyByAuto.delete(autoId);
                this.autoByCoalesceKey.delete(key);
            }
            return;
        }
        for (const id of ids) {
            const sub = this.subscribers.get(id);
            if (sub?.mcpRequestId) {
                const mcpSubscribers = this.byMcp.get(sub.mcpRequestId);
                mcpSubscribers?.delete(id);
                if (mcpSubscribers?.size === 0) this.byMcp.delete(sub.mcpRequestId);
            }
            this.subscribers.delete(id);
        }
        this.byAuto.delete(autoId);
        const key = this.coalesceKeyByAuto.get(autoId);
        if (key) {
            this.coalesceKeyByAuto.delete(autoId);
            this.autoByCoalesceKey.delete(key);
        }
    }

    /**
     * Reject the caller-local promises of every subscriber correlated to an MCP
     * request id. Reference-counted: the underlying `cancel_request` frame and
     * tracker rejection are only emitted for an automation id once the last of
     * its subscribers cancels, so a coalesced follower is never torn down by an
     * unrelated origin cancellation. Non-throwing: a frame delivery failure is
     * logged without tokens and the remaining frames are still attempted.
     */
    public cancel(
        mcpRequestId: string,
        reason: string,
        delivery: CancelDelivery
    ): void {
        const ids = this.byMcp.get(mcpRequestId);
        if (!ids || ids.size === 0) return;

        for (const id of [...ids]) {
            const sub = this.subscribers.get(id);
            if (!sub) continue;

            const autoSet = this.byAuto.get(sub.autoId);
            autoSet?.delete(id);
            const isLastForAuto = !autoSet || autoSet.size === 0;

            this.subscribers.delete(id);
            this.byMcp.get(sub.mcpRequestId)?.delete(id);
            try {
                sub.reject(new McpRequestCancelledError(`MCP request cancelled: ${reason}`, reason));
            } catch {
                // A throwing subscriber reject is a programming error that must
                // not break cancellation of the remaining subscribers.
            }

            if (!isLastForAuto) continue;

            this.byAuto.delete(sub.autoId);
            const key = this.coalesceKeyByAuto.get(sub.autoId);
            if (key) {
                this.coalesceKeyByAuto.delete(sub.autoId);
                this.autoByCoalesceKey.delete(key);
            }
            this.deliverCancelFrame(sub.autoId, delivery);
            try {
                delivery.rejectUnderlying(sub.autoId);
            } catch {
                // Underlying tracker rejection is best-effort.
            }
        }
        this.byMcp.delete(mcpRequestId);
    }

    private deliverCancelFrame(
        autoId: string,
        delivery: CancelDelivery
    ): void {
        try {
            delivery.sendFrame(autoId);
        } catch {
            delivery.log.warn('Failed to deliver cancel_request frame to Unreal', {
                autoId,
                correlationEntries: this.byMcp.size
            });
        }
    }

    public clear(): void {
        this.subscribers.clear();
        this.byMcp.clear();
        this.byAuto.clear();
        this.queuedByMcp.clear();
        this.autoByCoalesceKey.clear();
        this.coalesceKeyByAuto.clear();
    }

    private addToSet(map: Map<string, Set<string>>, key: string, value: string): void {
        const set = map.get(key) ?? new Set<string>();
        set.add(value);
        map.set(key, set);
    }
}
