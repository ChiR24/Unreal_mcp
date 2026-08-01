// src/server/gateway/gateway-capability-view.ts
// Response projections for one canonical capability record.
//
// `search` rows stay compact (no schema bodies) while `describe` returns the
// action-exact input/output schemas straight off the record. Neither projection
// ever reads the parent tool's union schema, so a described action can only
// show the parameters it actually declares.

import { isRecord } from '../../utils/validation/type-guards.js';
import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';
import { capabilityAvailability, isRunnable, type CapabilityAvailability } from './gateway-availability.js';

export type CapabilityNextCall = Record<string, unknown>;

/** The exact `consent` sibling this capability needs, so a grant is discoverable BEFORE the first refusal. */
export function capabilityConsentGrant(
  record: CapabilityRecord
): { capability: string; acknowledge: 'explicit' | 'elevated' } | undefined {
  if (record.policy.consent === 'none') return undefined;
  return {
    capability: record.id,
    acknowledge: record.policy.consent === 'elevated' ? 'elevated' : 'explicit'
  };
}

export function capabilityNextCall(
  record: CapabilityRecord,
  availability: CapabilityAvailability
): CapabilityNextCall {
  if (availability.status === 'disabled') {
    return { operation: 'configure', tool: record.routing.parentTool };
  }
  if (availability.status === 'unavailable') {
    return { operation: 'search', domain: record.discovery.domain };
  }
  // `routing.dispatchAction` is the NATIVE dispatch verb, not an execute
  // address: execute resolves a tool+action pair through `byLegacyPair`, which
  // is built from `record.legacyIds`. Publishing the dispatch action produced a
  // nextCall that either failed UNKNOWN_ACTION or - worse - resolved to a
  // DIFFERENT capability, silently running the wrong action on replay.
  //
  // `capability` is the one selector guaranteed to resolve (`index.byId`), so it
  // leads. The legacy pair is emitted only when the record actually declares
  // one for this parent tool, which by construction is a key in `byLegacyPair`.
  const legacy =
    record.legacyIds.find((entry) => entry.tool === record.routing.parentTool)
    ?? record.legacyIds[0];
  return {
    operation: 'execute',
    capability: record.id,
    ...(legacy === undefined ? {} : { tool: legacy.tool, action: legacy.action }),
    params: {}
  };
}

/**
 * Deliberately NOT `capabilityNextCall`: a search hit points at `describe`,
 * never `execute`, because the caller has not seen the contract yet.
 */
export function searchNextCall(
  record: CapabilityRecord,
  availability: CapabilityAvailability
): CapabilityNextCall {
  if (availability.status === 'available') {
    return { operation: 'describe', capability: record.id };
  }
  return capabilityNextCall(record, availability);
}

// Supplied at the gateway level, never inside `params` - `execute` refuses a
// params object carrying either (gateway-execute.ts: "params must not override
// action or subAction"). The canonical records still declare `action` in their
// input schema because that is the shape the parent tool validates against, so
// the params-facing projection has to drop them here. Publishing them as
// declared - and for 821 records as REQUIRED - made every schema-driven client
// fill in a field that execute then rejected as INVALID_PARAMS.
const GATEWAY_CONTROL_PARAMS: ReadonlySet<string> = new Set(['action', 'subAction']);

/** The input schema as it applies to `params`, without the gateway-level controls. */
export function paramsInputSchema(record: CapabilityRecord): Record<string, unknown> {
  const input = record.schemas.input;
  const properties: Record<string, unknown> = {};
  for (const [name, schema] of Object.entries(input.properties)) {
    if (!GATEWAY_CONTROL_PARAMS.has(name)) properties[name] = schema;
  }
  return {
    ...input,
    properties,
    required: input.required.filter((name) => !GATEWAY_CONTROL_PARAMS.has(name))
  };
}

export function declaredParameterNames(record: CapabilityRecord): readonly string[] {
  return Object.keys(record.schemas.input.properties)
    .filter((name) => !GATEWAY_CONTROL_PARAMS.has(name))
    .sort();
}

function requiredSet(record: CapabilityRecord): ReadonlySet<string> {
  return new Set(record.schemas.input.required.filter((name) => !GATEWAY_CONTROL_PARAMS.has(name)));
}

export function parameterSchema(
  record: CapabilityRecord,
  name: string
): Record<string, unknown> | undefined {
  const schema = record.schemas.input.properties[name];
  return isRecord(schema) ? schema : undefined;
}

export function parameterSummaries(record: CapabilityRecord): Array<Record<string, unknown>> {
  const required = requiredSet(record);
  return declaredParameterNames(record).map((name) => {
    const schema = parameterSchema(record, name) ?? {};
    const summary: Record<string, unknown> = {
      name,
      type: typeof schema.type === 'string' ? schema.type : 'unknown',
      required: required.has(name)
    };
    if (typeof schema.description === 'string') summary.description = schema.description;
    if (Array.isArray(schema.enum)) summary.enum = schema.enum;
    return summary;
  });
}

export function isRequiredParameter(record: CapabilityRecord, name: string): boolean {
  return requiredSet(record).has(name);
}

/** Compact `search` row: identity, routing, discovery, policy and hashes only. */
export function capabilitySearchRow(
  record: CapabilityRecord,
  reasons: readonly unknown[]
): Record<string, unknown> {
  const availability = capabilityAvailability(record);
  return {
    capability: record.id,
    parentTool: record.routing.parentTool,
    action: record.routing.dispatchAction,
    category: record.parent.category,
    domain: record.discovery.domain,
    family: record.discovery.family,
    summary: record.discovery.summary,
    effect: record.behavior.effect,
    availability,
    policy: record.policy,
    outputs: Object.keys(record.schemas.output.properties),
    hashes: record.hashes,
    reasons,
    runnable: isRunnable(availability),
    nextCall: searchNextCall(record, availability)
  };
}

/** Full `describe` contract for one capability, including its exact schemas. */
export function capabilityContract(record: CapabilityRecord): Record<string, unknown> {
  const availability = capabilityAvailability(record);
  const parameters = parameterSummaries(record);
  const consentGrant = capabilityConsentGrant(record);
  return {
    ...(consentGrant === undefined ? {} : { consentGrant }),
    scope: 'capability',
    capability: record.id,
    parentTool: record.routing.parentTool,
    action: record.routing.dispatchAction,
    dispatchMode: record.routing.dispatchMode,
    category: record.parent.category,
    domain: record.discovery.domain,
    family: record.discovery.family,
    topics: record.discovery.topics,
    summary: record.discovery.summary,
    whenToUse: record.discovery.whenToUse,
    whenNotToUse: record.discovery.whenNotToUse,
    aliases: record.aliases,
    legacyIds: record.legacyIds,
    perActionSchemas: true,
    inputSchema: paramsInputSchema(record),
    outputSchema: record.schemas.output,
    parameters,
    parameterCount: parameters.length,
    availability,
    behavior: record.behavior,
    policy: record.policy,
    cost: record.cost,
    deprecation: record.deprecation,
    hashes: record.hashes,
    runnable: isRunnable(availability),
    nextCall: capabilityNextCall(record, availability)
  };
}
