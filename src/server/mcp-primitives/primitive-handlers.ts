// src/server/mcp-primitives/primitive-handlers.ts
// Task 37: registers the MCP primitive request handlers that back the advertised
// session profile — resources/subscribe + resources/unsubscribe (Task 34),
// prompts/list + prompts/get (Task 32), and completion/complete (Task 33). Each
// handler is a thin protocol adapter over a pure primitive: it resolves the
// session id, delegates, and maps a typed refusal to the right McpError. The
// existing resources/list, resources/templates/list, and resources/read handlers
// stay owned by ResourceRegistry; this module never touches them.

import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import {
  CompleteRequestSchema,
  ErrorCode,
  GetPromptRequestSchema,
  ListPromptsRequestSchema,
  McpError,
  SubscribeRequestSchema,
  UnsubscribeRequestSchema,
} from '@modelcontextprotocol/sdk/types.js';

import { complete } from './completions/index.js';
import type { CompletionCandidateSource, CompletionReference, CompletionRequest } from './completions/index.js';
import { PromptError, getPrompt, listPrompts } from './prompts/index.js';
import type { PromptReferenceValidator } from './prompts/index.js';
import type { SessionCapabilityProfile } from './session-capability-profile.js';
import type { SubscriptionStore } from './subscriptions/subscription-store.js';

export interface PrimitiveHandlerDeps {
  readonly server: Server;
  readonly store: SubscriptionStore;
  readonly resolveSessionId: () => string;
  readonly promptValidator: PromptReferenceValidator;
  readonly completionSource: CompletionCandidateSource;
  readonly resolveProfile: (sessionId: string) => SessionCapabilityProfile;
}

// The methods this module registers, so the wiring can assemble the complete
// handler table for the fail-closed PrimitiveRegistry.
export const REGISTERED_PRIMITIVE_METHODS = [
  'resources/subscribe',
  'resources/unsubscribe',
  'prompts/list',
  'prompts/get',
  'completion/complete',
] as const;

function toCompletionRef(ref: { type: string; name?: string; uri?: string }): CompletionReference {
  return ref.type === 'ref/prompt'
    ? { type: 'ref/prompt', name: ref.name ?? '' }
    : { type: 'ref/resource', uri: ref.uri ?? '' };
}

function promptErrorToMcp(error: unknown): McpError {
  if (error instanceof PromptError) {
    return new McpError(ErrorCode.InvalidParams, error.message, { code: error.code });
  }
  return error instanceof McpError
    ? error
    : new McpError(ErrorCode.InternalError, error instanceof Error ? error.message : String(error));
}

/** Register the primitive handlers and report the methods they now back. */
export function registerPrimitiveHandlers(deps: PrimitiveHandlerDeps): ReadonlySet<string> {
  const { server, store, resolveSessionId, promptValidator, completionSource, resolveProfile } = deps;

  server.setRequestHandler(SubscribeRequestSchema, (request) => {
    const result = store.subscribe(resolveSessionId(), request.params.uri);
    if (!result.accepted) {
      // A server-originated McpError, NOT a -32601: the wired handler exists and
      // discriminated the URI (off-allowlist / invalid session), so the client
      // learns the subscription was refused, not that subscribe is unsupported.
      throw new McpError(ErrorCode.InvalidParams, `Resource is not subscribable: ${request.params.uri}`);
    }
    return {};
  });

  server.setRequestHandler(UnsubscribeRequestSchema, (request) => {
    store.unsubscribe(resolveSessionId(), request.params.uri);
    return {};
  });

  server.setRequestHandler(ListPromptsRequestSchema, () => ({
    prompts: listPrompts().map((entry) => ({
      name: entry.name,
      title: entry.title,
      description: entry.description,
      arguments: entry.arguments.map((argument) => ({
        name: argument.name,
        description: argument.description,
        required: argument.required,
      })),
    })),
  }));

  server.setRequestHandler(GetPromptRequestSchema, (request) => {
    try {
      const rendered = getPrompt(request.params.name, request.params.arguments ?? {}, promptValidator);
      return { description: rendered.description, messages: [...rendered.messages] };
    } catch (error) {
      throw promptErrorToMcp(error);
    }
  });

  server.setRequestHandler(CompleteRequestSchema, (request) => {
    const sessionId = resolveSessionId();
    const completionRequest: CompletionRequest = {
      ref: toCompletionRef(request.params.ref),
      argument: { name: request.params.argument.name, value: request.params.argument.value },
    };
    const outcome = complete(completionRequest, sessionId, resolveProfile(sessionId), completionSource);
    return {
      completion: {
        values: [...outcome.completion.values],
        total: outcome.completion.total,
        hasMore: outcome.completion.hasMore,
      },
      ...(outcome.guidance !== undefined ? { _meta: { guidance: outcome.guidance } } : {}),
    };
  });

  return new Set(REGISTERED_PRIMITIVE_METHODS);
}
