// src/server/gateway/gateway-execute.ts
// The gateway `execute` operation: canonical validation, one dispatch, one receipt.
//
// Task 24 extracted this seam unchanged; Task 26 replaced its manifest-driven
// tool-union checks with the generated per-action capability contracts.
//
// Stage order is normative and shared with the native `/mcp` surface (the
// executable specification is `tests/unit/task-27-suite/execute-reference.ts`):
//
//   resolve form + alias -> availability -> params envelope -> reserved and
//   gateway-control keys -> options -> declared defaults -> exact per-action
//   input schema -> connection -> dispatch -> output schema -> receipt
//
// Nothing reaches `handleConsolidatedToolCall` until every earlier stage
// passes, and a result that fails the declared output schema can never be
// returned as a success. `NOT_CONNECTED`, elicitation and `RESULT_TOO_LARGE`
// are TS-local stages the native surface does not share.

import type { Draft202012ObjectSchema } from '../../tools/catalog/capabilities/model.js';
import { isRecord } from '../../utils/validation/type-guards.js';
import { dynamicToolManager } from '../../tools/dynamic/dynamic-tool-manager.js';
import { getString } from './gateway-shared.js';
import { buildNextCall, closestMatches, MAX_SUGGESTIONS } from './gateway-guidance.js';
import { executeTargetIndex, resolveExecuteTarget, type ExecuteTarget } from './gateway-execute-resolve.js';
import {
  applyDeclaredDefaults,
  checkPreviewSupport,
  findControlKeyInParams,
  hasOwn,
  HONORED_EXECUTION_OPTION_KEYS,
  validateAgainstCapabilitySchema,
  validateExecutionOptions,
  VIOLATION_GATEWAY_CODES
} from './gateway-execute-validate.js';
import {
  executeErrorEnvelope,
  refuseWithTarget,
  type ResolvedFailure
} from './gateway-execute-envelope.js';
import { dispatchAndValidate, type GatewayContext } from './gateway-execute-dispatch.js';
import { checkConsentAuthorization, checkPreDispatchPolicy, checkScopeAuthorization } from './gateway-execute-policy.js';
import { ConsentGrantSchema, type ConsentGrant } from '../../tools/catalog/capabilities/semantic/authorization.js';
import { runWithGatewayConsent } from '../../automation/gateway-consent-context.js';
import { runWithGatewayExpectedRevisions } from '../../automation/gateway-expected-revisions-context.js';
import {
  ExpectedRevisionsSchema,
  type ExpectedRevisions
} from '../../tools/catalog/capabilities/semantic/execution-options.js';
import { buildReceiptContext } from './gateway-receipt-context.js';
import {
  conflictMessage,
  IDEMPOTENCY_CONFLICT_CODE,
  LOCAL_PRINCIPAL,
  runWithIdempotency,
  sharedExecuteLedger,
  type ConflictReason
} from './gateway-execute-idempotency.js';
import type { CorrelationId } from '../../tools/catalog/capabilities/semantic/ids.js';

export type { GatewayContext };

function declaredParameterNames(schema: Draft202012ObjectSchema): string[] {
  return isRecord(schema.properties)
    ? Object.keys(schema.properties).filter((name) => name !== 'action').sort()
    : [];
}

function validateInput(target: ExecuteTarget, params: Record<string, unknown>): ResolvedFailure | undefined {
  const record = target.record;
  // Canonical per-action schemas name the action as the capability itself, so
  // the dispatch action is supplied for validation only where it is declared.
  const declaresAction = isRecord(record.schemas.input.properties)
    && 'action' in record.schemas.input.properties;
  const candidate = declaresAction
    ? { ...params, action: record.routing.dispatchAction }
    : params;

  const violation = validateAgainstCapabilitySchema(candidate, record.schemas.input);
  if (violation === undefined) return undefined;

  const declared = declaredParameterNames(record.schemas.input);
  const offending = violation.pointer.split('/').filter((part) => part.length > 0).pop() ?? '';
  const suggestions = closestMatches(offending, declared, MAX_SUGGESTIONS);

  return {
    errorCode: VIOLATION_GATEWAY_CODES[violation.reason],
    message: `${violation.message} for ${record.id}. Call describe before execution.`,
    pointer: violation.pointer,
    ...(violation.reason === 'range' ? { field: violation.pointer } : {}),
    suggestions,
    allowedParameters: declared,
    nextCall: buildNextCall({
      operation: 'describe',
      tool: record.routing.parentTool,
      action: target.legacy.action,
      param: suggestions[0]
    })
  };
}

// The caller's own request minus the control the gateway cannot honor IS the
// call that will run for real, so the refusal hands back something executable
// rather than a description of what to change.
function previewFreeNextCall(
  target: ExecuteTarget,
  params: Record<string, unknown>,
  rawOptions: unknown
): Record<string, unknown> {
  const remaining = isRecord(rawOptions)
    ? Object.entries(rawOptions).filter(([key]) => key !== 'preview')
    : [];
  return {
    ...buildNextCall({
      operation: 'execute',
      tool: target.record.routing.parentTool,
      action: target.legacy.action
    }),
    params,
    ...(remaining.length === 0 ? {} : { options: Object.fromEntries(remaining) })
  };
}

type StaticCheck =
  | { readonly failure: ResolvedFailure }
  | {
    readonly params: Record<string, unknown>;
    readonly expectedRevisions?: ExpectedRevisions;
  };

function checkStaticRequest(target: ExecuteTarget, args: Record<string, unknown>): StaticCheck {
  const record = target.record;
  const refuse = (failure: ResolvedFailure): StaticCheck => ({ failure });

  if (!dynamicToolManager.isToolEnabled(record.routing.parentTool)) {
    return refuse({
      errorCode: 'TOOL_DISABLED',
      message: `Tool '${record.routing.parentTool}' is disabled or unavailable.`,
      suggestions: closestMatches(
        record.routing.parentTool,
        [...executeTargetIndex().parentTools],
        MAX_SUGGESTIONS
      ),
      nextCall: buildNextCall({ operation: 'configure', tool: record.routing.parentTool })
    });
  }

  if (args.params !== undefined && !isRecord(args.params)) {
    return refuse({
      errorCode: 'INVALID_PARAMS',
      message: 'params must be an object.',
      suggestions: declaredParameterNames(record.schemas.input).slice(0, MAX_SUGGESTIONS),
      nextCall: buildNextCall({
        operation: 'describe',
        tool: record.routing.parentTool,
        action: target.legacy.action
      })
    });
  }
  const params = isRecord(args.params) ? args.params : {};

  if (hasOwn(params, 'action') || hasOwn(params, 'subAction')) {
    return refuse({
      errorCode: 'INVALID_PARAMS',
      message: 'params must not override action or subAction. Supply the selected action at the gateway level.'
    });
  }

  const control = findControlKeyInParams(params);
  if (control !== undefined) {
    return refuse({
      errorCode: 'UNSUPPORTED_OPTION',
      option: control,
      message: `Gateway control '${control}' must not appear in action params. Supply it in options.`
    });
  }

  const optionViolation = validateExecutionOptions(args.options)
    ?? checkPreviewSupport(args.options, record.id);
  if (optionViolation !== undefined) {
    return refuse({
      errorCode: optionViolation.errorCode,
      message: optionViolation.message,
      ...(optionViolation.option === undefined
        ? {}
        : { option: optionViolation.option, field: optionViolation.option }),
      ...(optionViolation.pointer === undefined ? {} : { pointer: optionViolation.pointer }),
      ...(optionViolation.errorCode !== 'UNSUPPORTED_PREVIEW'
        ? {}
        : {
          suggestions: closestMatches('preview', [...HONORED_EXECUTION_OPTION_KEYS], MAX_SUGGESTIONS),
          nextCall: previewFreeNextCall(target, params, args.options)
        })
    });
  }

  const expectedRevisions = !isRecord(args.options) || args.options.expectedRevisions === undefined
    ? undefined
    : ExpectedRevisionsSchema.parse(args.options.expectedRevisions);

  const withDefaults = applyDeclaredDefaults(params, record.schemas.input);
  const inputFailure = validateInput(target, withDefaults);
  return inputFailure === undefined
    ? {
      params: withDefaults,
      ...(expectedRevisions === undefined ? {} : { expectedRevisions })
    }
    : { failure: inputFailure };
}


export async function executeGatewayCall(
  args: Record<string, unknown>,
  context: GatewayContext,
  correlationId: CorrelationId
): Promise<Record<string, unknown>> {
  const options = isRecord(args.options) ? args.options : undefined;
  const receiptContext = buildReceiptContext(correlationId, options);
  const index = executeTargetIndex();
  const resolution = resolveExecuteTarget(
    {
      capability: getString(args, 'capability'),
      tool: getString(args, 'tool'),
      action: getString(args, 'action'),
      params: isRecord(args.params) ? args.params : {}
    },
    index
  );

  if (!resolution.ok) {
    const { capabilityId, ...failure } = resolution.failure;
    return executeErrorEnvelope({
      ...failure,
      record: capabilityId === undefined ? undefined : index.byId.get(capabilityId),
      requestedTool: getString(args, 'tool'),
      requestedAction: getString(args, 'action')
    }, receiptContext);
  }

  const target = resolution.target;
  const checked = checkStaticRequest(target, args);
  if ('failure' in checked) return refuseWithTarget(target, checked.failure, receiptContext);

  // Pre-connection policy seam: a Task 39 stale-revision refusal never reaches
  // the connection gate, bridge or queue. Task 40 scope/consent cannot run here
  // — they need the authority descriptor, so they run after ensureConnected().
  const policyFailure = checkPreDispatchPolicy(target, options);
  if (policyFailure !== undefined) return refuseWithTarget(target, policyFailure, receiptContext);

  // Fail-closed default: a configured capability token means the offline admin
  // path is no longer available — the client must complete a handshake so the
  // plugin's authority descriptor is present before any protected/offline action.
  // With no token configured, the loopback offline path is preserved unchanged.
  const tokenConfigured = context.tools.automationBridge?.isCapabilityTokenConfigured?.() ?? false;
  const canRunWithoutConnection = target.record.id === 'system_control.get_project_settings' && !tokenConfigured;
  if (!canRunWithoutConnection && !await context.ensureConnected()) {
    return refuseWithTarget(target, {
      errorCode: 'NOT_CONNECTED',
      message: 'Unreal Engine is not connected.',
      nextCall: buildNextCall({ operation: 'search' })
    }, receiptContext);
  }

  // Authority fail-fast runs after the connection gate (so the plugin's authority
  // descriptor is available) and before dispatch. The plugin re-enforces.
  const authority = context.tools.automationBridge?.getAuthority?.();
  const scopeFailure = checkScopeAuthorization(target, authority);
  if (scopeFailure !== undefined) return refuseWithTarget(target, scopeFailure, receiptContext);

  let consentGrant: ConsentGrant | undefined;
  if (args.consent !== undefined) {
    const parsedConsent = ConsentGrantSchema.safeParse(args.consent);
    if (!parsedConsent.success) {
      return refuseWithTarget(target, {
        errorCode: 'INVALID_CONSENT',
        message: 'consent must be { capability: <exact capability id>, acknowledge: "explicit" | "elevated" }.',
        nextCall: buildNextCall({ operation: 'describe', tool: target.record.routing.parentTool, action: target.legacy.action })
      }, receiptContext);
    }
    consentGrant = parsedConsent.data;
  }
  const consentFailure = checkConsentAuthorization(target, authority, consentGrant);
  if (consentFailure !== undefined) return refuseWithTarget(target, consentFailure, receiptContext);

  const dispatch = (): Promise<Record<string, unknown>> =>
    dispatchAndValidate(target, checked.params, options, context, receiptContext);

  // Dedup sits here, after every refusal stage, so an unauthorized or invalid
  // request can never occupy a slot or be replayed as a recorded success.
  const guarded = (): Promise<Record<string, unknown>> =>
    runWithIdempotency(
      {
        capabilityId: target.record.id,
        principal: authority?.profile ?? LOCAL_PRINCIPAL,
        params: checked.params,
        idempotencyKey: receiptContext.idempotencyId
      },
      sharedExecuteLedger(),
      dispatch,
      (receipt: Record<string, unknown>) => receipt.success === true,
      (reason: ConflictReason) => refuseWithTarget(target, {
        errorCode: IDEMPOTENCY_CONFLICT_CODE,
        message: conflictMessage(reason),
        nextCall: buildNextCall({
          operation: 'describe',
          tool: target.record.routing.parentTool,
          action: target.legacy.action
        })
      }, receiptContext)
    );
  const withConsent = (): Promise<Record<string, unknown>> =>
    consentGrant === undefined ? guarded() : runWithGatewayConsent(consentGrant, guarded);
  return checked.expectedRevisions === undefined
    ? await withConsent()
    : await runWithGatewayExpectedRevisions(checked.expectedRevisions, withConsent);
}
