// src/server/mcp-primitives/fallback-pointers.ts
// Task 35: bounded feature fallbacks. When a session lacks an MCP primitive
// (resources, prompts, completions, subscriptions, Tasks), it receives exactly
// ONE bounded, executable pointer — never a schema or knowledge dump. A session
// that HAS the primitive is pointed at the native method instead. Native mirror:
// Private/MCP/Primitives/McpSessionCapabilityProfile.h (McpFallbackPointerFor).

import type { ClientCapabilityProfile } from './session-capability-profile.js';

export const FALLBACK_PRIMITIVES = ['resources', 'prompts', 'completions', 'subscriptions', 'tasks'] as const;
export type FallbackPrimitive = (typeof FALLBACK_PRIMITIVES)[number];

// A single pointer. `nextCall` is a tiny executable object: a native MCP method
// reference (capable client) or one bounded `unreal` gateway operation (fallback).
export interface FallbackPointer {
  readonly primitive: FallbackPrimitive;
  readonly mode: 'native' | 'gateway';
  readonly hint: string;
  readonly nextCall: Readonly<Record<string, unknown>>;
}

const NATIVE_METHOD: Record<FallbackPrimitive, string> = {
  resources: 'resources/list',
  prompts: 'prompts/list',
  completions: 'completion/complete',
  subscriptions: 'resources/subscribe',
  tasks: 'tasks/list',
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
  tasks: 'No MCP tasks on this client; run the action synchronously through the unreal gateway execute operation and read its receipt.',
};

const NATIVE_HINT: Record<FallbackPrimitive, string> = {
  resources: 'This client supports MCP resources; use the native resources methods.',
  prompts: 'This client supports MCP prompts; use the native prompts methods.',
  completions: 'This client supports MCP completions; use the native completion method.',
  subscriptions: 'This client supports resource subscriptions; use the native subscribe method.',
  tasks: 'This client supports MCP tasks; use the native tasks methods.',
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

// The bounded pointer for a single primitive under the given profile.
// Deterministic: identical profiles always yield identical pointers.
export function fallbackPointerFor(profile: ClientCapabilityProfile, primitive: FallbackPrimitive): FallbackPointer {
  return profileSupports(profile, primitive)
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
