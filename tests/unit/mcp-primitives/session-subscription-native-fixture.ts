// tests/unit/mcp-primitives/session-subscription-native-fixture.ts
//
// Task 38 lane C — EXECUTABLE native-behavior fixture (oracle) for cross-transport
// subscription / notification-coalescing / resource-revision / configure-overlay
// parity.
//
// This is NOT a source-text contract. It is a deterministic, executable,
// INDEPENDENT reimplementation of the NATIVE C++ plugin's *normalized* behavior.
// A Vitest suite drives it through the same scenarios as the TypeScript production
// primitives and compares SEMANTICS (not stdio/HTTP/SSE framing), so the parity
// proof is executable behavior — never grepped C++ text. Every rule below is
// grounded in a native source location verified first-hand against the plugin
// tree at HEAD ea607fdc.
//
// GROUNDING MAP (plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP):
//   Subscribable URI allowlist (9)      Primitives/McpResourceRevision.h:36-47,55-57
//   resources/updated WIRE params (uri) Transport/McpNativeTransportPrimitiveNotifications.cpp:12-18
//   coalescer payload (uri/rev/kind)    Primitives/McpNotificationCoalescer.h:24-28
//   change kinds updated|invalid|removed Primitives/McpNotificationCoalescer.cpp:12
//   fixed window, latest kind wins      Primitives/McpNotificationCoalescer.cpp:59-67
//   monotonic + late-timer suppression  Primitives/McpNotificationCoalescer.cpp:118-131
//   session configure overlay (port)    DynamicTools/McpSessionConfigureStore.cpp (all-enabled seed :73)
//   protected tools / core category     DynamicTools/McpSessionConfigureStore.cpp:26-34
//   fingerprint effective-change rev     DynamicTools/McpSessionConfigureStore.cpp:38-51,126,153,197,222
//   reset restores ALL enabled          DynamicTools/McpSessionConfigureStore.cpp:206-228
//   global/default seed formula         DynamicTools/McpDynamicToolManager.cpp:17,28
//     bool bEnabled = bLoadAllTools || (Category == TEXT("core"));
//   PRODUCTION DEFAULT PARITY (no divergence): the native GLOBAL/default manager's
//   bLoadAllTools is wired from the bLoadAllToolsOnStart project setting, whose
//   default is TRUE (Public/McpAutomationBridgeSettings.h:276), threaded through
//   Core/Subsystem/McpAutomationBridgeSubsystemLifecycle.cpp:224 ->
//   Transport/McpNativeTransportLifecycle.cpp:109 -> Initialize(). At that default
//   the seed is all-enabled, matching the TS global manager (unconditionally
//   all-enabled: src/tools/dynamic/dynamic-tool-manager.ts:193). The C++ signature
//   defaults (Initialize/Start bLoadAllTools=false) are defensive fallbacks the
//   wired production caller never uses. CONFIGURABLE CORE-ONLY PROFILE: setting
//   bLoadAllToolsOnStart=false seeds core-only (only the `core` category). The
//   native PER-SESSION store (SeedFrom :53-78) is always all-enabled regardless.
//
/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import type { ToolDefinition } from '../../../src/tools/catalog/consolidated-tool-definitions.js';

// ---------------------------------------------------------------------------
// Grounded native constants (independent of the TS modules under test).
// ---------------------------------------------------------------------------

/** McpResourceRevision.h:36-47 — the closed native subscribable-URI allowlist. */
export const NATIVE_SUBSCRIBABLE_URIS = [
  'ue://capability/catalog',
  'ue://project',
  'ue://level',
  'ue://selection',
  'ue://asset-registry',
  'ue://pie',
  'ue://build',
  'ue://render',
  'ue://logs',
] as const;

/** McpNotificationCoalescer.cpp:12 — the closed native change-kind set. */
export const NATIVE_RESOURCE_CHANGE_KINDS = ['updated', 'invalidated', 'removed'] as const;

/** McpNotificationCoalescer.h default window; matches TS DEFAULT_COALESCE_WINDOW_MS. */
export const NATIVE_DEFAULT_COALESCE_WINDOW_MS = 50;

/** Native per-session cap mirror of the TS store (McpSubscriptionStore mirror). */
export const NATIVE_MAX_SUBSCRIPTIONS_PER_SESSION = 9;

/** McpSessionConfigureStore.cpp:26-34 — protected tools and category. */
export const NATIVE_PROTECTED_TOOL_NAMES: ReadonlySet<string> = new Set(['manage_tools', 'inspect']);
export const NATIVE_PROTECTED_CATEGORY = 'core';

/**
 * The production default for the native global/default manager's bLoadAllTools,
 * wired from the `bLoadAllToolsOnStart` project setting whose default is TRUE
 * (Public/McpAutomationBridgeSettings.h:276), NOT the defensive `=false` C++
 * signature fallback (McpDynamicToolManager.cpp:17) the wired production caller
 * never exercises. At this default the native seed is all-enabled and matches the
 * TS global manager. Pass `false` explicitly to model the config-gated core-only
 * profile.
 */
export const NATIVE_DEFAULT_LOAD_ALL_TOOLS = true;

/** ue://capability/catalog is the catalog subscribable URI on both surfaces. */
export const NATIVE_CATALOG_SUBSCRIPTION_URI = 'ue://capability/catalog';

const NATIVE_SUBSCRIBABLE_SET: ReadonlySet<string> = new Set(NATIVE_SUBSCRIBABLE_URIS);
const NATIVE_CHANGE_KIND_SET: ReadonlySet<string> = new Set(NATIVE_RESOURCE_CHANGE_KINDS);

/** McpIsSubscribableUri (McpResourceRevision.h:55-57). */
export function nativeIsSubscribableUri(uri: string): boolean {
  return NATIVE_SUBSCRIBABLE_SET.has(uri);
}

/** IsKnownChangeKind (McpNotificationCoalescer.cpp:10-13). */
export function nativeIsChangeKind(kind: string): boolean {
  return NATIVE_CHANGE_KIND_SET.has(kind);
}

// ---------------------------------------------------------------------------
// Normalized payloads.
// ---------------------------------------------------------------------------

/** McpNotificationCoalescer.h:24-28 — the internal coalesced payload (3 fields). */
export interface NativeResourceUpdatedPayload {
  readonly uri: string;
  readonly revision: number;
  readonly changeKind: string;
}

/**
 * McpNativeTransportPrimitiveNotifications.cpp:14-18 — the resources/updated WIRE
 * params are URI-only; revision and change kind never leak. The client re-reads
 * the resource to observe the new revision.
 */
export function nativeWireParams(payload: NativeResourceUpdatedPayload): { uri: string } {
  return { uri: payload.uri };
}

// ---------------------------------------------------------------------------
// Native subscription store (independent reimplementation of McpSubscriptionStore).
// ---------------------------------------------------------------------------

export type NativeSubscribeRejectReason = 'NOT_SUBSCRIBABLE' | 'INVALID_SESSION';

export interface NativeSubscribeResult {
  readonly accepted: boolean;
  readonly alreadySubscribed: boolean;
  readonly evicted: string | null;
  readonly reason: NativeSubscribeRejectReason | null;
}

export type NativeReleaseHook = (sessionId: string, uri: string) => void;

export class NativeSubscriptionStore {
  private readonly sessions = new Map<string, Set<string>>();
  private readonly maxPerSession: number;
  private readonly onRelease: NativeReleaseHook | null;

  constructor(options: { maxPerSession?: number; onRelease?: NativeReleaseHook } = {}) {
    this.maxPerSession = options.maxPerSession ?? NATIVE_MAX_SUBSCRIPTIONS_PER_SESSION;
    this.onRelease = options.onRelease ?? null;
  }

  subscribe(sessionId: string, uri: string): NativeSubscribeResult {
    if (typeof sessionId !== 'string' || sessionId.trim().length === 0) {
      return { accepted: false, alreadySubscribed: false, evicted: null, reason: 'INVALID_SESSION' };
    }
    if (!nativeIsSubscribableUri(uri)) {
      return { accepted: false, alreadySubscribed: false, evicted: null, reason: 'NOT_SUBSCRIBABLE' };
    }
    let set = this.sessions.get(sessionId);
    if (set === undefined) {
      set = new Set<string>();
      this.sessions.set(sessionId, set);
    }
    if (set.has(uri)) {
      return { accepted: true, alreadySubscribed: true, evicted: null, reason: null };
    }
    let evicted: string | null = null;
    if (set.size >= this.maxPerSession) {
      evicted = set.values().next().value ?? null;
      if (evicted !== null) {
        set.delete(evicted);
        this.onRelease?.(sessionId, evicted);
      }
    }
    set.add(uri);
    return { accepted: true, alreadySubscribed: false, evicted, reason: null };
  }

  unsubscribe(sessionId: string, uri: string): boolean {
    const set = this.sessions.get(sessionId);
    if (set === undefined || !nativeIsSubscribableUri(uri) || !set.has(uri)) {
      return false;
    }
    set.delete(uri);
    this.onRelease?.(sessionId, uri);
    if (set.size === 0) {
      this.sessions.delete(sessionId);
    }
    return true;
  }

  isSubscribed(sessionId: string, uri: string): boolean {
    return nativeIsSubscribableUri(uri) && (this.sessions.get(sessionId)?.has(uri) ?? false);
  }

  subscriptions(sessionId: string): readonly string[] {
    const set = this.sessions.get(sessionId);
    return set === undefined ? [] : [...set];
  }

  count(sessionId: string): number {
    return this.sessions.get(sessionId)?.size ?? 0;
  }

  hasSession(sessionId: string): boolean {
    return this.sessions.has(sessionId);
  }

  sessionCount(): number {
    return this.sessions.size;
  }

  sessionsSubscribedTo(uri: string): string[] {
    if (!nativeIsSubscribableUri(uri)) {
      return [];
    }
    const result: string[] = [];
    for (const [sessionId, set] of this.sessions) {
      if (set.has(uri)) {
        result.push(sessionId);
      }
    }
    return result;
  }

  clearSession(sessionId: string): number {
    const set = this.sessions.get(sessionId);
    if (set === undefined) {
      return 0;
    }
    let released = 0;
    for (const uri of set) {
      this.onRelease?.(sessionId, uri);
      released++;
    }
    this.sessions.delete(sessionId);
    return released;
  }
}

// ---------------------------------------------------------------------------
// Native notification coalescer (independent reimplementation).
// ---------------------------------------------------------------------------

export type NativeRevisionSource = (uri: string) => number;
export type NativeNotificationSink = (sessionId: string, payload: NativeResourceUpdatedPayload) => void;
export type NativeClock = () => number;

interface NativePending {
  readonly sessionId: string;
  readonly uri: string;
  changeKind: string;
  readonly dueAt: number;
}

const KEY_SEP = '\u0000';

export class NativeNotificationCoalescer {
  private readonly pending = new Map<string, NativePending>();
  private readonly lastEmitted = new Map<string, number>();
  private readonly windowMs: number;

  constructor(
    private readonly store: NativeSubscriptionStore,
    private readonly revisionSource: NativeRevisionSource,
    private readonly sink: NativeNotificationSink,
    private readonly clock: NativeClock,
    windowMs: number = NATIVE_DEFAULT_COALESCE_WINDOW_MS,
  ) {
    this.windowMs = windowMs;
  }

  /** McpNotificationCoalescer.cpp:42-70 — reject unknown kind / not-subscribed; coalesce keeps dueAt, latest kind wins. */
  recordChange(sessionId: string, uri: string, changeKind = 'updated'): { recorded: boolean; reason: string | null } {
    if (!nativeIsChangeKind(changeKind)) {
      return { recorded: false, reason: 'INVALID_CHANGE_KIND' };
    }
    if (!this.store.isSubscribed(sessionId, uri)) {
      return { recorded: false, reason: 'NOT_SUBSCRIBED' };
    }
    const key = NativeNotificationCoalescer.key(sessionId, uri);
    const existing = this.pending.get(key);
    if (existing !== undefined) {
      existing.changeKind = changeKind;
    } else {
      this.pending.set(key, { sessionId, uri, changeKind, dueAt: this.clock() + this.windowMs });
    }
    return { recorded: true, reason: null };
  }

  recordGlobalChange(uri: string, changeKind = 'updated'): number {
    let recorded = 0;
    for (const sessionId of this.store.sessionsSubscribedTo(uri)) {
      if (this.recordChange(sessionId, uri, changeKind).recorded) {
        recorded++;
      }
    }
    return recorded;
  }

  /** McpNotificationCoalescer.cpp:100-140 — dueAt<=now, re-check subscription, monotonic suppression. */
  flushDue(now: number = this.clock()): number {
    let emitted = 0;
    for (const [key, change] of this.pending) {
      if (change.dueAt > now) {
        continue;
      }
      this.pending.delete(key);
      if (!this.store.isSubscribed(change.sessionId, change.uri)) {
        continue;
      }
      const revision = this.revisionSource(change.uri);
      const previous = this.lastEmitted.get(key);
      if (previous !== undefined && revision < previous) {
        continue;
      }
      this.lastEmitted.set(key, revision);
      this.sink(change.sessionId, { uri: change.uri, revision, changeKind: change.changeKind });
      emitted++;
    }
    return emitted;
  }

  nextDueAt(): number | null {
    let earliest: number | null = null;
    for (const change of this.pending.values()) {
      if (earliest === null || change.dueAt < earliest) {
        earliest = change.dueAt;
      }
    }
    return earliest;
  }

  pendingCount(sessionId?: string): number {
    if (sessionId === undefined) {
      return this.pending.size;
    }
    let count = 0;
    for (const change of this.pending.values()) {
      if (change.sessionId === sessionId) {
        count++;
      }
    }
    return count;
  }

  dropPending(sessionId: string, uri: string): void {
    const key = NativeNotificationCoalescer.key(sessionId, uri);
    this.pending.delete(key);
    this.lastEmitted.delete(key);
  }

  clearSession(sessionId: string): void {
    const prefix = `${sessionId}${KEY_SEP}`;
    for (const key of [...this.pending.keys()]) {
      if (key.startsWith(prefix)) {
        this.pending.delete(key);
      }
    }
    for (const key of [...this.lastEmitted.keys()]) {
      if (key.startsWith(prefix)) {
        this.lastEmitted.delete(key);
      }
    }
  }

  private static key(sessionId: string, uri: string): string {
    return `${sessionId}${KEY_SEP}${uri}`;
  }
}

// ---------------------------------------------------------------------------
// Native per-session configure overlay (faithful port; all-enabled seed).
// McpSessionConfigureStore.cpp — seeds every tool enabled (:73), fingerprint
// effective-change revision, protected tools/category, reset-to-all-enabled.
// ---------------------------------------------------------------------------

interface NativeToolState {
  name: string;
  category: string;
  enabled: boolean;
}

interface NativeOverlay {
  toolStates: Map<string, NativeToolState>;
  categoryStates: Map<string, { name: string; enabled: boolean }>;
  catalogStateRevision: number;
}

export interface NativeConfigureStatus {
  totalTools: number;
  enabledTools: number;
  disabledTools: number;
  catalogStateRevision: number;
}

export class NativeSessionConfigureStore {
  private readonly overlays = new Map<string, NativeOverlay>();

  constructor(private readonly seed: () => readonly ToolDefinition[]) {}

  getCatalogStateRevision(sessionId: string): number {
    return this.overlays.get(sessionId)?.catalogStateRevision ?? 0;
  }

  hasSession(sessionId: string): boolean {
    return this.overlays.has(sessionId);
  }

  clearSession(sessionId: string): boolean {
    return this.overlays.delete(sessionId);
  }

  enableTools(sessionId: string, toolNames: string[]): { enabled: string[]; notFound: string[] } {
    const overlay = this.overlay(sessionId);
    return this.applyMutation(overlay, () => {
      const enabled: string[] = [];
      const notFound: string[] = [];
      for (const name of toolNames) {
        const tool = overlay.toolStates.get(name);
        if (tool === undefined) {
          notFound.push(name);
          continue;
        }
        const cat = overlay.categoryStates.get(tool.category);
        if (cat !== undefined) cat.enabled = true;
        tool.enabled = true;
        enabled.push(name);
      }
      return { enabled, notFound };
    });
  }

  disableTools(sessionId: string, toolNames: string[]): { disabled: string[]; notFound: string[]; protected: string[] } {
    const overlay = this.overlay(sessionId);
    return this.applyMutation(overlay, () => {
      const disabled: string[] = [];
      const notFound: string[] = [];
      const prot: string[] = [];
      for (const name of toolNames) {
        if (NATIVE_PROTECTED_TOOL_NAMES.has(name)) {
          prot.push(name);
          continue;
        }
        const tool = overlay.toolStates.get(name);
        if (tool === undefined) {
          notFound.push(name);
          continue;
        }
        tool.enabled = false;
        disabled.push(name);
      }
      return { disabled, notFound, protected: prot };
    });
  }

  disableCategory(sessionId: string, category: string): { disabled: string[]; protected: string[] } {
    const overlay = this.overlay(sessionId);
    return this.applyMutation(overlay, () => {
      const disabled: string[] = [];
      const prot: string[] = [];
      const isAll = category === 'all';
      if (!isAll && category === NATIVE_PROTECTED_CATEGORY) {
        for (const tool of overlay.toolStates.values()) {
          if (tool.category === category && NATIVE_PROTECTED_TOOL_NAMES.has(tool.name)) prot.push(tool.name);
        }
        return { disabled, protected: prot };
      }
      for (const tool of overlay.toolStates.values()) {
        if (!isAll && tool.category !== category) continue;
        if (NATIVE_PROTECTED_TOOL_NAMES.has(tool.name) || tool.category === NATIVE_PROTECTED_CATEGORY) {
          prot.push(tool.name);
          continue;
        }
        if (tool.enabled) {
          tool.enabled = false;
          disabled.push(tool.name);
        }
      }
      for (const cat of overlay.categoryStates.values()) {
        if (cat.name === NATIVE_PROTECTED_CATEGORY) continue;
        if (isAll || cat.name === category) cat.enabled = false;
      }
      return { disabled, protected: prot };
    });
  }

  reset(sessionId: string): { enabled: number } {
    const overlay = this.overlay(sessionId);
    return this.applyMutation(overlay, () => {
      let changed = 0;
      for (const tool of overlay.toolStates.values()) {
        if (!tool.enabled) {
          tool.enabled = true;
          changed++;
        }
      }
      for (const cat of overlay.categoryStates.values()) {
        cat.enabled = true;
      }
      return { enabled: changed };
    });
  }

  isToolEnabled(sessionId: string, toolName: string): boolean {
    const overlay = this.overlay(sessionId);
    const tool = overlay.toolStates.get(toolName);
    if (tool === undefined) return false;
    const cat = overlay.categoryStates.get(tool.category);
    return tool.enabled && (cat?.enabled ?? true);
  }

  enabledToolNames(sessionId: string): string[] {
    const overlay = this.overlay(sessionId);
    return [...overlay.toolStates.keys()].filter((name) => this.isToolEnabled(sessionId, name)).sort();
  }

  getStatus(sessionId: string): NativeConfigureStatus {
    const overlay = this.overlay(sessionId);
    const total = overlay.toolStates.size;
    const enabled = this.enabledToolNames(sessionId).length;
    return {
      totalTools: total,
      enabledTools: enabled,
      disabledTools: total - enabled,
      catalogStateRevision: overlay.catalogStateRevision,
    };
  }

  private applyMutation<T>(overlay: NativeOverlay, mutate: () => T): T {
    const before = NativeSessionConfigureStore.fingerprint(overlay);
    const result = mutate();
    if (NativeSessionConfigureStore.fingerprint(overlay) !== before) {
      overlay.catalogStateRevision++;
    }
    return result;
  }

  private static fingerprint(overlay: NativeOverlay): string {
    const tools = [...overlay.toolStates.values()].map((s) => `t:${s.name}=${s.enabled ? 1 : 0}`);
    const cats = [...overlay.categoryStates.values()].map((c) => `c:${c.name}=${c.enabled ? 1 : 0}`);
    return [...tools, ...cats].sort().join(',');
  }

  private overlay(sessionId: string): NativeOverlay {
    let overlay = this.overlays.get(sessionId);
    if (overlay === undefined) {
      overlay = this.buildOverlay();
      this.overlays.set(sessionId, overlay);
    }
    return overlay;
  }

  private buildOverlay(): NativeOverlay {
    const toolStates = new Map<string, NativeToolState>();
    const categoryStates = new Map<string, { name: string; enabled: boolean }>();
    for (const def of this.seed()) {
      const category = def.category ?? 'utility';
      toolStates.set(def.name, { name: def.name, category, enabled: true }); // SeedFrom :73 all-enabled
      if (!categoryStates.has(category)) {
        categoryStates.set(category, { name: category, enabled: true });
      }
    }
    return { toolStates, categoryStates, catalogStateRevision: 0 };
  }
}

// ---------------------------------------------------------------------------
// Native GLOBAL/default manager seed — the DIVERGENCE.
// McpDynamicToolManager.cpp:28  bool bEnabled = bLoadAllTools || (Category == "core");
// ---------------------------------------------------------------------------

/**
 * The set of tool names a fresh native global/default manager reports as enabled,
 * given the bLoadAllTools flag (McpDynamicToolManager.cpp:28
 * `bEnabled = bLoadAllTools || core`). The production default is bLoadAllTools=true
 * (from bLoadAllToolsOnStart, default TRUE) -> all-enabled, matching the TS global
 * manager. Passing false models the config-gated core-only profile, where only the
 * `core` category is enabled.
 */
export function nativeGlobalInitialEnabledNames(
  defs: readonly ToolDefinition[],
  bLoadAllTools: boolean = NATIVE_DEFAULT_LOAD_ALL_TOOLS,
): string[] {
  return defs
    .filter((def) => bLoadAllTools || (def.category ?? 'utility') === 'core')
    .map((def) => def.name)
    .sort();
}

// ---------------------------------------------------------------------------
// Grounding guards — assert the oracle constants still match live native source.
// These are SUPPORTING checks (drift detection), NOT the parity proof. The parity
// proof (session-subscription-parity.test.ts) is 100% executable behavior.
// ---------------------------------------------------------------------------

const NATIVE_MCP_ROOT = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP',
);

export function readNativeSource(relative: string): string {
  return readFileSync(resolve(NATIVE_MCP_ROOT, relative), 'utf8');
}

/** Parse the native allowlist straight out of McpResourceRevision.h. */
export function parseNativeAllowlistFromSource(): string[] {
  const source = readNativeSource('Primitives/McpResourceRevision.h');
  const matches = [...source.matchAll(/TEXT\("(ue:\/\/[^"]+)"\)/g)].map((m) => m[1]);
  // De-duplicate while preserving first-seen order (the allowlist block lists each once).
  return [...new Set(matches)];
}

/**
 * Parse the `bLoadAllToolsOnStart` project-setting default out of the plugin
 * settings header (Public/McpAutomationBridgeSettings.h). This grounds
 * NATIVE_DEFAULT_LOAD_ALL_TOOLS in the wired production default, not the C++
 * signature fallback. Returns the literal initializer, or null if the field moved.
 */
export function parseNativeLoadAllToolsOnStartDefault(): boolean | null {
  const settingsPath = resolve(NATIVE_MCP_ROOT, '../../Public/McpAutomationBridgeSettings.h');
  const match = readFileSync(settingsPath, 'utf8').match(
    /bool\s+bLoadAllToolsOnStart\s*=\s*(true|false)\s*;/,
  );
  return match ? match[1] === 'true' : null;
}
