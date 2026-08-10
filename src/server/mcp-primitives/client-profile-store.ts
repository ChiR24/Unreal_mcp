// src/server/mcp-primitives/client-profile-store.ts
// Task 35: standalone, explicit-session store for per-session client capability
// profiles. It is intentionally decoupled from any central session/transport
// object — callers pass an explicit session id and MUST call clearSession on
// disconnect (Task 37 owns the lifecycle wiring). Nothing here leaks across
// sessions or into process/global state. Native mirror:
// Private/MCP/Primitives/McpClientProfileStore.{h,cpp}.

import type { SessionCapabilityProfile } from './session-capability-profile.js';

export class ClientProfileStore {
  private readonly profiles = new Map<string, SessionCapabilityProfile>();

  // Record (or replace) the profile for an explicit session id. An empty id is
  // rejected rather than silently keyed, so a blank/unauthenticated session can
  // never share one bucket.
  setSession(sessionId: string, profile: SessionCapabilityProfile): void {
    if (sessionId.length === 0) {
      throw new RangeError('ClientProfileStore.setSession requires a non-empty session id');
    }
    this.profiles.set(sessionId, profile);
  }

  getSession(sessionId: string): SessionCapabilityProfile | undefined {
    return this.profiles.get(sessionId);
  }

  hasSession(sessionId: string): boolean {
    return this.profiles.has(sessionId);
  }

  // Idempotent cleanup: clearing an absent (already-closed or stale) session is
  // a no-op, so a double disconnect never throws.
  clearSession(sessionId: string): void {
    this.profiles.delete(sessionId);
  }

  get size(): number {
    return this.profiles.size;
  }
}
