import { PendingRequest, AutomationBridgeResponseMessage } from './types.js';
import { randomUUID, createHash } from 'node:crypto';
import {
    PROGRESS_EXTENSION_MS,
    MAX_PROGRESS_EXTENSIONS,
    PROGRESS_STALE_THRESHOLD,
    ABSOLUTE_MAX_TIMEOUT_MS
} from '../constants.js';

// Note: The two-phase event pattern was disabled because C++ handlers send a single response,
// not request+event. All actions now use simple request-response. The PendingRequest interface
// retains waitForEvent/eventTimeout fields for potential future use.

export class RequestTracker {
    private pendingRequests = new Map<string, PendingRequest>();
    private coalescedRequests = new Map<string, Promise<AutomationBridgeResponseMessage>>();
    private lastRequestSentAt?: Date;


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
     * Create a new pending request with timeout handling.
     * @param action - The action name being requested
     * @param payload - The request payload
     * @param timeoutMs - Timeout in milliseconds before the request fails
     * @returns Object containing the requestId and a promise that resolves with the response
     * @throws Error if max pending requests limit is reached
     */
    public createRequest(
        action: string,
        payload: Record<string, unknown>,
        timeoutMs: number
    ): { requestId: string; promise: Promise<AutomationBridgeResponseMessage> } {
        if (this.pendingRequests.size >= this.maxPendingRequests) {
            throw new Error(`Max pending requests limit reached (${this.maxPendingRequests})`);
        }

        const requestId = randomUUID();

        const promise = new Promise<AutomationBridgeResponseMessage>((resolve, reject) => {
            const timeout = setTimeout(() => {
                if (this.pendingRequests.has(requestId)) {
                    this.pendingRequests.delete(requestId);
                    reject(new Error(`Request ${requestId} timed out after ${timeoutMs}ms`));
                }
            }, timeoutMs);

            // Set up absolute timeout cap to prevent indefinite extension
            const absoluteTimeout = setTimeout(() => {
                if (this.pendingRequests.has(requestId)) {
                    this.pendingRequests.delete(requestId);
                    const totalMs = ABSOLUTE_MAX_TIMEOUT_MS;
                    reject(new Error(`Request ${requestId} exceeded absolute max timeout (${totalMs}ms)`));
                }
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

        // Check 1: Max extensions limit
        if (pending.extensionCount !== undefined && pending.extensionCount >= MAX_PROGRESS_EXTENSIONS) {
            pending.reject(new Error(
                `Request ${requestId} exceeded max progress extensions (${MAX_PROGRESS_EXTENSIONS}) - possible deadlock detected`
            ));
            this.cleanupRequest(requestId);
            return false;
        }

        // Check 2: Stale detection - same percent for too many updates
        if (percent !== undefined && pending.lastProgressPercent === percent) {
            pending.staleCount = (pending.staleCount || 0) + 1;
            if (pending.staleCount >= PROGRESS_STALE_THRESHOLD) {
                pending.reject(new Error(
                    `Request ${requestId} stalled - progress unchanged at ${percent}% for ${PROGRESS_STALE_THRESHOLD} updates`
                ));
                this.cleanupRequest(requestId);
                return false;
            }
        } else {
            // Reset stale count on progress change
            pending.staleCount = 0;
        }

        // Clear existing timeout and set new one
        clearTimeout(pending.timeout);
        
        const newTimeout = setTimeout(() => {
            if (this.pendingRequests.has(requestId)) {
                this.pendingRequests.delete(requestId);
                pending.reject(new Error(`Request ${requestId} timed out after extension`));
            }
        }, PROGRESS_EXTENSION_MS);

        pending.timeout = newTimeout;
        pending.extensionCount = (pending.extensionCount || 0) + 1;
        pending.lastProgressPercent = percent;
        pending.totalExtensionMs = (pending.totalExtensionMs || 0) + PROGRESS_EXTENSION_MS;

        return true;
    }

    /**
     * Clean up request timers and remove from map.
     */
    private cleanupRequest(requestId: string): void {
        const pending = this.pendingRequests.get(requestId);
        if (pending) {
            clearTimeout(pending.timeout);
            if (pending.eventTimeout) clearTimeout(pending.eventTimeout);
            if (pending.absoluteTimeout) clearTimeout(pending.absoluteTimeout);
            this.pendingRequests.delete(requestId);
        }
    }

    public getPendingRequest(requestId: string): PendingRequest | undefined {
        return this.pendingRequests.get(requestId);
    }

    public resolveRequest(requestId: string, response: AutomationBridgeResponseMessage): void {
        const pending = this.pendingRequests.get(requestId);
        if (pending) {
            clearTimeout(pending.timeout);
            if (pending.eventTimeout) clearTimeout(pending.eventTimeout);
            if (pending.absoluteTimeout) clearTimeout(pending.absoluteTimeout);
            this.pendingRequests.delete(requestId);
            pending.resolve(response);
        }
    }

    public rejectRequest(requestId: string, error: Error): void {
        const pending = this.pendingRequests.get(requestId);
        if (pending) {
            clearTimeout(pending.timeout);
            if (pending.eventTimeout) clearTimeout(pending.eventTimeout);
            if (pending.absoluteTimeout) clearTimeout(pending.absoluteTimeout);
            this.pendingRequests.delete(requestId);
            pending.reject(error);
        }
    }

    public rejectAll(error: Error): void {
        for (const [, pending] of this.pendingRequests) {
            clearTimeout(pending.timeout);
            if (pending.eventTimeout) clearTimeout(pending.eventTimeout);
            if (pending.absoluteTimeout) clearTimeout(pending.absoluteTimeout);
            pending.reject(error);
        }
        this.pendingRequests.clear();
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
        });
    }

    public createCoalesceKey(action: string, payload: Record<string, unknown>): string {
        // Only coalesce read-only operations
        const readOnlyActions = ['list', 'get_', 'exists', 'search', 'find'];
        if (!readOnlyActions.some(a => action.startsWith(a))) return '';

        // Create a stable hash of the payload
        const stablePayload = JSON.stringify(payload, Object.keys(payload).sort());
        return `${action}:${createHash('md5').update(stablePayload).digest('hex')}`;
    }
}
