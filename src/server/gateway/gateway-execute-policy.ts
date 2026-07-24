// src/server/gateway/gateway-execute-policy.ts
// Hand-authored pre-dispatch policy seam: it runs after a request has resolved
// to exactly one capability and after static validation, but BEFORE the
// connection gate and any dispatch, so a blocked call never reaches the bridge,
// the subsystem queue, or editor work.
//
// Task 39 implements the stale catalog-revision precondition here. Task 40 adds
// scope/consent predicates through the same seam without touching the envelope
// or dispatch files; each predicate returns a typed ResolvedFailure or
// undefined, and the first failure short-circuits.

import { catalogRevision } from './gateway-capability-index.js';
import { buildNextCall } from './gateway-guidance.js';
import type { ResolvedFailure } from './gateway-execute-envelope.js';
import type { ExecuteTarget } from './gateway-execute-resolve.js';
import { CatalogRevisionSchema } from '../../tools/catalog/capabilities/semantic/ids.js';

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
