// src/server/gateway/gateway-describe-capability.ts
// The two exact levels of describe: one capability's full contract, and one
// declared parameter of it.
//
// Both levels are sourced from the capability record, so the parameter set is
// exactly what that action declares. A name that exists only on the parent
// tool's union schema is reported as unknown here — surfacing it would tell the
// caller to send a parameter the action does not accept.

import type { CapabilityRecord } from '../../tools/catalog/capabilities/model.js';
import { catalogRevision, resolveCapability, allCapabilityIds } from './gateway-capability-index.js';
import {
  capabilityContract,
  declaredParameterNames,
  isRequiredParameter,
  parameterSchema,
  primaryExecutableAction
} from './gateway-capability-view.js';
import { closestMatches, MAX_SUGGESTIONS } from './gateway-guidance.js';
import { gatewayError } from './gateway-shared.js';

export function unknownCapabilityError(reference: string): Record<string, unknown> {
  const suggestions = closestMatches(reference, [...allCapabilityIds()], MAX_SUGGESTIONS);
  const nextCall = suggestions.length > 0
    ? { operation: 'describe', capability: suggestions[0] }
    : { operation: 'search', query: reference };
  return {
    ...gatewayError('describe', 'UNKNOWN_CAPABILITY', `Unknown capability '${reference}'. Call search to find canonical capability IDs.`),
    suggestions,
    nextCall
  };
}

function unknownParamError(record: CapabilityRecord, param: string): Record<string, unknown> {
  const declared = declaredParameterNames(record);
  const suggestions = closestMatches(param, [...declared], MAX_SUGGESTIONS);
  const nextCall: Record<string, unknown> = { operation: 'describe', capability: record.id };
  if (suggestions.length > 0) nextCall.param = suggestions[0];
  return {
    ...gatewayError('describe', 'UNKNOWN_PARAM', `Unknown parameter '${param}' for capability '${record.id}'.`),
    capability: record.id,
    availableParameters: declared,
    suggestions,
    nextCall
  };
}

export function describeCapabilityRecord(
  record: CapabilityRecord,
  origin: Record<string, unknown>
): Record<string, unknown> {
  return {
    success: true,
    operation: 'describe',
    catalogRevision: catalogRevision(),
    ...capabilityContract(record),
    ...origin,
    message: 'Exact capability contract. inputSchema declares every parameter this action accepts.'
  };
}

export function describeCapabilityParameter(
  record: CapabilityRecord,
  param: string,
  origin: Record<string, unknown>
): Record<string, unknown> {
  if (!declaredParameterNames(record).includes(param)) return unknownParamError(record, param);
  return {
    success: true,
    operation: 'describe',
    catalogRevision: catalogRevision(),
    scope: 'parameter',
    capability: record.id,
    parentTool: record.routing.parentTool,
    action: primaryExecutableAction(record),
    param,
    required: isRequiredParameter(record, param),
    schema: parameterSchema(record, param) ?? {},
    perActionSchemas: true,
    ...origin,
    message: 'Single parameter schema for this capability. Pass it with the exact casing shown.'
  };
}

export function describeCapabilityReference(
  reference: string,
  param: string | undefined
): Record<string, unknown> {
  const resolved = resolveCapability(reference);
  if (resolved.kind === 'unknown') return unknownCapabilityError(reference);
  const origin: Record<string, unknown> = resolved.kind === 'alias'
    ? { resolvedFromAlias: resolved.alias }
    : {};
  return param === undefined
    ? describeCapabilityRecord(resolved.record, origin)
    : describeCapabilityParameter(resolved.record, param, origin);
}
