// src/server/mcp-primitives/primitive-wiring.ts
// Task 37: the single orchestrator that wires the Tasks 31-36 pure primitives
// into the running TypeScript stdio server. It owns ONE shared instance each of
// the Task 31 revision provider, the Task 34 subscription store + notification
// coalescer (via the driver), and the Task 35 client-profile store; it reuses the
// shared Task 36 configure store through the existing manage_tools seam. It:
//   * registers the subscribe/unsubscribe/prompts/completions handlers,
//   * fails closed pre-connect if an advertised capability lacks its handler,
//   * derives the structural client profile on `initialized` (no brand logic),
//   * folds a configure visibility change into one coalesced resources/updated,
//   * and drains every store + the flush timer on close through one idempotent
//     helper. Keeping this the only seam lets server-factory/server-setup stay thin.

import { Server } from '@modelcontextprotocol/sdk/server/index.js';

import { deriveClientCapabilityProfile } from '../tool-registry-client.js';
import {
  STDIO_SESSION_ID,
  clearManageToolsSession,
  setClientProfileResolver,
  setConfigureVisibilityHook,
} from '../tool-registry-manage-tools.js';
import { ClientProfileStore } from './client-profile-store.js';
import { createStaticCompletionSource } from './completions/index.js';
import { registerPrimitiveHandlers } from './primitive-handlers.js';
import { PrimitiveNotificationDriver } from './primitive-notifications.js';
import { ADVERTISED_SESSION_CAPABILITIES, createPrimitiveRegistry } from './primitive-registry.js';
import {
  buildCatalogRevisionReader,
  buildEnabledCapabilityProvider,
  buildPromptReferenceValidator,
} from './primitive-sources.js';
import { InMemoryRevisionProvider } from './resource-revision.js';
import { SessionCapabilityProfile } from './session-capability-profile.js';

// The read-only resource + tools methods registered by ResourceRegistry and
// ToolRegistry BEFORE this wiring runs. Recording them lets the fail-closed
// registry validate the COMPLETE advertised surface, not just the new methods.
const EXTERNAL_PRIMITIVE_METHODS = [
  'tools/list',
  'tools/call',
  'resources/list',
  'resources/templates/list',
  'resources/read',
  // Registered by the SDK's own Protocol constructor because server-factory
  // supplies a BoundedTaskStore. Listing them here is what makes the advertised
  // `tasks` capability fail closed alongside every other one; the wire proof
  // that they really answer lives in tests/unit/task-44.
  'tasks/get',
  'tasks/list',
  'tasks/cancel',
  'tasks/result',
] as const;

export interface WiredPrimitives {
  /** Idempotently drains every primitive store + the flush timer for this server. */
  dispose(): void;
}

export function wirePrimitives(server: Server): WiredPrimitives {
  const revisions = new InMemoryRevisionProvider();
  const clientProfileStore = new ClientProfileStore();
  const enabledCapabilities = buildEnabledCapabilityProvider();
  const driver = new PrimitiveNotificationDriver({
    server,
    revisions,
    catalog: buildCatalogRevisionReader(),
  });

  const deriveProfile = (sessionId: string): SessionCapabilityProfile => {
    const existing = clientProfileStore.getSession(sessionId);
    if (existing !== undefined) {
      return existing;
    }
    const declared = deriveClientCapabilityProfile(server.getClientCapabilities());
    const profile = new SessionCapabilityProfile(declared, enabledCapabilities);
    clientProfileStore.setSession(sessionId, profile);
    return profile;
  };

  const registered = registerPrimitiveHandlers({
    server,
    store: driver.store,
    resolveSessionId: () => STDIO_SESSION_ID,
    promptValidator: buildPromptReferenceValidator(),
    completionSource: createStaticCompletionSource(),
    resolveProfile: deriveProfile,
  });

  // Fail-closed, pre-connect: every advertised capability must be fully backed.
  const handlerTable = new Map<string, unknown>();
  for (const method of [...registered, ...EXTERNAL_PRIMITIVE_METHODS]) {
    handlerTable.set(method, true);
  }
  createPrimitiveRegistry({ handlers: handlerTable, capabilities: ADVERTISED_SESSION_CAPABILITIES });

  // Derive the structural profile once the client's declared capabilities land.
  const previousInitialized = server.oninitialized;
  server.oninitialized = (): void => {
    clientProfileStore.clearSession(STDIO_SESSION_ID);
    deriveProfile(STDIO_SESSION_ID);
    previousInitialized?.();
  };

  // A configure visibility change folds into one coalesced resources/updated for
  // any session subscribed to the catalog; syncCatalog no-ops when nothing moved.
  setConfigureVisibilityHook((sessionId) => {
    driver.syncCatalog(sessionId);
  });

  // configure get_status reports the derived structural client profile (six
  // booleans only — no class internals leak into the wire payload).
  setClientProfileResolver((sessionId) => {
    const profile = deriveProfile(sessionId);
    return {
      hasResources: profile.hasResources,
      hasPrompts: profile.hasPrompts,
      hasCompletions: profile.hasCompletions,
      hasSubscriptions: profile.hasSubscriptions,
      hasElicitation: profile.hasElicitation,
      hasTasks: profile.hasTasks,
    };
  });

  const release = (): void => {
    driver.releaseSession(STDIO_SESSION_ID);
    clientProfileStore.clearSession(STDIO_SESSION_ID);
    clearManageToolsSession(STDIO_SESSION_ID);
    driver.dispose();
  };

  const previousOnClose = server.onclose;
  server.onclose = (): void => {
    release();
    previousOnClose?.();
  };

  return { dispose: release };
}
