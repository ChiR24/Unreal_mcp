// scripts/gateway-manifest/emit.ts
// Output formatters: pretty JSON and compact JSON.
// Both are pure functions of the manifest data - no I/O.

import type { ToolDefinition } from '../../src/tools/definitions/shared/tool-definition.js';
import { buildGatewayManifest } from './build.js';

export const prettyManifest = (defs: readonly ToolDefinition[]): string =>
  JSON.stringify(buildGatewayManifest(defs), null, 2);

export const compactManifest = (defs: readonly ToolDefinition[]): string =>
  JSON.stringify(buildGatewayManifest(defs));
