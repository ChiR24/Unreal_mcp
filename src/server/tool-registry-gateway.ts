import type { ITools } from '../types/tools/tool-interfaces.js';
import type { ToolDefinition } from '../tools/definitions/shared/tool-definition.js';
import { getManifestToolDefinitions } from '../gateway/gateway-manifest.js';
import { dynamicToolManager } from '../tools/dynamic/dynamic-tool-manager.js';
import { handleConsolidatedToolCall } from '../tools/orchestration/consolidated-tool-handlers.js';
import { cleanObject } from '../utils/serialization/safe-json.js';
import { isRecord } from '../utils/validation/type-guards.js';
import type { Logger } from '../utils/logging/logger.js';
import { maybeElicitMissingArgs } from './tool-registry-elicitation.js';
import { handleManageToolsCall } from './tool-registry-manage-tools.js';

const DEFAULT_SEARCH_LIMIT = 12;
const MAX_SEARCH_LIMIT = 25;
const MAX_EXECUTION_RESULT_CHARS = 100_000;

export type GatewayContext = {
  tools: ITools;
  logger: Logger;
  elicitationTimeoutMs: number;
  ensureConnected: () => Promise<boolean>;
};

type GatewayDescriptor = {
  name: string;
  category: string | undefined;
  description: string;
  actions: string[];
  parameterNames: string[];
  enabled: boolean;
};

function getActionValues(tool: ToolDefinition): string[] {
  const properties = isRecord(tool.inputSchema.properties) ? tool.inputSchema.properties : {};
  const action = isRecord(properties.action) ? properties.action : undefined;
  return Array.isArray(action?.enum)
    ? action.enum.filter((value): value is string => typeof value === 'string')
    : [];
}

function getParameterNames(tool: ToolDefinition): string[] {
  const properties = isRecord(tool.inputSchema.properties) ? tool.inputSchema.properties : {};
  return Object.keys(properties).filter((name) => name !== 'action' && name !== 'subAction' && name !== 'params').sort();
}

function getDescriptor(tool: ToolDefinition): GatewayDescriptor {
  return {
    name: tool.name,
    category: tool.category,
    description: tool.description,
    actions: getActionValues(tool),
    parameterNames: getParameterNames(tool),
    enabled: dynamicToolManager.isToolEnabled(tool.name)
  };
}

function getString(args: Record<string, unknown>, key: string): string | undefined {
  const value = args[key];
  return typeof value === 'string' && value.trim().length > 0 ? value.trim() : undefined;
}

function getBoundedInteger(value: unknown, fallback: number, minimum: number, maximum: number): number {
  return typeof value === 'number' && Number.isInteger(value)
    ? Math.min(Math.max(value, minimum), maximum)
    : fallback;
}

function gatewayError(operation: string, errorCode: string, message: string): Record<string, unknown> {
  return { success: false, operation, errorCode, error: message, message };
}

function isGatewayFailure(result: unknown): result is Record<string, unknown> {
  return isRecord(result) && result.success === false;
}

let gatewayRequestCounter = 0;

function nextGatewayCorrelationId(): string {
  gatewayRequestCounter += 1;
  return `gw-${gatewayRequestCounter}`;
}

function findTool(name: string | undefined): ToolDefinition | undefined {
  return name === undefined ? undefined : getManifestToolDefinitions().find((tool) => tool.name === name);
}

function rejectInvalidParams(tool: ToolDefinition, params: Record<string, unknown>): Record<string, unknown> | undefined {
  if ('action' in params || 'subAction' in params) {
    return gatewayError('execute', 'INVALID_PARAMS', 'params must not override action or subAction. Supply the selected action at the gateway level.');
  }

  const allowed = new Set(getParameterNames(tool));
  const unknown = Object.keys(params).filter((name) => !allowed.has(name));
  if (unknown.length > 0) {
    return {
      ...gatewayError('execute', 'UNDECLARED_PARAMETER', `Unknown parameter(s) for ${tool.name}: ${unknown.join(', ')}. Call describe before execution.`),
      allowedParameters: Array.from(allowed)
    };
  }
  return undefined;
}

export function searchGatewayCatalog(args: Record<string, unknown>): Record<string, unknown> {
  const query = (getString(args, 'query') ?? '').toLowerCase();
  const limit = getBoundedInteger(args.limit, DEFAULT_SEARCH_LIMIT, 1, MAX_SEARCH_LIMIT);
  const offset = getBoundedInteger(args.offset, 0, 0, Number.MAX_SAFE_INTEGER);
  const matches = getManifestToolDefinitions()
    .map(getDescriptor)
    .filter((tool) => {
      if (query.length === 0) return true;
      const searchable = [tool.name, tool.category ?? '', tool.description, ...tool.actions].join(' ').toLowerCase();
      return searchable.includes(query);
    });
  const results = matches.slice(offset, offset + limit);
  return {
    success: true,
    operation: 'search',
    query,
    results,
    total: matches.length,
    offset,
    limit,
    hasMore: offset + results.length < matches.length,
    message: 'Search results are compact. Call describe with an exact tool and action before execute.'
  };
}

export function describeGatewayCapability(args: Record<string, unknown>): Record<string, unknown> {
  const tool = findTool(getString(args, 'tool'));
  if (!tool) return gatewayError('describe', 'UNKNOWN_TOOL', 'Unknown tool. Call search to retrieve canonical tool names.');

  const action = getString(args, 'action');
  const actions = getActionValues(tool);
  if (action !== undefined && !actions.includes(action)) {
    return {
      ...gatewayError('describe', 'UNKNOWN_ACTION', `Unknown action '${action}' for ${tool.name}.`),
      tool: tool.name,
      availableActions: actions
    };
  }

  return {
    success: true,
    operation: 'describe',
    tool: tool.name,
    action,
    category: tool.category,
    description: tool.description,
    enabled: dynamicToolManager.isToolEnabled(tool.name),
    actions: action === undefined ? actions : [action],
    parameterNames: getParameterNames(tool),
    inputSchema: cleanObject(tool.inputSchema),
    perActionSchemas: false,
    message: 'The canonical catalog does not register per-action parameter schemas. parameterNames is the union across all actions — pass only parameters relevant to the selected action. Use the exact casing shown in inputSchema.'
  };
}

async function executeGatewayCall(args: Record<string, unknown>, context: GatewayContext): Promise<Record<string, unknown>> {
  const tool = findTool(getString(args, 'tool'));
  const action = getString(args, 'action');
  if (!tool) return gatewayError('execute', 'UNKNOWN_TOOL', 'Unknown tool. Call search before execute.');
  const actions = getActionValues(tool);
  if (!action || !actions.includes(action)) {
    return { ...gatewayError('execute', 'UNKNOWN_ACTION', `Unknown action for ${tool.name}. Call describe before execute.`), tool: tool.name, availableActions: actions };
  }
  if (!dynamicToolManager.isToolEnabled(tool.name)) {
    return gatewayError('execute', 'TOOL_DISABLED', `Tool '${tool.name}' is disabled or unavailable.`);
  }
  if (!isRecord(args.params) && args.params !== undefined) {
    return gatewayError('execute', 'INVALID_PARAMS', 'params must be an object.');
  }
  const params = isRecord(args.params) ? args.params : {};
  const invalidParams = rejectInvalidParams(tool, params);
  if (invalidParams) return invalidParams;

  const canRunWithoutConnection = tool.name === 'system_control' && action === 'get_project_settings';
  if (!canRunWithoutConnection && !await context.ensureConnected()) {
    return gatewayError('execute', 'NOT_CONNECTED', 'Unreal Engine is not connected.');
  }
  const targetArgs = await maybeElicitMissingArgs(
    tool.name,
    { ...params, action, subAction: action },
    context.tools.elicit,
    context.elicitationTimeoutMs,
    context.logger
  );
  const result = cleanObject(await handleConsolidatedToolCall(tool.name, targetArgs, context.tools));
  const serialized = JSON.stringify(result);
  if (serialized.length > MAX_EXECUTION_RESULT_CHARS) {
    return {
      ...gatewayError('execute', 'RESULT_TOO_LARGE', 'Result exceeded the gateway safety limit. Retry with the action pagination or filtering parameters described by this capability.'),
      tool: tool.name,
      action,
      resultChars: serialized.length
    };
  }
  return { success: !isRecord(result) || (result.success !== false && result.isError !== true), operation: 'execute', tool: tool.name, action, result };
}

async function configureGateway(args: Record<string, unknown>): Promise<Record<string, unknown>> {
  const action = getString(args, 'action');
  if (!action) return gatewayError('configure', 'MISSING_ACTION', 'configure requires a manage_tools action.');
  if (!isRecord(args.params) && args.params !== undefined) return gatewayError('configure', 'INVALID_PARAMS', 'params must be an object.');
  const result = await handleManageToolsCall({ ...(isRecord(args.params) ? args.params : {}), action });
  return { success: result.success === true, operation: 'configure', action, result };
}

export async function handleUnrealGatewayCall(args: Record<string, unknown>, context: GatewayContext): Promise<Record<string, unknown>> {
  const operation = getString(args, 'operation') ?? 'unknown';
  const correlationId = nextGatewayCorrelationId();
  const tool = getString(args, 'tool');
  const action = getString(args, 'action');
  context.logger.debug('gateway request received', { correlationId, operation, tool, action });

  let result: Record<string, unknown>;
  switch (operation) {
    case 'search': result = searchGatewayCatalog(args); break;
    case 'describe': result = describeGatewayCapability(args); break;
    case 'execute': result = await executeGatewayCall(args, context); break;
    case 'configure': result = await configureGateway(args); break;
    default: result = gatewayError(operation, 'UNKNOWN_OPERATION', 'operation must be search, describe, execute, or configure.');
  }

  if (isGatewayFailure(result)) {
    context.logger.warn('gateway request failed', {
      correlationId,
      operation,
      tool,
      action,
      errorCode: typeof result.errorCode === 'string' ? result.errorCode : undefined
    });
  } else {
    context.logger.debug('gateway request completed', { correlationId, operation, tool, action });
  }

  return result;
}
