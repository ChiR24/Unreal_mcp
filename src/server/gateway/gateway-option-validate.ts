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
import { hasOwn } from './gateway-schema-validate.js';

export const MAX_TIMEOUT_MS = 600_000;

export type OptionViolation = {
  readonly errorCode: 'UNSUPPORTED_OPTION' | 'INVALID_OPTIONS' | 'OUT_OF_RANGE' | 'UNSUPPORTED_PREVIEW';
  readonly message: string;
  readonly option?: string;
  readonly pointer?: string;
};

/**
 * Accepted option keys no dispatch path reads. `dispatchAndValidate` builds
 * `{ ...params, action, subAction }` and passes `options` to the envelope builder
 * alone, so each was validated, echoed on a success receipt, then dropped: a
 * client pinning `savePolicy: 'none'` got the capability's own save behaviour and
 * a receipt naming the policy it asked for. `preview` is the same defect but
 * keeps its own gate and code because its silent failure is destructive.
 */
export const UNIMPLEMENTED_EXECUTION_OPTION_KEYS: readonly string[] = [
  'savePolicy',
  'validationLevel',
  'taskPreference'
];

/**
 * Derived from both refused sets, not listed, because this is what a refused
 * caller is redirected to — naming an option that does nothing would send them
 * into a second refusal. It previously claimed all seven non-preview keys were
 * honored when only `idempotencyKey`, `expectedCatalogRevision`,
 * `expectedRevisions` and `timeoutMs` are.
 */
export const HONORED_EXECUTION_OPTION_KEYS: readonly string[] =
  EXECUTION_OPTION_KEYS.filter(
    (key) => key !== 'preview' && !UNIMPLEMENTED_EXECUTION_OPTION_KEYS.includes(key)
  );

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

  const unimplemented = UNIMPLEMENTED_EXECUTION_OPTION_KEYS.find((key) => hasOwn(raw, key));
  if (unimplemented !== undefined) {
    return {
      errorCode: 'UNSUPPORTED_OPTION',
      option: unimplemented,
      pointer: `/options/${unimplemented}`,
      message: unimplementedOptionMessage(unimplemented)
    };
  }

  const timeout = raw.timeoutMs;
  if (timeout !== undefined
    && (typeof timeout !== 'number' || !Number.isInteger(timeout) || timeout <= 0 || timeout > MAX_TIMEOUT_MS)) {
    return {
      errorCode: 'OUT_OF_RANGE',
      option: 'timeoutMs',
      message: `options.timeoutMs must be an integer in 1..${MAX_TIMEOUT_MS}`
    };
  }

  const preview = raw.preview;
  if (preview !== undefined && typeof preview !== 'boolean') {
    return {
      errorCode: 'INVALID_OPTIONS',
      pointer: '/options/preview',
      message: 'options.preview must be a boolean.'
    };
  }

  // Validated with the SAME schema `buildReceiptContext` parses with, so the two
  // can never disagree. Previously only the key NAME was checked here, and a
  // malformed value was dropped silently downstream: `runWithIdempotency` then
  // took the no-ledger path and the receipt omitted `idempotencyId`, so a retry
  // re-ran the mutation with nothing on the wire reporting that dedup was off.
  // A dedup guard that cannot be honoured must refuse, not proceed unprotected.
  const idempotencyKey = raw.idempotencyKey;
  if (idempotencyKey !== undefined && !IdempotencyKeySchema.safeParse(idempotencyKey).success) {
    return {
      errorCode: 'INVALID_OPTIONS',
      option: 'idempotencyKey',
      pointer: '/options/idempotencyKey',
      message: 'options.idempotencyKey must be a string of 1..128 characters.'
    };
  }

  return validateExpectedRevisions(raw.expectedRevisions);
}

/**
 * Shape-check the Task 42 live-state pins. Mirrors McpParseExpectedRevisions in
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
  return EXECUTION_OPTION_KEYS.find((control) => hasOwn(params, control));
}

/**
 * Shared refusal text for an accepted-but-unread option, emitted verbatim by
 * both transports so the same request is refused with the same sentence over
 * stdio and native `/mcp`.
 */
export function unimplementedOptionMessage(option: string): string {
  return `Execution option '${option}' is accepted by the options schema but no dispatch path reads it. `
    + 'Honoring it would run the operation with different behaviour than requested and report success. '
    + `Re-send without options.${option}.`;
}

/**
 * The single refusal text both transports emit, so a client sees the same
 * sentence whether it reached the gateway over stdio or native `/mcp`. The
 * native mirror interpolates the capability id through FString::Printf with the
 * identical wording (asserted by the Task 43 transport-equivalence suite).
 */
export function unsupportedPreviewMessage(capabilityId: string): string {
  return `Capability '${capabilityId}' does not implement options.preview. `
    + 'No dispatch path performs a dry run, so preview:true would perform the real operation. '
    + 'Re-send without options.preview to execute for real.';
}

/**
 * `preview: true` is refused for every capability, before dispatch.
 *
 * No dispatch path reads the option, so there is no dry run to perform: honoring
 * the request would apply the real, irreversible mutation and then report it as
 * a preview. `behavior.supportsPreview` is deliberately NOT consulted — 124
 * records declare it and 10 of those are destructive, yet not one declaration is
 * backed by an implementation, so trusting it would leave the fake dry run in
 * place for exactly the most dangerous capabilities.
 */
export function checkPreviewSupport(
  rawOptions: unknown,
  capabilityId: string
): OptionViolation | undefined {
  if (!isRecord(rawOptions) || rawOptions.preview !== true) return undefined;
  return {
    errorCode: 'UNSUPPORTED_PREVIEW',
    option: 'preview',
    pointer: '/options/preview',
    message: unsupportedPreviewMessage(capabilityId)
  };
}
