import { AutomationLogger } from './log-redaction.js';
import type { RequestTracker } from './request-tracker.js';
import type {
    AutomationBridgeAutomationEvent,
    AutomationBridgeMessage,
    AutomationBridgeResponseMessage,
    ProgressUpdateMessage
} from './types.js';

function FStringSafe(val: unknown): string {
    try {
        if (val === undefined || val === null) return '';
        if (typeof val === 'string') return val;
        return JSON.stringify(val);
    } catch {
        try { return String(val); } catch { return ''; }
    }
}

/** Response result with optional saved flag */
interface ResponseResult {
    saved?: boolean;
    action?: string;
    success?: boolean;
    message?: string;
    error?: string;
    [key: string]: unknown;
}

/** Event message structure */
interface EventMessage extends AutomationBridgeMessage {
    requestId?: string;
    event?: string;
    payload?: unknown;
    result?: ResponseResult;
    message?: string;
}

/** Response with optional action field */
interface ResponseWithAction extends AutomationBridgeResponseMessage {
    action?: string;
}

const CONSOLIDATED_TOOL_ACTIONS = new Set([
    'animation_physics',
    'create_effect',
    'build_environment',
    'manage_sequence',
    'system_control',
    'manage_ui',
    'inspect'
]);

export class MessageHandler {
    private log = new AutomationLogger('MessageHandler');

    constructor(
        private requestTracker: RequestTracker,
        private readonly emitAutomationEvent?: (event: AutomationBridgeAutomationEvent) => void
    ) { }

    public handleMessage(message: AutomationBridgeMessage): void {
        switch (message.type) {
            case 'automation_response':
                this.handleAutomationResponse(message as AutomationBridgeResponseMessage);
                break;
            case 'bridge_ping':
                // Handled by connection manager or ignored if client
                break;
            case 'bridge_pong':
                // Handled by connection manager
                break;
            case 'bridge_goodbye':
                this.log.info('Automation bridge client initiated shutdown.', message);
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

        // Enforce action match logic
        const enforcedResponse = this.enforceActionMatch(response, pending.action);

        if (pending.waitForEvent) {
            if (!pending.initialResponse) {
                // Store initial response and wait for event
                pending.initialResponse = enforcedResponse;

                // If the initial response indicates failure, resolve immediately
                if (enforcedResponse.success === false) {
                    this.requestTracker.resolveRequest(requestId, enforcedResponse);
                    return;
                }

                // If the response indicates it's already saved/done, resolve immediately
                const result = enforcedResponse.result as ResponseResult | undefined;
                if (result && result.saved === true) {
                    this.requestTracker.resolveRequest(requestId, enforcedResponse);
                    return;
                }

                // Set event timeout
                const eventTimeoutMs = pending.eventTimeoutMs || 30000; // Default 30s for event
                pending.eventTimeout = setTimeout(() => {
                    this.requestTracker.rejectRequest(requestId, new Error(`Timed out waiting for completion event for ${pending.action}`));
                }, eventTimeoutMs);

                this.log.debug(`Received initial response for ${pending.action}, waiting for completion event...`);
            } else {
                // Second response, treat as completion
                this.requestTracker.resolveRequest(requestId, enforcedResponse);
            }
        } else {
            this.requestTracker.resolveRequest(requestId, enforcedResponse);
        }
    }

    private handleAutomationEvent(message: AutomationBridgeMessage): void {
        const evt = message as EventMessage;
        const reqId = typeof evt.requestId === 'string' ? evt.requestId : undefined;

        if (reqId) {
            const pending = this.requestTracker.getPendingRequest(reqId);
            if (pending) {
                if (!pending.waitForEvent) {
                    this.log.debug(`Received automation_event for ${reqId} while waiting for automation_response; ignoring event=${String(evt.event || '')}`);
                    return;
                }

                try {
                    const baseSuccess = (pending.initialResponse && typeof pending.initialResponse.success === 'boolean') ? pending.initialResponse.success : undefined;
                    const evtSuccess = (evt.result && typeof evt.result.success === 'boolean') ? !!evt.result.success : undefined;

                    const synthetic: AutomationBridgeResponseMessage = {
                        type: 'automation_response',
                        requestId: reqId,
                        success: evtSuccess !== undefined ? evtSuccess : baseSuccess,
                        message: typeof evt.result?.message === 'string' ? evt.result.message : (typeof evt.message === 'string' ? evt.message : FStringSafe(evt.event)),
                        error: typeof evt.result?.error === 'string' ? evt.result.error : undefined,
                        result: evt.result ?? evt.payload ?? undefined
                    };

                    this.log.info(`automation_event resolved pending request ${reqId} (event=${String(evt.event || '')})`);
                    this.requestTracker.resolveRequest(reqId, synthetic);
                } catch (error) {
                    const message = error instanceof Error ? error.message : String(error);
                    this.log.warn(`Failed to resolve pending automation request from automation_event ${reqId}: ${message}`);
                }
                return;
            }
        }

        const normalized = this.normalizeAutomationEvent(evt);
        if (!normalized) {
            this.log.warn('Dropped automation_event without a valid event name');
            return;
        }

        this.emitAutomationEvent?.(normalized);
        this.log.debug('Received automation_event (no pending request):', normalized);
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

        // Log the progress update
        const progressStr = percent !== undefined ? ` (${percent.toFixed(1)}%)` : '';
        const msgStr = statusMsg ? `: ${statusMsg}` : '';
        this.log.debug(`Progress update for ${pending.action}${progressStr}${msgStr}`);

        // If stillWorking is explicitly false, operation may be completing soon
        if (stillWorking === false) {
            this.log.debug(`Progress update indicates operation completing for ${pending.action}`);
        }

        // Extend the timeout - this also handles deadlock detection
        const extended = this.requestTracker.extendTimeout(requestId, percent, statusMsg);
        if (!extended) {
            this.log.warn(`Timeout extension rejected for ${pending.action} - possible deadlock detected`);
        }
    }

    private enforceActionMatch(response: AutomationBridgeResponseMessage, expectedAction: string): AutomationBridgeResponseMessage {
        try {
            const expected = (expectedAction || '').toString().toLowerCase();
            const echoed: string | undefined = (() => {
                const r = response as ResponseWithAction;
                const resultObj = response.result as ResponseResult | undefined;
                const candidate = (typeof r.action === 'string' && r.action) || (typeof resultObj?.action === 'string' && resultObj.action);
                return candidate || undefined;
            })();

            if (expected && echoed && typeof echoed === 'string') {
                const got = echoed.toLowerCase();

                if (CONSOLIDATED_TOOL_ACTIONS.has(expected) && got !== expected) {
                    // For consolidated tools, only accept sub-actions that look
                    // like snake_case identifiers. This rejects arbitrary response
                    // action values (e.g. "manage_sequence/../private",
                    // "delete everything", "manage_sequence\delete") that would
                    // otherwise pass the prefix check. The format check is
                    // intentionally permissive: it accepts any lowercase
                    // snake_case string, so legitimate sub-actions added in the
                    // future still pass without updating an allow-list.
                    if (!/^[a-z][a-z0-9_]*$/u.test(got)) {
                        const mutated: ResponseWithAction = { ...response };
                        mutated.success = false;
                        if (!mutated.error) mutated.error = 'ACTION_NOT_A_SUBACTION';
                        mutated.message = `Response action '${got}' is not a valid sub-action of consolidated tool '${expected}'`;
                        return mutated as AutomationBridgeResponseMessage;
                    }
                    return response;
                }

                const startsEitherWay = got.startsWith(expected) || expected.startsWith(got);

                if (!startsEitherWay) {
                    const mutated: ResponseWithAction = { ...response };
                    mutated.success = false;
                    if (!mutated.error) mutated.error = 'ACTION_PREFIX_MISMATCH';
                    const msgBase = typeof mutated.message === 'string' ? `${mutated.message} ` : '';
                    mutated.message = `${msgBase}Response action mismatch (expected~='${expected}', got='${echoed}')`;
                    return mutated as AutomationBridgeResponseMessage;
                }
            }
        } catch (e) {
            this.log.debug('enforceActionMatch check skipped', e instanceof Error ? e.message : String(e));
        }
        return response;
    }
}
