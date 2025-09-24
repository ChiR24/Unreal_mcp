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

  function isSafeSchema(schema: ElicitSchema): boolean {
    if (!schema || schema.type !== 'object' || typeof schema.properties !== 'object') return false;
    const values = Object.values(schema.properties || {});
    return values.every((p: any) => {
      if (!p || typeof p !== 'object') return false;
      if (p.type === 'string') return true;
      if (p.type === 'number' || p.type === 'integer') return true;
      if (p.type === 'boolean') return true;
      if (Array.isArray((p as any).enum)) return true; // enum is a specialized string schema
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
      const timeoutMs = opts.timeoutMs ?? 90_000;
      const timeout = new Promise((_, rej) => setTimeout(() => rej(new Error('timeout')), timeoutMs));
      // The SDK exposes request on the Server instance for client features.
      // We intentionally use any to avoid depending on SDK internals.
      const req = (server as any).request?.('elicitation/create', params) ?? (server as any).transport?.request?.('elicitation/create', params);
      if (!req || typeof req.then !== 'function') throw new Error('request-not-available');

      const res: any = await Promise.race([req, timeout]);
      const action = res?.result?.action;
      const content = res?.result?.content;

      if (action === 'accept') return { ok: true, value: content };
      if (action === 'decline' || action === 'cancel') {
        if (opts.fallback) return opts.fallback();
        return { ok: false, error: action };
      }
      if (opts.fallback) return opts.fallback();
      return { ok: false, error: 'unexpected-response' };
    } catch (e: any) {
      const msg = String(e?.message || e);
      // If client doesn't support it, don’t try again this session
      if (msg.includes('Method not found') || String((e as any)?.code) === '-32601') {
        supported = false;
      }
      log.debug('Elicitation failed; falling back', { error: msg });
      if (opts.fallback) return opts.fallback();
      return { ok: false, error: 'rpc-failed' };
    }
  }

  return {
    supports: () => supported,
    elicit
  };
}