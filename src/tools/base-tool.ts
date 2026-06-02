import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { IBaseTool } from '../types/tool-interfaces.js';
import { isRecord } from '../utils/type-guards.js';

type FailureResponse = { error?: unknown; message?: string } | undefined;

function formatFailureMessage(action: string, toolName: string, response: FailureResponse): string {
    if (typeof response?.error === 'string') {
        return response.error;
    }

    if (isRecord(response?.error) && typeof response.error.message === 'string') {
        const code = typeof response.error.code === 'string' || typeof response.error.code === 'number'
            ? response.error.code
            : 'UNKNOWN';
        return `${response.error.message} (Code: ${code})`;
    }

    return response?.message ?? `Failed to execute ${action} in ${toolName}`;
}

export abstract class BaseTool implements IBaseTool {
    constructor(protected bridge: UnrealBridge) { }

    getAutomationBridge(): AutomationBridge {
        return this.bridge.getAutomationBridge();
    }

    protected async sendRequest<T = unknown>(action: string, params: Record<string, unknown>, toolName: string = 'unknown_tool', options?: { timeoutMs?: number }): Promise<T> {
        const automation = this.getAutomationBridge();

        if (!automation.isConnected()) {
            throw new Error(`Automation bridge not connected for ${toolName}`);
        }

        const response = await automation.sendAutomationRequest(toolName, {
            action,
            ...params
        }, options);

        if (!response || response.success === false) {
            throw new Error(formatFailureMessage(action, toolName, response));
        }

        return (response.result ?? response) as T;
    }

    protected async sendAutomationRequest<T = unknown>(action: string, params: Record<string, unknown> = {}, options?: { timeoutMs?: number; waitForEvent?: boolean; waitForEventTimeoutMs?: number }): Promise<T> {
        const automation = this.getAutomationBridge();
        if (!automation.isConnected()) {
            throw new Error('Automation bridge not connected');
        }
        return automation.sendAutomationRequest(action, params, options);
    }
}
