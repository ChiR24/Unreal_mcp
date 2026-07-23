// src/server/mcp-primitives/primitive-sources.ts
// Task 37: the live adapters that bridge the pure Tasks 31-36 primitives to the
// running server's state. Each is a thin, injected read-only view — no transport
// wiring of its own:
//   * the catalog-revision reader the coalescer polls (Task 36 C1),
//   * the session enabled-capability provider the completion primitive filters
//     against (Task 35 C3), and
//   * the prompt reference validator the prompt catalog fails closed on (Task 32).
// The single stdio session reads the global dynamic tool manager (its real
// configure target); injected/native sessions read the shared Task 36 overlay
// store, so one session's visibility can never be read as another's.

import { capabilityIndex } from '../gateway/gateway-capability-index.js';
import { dynamicToolManager } from '../../tools/dynamic/dynamic-tool-manager.js';
import { STDIO_SESSION_ID, sessionConfigureStore } from '../tool-registry-manage-tools.js';
import { NEW_RESOURCE_DEFINITIONS, RESOURCE_TEMPLATES } from '../../resources/resource-catalog.js';
import type { CatalogRevisionReader } from './catalog-revision-reader.js';
import type { EnabledCapabilityProvider } from './session-capability-profile.js';
import type { PromptReferenceValidator } from './prompts/index.js';
import { SUBSCRIBABLE_URIS } from './resource-revision.js';

// The six pre-existing static resources registered directly by ResourceRegistry
// (they predate the Task 31 catalog module and are not re-exported there).
const LEGACY_RESOURCE_URIS = [
  'ue://assets',
  'ue://actors',
  'ue://level',
  'ue://health',
  'ue://automation-bridge',
  'ue://version',
] as const;

// Every resource URI a workflow prompt step may reference: the legacy six, the
// Task 31 static additions, the read-only templates, and the subscribable
// allowlist. A prompt referencing anything outside this set fails closed.
const KNOWN_RESOURCE_URIS: ReadonlySet<string> = new Set<string>([
  ...LEGACY_RESOURCE_URIS,
  ...NEW_RESOURCE_DEFINITIONS.map((definition) => definition.uri),
  ...RESOURCE_TEMPLATES.map((template) => template.uriTemplate),
  ...SUBSCRIBABLE_URIS,
]);

// Whether a parent tool is enabled for the session, read from the session's real
// configure target: the global manager for stdio, the per-session overlay
// otherwise.
function isParentToolEnabled(sessionId: string, parentTool: string): boolean {
  return sessionId === STDIO_SESSION_ID
    ? dynamicToolManager.isToolEnabled(parentTool)
    : sessionConfigureStore.isToolEnabled(sessionId, parentTool);
}

/**
 * The catalog-state revision the coalescer polls to fold a configure visibility
 * change into one `resources/updated`. The stdio session's revision is the
 * global manager's (its configure target); other sessions read the Task 36
 * overlay. Advances only on an effective visibility change, never a no-op.
 */
export function buildCatalogRevisionReader(): CatalogRevisionReader {
  return {
    getCatalogStateRevision(sessionId: string): number {
      return sessionId === STDIO_SESSION_ID
        ? dynamicToolManager.getCatalogStateRevision()
        : sessionConfigureStore.getCatalogStateRevision(sessionId);
    },
  };
}

/**
 * The set of capability ids enabled for a session — a capability is enabled when
 * its parent tool is enabled. The completion primitive filters capability-scoped
 * slots by this set so a disabled tool's capabilities are never suggested.
 */
export function buildEnabledCapabilityProvider(): EnabledCapabilityProvider {
  return {
    enabledCapabilityIds(sessionId: string): ReadonlySet<string> {
      const enabled = new Set<string>();
      for (const record of capabilityIndex().records) {
        if (isParentToolEnabled(sessionId, record.routing.parentTool)) {
          enabled.add(record.id);
        }
      }
      return enabled;
    },
  };
}

/**
 * The reference validator the prompt catalog consults so a stale registry fails
 * closed instead of rendering a dangling capability id or resource uri. Reads the
 * generated canonical registry and the known resource surface, never the editor.
 */
export function buildPromptReferenceValidator(): PromptReferenceValidator {
  const index = capabilityIndex();
  return {
    capabilityExists(capabilityId: string): boolean {
      return index.byId.has(capabilityId) || index.byAlias.has(capabilityId);
    },
    resourceExists(resourceUri: string): boolean {
      return KNOWN_RESOURCE_URIS.has(resourceUri);
    },
  };
}
