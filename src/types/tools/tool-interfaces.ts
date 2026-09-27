import type { AutomationBridgeStatus } from '../../automation/index.js';
import type { BridgeAuthority } from '../../automation/message-schema.js';
import type { ExpectedRevisions } from '../../tools/catalog/capabilities/semantic/execution-options.js';

export interface AutomationRequestBridge {
    isConnected(): boolean;
    sendAutomationRequest(action: string, payload: Record<string, unknown>, options?: { timeoutMs?: number; mcpRequestId?: string; correlationId?: string; consent?: { capability: string; acknowledge: 'explicit' | 'elevated' }; expectedRevisions?: ExpectedRevisions }): Promise<unknown>;
    getAuthority?(): BridgeAuthority | undefined;
    /** Resolved bridge URL (`ws://host:port`), for diagnostics. */
    getClientUrl?(): string;
}

export interface AutomationStatusBridge {
    getStatus(): AutomationBridgeStatus;
}

export interface ITools {
    automationBridge?: AutomationRequestBridge;
}
