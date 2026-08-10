import { z } from 'zod';

import { CONSENT_MODES, POLICY_SCOPES } from '../constants.js';
import { CapabilityIdSchema } from '../identifiers.js';

// Task 40 shared, pure authorization predicates. This is the single TypeScript
// definition of "is this principal allowed", so the fail-fast layer classifies a
// refusal exactly as the plugin (the sole security authority) re-enforces it per
// request. It is an EXECUTE-path predicate only: `search` and `describe` are
// deliberately unfiltered, so discovery never depends on the active principal.
//
// The plugin remains the boundary: these predicates are advisory fail-fast only.
// No token or secret is ever an input or output here — a principal is described
// by its already-resolved granted scope set, never by any presented token.

export type PolicyScope = (typeof POLICY_SCOPES)[number]; // 'read' | 'write' | 'destructive' | 'admin'
export type ConsentMode = (typeof CONSENT_MODES)[number]; // 'none' | 'explicit' | 'elevated'

/**
 * Exact-set scope check with an `admin` wildcard. A principal is authorized for a
 * capability's required scope iff it holds `admin` OR the exact required scope is
 * a member of its granted set. This is NOT rank-based: holding `write` does NOT
 * imply `read` or `destructive`. An empty granted set authorizes nothing.
 */
export function isScopeAuthorized(required: PolicyScope, granted: readonly PolicyScope[]): boolean {
  if (granted.includes('admin')) return true;
  return granted.includes(required);
}

/** The per-call consent acknowledgement strength a client may present. */
export type ConsentAcknowledgement = 'explicit' | 'elevated';

/**
 * Whether a capability's consent policy is satisfied by the current call's
 * acknowledgement. `none` always passes; `explicit` is met by `explicit` or the
 * stronger `elevated`; `elevated` requires `elevated`. Consent is never inferred
 * from loopback, prior calls, idempotency or preview: only an explicit,
 * capability-bound acknowledgement on THIS call counts.
 */
export function isConsentSatisfied(
  policy: ConsentMode,
  acknowledgement: ConsentAcknowledgement | undefined
): boolean {
  switch (policy) {
    case 'none':
      return true;
    case 'explicit':
      return acknowledgement === 'explicit' || acknowledgement === 'elevated';
    case 'elevated':
      return acknowledgement === 'elevated';
  }
}

export const ConsentAcknowledgementSchema = z.enum(['explicit', 'elevated']);

/**
 * The strict per-call consent grant a client supplies as the `consent` sibling of
 * the gateway call — never inside `params`, and never as an execution option:
 * `{ capability: <exact id>, acknowledge: 'explicit' | 'elevated' }`. It is bound
 * to one capability and one call; it is never persisted, inherited or reused.
 * `describe` returns the exact grant to pass back as `consentGrant`.
 */
export const ConsentGrantSchema = z
  .strictObject({
    capability: CapabilityIdSchema,
    acknowledge: ConsentAcknowledgementSchema
  })
  .readonly();

export type ConsentGrant = z.infer<typeof ConsentGrantSchema>;
