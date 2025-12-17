import { PendingRequest, AutomationBridgeResponseMessage } from './types.js';
import { randomUUID, createHash } from 'node:crypto';

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
                eventTimeoutMs: timeoutMs
            });
        });

        return { requestId, promise };
    }

    public getPendingRequest(requestId: string): PendingRequest | undefined {
        return this.pendingRequests.get(requestId);
    }

    public resolveRequest(requestId: string, response: AutomationBridgeResponseMessage): void {
        const pending = this.pendingRequests.get(requestId);
        if (pending) {
            clearTimeout(pending.timeout);
            if (pending.eventTimeout) clearTimeout(pending.eventTimeout);
            this.pendingRequests.delete(requestId);
            pending.resolve(response);
        }
    }

    public rejectRequest(requestId: string, error: Error): void {
        const pending = this.pendingRequests.get(requestId);
        if (pending) {
            clearTimeout(pending.timeout);
            if (pending.eventTimeout) clearTimeout(pending.eventTimeout);
            this.pendingRequests.delete(requestId);
            pending.reject(error);
        }
    }

    public rejectAll(error: Error): void {
        for (const [, pending] of this.pendingRequests) {
            clearTimeout(pending.timeout);
            if (pending.eventTimeout) clearTimeout(pending.eventTimeout);
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
