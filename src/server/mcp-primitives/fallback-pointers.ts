// src/server/mcp-primitives/fallback-pointers.ts
// Task 35: bounded feature fallbacks. When a session lacks an MCP primitive
// (resources, prompts, completions, subscriptions, Tasks), it receives exactly
// ONE bounded, executable pointer — never a schema or knowledge dump. A session
// that HAS a SERVER-BACKED primitive is pointed at the native method instead;
// Tasks are never server-backed here, so a Tasks-declaring client still gets the
// bounded gateway pointer. Native mirror:
// Private/MCP/Primitives/McpSessionCapabilityProfile.h (McpFallbackPointerFor).

import type { ClientCapabilityProfile } from './session-capability-profile.js';

export const FALLBACK_PRIMITIVES = ['resources', 'prompts', 'completions', 'subscriptions', 'tasks'] as const;
export type FallbackPrimitive = (typeof FALLBACK_PRIMITIVES)[number];

// The primitives the SERVER backs with a real, registered native MCP method
// (primitive-handlers.ts REGISTERED_PRIMITIVE_METHODS + primitive-registry.ts
// ADVERTISED_SESSION_CAPABILITIES). `tasks` is excluded until Task 44 implements
// and advertises MCP Tasks, so a Tasks-declaring client is never pointed at a
// phantom native Tasks method the server never registers (it answers -32601).
export const SERVER_BACKED_PRIMITIVES = ['resources', 'prompts', 'completions', 'subscriptions'] as const;
export type ServerBackedPrimitive = (typeof SERVER_BACKED_PRIMITIVES)[number];

// A single pointer. `nextCall` is a tiny executable object: a native MCP method
// reference (capable client) or one bounded `unreal` gateway operation (fallback).
export interface FallbackPointer {
  readonly primitive: FallbackPrimitive;
  readonly mode: 'native' | 'gateway';
  readonly hint: string;
  readonly nextCall: Readonly<Record<string, unknown>>;
}

const NATIVE_METHOD: Record<ServerBackedPrimitive, string> = {
  resources: 'resources/list',
  prompts: 'prompts/list',
  completions: 'completion/complete',
  subscriptions: 'resources/subscribe',
};

const GATEWAY_OPERATION: Record<FallbackPrimitive, string> = {
  resources: 'search',
  prompts: 'describe',
  completions: 'search',
  subscriptions: 'search',
  tasks: 'execute',
};

const GATEWAY_HINT: Record<FallbackPrimitive, string> = {
  resources: 'No MCP resources on this client; discover the same state through the unreal gateway search operation.',
  prompts: 'No MCP prompts on this client; get the equivalent guided workflow through the unreal gateway describe operation.',
  completions: 'No MCP completions on this client; discover valid values through the unreal gateway search operation.',
  subscriptions: 'No subscriptions on this client; poll for changes by re-running the unreal gateway search operation.',
  tasks: 'MCP Tasks are not available on this server; run the action synchronously through the unreal gateway execute operation and read its receipt.',
};

const NATIVE_HINT: Record<ServerBackedPrimitive, string> = {
  resources: 'This client supports MCP resources; use the native resources methods.',
  prompts: 'This client supports MCP prompts; use the native prompts methods.',
  completions: 'This client supports MCP completions; use the native completion method.',
  subscriptions: 'This client supports resource subscriptions; use the native subscribe method.',
};

function assertNever(value: never): never {
  throw new Error(`Unhandled fallback primitive: ${String(value)}`);
}

function profileSupports(profile: ClientCapabilityProfile, primitive: FallbackPrimitive): boolean {
  switch (primitive) {
    case 'resources': return profile.hasResources;
    case 'prompts': return profile.hasPrompts;
    case 'completions': return profile.hasCompletions;
    case 'subscriptions': return profile.hasSubscriptions;
    case 'tasks': return profile.hasTasks;
    default: return assertNever(primitive);
  }
}

// Type-narrowing guard: a primitive takes the native branch only when the server
// backs it, which also proves the key is valid for NATIVE_METHOD/NATIVE_HINT.
function isServerBacked(primitive: FallbackPrimitive): primitive is ServerBackedPrimitive {
  return (SERVER_BACKED_PRIMITIVES as readonly FallbackPrimitive[]).includes(primitive);
}

// The bounded pointer for a single primitive under the given profile.
// Deterministic: identical profiles always yield identical pointers. Native mode
// requires BOTH a server-backed primitive AND a client that declares it; a
// Tasks-declaring client still falls to the bounded gateway because the server
// does not back Tasks.
export function fallbackPointerFor(profile: ClientCapabilityProfile, primitive: FallbackPrimitive): FallbackPointer {
  return isServerBacked(primitive) && profileSupports(profile, primitive)
    ? { primitive, mode: 'native', hint: NATIVE_HINT[primitive], nextCall: { method: NATIVE_METHOD[primitive] } }
    : { primitive, mode: 'gateway', hint: GATEWAY_HINT[primitive], nextCall: { operation: GATEWAY_OPERATION[primitive] } };
}

// One bounded gateway pointer for every primitive the client lacks, in stable
// declaration order. A fully capable client gets an empty list.
export function missingPrimitivePointers(profile: ClientCapabilityProfile): FallbackPointer[] {
  return FALLBACK_PRIMITIVES
    .filter((primitive) => !profileSupports(profile, primitive))
    .map((primitive) => fallbackPointerFor(profile, primitive));
}
