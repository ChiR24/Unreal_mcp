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
 * Security validation: Check for path traversal attempts and blocked patterns.
 * Returns an error message if validation fails, undefined if validation passes.
 */
export function validateSecurityPatterns(args: Record<string, unknown>): string | undefined {
  // Path traversal patterns to block
  const traversalPatterns = [
    '../',           // Unix parent directory
    '..\\',          // Windows parent directory
    '/etc/',         // Unix system directory
    '\\Windows\\',   // Windows system directory
    '\\Program Files', // Windows program files
  ];
  
  // Check all string arguments for traversal patterns
  for (const [key, value] of Object.entries(args)) {
    if (typeof value === 'string') {
      const lowerValue = value.toLowerCase();
      for (const pattern of traversalPatterns) {
        if (value.includes(pattern) || lowerValue.includes(pattern.toLowerCase())) {
          return `Security violation: '${key}' contains blocked path pattern. Path traversal is not allowed.`;
        }
      }
      
      // Additional check for paths starting with / (could be absolute system paths)
      // Allow /Game/, /Engine/, /Script/, /Temp/ as they are UE paths
      // Also allow exact matches like /Game, /Engine (without trailing slash)
      if (key.toLowerCase().includes('path') && value.startsWith('/')) {
        const allowedPrefixes = ['/Game/', '/Engine/', '/Script/', '/Temp/'];
        const exactAllowed = ['/Game', '/Engine', '/Script', '/Temp'];
        const isAllowed = allowedPrefixes.some(prefix => value.startsWith(prefix)) ||
                          exactAllowed.includes(value);
        if (!isAllowed) {
          return `Security violation: '${key}' uses unauthorized absolute path. Only /Game/, /Engine/, /Script/, and /Temp/ paths are allowed.`;
        }
      }
    }
  }
  
  return undefined;
}

/**
 * Validates arguments for security concerns before sending to the engine.
 * Throws an error if validation fails.
 */
export function validateArgsSecurity(args: HandlerArgs): void {
  ensureArgsPresent(args);
  const argsRecord = args as Record<string, unknown>;
  
  const securityError = validateSecurityPatterns(argsRecord);
  if (securityError) {
    throw new Error(securityError);
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
 */
export async function executeAutomationRequest(
  tools: ITools,
  toolName: string,
  args: HandlerArgs,
  errorMessage: string = 'Automation bridge not available',
  options: { timeoutMs?: number } = {}
): Promise<unknown> {
  // Security validation: Check for path traversal and other security violations
  validateArgsSecurity(args);
  
  const automationBridge = tools.automationBridge;
  // If the bridge is missing or not a function, we can't proceed with automation requests
  if (!automationBridge || typeof automationBridge.sendAutomationRequest !== 'function') {
    throw new Error(errorMessage);
  }

  if (!automationBridge.isConnected()) {
    throw new Error(`Automation bridge is not connected to Unreal Engine. Please check if the editor is running and the plugin is enabled. Action: ${toolName}`);
  }

  // Extract timeoutMs from args if present (for tools that need custom timeouts)
  // This allows tests and handlers to specify longer timeouts for heavy operations
  const argsRecord = args as Record<string, unknown>;
  const timeoutMs = options.timeoutMs ?? (typeof argsRecord.timeoutMs === 'number' ? argsRecord.timeoutMs : undefined);
  
  // Remove timeoutMs from payload to avoid sending it to UE (it's client-side only)
  const cleanedArgs = { ...argsRecord };
  delete cleanedArgs.timeoutMs;

  return await automationBridge.sendAutomationRequest(toolName, cleanedArgs, timeoutMs ? { timeoutMs } : {});
}

/**
 * Normalize location to [x, y, z] array format
 * Accepts both {x,y,z} object and [x,y,z] array formats
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

/** Input type for rotation normalization */
type RotationInput = Rotator | [number, number, number] | number[] | null | undefined;

/**
 * Normalize rotation to {pitch, yaw, roll} object format
 * Accepts both {pitch,yaw,roll} object and [pitch,yaw,roll] array formats
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

/**
 * Validates that only expected parameters are present in args.
 * Throws an error if unknown parameters are found.
 * 
 * @param args - The arguments object to validate
 * @param allowedParams - Array of allowed parameter names (action and subAction are always allowed)
 * @param context - Context string for error messages (e.g., tool name or action)
 */
export function validateExpectedParams(
  args: Record<string, unknown>,
  allowedParams: string[],
  context: string = 'handler'
): void {
  const alwaysAllowed = ['action', 'subAction', 'timeoutMs'];
  const allAllowed = new Set([...alwaysAllowed, ...allowedParams]);
  
  const unknownParams = Object.keys(args).filter(key => !allAllowed.has(key));
  
  if (unknownParams.length > 0) {
    throw new Error(
      `Invalid parameters for ${context}: unknown parameters [${unknownParams.join(', ')}]. ` +
      `Allowed: [${allowedParams.join(', ')}]`
    );
  }
}

/**
 * Validates that required parameters are present and non-empty.
 * Throws an error if any required parameter is missing or empty.
 * 
 * @param args - The arguments object to validate
 * @param requiredParams - Array of required parameter names
 * @param context - Context string for error messages
 */
export function validateRequiredParams(
  args: Record<string, unknown>,
  requiredParams: string[],
  context: string = 'handler'
): void {
  const missingParams = requiredParams.filter(param => {
    const value = args[param];
    return value === undefined || value === null || 
           (typeof value === 'string' && value.trim() === '');
  });
  
  if (missingParams.length > 0) {
    throw new Error(
      `Missing required parameters for ${context}: [${missingParams.join(', ')}]`
    );
  }
}
