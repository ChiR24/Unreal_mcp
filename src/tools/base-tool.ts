import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { IBaseTool } from '../types/tool-interfaces.js';

export abstract class BaseTool implements IBaseTool {
    constructor(protected bridge: UnrealBridge) { }

    getAutomationBridge(): AutomationBridge {
        return this.bridge.getAutomationBridge();
    }

    protected async sendRequest(action: string, params: Record<string, unknown>, toolName: string = 'unknown_tool'): Promise<any> {
        const automation = this.getAutomationBridge();

        // Basic validation
        if (!automation.isConnected()) {
            throw new Error(`Automation bridge not connected for ${toolName}`);
        }

        const response = await automation.sendAutomationRequest(toolName, {
            action,
            ...params
        });

        if (!response || response.success === false) {
            throw new Error(response?.error || response?.message || `Failed to execute ${action} in ${toolName}`);
        }

        return response.result ?? response;
    }

    protected async sendAutomationRequest(action: string, params: Record<string, unknown> = {}, options?: { timeoutMs?: number; waitForEvent?: boolean; waitForEventTimeoutMs?: number }): Promise<any> {
        const automation = this.getAutomationBridge();
        if (!automation.isConnected()) {
            throw new Error('Automation bridge not connected');
        }
        return automation.sendAutomationRequest(action, params, options);
    }
}
