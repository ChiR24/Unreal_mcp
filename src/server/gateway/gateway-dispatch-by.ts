// src/server/gateway/gateway-dispatch-by.ts
// A folded record stands for a family of handler actions that differ only by
// one selector value. Two data fields on the record drive the fold at execute
// time, and nothing else does:
//   routing.dispatchBy    selector value -> bridge action, for a call that names
//                         the primary operation;
//   legacyIds[n].folded   the selector values an old name implied, for a call
//                         that still names the old action (or its alias id).
// Both doors apply the same two steps in the same order: the pins are injected
// before schema validation, the action is chosen after it. The native mirror
// is McpNativeGatewayFolding.cpp.

import type { CapabilityRecord, LegacyCapabilityId } from '../../tools/catalog/capabilities/model.js';
import { hasOwn } from '../../utils/validation/type-guards.js';
import type { ExecuteTarget } from './gateway-execute-resolve.js';

/** The action the caller actually named: the legacy pair, or the alias id's action segment. */
export function requestedAction(target: ExecuteTarget): string | undefined {
  if (target.migratedFrom !== undefined) return target.migratedFrom.action;
  if (target.resolvedFromAlias !== undefined) {
    const alias = target.resolvedFromAlias;
    return alias.slice(alias.lastIndexOf('.') + 1);
  }
  return undefined;
}

export function foldedPairFor(
  record: CapabilityRecord,
  action: string | undefined
): LegacyCapabilityId | undefined {
  if (action === undefined) return undefined;
  return record.legacyIds.find((entry) => entry.action === action && entry.folded !== undefined);
}

/**
 * Inject the selector values a folded family pins for an old name. Each pinned
 * selector is one of exactly three cases:
 *   - the caller omitted it           -> inject the pinned value;
 *   - the caller sent the same value  -> keep the caller's value;
 *   - the caller sent a different one -> return undefined (refuse the call).
 * The action the caller named decides dispatch, so an omitted selector is
 * filled in, but a caller-supplied value is never overridden. A legacy caller
 * never sent the selector pre-fold, so a disagreement is a contradictory
 * request, not a legacy call, and must be refused rather than silently
 * dispatched.
 */
export function applyFoldedPins(
  target: ExecuteTarget,
  params: Record<string, unknown>
): Record<string, unknown> | undefined {
  const folded = foldedPairFor(target.record, requestedAction(target));
  if (folded?.folded === undefined) return params;
  const pinned: Record<string, unknown> = { ...params };
  for (const [name, value] of Object.entries(folded.folded)) {
    if (hasOwn(pinned, name)) {
      if (pinned[name] !== value) return undefined;
      continue;
    }
    pinned[name] = value;
  }
  return pinned;
}

/**
 * A call that omits the selector runs the variant its parameters point to:
 * every sent parameter the variants do not all share must be declared by one
 * and the same variant. A small model that sent `operations` without
 * `nodeKind: "batch"` got the default variant, which ignored the steps and
 * failed on a field the batch never needed. Parameters pointing at different
 * variants select nothing, so the default still runs as before.
 */
export function inferSelector(
  record: CapabilityRecord,
  params: Record<string, unknown>
): Record<string, unknown> {
  const dispatchBy = record.routing.dispatchBy;
  if (dispatchBy?.declaredBy === undefined || hasOwn(params, dispatchBy.param)) return params;
  let candidates: readonly string[] | undefined;
  for (const name of Object.keys(params)) {
    const owners = hasOwn(dispatchBy.declaredBy, name) ? dispatchBy.declaredBy[name] : undefined;
    if (owners === undefined) continue;
    candidates = candidates === undefined ? owners : candidates.filter((value) => owners.includes(value));
  }
  return candidates?.length === 1 ? { ...params, [dispatchBy.param]: candidates[0] } : params;
}

/**
 * The bridge action to dispatch once params are validated. An old name
 * dispatches itself, so a folded family never changes what the handlers see;
 * the primary operation maps its selector through routing.dispatchBy. The
 * fallback is unreachable for a well-formed record (validation enforces the
 * selector's enum, which the map keys equal), so a selector value that does
 * not map FAILS CLOSED rather than silently dispatching the primary.
 */
export function resolveDispatchAction(
  target: ExecuteTarget,
  params: Record<string, unknown>
): string | undefined {
  const folded = foldedPairFor(target.record, requestedAction(target));
  if (folded !== undefined) return folded.action;
  const dispatchBy = target.record.routing.dispatchBy;
  if (dispatchBy === undefined) return target.legacy.action;
  const value = params[dispatchBy.param];
  const mapped = typeof value === 'string' && hasOwn(dispatchBy.actions, value)
    ? dispatchBy.actions[value]
    : undefined;
  return mapped;
}
