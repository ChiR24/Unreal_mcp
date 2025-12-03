import { PendingRequest, AutomationBridgeResponseMessage } from './types.js';
import { randomUUID, createHash } from 'node:crypto';

const WAIT_FOR_EVENT_ACTIONS = new Set<string>([
    'sequence_create',
    'sequence_open',
    'sequence_add_camera',
    'sequence_add_actor',
    'sequence_add_actors',
    'sequence_remove_actors',
    'sequence_set_properties',
    'sequence_set_playback_speed',
    'sequence_get_properties',
    'sequence_get_bindings',
    'sequence_add_spawnable_from_class',
    'add_sequencer_keyframe',
    'manage_sequencer_track',
    'duplicate_asset',
    'rename_asset',
    'delete_assets',
    'move_asset',
    'import_asset',
    'save_asset',
    'set_tags',
    'create_thumbnail',
    'generate_report',
    'validate',
    'set_object_property'
]);

export class RequestTracker {
    private pendingRequests = new Map<string, PendingRequest>();
    private coalescedRequests = new Map<string, Promise<AutomationBridgeResponseMessage>>();


    constructor(
        private maxPendingRequests: number
    ) { }

    public createRequest(
        action: string,
        payload: Record<string, unknown>,
        timeoutMs: number
    ): { requestId: string; promise: Promise<AutomationBridgeResponseMessage> } {
        if (this.pendingRequests.size >= this.maxPendingRequests) {
            throw new Error(`Max pending requests limit reached (${this.maxPendingRequests})`);
        }

        const requestId = randomUUID();
        const waitForEvent = WAIT_FOR_EVENT_ACTIONS.has(action);

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
                waitForEvent
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
