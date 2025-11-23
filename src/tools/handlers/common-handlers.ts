import { ITools } from '../../types/tool-interfaces.js';

export function ensureArgsPresent(args: any) {
  if (args === null || args === undefined) {
    throw new Error('Invalid arguments: null or undefined');
  }
}

export function requireAction(args: any): string {
  ensureArgsPresent(args);
  const action = args.action;
  if (typeof action !== 'string' || action.trim() === '') {
    throw new Error('Missing required parameter: action');
  }
  return action;
}

export function requireNonEmptyString(value: any, field: string, message?: string): string {
  if (typeof value !== 'string' || value.trim() === '') {
    throw new Error(message ?? `Invalid ${field}: must be a non-empty string`);
  }
  return value;
}

export async function executeAutomationRequest(
  tools: ITools,
  toolName: string,
  args: any,
  errorMessage: string = 'Automation bridge not available'
) {
  const automationBridge = tools.automationBridge;
  // If the bridge is missing or not a function, we can't proceed with automation requests
  if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function') {
    throw new Error(errorMessage);
  }

  if (!automationBridge.isConnected()) {
    throw new Error(`Automation bridge is not connected to Unreal Engine. Please check if the editor is running and the plugin is enabled. Action: ${toolName}`);
  }

  return await automationBridge.sendAutomationRequest(toolName, args);
}
