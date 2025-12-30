import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs } from '../../types/handler-types.js';

export interface ArgConfig {
  /** The primary key to store the normalized value in. */
  key: string;
  /** A list of alternative keys (aliases) to look for in the input args. */
  aliases?: string[];
  /** If true, the value must be a non-empty string. Throws an error if missing or empty. */
  required?: boolean;
  /** If provided, uses this default value if no valid input is found. */
  default?: unknown;
  /** If provided, maps the input string using this dictionary (e.g. for friendly class names). */
  map?: Record<string, string>;
  /** Custom validation function. Throws or returns invalid message string if check fails. */
  validator?: (val: unknown) => void | string;
}

/**
 * Normalizes a raw arguments object based on a list of configurations.
 * Handles aliasing, defaults, required checks, and value mapping.
 *
 * @param args The raw arguments object from the tool call.
 * @param configs A list of configuration objects for each expected argument.
 * @returns A new object containing the normalized arguments.
 * @throws Error if a required argument is missing or validation fails.
 */
/**
 * Typed result from normalizeArgs that provides type-safe accessors.
 */
export interface NormalizedArgs {
  /** Get a string value, throwing if not a string */
  getString(key: string): string;
  /** Get a string value or undefined */
  getOptionalString(key: string): string | undefined;
  /** Get a number value, throwing if not a number */
  getNumber(key: string): number;
  /** Get a number value or undefined */
  getOptionalNumber(key: string): number | undefined;
  /** Get a boolean value, throwing if not a boolean */
  getBoolean(key: string): boolean;
  /** Get a boolean value or undefined */
  getOptionalBoolean(key: string): boolean | undefined;
  /** Get raw value (unknown type) */
  get(key: string): unknown;
  /** Get the raw normalized object */
  raw(): Record<string, unknown>;
}

function createNormalizedArgs(data: Record<string, unknown>): NormalizedArgs {
  return {
    getString(key: string): string {
      const val = data[key];
      if (typeof val !== 'string') {
        throw new Error(`Expected string for '${key}', got ${typeof val}`);
      }
      return val;
    },
    getOptionalString(key: string): string | undefined {
      const val = data[key];
      if (val === undefined || val === null) return undefined;
      if (typeof val !== 'string') {
        throw new Error(`Expected string for '${key}', got ${typeof val}`);
      }
      return val;
    },
    getNumber(key: string): number {
      const val = data[key];
      if (typeof val !== 'number') {
        throw new Error(`Expected number for '${key}', got ${typeof val}`);
      }
      return val;
    },
    getOptionalNumber(key: string): number | undefined {
      const val = data[key];
      if (val === undefined || val === null) return undefined;
      if (typeof val !== 'number') {
        throw new Error(`Expected number for '${key}', got ${typeof val}`);
      }
      return val;
    },
    getBoolean(key: string): boolean {
      const val = data[key];
      if (typeof val !== 'boolean') {
        throw new Error(`Expected boolean for '${key}', got ${typeof val}`);
      }
      return val;
    },
    getOptionalBoolean(key: string): boolean | undefined {
      const val = data[key];
      if (val === undefined || val === null) return undefined;
      if (typeof val !== 'boolean') {
        throw new Error(`Expected boolean for '${key}', got ${typeof val}`);
      }
      return val;
    },
    get(key: string): unknown {
      return data[key];
    },
    raw(): Record<string, unknown> {
      return data;
    }
  };
}

/**
 * Legacy return type - returns Record for backward compatibility.
 * Use normalizeArgsTyped for new code.
 */
export function normalizeArgs(args: HandlerArgs, configs: ArgConfig[]): Record<string, unknown> {
  return normalizeArgsInternal(args, configs);
}

/**
 * Type-safe version that returns NormalizedArgs with accessor methods.
 */
export function normalizeArgsTyped(args: HandlerArgs, configs: ArgConfig[]): NormalizedArgs {
  return createNormalizedArgs(normalizeArgsInternal(args, configs));
}

function normalizeArgsInternal(args: HandlerArgs, configs: ArgConfig[]): Record<string, unknown> {
  const normalized: Record<string, unknown> = { ...args }; // Start with a shallow copy to preserve extra args

  for (const config of configs) {
    let val: unknown = undefined;

    // 1. Check primary key
    if (args[config.key] !== undefined && args[config.key] !== null && args[config.key] !== '') {
      val = args[config.key];
    }

    // 2. Check aliases if primary not found
    if (val === undefined && config.aliases) {
      for (const alias of config.aliases) {
        if (args[alias] !== undefined && args[alias] !== null && args[alias] !== '') {
          val = args[alias];
          break;
        }
      }
    }

    // 3. Apply default if still undefined
    if (val === undefined && config.default !== undefined) {
      val = config.default;
    }

    // 4. Validate 'required'
    if (config.required) {
      if (val === undefined || val === null || (typeof val === 'string' && val.trim() === '')) {
        const aliasStr = config.aliases ? ` (or ${config.aliases.join(', ')})` : '';
        throw new Error(`Missing required argument: ${config.key}${aliasStr}`);
      }
    }

    // 5. Apply map
    if (config.map && typeof val === 'string') {
      // Check for exact match first
      if (config.map[val]) {
        val = config.map[val];
      }
    }

    // 6. Custom validator
    if (config.validator && val !== undefined) {
      const err = config.validator(val);
      if (typeof err === 'string') {
        throw new Error(`Invalid argument '${config.key}': ${err}`);
      }
    }

    // 7. Store result (only if we found something or used a default)
    if (val !== undefined) {
      normalized[config.key] = val;
    }
  }

  return normalized;
}

// ============================================================================
// Type-safe extraction helpers for handler use
// ============================================================================

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
 * Extract a number from normalized args, asserting it exists.
 */
export function extractNumber(params: Record<string, unknown>, key: string): number {
  const val = params[key];
  if (typeof val !== 'number') {
    throw new Error(`Expected number for '${key}', got ${typeof val}`);
  }
  return val;
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
 * Extract a boolean from normalized args, asserting it exists.
 */
export function extractBoolean(params: Record<string, unknown>, key: string): boolean {
  const val = params[key];
  if (typeof val !== 'boolean') {
    throw new Error(`Expected boolean for '${key}', got ${typeof val}`);
  }
  return val;
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
 * Extract an array from normalized args, asserting it exists.
 * Optional validator function can check each element.
 */
export function extractArray<T>(params: Record<string, unknown>, key: string, validator?: (item: unknown, index: number) => boolean): T[] {
  const val = params[key];
  if (!Array.isArray(val)) {
    throw new Error(`Expected array for '${key}', got ${typeof val}`);
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
    // 3. Try smart resolution via actor tools
    if (tools.actorTools && typeof tools.actorTools.findByName === 'function') {
      try {
        const res = await tools.actorTools.findByName(potentialName) as FindByNameResult;
        const container = res && (res.result || res);
        const actors = container && Array.isArray(container.actors) ? container.actors : [];
        if (actors.length > 0) {
          const first = actors[0];
          const resolvedPath = first.path || first.objectPath || first.levelPath;
          if (typeof resolvedPath === 'string' && resolvedPath.trim().length > 0) {
            return resolvedPath.trim();
          }
        }
      } catch {
        // Ignore lookup errors
      }
    }
    // Fallback to the name itself
    if (fallback) return potentialName;
  }

  return undefined;
}
