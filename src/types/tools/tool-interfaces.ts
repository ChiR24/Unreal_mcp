import type { AutomationBridgeStatus } from '../../automation/index.js';
import type { DebugService } from '../../debug/index.js';
import type { AutomationErrorDetail } from '../automation/automation-responses.js';

export interface AutomationRequestBridge {
    isConnected(): boolean;
    sendAutomationRequest(action: string, payload: Record<string, unknown>, options?: { timeoutMs?: number; waitForEvent?: boolean; waitForEventTimeoutMs?: number }): Promise<unknown>;
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
    debugService?: DebugService;
    bridge?: unknown;
    [key: string]: unknown;
}
