// src/server/mcp-primitives/session-configure-store.ts
// Task 36 primitive: explicit-session, policy-bounded, revisioned configure
// overlay. WRITE side of the C1 catalog-revision contract
// (catalog-revision-reader.ts); implements CatalogRevisionReader.
//
// Each session gets an INDEPENDENT overlay of tool/category visibility,
// preferences, and bounded limits, seeded lazily from the immutable generated
// catalog (the same seed the global DynamicToolManager reads). It NEVER mutates
// that seed and holds NO transport/session-lifecycle wiring: the session id is
// always passed in explicitly (Task 37 supplies native ids). The revision
// advances exactly once per effective visibility batch and never for a no-op, a
// rejected protected mutation, or a limit/preference-only change. Native mirror:
// Private/MCP/DynamicTools/McpSessionConfigureStore.{h,cpp}.

import { consolidatedToolDefinitions, type ToolDefinition } from '../../tools/catalog/consolidated-tool-definitions.js';
import { countEnabledTools, isToolStateEnabled, listCategoryStates } from '../../tools/dynamic/dynamic-tool-queries.js';
import {
  disableCategoryState,
  disableToolStates,
  enableCategoryState,
  enableToolStates,
  resetToolStates,
} from '../../tools/dynamic/dynamic-tool-state-operations.js';
import type {
  CategoryDisableResult,
  CategoryEnableResult,
  CategoryState,
  DisableToolsResult,
  EnableToolsResult,
  ToolCategory,
  ToolState,
} from '../../tools/dynamic/dynamic-tool-types.js';
import { BASELINE_CATALOG_STATE_REVISION, type CatalogRevisionReader } from './catalog-revision-reader.js';

/** Closed, policy-bounded set of numeric session limits. Unknown keys are rejected. */
export const LIMIT_BOUNDS: Readonly<Record<string, { readonly min: number; readonly max: number }>> = Object.freeze({
  maxResults: { min: 1, max: 1000 },
  maxDepth: { min: 1, max: 32 },
  pageSize: { min: 1, max: 200 },
});

/** Preference bounds: at most this many keys, each value at most this many chars. */
export const MAX_PREFERENCE_KEYS = 16;
export const MAX_PREFERENCE_VALUE_LENGTH = 256;

export interface SetLimitResult {
  accepted: boolean;
  key: string;
  value: number;
  clamped: boolean;
}

export interface SetPreferenceResult {
  accepted: boolean;
  key: string;
}

export interface SessionConfigureStatus {
  totalTools: number;
  enabledTools: number;
  disabledTools: number;
  categories: CategoryState[];
  catalogStateRevision: number;
  preferences: Record<string, string>;
  limits: Record<string, number>;
}

interface SessionOverlay {
  toolStates: Map<string, ToolState>;
  categoryStates: Map<ToolCategory, CategoryState>;
  catalogStateRevision: number;
  preferences: Map<string, string>;
  limits: Map<string, number>;
}

/**
 * Per-session configure overlay. Visibility mutations reuse the same pure
 * operations as the global manager (so protected/core/no-op rules stay identical)
 * but on this session's own maps; the global singleton is never touched.
 */
export class SessionConfigureStore implements CatalogRevisionReader {
  private readonly overlays = new Map<string, SessionOverlay>();
  private readonly seed: () => readonly ToolDefinition[];

  constructor(seed: () => readonly ToolDefinition[] = () => consolidatedToolDefinitions) {
    this.seed = seed;
  }

  getCatalogStateRevision(sessionId: string): number {
    return this.overlays.get(sessionId)?.catalogStateRevision ?? BASELINE_CATALOG_STATE_REVISION;
  }

  hasSession(sessionId: string): boolean {
    return this.overlays.has(sessionId);
  }

  /** Drop a session's overlay; a later access reseeds it pristine. */
  clearSession(sessionId: string): boolean {
    return this.overlays.delete(sessionId);
  }

  enableTools(sessionId: string, toolNames: string[]): EnableToolsResult {
    const overlay = this.overlay(sessionId);
    return this.applyMutation(overlay, () => enableToolStates(overlay.toolStates, overlay.categoryStates, toolNames));
  }

  disableTools(sessionId: string, toolNames: string[]): DisableToolsResult {
    const overlay = this.overlay(sessionId);
    return this.applyMutation(overlay, () => disableToolStates(overlay.toolStates, overlay.categoryStates, toolNames));
  }

  enableCategory(sessionId: string, category: ToolCategory): CategoryEnableResult {
    const overlay = this.overlay(sessionId);
    return this.applyMutation(overlay, () => enableCategoryState(overlay.toolStates, overlay.categoryStates, category));
  }

  disableCategory(sessionId: string, category: ToolCategory): CategoryDisableResult {
    const overlay = this.overlay(sessionId);
    return this.applyMutation(overlay, () => disableCategoryState(overlay.toolStates, overlay.categoryStates, category));
  }

  reset(sessionId: string): { enabled: number } {
    const overlay = this.overlay(sessionId);
    const count = this.applyMutation(overlay, () => resetToolStates(overlay.toolStates, overlay.categoryStates));
    return { enabled: count };
  }

  /** Set a bounded numeric limit. Non-visibility: never advances the revision. */
  setLimit(sessionId: string, key: string, value: number): SetLimitResult {
    const bounds = LIMIT_BOUNDS[key];
    if (bounds === undefined || !Number.isFinite(value)) {
      return { accepted: false, key, value, clamped: false };
    }
    const clampedValue = Math.min(bounds.max, Math.max(bounds.min, Math.trunc(value)));
    this.overlay(sessionId).limits.set(key, clampedValue);
    return { accepted: true, key, value: clampedValue, clamped: clampedValue !== value };
  }

  /** Set a bounded string preference. Non-visibility: never advances the revision. */
  setPreference(sessionId: string, key: string, value: string): SetPreferenceResult {
    if (typeof value !== 'string' || value.length > MAX_PREFERENCE_VALUE_LENGTH) {
      return { accepted: false, key };
    }
    const preferences = this.overlay(sessionId).preferences;
    if (!preferences.has(key) && preferences.size >= MAX_PREFERENCE_KEYS) {
      return { accepted: false, key };
    }
    preferences.set(key, value);
    return { accepted: true, key };
  }

  isToolEnabled(sessionId: string, toolName: string): boolean {
    const overlay = this.overlay(sessionId);
    return isToolStateEnabled(overlay.toolStates, overlay.categoryStates, toolName);
  }

  listTools(sessionId: string): ToolState[] {
    return Array.from(this.overlay(sessionId).toolStates.values());
  }

  listCategories(sessionId: string): CategoryState[] {
    const overlay = this.overlay(sessionId);
    return listCategoryStates(overlay.toolStates, overlay.categoryStates);
  }

  getStatus(sessionId: string): SessionConfigureStatus {
    const overlay = this.overlay(sessionId);
    const tools = Array.from(overlay.toolStates.values());
    const enabledCount = countEnabledTools(overlay.toolStates, overlay.categoryStates);
    return {
      totalTools: tools.length,
      enabledTools: enabledCount,
      disabledTools: tools.length - enabledCount,
      categories: listCategoryStates(overlay.toolStates, overlay.categoryStates),
      catalogStateRevision: overlay.catalogStateRevision,
      preferences: Object.fromEntries(overlay.preferences),
      limits: Object.fromEntries(overlay.limits),
    };
  }

  // Only the flags isToolStateEnabled() reads, so a limit/preference change or a
  // reset that only rewrites the enabledCount cache is never seen as a move.
  private static fingerprint(overlay: SessionOverlay): string {
    const tools = Array.from(overlay.toolStates.values(), state => `${state.name}=${state.enabled ? 1 : 0}`);
    const categories = Array.from(overlay.categoryStates.values(), cat => `${cat.name}=${cat.enabled ? 1 : 0}`);
    return `${tools.join(',')}|${categories.join(',')}`;
  }

  private applyMutation<T>(overlay: SessionOverlay, mutate: () => T): T {
    const before = SessionConfigureStore.fingerprint(overlay);
    const result = mutate();
    if (SessionConfigureStore.fingerprint(overlay) !== before) {
      overlay.catalogStateRevision++;
    }
    return result;
  }

  private overlay(sessionId: string): SessionOverlay {
    let overlay = this.overlays.get(sessionId);
    if (overlay === undefined) {
      overlay = this.buildOverlay();
      this.overlays.set(sessionId, overlay);
    }
    return overlay;
  }

  private buildOverlay(): SessionOverlay {
    const toolStates = new Map<string, ToolState>();
    const categoryStates = new Map<ToolCategory, CategoryState>();
    for (const def of this.seed()) {
      const category: ToolCategory = def.category ?? 'utility';
      toolStates.set(def.name, { name: def.name, category, enabled: true, description: def.description });
      let catState = categoryStates.get(category);
      if (catState === undefined) {
        catState = { name: category, enabled: true, toolCount: 0, enabledCount: 0 };
        categoryStates.set(category, catState);
      }
      catState.toolCount++;
      catState.enabledCount++;
    }
    return { toolStates, categoryStates, catalogStateRevision: 0, preferences: new Map(), limits: new Map() };
  }
}
