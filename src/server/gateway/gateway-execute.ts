// src/server/gateway/gateway-execute.ts
// The gateway `execute` operation: canonical validation, one dispatch, one receipt.
//
// Task 24 extracted this seam unchanged; Task 26 replaced its manifest-driven
// tool-union checks with the generated per-action capability contracts.
//
// Stage order is normative and shared with the native `/mcp` surface (the
// executable specification is `tests/unit/gateway-discovery-suite/execute-reference.ts`):
//
//   resolve form + alias -> availability -> params envelope -> reserved and
//   gateway-control keys -> options -> declared defaults -> exact per-action
//   input schema -> connection -> dispatch -> output schema -> receipt
//
// Nothing reaches `handleConsolidatedToolCall` until every earlier stage
// passes, and a result that fails the declared output schema can never be
// returned as a success. `NOT_CONNECTED`, elicitation and `RESULT_TOO_LARGE`
// are TS-local stages the native surface does not share.

import { isRecord } from '../../utils/validation/type-guards.js';
import { getString } from './gateway-shared.js';
import { buildNextCall } from './gateway-guidance.js';
import { executeTargetIndex, resolveExecuteTarget } from './gateway-execute-resolve.js';
import { resolveDispatchAction } from './gateway-dispatch-by.js';
import { checkStaticRequest } from './gateway-execute-static-check.js';
import { executeErrorEnvelope, refuseWithTarget } from './gateway-execute-envelope.js';
import { dispatchAndValidate, type GatewayContext } from './gateway-execute-dispatch.js';
import { checkConsentAuthorization, checkPreDispatchPolicy, checkScopeAuthorization, matchedFoldedGrant } from './gateway-execute-policy.js';
import { ConsentGrantSchema, type ConsentGrant } from '../../tools/catalog/capabilities/semantic/authorization.js';
import { runWithGatewayConsent, runWithGatewayExpectedRevisions, runWithGatewayTimeout } from '../../automation/gateway-contexts.js';
import { buildReceiptContext } from './gateway-receipt-context.js';
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

// Keyed on the ACTION, not one hard-coded capability id. Two records dispatch
// `get_project_settings` (system_control.* and inspect.*, the one
// workflow-prompts.ts:89 tells clients to call), and pinning the literal id
// refused the documented path with NOT_CONNECTED while its twin succeeded —
// for a reason nothing in either record explained. Restricted to read effects
// at the use site so this can never widen into a mutation running without a
// connection. Matched on the capability id's ACTION SEGMENT, which is
// `get_project_settings` for both records. Deliberately not
// `routing.dispatchAction`: that is the native dispatch verb and is
// `system_control` for the system_control record, so keying on it would have
// broken the very path this gate was written for.
const OFFLINE_READABLE_ACTIONS: ReadonlySet<string> = new Set(['get_project_settings']);

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
  // The check resolves the EFFECTIVE token (explicit option, env, or token
  // file), so a file-backed token closes the offline path too.
  // The token probe is evaluated LAST and only for an offline-eligible read:
  // isCapabilityTokenConfigured() re-reads the capability-token file from disk
  // on every call by design, and its answer can only change the outcome when
  // the action is already one of the offline-readable reads. Checking it first
  // put a file read on every execute for a result all but one action discards.
  const actionSegment = target.record.id.slice(target.record.id.indexOf('.') + 1);
  const offlineEligible =
    OFFLINE_READABLE_ACTIONS.has(actionSegment)
    && target.record.behavior.effect === 'read';
  const canRunWithoutConnection =
    offlineEligible
    && !((await context.tools.automationBridge?.isCapabilityTokenConfigured?.()) ?? false);
  if (!canRunWithoutConnection && !await context.ensureConnected()) {
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
      }, receiptContext),
      (recorded: Record<string, unknown>) => markReplayed(recorded, receiptContext.correlationId)
    );
  const withConsent = (): Promise<Record<string, unknown>> =>
    consentGrant === undefined ? guarded() : runWithGatewayConsent(consentGrant, guarded);
  const withRevisions = (): Promise<Record<string, unknown>> =>
    checked.expectedRevisions === undefined
      ? withConsent()
      : runWithGatewayExpectedRevisions(checked.expectedRevisions, withConsent);
  // Outermost, so the deadline covers the ledger claim and the dispatch it
  // guards; `executeAutomationRequest` reads it back at the bridge boundary.
  return checked.timeoutMs === undefined
    ? await withRevisions()
    : await runWithGatewayTimeout(checked.timeoutMs, withRevisions);
}
