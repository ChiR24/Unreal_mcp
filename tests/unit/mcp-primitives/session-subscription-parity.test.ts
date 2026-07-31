// tests/unit/mcp-primitives/session-subscription-parity.test.ts
//
// Task 38 lane C — RED-first normalized cross-transport parity.
//
// This suite asserts the DESIRED cross-transport parity between the TypeScript
// production primitives and the executable native fixture. It compares SEMANTICS
// (normalized results/revisions), never stdio/HTTP/SSE framing and never grepped
// source text, and uses exact `toEqual` — no broad success/error alternatives.
//
// Most assertions are GREEN today: subscriptions, coalescing, revisions, session
// isolation, reset/reconnect, teardown, cleanup, cross-session leak rejection,
// and full/minimal client profiles are a faithful native port and already match.
//
// The default-overlay parity below is GREEN: the production default on BOTH
// transports is all-enabled (TS unconditionally; native seeded from the
// bLoadAllToolsOnStart project setting, default TRUE), so a fresh session
// advertises the same enabled tool set. Core-only is a config-gated opt-in
// (bLoadAllToolsOnStart=false), and its target state is reachable and identical on
// both transports; that profile parity is proved too. (A prior revision modeled
// the native default as core-only by grounding NATIVE_DEFAULT_LOAD_ALL_TOOLS on the
// C++ signature fallback rather than the wired setting default — a fixture bug,
// now corrected.) Do not weaken these exact assertions.

import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import {
  asResourceRevision,
  InMemoryRevisionProvider,
  type SubscribableUri,
} from '../../../src/server/mcp-primitives/resource-revision.js';
import { SubscriptionStore } from '../../../src/server/mcp-primitives/subscriptions/subscription-store.js';
import { NotificationCoalescer } from '../../../src/server/mcp-primitives/subscriptions/notification-coalescer.js';
import type { ResourceUpdatedPayload } from '../../../src/server/mcp-primitives/subscriptions/subscription-types.js';
import { SessionConfigureStore } from '../../../src/server/mcp-primitives/session-configure-store.js';
import type { CatalogRevisionReader } from '../../../src/server/mcp-primitives/catalog-revision-reader.js';
import { parseClientCapabilityProfile } from '../../../src/server/mcp-primitives/session-capability-profile.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import { consolidatedToolDefinitions } from '../../../src/tools/catalog/consolidated-tool-definitions.js';
import {
  NATIVE_DEFAULT_LOAD_ALL_TOOLS,
  NativeNotificationCoalescer,
  NativeSessionConfigureStore,
  NativeSubscriptionStore,
  nativeGlobalInitialEnabledNames,
  nativeWireParams,
  type NativeResourceUpdatedPayload,
} from './session-subscription-native-fixture.js';

const A = 'session-A';
const B = 'session-B';
const SELECTION = 'ue://selection';
const LEVEL = 'ue://level';
const PROJECT = 'ue://project';

const TOTAL_TOOL_COUNT = consolidatedToolDefinitions.length;
const CORE_TOOL_COUNT = consolidatedToolDefinitions.filter((d) => (d.category ?? 'utility') === 'core').length;

class CountingCatalogReader implements CatalogRevisionReader {
  private readonly counts = new Map<string, number>();
  getCatalogStateRevision(sessionId: string): number {
    return this.counts.get(sessionId) ?? 0;
  }
}

interface PairedCoalescers {
  readonly tsStore: SubscriptionStore;
  readonly tsCo: NotificationCoalescer;
  readonly natStore: NativeSubscriptionStore;
  readonly natCo: NativeNotificationCoalescer;
  readonly revisions: InMemoryRevisionProvider;
  readonly tsEmitted: ResourceUpdatedPayload[];
  readonly natEmitted: NativeResourceUpdatedPayload[];
  advance(ms: number): void;
  now(): number;
}

/** Build a TS coalescer and the native fixture coalescer over one shared fake clock + revision source. */
function pairedCoalescers(windowMs = 50): PairedCoalescers {
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
    windowMs,
    sink: (_s, p) => tsEmitted.push(p),
  });
  const natStore = new NativeSubscriptionStore();
  const natCo = new NativeNotificationCoalescer(
    natStore,
    (uri) => revisions.currentRevision(uri as SubscribableUri),
    (_s, p) => natEmitted.push(p),
    () => clock.v,
    windowMs,
  );
  return {
    tsStore,
    tsCo,
    natStore,
    natCo,
    revisions,
    tsEmitted,
    natEmitted,
    advance: (ms) => {
      clock.v += ms;
    },
    now: () => clock.v,
  };
}

/** Normalized, order-independent snapshot of a configure overlay. */
function overlaySnapshot(enabled: readonly string[], disabledCount: number, revision: number): {
  enabled: string[];
  disabledCount: number;
  revision: number;
} {
  return { enabled: [...enabled].sort(), disabledCount, revision };
}

function tsSessionEnabled(store: SessionConfigureStore, sessionId: string): string[] {
  return store.listTools(sessionId).filter((t) => store.isToolEnabled(sessionId, t.name)).map((t) => t.name).sort();
}

beforeEach(() => {
  dynamicToolManager.reset();
});

afterEach(() => {
  dynamicToolManager.reset();
});

// ===========================================================================
// GREEN parity — subscription store + notification coalescer (faithful port).
// ===========================================================================

describe('parity: subscribe/unsubscribe allowlist', () => {
  it('accepts every allowlisted URI and rejects the same off-list URI and blank session', () => {
    const h = pairedCoalescers();
    for (const uri of ['ue://assets', 'file:///etc/passwd', SELECTION]) {
      expect(h.natStore.subscribe(A, uri)).toEqual(h.tsStore.subscribe(A, uri));
    }
    expect(h.natStore.subscribe('', LEVEL)).toEqual(h.tsStore.subscribe('', LEVEL));
    expect(h.natStore.subscriptions(A)).toEqual([...h.tsStore.subscriptions(A)]);
  });

  it('unsubscribe drains identically and is well-defined on repeats', () => {
    const h = pairedCoalescers();
    h.tsStore.subscribe(A, SELECTION);
    h.natStore.subscribe(A, SELECTION);
    expect(h.natStore.unsubscribe(A, SELECTION)).toBe(h.tsStore.unsubscribe(A, SELECTION));
    expect(h.natStore.unsubscribe(A, SELECTION)).toBe(h.tsStore.unsubscribe(A, SELECTION));
    expect(h.natStore.hasSession(A)).toBe(h.tsStore.hasSession(A));
  });
});

describe('parity: URI-only resources/updated + burst coalescing', () => {
  it('a same-window burst folds to one identical emission on both transports', () => {
    const h = pairedCoalescers(50);
    h.tsStore.subscribe(A, SELECTION);
    h.natStore.subscribe(A, SELECTION);
    h.revisions.set(SELECTION, asResourceRevision(9));
    h.tsCo.recordChange(A, SELECTION, 'updated');
    h.natCo.recordChange(A, SELECTION, 'updated');
    h.advance(20);
    h.tsCo.recordChange(A, SELECTION, 'removed');
    h.natCo.recordChange(A, SELECTION, 'removed');
    h.advance(30);
    expect(h.tsCo.flushDue(h.now())).toBe(1);
    expect(h.natCo.flushDue(h.now())).toBe(1);
    expect(h.natEmitted.map((p) => ({ ...p }))).toEqual(h.tsEmitted.map((p) => ({ ...p })));
  });

  it('the wire params carry the URI only on both transports', () => {
    const h = pairedCoalescers(10);
    h.tsStore.subscribe(A, SELECTION);
    h.natStore.subscribe(A, SELECTION);
    h.revisions.set(SELECTION, asResourceRevision(2));
    h.tsCo.recordChange(A, SELECTION, 'updated');
    h.natCo.recordChange(A, SELECTION, 'updated');
    h.advance(10);
    h.tsCo.flushDue(h.now());
    h.natCo.flushDue(h.now());
    // TS wire params are produced by the driver as { uri } (primitive-notifications.ts);
    // both surfaces reduce the internal 3-field payload to URI-only on the wire.
    expect(nativeWireParams(h.natEmitted[0])).toEqual({ uri: h.tsEmitted[0].uri });
  });
});

describe('parity: effective-change-only revisions + monotonic suppression', () => {
  it('suppresses a stale lower revision identically, then emits a higher one', () => {
    const h = pairedCoalescers(10);
    h.tsStore.subscribe(A, SELECTION);
    h.natStore.subscribe(A, SELECTION);
    for (const rev of [5, 4, 7]) {
      h.revisions.set(SELECTION, asResourceRevision(rev));
      h.tsCo.recordChange(A, SELECTION, 'updated');
      h.natCo.recordChange(A, SELECTION, 'updated');
      h.advance(10);
      h.tsCo.flushDue(h.now());
      h.natCo.flushDue(h.now());
    }
    expect(h.natEmitted.map((p) => p.revision)).toEqual(h.tsEmitted.map((p) => p.revision));
    expect(h.natEmitted.map((p) => p.revision)).toEqual([5, 7]); // stale 4 suppressed on both
  });
});

describe('parity: no-op stability', () => {
  it('a limit-only and an already-enabled change bump neither transport revision', () => {
    const ts = new SessionConfigureStore();
    const nat = new NativeSessionConfigureStore(() => consolidatedToolDefinitions);
    ts.setLimit(A, 'maxResults', 10);
    ts.enableTools(A, ['manage_ai']); // already enabled
    nat.enableTools(A, ['manage_ai']); // already enabled
    expect(nat.getCatalogStateRevision(A)).toBe(ts.getCatalogStateRevision(A));
    expect(nat.getCatalogStateRevision(A)).toBe(0);
  });
});

describe('parity: protected invariants', () => {
  it('both refuse to disable protected tools and the core category with no revision change', () => {
    const ts = new SessionConfigureStore();
    const nat = new NativeSessionConfigureStore(() => consolidatedToolDefinitions);
    const tsProt = ts.disableTools(A, ['manage_tools', 'inspect']);
    const natProt = nat.disableTools(A, ['manage_tools', 'inspect']);
    expect([...natProt.protected].sort()).toEqual([...tsProt.protected].sort());
    ts.disableCategory(A, 'core');
    nat.disableCategory(A, 'core');
    expect(nat.isToolEnabled(A, 'manage_tools')).toBe(ts.isToolEnabled(A, 'manage_tools'));
    expect(nat.getCatalogStateRevision(A)).toBe(ts.getCatalogStateRevision(A));
    expect(nat.getCatalogStateRevision(A)).toBe(0);
  });
});

describe('parity: two-session isolation', () => {
  it('one session mutation never leaks into another on either transport', () => {
    const ts = new SessionConfigureStore();
    const nat = new NativeSessionConfigureStore(() => consolidatedToolDefinitions);
    ts.disableCategory(A, 'gameplay');
    nat.disableCategory(A, 'gameplay');
    const tsA = overlaySnapshot(tsSessionEnabled(ts, A), ts.getStatus(A).disabledTools, ts.getCatalogStateRevision(A));
    const natA = overlaySnapshot(nat.enabledToolNames(A), nat.getStatus(A).disabledTools, nat.getCatalogStateRevision(A));
    const tsB = overlaySnapshot(tsSessionEnabled(ts, B), ts.getStatus(B).disabledTools, ts.getCatalogStateRevision(B));
    const natB = overlaySnapshot(nat.enabledToolNames(B), nat.getStatus(B).disabledTools, nat.getCatalogStateRevision(B));
    expect(natA).toEqual(tsA);
    expect(natB).toEqual(tsB);
    expect(natB.revision).toBe(0); // untouched session stays pristine on both
  });
});

describe('parity: reset and reconnect defaults', () => {
  it('reset restores the same all-enabled overlay on both transports', () => {
    const ts = new SessionConfigureStore();
    const nat = new NativeSessionConfigureStore(() => consolidatedToolDefinitions);
    ts.disableCategory(A, 'gameplay');
    nat.disableCategory(A, 'gameplay');
    ts.reset(A);
    nat.reset(A);
    expect(overlaySnapshot(nat.enabledToolNames(A), nat.getStatus(A).disabledTools, nat.getCatalogStateRevision(A))).toEqual(
      overlaySnapshot(tsSessionEnabled(ts, A), ts.getStatus(A).disabledTools, ts.getCatalogStateRevision(A)),
    );
  });

  it('reconnect (clearSession then re-access) reseeds pristine identically', () => {
    const ts = new SessionConfigureStore();
    const nat = new NativeSessionConfigureStore(() => consolidatedToolDefinitions);
    ts.disableCategory(A, 'gameplay');
    nat.disableCategory(A, 'gameplay');
    expect(ts.clearSession(A)).toBe(nat.clearSession(A));
    expect(nat.getStatus(A).disabledTools).toBe(ts.getStatus(A).disabledTools);
    expect(nat.getStatus(A).catalogStateRevision).toBe(ts.getStatus(A).catalogStateRevision);
    expect(nat.getStatus(A).disabledTools).toBe(0); // both reseed all-enabled
  });
});

describe('parity: disconnect-mid-window and teardown cleanup', () => {
  it('a change recorded then disconnected before the window emits nothing on both', () => {
    const h = pairedCoalescers(50);
    h.tsStore.subscribe(A, SELECTION);
    h.natStore.subscribe(A, SELECTION);
    h.tsCo.recordChange(A, SELECTION, 'updated');
    h.natCo.recordChange(A, SELECTION, 'updated');
    // Disconnect mid-window.
    h.tsStore.clearSession(A);
    h.tsCo.clearSession(A);
    h.natStore.clearSession(A);
    h.natCo.clearSession(A);
    h.advance(100);
    expect(h.natCo.flushDue(h.now())).toBe(h.tsCo.flushDue(h.now()));
    expect(h.natEmitted).toEqual([]);
    expect(h.tsEmitted).toEqual([]);
    expect(h.natCo.pendingCount()).toBe(h.tsCo.pendingCount());
    expect(h.natCo.pendingCount()).toBe(0);
  });
});

describe('parity: cross-session leak rejection', () => {
  it('a change for a non-subscribed session is dropped and never delivered elsewhere', () => {
    const h = pairedCoalescers(10);
    h.tsStore.subscribe(A, SELECTION);
    h.natStore.subscribe(A, SELECTION);
    // B never subscribed: a direct change for B is dropped identically on both.
    const tsB = h.tsCo.recordChange(B, SELECTION, 'updated');
    const natB = h.natCo.recordChange(B, SELECTION, 'updated');
    expect(natB).toEqual(tsB);
    expect(tsB.reason).toBe('NOT_SUBSCRIBED');
    // A global change fans out only to the subscribed session (A), never to B.
    h.revisions.set(SELECTION, asResourceRevision(3));
    expect(h.natCo.recordGlobalChange(SELECTION, 'updated')).toBe(h.tsCo.recordGlobalChange(SELECTION, 'updated'));
    h.advance(10);
    expect(h.natCo.flushDue(h.now())).toBe(h.tsCo.flushDue(h.now()));
    expect(h.natEmitted.map((p) => p.uri)).toEqual(h.tsEmitted.map((p) => p.uri));
    expect(h.tsEmitted).toHaveLength(1); // only A received it
  });
});

// ===========================================================================
// GREEN — exact-parity comparator teeth (injected drift MUST fail equality).
// ===========================================================================

describe('parity comparator teeth: injected drift fails exact parity', () => {
  it('a one-field overlay drift is not exactly equal to the TS overlay', () => {
    const ts = new SessionConfigureStore();
    const nat = new NativeSessionConfigureStore(() => consolidatedToolDefinitions);
    ts.disableCategory(A, 'gameplay');
    nat.disableCategory(A, 'gameplay');
    const tsSnap = overlaySnapshot(tsSessionEnabled(ts, A), ts.getStatus(A).disabledTools, ts.getCatalogStateRevision(A));
    const natSnap = overlaySnapshot(nat.enabledToolNames(A), nat.getStatus(A).disabledTools, nat.getCatalogStateRevision(A));
    expect(natSnap).toEqual(tsSnap); // aligned...
    expect({ ...natSnap, revision: natSnap.revision + 1 }).not.toEqual(tsSnap); // ...but one-field drift is caught
    expect({ ...natSnap, disabledCount: natSnap.disabledCount + 1 }).not.toEqual(tsSnap);
  });

  it('a stale (lower) revision payload is not exactly equal to the emitted one', () => {
    const ref: NativeResourceUpdatedPayload = { uri: SELECTION, revision: 5, changeKind: 'updated' };
    expect({ ...ref, revision: 4 }).not.toEqual(ref);
    expect({ ...ref, changeKind: 'removed' }).not.toEqual(ref);
  });
});

// ===========================================================================
// GREEN — full vs minimal client profile does not change subscription semantics.
// ===========================================================================

describe('parity: full and minimal client profiles', () => {
  it('advertised subscription capability follows the declared client capabilities', () => {
    expect(parseClientCapabilityProfile({ resources: { subscribe: true } }).hasSubscriptions).toBe(true);
    expect(parseClientCapabilityProfile({}).hasSubscriptions).toBe(false);
  });

  it('the subscription store behaves identically regardless of client profile', () => {
    // The store/coalescer are profile-agnostic: a full-capability and a minimal
    // client that both subscribe get identical semantics; the profile only gates
    // whether the server advertises the capability, not the internal behavior.
    const full = pairedCoalescers();
    const minimal = pairedCoalescers();
    for (const h of [full, minimal]) {
      h.tsStore.subscribe(A, PROJECT);
      h.natStore.subscribe(A, PROJECT);
    }
    expect(minimal.natStore.subscriptions(A)).toEqual([...full.tsStore.subscriptions(A)]);
  });
});

// ===========================================================================
// GREEN — truthful cross-transport DEFAULT parity. The production default on both
// transports is all-enabled: TS is unconditionally all-enabled and the native
// global manager is seeded from bLoadAllToolsOnStart (default TRUE), so a fresh
// default session advertises the SAME enabled tool set. (The former RED premise —
// native default is core-only — grounded NATIVE_DEFAULT_LOAD_ALL_TOOLS on the C++
// signature fallback, not the wired setting default: a fixture bug, now corrected.)
// ===========================================================================

describe('parity: default configure overlay is equal across transports', () => {
  it('a fresh default session advertises the same enabled tool set on both transports', () => {
    const tsEnabled = dynamicToolManager.listTools().filter((t) => t.enabled).map((t) => t.name).sort();
    const nativeEnabled = nativeGlobalInitialEnabledNames(consolidatedToolDefinitions, NATIVE_DEFAULT_LOAD_ALL_TOOLS);
    expect(NATIVE_DEFAULT_LOAD_ALL_TOOLS).toBe(true);
    expect(nativeEnabled).toEqual(tsEnabled);
    expect(nativeEnabled).toHaveLength(TOTAL_TOOL_COUNT);
  });

  it('a fresh default session reports the same disabled-tool count on both transports', () => {
    const tsDisabled = dynamicToolManager.getStatus().disabledTools;
    const nativeDisabled =
      TOTAL_TOOL_COUNT -
      nativeGlobalInitialEnabledNames(consolidatedToolDefinitions, NATIVE_DEFAULT_LOAD_ALL_TOOLS).length;
    expect(nativeDisabled).toBe(tsDisabled);
    expect(nativeDisabled).toBe(0);
  });
});

// ===========================================================================
// GREEN — configurable load-all / core-only profiles are consistent across
// transports. The native seed formula (bLoadAllTools || core) is shared and
// executable; the identical core-only target state is reachable on the TS surface
// by disabling the non-core tools. Neither path disables the protected core tools.
// ===========================================================================

const NON_CORE_TOOL_NAMES = consolidatedToolDefinitions
  .filter((d) => (d.category ?? 'utility') !== 'core')
  .map((d) => d.name);

describe('parity: configurable load-all and core-only profiles', () => {
  it('the load-all profile (bLoadAllTools=true) enables all tools on both transports', () => {
    const nativeAll = nativeGlobalInitialEnabledNames(consolidatedToolDefinitions, true);
    const tsAll = dynamicToolManager.listTools().filter((t) => t.enabled).map((t) => t.name).sort();
    expect(nativeAll).toEqual(tsAll);
    expect(nativeAll).toHaveLength(TOTAL_TOOL_COUNT);
  });

  it('the core-only profile (bLoadAllTools=false) enables exactly the core category on native', () => {
    const nativeCore = nativeGlobalInitialEnabledNames(consolidatedToolDefinitions, false);
    expect(nativeCore).toHaveLength(CORE_TOOL_COUNT);
    for (const name of nativeCore) {
      const def = consolidatedToolDefinitions.find((d) => d.name === name);
      expect(def?.category ?? 'utility').toBe('core');
    }
  });

  it('the native core-only seed equals the TS core-only target (disable non-core tools)', () => {
    const nativeCore = nativeGlobalInitialEnabledNames(consolidatedToolDefinitions, false);
    const ts = new SessionConfigureStore();
    ts.disableTools(A, NON_CORE_TOOL_NAMES);
    expect(tsSessionEnabled(ts, A)).toEqual(nativeCore);
    expect(nativeCore).toEqual(expect.arrayContaining(['manage_tools', 'inspect']));
  });

  it('true vs false profiles differ by exactly the non-core tools, core is a subset of all', () => {
    const all = nativeGlobalInitialEnabledNames(consolidatedToolDefinitions, true);
    const core = nativeGlobalInitialEnabledNames(consolidatedToolDefinitions, false);
    const nonCore = all.filter((n) => !core.includes(n)).sort();
    expect(nonCore).toEqual([...NON_CORE_TOOL_NAMES].sort());
    expect(core.every((n) => all.includes(n))).toBe(true);
  });

  it('the core-only profile preserves two-session isolation on native', () => {
    const nat = new NativeSessionConfigureStore(() => consolidatedToolDefinitions);
    nat.disableTools(A, NON_CORE_TOOL_NAMES);
    expect(nat.enabledToolNames(A)).toEqual(nativeGlobalInitialEnabledNames(consolidatedToolDefinitions, false));
    expect(nat.enabledToolNames(B)).toHaveLength(TOTAL_TOOL_COUNT);
    expect(nat.getCatalogStateRevision(B)).toBe(0);
  });
});
