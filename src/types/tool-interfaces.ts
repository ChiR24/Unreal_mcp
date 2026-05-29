import { AutomationBridge } from '../automation/index.js';
import type { AutomationErrorDetail } from './automation-responses.js';

export interface IBaseTool {
    getAutomationBridge(): AutomationBridge;
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
    automationBridge?: AutomationBridge;
    bridge?: unknown;
    [key: string]: unknown;
}
