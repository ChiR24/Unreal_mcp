/// <reference types="node" />

// Task 25: normalized `describe` reference. Three levels - tool summary, exact
// capability contract, single parameter schema; mirrored exactly by
// McpNativeGatewayDescribe.cpp.

import {
  allParents,
  boundedLimit,
  boundedOffset,
  closestMatches,
  guidedError,
  isAvailable,
  loadCanonicalRegistry,
  ordinalCompare,
  sortedUnique,
  type DiscoveryInput,
  type DiscoveryRecord,
  type JsonValue,
} from './native-discovery-model.js';

export const DESCRIBE_DEFAULT_LIMIT = 20;
export const DESCRIBE_MAX_LIMIT = 50;

const schemaProperties = (schema: JsonValue): Readonly<Record<string, JsonValue>> => {
  if (schema === null || typeof schema !== 'object' || Array.isArray(schema)) return {};
  const properties = (schema as Record<string, JsonValue>).properties;
  if (properties === null || typeof properties !== 'object' || Array.isArray(properties)) return {};
  return properties as Record<string, JsonValue>;
};

const requiredNames = (schema: JsonValue): readonly string[] => {
  if (schema === null || typeof schema !== 'object' || Array.isArray(schema)) return [];
  const required = (schema as Record<string, JsonValue>).required;
  return Array.isArray(required) ? required.filter((v): v is string => typeof v === 'string') : [];
};

const parameterView = (schema: JsonValue, name: string): JsonValue => {
  const property = schemaProperties(schema)[name];
  const body = property !== undefined && typeof property === 'object' && property !== null && !Array.isArray(property)
    ? (property as Record<string, JsonValue>)
    : {};
  const view: Record<string, JsonValue> = {
    name,
    required: requiredNames(schema).includes(name),
    type: typeof body.type === 'string' ? body.type : 'unknown',
  };
  if (typeof body.description === 'string') view.description = body.description;
  if (Array.isArray(body.enum)) view.enum = body.enum;
  return view;
};

const recordsForParent = (parent: string): readonly DiscoveryRecord[] =>
  loadCanonicalRegistry()
    .records.filter((record) => record.routing.parentTool === parent)
    .slice()
    .sort((a, b) => ordinalCompare(a.id, b.id));

export const describeCapability = (input: DiscoveryInput): JsonValue => {
  const registry = loadCanonicalRegistry();
  const limit = boundedLimit(input.limit, DESCRIBE_DEFAULT_LIMIT, DESCRIBE_MAX_LIMIT);
  const offset = boundedOffset(input.offset);
  const query = (input.query ?? '').trim().toLowerCase();
  const tool = input.tool ?? '';

  if (!allParents().includes(tool)) {
    return guidedError('describe', 'UNKNOWN_TOOL', 'Unknown tool. Call search to retrieve canonical capability names.', {
      nextCall: { operation: 'search' },
      suggestions: closestMatches(tool, allParents()) as readonly JsonValue[],
    });
  }

  const siblings = recordsForParent(tool);
  const actions = sortedUnique(siblings.map((record) => record.routing.dispatchAction));

  if (input.action === undefined) {
    const filtered = query.length === 0 ? actions : actions.filter((a) => a.toLowerCase().includes(query));
    const paged = filtered.slice(offset, offset + limit);
    return {
      actionCount: filtered.length,
      actionHasMore: offset + paged.length < filtered.length,
      actionLimit: limit,
      actionOffset: offset,
      actions: paged as readonly JsonValue[],
      capabilityCount: siblings.length,
      catalogRevision: registry.catalogRevision,
      domains: sortedUnique(siblings.map((r) => r.discovery.domain)) as readonly JsonValue[],
      drillDown: { action: paged[0] ?? actions[0] ?? '', operation: 'describe', tool },
      families: sortedUnique(siblings.map((r) => r.discovery.family)) as readonly JsonValue[],
      message: 'Tool summary. Drill into an action to receive that capability\'s exact contract.',
      operation: 'describe',
      scope: 'tool',
      success: true,
      tool,
    };
  }

  const record = siblings.find((entry) => entry.routing.dispatchAction === input.action);
  if (record === undefined) {
    return guidedError('describe', 'UNKNOWN_ACTION', `Unknown action '${input.action}' for ${tool}.`, {
      availableActions: actions as readonly JsonValue[],
      nextCall: { operation: 'describe', tool },
      suggestions: closestMatches(input.action, actions) as readonly JsonValue[],
      tool,
    });
  }

  const paramNames = Object.keys(schemaProperties(record.schemas.input))
    .filter((name) => name !== 'action' && name !== 'subAction')
    .sort(ordinalCompare);

  if (input.param !== undefined) {
    if (!paramNames.includes(input.param)) {
      return guidedError('describe', 'UNKNOWN_PARAM', `Unknown parameter '${input.param}' for ${record.id}.`, {
        availableParameters: paramNames as readonly JsonValue[],
        capability: record.id,
        nextCall: { action: input.action, operation: 'describe', tool },
        suggestions: closestMatches(input.param, paramNames) as readonly JsonValue[],
      });
    }
    return {
      capability: record.id,
      catalogRevision: registry.catalogRevision,
      message: 'Exact per-action parameter schema. Pass it under params on execute.',
      operation: 'describe',
      param: input.param,
      required: requiredNames(record.schemas.input).includes(input.param),
      schema: schemaProperties(record.schemas.input)[input.param] ?? {},
      scope: 'capability',
      success: true,
    };
  }

  return {
    action: record.routing.dispatchAction,
    availability: record.availability,
    available: isAvailable(record),
    behavior: record.behavior as JsonValue,
    capability: record.id,
    catalogRevision: registry.catalogRevision,
    cost: record.cost,
    deprecation: record.deprecation as JsonValue,
    domain: record.discovery.domain,
    effect: record.behavior.effect,
    exampleCount: record.examples.length,
    family: record.discovery.family,
    hashes: record.hashes as JsonValue,
    inputSchema: record.schemas.input,
    message: 'Exact capability contract. Every parameter below is action-specific, not a tool union.',
    operation: 'describe',
    outputSchema: record.schemas.output,
    parameters: paramNames.map((name) => parameterView(record.schemas.input, name)),
    parent: record.routing.parentTool,
    policy: record.policy,
    scope: 'capability',
    success: true,
    summary: record.discovery.summary,
    tool,
    whenNotToUse: record.discovery.whenNotToUse as readonly JsonValue[],
    whenToUse: record.discovery.whenToUse as readonly JsonValue[],
  };
};

