import { ITools } from '../../../../types/tools/tool-interfaces.js';
import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../dispatch/common-handlers.js';
import { TOOL_ACTIONS } from '../../../../utils/commands/action-constants.js';
export type { ArgConfig } from './argument-normalization.js';
export { normalizeArgs } from './argument-normalization.js';

// Type-safe extraction helpers for handler use

/**
 * Extract a string from normalized args, asserting it exists.
 */
export function extractString(params: Record<string, unknown>, key: string): string {
  const val = params[key];
  if (typeof val !== 'string') {
    throw new Error(`Expected string for '${key}', got ${typeof val}`);
  }
  return val;
}

/**
 * Extract an optional string from normalized args.
 */
export function extractOptionalString(params: Record<string, unknown>, key: string): string | undefined {
  const val = params[key];
  if (val === undefined || val === null) return undefined;
  return typeof val === 'string' ? val : String(val);
}

/**
 * Extract an optional number from normalized args.
 */
export function extractOptionalNumber(params: Record<string, unknown>, key: string): number | undefined {
  const val = params[key];
  if (val === undefined || val === null) return undefined;
  return typeof val === 'number' ? val : undefined;
}

/**
 * Extract an optional boolean from normalized args.
 */
export function extractOptionalBoolean(params: Record<string, unknown>, key: string): boolean | undefined {
  const val = params[key];
  if (val === undefined || val === null) return undefined;
  return typeof val === 'boolean' ? val : undefined;
}

/**
 * Extract an optional array from normalized args.
 * Optional validator function can check each element.
 */
export function extractOptionalArray<T>(params: Record<string, unknown>, key: string, validator?: (item: unknown, index: number) => boolean): T[] | undefined {
  const val = params[key];
  if (val === undefined || val === null) return undefined;
  if (!Array.isArray(val)) {
    // If it's not an array but not null/undefined, that's a type error
    // We swallow this and return undefined (as if the optional arg wasn't provided)
    // rather than throwing, to allow graceful fallback to default behavior.
    return undefined;
  }

  if (validator) {
    val.forEach((item, index) => {
      if (!validator(item, index)) {
        throw new Error(`Invalid item in array '${key}' at index ${index}`);
      }
    });
  }

  return val as T[];
}

/**
 * Extract an optional object from normalized args.
 */
export function extractOptionalObject(params: Record<string, unknown>, key: string): Record<string, unknown> | undefined {
  const val = params[key];
  if (val === undefined || val === null) return undefined;
  if (typeof val === 'object' && !Array.isArray(val)) return val as Record<string, unknown>;
  return undefined;
}

/**
 * Extract an optional vector-like value ({ r, g, b, a } / { x, y, z } object, or a 3-or-4 element numeric
 * array) from normalized args, normalizing the array form to the object the bridge expects.
 *
 * Absent (undefined/null) yields undefined so the caller can apply its own optional default. A value that IS
 * present but cannot be read as a vector THROWS rather than resolving to undefined: the alternative is a
 * caller-side `?? someDefault`, which silently substitutes a different colour for the one that was asked for
 * and still reports success.
 */
export function extractOptionalVector(params: Record<string, unknown>, key: string): Record<string, unknown> | undefined {
  const val = params[key];
  if (val === undefined || val === null) return undefined;

  if (Array.isArray(val)) {
    const nums = val.filter((n): n is number => typeof n === 'number' && Number.isFinite(n));
    if (nums.length !== val.length || (val.length !== 3 && val.length !== 4)) {
      throw new Error(
        `Expected 3 or 4 finite numbers for '${key}', got ${JSON.stringify(val)}`,
      );
    }
    return { r: nums[0], g: nums[1], b: nums[2], a: nums.length === 4 ? nums[3] : 1 };
  }

  if (typeof val === 'object') return val as Record<string, unknown>;

  throw new Error(`Expected an object or a numeric array for '${key}', got ${typeof val}`);
}

/** Response from actor findByName */
interface FindByNameResult {
  success?: boolean;
  result?: { actors?: ActorResult[] };
  actors?: ActorResult[];
}

interface ActorResult {
  path?: string;
  objectPath?: string;
  levelPath?: string;
  name?: string;
}

/**
 * Helper to resolve an object path.
 * Can use a direct path, an actor name, or try to find an actor by name via the tool.
 */
export async function resolveObjectPath(
  args: HandlerArgs,
  tools: ITools,
  config?: {
    pathKeys?: string[];     // defaults to ['objectPath', 'path']
    actorKeys?: string[];    // defaults to ['actorName', 'name']
    fallbackToName?: boolean; // if true, returns the name itself if resolution fails (default true)
  }
): Promise<string | undefined> {
  const pathKeys = config?.pathKeys || ['objectPath', 'path'];
  const actorKeys = config?.actorKeys || ['actorName', 'name'];
  const fallback = config?.fallbackToName !== false;

  // 1. Try direct path keys
  for (const key of pathKeys) {
    const val = args[key];
    if (typeof val === 'string' && val.trim().length > 0) {
      return val.trim().replace(/\/+$/, '');
    }
  }

  // 2. Try actor keys - direct pass-through first
  let potentialName: string | undefined;
  for (const key of actorKeys) {
    const val = args[key];
    if (typeof val === 'string' && val.trim().length > 0) {
      potentialName = val.trim();
      break;
    }
  }

  if (potentialName) {
    // 3. Try smart resolution via automation bridge
    try {
      const res = await executeAutomationRequest(tools, TOOL_ACTIONS.CONTROL_ACTOR, { action: 'find_by_name', name: potentialName }) as FindByNameResult;
      const container = res && (res.result || res);
      const actors = container && Array.isArray(container.actors) ? container.actors : [];
      if (actors.length > 0) {
        const first = actors[0];
        const resolvedPath = first.path || first.objectPath || first.levelPath;
        if (typeof resolvedPath === 'string' && resolvedPath.trim().length > 0) {
          return resolvedPath.trim();
        }
      }
    } catch (error) {
      if (!(error instanceof Error)) {
        throw error;
      }
    }
    // Fallback to the name itself
    if (fallback) return potentialName;
  }

  return undefined;
}
