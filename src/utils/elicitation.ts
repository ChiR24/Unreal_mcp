import { Logger } from './logger.js';
import { isRecord } from './type-guards.js';

// Minimal helper to opportunistically use MCP Elicitation when available.
// Safe across clients: validates schema shape and handles timeouts and -32601 errors.
export type PrimitiveSchema =
  | { type: 'string'; title?: string; description?: string; minLength?: number; maxLength?: number; pattern?: string; format?: 'email'|'uri'|'date'|'date-time'; default?: string }
  | { type: 'number'|'integer'; title?: string; description?: string; minimum?: number; maximum?: number; default?: number }
  | { type: 'boolean'; title?: string; description?: string; default?: boolean }
  | { type: 'string'; enum: string[]; enumNames?: string[]; title?: string; description?: string; default?: string };

export interface ElicitSchema {
  type: 'object';
  properties: Record<string, PrimitiveSchema>;
  required?: string[];
}

export interface ElicitOptions {
  timeoutMs?: number;
  // Handler invoked when elicitation cannot be performed.
  alternate?: () => Promise<{ ok: boolean; value?: unknown; error?: string }>;
}

type ElicitCapableServer = {
  elicitInput?: unknown;
};

function parsePositiveIntegerEnv(raw: string): number | undefined {
  const trimmed = raw.trim();
  if (!/^\d+$/.test(trimmed)) return undefined;

  const parsed = Number(trimmed);
  return Number.isInteger(parsed) && parsed > 0 ? parsed : undefined;
}

export function createElicitationHelper(server: ElicitCapableServer, log: Logger) {
  // We do not require explicit capability detection: we optimistically try once
  // and disable on a Method-not-found (-32601) error for the session.
  let supported = true; // optimistic; will be set false on first failure

  const MIN_TIMEOUT_MS = 30_000;
  const MAX_TIMEOUT_MS = 10 * 60 * 1000;
  const DEFAULT_TIMEOUT_MS = 3 * 60 * 1000;

  const clampTimeoutMs = (timeoutMs: number): number => Math.min(Math.max(timeoutMs, MIN_TIMEOUT_MS), MAX_TIMEOUT_MS);

  const timeoutEnvRaw = process.env.MCP_ELICITATION_TIMEOUT_MS ?? process.env.ELICITATION_TIMEOUT_MS ?? '';
  const parsedEnvTimeout = parsePositiveIntegerEnv(timeoutEnvRaw);
  const defaultTimeoutMs = parsedEnvTimeout !== undefined
    ? clampTimeoutMs(parsedEnvTimeout)
    : DEFAULT_TIMEOUT_MS;

  if (timeoutEnvRaw) {
    log.debug('Configured elicitation timeout override detected', {
      defaultTimeoutMs,
      fromEnv: timeoutEnvRaw
    });
  }

  function isSafeSchema(schema: ElicitSchema): boolean {
    if (!isRecord(schema) || schema.type !== 'object' || !isRecord(schema.properties)) return false;

    const properties: Record<string, unknown> = schema.properties;
    const propertyEntries = Object.entries(properties);
    const propertyKeys = propertyEntries.map(([key]) => key);

    if (schema.required) {
      if (!Array.isArray(schema.required)) return false;
      const invalidRequired = schema.required.some((key) => typeof key !== 'string' || !propertyKeys.includes(key));
      if (invalidRequired) return false;
    }

    return propertyEntries.every(([, rawSchema]) => {
      if (!isRecord(rawSchema)) return false;

      if ('properties' in rawSchema || 'items' in rawSchema) return false; // nested schemas unsupported

      if (Array.isArray(rawSchema.enum)) {
        const enumValues = rawSchema.enum;
        const allStrings = enumValues.every((value: unknown) => typeof value === 'string');
        if (!allStrings) return false;
        return !('type' in rawSchema) || rawSchema.type === 'string';
      }

      if (rawSchema.type === 'string') return true;
      if (rawSchema.type === 'number' || rawSchema.type === 'integer') return true;
      if (rawSchema.type === 'boolean') return true;

      return false;
    });
  }

  async function elicit(message: string, requestedSchema: ElicitSchema, opts: ElicitOptions = {}) {
    if (!supported || !isSafeSchema(requestedSchema)) {
      if (opts.alternate) return opts.alternate();
      return { ok: false, error: 'elicitation-unsupported' };
    }

    const params = { message, requestedSchema } as Record<string, unknown>;

    try {
      const elicitMethod = server.elicitInput;
      if (typeof elicitMethod !== 'function') {
        supported = false;
        throw new Error('elicitInput-not-available');
      }

      const requestedTimeout = typeof opts.timeoutMs === 'number' && Number.isFinite(opts.timeoutMs)
        ? opts.timeoutMs
        : undefined;
      const timeoutMs = requestedTimeout !== undefined ? clampTimeoutMs(requestedTimeout) : defaultTimeoutMs;
      const res = await elicitMethod.call(server, params, { timeout: timeoutMs });
      if (!isRecord(res)) {
        if (opts.alternate) return opts.alternate();
        return { ok: false, error: 'unexpected-response' };
      }
      const action = res?.action;
      const content = res?.content;

      if (action === 'accept') return { ok: true, value: content };
      if (action === 'decline' || action === 'cancel') {
        if (opts.alternate) return opts.alternate();
        return { ok: false, error: action };
      }
      if (opts.alternate) return opts.alternate();
      return { ok: false, error: 'unexpected-response' };
    } catch (e: unknown) {
      const errObj = isRecord(e) ? e : null;
      const msg = String(errObj?.message || e);
      const nestedError = isRecord(errObj?.error) ? errObj.error : null;
      const code = errObj?.code ?? nestedError?.code;
      // If client doesn't support it, don’t try again this session
      if (
        msg.includes('Method not found') ||
        msg.includes('elicitInput-not-available') ||
        msg.includes('request-not-available') ||
        String(code) === '-32601'
      ) {
        supported = false;
      }
      log.debug('Elicitation failed', { error: msg, code, usingAlternate: Boolean(opts.alternate) });
      if (opts.alternate) return opts.alternate();
      return { ok: false, error: msg.toLowerCase().includes('timeout') ? 'timeout' : 'rpc-failed' };
    }
  }

  return {
    supports: () => supported,
    elicit,
    getDefaultTimeoutMs: () => defaultTimeoutMs
  };
}
