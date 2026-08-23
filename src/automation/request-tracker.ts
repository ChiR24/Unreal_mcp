import { PendingRequest, AutomationBridgeResponseMessage } from './types.js';
import type {
    NaturalTimeoutKind,
    NaturalTimeoutObserver,
    RequestTrackerRequestSpec
} from './types.js';
import { randomUUID, createHash } from 'node:crypto';
import {
    PROGRESS_EXTENSION_MS,
    MAX_PROGRESS_EXTENSIONS,
    PROGRESS_STALE_THRESHOLD,
    ABSOLUTE_MAX_TIMEOUT_MS
} from '../constants.js';

const READ_ONLY_ACTION_PREFIXES = ['list', 'get_', 'exists', 'search', 'find'];

// Note: The two-step event pattern was disabled because C++ handlers send a single response,
// not request+event. All actions now use simple request-response. The PendingRequest interface
// retains waitForEvent/eventTimeout fields for potential future use.

export class RequestTracker {
    private pendingRequests = new Map<string, PendingRequest>();
    private coalescedRequests = new Map<string, Promise<AutomationBridgeResponseMessage>>();
    private lastRequestSentAt?: Date;
    private naturalTimeoutObserver?: NaturalTimeoutObserver;

    constructor(
        private maxPendingRequests: number
    ) { }

    /**
     * Get the maximum number of pending requests allowed.
     * @returns The configured maximum pending requests limit
     */
    public getMaxPendingRequests(): number {
        return this.maxPendingRequests;
    }

    /**
     * Get the timestamp of when the last request was sent.
     * @returns The Date of last request or undefined if no requests sent yet
     */
    public getLastRequestSentAt(): Date | undefined {
        return this.lastRequestSentAt;
    }

    /**
     * Update the last request sent timestamp.
     * Called when a new request is dispatched.
     */
    public updateLastRequestSentAt(): void {
        this.lastRequestSentAt = new Date();
    }

    /**
     * Install the sink that receives typed terminal notifications for every
     * natural tracker timeout class. The notification fires after the pending
     * entry and its timers are already cleared.
     */
    public setNaturalTimeoutObserver(observer: NaturalTimeoutObserver | undefined): void {
        this.naturalTimeoutObserver = observer;
    }

    /**
     * Create a new pending request with timeout handling.
     * @param spec - Object-style request specification (action, payload, timeoutMs)
     * @returns Object containing the requestId and a promise that resolves with the response
     * @throws Error if max pending requests limit is reached
     */
    public createRequest(
        spec: RequestTrackerRequestSpec
    ): { requestId: string; promise: Promise<AutomationBridgeResponseMessage> } {
        const { action, payload, timeoutMs } = spec;
        if (this.pendingRequests.size >= this.maxPendingRequests) {
            throw new Error(`Max pending requests limit reached (${this.maxPendingRequests})`);
        }

        const requestId = randomUUID();

        const promise = new Promise<AutomationBridgeResponseMessage>((resolve, reject) => {
            const timeout = setTimeout(() => {
                this.settleNaturalTimeout(
                    requestId,
                    'ordinary_deadline',
                    new Error(`Request ${requestId} timed out after ${timeoutMs}ms`)
                );
            }, timeoutMs);

            const absoluteTimeout = setTimeout(() => {
                this.settleNaturalTimeout(
                    requestId,
                    'absolute_deadline',
                    new Error(`Request ${requestId} exceeded absolute max timeout (${ABSOLUTE_MAX_TIMEOUT_MS}ms)`)
                );
            }, ABSOLUTE_MAX_TIMEOUT_MS);

            this.pendingRequests.set(requestId, {
                resolve,
                reject,
                timeout,
                action,
                payload,
                requestedAt: new Date(),
                // Note: waitForEvent and eventTimeoutMs are preserved for potential future use
                // but currently all actions use simple request-response pattern
                waitForEvent: false,
                eventTimeoutMs: timeoutMs,
                // Progress tracking initialization
                extensionCount: 0,
                lastProgressPercent: undefined,
                staleCount: 0,
                absoluteTimeout,
                totalExtensionMs: 0
            });
        });

        return { requestId, promise };
    }

    /**
     * Extend the timeout for a pending request based on progress update.
     * Implements safeguards against deadlock from false "alive" signals:
     * 1. Max extensions limit (MAX_PROGRESS_EXTENSIONS)
     * 2. Stale detection (percent unchanged for PROGRESS_STALE_THRESHOLD updates)
     * 3. Absolute max timeout cap (ABSOLUTE_MAX_TIMEOUT_MS)
     *
     * @param requestId - The request ID to extend
     * @param percent - Current progress percent (0-100)
     * @param message - Optional progress message
     * @returns True if timeout was extended, false if rejected (deadlock prevention)
     */
    public extendTimeout(requestId: string, percent?: number, _message?: string): boolean {
        const pending = this.pendingRequests.get(requestId);
        if (!pending) {
            return false;
        }

        if (pending.extensionCount !== undefined && pending.extensionCount >= MAX_PROGRESS_EXTENSIONS) {
            this.settleNaturalTimeout(
                requestId,
                'extension_cap',
                new Error(
                    `Request ${requestId} exceeded max progress extensions (${MAX_PROGRESS_EXTENSIONS}) - possible deadlock detected`
                )
            );
            return false;
        }

        if (percent !== undefined && pending.lastProgressPercent === percent) {
            pending.staleCount = (pending.staleCount || 0) + 1;
            if (pending.staleCount >= PROGRESS_STALE_THRESHOLD) {
                this.settleNaturalTimeout(
                    requestId,
                    'stale_progress',
                    new Error(
                        `Request ${requestId} stalled - progress unchanged at ${percent}% for ${PROGRESS_STALE_THRESHOLD} updates`
                    )
                );
                return false;
            }
        } else {
            pending.staleCount = 0;
        }

        clearTimeout(pending.timeout);

        const newTimeout = setTimeout(() => {
            this.settleNaturalTimeout(
                requestId,
                'progress_extension_deadline',
                new Error(`Request ${requestId} timed out after extension`)
            );
        }, PROGRESS_EXTENSION_MS);

        pending.timeout = newTimeout;
        pending.extensionCount = (pending.extensionCount || 0) + 1;
        pending.lastProgressPercent = percent;
        pending.totalExtensionMs = (pending.totalExtensionMs || 0) + PROGRESS_EXTENSION_MS;

        return true;
    }

    /**
     * Sole terminal settlement for a natural tracker timeout. Deletes the
     * pending map entry and clears every timer first, then notifies the
     * observer (which settles correlation before delivering the advisory
     * cancel frame) and rejects the shared promise exactly once. Map deletion
     * is the single settle token: a late inbound frame finds no entry.
     */
    private settleNaturalTimeout(requestId: string, kind: NaturalTimeoutKind, error: Error): void {
        const pending = this.pendingRequests.get(requestId);
        if (!pending) return;
        this.cleanupRequest(requestId);
        this.naturalTimeoutObserver?.({
            requestId,
            action: pending.action,
            kind,
            error
        });
        pending.reject(error);
    }

    private clearRequestTimers(pending: PendingRequest): void {
        clearTimeout(pending.timeout);
        if (pending.eventTimeout) clearTimeout(pending.eventTimeout);
        if (pending.absoluteTimeout) clearTimeout(pending.absoluteTimeout);
    }

    /**
     * Clean up request timers and remove from map.
     */
    private cleanupRequest(requestId: string): PendingRequest | undefined {
        const pending = this.pendingRequests.get(requestId);
        if (pending) {
            this.clearRequestTimers(pending);
            this.pendingRequests.delete(requestId);
        }
        return pending;
    }

    public getPendingRequest(requestId: string): PendingRequest | undefined {
        return this.pendingRequests.get(requestId);
    }

    public resolveRequest(requestId: string, response: AutomationBridgeResponseMessage): void {
        const pending = this.cleanupRequest(requestId);
        if (pending) {
            pending.resolve(response);
        }
    }

    public rejectRequest(requestId: string, error: Error): void {
        const pending = this.cleanupRequest(requestId);
        if (pending) {
            pending.reject(error);
        }
    }

    public rejectAll(error: Error): void {
        for (const [, pending] of this.pendingRequests) {
            this.clearRequestTimers(pending);
            pending.reject(error);
        }
        this.pendingRequests.clear();
    }

    /**
     * Stamp the connection id that carried a pending request's frame. Called
     * synchronously after a successful send, so the owner is the socket that
     * actually received the frame.
     */
    public setOwnerId(requestId: string, ownerId: string): void {
        const pending = this.pendingRequests.get(requestId);
        if (pending) {
            pending.ownerId = ownerId;
        }
    }

    /**
     * Settle every pending request owned by a removed connection. Deletes each
     * entry via cleanupRequest (timers cleared, map deleted) before rejecting,
     * so settlement is exactly once and idempotent. Never notifies the
     * natural-timeout observer: a disconnect is an explicit non-notify class
     * and must not emit an advisory cancel_request frame.
     */
    public rejectOwnedBy(ownerId: string, error: Error): number {
        let settled = 0;
        for (const [requestId, pending] of this.pendingRequests) {
            if (pending.ownerId === ownerId) {
                this.cleanupRequest(requestId);
                pending.reject(error);
                settled++;
            }
        }
        return settled;
    }

    public getPendingCount(): number {
        return this.pendingRequests.size;
    }

    public getPendingDetails(): Array<{ requestId: string; action: string; ageMs: number }> {
        const now = Date.now();
        return Array.from(this.pendingRequests.entries()).map(([id, pending]) => ({
            requestId: id,
            action: pending.action,
            ageMs: Math.max(0, now - pending.requestedAt.getTime())
        }));
    }

    public getCoalescedRequest(key: string): Promise<AutomationBridgeResponseMessage> | undefined {
        return this.coalescedRequests.get(key);
    }

    public setCoalescedRequest(key: string, promise: Promise<AutomationBridgeResponseMessage>): void {
        this.coalescedRequests.set(key, promise);
        // Remove from map when settled
        promise.finally(() => {
            if (this.coalescedRequests.get(key) === promise) {
                this.coalescedRequests.delete(key);
            }
        }).catch(() => undefined);
    }

    public createCoalesceKey(action: string, payload: Record<string, unknown>): string {
        // Only coalesce read-only operations
        if (!READ_ONLY_ACTION_PREFIXES.some(a => action.startsWith(a))) return '';

        // Create a stable hash of the payload
        const stablePayload = JSON.stringify(stabilizeJsonValue(payload));
        return `${action}:${createHash('md5').update(stablePayload).digest('hex')}`;
    }
}

function stabilizeJsonValue(value: unknown): unknown {
    if (Array.isArray(value)) {
        return value.map(item => stabilizeJsonValue(item));
    }

    if (value && typeof value === 'object') {
        return Object.fromEntries(
            Object.entries(value as Record<string, unknown>)
                .sort(([left], [right]) => left.localeCompare(right))
                .map(([key, child]) => [key, stabilizeJsonValue(child)])
        );
    }

    return value;
}
