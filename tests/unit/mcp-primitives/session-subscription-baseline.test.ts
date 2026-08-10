// tests/unit/mcp-primitives/session-subscription-baseline.test.ts
//
// Task 38 lane C — PASSING baseline characterization.
//
// This suite is GREEN today. It (1) characterizes the CURRENT TypeScript
// production behavior for subscriptions, notification coalescing, resource
// revisions, and the session configure overlay; (2) characterizes the EXECUTABLE
// native fixture (session-subscription-native-fixture.ts) over the same
// scenarios; (3) proves the two AGREE on the faithful-port semantics (normalized
// parity baseline); (4) grounds the fixture constants against live native C++
// source (drift guard, NOT the parity proof); and (5) records the truthful
// cross-transport default parity: at the production default both transports are
// all-enabled (native seeded from the bLoadAllToolsOnStart project setting,
// default TRUE), and core-only is a config-gated opt-in (bLoadAllToolsOnStart=
// false). The exact default and profile parity assertions live in the sibling
// parity suite.
//
// No wall clock, no transport, no live editor.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import {
  asResourceRevision,
  InMemoryRevisionProvider,
  isSubscribableUri,
  SUBSCRIBABLE_URIS,
  type SubscribableUri,
} from '../../../src/server/mcp-primitives/resource-revision.js';
import { SubscriptionStore } from '../../../src/server/mcp-primitives/subscriptions/subscription-store.js';
import { NotificationCoalescer } from '../../../src/server/mcp-primitives/subscriptions/notification-coalescer.js';
import {
  CATALOG_SUBSCRIPTION_URI,
  DEFAULT_COALESCE_WINDOW_MS,
  RESOURCE_CHANGE_KINDS,
  type ResourceUpdatedPayload,
} from '../../../src/server/mcp-primitives/subscriptions/subscription-types.js';
import {
  PrimitiveNotificationDriver,
  type NotifyingServer,
} from '../../../src/server/mcp-primitives/primitive-notifications.js';
import { SessionConfigureStore } from '../../../src/server/mcp-primitives/session-configure-store.js';
import type { CatalogRevisionReader } from '../../../src/server/mcp-primitives/catalog-revision-reader.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import { consolidatedToolDefinitions } from '../../../src/tools/catalog/consolidated-tool-definitions.js';
import {
  NATIVE_DEFAULT_COALESCE_WINDOW_MS,
  NATIVE_DEFAULT_LOAD_ALL_TOOLS,
  NATIVE_RESOURCE_CHANGE_KINDS,
  NATIVE_SUBSCRIBABLE_URIS,
  NativeNotificationCoalescer,
  NativeSessionConfigureStore,
  NativeSubscriptionStore,
  nativeGlobalInitialEnabledNames,
  nativeWireParams,
  parseNativeAllowlistFromSource,
  parseNativeLoadAllToolsOnStartDefault,
  readNativeSource,
  type NativeResourceUpdatedPayload,
} from './session-subscription-native-fixture.js';

const A = 'session-A';
const SELECTION = 'ue://selection';
const LEVEL = 'ue://level';

/** Per-session catalog state cursor double (Task 36 C1 shape). */
class CountingCatalogReader implements CatalogRevisionReader {
  private readonly counts = new Map<string, number>();
  getCatalogStateRevision(sessionId: string): number {
    return this.counts.get(sessionId) ?? 0;
  }
  bump(sessionId: string): void {
    this.counts.set(sessionId, this.getCatalogStateRevision(sessionId) + 1);
  }
}

const CORE_TOOL_COUNT = consolidatedToolDefinitions.filter(
  (d) => (d.category ?? 'utility') === 'core',
).length;
const TOTAL_TOOL_COUNT = consolidatedToolDefinitions.length;

beforeEach(() => {
  dynamicToolManager.reset();
});

afterEach(() => {
  dynamicToolManager.reset();
  vi.useRealTimers();
});

// ---------------------------------------------------------------------------
// 1. Current TypeScript behavior (characterization).
// ---------------------------------------------------------------------------

describe('baseline: current TS subscription/revision/configure behavior', () => {
  it('TS subscription store: allowlist gate, idempotent duplicate, blank rejected', () => {
    const store = new SubscriptionStore();
    expect(store.subscribe(A, SELECTION)).toEqual({
      accepted: true,
      alreadySubscribed: false,
      evicted: null,
      reason: null,
    });
    expect(store.subscribe(A, SELECTION).alreadySubscribed).toBe(true);
    expect(store.subscribe(A, 'ue://assets').reason).toBe('NOT_SUBSCRIBABLE');
    expect(store.subscribe('', SELECTION).reason).toBe('INVALID_SESSION');
    expect(store.count(A)).toBe(1);
  });

  it('TS coalescer: burst folds to one bounded (3-key) emission', () => {
    const clock = { v: 0 };
    const emitted: Array<{ sessionId: string; payload: ResourceUpdatedPayload }> = [];
    const revisions = new InMemoryRevisionProvider();
    const store = new SubscriptionStore();
    const coalescer = new NotificationCoalescer({
      store,
      revisions,
      catalog: new CountingCatalogReader(),
      clock: () => clock.v,
      windowMs: 50,
      sink: (sessionId, payload) => emitted.push({ sessionId, payload }),
    });
    store.subscribe(A, SELECTION);
    revisions.set(SELECTION, asResourceRevision(4));
    coalescer.recordChange(A, SELECTION, 'updated');
    clock.v += 10;
    coalescer.recordChange(A, SELECTION, 'invalidated');
    clock.v += 40;
    expect(coalescer.flushDue(clock.v)).toBe(1);
    expect(emitted).toHaveLength(1);
    expect(Object.keys(emitted[0].payload).sort()).toEqual(['changeKind', 'revision', 'uri']);
    expect(emitted[0].payload).toEqual({ uri: SELECTION, revision: 4, changeKind: 'invalidated' });
  });

  it('TS driver: resources/updated crosses the wire as URI-only params', async () => {
    vi.useFakeTimers();
    const notifications: Array<{ method: string; params?: Record<string, unknown> }> = [];
    const server: NotifyingServer = {
      notification: async (n) => {
        notifications.push(n);
      },
    };
    const revisions = new InMemoryRevisionProvider();
    const catalog = new CountingCatalogReader();
    const driver = new PrimitiveNotificationDriver({ server, revisions, catalog, clock: () => Date.now() });
    driver.store.subscribe(A, CATALOG_SUBSCRIPTION_URI);
    revisions.set(CATALOG_SUBSCRIPTION_URI, asResourceRevision(3));
    catalog.bump(A);
    driver.syncCatalog(A);
    await vi.advanceTimersByTimeAsync(DEFAULT_COALESCE_WINDOW_MS);
    expect(notifications).toHaveLength(1);
    expect(notifications[0].method).toBe('notifications/resources/updated');
    expect(Object.keys(notifications[0].params ?? {})).toEqual(['uri']);
    expect(notifications[0].params).toEqual({ uri: CATALOG_SUBSCRIPTION_URI });
    driver.dispose();
  });

  it('TS session configure overlay: fresh session is fully enabled at revision 0', () => {
    const store = new SessionConfigureStore();
    const status = store.getStatus(A);
    expect(status.disabledTools).toBe(0);
    expect(status.enabledTools).toBe(TOTAL_TOOL_COUNT);
    expect(status.catalogStateRevision).toBe(0);
  });

  it('TS session configure overlay: effective-change-only revision, protected core', () => {
    const store = new SessionConfigureStore();
    store.enableTools(A, ['manage_ai']); // already enabled -> no-op
    expect(store.getCatalogStateRevision(A)).toBe(0);
    store.disableCategory(A, 'gameplay'); // effective
    expect(store.getCatalogStateRevision(A)).toBe(1);
    const prot = store.disableTools(A, ['manage_tools', 'inspect']);
    expect(prot.protected).toEqual(expect.arrayContaining(['manage_tools', 'inspect']));
    expect(store.getCatalogStateRevision(A)).toBe(1); // rejected protected -> no bump
  });

  it('TS global dynamic manager is unconditionally all-enabled', () => {
    const status = dynamicToolManager.getStatus();
    expect(status.disabledTools).toBe(0);
    expect(status.enabledTools).toBe(TOTAL_TOOL_COUNT);
  });
});

// ---------------------------------------------------------------------------
// 2. Native fixture behavior (characterization).
// ---------------------------------------------------------------------------

describe('baseline: executable native fixture behavior', () => {
  it('native subscription store mirrors allowlist/idempotent/blank rules', () => {
    const store = new NativeSubscriptionStore();
    expect(store.subscribe(A, SELECTION).accepted).toBe(true);
    expect(store.subscribe(A, SELECTION).alreadySubscribed).toBe(true);
    expect(store.subscribe(A, 'ue://assets').reason).toBe('NOT_SUBSCRIBABLE');
    expect(store.subscribe('   ', SELECTION).reason).toBe('INVALID_SESSION');
    expect(store.count(A)).toBe(1);
  });

  it('native coalescer folds a burst to one 3-key emission', () => {
    const clock = { v: 0 };
    const emitted: Array<{ sessionId: string; payload: NativeResourceUpdatedPayload }> = [];
    const revisions = new Map<string, number>();
    const store = new NativeSubscriptionStore();
    const coalescer = new NativeNotificationCoalescer(
      store,
      (uri) => revisions.get(uri) ?? 1,
      (sessionId, payload) => emitted.push({ sessionId, payload }),
      () => clock.v,
      50,
    );
    store.subscribe(A, SELECTION);
    revisions.set(SELECTION, 4);
    coalescer.recordChange(A, SELECTION, 'updated');
    clock.v += 10;
    coalescer.recordChange(A, SELECTION, 'invalidated');
    clock.v += 40;
    expect(coalescer.flushDue(clock.v)).toBe(1);
    expect(Object.keys(emitted[0].payload).sort()).toEqual(['changeKind', 'revision', 'uri']);
    expect(nativeWireParams(emitted[0].payload)).toEqual({ uri: SELECTION });
  });

  it('native session configure overlay is a faithful all-enabled port', () => {
    const store = new NativeSessionConfigureStore(() => consolidatedToolDefinitions);
    const status = store.getStatus(A);
    expect(status.disabledTools).toBe(0);
    expect(status.enabledTools).toBe(TOTAL_TOOL_COUNT);
    expect(status.catalogStateRevision).toBe(0);
    store.enableTools(A, ['manage_ai']);
    expect(store.getCatalogStateRevision(A)).toBe(0); // no-op
    store.disableCategory(A, 'gameplay');
    expect(store.getCatalogStateRevision(A)).toBe(1); // effective
  });

  it('native GLOBAL/default manager seeds core-only when bLoadAllTools is false', () => {
    const enabled = nativeGlobalInitialEnabledNames(consolidatedToolDefinitions, false);
    expect(enabled.length).toBe(CORE_TOOL_COUNT);
    expect(enabled.length).toBeLessThan(TOTAL_TOOL_COUNT);
    for (const name of enabled) {
      const def = consolidatedToolDefinitions.find((d) => d.name === name);
      expect(def?.category ?? 'utility').toBe('core');
    }
  });
});

// ---------------------------------------------------------------------------
// 3. Normalized agreement — the faithful-port semantics match today (GREEN).
// ---------------------------------------------------------------------------

describe('baseline: TS and native fixture agree on ported semantics', () => {
  it('agree on the exact 9-URI allowlist and change-kind set', () => {
    expect([...NATIVE_SUBSCRIBABLE_URIS]).toEqual([...SUBSCRIBABLE_URIS]);
    expect([...NATIVE_RESOURCE_CHANGE_KINDS]).toEqual([...RESOURCE_CHANGE_KINDS]);
    expect(NATIVE_DEFAULT_COALESCE_WINDOW_MS).toBe(DEFAULT_COALESCE_WINDOW_MS);
    for (const uri of SUBSCRIBABLE_URIS) {
      expect(isSubscribableUri(uri)).toBe(true);
    }
  });

  it('agree on subscribe ordering and cap eviction for the same sequence', () => {
    const ts = new SubscriptionStore({ maxPerSession: 2 });
    const nat = new NativeSubscriptionStore({ maxPerSession: 2 });
    const seq: SubscribableUri[] = [SELECTION, LEVEL, 'ue://project'];
    for (const uri of seq) {
      ts.subscribe(A, uri);
      nat.subscribe(A, uri);
    }
    expect([...nat.subscriptions(A)]).toEqual([...ts.subscriptions(A)]);
    expect(nat.subscriptions(A)).toEqual([LEVEL, 'ue://project']); // SELECTION evicted (oldest)
  });

  it('agree on coalesced emissions for an identical fake-clock burst', () => {
    const clock = { v: 0 };
    const revisions = new InMemoryRevisionProvider();
    const tsEmitted: ResourceUpdatedPayload[] = [];
    const natEmitted: NativeResourceUpdatedPayload[] = [];
    const tsStore = new SubscriptionStore();
    const tsCo = new NotificationCoalescer({
      store: tsStore,
      revisions,
      catalog: new CountingCatalogReader(),
      clock: () => clock.v,
      windowMs: 50,
      sink: (_s, p) => tsEmitted.push(p),
    });
    const natStore = new NativeSubscriptionStore();
    const natCo = new NativeNotificationCoalescer(
      natStore,
      (uri) => revisions.currentRevision(uri as SubscribableUri),
      (_s, p) => natEmitted.push(p),
      () => clock.v,
      50,
    );
    tsStore.subscribe(A, SELECTION);
    natStore.subscribe(A, SELECTION);
    revisions.set(SELECTION, asResourceRevision(7));
    tsCo.recordChange(A, SELECTION, 'updated');
    natCo.recordChange(A, SELECTION, 'updated');
    clock.v += 20;
    tsCo.recordChange(A, SELECTION, 'removed');
    natCo.recordChange(A, SELECTION, 'removed');
    clock.v += 30;
    expect(tsCo.flushDue(clock.v)).toBe(1);
    expect(natCo.flushDue(clock.v)).toBe(1);
    expect(natEmitted.map((p) => ({ ...p }))).toEqual(tsEmitted.map((p) => ({ ...p })));
  });

  it('agree on session-overlay enabled set and revision for the same mutations', () => {
    const ts = new SessionConfigureStore();
    const nat = new NativeSessionConfigureStore(() => consolidatedToolDefinitions);
    ts.disableCategory(A, 'gameplay');
    nat.disableCategory(A, 'gameplay');
    ts.disableTools(A, ['manage_asset']);
    nat.disableTools(A, ['manage_asset']);
    const tsEnabled = ts.listTools(A).filter((t) => ts.isToolEnabled(A, t.name)).map((t) => t.name).sort();
    expect(nat.enabledToolNames(A)).toEqual(tsEnabled);
    expect(nat.getCatalogStateRevision(A)).toBe(ts.getCatalogStateRevision(A));
  });
});

// ---------------------------------------------------------------------------
// 4. Grounding guards — fixture constants still match live native C++ source.
//    Supporting drift detection only; NOT the parity proof.
// ---------------------------------------------------------------------------

describe('baseline: native fixture is grounded in live plugin source', () => {
  it('fixture allowlist matches McpResourceRevision.h', () => {
    expect(parseNativeAllowlistFromSource()).toEqual([...NATIVE_SUBSCRIBABLE_URIS]);
  });

  it('native global seed is bLoadAllTools||core, wired from a project setting defaulting TRUE', () => {
    const src = readNativeSource('DynamicTools/McpDynamicToolManager.cpp');
    expect(src).toContain('bool bEnabled = bLoadAllTools || (Category == TEXT("core"));');
    expect(parseNativeLoadAllToolsOnStartDefault()).toBe(true);
    expect(NATIVE_DEFAULT_LOAD_ALL_TOOLS).toBe(true);
  });

  it('native resources/updated wire params are URI-only in source', () => {
    const src = readNativeSource('Transport/McpNativeTransportPrimitiveNotifications.cpp');
    expect(src).toContain('Params->SetStringField(TEXT("uri"), Uri);');
    expect(src).not.toContain('SetNumberField(TEXT("revision")');
  });
});

// ---------------------------------------------------------------------------
// 5. Truthful cross-transport default parity + config-gated core-only profile.
// ---------------------------------------------------------------------------

describe('baseline: default parity holds; core-only is a config-gated profile', () => {
  it('at the production default both transports are all-enabled (no divergence)', () => {
    const tsEnabled = dynamicToolManager.listTools().filter((t) => t.enabled).map((t) => t.name).sort();
    const nativeDefault = nativeGlobalInitialEnabledNames(consolidatedToolDefinitions, NATIVE_DEFAULT_LOAD_ALL_TOOLS);
    expect(NATIVE_DEFAULT_LOAD_ALL_TOOLS).toBe(true);
    expect(nativeDefault).toEqual(tsEnabled);
    expect(nativeDefault.length).toBe(TOTAL_TOOL_COUNT);
  });

  it('the config-gated core-only profile (bLoadAllTools=false) enables only the core category', () => {
    const nativeCore = nativeGlobalInitialEnabledNames(consolidatedToolDefinitions, false);
    expect(nativeCore.length).toBe(CORE_TOOL_COUNT);
    expect(nativeCore.length).toBeLessThan(TOTAL_TOOL_COUNT);
    for (const name of nativeCore) {
      const def = consolidatedToolDefinitions.find((d) => d.name === name);
      expect(def?.category ?? 'utility').toBe('core');
    }
  });
});
