// src/server/gateway/gateway-describe.ts
// Progressive, searchable gateway discovery for the `unreal` tool.
//
// Three drill-down levels, none of which dumps every tool schema or a full
// inputSchema for a described tool:
//   1. describe { tool }              -> tool summary + paginated/filterable actions
//   2. describe { tool, action }      -> paginated/filterable parameter catalog (tool-union)
//   3. describe { tool, action, param } -> exactly one parameter's full schema
//
// Invalid tool/action/param calls return closest-match suggestions and an
// executable `nextCall` payload. `perActionSchemas` is always false: the
// parameter catalog is the union across all actions of the parent tool.

import { isRecord } from '../../utils/validation/type-guards.js';
import { dynamicToolManager } from '../../tools/dynamic/dynamic-tool-manager.js';
import type { ToolDefinition } from '../../tools/definitions/shared/tool-definition.js';
import {
  getString,
  getBoundedInteger,
  gatewayError,
  findTool,
  allToolNames,
  getActionValues,
  getParameterNames
} from './gateway-shared.js';
import { closestMatches, buildNextCall, MAX_SUGGESTIONS } from './gateway-guidance.js';

const DEFAULT_DESCRIBE_LIMIT = 20;
const MAX_DESCRIBE_LIMIT = 50;

function getParameterSchema(tool: ToolDefinition, name: string): Record<string, unknown> | undefined {
  const properties = isRecord(tool.inputSchema.properties) ? tool.inputSchema.properties : {};
  const schema = isRecord(properties[name]) ? properties[name] : undefined;
  return schema as Record<string, unknown> | undefined;
}

function getRequiredSet(tool: ToolDefinition): Set<string> {
  const required = Array.isArray(tool.inputSchema.required) ? tool.inputSchema.required : [];
  return new Set(required.filter((value): value is string => typeof value === 'string'));
}

function getParameterSummary(tool: ToolDefinition, name: string): Record<string, unknown> {
  const schema = getParameterSchema(tool, name) ?? {};
  const summary: Record<string, unknown> = { name, type: typeof schema.type === 'string' ? schema.type : 'unknown' };
  if (typeof schema.description === 'string') summary.description = schema.description;
  if (Array.isArray(schema.enum)) summary.enum = schema.enum;
  return summary;
}

function unknownToolError(toolArg: string | undefined): Record<string, unknown> {
  const error = gatewayError('describe', 'UNKNOWN_TOOL', 'Unknown tool. Call search to retrieve canonical tool names.');
  const suggestions = closestMatches(toolArg ?? '', allToolNames(), MAX_SUGGESTIONS);
  const nextCall = suggestions.length > 0
    ? buildNextCall({ operation: 'describe', tool: suggestions[0] })
    : buildNextCall({ operation: 'search' });
  return { ...error, suggestions, nextCall };
}

function unknownActionError(toolName: string, actionArg: string, actions: string[]): Record<string, unknown> {
  const error = gatewayError('describe', 'UNKNOWN_ACTION', `Unknown action '${actionArg}' for ${toolName}.`);
  const suggestions = closestMatches(actionArg, actions, MAX_SUGGESTIONS);
  const nextCall = suggestions.length > 0
    ? buildNextCall({ operation: 'describe', tool: toolName, action: suggestions[0] })
    : buildNextCall({ operation: 'describe', tool: toolName });
  return { ...error, tool: toolName, availableActions: actions, suggestions, nextCall };
}

function unknownParamError(
  toolName: string,
  actionArg: string | undefined,
  paramArg: string,
  paramNames: string[]
): Record<string, unknown> {
  const error = gatewayError('describe', 'UNKNOWN_PARAM', `Unknown parameter '${paramArg}' for ${toolName}.`);
  const suggestions = closestMatches(paramArg, paramNames, MAX_SUGGESTIONS);
  const nextCall = suggestions.length > 0
    ? buildNextCall({ operation: 'describe', tool: toolName, action: actionArg, param: suggestions[0] })
    : buildNextCall({ operation: 'describe', tool: toolName, action: actionArg });
  return { ...error, tool: toolName, action: actionArg, availableParameters: paramNames, suggestions, nextCall };
}

function describeParamDetail(tool: ToolDefinition, actionArg: string | undefined, paramArg: string): Record<string, unknown> {
  return {
    success: true,
    operation: 'describe',
    tool: tool.name,
    action: actionArg,
    param: paramArg,
    required: getRequiredSet(tool).has(paramArg),
    schema: getParameterSchema(tool, paramArg) ?? {},
    scope: 'union',
    perActionSchemas: false,
    message: 'This parameter belongs to the tool-union catalog (per-action parameter mappings do not exist). Pass it only when relevant to the selected action, using the exact casing shown.'
  };
}

function describeActionParameters(
  tool: ToolDefinition,
  actionArg: string,
  query: string,
  limit: number,
  offset: number
): Record<string, unknown> {
  const paramNames = getParameterNames(tool);
  const filtered = query.length === 0
    ? paramNames
    : paramNames.filter((name) => {
      const summary = getParameterSummary(tool, name);
      const description = typeof summary.description === 'string' ? summary.description.toLowerCase() : '';
      return name.toLowerCase().includes(query) || description.includes(query);
    });
  const paged = filtered.slice(offset, offset + limit);
  const first = paged[0] ?? paramNames[0];
  return {
    success: true,
    operation: 'describe',
    tool: tool.name,
    action: actionArg,
    category: tool.category,
    description: tool.description,
    enabled: dynamicToolManager.isToolEnabled(tool.name),
    actions: [actionArg],
    parameters: paged.map((name) => getParameterSummary(tool, name)),
    parameterCount: filtered.length,
    parameterOffset: offset,
    parameterLimit: limit,
    parameterHasMore: offset + paged.length < filtered.length,
    scope: 'union',
    perActionSchemas: false,
    drillDown: buildNextCall({ operation: 'describe', tool: tool.name, action: actionArg, param: first }),
    message: 'parameterNames is the union catalog for this parent tool (per-action schemas do not exist). Drill into a single param (drillDown) for its full schema.'
  };
}

function describeToolSummary(
  tool: ToolDefinition,
  query: string,
  limit: number,
  offset: number
): Record<string, unknown> {
  const actions = getActionValues(tool);
  const filtered = query.length === 0
    ? actions
    : actions.filter((name) => name.toLowerCase().includes(query));
  const paged = filtered.slice(offset, offset + limit);
  const first = paged[0] ?? actions[0];
  return {
    success: true,
    operation: 'describe',
    tool: tool.name,
    category: tool.category,
    description: tool.description,
    enabled: dynamicToolManager.isToolEnabled(tool.name),
    actions: paged,
    actionCount: filtered.length,
    actionOffset: offset,
    actionLimit: limit,
    actionHasMore: offset + paged.length < filtered.length,
    scope: 'tool',
    perActionSchemas: false,
    drillDown: buildNextCall({ operation: 'describe', tool: tool.name, action: first }),
    message: 'Tool summary. Drill into an action (drillDown) to list its parameter catalog — parameters are the tool-union, not action-specific.'
  };
}

export function describeGatewayCapability(args: Record<string, unknown>): Record<string, unknown> {
  const toolArg = getString(args, 'tool');
  const actionArg = getString(args, 'action');
  const paramArg = getString(args, 'param');
  const query = (getString(args, 'query') ?? '').toLowerCase();
  const limit = getBoundedInteger(args.limit, DEFAULT_DESCRIBE_LIMIT, 1, MAX_DESCRIBE_LIMIT);
  const offset = getBoundedInteger(args.offset, 0, 0, Number.MAX_SAFE_INTEGER);

  const tool = findTool(toolArg);
  if (!tool) return unknownToolError(toolArg);

  const actions = getActionValues(tool);
  if (actionArg !== undefined && !actions.includes(actionArg)) {
    return unknownActionError(tool.name, actionArg, actions);
  }

  if (paramArg !== undefined) {
    const paramNames = getParameterNames(tool);
    if (!paramNames.includes(paramArg)) {
      return unknownParamError(tool.name, actionArg, paramArg, paramNames);
    }
    return describeParamDetail(tool, actionArg, paramArg);
  }

  if (actionArg === undefined) {
    return describeToolSummary(tool, query, limit, offset);
  }

  return describeActionParameters(tool, actionArg, query, limit, offset);
}
