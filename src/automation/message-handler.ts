import { AutomationLogger } from './log-redaction.js';
import type { RequestTracker } from './request-tracker.js';
import type {
    AutomationBridgeAutomationEvent,
    AutomationBridgeMessage,
    AutomationBridgeResponseMessage,
    AutomationProgressUpdate,
    ProgressUpdateMessage
} from './types.js';

/** Event message structure */
interface EventMessage extends AutomationBridgeMessage {
    requestId?: string;
    event?: string;
    payload?: unknown;
    result?: unknown;
    message?: string;
}

export class MessageHandler {
    private log = new AutomationLogger('MessageHandler');

    constructor(
        private requestTracker: RequestTracker,
        private readonly emitAutomationEvent?: (event: AutomationBridgeAutomationEvent) => void,
        private readonly emitRequestProgress?: (
            requestId: string,
            update: AutomationProgressUpdate
        ) => void
    ) { }

    public handleMessage(message: AutomationBridgeMessage): void {
        switch (message.type) {
            case 'automation_response':
                this.handleAutomationResponse(message as AutomationBridgeResponseMessage);
                break;
            case 'automation_event':
                this.handleAutomationEvent(message);
                break;
            case 'progress_update':
                this.handleProgressUpdate(message as ProgressUpdateMessage);
                break;
            default:
                this.log.debug('Received automation bridge message with no handler', message);
                break;
        }
    }

    private handleAutomationResponse(response: AutomationBridgeResponseMessage): void {
        const requestId = response.requestId;
        if (!requestId) {
            this.log.warn('Received automation_response without requestId');
            return;
        }

        const pending = this.requestTracker.getPendingRequest(requestId);
        if (!pending) {
            this.log.debug(`No pending automation request found for requestId=${requestId}`);
            return;
        }

        this.requestTracker.resolveRequest(requestId, response);
    }

    private handleAutomationEvent(message: AutomationBridgeMessage): void {
        const evt = message as EventMessage;
        const normalized = this.normalizeAutomationEvent(evt);
        if (!normalized) {
            this.log.warn('Dropped automation_event without a valid event name');
            return;
        }

        this.emitAutomationEvent?.(normalized);
        this.log.debug('Received automation_event:', normalized);
    }

    private normalizeAutomationEvent(evt: EventMessage): AutomationBridgeAutomationEvent | null {
        if (typeof evt.event !== 'string' || evt.event.trim().length === 0) {
            return null;
        }

        const normalized: AutomationBridgeAutomationEvent = {
            type: 'automation_event',
            event: evt.event.trim()
        };
        if (typeof evt.requestId === 'string' && evt.requestId.length > 0) {
            normalized.requestId = evt.requestId;
        }
        if (evt.payload !== undefined) {
            normalized.payload = evt.payload;
        }
        if (evt.result !== undefined) {
            normalized.result = evt.result;
        }
        if (typeof evt.message === 'string' && evt.message.length > 0) {
            normalized.message = evt.message;
        }
        return normalized;
    }

    /**
     * Handle progress update messages from UE during long-running operations.
     * Extends the request timeout to keep the connection alive.
     */
    private handleProgressUpdate(message: ProgressUpdateMessage): void {
        const { requestId, percent, message: statusMsg, stillWorking } = message;

        if (!requestId) {
            this.log.debug('Received progress_update without requestId');
            return;
        }

        const pending = this.requestTracker.getPendingRequest(requestId);
        if (!pending) {
            this.log.debug(`No pending request for progress_update requestId=${requestId}`);
            return;
        }

        const progressStr = percent !== undefined ? ` (${percent.toFixed(1)}%)` : '';
        const msgStr = statusMsg ? `: ${statusMsg}` : '';
        this.log.debug(`Progress update for ${pending.action}${progressStr}${msgStr}`);

        // If stillWorking is explicitly false, operation may be completing soon
        if (stillWorking === false) {
            this.log.debug(`Progress update indicates operation completing for ${pending.action}`);
        }

        // Forward toward the MCP client BEFORE the timeout bookkeeping: a
        // rejected extension must not also cost the client the progress frame
        // that Unreal already produced.
        if (percent !== undefined && Number.isFinite(percent)) {
            this.emitRequestProgress?.(requestId, {
                progress: percent,
                total: 100,
                ...(statusMsg ? { message: statusMsg } : {})
            });
        }

        // Extend the timeout - this also handles deadlock detection
        const extended = this.requestTracker.extendTimeout(requestId, percent);
        if (!extended) {
            this.log.warn(`Timeout extension rejected for ${pending.action} - possible deadlock detected`);
        }
    }

}
