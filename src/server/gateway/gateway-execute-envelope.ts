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
import { extractChanges, extractHandles, extractTask } from '../../tools/catalog/capabilities/semantic/receipt-outcome.js';
import { maskSecretsDeep, redactText } from '../../tools/catalog/capabilities/semantic/receipt-redaction.js';
import { catalogRevision } from './gateway-capability-index.js';
import type { ExecuteTarget } from './gateway-execute-resolve.js';
import {
  correlationFields,
  elapsedMs,
  revisionFields,
  type GatewayReceiptContext
} from './gateway-receipt-context.js';

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
  /** Present only on a STALE_STATE refusal from the pre-dispatch policy seam. */
  readonly currentRevision?: string;
  readonly expectedRevision?: string;
};

const OPTION_CODES = new Set(['UNSUPPORTED_OPTION']);
const RANGE_CODES = new Set(['OUT_OF_RANGE']);
const DISPATCH_CODES = new Set(['NOT_CONNECTED', 'ROUTING_ERROR', 'DISPATCH_ERROR']);
const CAPABILITY_DISABLED_CODES = new Set(['TOOL_DISABLED', 'CAPABILITY_DISABLED']);
const CAPABILITY_UNAVAILABLE_CODES = new Set(['CAPABILITY_REMOVED', 'CAPABILITY_UNAVAILABLE']);
const CONFLICT_CODES = new Set(['FORM_CONFLICT', 'ALIAS_CONFLICT']);
const OUTPUT_VIOLATION_CODES = new Set(['OUTPUT_SCHEMA_VIOLATION']);
const OUTPUT_SIZE_CODES = new Set(['RESULT_TOO_LARGE']);
const UNREAL_CODES = new Set(['UNREAL_EXECUTION_ERROR']);

// Map one gateway error code onto the typed semantic error algebra. The mapping
// is additive: output-contract failures, disabled/missing capabilities, stale
// revisions, conflicts and dispatch/routing each classify to their own plan
// kind, and anything unmatched stays a validation error (the legacy default).
export function toSemanticError(failure: ExecuteFailure): SemanticError {
  const { errorCode, message } = failure;
  if (errorCode === 'STALE_STATE') {
    return {
      kind: 'staleState',
      code: 'STALE_STATE',
      message,
      currentRevision: failure.currentRevision ?? catalogRevision(),
      expectedRevision: failure.expectedRevision ?? catalogRevision()
    };
  }
  if (OPTION_CODES.has(errorCode)) {
    return {
      kind: 'option',
      code: 'UNSUPPORTED_OPTION',
      option: failure.option ?? '',
      supported: [...EXECUTION_OPTION_KEYS],
      message
    };
  }
  if (RANGE_CODES.has(errorCode)) {
    return { kind: 'range', code: 'OUT_OF_RANGE', field: failure.field ?? failure.pointer ?? '', message };
  }
  if (DISPATCH_CODES.has(errorCode)) {
    return {
      kind: 'dispatch',
      code: errorCode === 'NOT_CONNECTED' ? 'NOT_CONNECTED' : 'DISPATCH_ERROR',
      message,
      retryable: true
    };
  }
  if (CAPABILITY_DISABLED_CODES.has(errorCode)) {
    return { kind: 'capability', code: 'CAPABILITY_DISABLED', message, retryable: false };
  }
  if (CAPABILITY_UNAVAILABLE_CODES.has(errorCode)) {
    return { kind: 'capability', code: 'CAPABILITY_UNAVAILABLE', message, retryable: false };
  }
  if (CONFLICT_CODES.has(errorCode)) {
    return { kind: 'conflict', code: 'STATE_CONFLICT', message };
  }
  if (OUTPUT_VIOLATION_CODES.has(errorCode)) {
    return {
      kind: 'output',
      code: 'OUTPUT_SCHEMA_VIOLATION',
      message,
      ...(failure.pointer === undefined ? {} : { pointer: failure.pointer })
    };
  }
  if (OUTPUT_SIZE_CODES.has(errorCode)) {
    return {
      kind: 'output',
      code: 'RESULT_TOO_LARGE',
      message,
      ...(failure.resultChars === undefined ? {} : { resultChars: failure.resultChars })
    };
  }
  if (UNREAL_CODES.has(errorCode)) {
    return { kind: 'execution', code: 'UNREAL_ENGINE_ERROR', message, retryable: false };
  }
  return {
    kind: 'validation',
    code: 'VALIDATION_ERROR',
    message,
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

export function refuseWithTarget(
  target: ExecuteTarget,
  failure: ResolvedFailure,
  context: GatewayReceiptContext
): Record<string, unknown> {
  return executeErrorEnvelope({
    ...failure,
    record: target.record,
    ...(target.resolvedFromAlias === undefined ? {} : { resolvedFromAlias: target.resolvedFromAlias }),
    ...(target.migratedFrom === undefined ? {} : { migratedFrom: target.migratedFrom })
  }, context);
}

export function executeErrorEnvelope(
  failure: ExecuteFailure,
  context: GatewayReceiptContext
): Record<string, unknown> {
  const receipt: Receipt | undefined = failure.record === undefined
    ? undefined
    : buildErrorReceipt({
      capabilityId: failure.record.id,
      error: toSemanticError(failure),
      ...correlationFields(context),
      ...revisionFields(failure.record),
      timingMs: elapsedMs(context)
    });

  return definedOnly({
    success: false,
    operation: 'execute',
    errorCode: failure.errorCode,
    error: redactText(failure.message),
    message: redactText(failure.message),
    correlationId: context.correlationId,
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
    result: maskSecretsDeep(failure.detail),
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
}, context: GatewayReceiptContext): Record<string, unknown> {
  const capabilityId: CapabilityId = input.record.id;
  // The projected output is built from declared, schema-checked fields, then
  // deep-masked so a secret echoed into a declared output field never survives
  // on the nested receipt.data (the outer envelope result is masked separately).
  const data = JsonValueSchema.safeParse(maskSecretsDeep(input.canonicalOutput));
  const receipt = buildSuccessReceipt({
    capabilityId,
    data: data.success ? data.data : null,
    handles: extractHandles(input.result),
    changes: extractChanges(input.result),
    task: extractTask(input.result),
    warnings: input.warnings,
    ...correlationFields(context),
    ...revisionFields(input.record),
    timingMs: elapsedMs(context),
    validation: { outputSchema: 'passed' }
  });

  return definedOnly({
    success: true,
    operation: 'execute',
    correlationId: context.correlationId,
    catalogRevision: catalogRevision(),
    ...capabilityFields(input.record),
    resolvedFromAlias: input.resolvedFromAlias,
    migratedFrom: input.migratedFrom,
    options: input.options,
    warnings: input.warnings.length > 0 ? input.warnings : undefined,
    receipt,
    result: maskSecretsDeep(input.result)
  });
}
