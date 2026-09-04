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
import { liveStateRevisionsFromEnvelope } from '../../tools/catalog/capabilities/semantic/live-state-revisions.js';
import { extractChanges, extractHandles, extractTask } from '../../tools/catalog/capabilities/semantic/receipt-outcome.js';
import { maskSecretsDeep, redactText } from '../../tools/catalog/capabilities/semantic/receipt-redaction.js';
import { catalogRevision } from './gateway-capability-index.js';
import { IDEMPOTENCY_CONFLICT_CODE } from './gateway-execute-idempotency.js';
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
  /** Set when `detail` was withheld for size, so "no detail" reads apart from "detail dropped". */
  readonly detailOmitted?: boolean;
  readonly resultChars?: number;
  /** Present only on a STALE_STATE refusal from the pre-dispatch policy seam. */
  readonly currentRevision?: string;
  readonly expectedRevision?: string;
  readonly requiredScope?: string;
  readonly grantedScopes?: readonly string[];
};

// Reported when a producer omits a field the receipt schema requires. An
// explicit marker beats a plausible-looking default: the schema demands a
// non-empty string, and substituting a real-looking value (a catalog digest, an
// 'admin' scope) makes an unreported field indistinguishable from a stated one.
const UNREPORTED_REVISION = 'unreported';
const UNREPORTED_SCOPE = 'unreported';

// Map one gateway error code onto the typed semantic error algebra.
//
// One switch, not nine module-level Sets behind a thirteen-branch if-chain:
// five of those Sets held a single string, four codes were matched with `===`
// instead, and a reader had to check both mechanisms to know whether a code was
// handled. Grouped case labels keep the multi-code classes explicit, and the
// codes now sit next to the branch that consumes them rather than 80 lines away.
//
// The mapping is additive: output-contract failures, disabled/missing
// capabilities, stale revisions, conflicts and dispatch/routing each classify to
// their own plan kind, and anything unmatched stays a validation error.
export function toSemanticError(failure: ExecuteFailure): SemanticError {
  const { errorCode, message } = failure;
  switch (errorCode) {
    case 'STALE_STATE':
      return {
        kind: 'staleState',
        code: 'STALE_STATE',
        message,
        // An explicit sentinel, never `catalogRevision()`. The plugin owns the
        // live-state comparison and may report STALE_STATE without the pair; the
        // old fallback substituted the CATALOG content digest for both, so the
        // receipt claimed current === expected on a refusal whose whole point is
        // that they differ, and mixed a catalog digest into a live-state error.
        // The schema requires both fields, so they are marked unreported rather
        // than omitted.
        currentRevision: failure.currentRevision ?? UNREPORTED_REVISION,
        expectedRevision: failure.expectedRevision ?? UNREPORTED_REVISION
      };
    // UNSUPPORTED_PREVIEW is an option refusal too: checkPreviewSupport emits it
    // with `option: 'preview'`. It was absent from the classification, so it
    // fell through to the trailing `validation` default, its `option` field was
    // dropped, and the client saw a generic VALIDATION_ERROR for a refusal that
    // names exactly which option is unsupported.
    case 'UNSUPPORTED_OPTION':
    case 'UNSUPPORTED_PREVIEW':
      return {
        kind: 'option',
        code: 'UNSUPPORTED_OPTION',
        option: failure.option ?? '',
        supported: [...EXECUTION_OPTION_KEYS],
        message
      };
    case 'OUT_OF_RANGE':
      return { kind: 'range', code: 'OUT_OF_RANGE', field: failure.field ?? failure.pointer ?? '', message };
    case 'NOT_CONNECTED':
    case 'ROUTING_ERROR':
    case 'DISPATCH_ERROR':
      return {
        kind: 'dispatch',
        code: errorCode === 'NOT_CONNECTED' ? 'NOT_CONNECTED' : 'DISPATCH_ERROR',
        message,
        retryable: true
      };
    case 'TOOL_DISABLED':
    case 'CAPABILITY_DISABLED':
      return { kind: 'capability', code: 'CAPABILITY_DISABLED', message, retryable: false };
    case 'CAPABILITY_REMOVED':
    case 'CAPABILITY_UNAVAILABLE':
      return { kind: 'capability', code: 'CAPABILITY_UNAVAILABLE', message, retryable: false };
    case IDEMPOTENCY_CONFLICT_CODE:
      return { kind: 'conflict', code: IDEMPOTENCY_CONFLICT_CODE, message };
    case 'FORM_CONFLICT':
    case 'ALIAS_CONFLICT':
      return { kind: 'conflict', code: 'STATE_CONFLICT', message };
    case 'OUTPUT_SCHEMA_VIOLATION':
      return {
        kind: 'output',
        code: 'OUTPUT_SCHEMA_VIOLATION',
        message,
        ...(failure.pointer === undefined ? {} : { pointer: failure.pointer })
      };
    case 'RESULT_TOO_LARGE':
      return {
        kind: 'output',
        code: 'RESULT_TOO_LARGE',
        message,
        ...(failure.resultChars === undefined ? {} : { resultChars: failure.resultChars })
      };
    case 'UNREAL_EXECUTION_ERROR':
      return { kind: 'execution', code: 'UNREAL_ENGINE_ERROR', message, retryable: false };
    case 'SCOPE_NOT_GRANTED':
      return {
        kind: 'authorization',
        code: 'SCOPE_NOT_GRANTED',
        message,
        // Sentinel, not an invented 'admin'. Claiming a scope the producer
        // never stated tells the caller to obtain a grant that may not be the
        // one actually required.
        requiredScope: failure.requiredScope ?? UNREPORTED_SCOPE,
        grantedScopes: failure.grantedScopes ?? []
      };
    case 'CONSENT_REQUIRED':
      // Sentinel, not an invented 'destructive': the consent strength a caller
      // must acknowledge is the producer's to state.
      return { kind: 'consent', code: 'CONSENT_REQUIRED', message, scope: failure.requiredScope ?? UNREPORTED_SCOPE };
    default:
      return {
        kind: 'validation',
        code: 'VALIDATION_ERROR',
        message,
        ...(failure.pointer === undefined ? {} : { pointer: failure.pointer })
      };
  }
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
  const liveRevisions = liveStateRevisionsFromEnvelope(failure.detail);
  const receipt: Receipt | undefined = failure.record === undefined
    ? undefined
    : buildErrorReceipt({
      capabilityId: failure.record.id,
      error: toSemanticError(failure),
      ...correlationFields(context),
      ...revisionFields(failure.record),
      liveRevisions,
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
    detailOmitted: failure.detailOmitted,
    liveRevisions,
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
  const liveRevisions = liveStateRevisionsFromEnvelope(input.result);
  // The projected output is built from declared, schema-checked fields, then
  // deep-masked so a secret echoed into a declared output field never survives
  // on the published payload (the outer envelope result is masked separately).
  const parsedOutput = JsonValueSchema.safeParse(maskSecretsDeep(input.canonicalOutput));
  const publishedData = parsedOutput.success ? parsedOutput.data : null;
  const receipt = buildSuccessReceipt({
    capabilityId,
    data: publishedData,
    handles: extractHandles(input.result),
    changes: extractChanges(input.result),
    task: extractTask(input.result),
    warnings: input.warnings,
    ...correlationFields(context),
    ...revisionFields(input.record),
    timingMs: elapsedMs(context),
    validation: { outputSchema: 'passed' },
    liveRevisions
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
    liveRevisions,
    receipt,
    // The documented payload location, and the one the native surface already
    // publishes. Without it a client following the receipt contract read
    // `structuredContent.data` and found the payload on native and NOTHING over
    // stdio, where it was reachable only as the unprojected `result` or nested
    // under `receipt.data`. The receipt now binds to it through receipt.dataDigest (dogfood #11).
    data: publishedData,
    result: maskSecretsDeep(input.result)
  });
}
