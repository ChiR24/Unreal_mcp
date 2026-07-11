// src/gateway/gateway-manifest.ts
// Typed loader for the neutral generated gateway manifest.
// The manifest is the single source of truth for the gateway catalog/definition,
// generated from canonical TypeScript tool definitions (scripts/generate-gateway-manifest.ts).
// Both the TS gateway and the native MCP Gateway consume this same artifact (no drift).

import { gatewayManifest } from './gateway-manifest.generated.js';
import type { ToolDefinition } from '../tools/definitions/shared/tool-definition.js';

export type GatewayManifestTool = {
  name: string;
  category: string | null;
  description: string;
  actions: string[];
  parameterNames: string[];
  inputSchema: Record<string, unknown>;
  perActionSchemas: boolean;
};

export type GatewayManifest = {
  version: number;
  source: string;
  tools: GatewayManifestTool[];
};

const manifest = gatewayManifest as unknown as GatewayManifest;

export function getGatewayManifest(): GatewayManifest {
  return manifest;
}

export function getGatewayManifestTools(): readonly GatewayManifestTool[] {
  return manifest.tools;
}

/** Manifest-derived tool definitions consumed by the TS gateway search/describe/execute. */
export function getManifestToolDefinitions(): ToolDefinition[] {
  return manifest.tools.map((tool) => ({
    name: tool.name,
    category: (tool.category ?? undefined) as ToolDefinition['category'],
    description: tool.description,
    inputSchema: tool.inputSchema,
  }));
}
