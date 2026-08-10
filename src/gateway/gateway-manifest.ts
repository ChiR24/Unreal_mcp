// src/gateway/gateway-manifest.ts
// Typed loader for the neutral generated gateway manifest.
// The manifest is the single source of truth for the gateway catalog/definition,
// generated from canonical TypeScript tool definitions (scripts/generate-gateway-manifest.ts).
// Both the TS gateway and the native MCP Gateway consume this same artifact (no drift).

import type { ToolDefinition } from '../tools/definitions/shared/tool-definition.js';
import { gatewayManifest } from './gateway-manifest.generated.js';
import type { GatewayManifest, GatewayManifestTool } from './gateway-manifest-types.js';
import { GatewayManifestSchema } from './gateway-manifest-types.js';

export type { GatewayManifest, GatewayManifestTool } from './gateway-manifest-types.js';

const manifest = GatewayManifestSchema.parse(gatewayManifest);

export function getGatewayManifest(): GatewayManifest {
  return manifest;
}

export function getGatewayManifestTools(): readonly GatewayManifestTool[] {
  return manifest.tools;
}

export function getManifestToolDefinitions(): ToolDefinition[] {
  return manifest.tools.map((tool) => ({
    name: tool.name,
    category: tool.category ?? undefined,
    description: tool.description,
    inputSchema: tool.inputSchema,
  }));
}
