import type { AutomationBridgeStatus } from '../../automation/index.js';
import type { BridgeAuthority } from '../../automation/message-schema.js';
import type { ExpectedRevisions } from '../../tools/catalog/capabilities/semantic/execution-options.js';
import type { BridgeConnectionRefusalCode } from '../../automation/types.js';

export interface AutomationRequestBridge {
    isConnected(): boolean;
    sendAutomationRequest(action: string, payload: Record<string, unknown>, options?: { timeoutMs?: number; mcpRequestId?: string; correlationId?: string; consent?: { capability: string; acknowledge: 'explicit' | 'elevated'; nonce?: string }; expectedRevisions?: ExpectedRevisions }): Promise<unknown>;
    getAuthority?(): BridgeAuthority | undefined;
    /** Closed-set handshake refusal from the most recent connection attempt, if any. */
    getConnectionRefusalCode?(): BridgeConnectionRefusalCode | undefined;
    /** Set while connected to a plugin from another release, naming the install that fixes it. */
    getVersionMismatch?(): string | undefined;
    /** Resolved bridge URL (`ws://host:port`), for diagnostics. */
    getClientUrl?(): string;
}

export interface AutomationStatusBridge {
    getStatus(): AutomationBridgeStatus;
}

export interface ITools {
    automationBridge?: AutomationRequestBridge;
}
