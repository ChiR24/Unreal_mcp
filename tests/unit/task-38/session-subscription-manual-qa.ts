// tests/unit/task-38/session-subscription-manual-qa.ts
//
// Task 38 lane C — deterministic manual-QA driver (NOT a Vitest test).
//
// Exact invocation:
//   node --loader ts-node/esm tests/unit/task-38/session-subscription-manual-qa.ts
//
// It drives two sessions over a deterministic fake millisecond clock across BOTH
// the TypeScript production primitives and the executable native fixture, and
// records a single verdict to .omo/evidence/task-38/session-manual-qa.json. It
// PASSES only when both required properties hold on both surfaces:
//   (1) independentRevisions      — each session's catalog-state revision advances
//                                    solely from its own effective mutations.
//   (2) zeroPostCloseNotifications — after a session is closed, no notification is
//                                    ever delivered (with a live positive control
//                                    proving the path is not vacuously silent).
// Exit code is 0 on PASS, 1 on FAIL.

import { execFileSync } from 'node:child_process';
import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  asResourceRevision,
  InMemoryRevisionProvider,
  type SubscribableUri,
} from '../../../src/server/mcp-primitives/resource-revision.js';
import { SubscriptionStore } from '../../../src/server/mcp-primitives/subscriptions/subscription-store.js';
import { NotificationCoalescer } from '../../../src/server/mcp-primitives/subscriptions/notification-coalescer.js';
import { CATALOG_SUBSCRIPTION_URI } from '../../../src/server/mcp-primitives/subscriptions/subscription-types.js';
import {
  PrimitiveNotificationDriver,
  type NotifyingServer,
} from '../../../src/server/mcp-primitives/primitive-notifications.js';
import { SessionConfigureStore } from '../../../src/server/mcp-primitives/session-configure-store.js';
import type { CatalogRevisionReader } from '../../../src/server/mcp-primitives/catalog-revision-reader.js';
import { consolidatedToolDefinitions } from '../../../src/tools/catalog/consolidated-tool-definitions.js';
import {
  NativeNotificationCoalescer,
  NativeSessionConfigureStore,
  NativeSubscriptionStore,
} from './session-subscription-native-fixture.js';

const A = 'session-A';
const B = 'session-B';
const SELECTION = 'ue://selection';
const LEVEL = 'ue://level';

interface CheckResult {
  name: string;
  surface: 'ts' | 'native' | 'ts+native';
  pass: boolean;
  detail: string;
}

class MutableCatalogReader implements CatalogRevisionReader {
  private readonly counts = new Map<string, number>();
  getCatalogStateRevision(sessionId: string): number {
    return this.counts.get(sessionId) ?? 0;
  }
  bump(sessionId: string): void {
    this.counts.set(sessionId, this.getCatalogStateRevision(sessionId) + 1);
  }
}

const checks: CheckResult[] = [];
function record(name: string, surface: CheckResult['surface'], pass: boolean, detail: string): void {
  checks.push({ name, surface, pass, detail });
}

// ---------------------------------------------------------------------------
// Property 1 — independent per-session revisions (fake-clock free; pure state).
// ---------------------------------------------------------------------------

function checkIndependentRevisions(): boolean {
  const ts = new SessionConfigureStore();
  const nat = new NativeSessionConfigureStore(() => consolidatedToolDefinitions);

  // Session A: two effective batches. Session B: one effective batch.
  ts.disableCategory(A, 'gameplay');
  ts.disableTools(A, ['manage_asset']);
  ts.disableTools(B, ['manage_level']);
  nat.disableCategory(A, 'gameplay');
  nat.disableTools(A, ['manage_asset']);
  nat.disableTools(B, ['manage_level']);

  const tsA = ts.getCatalogStateRevision(A);
  const tsB = ts.getCatalogStateRevision(B);
  const natA = nat.getCatalogStateRevision(A);
  const natB = nat.getCatalogStateRevision(B);
  const bUnaffectedByA = ts.isToolEnabled(B, 'manage_ai') && nat.isToolEnabled(B, 'manage_ai');

  const tsOk = tsA === 2 && tsB === 1;
  const natOk = natA === 2 && natB === 1;
  record('independent-revisions', 'ts', tsOk, `A=${tsA} (expect 2), B=${tsB} (expect 1)`);
  record('independent-revisions', 'native', natOk, `A=${natA} (expect 2), B=${natB} (expect 1)`);
  record(
    'session-B-isolated-from-A',
    'ts+native',
    bUnaffectedByA,
    `B.manage_ai enabled after A disabled gameplay: ${bUnaffectedByA}`,
  );
  return tsOk && natOk && bUnaffectedByA;
}

// ---------------------------------------------------------------------------
// Property 2a — zero post-close notifications, deterministic fake clock.
// ---------------------------------------------------------------------------

function checkZeroPostCloseFakeClock(): boolean {
  const clock = { v: 0 };
  const revisions = new InMemoryRevisionProvider();
  const tsEmitted: string[] = [];
  const natEmitted: string[] = [];

  const tsStore = new SubscriptionStore();
  const tsCo = new NotificationCoalescer({
    store: tsStore,
    revisions,
    catalog: new MutableCatalogReader(),
    clock: () => clock.v,
    windowMs: 50,
    sink: (sessionId) => tsEmitted.push(sessionId),
  });
  const natStore = new NativeSubscriptionStore();
  const natCo = new NativeNotificationCoalescer(
    natStore,
    (uri) => revisions.currentRevision(uri as SubscribableUri),
    (sessionId) => natEmitted.push(sessionId),
    () => clock.v,
    50,
  );

  for (const [store, co, sink] of [
    [tsStore, tsCo, tsEmitted] as const,
    [natStore, natCo, natEmitted] as const,
  ]) {
    store.subscribe(A, SELECTION);
    store.subscribe(B, LEVEL);
    revisions.set(SELECTION, asResourceRevision(2));
    revisions.set(LEVEL, asResourceRevision(2));
    // Live positive control: an open session must actually receive a notification.
    co.recordChange(A, SELECTION, 'updated');
    co.recordChange(B, LEVEL, 'updated');
    clock.v += 50;
    co.flushDue(clock.v);
    const liveCount = sink.length;
    // Now queue new changes, close BOTH sessions, and flush past the window.
    revisions.set(SELECTION, asResourceRevision(3));
    revisions.set(LEVEL, asResourceRevision(3));
    co.recordChange(A, SELECTION, 'updated');
    co.recordChange(B, LEVEL, 'updated');
    store.clearSession(A);
    store.clearSession(B);
    co.clearSession(A);
    co.clearSession(B);
    clock.v += 100;
    co.flushDue(clock.v);
    const postCloseCount = sink.length - liveCount;
    const surface = store === tsStore ? 'ts' : 'native';
    record(
      'zero-post-close-notifications-fakeclock',
      surface,
      liveCount === 2 && postCloseCount === 0,
      `live=${liveCount} (expect 2), postClose=${postCloseCount} (expect 0)`,
    );
  }

  const tsOk = tsEmitted.length === 2;
  const natOk = natEmitted.length === 2;
  return tsOk && natOk;
}

// ---------------------------------------------------------------------------
// Property 2b — end-to-end driver post-close over the real notification path.
// ---------------------------------------------------------------------------

async function checkZeroPostCloseDriver(): Promise<boolean> {
  const notifications: Array<{ uri: unknown }> = [];
  const server: NotifyingServer = {
    notification: async (n) => {
      notifications.push({ uri: n.params?.uri });
    },
  };
  const revisions = new InMemoryRevisionProvider();
  const catalog = new MutableCatalogReader();
  const driver = new PrimitiveNotificationDriver({ server, revisions, catalog });
  const wait = (ms: number): Promise<void> => new Promise((r) => setTimeout(r, ms));

  driver.store.subscribe(A, CATALOG_SUBSCRIPTION_URI);
  revisions.set(CATALOG_SUBSCRIPTION_URI, asResourceRevision(3));
  catalog.bump(A);
  driver.syncCatalog(A);
  await wait(150);
  const liveCount = notifications.length;

  revisions.set(CATALOG_SUBSCRIPTION_URI, asResourceRevision(4));
  catalog.bump(A);
  driver.syncCatalog(A);
  driver.releaseSession(A);
  await wait(150);
  const postCloseCount = notifications.length - liveCount;
  driver.dispose();

  const uriOnly = notifications.every((n) => n.uri === CATALOG_SUBSCRIPTION_URI);
  const pass = liveCount === 1 && postCloseCount === 0 && uriOnly;
  record(
    'zero-post-close-notifications-driver',
    'ts',
    pass,
    `live=${liveCount} (expect 1), postClose=${postCloseCount} (expect 0), uriOnly=${uriOnly}`,
  );
  return pass;
}

function gitHead(): string {
  try {
    return execFileSync('git', ['rev-parse', '--short', 'HEAD'], { encoding: 'utf8' }).trim();
  } catch {
    return 'unknown';
  }
}

async function main(): Promise<void> {
  const independentRevisions = checkIndependentRevisions();
  const zeroFakeClock = checkZeroPostCloseFakeClock();
  const zeroDriver = await checkZeroPostCloseDriver();
  const zeroPostCloseNotifications = zeroFakeClock && zeroDriver;

  const pass = independentRevisions && zeroPostCloseNotifications;
  const here = dirname(fileURLToPath(import.meta.url));
  const artifact = resolve(here, '../../../.omo/evidence/task-38/session-manual-qa.json');
  mkdirSync(dirname(artifact), { recursive: true });
  const report = {
    task: 38,
    lane: 'C',
    kind: 'manual-qa',
    title: 'Deterministic fake-clock/two-session subscription driver',
    invocation: 'node --loader ts-node/esm tests/unit/task-38/session-subscription-manual-qa.ts',
    clock: 'deterministic fake millisecond counter (properties 1 & 2a) + bounded real-timer end-to-end control (2b)',
    sessions: [A, B],
    surfaces: ['ts', 'native-fixture'],
    properties: {
      independentRevisions,
      zeroPostCloseNotifications,
    },
    checks,
    result: pass ? 'PASS' : 'FAIL',
    generatedAt: new Date().toISOString(),
    gitHead: gitHead(),
    packageVersion: '0.5.30',
    note: 'PASS requires BOTH independent per-session revisions AND zero post-close notifications on both surfaces. Native surface is the executable fixture (session-subscription-native-fixture.ts), grounded in cited plugin C++.',
  };
  writeFileSync(artifact, `${JSON.stringify(report, null, 2)}\n`, 'utf8');
  process.stdout.write(`${report.result}: manual QA -> ${artifact}\n`);
  for (const c of checks) {
    process.stdout.write(`  [${c.pass ? 'PASS' : 'FAIL'}] ${c.name} (${c.surface}) — ${c.detail}\n`);
  }
  process.exitCode = pass ? 0 : 1;
}

void main();
