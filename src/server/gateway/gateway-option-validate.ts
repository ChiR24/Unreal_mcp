// src/server/gateway/gateway-option-validate.ts
// Stage 2-3 execution-option validation for the canonical execute pipeline:
// the gateway `options` envelope rules that mirror the native `/mcp` surface
// exactly. Extracted from gateway-execute-validate.ts.

import { isRecord } from '../../utils/validation/type-guards.js';
import { IdempotencyKeySchema } from '../../tools/catalog/capabilities/semantic/ids.js';
import {
  EXECUTION_OPTION_KEYS,
  LIVE_STATE_REVISION_KEYS
} from '../../tools/catalog/capabilities/semantic/execution-options.js';

export const MAX_TIMEOUT_MS = 600_000;

export type OptionViolation = {
  readonly errorCode: 'UNSUPPORTED_OPTION' | 'INVALID_OPTIONS' | 'OUT_OF_RANGE';
  readonly message: string;
  readonly option?: string;
  readonly pointer?: string;
};

/**
 * Cross-cutting execution controls live in the gateway `options` envelope and
 * never inside action `params`. The supported key set is Task 3's; the value
 * rules match the shared execute reference exactly so the two surfaces agree.
 */
export function validateExecutionOptions(raw: unknown): OptionViolation | undefined {
  if (raw === undefined || raw === null) return undefined;
  if (!isRecord(raw)) {
    return { errorCode: 'INVALID_OPTIONS', message: 'options must be an object.' };
  }

  const supported = new Set<string>(EXECUTION_OPTION_KEYS);
  for (const key of Object.keys(raw)) {
    if (!supported.has(key)) {
      return {
        errorCode: 'UNSUPPORTED_OPTION',
        option: key,
        message: `Unsupported execution option '${key}'. Supported: [${EXECUTION_OPTION_KEYS.join(', ')}]`
      };
    }
  }

  const timeout = Object.hasOwn(raw, 'timeoutMs') ? raw.timeoutMs : undefined;
  if (timeout !== undefined
    && (typeof timeout !== 'number' || !Number.isInteger(timeout) || timeout <= 0 || timeout > MAX_TIMEOUT_MS)) {
    return {
      errorCode: 'OUT_OF_RANGE',
      option: 'timeoutMs',
      message: `options.timeoutMs must be an integer in 1..${MAX_TIMEOUT_MS}`
    };
  }

  // Validated with the SAME schema `buildReceiptContext` parses with, so the two
  // can never disagree. Previously only the key NAME was checked here, and a
  // malformed value was dropped silently downstream: `runWithIdempotency` then
  // took the no-ledger path and the receipt omitted `idempotencyId`, so a retry
  // re-ran the mutation with nothing on the wire reporting that dedup was off.
  // A dedup guard that cannot be honoured must refuse, not proceed unprotected.
  const idempotencyKey = Object.hasOwn(raw, 'idempotencyKey') ? raw.idempotencyKey : undefined;
  if (idempotencyKey !== undefined && !IdempotencyKeySchema.safeParse(idempotencyKey).success) {
    return {
      errorCode: 'INVALID_OPTIONS',
      option: 'idempotencyKey',
      pointer: '/options/idempotencyKey',
      message: 'options.idempotencyKey must be a string of 1..128 characters.'
    };
  }

  return validateExpectedRevisions(Object.hasOwn(raw, 'expectedRevisions') ? raw.expectedRevisions : undefined);
}

/**
 * Shape-check the live-state pins. Mirrors McpParseExpectedRevisions in
 * the plugin exactly, so both transports refuse the same input with the same
 * code. The revision COMPARISON is deliberately not done here: it belongs on the
 * game thread immediately before mutation, where the value cannot be stale yet.
 */
function validateExpectedRevisions(raw: unknown): OptionViolation | undefined {
  if (raw === undefined) return undefined;
  if (!isRecord(raw)) {
    return {
      errorCode: 'INVALID_OPTIONS',
      pointer: '/options/expectedRevisions',
      message: 'options.expectedRevisions must be an object of state revisions.'
    };
  }

  const pinnable: readonly string[] = LIVE_STATE_REVISION_KEYS;
  for (const [key, value] of Object.entries(raw)) {
    if (!pinnable.includes(key)) {
      return {
        errorCode: 'UNSUPPORTED_OPTION',
        option: `expectedRevisions.${key}`,
        message: `Unsupported expected revision '${key}'. Supported: [${pinnable.join(', ')}]`
      };
    }
    if (typeof value !== 'number' || !Number.isInteger(value) || value < 1) {
      return {
        errorCode: 'OUT_OF_RANGE',
        option: `expectedRevisions.${key}`,
        message: `options.expectedRevisions.${key} must be an integer >= 1`
      };
    }
  }

  return undefined;
}

/** A gateway control smuggled into action params is refused, never forwarded. */
export function findControlKeyInParams(params: Record<string, unknown>): string | undefined {
  return EXECUTION_OPTION_KEYS.find((control) => Object.hasOwn(params, control));
}

