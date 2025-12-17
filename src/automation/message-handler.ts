import { Logger } from '../utils/logger.js';
import { AutomationBridgeMessage, AutomationBridgeResponseMessage } from './types.js';
import { RequestTracker } from './request-tracker.js';

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

export class MessageHandler {
    private log = new Logger('MessageHandler');

    constructor(
        private requestTracker: RequestTracker
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
                } catch (e) {
                    this.log.warn(`Failed to resolve pending automation request from automation_event ${reqId}: ${String(e)}`);
                }
                return;
            }
        }

        this.log.debug('Received automation_event (no pending request):', message);
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

                const consolidatedToolActions = new Set([
                    'animation_physics',
                    'create_effect',
                    'build_environment',
                    'system_control',
                    'manage_ui'
                ]);

                if (consolidatedToolActions.has(expected) && got !== expected) {
                    return response;
                }

                const startsEitherWay = got.startsWith(expected) || expected.startsWith(got);

                if (!startsEitherWay) {
                    const mutated: ResponseWithAction = { ...response };
                    mutated.success = false;
                    if (!mutated.error) mutated.error = 'ACTION_PREFIX_MISMATCH';
                    const msgBase = typeof mutated.message === 'string' ? mutated.message + ' ' : '';
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
