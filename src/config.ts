import { z } from 'zod';
import { Logger } from './utils/logger.js';
import { isRecord } from './utils/type-guards.js';
import dotenv from 'dotenv';

// Suppress dotenv output to avoid corrupting MCP stdout stream.
// Unit tests assert schema defaults and must not inherit developer-local .env values.
const shouldLoadDotenv = process.env.NODE_ENV !== 'test' && process.env.VITEST !== 'true' && process.env.VITEST_WORKER_ID === undefined;
if (shouldLoadDotenv) {
  const originalWrite = process.stdout.write;

  process.stdout.write = function () { return true; } as typeof process.stdout.write;
  try {
    dotenv.config();
  } finally {
    process.stdout.write = originalWrite;
  }
}

const log = new Logger('Config');

const stringToBoolean = (val: unknown) => {
  if (typeof val === 'boolean') return val;
  if (typeof val === 'string') {
    const lower = val.toLowerCase().trim();
    return lower === 'true' || lower === '1' || lower === 'on';
  }
  return false;
};

export const stringToPositiveInteger = (val: unknown, defaultVal: number) => {
  if (typeof val === 'number') {
    return Number.isInteger(val) && val > 0 ? val : defaultVal;
  }
  if (typeof val === 'string') {
    const trimmed = val.trim();
    if (trimmed.length === 0) return defaultVal;
    if (!/^\d+$/.test(trimmed)) return defaultVal;
    const parsed = Number(trimmed);
    return Number.isInteger(parsed) && parsed > 0 ? parsed : defaultVal;
  }
  return defaultVal;
};

function withEnvAliases(val: unknown): unknown {
  if (!isRecord(val)) return val;

  const env = { ...val };
  if (env.MCP_REQUEST_TIMEOUT_MS === undefined && env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS !== undefined) {
    env.MCP_REQUEST_TIMEOUT_MS = env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS;
  }
  if (env.MCP_CONNECTION_TIMEOUT_MS === undefined && env.UNREAL_CONNECTION_TIMEOUT !== undefined) {
    env.MCP_CONNECTION_TIMEOUT_MS = env.UNREAL_CONNECTION_TIMEOUT;
  }

  return env;
}

function normalizeAdditionalPathPrefix(value: string): string | undefined {
  const trimmed = value.trim();
  if (!trimmed) return undefined;

  let prefix = trimmed.startsWith('/') ? trimmed : `/${trimmed}`;
  prefix = prefix.replace(/\\/g, '/');
  if (!prefix.endsWith('/')) prefix += '/';

  if (prefix === '/' || prefix.includes('..') || prefix.includes('//')) return undefined;
  return prefix;
}

const EnvSchemaShape = z.object({
  // Server Settings
  NODE_ENV: z.enum(['development', 'production', 'test']).default('development'),
  LOG_LEVEL: z.enum(['debug', 'info', 'warn', 'error']).default('debug'),
  MCP_ROUTE_STDOUT_LOGS: z.preprocess(stringToBoolean, z.boolean()).default(true),

  // Unreal Settings
  UE_PROJECT_PATH: z.string().optional(),
  UE_EDITOR_EXE: z.string().optional(),


  // Connection Settings
  MCP_AUTOMATION_PORT: z.preprocess((v) => stringToPositiveInteger(v, 8091), z.number()).default(8091),
  MCP_AUTOMATION_HOST: z.string().default('127.0.0.1'),
  MCP_AUTOMATION_CLIENT_MODE: z.preprocess(stringToBoolean, z.boolean()).default(false),

  // Timeouts
  MCP_CONNECTION_TIMEOUT_MS: z.preprocess((v) => stringToPositiveInteger(v, 5000), z.number()).default(5000),
  MCP_REQUEST_TIMEOUT_MS: z.preprocess((v) => stringToPositiveInteger(v, 30000), z.number()).default(30000),

  // Tool Categories (comma-separated: core,world,gameplay,utility,all)
  MCP_DEFAULT_CATEGORIES: z.string().default('all'),

  // Additional UE content path prefixes (comma-separated)
  // Plugins with CanContainContent in their .uplugin register mount points beyond /Game/.
  // Example: /ProjectObject/,/ProjectAnimation/
  MCP_ADDITIONAL_PATH_PREFIXES: z.string().default(''),
});

export const EnvSchema = z.preprocess(withEnvAliases, EnvSchemaShape);

export type Config = z.infer<typeof EnvSchema>;

let config: Config;

try {
  config = EnvSchema.parse(process.env);
  log.debug('Configuration loaded successfully');
} catch (error) {
  if (error instanceof z.ZodError) {
    log.error('❌ Invalid configuration:', error.issues);
    log.warn('⚠️ Falling back to safe defaults.');
    // Fallback to parsing an empty object to get all defaults
    config = EnvSchema.parse({});
  } else {
    throw error;
  }
}

export { config };

/**
 * Parse MCP_ADDITIONAL_PATH_PREFIXES into a validated prefix array.
 * Each prefix is normalized to start and end with /.
 * Traversal patterns in configured values are silently dropped.
 */
export function getAdditionalPathPrefixes(): string[] {
  const raw = config.MCP_ADDITIONAL_PATH_PREFIXES;
  if (!raw || raw.trim() === '') return [];
  return Array.from(new Set(
    raw
      .split(',')
      .map(normalizeAdditionalPathPrefix)
      .filter((prefix): prefix is string => prefix !== undefined)
  ));
}
