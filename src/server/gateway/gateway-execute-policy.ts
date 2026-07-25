// src/server/gateway/gateway-execute-policy.ts
// Hand-authored pre-dispatch policy seam: it runs after a request has resolved
// to exactly one capability and after static validation, but BEFORE the
// connection gate and any dispatch, so a blocked call never reaches the bridge,
// the subsystem queue, or editor work.
//
// Task 39 implements the stale catalog-revision precondition here, and it is the
// only predicate on the pre-connection seam. Task 40's scope/consent predicates
// also live in this file but are called LATER, after ensureConnected(), because
// they read the plugin's bridge_ack authority descriptor, which does not exist
// until the handshake completes. Each predicate returns a typed ResolvedFailure
// or undefined, and the first failure short-circuits before any dispatch.

import { catalogRevision } from './gateway-capability-index.js';
import { buildNextCall } from './gateway-guidance.js';
import type { ResolvedFailure } from './gateway-execute-envelope.js';
import type { ExecuteTarget } from './gateway-execute-resolve.js';
import { CatalogRevisionSchema } from '../../tools/catalog/capabilities/semantic/ids.js';
import { isConsentSatisfied, isScopeAuthorized, type ConsentGrant, type PolicyScope } from '../../tools/catalog/capabilities/semantic/authorization.js';
import { POLICY_SCOPES } from '../../tools/catalog/capabilities/constants.js';
import type { BridgeAuthority } from '../../automation/message-schema.js';

// The descriptor is wire data: it carries arbitrary strings. Narrowing by
// membership (rather than asserting) makes an unrecognised scope name grant
// nothing, so a future or malformed scope can never widen this principal.
function knownScopes(advertised: readonly string[]): readonly PolicyScope[] {
  const known: readonly string[] = POLICY_SCOPES;
  return advertised.filter((scope): scope is PolicyScope => known.includes(scope));
}

// The client may pin the catalog revision it planned against via
// `options.expectedCatalogRevision`. A present-but-malformed pin (empty, non
// string, non-hex, or over-length) fails closed as a validation error before the
// stale comparison, mirroring the native surface, so a malformed pin is never
// coerced into a stale-state refusal and never dispatched. A well-formed pin that
// no longer matches the live digest is refused as stale with the current
// reference; only a well-formed pin equal to the live digest proceeds.
function checkExpectedCatalogRevision(
  options: Record<string, unknown> | undefined
): ResolvedFailure | undefined {
  const expected = options?.expectedCatalogRevision;
  if (expected === undefined) return undefined;

  const parsed = CatalogRevisionSchema.safeParse(expected);
  if (!parsed.success) {
    return {
      errorCode: 'INVALID_OPTIONS',
      message: 'options.expectedCatalogRevision must be a lowercase hex catalog-revision digest of 1..64 characters.',
      pointer: '/options/expectedCatalogRevision'
    };
  }

  const current = catalogRevision();
  if (parsed.data === current) return undefined;

  return {
    errorCode: 'STALE_STATE',
    message: `The capability catalog revision changed since it was read (expected '${parsed.data}', current '${current}'). Re-run search or describe and retry.`,
    currentRevision: current,
    expectedRevision: parsed.data,
    nextCall: buildNextCall({ operation: 'search' })
  };
}

export function checkPreDispatchPolicy(
  _target: ExecuteTarget,
  options: Record<string, unknown> | undefined
): ResolvedFailure | undefined {
  return checkExpectedCatalogRevision(options);
}

// Task 40 scope fail-fast. Runs AFTER the connection gate so the plugin's
// bridge_ack authority descriptor is available, and BEFORE dispatch so a refusal
// never reaches the bridge, queue or editor. An ABSENT descriptor means admin
// authority (no-token loopback, legacy token, or an old plugin predating scoped
// authority): the plugin stays the boundary and re-enforces. An EMPTY scope set
// is not absent — it advertises "no scopes" and correctly refuses everything.
// When scopes ARE advertised, this classifies exactly as the plugin will —
// admin wildcard OR exact required-scope membership.
export function checkScopeAuthorization(
  target: ExecuteTarget,
  authority: BridgeAuthority | undefined
): ResolvedFailure | undefined {
  const grantedScopes = authority?.scopes;
  if (grantedScopes === undefined) return undefined;

  const requiredScope = target.record.policy.requiredScope;
  if (isScopeAuthorized(requiredScope, knownScopes(grantedScopes))) {
    return undefined;
  }

  return {
    errorCode: 'SCOPE_NOT_GRANTED',
    message: `This action requires the '${requiredScope}' scope, which the active principal does not hold. Retry with a principal granted '${requiredScope}' (or admin).`,
    requiredScope,
    grantedScopes,
    nextCall: buildNextCall({
      operation: 'describe',
      tool: target.record.routing.parentTool,
      action: target.legacy.action
    })
  };
}

// Task 40 consent fail-fast, gated identically to scope: it only refuses when the
// plugin advertises scoped authority, so no-token loopback / legacy / old-plugin
// behaviour is unchanged and the plugin remains the boundary that re-validates
// consent per record policy on every request. Consent is capability-bound and
// current-call only: a grant naming a different capability, or no grant, never
// satisfies a non-`none` policy — it is never inferred from loopback, prior
// calls, idempotency or preview.
export function checkConsentAuthorization(
  target: ExecuteTarget,
  authority: BridgeAuthority | undefined,
  consent: ConsentGrant | undefined
): ResolvedFailure | undefined {
  if (authority?.scopes === undefined) return undefined;

  const policy = target.record.policy.consent;
  if (policy === 'none') return undefined;

  const acknowledgement = consent !== undefined && consent.capability === target.record.id
    ? consent.acknowledge
    : undefined;
  if (isConsentSatisfied(policy, acknowledgement)) return undefined;

  const acknowledge = policy === 'elevated' ? 'elevated' : 'explicit';
  return {
    errorCode: 'CONSENT_REQUIRED',
    message: `This action requires ${policy} consent. Re-run with consent: { capability: '${target.record.id}', acknowledge: '${acknowledge}' }.`,
    requiredScope: target.record.policy.requiredScope,
    nextCall: buildNextCall({
      operation: 'describe',
      tool: target.record.routing.parentTool,
      action: target.legacy.action
    })
  };
}
