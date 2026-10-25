// src/server/mcp-primitives/primitive-wiring.ts
// The MCP primitives beyond tools and resources: workflow prompts and argument
// completion. SERVER_CAPABILITIES is the exact surface the server advertises;
// every entry has its handlers registered here or by the tool and resource
// registries. `server/discover` (MCP 2026-07-28) is answered by the SDK from the
// same capabilities and instructions.

import type { CacheHint, Server, ServerCapabilities } from '@modelcontextprotocol/server';

import { complete } from './completions.js';
import { getPrompt, listPrompts } from './prompts.js';

export const SERVER_CAPABILITIES: ServerCapabilities = {
  tools: {},
  resources: {},
  prompts: {},
  completions: {},
};

const STATIC_LISTING: CacheHint = { ttlMs: 3_600_000, cacheScope: 'public' };

/**
 * The 2026-07-28 cache hints, the same as the native door's: the tool, prompt and template listings and the discover
 * answer never change while the server runs; the resource list follows the editor loosely and a read is live state,
 * so neither is shared and a read is never reused.
 */
export const CACHE_HINTS = {
  'tools/list': STATIC_LISTING,
  'prompts/list': STATIC_LISTING,
  'resources/templates/list': STATIC_LISTING,
  'server/discover': STATIC_LISTING,
  'resources/list': { ttlMs: 60_000, cacheScope: 'private' },
  'resources/read': { ttlMs: 0, cacheScope: 'private' },
} as const satisfies Record<string, CacheHint>;

export function wirePrimitives(server: Server): void {
  server.setRequestHandler('prompts/list', () => ({ prompts: listPrompts() }));
  server.setRequestHandler('prompts/get', (request) => getPrompt(request.params.name, request.params.arguments ?? {}));
  server.setRequestHandler('completion/complete', (request) => ({
    completion: { ...complete(request.params.ref, request.params.argument.name, request.params.argument.value) },
  }));
}
