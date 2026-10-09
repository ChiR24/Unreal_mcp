// src/server/gateway/gateway-execute.ts
// The gateway `execute` operation: canonical validation, one dispatch, one receipt.
//
// Extracted this seam unchanged; Task 26 replaced its manifest-driven
// tool-union checks with the generated per-action capability contracts.
//
// Stage order is normative and shared with the native `/mcp` surface:
//
//   resolve form + alias -> availability -> params envelope -> reserved and
//   gateway-control keys -> options -> declared defaults -> exact per-action
//   input schema -> connection -> dispatch -> output schema -> receipt
//
// Nothing reaches the bridge until every earlier stage passes, and a result
// that fails the declared output schema can never be returned as a success.
// `NOT_CONNECTED` and `RESULT_TOO_LARGE` are TS-local stages the native
// surface does not share.

import { isRecord } from '../../utils/validation/type-guards.js';
import { getString } from './gateway-shared.js';
import { buildNextCall } from './gateway-guidance.js';
import { resolveExecuteTarget } from './gateway-execute-resolve.js';
import { capabilityIndex } from './gateway-capability-index.js';
import { resolveDispatchAction } from './gateway-dispatch-by.js';
import { checkStaticRequest } from './gateway-execute-static-check.js';
import { executeErrorEnvelope, executeSuccessEnvelope, refuseWithTarget } from './gateway-execute-envelope.js';
import { dispatchAndValidate, type GatewayContext } from './gateway-execute-dispatch.js';
import { checkConsentAuthorization, checkExpectedCatalogRevision, checkScopeAuthorization, matchedFoldedGrant } from './gateway-execute-policy.js';
import { ConsentGrantSchema, type ConsentGrant } from '../../tools/catalog/capabilities/semantic/authorization.js';
import { buildReceiptContext, type GatewayReceiptContext } from './gateway-receipt-context.js';
import { noteTaskDone, noteTaskRunning } from './gateway-task-results.js';
import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';
import {
  conflictMessage,
  IDEMPOTENCY_CONFLICT_CODE,
  LOCAL_PRINCIPAL,
  markReplayed,
  runWithIdempotency,
  sharedExecuteLedger,
  type ConflictReason
} from './gateway-execute-idempotency.js';
import type { CorrelationId } from '../../tools/catalog/capabilities/semantic/ids.js';

export type { GatewayContext };

export async function executeGatewayCall(
  args: Record<string, unknown>,
  context: GatewayContext,
  correlationId: CorrelationId
): Promise<Record<string, unknown>> {
  const options = isRecord(args.options) ? args.options : undefined;
  const receiptContext = buildReceiptContext(correlationId, options);
  const index = capabilityIndex();
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

  // Pre-connection policy seam: a stale-revision refusal never reaches
  // the connection gate, bridge or queue. Task 40 scope/consent cannot run here
  // — they need the authority descriptor, so they run after ensureConnected().
  const policyFailure = checkExpectedCatalogRevision(options);
  if (policyFailure !== undefined) return refuseWithTarget(target, policyFailure, receiptContext);

  if (!await context.ensureConnected()) {
    // Name the target the server actually dialed: with the editor closed, or
    // another process holding the port, "not connected" alone leaves the caller
    // guessing. The errorCode is unchanged so existing callers keep branching
    // on NOT_CONNECTED.
    const bridgeTarget = context.tools.automationBridge?.getClientUrl?.();
    return refuseWithTarget(target, {
      errorCode: 'NOT_CONNECTED',
      message: bridgeTarget
        ? `Unreal Engine is not connected: no bridge listener responded at ${bridgeTarget}.`
        : 'Unreal Engine is not connected.',
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

  // A grant naming a folded old pair authorized that pair's operation; the
  // resolved dispatch target must agree with it, or a grant for one sibling
  // was used to run another of the same family. Scoped to consent-bearing
  // policies: a policy-`none` capability needs no grant, so one the caller
  // happened to send must not refuse the call.
  if (consentGrant !== undefined && target.record.policy.consent !== 'none') {
    const granted = matchedFoldedGrant(consentGrant.capability, target);
    const dispatchAction = resolveDispatchAction(target, checked.params);
    if (granted !== undefined && dispatchAction !== undefined && String(granted.action) !== dispatchAction) {
      return refuseWithTarget(target, {
        errorCode: 'CONSENT_REQUIRED',
        message: `The consent grant names '${String(granted.tool)}.${String(granted.action)}', which authorizes that operation only; this call dispatches '${dispatchAction}'. Re-run with consent naming the capability id '${target.record.id}' to authorize the family.`,
        requiredScope: target.record.policy.requiredScope,
        nextCall: buildNextCall({
          operation: 'describe',
          tool: target.record.routing.parentTool,
          action: target.legacy.action
        })
      }, receiptContext);
    }
  }

  const dispatch = async (): Promise<Record<string, unknown>> => {
    const receipt = await dispatchAndValidate(target, checked.params, options, context, receiptContext, {
      correlationId,
      consent: consentGrant,
      expectedRevisions: checked.expectedRevisions,
      timeoutMs: checked.timeoutMs
    }, checked.unread);
    // A plugin from another release can lack the action or read its params
    // differently, so every failed dispatch names the mismatch until the pair agrees.
    const versionMismatch = receipt.success === true ? undefined : context.tools.automationBridge?.getVersionMismatch?.();
    return versionMismatch === undefined ? receipt : { ...receipt, versionMismatch };
  };

  // Dedup sits here, after every refusal stage, so an unauthorized or invalid
  // request can never occupy a slot or be replayed as a recorded success.
  const settled = runWithIdempotency(
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
      }, receiptContext),
      (recorded: Record<string, unknown>) => markReplayed(recorded, receiptContext.correlationId)
    );
  // An explicit timeoutMs is the caller saying how long it will wait; any other call answers "still running"
  // past STILL_RUNNING_AFTER_MS, and its result is read back with manage_tools get_task_result.
  if (checked.timeoutMs !== undefined) return await settled;
  return await answerWhileRunning(settled, target.record, receiptContext, context);
}

/** When a call that has not finished answers for itself, as the native transport's keepalive does. */
export const STILL_RUNNING_AFTER_MS = 27_000;

// A client gives up on a call at its own timeout (often 30 s) while Unreal keeps
// working, and the result was lost. Past 27 s a call answers with a success
// receipt whose task is still running: the work goes on, manage_tools
// get_task_result reads its result once it settles, and a keyed call's
// idempotency slot stays claimed until then, so the same call with the same
// idempotencyKey replays the real result.
async function answerWhileRunning(
  settled: Promise<Record<string, unknown>>,
  record: CapabilityRecord,
  receiptContext: GatewayReceiptContext,
  context: GatewayContext
): Promise<Record<string, unknown>> {
  let timer: ReturnType<typeof setTimeout> | undefined;
  const running = new Promise<undefined>((resolve) => {
    timer = setTimeout(resolve, STILL_RUNNING_AFTER_MS, undefined);
  });
  const first = await Promise.race([settled, running]).finally(() => clearTimeout(timer));
  if (first !== undefined) return first;
  const taskId = receiptContext.correlationId;
  noteTaskRunning(taskId);
  settled.then(
    (receipt) => {
      noteTaskDone(taskId, receipt);
      context.logger.info(`${record.id} finished after its still-running answer (success=${String(receipt.success)}).`);
    },
    (error: unknown) => {
      const text = error instanceof Error ? error.message : String(error);
      noteTaskDone(taskId, { success: false, errorCode: 'EXECUTION_ERROR', message: text });
      context.logger.warn(`${record.id} failed after its still-running answer: ${text}`);
    }
  );
  const again = receiptContext.idempotencyId === undefined
    ? 'do not send this call again: it would run twice.'
    : 'the same call with the same idempotencyKey also answers with it instead of running again.';
  const message = `Still running after ${STILL_RUNNING_AFTER_MS / 1000} s. Unreal keeps going; this answer does not stop it. `
    + `Read its result with manage_tools get_task_result taskId ${taskId} once it is done; ${again}`;
  return executeSuccessEnvelope({
    record,
    result: { success: true, message, task: { taskId, state: 'running' } },
    canonicalOutput: { message },
    warnings: []
  }, receiptContext);
}
