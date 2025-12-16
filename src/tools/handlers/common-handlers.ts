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
  errorMessage: string = 'Automation bridge not available',
  options: { timeoutMs?: number } = {}
) {
  const automationBridge = tools.automationBridge;
  // If the bridge is missing or not a function, we can't proceed with automation requests
  if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function') {
    throw new Error(errorMessage);
  }

  if (!automationBridge.isConnected()) {
    throw new Error(`Automation bridge is not connected to Unreal Engine. Please check if the editor is running and the plugin is enabled. Action: ${toolName}`);
  }

  return await automationBridge.sendAutomationRequest(toolName, args, options);
}

/**
 * Normalize location to [x, y, z] array format
 * Accepts both {x,y,z} object and [x,y,z] array formats
 */
export function normalizeLocation(location: any): [number, number, number] | undefined {
  if (!location) return undefined;

  // Already array format
  if (Array.isArray(location) && location.length >= 3) {
    return [Number(location[0]) || 0, Number(location[1]) || 0, Number(location[2]) || 0];
  }

  // Object format {x, y, z}
  if (typeof location === 'object' && ('x' in location || 'y' in location || 'z' in location)) {
    return [Number(location.x) || 0, Number(location.y) || 0, Number(location.z) || 0];
  }

  return undefined;
}

/**
 * Normalize rotation to {pitch, yaw, roll} object format
 * Accepts both {pitch,yaw,roll} object and [pitch,yaw,roll] array formats
 */
export function normalizeRotation(rotation: any): { pitch: number; yaw: number; roll: number } | undefined {
  if (!rotation) return undefined;

  // Array format [pitch, yaw, roll]
  if (Array.isArray(rotation) && rotation.length >= 3) {
    return { pitch: Number(rotation[0]) || 0, yaw: Number(rotation[1]) || 0, roll: Number(rotation[2]) || 0 };
  }

  // Already object format
  if (typeof rotation === 'object') {
    return {
      pitch: Number(rotation.pitch) || 0,
      yaw: Number(rotation.yaw) || 0,
      roll: Number(rotation.roll) || 0
    };
  }

  return undefined;
}
