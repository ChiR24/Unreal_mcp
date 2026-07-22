// src/server/mcp-primitives/session-capability-profile.ts
// Task 35 primitive C3: adaptive per-session client-capability profile.
//
// Single TypeScript source of truth for the structural client profile shared by
// the client-profile store, the bounded fallback pointers, and (later) Task 33
// completions. It carries NO transport wiring and NO session-lifecycle edits; it
// derives six capability booleans STRUCTURALLY from the client's declared MCP
// capabilities and never inspects the client name or version. Native mirror:
// Private/MCP/Primitives/McpSessionCapabilityProfile.h.

import { isRecord } from '../../utils/validation/type-guards.js';

// The six structural capability booleans. Two clients that declare identical
// capabilities MUST get identical booleans regardless of brand: nothing here is
// derived from a client name or version string.
export interface ClientCapabilityProfile {
  readonly hasResources: boolean;
  readonly hasPrompts: boolean;
  readonly hasCompletions: boolean;
  readonly hasSubscriptions: boolean;
  readonly hasElicitation: boolean;
  readonly hasTasks: boolean;
}

// The profile of a client that declared nothing: every capability absent.
export const MINIMAL_PROFILE: ClientCapabilityProfile = Object.freeze({
  hasResources: false,
  hasPrompts: false,
  hasCompletions: false,
  hasSubscriptions: false,
  hasElicitation: false,
  hasTasks: false,
});

// Injected source of the capability IDs currently enabled for a session. Kept as
// an interface so C3 carries no registry/gateway wiring (mirrors C2's
// RevisionProvider). Task 33 consumes it to rank/filter completions by the
// session profile; Task 37 injects the live per-session visibility source.
export interface EnabledCapabilityProvider {
  enabledCapabilityIds(sessionId: string): ReadonlySet<string>;
}

// The default provider: a session with no injected source reports no enabled
// capabilities. Deterministic and side-effect free.
export const EMPTY_ENABLED_CAPABILITIES: EnabledCapabilityProvider = {
  enabledCapabilityIds: () => new Set<string>(),
};

// A session's capability view: the structural client booleans plus the enabled
// capability-id accessor Task 33 filters against. `enabledCapabilityIds` takes
// the session id explicitly and forwards to the injected provider, so this view
// never reaches into a central session object itself.
export class SessionCapabilityProfile implements ClientCapabilityProfile {
  readonly hasResources: boolean;
  readonly hasPrompts: boolean;
  readonly hasCompletions: boolean;
  readonly hasSubscriptions: boolean;
  readonly hasElicitation: boolean;
  readonly hasTasks: boolean;

  private readonly capabilities: EnabledCapabilityProvider;

  constructor(
    profile: ClientCapabilityProfile,
    capabilities: EnabledCapabilityProvider = EMPTY_ENABLED_CAPABILITIES,
  ) {
    this.hasResources = profile.hasResources;
    this.hasPrompts = profile.hasPrompts;
    this.hasCompletions = profile.hasCompletions;
    this.hasSubscriptions = profile.hasSubscriptions;
    this.hasElicitation = profile.hasElicitation;
    this.hasTasks = profile.hasTasks;
    this.capabilities = capabilities;
  }

  enabledCapabilityIds(sessionId: string): ReadonlySet<string> {
    return this.capabilities.enabledCapabilityIds(sessionId);
  }
}

// A capability key is "present" when declared either as a nested object or as a
// bare `true`. Anything else (absent, false, string, number) is treated as
// absent so a malformed value can never advertise a capability.
function isCapabilityPresent(value: unknown): boolean {
  return value === true || isRecord(value);
}

// Structural presence check: the key is present at the top level or nested under
// `experimental`. Reads only the declared capability object, never a name/version.
function hasStructuralKey(capabilities: Record<string, unknown>, key: string): boolean {
  if (isCapabilityPresent(capabilities[key])) return true;
  const experimental = capabilities.experimental;
  return isRecord(experimental) && isCapabilityPresent(experimental[key]);
}

// Derive the structural profile from the raw declared client capabilities. A
// non-object (undefined, array, primitive, malformed) yields the minimal
// profile rather than throwing, so a hostile or broken initialize cannot crash
// or falsely enable a capability.
export function parseClientCapabilityProfile(capabilities: unknown): ClientCapabilityProfile {
  if (!isRecord(capabilities)) return MINIMAL_PROFILE;

  const resources = capabilities.resources;
  const subscribeNested = isRecord(resources) && resources.subscribe === true;

  return {
    hasResources: hasStructuralKey(capabilities, 'resources'),
    hasPrompts: hasStructuralKey(capabilities, 'prompts'),
    hasCompletions: hasStructuralKey(capabilities, 'completions'),
    hasSubscriptions: subscribeNested || hasStructuralKey(capabilities, 'subscriptions'),
    hasElicitation: hasStructuralKey(capabilities, 'elicitation'),
    hasTasks: hasStructuralKey(capabilities, 'tasks'),
  };
}
