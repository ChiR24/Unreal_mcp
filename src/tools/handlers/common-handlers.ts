import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, Vector3, Rotator } from '../../types/handler-types.js';

/**
 * Validates that args is not null/undefined.
 */
export function ensureArgsPresent(args: unknown): asserts args is Record<string, unknown> {
  if (args === null || args === undefined) {
    throw new Error('Invalid arguments: null or undefined');
  }
}

/**
 * Extracts and validates the 'action' field from args.
 */
export function requireAction(args: HandlerArgs): string {
  ensureArgsPresent(args);
  const action = args.action;
  if (typeof action !== 'string' || action.trim() === '') {
    throw new Error('Missing required parameter: action');
  }
  return action;
}

/**
 * Validates that a value is a non-empty string.
 */
export function requireNonEmptyString(value: unknown, field: string, message?: string): string {
  if (typeof value !== 'string' || value.trim() === '') {
    throw new Error(message ?? `Invalid ${field}: must be a non-empty string`);
  }
  return value;
}

/**
 * Execute a request via the automation bridge.
 * 
 * This is the primary entry point for all Unreal Engine automation requests.
 * It handles connection validation and dispatches the request to the C++ plugin.
 * 
 * @param tools - The tools interface containing the automation bridge
 * @param toolName - Name of the C++ handler to invoke (e.g., 'manage_asset', 'control_actor')
 * @param args - Arguments to pass to the C++ handler
 * @param errorMessage - Custom error message if bridge is unavailable
 * @param options - Optional configuration (timeoutMs for long operations)
 * @returns Promise resolving to the C++ handler's response
 * @throws Error if automation bridge is not connected or available
 * 
 * @example
 * ```typescript
 * const result = await executeAutomationRequest(
 *   tools,
 *   'manage_asset',
 *   { action: 'list', path: '/Game/MyFolder' },
 *   'Asset operation failed'
 * );
 * ```
 */
export async function executeAutomationRequest(
  tools: ITools,
  toolName: string,
  args: HandlerArgs,
  errorMessage: string = 'Automation bridge not available',
  options: { timeoutMs?: number } = {}
): Promise<unknown> {
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
 * Normalize location to [x, y, z] array format for C++ handlers.
 * 
 * Accepts multiple input formats and converts to a consistent tuple format
 * that the C++ automation bridge expects.
 * 
 * @param location - Location in {x,y,z} object or [x,y,z] array format
 * @returns Tuple [x, y, z] or undefined if input is falsy
 * 
 * @example
 * ```typescript
 * normalizeLocation({ x: 100, y: 200, z: 50 }) // [100, 200, 50]
 * normalizeLocation([100, 200, 50])            // [100, 200, 50]
 * normalizeLocation(undefined)                 // undefined
 * ```
 */
export function normalizeLocation(location: unknown): [number, number, number] | undefined {
  if (!location) return undefined;

  // Already array format
  if (Array.isArray(location) && location.length >= 3) {
    return [Number(location[0]) || 0, Number(location[1]) || 0, Number(location[2]) || 0];
  }

  // Object format {x, y, z}
  if (typeof location === 'object' && ('x' in location || 'y' in location || 'z' in location)) {
    const loc = location as Vector3;
    return [Number(loc.x) || 0, Number(loc.y) || 0, Number(loc.z) || 0];
  }

  return undefined;
}

/** Input type for rotation normalization - accepts object or array format */
type RotationInput = Rotator | [number, number, number] | number[] | null | undefined;

/**
 * Normalize rotation to {pitch, yaw, roll} object format for C++ handlers.
 * 
 * Converts array format [pitch, yaw, roll] to object format expected by
 * many Unreal Engine APIs.
 * 
 * @param rotation - Rotation in {pitch,yaw,roll} object or [p,y,r] array format
 * @returns Rotator object or undefined if input is falsy
 * 
 * @example
 * ```typescript
 * normalizeRotation([45, 90, 0])                    // { pitch: 45, yaw: 90, roll: 0 }
 * normalizeRotation({ pitch: 45, yaw: 90, roll: 0 }) // { pitch: 45, yaw: 90, roll: 0 }
 * normalizeRotation(null)                            // undefined
 * ```
 */
export function normalizeRotation(rotation: RotationInput): Rotator | undefined {
  if (!rotation) return undefined;

  // Array format [pitch, yaw, roll]
  if (Array.isArray(rotation) && rotation.length >= 3) {
    return { pitch: Number(rotation[0]) || 0, yaw: Number(rotation[1]) || 0, roll: Number(rotation[2]) || 0 };
  }

  // Already object format
  if (typeof rotation === 'object') {
    const rot = rotation as Rotator;
    return {
      pitch: Number(rot.pitch) || 0,
      yaw: Number(rot.yaw) || 0,
      roll: Number(rot.roll) || 0
    };
  }

  return undefined;
}
