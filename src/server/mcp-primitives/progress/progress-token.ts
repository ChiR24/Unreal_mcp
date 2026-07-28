// src/server/mcp-primitives/progress/progress-token.ts
// Task 44: the client's progress token, preserved verbatim.
//
// MCP declares `_meta.progressToken` as `string | number`, and every progress
// notification the server emits MUST carry back the SAME value with the SAME
// type. A client correlates progress to its own in-flight request by that value,
// so substituting a server-side id (the JSON-RPC request id, a counter, a uuid)
// silently breaks correlation while still looking correct in a server-side test
// that only checks "a token was present".
//
// This module therefore only ever READS. It has no counter, no clock and no
// access to a request id, so there is nothing here to invent a token from: a
// client that sent no token gets no token, and the reporter stays silent.

import { isRecord } from '../../../utils/validation/type-guards.js';

/** The wire type of a client progress token. Never widened, never coerced. */
export type ProgressToken = string | number;

/**
 * Read the client's progress token out of a request's `_meta` bag.
 *
 * Returns the value BYTE FOR BYTE and TYPE FOR TYPE, so the string `'42'` and
 * the number `42` stay distinguishable. Returns `undefined` — never a
 * substitute — when the client sent nothing, when `_meta` is not an object, or
 * when the value is not a conforming `string | number`.
 *
 * Non-finite numbers are refused because `NaN`/`Infinity` serialize to `null`,
 * so echoing one back would be a corrupted token rather than a preserved one.
 */
export function readProgressToken(meta: unknown): ProgressToken | undefined {
  if (!isRecord(meta)) return undefined;

  const token: unknown = meta.progressToken;
  if (typeof token === 'string') return token;
  if (typeof token === 'number' && Number.isFinite(token)) return token;
  return undefined;
}
