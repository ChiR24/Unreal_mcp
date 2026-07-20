// src/server/gateway/gateway-execute-envelope.ts
// Final stage of the canonical execute pipeline: one wire envelope for every
// outcome.
//
// Two contracts are carried at once, deliberately:
//   * the gateway envelope (`success`/`operation`/`errorCode`/`error`/`message`
//     plus the guided `suggestions`/`nextCall`) that the read-only orchestrator
//     and every existing client already depend on, and
//   * the Task 3 semantic `receipt`, which names the canonical capability and
//     carries the typed error algebra.
//
// The receipt is built through the Task 3 builders so its shape cannot drift
// from `ReceiptSchema`; the suites assert conformance against that schema on
// both the success and the error path. It is not re-parsed per call because
// that would walk every result payload a second time for no added guarantee.

import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';
import type { CapabilityId } from '../../tools/catalog/capabilities/identifiers.js';
import {
  buildErrorReceipt,
  buildSuccessReceipt,
  type Receipt
} from '../../tools/catalog/capabilities/semantic/envelope.js';
import type { SemanticError } from '../../tools/catalog/capabilities/semantic/errors.js';
import { EXECUTION_OPTION_KEYS } from '../../tools/catalog/capabilities/semantic/execution-options.js';
import { JsonValueSchema } from '../../tools/catalog/capabilities/semantic/property-assignment.js';
import { catalogRevision } from './gateway-capability-index.js';
import type { ExecuteTarget } from './gateway-execute-resolve.js';

export type ExecuteFailure = {
  readonly errorCode: string;
  readonly message: string;
  /** Present once the request resolved to exactly one capability. */
  readonly record?: CapabilityRecord;
  readonly option?: string;
  readonly field?: string;
  readonly pointer?: string;
  readonly suggestions?: readonly string[];
  readonly nextCall?: Record<string, unknown>;
  readonly availableActions?: readonly string[];
  readonly allowedParameters?: readonly string[];
  /** Echoed when resolution failed before it reached a record. */
  readonly requestedTool?: string;
  readonly requestedAction?: string;
  // Provenance stays visible on refusals too: a caller who used an alias or a
  // legacy pair must see what it resolved to even when the call is rejected.
  readonly resolvedFromAlias?: string;
  readonly migratedFrom?: { readonly tool: string; readonly action: string };
  /** Whatever Unreal actually reported, preserved verbatim beside the typed error. */
  readonly detail?: unknown;
  readonly resultChars?: number;
};

const RANGE_CODES = new Set(['OUT_OF_RANGE']);
const OPTION_CODES = new Set(['UNSUPPORTED_OPTION']);
const CONNECTION_CODES = new Set(['NOT_CONNECTED']);
const UNREAL_CODES = new Set(['UNREAL_EXECUTION_ERROR']);
const EXECUTION_CODES = new Set(['TOOL_DISABLED', 'RESULT_TOO_LARGE']);

/** Map one gateway error code onto the typed semantic error algebra. */
export function toSemanticError(failure: ExecuteFailure): SemanticError {
  if (OPTION_CODES.has(failure.errorCode)) {
    return {
      kind: 'option',
      code: 'UNSUPPORTED_OPTION',
      option: failure.option ?? '',
      supported: [...EXECUTION_OPTION_KEYS],
      message: failure.message
    };
  }
  if (RANGE_CODES.has(failure.errorCode)) {
    return {
      kind: 'range',
      code: 'OUT_OF_RANGE',
      field: failure.field ?? failure.pointer ?? '',
      message: failure.message
    };
  }
  if (CONNECTION_CODES.has(failure.errorCode)) {
    return { kind: 'execution', code: 'CONNECTION_ERROR', message: failure.message, retryable: true };
  }
  if (UNREAL_CODES.has(failure.errorCode)) {
    return { kind: 'execution', code: 'UNREAL_ENGINE_ERROR', message: failure.message, retryable: false };
  }
  if (EXECUTION_CODES.has(failure.errorCode)) {
    return { kind: 'execution', code: 'EXECUTION_ERROR', message: failure.message, retryable: false };
  }
  return {
    kind: 'validation',
    code: 'VALIDATION_ERROR',
    message: failure.message,
    ...(failure.pointer === undefined ? {} : { pointer: failure.pointer })
  };
}

// The legacy `tool`/`action` echo is part of the guided-error contract both
// transports are diffed against, so it is emitted from the resolved record when
// there is one and from the raw request when resolution never got that far.
function capabilityFields(
  record: CapabilityRecord | undefined,
  requested?: { readonly tool?: string; readonly action?: string }
): Record<string, unknown> {
  if (record === undefined) {
    return { tool: requested?.tool, action: requested?.action };
  }
  return {
    capability: record.id,
    tool: record.routing.parentTool,
    action: record.legacyIds[0]?.action ?? record.routing.dispatchAction
  };
}

function definedOnly(entries: Record<string, unknown>): Record<string, unknown> {
  const out: Record<string, unknown> = {};
  for (const [key, value] of Object.entries(entries)) {
    if (value !== undefined) out[key] = value;
  }
  return out;
}

/** A refusal raised after resolution, so the target supplies the provenance. */
export type ResolvedFailure = Omit<ExecuteFailure, 'record' | 'resolvedFromAlias' | 'migratedFrom'>;

export function refuseWithTarget(target: ExecuteTarget, failure: ResolvedFailure): Record<string, unknown> {
  return executeErrorEnvelope({
    ...failure,
    record: target.record,
    ...(target.resolvedFromAlias === undefined ? {} : { resolvedFromAlias: target.resolvedFromAlias }),
    ...(target.migratedFrom === undefined ? {} : { migratedFrom: target.migratedFrom })
  });
}

export function executeErrorEnvelope(failure: ExecuteFailure): Record<string, unknown> {
  const receipt: Receipt | undefined = failure.record === undefined
    ? undefined
    : buildErrorReceipt({
      capabilityId: failure.record.id,
      error: toSemanticError(failure)
    });

  return definedOnly({
    success: false,
    operation: 'execute',
    errorCode: failure.errorCode,
    error: failure.message,
    message: failure.message,
    catalogRevision: catalogRevision(),
    ...capabilityFields(failure.record, { tool: failure.requestedTool, action: failure.requestedAction }),
    resolvedFromAlias: failure.resolvedFromAlias,
    migratedFrom: failure.migratedFrom,
    suggestions: failure.suggestions,
    nextCall: failure.nextCall,
    availableActions: failure.availableActions,
    allowedParameters: failure.allowedParameters,
    pointer: failure.pointer,
    resultChars: failure.resultChars,
    result: failure.detail,
    receipt
  });
}

export function executeSuccessEnvelope(input: {
  readonly record: CapabilityRecord;
  readonly result: unknown;
  /** The projected payload that already satisfied the declared output schema. */
  readonly canonicalOutput: unknown;
  readonly resolvedFromAlias?: string;
  readonly migratedFrom?: { readonly tool: string; readonly action: string };
  readonly options?: Record<string, unknown>;
  readonly warnings: readonly string[];
}): Record<string, unknown> {
  const capabilityId: CapabilityId = input.record.id;
  // The projected output is built from declared, schema-checked fields, so this
  // parse is a bounded confirmation rather than a second walk of the raw result.
  const data = JsonValueSchema.safeParse(input.canonicalOutput);
  const receipt = buildSuccessReceipt({
    capabilityId,
    data: data.success ? data.data : null,
    warnings: input.warnings
  });

  return definedOnly({
    success: true,
    operation: 'execute',
    catalogRevision: catalogRevision(),
    ...capabilityFields(input.record),
    resolvedFromAlias: input.resolvedFromAlias,

    migratedFrom: input.migratedFrom,
    options: input.options,
    warnings: input.warnings.length > 0 ? input.warnings : undefined,
    receipt,
    result: input.result
  });
}
