// scripts/gateway-manifest/build.ts
// Pure build logic: transforms canonical ToolDefinition[] into the neutral
// GatewayManifest shape. No I/O, no side effects.

import type { GatewayManifest } from '../../src/gateway/gateway-manifest-types.js';
import type { ToolDefinition } from '../../src/tools/definitions/shared/tool-definition.js';
import { isRecord } from '../../src/utils/validation/type-guards.js';

export function actionValues(tool: ToolDefinition): string[] {
  const props = isRecord(tool.inputSchema.properties) ? tool.inputSchema.properties : {};
  const action = isRecord(props.action) ? props.action : undefined;
  return Array.isArray(action?.enum)
    ? action.enum.filter((v): v is string => typeof v === 'string')
    : [];
}

export function parameterNames(tool: ToolDefinition): string[] {
  const props = isRecord(tool.inputSchema.properties) ? tool.inputSchema.properties : {};
  return Object.keys(props).filter((n) => n !== 'action' && n !== 'subAction' && n !== 'params').sort();
}

export function buildGatewayManifest(defs: readonly ToolDefinition[]): GatewayManifest {
  return {
    version: 1,
    source: 'consolidatedToolDefinitions',
    tools: defs.map((tool) => ({
      name: tool.name,
      category: tool.category ?? null,
      description: tool.description,
      actions: actionValues(tool),
      parameterNames: parameterNames(tool),
      inputSchema: tool.inputSchema,
      perActionSchemas: false,
    })),
  };
}
