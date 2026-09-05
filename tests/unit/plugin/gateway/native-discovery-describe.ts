/// <reference types="node" />

// Task 25: normalized `describe` reference. Four levels - bare overview of the
// parent tools, tool summary, exact capability contract, single parameter
// schema; mirrored exactly by McpNativeGatewayDescribe.cpp and
// McpNativeGatewayDescribeOverview.cpp.
//
// Actions are PUBLIC names (the capability id's last segment), never the
// internal dispatch verb: every manage_audio capability dispatches through
// "manage_audio", so the dispatch verb can neither list nor address one.

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

const isObject = (value: JsonValue | undefined): value is Record<string, JsonValue> =>
  value !== undefined && value !== null && typeof value === 'object' && !Array.isArray(value);

const schemaProperties = (schema: JsonValue): Readonly<Record<string, JsonValue>> => {
  if (!isObject(schema)) return {};
  const properties = schema.properties;
  return isObject(properties) ? properties : {};
};

/** `policy.consent` when it demands a grant, else undefined for `none`/unreadable. */
const consentMode = (record: DiscoveryRecord): string | undefined => {
  const policy = record.policy;
  if (!isObject(policy)) return undefined;
  const consent = policy.consent;
  if (typeof consent !== 'string' || consent === 'none') return undefined;
  return consent;
};

const requiredNames = (schema: JsonValue): readonly string[] => {
  if (!isObject(schema)) return [];
  const required = schema.required;
  return Array.isArray(required) ? required.filter((v): v is string => typeof v === 'string') : [];
};

const parameterView = (schema: JsonValue, name: string): JsonValue => {
  const property = schemaProperties(schema)[name];
  const body = isObject(property) ? property : {};
  const view: Record<string, JsonValue> = {
    name,
    required: requiredNames(schema).includes(name),
    type: typeof body.type === 'string' ? body.type : 'unknown',
  };
  if (typeof body.description === 'string') view.description = body.description;
  if (Array.isArray(body.enum)) view.enum = body.enum;
  return view;
};

/** The published input schema: the envelope key `action` is stripped from properties and required (McpStripActionFromInputSchema). */
const stripAction = (schema: JsonValue): JsonValue => {
  if (!isObject(schema)) return schema;
  const copy: Record<string, JsonValue> = { ...schema };
  const properties = schema.properties;
  if (isObject(properties) && 'action' in properties) {
    const { action: _action, ...rest } = properties;
    copy.properties = rest;
  }
  const required = schema.required;
  if (Array.isArray(required)) copy.required = required.filter((entry) => entry !== 'action');
  return copy;
};

/** The action name execute accepts and search advertises: the id's last segment (McpCapabilityPublicAction). */
export const publicAction = (record: DiscoveryRecord): string => {
  const dot = record.id.lastIndexOf('.');
  return dot >= 0 ? record.id.slice(dot + 1) : record.routing.dispatchAction;
};

const recordsForParent = (parent: string): readonly DiscoveryRecord[] =>
  loadCanonicalRegistry()
    .records.filter((record) => record.routing.parentTool === parent)
    .slice()
    .sort((a, b) => ordinalCompare(a.id, b.id));

/** Public action first, dispatch verb as the fallback (FMcpCapabilityStore::FindByParentAction). */
const findByParentAction = (siblings: readonly DiscoveryRecord[], action: string): DiscoveryRecord | undefined =>
  siblings.find((entry) => publicAction(entry) === action)
  ?? siblings.find((entry) => entry.routing.dispatchAction === action);

// Bare describe overview: the canonical parents, one level above the tool
// summary (McpGatewayDescribeToolOverview).
const toolOverview = (limit: number, offset: number): JsonValue => {
  const registry = loadCanonicalRegistry();
  const parents = allParents();
  const boundedOffsetValue = Math.min(offset, parents.length);
  const page = parents.slice(boundedOffsetValue, boundedOffsetValue + limit);
  return {
    catalogRevision: registry.catalogRevision,
    message: 'Canonical parent tools. Pass tool to list its actions, tool + action for one exact contract, or query to search.',
    operation: 'describe',
    scope: 'catalog',
    success: true,
    toolCount: parents.length,
    toolHasMore: boundedOffsetValue + page.length < parents.length,
    toolLimit: limit,
    toolOffset: boundedOffsetValue,
    tools: page.map((parent) => ({
      actionCount: recordsForParent(parent).length,
      nextCall: { operation: 'describe', tool: parent },
      tool: parent,
    })),
  };
};

export const describeCapability = (input: DiscoveryInput): JsonValue => {
  const registry = loadCanonicalRegistry();
  const limit = boundedLimit(input.limit, DESCRIBE_DEFAULT_LIMIT, DESCRIBE_MAX_LIMIT);
  const offset = boundedOffset(input.offset);
  const query = (input.query ?? '').trim().toLowerCase();
  const tool = input.tool ?? '';

  if (tool.length === 0 && input.action === undefined) return toolOverview(limit, offset);

  if (!allParents().includes(tool)) {
    return guidedError('describe', 'UNKNOWN_TOOL', 'Unknown tool. Call search to retrieve canonical capability names.', {
      nextCall: { operation: 'search' },
      suggestions: closestMatches(tool, allParents()) as readonly JsonValue[],
    });
  }

  const siblings = recordsForParent(tool);
  const actions = sortedUnique(siblings.map(publicAction));

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

  const record = findByParentAction(siblings, input.action);
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
    action: publicAction(record),
    availability: record.availability,
    available: isAvailable(record),
    behavior: record.behavior as JsonValue,
    capability: record.id,
    catalogRevision: registry.catalogRevision,
    ...(consentMode(record) === undefined
      ? {}
      : {
          consentGrant: {
            acknowledge: consentMode(record) === 'elevated' ? 'elevated' : 'explicit',
            capability: record.id,
          },
        }),
    cost: record.cost,
    deprecation: record.deprecation as JsonValue,
    domain: record.discovery.domain,
    effect: record.behavior.effect,
    exampleCount: record.examples.length,
    // The record's real example pair ships with the contract, so a caller never
    // has to guess a request shape the catalog already answers.
    ...(record.examples.length > 0 ? { examples: record.examples } : {}),
    family: record.discovery.family,
    hashes: record.hashes as JsonValue,
    inputSchema: stripAction(record.schemas.input),
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
