// src/server/mcp-primitives/primitive-wiring.ts
// The MCP primitives beyond tools and resources: workflow prompts, argument
// completion and discovery. SERVER_CAPABILITIES is the exact surface the server
// advertises; every entry has its handlers registered here or by the tool and
// resource registries.

import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import {
  CompleteRequestSchema,
  GetPromptRequestSchema,
  ListPromptsRequestSchema,
  SUPPORTED_PROTOCOL_VERSIONS,
  type ServerCapabilities,
} from '@modelcontextprotocol/sdk/types.js';
import { z } from 'zod';

import { complete } from './completions.js';
import { getPrompt, listPrompts } from './prompts.js';

export const SERVER_CAPABILITIES: ServerCapabilities = {
  tools: {},
  resources: {},
  prompts: {},
  completions: {},
};

const DiscoverRequestSchema = z.object({ method: z.literal('server/discover'), params: z.optional(z.looseObject({})) });

/**
 * The `server/discover` answer (MCP 2026-07-28): versions, capabilities, identity and instructions, without a
 * session. It lists only the revisions this server negotiates at initialize, so a 2026-07-28 client falls back to
 * that handshake. Nothing in it changes while the process runs, hence the hour-long public cache hint.
 */
export function discoverResult(serverInfo: { name: string; version: string }, instructions: string): Record<string, unknown> {
  return {
    resultType: 'complete',
    supportedVersions: [...SUPPORTED_PROTOCOL_VERSIONS],
    capabilities: SERVER_CAPABILITIES,
    instructions,
    ttlMs: 3_600_000,
    cacheScope: 'public',
    _meta: { 'io.modelcontextprotocol/serverInfo': serverInfo },
  };
}

export function wirePrimitives(server: Server, serverInfo: { name: string; version: string }, instructions: string): void {
  server.setRequestHandler(ListPromptsRequestSchema, () => ({ prompts: listPrompts() }));
  server.setRequestHandler(GetPromptRequestSchema, (request) => getPrompt(request.params.name, request.params.arguments ?? {}));
  server.setRequestHandler(CompleteRequestSchema, (request) => ({
    completion: complete(request.params.ref, request.params.argument.name, request.params.argument.value),
  }));
  server.setRequestHandler(DiscoverRequestSchema, () => discoverResult(serverInfo, instructions));
}
