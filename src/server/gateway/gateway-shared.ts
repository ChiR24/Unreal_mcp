// src/server/gateway/gateway-shared.ts
// Shared helpers for the `unreal` gateway (search/describe/execute/configure).
// Kept dependency-free of the operation dispatch so describe/search/execute can
// reuse parsing, lookup, and error envelope construction without cycles.

import { getManifestToolDefinitions } from '../../gateway/gateway-manifest.js';
import { isRecord } from '../../utils/validation/type-guards.js';
import { normalizeSchemaTypes } from './gateway-schema-normalize.js';
import type { ToolDefinition } from '../../tools/definitions/shared/tool-definition.js';
import { CorrelationIdSchema, type CorrelationId } from '../../tools/catalog/capabilities/semantic/ids.js';

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

// Clamps like getBoundedInteger, but returns undefined instead of a fallback so
// callers can tell "no page size requested" apart from an explicit one.
export function getOptionalBoundedInteger(value: unknown, minimum: number, maximum: number): number | undefined {
  return typeof value === 'number' && Number.isInteger(value)
    ? Math.min(Math.max(value, minimum), maximum)
    : undefined;
}

export type ParameterCoercion = {
  readonly parameter: string;
  readonly requested: number;
  readonly applied: number;
  readonly reason: string;
};

// Reports what getBoundedInteger silently did to an out-of-range argument, so a
// caller who asked for limit=0 can tell the response describes a different
// request. Describes the clamp without performing it: one clamp, one reporter.
export function integerCoercion(
  parameter: string,
  value: unknown,
  minimum: number,
  maximum: number
): ParameterCoercion | undefined {
  if (typeof value !== 'number' || !Number.isInteger(value)) return undefined;
  const applied = Math.min(Math.max(value, minimum), maximum);
  if (applied === value) return undefined;
  return {
    parameter,
    requested: value,
    applied,
    reason: value < minimum
      ? `${parameter}=${value} is below the minimum of ${minimum}.`
      : `${parameter}=${value} is above the maximum of ${maximum}.`
  };
}

export function gatewayError(operation: string, errorCode: string, message: string): Record<string, unknown> {
  return { success: false, operation, errorCode, error: message, message };
}

export function isGatewayFailure(result: unknown): result is Record<string, unknown> {
  return isRecord(result) && result.success === false;
}

// The manifest is parsed once at module load and gateway consumers only read it,
// so the normalized projection is built on first use and shared thereafter.
let normalizedToolDefinitions: ToolDefinition[] | undefined;

function gatewayToolDefinitions(): ToolDefinition[] {
  normalizedToolDefinitions ??= getManifestToolDefinitions().map((tool) => ({
    ...tool,
    inputSchema: normalizeSchemaTypes(tool.inputSchema)
  }));
  return normalizedToolDefinitions;
}

export function findTool(name: string | undefined): ToolDefinition | undefined {
  return name === undefined ? undefined : gatewayToolDefinitions().find((tool) => tool.name === name);
}

export function allToolNames(): string[] {
  return gatewayToolDefinitions().map((tool) => tool.name);
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

export function nextGatewayCorrelationId(): CorrelationId {
  gatewayRequestCounter += 1;
  return CorrelationIdSchema.parse(`gw-${gatewayRequestCounter}`);
}
