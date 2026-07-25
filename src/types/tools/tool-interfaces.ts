import type { AutomationBridgeStatus } from '../../automation/index.js';
import type { BridgeAuthority } from '../../automation/message-schema.js';
import type { AutomationErrorDetail } from '../automation/automation-responses.js';

export interface AutomationRequestBridge {
    isConnected(): boolean;
    sendAutomationRequest(action: string, payload: Record<string, unknown>, options?: { timeoutMs?: number; waitForEvent?: boolean; waitForEventTimeoutMs?: number; mcpRequestId?: string; correlationId?: string; consent?: { capability: string; acknowledge: 'explicit' | 'elevated' } }): Promise<unknown>;
    getAuthority?(): BridgeAuthority | undefined;
    isCapabilityTokenConfigured?(): boolean;
}

export interface AutomationStatusBridge {
    getStatus(): AutomationBridgeStatus;
}

export interface IBaseTool {
    getAutomationBridge(): AutomationRequestBridge;
}

export interface StandardActionResponse<T = unknown> {
    success: boolean;
    message?: string;
    data?: T;
    warnings?: string[];
    error?: string | AutomationErrorDetail | null;
    [key: string]: unknown;
}

export interface IAssetResources {
    list(directory?: string, recursive?: boolean, limit?: number): Promise<Record<string, unknown>>;
}

export interface ITools {
    systemTools: {
        executeConsoleCommand: (command: string) => Promise<StandardActionResponse>;
        getProjectSettings: (section?: string) => Promise<Record<string, unknown>>;
    };
    elicit?: unknown;
    supportsElicitation?: () => boolean;
    elicitationTimeoutMs?: number;
    assetResources: IAssetResources;
    actorResources?: unknown;
    levelResources?: unknown;
    automationBridge?: AutomationRequestBridge;
    bridge?: unknown;
    [key: string]: unknown;
}
