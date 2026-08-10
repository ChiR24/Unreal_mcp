// src/gateway/gateway-manifest-types.ts
// Shared Zod schemas and inferred types for the neutral gateway manifest.
// Extracted to break the circular dependency between the runtime loader
// (gateway-manifest.ts) and the generated artifact (gateway-manifest.generated.ts).
// The schema validates the generated manifest at runtime via .parse(), replacing
// the previous `as unknown as GatewayManifest` cast. Category literals narrow
// to the exact ToolDefinition union without any cast.

import { z } from 'zod';

export const GatewayManifestToolSchema = z.strictObject({
  name: z.string(),
  category: z.nullable(z.enum(['core', 'world', 'gameplay', 'utility'])),
  description: z.string(),
  actions: z.array(z.string()),
  parameterNames: z.array(z.string()),
  inputSchema: z.record(z.string(), z.unknown()),
  perActionSchemas: z.boolean(),
});

export const GatewayManifestSchema = z.strictObject({
  version: z.number(),
  source: z.string(),
  tools: z.array(GatewayManifestToolSchema),
});

export type GatewayManifestTool = z.infer<typeof GatewayManifestToolSchema>;
export type GatewayManifest = z.infer<typeof GatewayManifestSchema>;
