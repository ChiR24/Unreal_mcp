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
