import { z } from 'zod';
import { Logger } from './utils/logger.js';
import dotenv from 'dotenv';

// Suppress dotenv output to avoid corrupting MCP stdout stream
const originalWrite = process.stdout.write;
 
process.stdout.write = function () { return true; } as typeof process.stdout.write;
try {
  dotenv.config();
} finally {
  process.stdout.write = originalWrite;
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

const stringToNumber = (val: unknown, defaultVal: number) => {
  if (typeof val === 'number') return val;
  if (typeof val === 'string') {
    const parsed = parseInt(val, 10);
    return isNaN(parsed) ? defaultVal : parsed;
  }
  return defaultVal;
};

export const EnvSchema = z.object({
  // Server Settings
  NODE_ENV: z.enum(['development', 'production', 'test']).default('development'),
  LOG_LEVEL: z.enum(['debug', 'info', 'warn', 'error']).default('debug'),
  MCP_ROUTE_STDOUT_LOGS: z.preprocess(stringToBoolean, z.boolean().default(true)),

  // Unreal Settings
  UE_PROJECT_PATH: z.string().optional(),
  UE_EDITOR_EXE: z.string().optional(),


  // Connection Settings
  MCP_AUTOMATION_PORT: z.preprocess((v) => stringToNumber(v, 8091), z.number().default(8091)),
  MCP_AUTOMATION_HOST: z.string().default('127.0.0.1'),
  MCP_AUTOMATION_CLIENT_MODE: z.preprocess(stringToBoolean, z.boolean().default(false)),

  // Timeouts
  MCP_CONNECTION_TIMEOUT_MS: z.preprocess((v) => stringToNumber(v, 5000), z.number().default(5000)),
  MCP_REQUEST_TIMEOUT_MS: z.preprocess((v) => stringToNumber(v, 30000), z.number().default(30000)),
});

export type Config = z.infer<typeof EnvSchema>;

let config: Config;

try {
  config = EnvSchema.parse(process.env);
  log.debug('Configuration loaded successfully');
} catch (error) {
  if (error instanceof z.ZodError) {
    log.error('❌ Invalid configuration:', error.format());
    log.warn('⚠️ Falling back to safe defaults.');
    // Fallback to parsing an empty object to get all defaults
    config = EnvSchema.parse({});
  } else {
    throw error;
  }
}

export { config };
