// src/server/mcp-primitives/primitive-registry.ts
// Task 37: the fail-closed primitive registration-table validator (the PARITY
// seam). The MCP SDK's Server.assertRequestHandlerCapability guards
// completion/complete, prompts/*, resources/list|read|templates, tools/*, and
// tasks/* — but has NO case for resources/subscribe or resources/unsubscribe. So
// a server may advertise resources.subscribe:true, register no subscribe handler,
// connect happily, and only fail at RUNTIME with -32601 when a client subscribes.
// Tasks has the INVERSE hole and it is just as load-bearing: that same guard
// early-returns while `_capabilities` is still unset, which is exactly when the
// Protocol constructor auto-registers tasks/* for a supplied `taskStore`. So a
// server handed a task store but no `tasks` capability answers tasks/* for real
// while its initialize says Tasks is unsupported. Neither direction is caught by
// the SDK; both are caught here.
// This module closes that gap: it derives the advertised capability surface from
// the actual handler table and refuses to construct — BEFORE connect — when an
// advertised capability lacks its backing handler(s). It carries NO transport
// wiring; primitive-wiring.ts feeds it the real handler table at setup time.

import type { ServerCapabilities } from '@modelcontextprotocol/sdk/types.js';

// The method groups that back each advertised capability. A capability is only
// advertised when EVERY method in its group is present, so the surface can never
// claim a primitive it does not fully implement.
const TOOLS_METHODS = ['tools/list', 'tools/call'] as const;
const RESOURCES_READ_METHODS = ['resources/list', 'resources/templates/list', 'resources/read'] as const;
const RESOURCES_SUBSCRIBE_METHODS = ['resources/subscribe', 'resources/unsubscribe'] as const;
const PROMPTS_METHODS = ['prompts/list', 'prompts/get'] as const;
const COMPLETIONS_METHODS = ['completion/complete'] as const;
// Task 44: the four MCP Tasks methods. The SDK's Protocol constructor registers
// all four together the moment a `taskStore` is supplied, so the group is
// all-or-nothing by construction — there is no partial Tasks surface to advertise.
const TASKS_METHODS = ['tasks/get', 'tasks/list', 'tasks/cancel', 'tasks/result'] as const;

// The capability surface the implemented session profile advertises — the
// independent oracle the fail-closed validator checks the handler table against.
// server-factory must mirror this exact shape as an inline literal (kept a
// literal for the native source-contract text-slice); the wiring test pins that
// the wired server's actual advertisement equals it.
export const ADVERTISED_SESSION_CAPABILITIES: ServerCapabilities = {
  tools: {},
  resources: { subscribe: true },
  prompts: {},
  completions: {},
  tasks: { list: {}, cancel: {}, requests: { tools: { call: {} } } },
};


export interface CreatePrimitiveRegistryInput {
  readonly handlers: ReadonlyMap<string, unknown>;
  readonly capabilities: ServerCapabilities;
}

export interface PrimitiveRegistry {
  readonly capabilities: ServerCapabilities;
}

/**
 * Thrown when an advertised capability lacks a required handler in the table.
 * Fail-closed and pre-connect: construction stops rather than deferring the
 * divergence to a runtime -32601. `method` names the exact missing MCP method.
 */
export class PrimitiveRegistrationError extends Error {
  readonly code = 'PRIMITIVE_HANDLER_MISSING';
  readonly capability: string;
  readonly method: string;

  constructor(capability: string, method: string) {
    super(
      `Advertised capability '${capability}' is missing its backing handler for '${method}'. ` +
        'Register the handler before connect or drop the capability.',
    );
    this.name = 'PrimitiveRegistrationError';
    this.capability = capability;
    this.method = method;
    // Preserve prototype for instanceof across the emit target.
    Object.setPrototypeOf(this, PrimitiveRegistrationError.prototype);
  }
}

function hasAll(handlers: ReadonlyMap<string, unknown>, methods: readonly string[]): boolean {
  return methods.every((method) => handlers.has(method));
}

/**
 * Derive the EXACT advertised capability surface a handler table backs. A
 * complete table derives exactly `{ tools, resources.subscribe, prompts,
 * completions, tasks }` and nothing else — never logging, never any listChanged
 * member. A partial table only advertises the capabilities it fully backs.
 */
export function deriveAdvertisedCapabilities(handlers: ReadonlyMap<string, unknown>): ServerCapabilities {
  const capabilities: ServerCapabilities = {};
  if (hasAll(handlers, TOOLS_METHODS)) {
    capabilities.tools = {};
  }
  if (hasAll(handlers, RESOURCES_READ_METHODS)) {
    capabilities.resources = hasAll(handlers, RESOURCES_SUBSCRIBE_METHODS) ? { subscribe: true } : {};
  }
  if (hasAll(handlers, PROMPTS_METHODS)) {
    capabilities.prompts = {};
  }
  if (hasAll(handlers, COMPLETIONS_METHODS)) {
    capabilities.completions = {};
  }
  if (hasAll(handlers, TASKS_METHODS)) {
    // `requests.tools.call` claims a tools/call MAY be task-augmented, so it is
    // derived from the tools/call handler being present rather than asserted:
    // a Tasks surface with no tool to run must not claim it.
    capabilities.tasks = hasAll(handlers, TOOLS_METHODS)
      ? { list: {}, cancel: {}, requests: { tools: { call: {} } } }
      : { list: {}, cancel: {} };
  }
  return capabilities;
}

// The methods each advertised capability requires, in a stable check order.
function requiredMethods(capabilities: ServerCapabilities): ReadonlyArray<{ capability: string; method: string }> {
  const required: Array<{ capability: string; method: string }> = [];
  const push = (capability: string, methods: readonly string[]): void => {
    for (const method of methods) {
      required.push({ capability, method });
    }
  };
  if (capabilities.tools !== undefined) {
    push('tools', TOOLS_METHODS);
  }
  if (capabilities.resources !== undefined) {
    push('resources', RESOURCES_READ_METHODS);
    if (capabilities.resources.subscribe === true) {
      push('resources', RESOURCES_SUBSCRIBE_METHODS);
    }
  }
  if (capabilities.prompts !== undefined) {
    push('prompts', PROMPTS_METHODS);
  }
  if (capabilities.completions !== undefined) {
    push('completions', COMPLETIONS_METHODS);
  }
  if (capabilities.tasks !== undefined) {
    push('tasks', TASKS_METHODS);
    if (capabilities.tasks.requests?.tools?.call !== undefined) {
      push('tasks', TOOLS_METHODS);
    }
  }
  return required;
}

/**
 * Validate that every advertised capability has its backing handler and return
 * the validated surface. Throws `PrimitiveRegistrationError` on the first missing
 * method (fail-closed, pre-connect) so an advertised-but-unbacked capability can
 * never reach a connected client.
 */
export function createPrimitiveRegistry(input: CreatePrimitiveRegistryInput): PrimitiveRegistry {
  const { handlers, capabilities } = input;
  for (const { capability, method } of requiredMethods(capabilities)) {
    if (!handlers.has(method)) {
      throw new PrimitiveRegistrationError(capability, method);
    }
  }
  return { capabilities };
}
