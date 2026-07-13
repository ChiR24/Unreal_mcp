// src/server/gateway/gateway-shared.ts
// Shared helpers for the `unreal` gateway (search/describe/execute/configure).
// Kept dependency-free of the operation dispatch so describe/search/execute can
// reuse parsing, lookup, and error envelope construction without cycles.

import { getManifestToolDefinitions } from '../../gateway/gateway-manifest.js';
import { isRecord } from '../../utils/validation/type-guards.js';
import type { ToolDefinition } from '../../tools/definitions/shared/tool-definition.js';

export const DEFAULT_SEARCH_LIMIT = 12;
export const MAX_SEARCH_LIMIT = 25;

export function getString(args: Record<string, unknown>, key: string): string | undefined {
  const value = args[key];
  return typeof value === 'string' && value.trim().length > 0 ? value.trim() : undefined;
}

export function getBoundedInteger(value: unknown, fallback: number, minimum: number, maximum: number): number {
  return typeof value === 'number' && Number.isInteger(value)
    ? Math.min(Math.max(value, minimum), maximum)
    : fallback;
}

export function gatewayError(operation: string, errorCode: string, message: string): Record<string, unknown> {
  return { success: false, operation, errorCode, error: message, message };
}

export function isGatewayFailure(result: unknown): result is Record<string, unknown> {
  return isRecord(result) && result.success === false;
}

export function findTool(name: string | undefined): ToolDefinition | undefined {
  return name === undefined ? undefined : getManifestToolDefinitions().find((tool) => tool.name === name);
}

export function allToolNames(): string[] {
  return getManifestToolDefinitions().map((tool) => tool.name);
}

export function getActionValues(tool: ToolDefinition): string[] {
  const properties = isRecord(tool.inputSchema.properties) ? tool.inputSchema.properties : {};
  const action = isRecord(properties.action) ? properties.action : undefined;
  return Array.isArray(action?.enum)
    ? action.enum.filter((value): value is string => typeof value === 'string')
    : [];
}

export function getParameterNames(tool: ToolDefinition): string[] {
  const properties = isRecord(tool.inputSchema.properties) ? tool.inputSchema.properties : {};
  return Object.keys(properties).filter((name) => name !== 'action' && name !== 'subAction' && name !== 'params').sort();
}

let gatewayRequestCounter = 0;

export function nextGatewayCorrelationId(): string {
  gatewayRequestCounter += 1;
  return `gw-${gatewayRequestCounter}`;
}
