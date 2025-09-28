import type { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { Logger } from './logger.js';

// Minimal helper to opportunistically use MCP Elicitation when available.
// Safe across clients: validates schema shape, handles timeouts and -32601 fallbacks.
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
  fallback?: () => Promise<{ ok: boolean; value?: any; error?: string }>;
}

export function createElicitationHelper(server: Server, log: Logger) {
  // We do not require explicit capability detection: we optimistically try once
  // and disable on a Method-not-found (-32601) error for the session.
  let supported = true; // optimistic; will be set false on first failure

  const MIN_TIMEOUT_MS = 30_000;
  const MAX_TIMEOUT_MS = 10 * 60 * 1000;
  const DEFAULT_TIMEOUT_MS = 3 * 60 * 1000;

  const timeoutEnvRaw = process.env.MCP_ELICITATION_TIMEOUT_MS ?? process.env.ELICITATION_TIMEOUT_MS ?? '';
  const parsedEnvTimeout = Number.parseInt(timeoutEnvRaw, 10);
  const defaultTimeoutMs = Number.isFinite(parsedEnvTimeout) && parsedEnvTimeout > 0
    ? Math.min(Math.max(parsedEnvTimeout, MIN_TIMEOUT_MS), MAX_TIMEOUT_MS)
    : DEFAULT_TIMEOUT_MS;

  if (timeoutEnvRaw) {
    log.debug('Configured elicitation timeout override detected', {
      defaultTimeoutMs,
      fromEnv: timeoutEnvRaw
    });
  }

  function isSafeSchema(schema: ElicitSchema): boolean {
    if (!schema || schema.type !== 'object' || typeof schema.properties !== 'object') return false;

    const propertyEntries = Object.entries(schema.properties ?? {});
    const propertyKeys = propertyEntries.map(([key]) => key);

    if (schema.required) {
      if (!Array.isArray(schema.required)) return false;
      const invalidRequired = schema.required.some((key) => typeof key !== 'string' || !propertyKeys.includes(key));
      if (invalidRequired) return false;
    }

    return propertyEntries.every(([, rawSchema]) => {
      if (!rawSchema || typeof rawSchema !== 'object') return false;
      const primitive = rawSchema as PrimitiveSchema & { properties?: unknown; items?: unknown }; // narrow for guards

      if ('properties' in primitive || 'items' in primitive) return false; // nested schemas unsupported

      if (Array.isArray((primitive as any).enum)) {
        const enumValues = (primitive as any).enum;
        const allStrings = enumValues.every((value: unknown) => typeof value === 'string');
        if (!allStrings) return false;
        return !('type' in primitive) || (primitive as any).type === 'string';
      }

      if ((primitive as any).type === 'string') return true;
      if ((primitive as any).type === 'number' || (primitive as any).type === 'integer') return true;
      if ((primitive as any).type === 'boolean') return true;

      return false;
    });
  }

  async function elicit(message: string, requestedSchema: ElicitSchema, opts: ElicitOptions = {}) {
    if (!supported || !isSafeSchema(requestedSchema)) {
      if (opts.fallback) return opts.fallback();
      return { ok: false, error: 'elicitation-unsupported' };
    }

    const params = { message, requestedSchema } as any;

    try {
      const elicitMethod = (server as any)?.elicitInput;
      if (typeof elicitMethod !== 'function') {
        supported = false;
        throw new Error('elicitInput-not-available');
      }

      const requestedTimeout = opts.timeoutMs;
      const timeoutMs = Math.max(MIN_TIMEOUT_MS, requestedTimeout ?? defaultTimeoutMs);
      const res: any = await elicitMethod.call(server, params, { timeout: timeoutMs });
      const action = res?.action;
      const content = res?.content;

      if (action === 'accept') return { ok: true, value: content };
      if (action === 'decline' || action === 'cancel') {
        if (opts.fallback) return opts.fallback();
        return { ok: false, error: action };
      }
      if (opts.fallback) return opts.fallback();
      return { ok: false, error: 'unexpected-response' };
    } catch (e: any) {
      const msg = String(e?.message || e);
      const code = (e as any)?.code ?? (e as any)?.error?.code;
      // If client doesn't support it, don’t try again this session
      if (
        msg.includes('Method not found') ||
        msg.includes('elicitInput-not-available') ||
        msg.includes('request-not-available') ||
        String(code) === '-32601'
      ) {
        supported = false;
      }
      log.debug('Elicitation failed; falling back', { error: msg, code });
      if (opts.fallback) return opts.fallback();
      return { ok: false, error: msg.includes('timeout') ? 'timeout' : 'rpc-failed' };
    }
  }

  return {
    supports: () => supported,
    elicit,
    getDefaultTimeoutMs: () => defaultTimeoutMs
  };
}