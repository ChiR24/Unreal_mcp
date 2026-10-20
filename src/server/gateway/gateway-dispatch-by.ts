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
import type { ExecuteTarget } from './gateway-execute-resolve.js';
import type { SchemaViolation } from './gateway-schema-validate.js';

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
 *   - the caller sent a different one -> return the conflict (refuse the call).
 * The action the caller named decides dispatch, so an omitted selector is
 * filled in, but a caller-supplied value is never overridden. A legacy caller
 * never sent the selector pre-fold, so a disagreement is a contradictory
 * request, not a legacy call, and must be refused rather than silently
 * dispatched.
 */
export function applyFoldedPins(
  target: ExecuteTarget,
  params: Record<string, unknown>
): { params: Record<string, unknown> } | { conflict: { name: string; pinned: unknown; sent: unknown } } {
  const folded = foldedPairFor(target.record, requestedAction(target))?.folded;
  if (folded === undefined) return { params };
  const pinned: Record<string, unknown> = { ...params };
  for (const [name, value] of Object.entries(folded)) {
    if (!Object.hasOwn(pinned, name)) pinned[name] = value;
    else if (pinned[name] !== value) return { conflict: { name, pinned: value, sent: pinned[name] } };
  }
  return { params: pinned };
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
  if (dispatchBy?.declaredBy === undefined || Object.hasOwn(params, dispatchBy.param)) return params;
  let candidates: readonly string[] | undefined;
  for (const name of Object.keys(params)) {
    const owners = Object.hasOwn(dispatchBy.declaredBy, name) ? dispatchBy.declaredBy[name] : undefined;
    if (owners === undefined) continue;
    candidates = candidates === undefined ? owners : candidates.filter((value) => owners.includes(value));
  }
  return candidates?.length === 1 ? { ...params, [dispatchBy.param]: candidates[0] } : params;
}

/**
 * The parameters a call sent that only other variants of its folded family read. The family
 * declares them, so the call is accepted, and the variant that ran ignored them without a word:
 * inspect_graph info=node with pinName answered every pin. The receipt names each one.
 * `sent` is the caller's own parameter names (before declared defaults fill any in).
 */
export function unreadVariantParams(
  record: CapabilityRecord,
  sent: readonly string[],
  params: Record<string, unknown>
): readonly string[] {
  const dispatchBy = record.routing.dispatchBy;
  const selected = dispatchBy === undefined ? undefined : params[dispatchBy.param];
  if (dispatchBy?.declaredBy === undefined || typeof selected !== 'string') return [];
  const declaredBy = dispatchBy.declaredBy;
  return sent.flatMap((name) => {
    const owners = Object.hasOwn(declaredBy, name) ? declaredBy[name] : undefined;
    return owners === undefined || owners.includes(selected)
      ? []
      : [`${name} is read only when ${dispatchBy.param} is ${owners.join(' or ')}; this ${dispatchBy.param}=${selected} call did not use it.`];
  });
}

/**
 * What the dispatched variant's own record requires beyond the union schema (mirror of
 * McpCheckVariantRequirements in McpFoldVariantRules.cpp). The union keeps only what every variant
 * requires, so debugOp=draw without location reached the engine and failed there.
 */
export function missingVariantRequirement(
  record: CapabilityRecord,
  action: string,
  params: Record<string, unknown>
): SchemaViolation | undefined {
  const dispatchBy = record.routing.dispatchBy;
  const requirement = dispatchBy?.requires !== undefined && Object.hasOwn(dispatchBy.requires, action)
    ? dispatchBy.requires[action]
    : undefined;
  if (dispatchBy === undefined || requirement === undefined) return undefined;
  const value = params[dispatchBy.param];
  const when = typeof value === 'string' ? `when ${dispatchBy.param} is ${value}` : `when action is ${action}`;
  const missing = (requirement.required ?? []).filter((name) => !Object.hasOwn(params, name));
  if (missing.length > 0) {
    return {
      reason: 'missing-required',
      pointer: `/${missing[0]}`,
      message: missing.length === 1
        ? `Missing required parameter '${missing[0]}' ${when}`
        : `Missing required parameters ${when}: ${missing.join(', ')}`
    };
  }
  const group = requirement.oneOf ?? [];
  return group.length > 0 && !group.some((name) => Object.hasOwn(params, name))
    ? { reason: 'required-one-of', pointer: '/requiredOneOf', message: `At least one of [${group.join(', ')}] must be provided ${when}` }
    : undefined;
}

/** Selector value -> what its variant requires beyond the union schema; a one-of group reads 'a|b'. */
export function variantRequired(record: CapabilityRecord): Record<string, string[]> | undefined {
  const dispatchBy = record.routing.dispatchBy;
  if (dispatchBy?.requires === undefined) return undefined;
  const out: Record<string, string[]> = {};
  for (const [value, action] of Object.entries(dispatchBy.actions)) {
    const requirement = Object.hasOwn(dispatchBy.requires, action) ? dispatchBy.requires[action] : undefined;
    if (requirement === undefined) continue;
    // A one-name group the variant also requires (spawn_blueprint's blueprintPath) is listed once.
    out[value] = [...new Set([...(requirement.required ?? []), ...(requirement.oneOf === undefined ? [] : [requirement.oneOf.join('|')])])];
  }
  return Object.keys(out).length === 0 ? undefined : out;
}

/** The selector values whose variant requires this parameter though the union schema does not. */
export function variantsRequiring(record: CapabilityRecord, name: string): string[] | undefined {
  const required = variantRequired(record);
  const values = required === undefined ? [] : Object.keys(required).filter((value) => required[value]?.includes(name));
  return values.length === 0 ? undefined : values;
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
  const mapped = typeof value === 'string' && Object.hasOwn(dispatchBy.actions, value)
    ? dispatchBy.actions[value]
    : undefined;
  return mapped;
}
