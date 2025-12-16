import { ITools } from '../../types/tool-interfaces.js';

export interface ArgConfig {
  /** The primary key to store the normalized value in. */
  key: string;
  /** A list of alternative keys (aliases) to look for in the input args. */
  aliases?: string[];
  /** If true, the value must be a non-empty string. Throws an error if missing or empty. */
  required?: boolean;
  /** If provided, uses this default value if no valid input is found. */
  default?: any;
  /** If provided, maps the input string using this dictionary (e.g. for friendly class names). */
  map?: Record<string, string>;
  /** Custom validation function. Throws or returns invalid message string if check fails. */
  validator?: (val: any) => void | string;
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
export function normalizeArgs(args: any, configs: ArgConfig[]): any {
  const normalized: any = { ...args }; // Start with a shallow copy to preserve extra args

  for (const config of configs) {
    let val: any = undefined;

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

/**
 * Helper to resolve an object path.
 * Can use a direct path, an actor name, or try to find an actor by name via the tool.
 */
export async function resolveObjectPath(
  args: any,
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
    if (typeof args[key] === 'string' && args[key].trim().length > 0) {
      return args[key].trim().replace(/\/+$/, '');
    }
  }

  // 2. Try actor keys - direct pass-through first
  let potentialName: string | undefined;
  for (const key of actorKeys) {
    if (typeof args[key] === 'string' && args[key].trim().length > 0) {
      potentialName = args[key].trim();
      break;
    }
  }

  if (potentialName) {
    // 3. Try smart resolution via actor tools
    if (tools.actorTools && typeof (tools.actorTools as any).findByName === 'function') {
      try {
        const res: any = await (tools.actorTools as any).findByName(potentialName);
        const container: any = res && (res.result || res);
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
