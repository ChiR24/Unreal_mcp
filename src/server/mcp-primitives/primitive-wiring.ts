// src/server/mcp-primitives/primitive-wiring.ts
// The MCP primitives beyond tools and resources: workflow prompts and
// argument completion. SERVER_CAPABILITIES is the exact surface the server
// advertises; every entry has its handlers registered here or by the tool and
// resource registries.

import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import {
  CompleteRequestSchema,
  GetPromptRequestSchema,
  ListPromptsRequestSchema,
  type ServerCapabilities,
} from '@modelcontextprotocol/sdk/types.js';

import { complete } from './completions.js';
import { getPrompt, listPrompts } from './prompts.js';

export const SERVER_CAPABILITIES: ServerCapabilities = {
  tools: {},
  resources: {},
  prompts: {},
  completions: {},
};

export function wirePrimitives(server: Server): void {
  server.setRequestHandler(ListPromptsRequestSchema, () => ({ prompts: listPrompts() }));
  server.setRequestHandler(GetPromptRequestSchema, (request) => getPrompt(request.params.name, request.params.arguments ?? {}));
  server.setRequestHandler(CompleteRequestSchema, (request) => ({
    completion: complete(request.params.ref, request.params.argument.name, request.params.argument.value),
  }));
}
