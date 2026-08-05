import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';

export interface ArgConfig {
  key: string;
  aliases?: string[];
  required?: boolean;
  default?: unknown;
  map?: Record<string, string>;
  validator?: (val: unknown) => void | string;
}

export interface NormalizedArgs {
  getString(key: string): string;
  getOptionalString(key: string): string | undefined;
  getNumber(key: string): number;
  getOptionalNumber(key: string): number | undefined;
  getBoolean(key: string): boolean;
  getOptionalBoolean(key: string): boolean | undefined;
  get(key: string): unknown;
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

export function normalizeArgs(args: HandlerArgs, configs: ArgConfig[]): Record<string, unknown> {
  return normalizeArgsInternal(args, configs);
}

export function normalizeArgsTyped(args: HandlerArgs, configs: ArgConfig[]): NormalizedArgs {
  return createNormalizedArgs(normalizeArgsInternal(args, configs));
}

function normalizeArgsInternal(args: HandlerArgs, configs: ArgConfig[]): Record<string, unknown> {
  const normalized: Record<string, unknown> = { ...args };

  for (const config of configs) {
    let val: unknown = undefined;

    if (args[config.key] !== undefined && args[config.key] !== null && args[config.key] !== '') {
      val = args[config.key];
    }

    if (val === undefined && config.aliases) {
      for (const alias of config.aliases) {
        if (args[alias] !== undefined && args[alias] !== null && args[alias] !== '') {
          val = args[alias];
          break;
        }
      }
    }

    if (val === undefined && config.default !== undefined) {
      val = config.default;
    }

    if (config.required) {
      if (val === undefined || val === null || (typeof val === 'string' && val.trim() === '')) {
        const aliasStr = config.aliases ? ` (or ${config.aliases.join(', ')})` : '';
        throw new Error(`Missing required argument: ${config.key}${aliasStr}`);
      }
    }

    if (config.map && typeof val === 'string') {
      const mappedValue = config.map[val];
      if (mappedValue) {
        val = mappedValue;
      }
    }

    if (config.validator && val !== undefined) {
      const err = config.validator(val);
      if (typeof err === 'string') {
        throw new Error(`Invalid argument '${config.key}': ${err}`);
      }
    }

    if (val !== undefined) {
      normalized[config.key] = val;
    }
  }

  return normalized;
}
