// src/server/mcp-primitives/catalog-revision-reader.ts
// Task 36 primitive C1: explicit-session catalog state revision reader.
//
// Single TypeScript source of truth for the READ side of per-session runtime
// catalog visibility revisions. It carries NO transport wiring and NO
// session-lifecycle edits; it defines only the injected, session-scoped read
// contract that the resource surface (Task 31) and the subscription surface
// (Task 34) consume to tag payloads with the caller's current catalog state.
// The revisioned WRITE side is the SessionConfigureStore
// (session-configure-store.ts). Native mirror:
// Private/MCP/Primitives/IMcpCatalogRevisionReader.h.

/**
 * The revision every session reports before any effective visibility mutation.
 * It is deliberately a fixed baseline (not read from any global counter), so an
 * unconfigured or unknown session is well defined without a global fallback.
 */
export const BASELINE_CATALOG_STATE_REVISION = 0;

/**
 * Read-only, per-session catalog state revision source. The revision is the
 * monotonic counter that advances exactly once per effective visibility
 * mutation batch for a session, and never for a no-op, a rejected protected
 * mutation, or a limit/preference-only (non-visibility) change.
 *
 * The session id is REQUIRED and has no default: there is deliberately no
 * global/optional fallback and no no-arg overload, so one session's revision can
 * never be read as if it were another's. An unknown session reads
 * BASELINE_CATALOG_STATE_REVISION rather than throwing, so a reader on a
 * not-yet-configured session stays well defined.
 */
export interface CatalogRevisionReader {
  getCatalogStateRevision(sessionId: string): number;
}
