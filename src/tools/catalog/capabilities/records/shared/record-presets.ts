// src/tools/catalog/capabilities/records/shared/record-presets.ts
// The effect-derived presets every domain record builder shares.
//
// `policy` was written out byte-for-byte in all six domain builders (core,
// world, gameplay, build-environment, manage-blueprint, manage-sequence) and
// `behavior` in five of them. They encode a POLICY decision — which effect class
// demands consent, what a write may assume about retry and undo — so six copies
// meant a consent or retry change had to be applied six times, and a missed copy
// would silently give one domain a different security contract from the rest.
//
// Only the presets that were already identical live here. `routing`, `schema`
// and the availability blocks legitimately differ per domain (different
// dispatch modes, different schema shapes) and stay with their builders.

import type { CapabilityBehaviorSource, CapabilityPolicy } from '../../index.js';

/** The effect classes a capability record may declare. */
export type EffectType = 'read' | 'write' | 'destructive';

/**
 * Scope, consent and data access derived from the effect class.
 * Only `destructive` demands an explicit consent grant.
 */
export function policy(effect: EffectType): CapabilityPolicy {
  return {
    requiredScope: effect,
    consent: effect === 'destructive' ? 'explicit' : 'none',
    dataAccess: effect === 'read' ? 'project-read' : 'project-write',
  };
}

/**
 * Retry/undo/preview defaults derived from the effect class, with per-record
 * overrides. A read is idempotent and safe to retry; a write is neither, and
 * gains undo support.
 */
export function behavior(
  effect: EffectType,
  opts: Partial<CapabilityBehaviorSource> = {}
): CapabilityBehaviorSource {
  const isWrite = effect !== 'read';
  return {
    effect,
    idempotency: opts.idempotency ?? (effect === 'read' ? 'idempotent' : 'non-idempotent'),
    longRunning: opts.longRunning ?? false,
    safeToRetry: opts.safeToRetry ?? effect === 'read',
    supportsPreview: opts.supportsPreview ?? false,
    supportsUndo: opts.supportsUndo ?? isWrite,
  };
}
