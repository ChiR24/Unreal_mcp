import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { IBaseTool } from '../types/tool-interfaces.js';

/**
 * Validates that the automation bridge is available and throws a descriptive error if not.
 * 
 * Use this helper in legacy tool classes that don't extend BaseTool to ensure
 * consistent error messaging when the bridge is unavailable.
 * 
 * @param bridge - The AutomationBridge instance (may be undefined)
 * @param operation - Description of the operation being attempted (for error message)
 * @returns The validated AutomationBridge instance
 * @throws Error if bridge is undefined or null
 * 
 * @example
 * ```typescript
 * const bridge = requireBridge(this.automationBridge, 'Audio operations');
 * await bridge.sendAutomationRequest(...);
 * ```
 */
export function requireBridge(bridge: AutomationBridge | undefined | null, operation: string): AutomationBridge {
    if (!bridge) {
        throw new Error(`Automation Bridge not available. ${operation} require plugin support.`);
    }
    return bridge;
}

export abstract class BaseTool implements IBaseTool {
    constructor(protected bridge: UnrealBridge) { }

    getAutomationBridge(): AutomationBridge {
        return this.bridge.getAutomationBridge();
    }

    protected async sendRequest<T = unknown>(action: string, params: Record<string, unknown>, toolName: string = 'unknown_tool', options?: { timeoutMs?: number }): Promise<T> {
        const automation = this.getAutomationBridge();

        // Basic validation
        if (!automation.isConnected()) {
            throw new Error(`Automation bridge not connected for ${toolName}`);
        }

        const response = await automation.sendAutomationRequest(toolName, {
            action,
            ...params
        }, options);

        if (!response || response.success === false) {
            let errorMessage = `Failed to execute ${action} in ${toolName}`;
            if (response?.error) {
                if (typeof response.error === 'string') {
                    errorMessage = response.error;
                } else if (typeof response.error === 'object') {
                    const errObj = response.error as Record<string, unknown>;
                    if (errObj.message) {
                        errorMessage = `${errObj.message} (Code: ${errObj.code || 'UNKNOWN'})`;
                    }
                }
            } else if (response?.message) {
                errorMessage = response.message;
            }
            throw new Error(errorMessage);
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
