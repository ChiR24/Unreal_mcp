import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Source-contract lane for plan Task 34: the revisioned per-session subscription
// store and the bounded debounced/coalesced notification engine, on both the
// TypeScript and native surfaces. Like the other plugin contract suites it reads
// C++/TS source text because no live-editor HTTP harness runs in CI; the
// serialized UE BuildPlugin gate remains the authoritative compile proof.
//
// Two groups: "Task 34 BASELINE" pins behavior that already holds and Task 34
// must not disturb (subscriptions unadvertised/unwired, Task 30 list-changed
// suppressed) and passes today; the mirror/contract groups assert the new
// store + coalescer that this task introduces.

const root = process.cwd();
const nativeRoot = resolve(root, 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP');

const primitive = (rel: string): string => readFileSync(resolve(nativeRoot, 'Primitives', rel), 'utf8');
const transport = (rel: string): string => readFileSync(resolve(nativeRoot, 'Transport', rel), 'utf8');

const storeHeader = primitive('McpSubscriptionStore.h');
const storeSource = primitive('McpSubscriptionStore.cpp');
const coalescerHeader = primitive('McpNotificationCoalescer.h');
const coalescerSource = primitive('McpNotificationCoalescer.cpp');

const tsTypes = readFileSync(resolve(root, 'src/server/mcp-primitives/subscriptions/subscription-types.ts'), 'utf8');
const tsStore = readFileSync(resolve(root, 'src/server/mcp-primitives/subscriptions/subscription-store.ts'), 'utf8');
const tsCoalescer = readFileSync(resolve(root, 'src/server/mcp-primitives/subscriptions/notification-coalescer.ts'), 'utf8');

const serverFactory = readFileSync(resolve(root, 'src/server/server-factory.ts'), 'utf8');
const discovery = transport('McpNativeTransportToolDiscovery.cpp');
const sessions = transport('McpNativeTransportSessions.cpp');
const lifecycle = transport('McpNativeTransportLifecycle.cpp');

const countPureLines = (source: string): number =>
  source.split(/\r?\n/u).filter((line) => !/^\s*$/u.test(line) && !/^\s*(?:#|\/\/)/u.test(line)).length;

const sliceBetween = (source: string, start: string, end: string): string => {
  const from = source.indexOf(start);
  const to = source.indexOf(end, from + start.length);
  return source.slice(from, to === -1 ? undefined : to);
};

describe('Task 34 BASELINE: subscriptions stay unadvertised/unwired and Task 30 list-changed suppressed', () => {
  it('advertises the Task 37 wired primitives on the TS surface (tools + resources.subscribe + prompts + completions), never logging/tasks', () => {
    const capabilities = sliceBetween(serverFactory, 'capabilities: {', ');');
    // Task 37 wired the subscribe/prompts/completions primitives, so the TS
    // surface now advertises them truthfully (they are backed by real handlers).
    // The `resources` object gained `subscribe: true`.
    expect(capabilities).toContain('tools: {}');
    expect(capabilities).toContain('resources: { subscribe: true }');
    expect(capabilities).toContain('prompts: {}');
    expect(capabilities).toContain('completions: {}');
    // Still-unbacked primitives are never advertised: no `subscriptions` client
    // key, no logging, no tasks, no list-changed member.
    for (const unbacked of ['subscriptions', 'logging', 'tasks', 'listChanged']) {
      expect(capabilities).not.toContain(unbacked);
    }
  });

  it('does not wire the subscription modules into TS server construction yet (Task 37 owns that)', () => {
    for (const token of ['subscriptions/', 'NotificationCoalescer', 'SubscriptionStore']) {
      expect(serverFactory).not.toContain(token);
    }
  });

  it('keeps the native tools capability omitting listChanged and the broadcast suppressed', () => {
    expect(discovery).toContain('omits listChanged entirely');
    expect(discovery).not.toContain('SetBoolField(TEXT("listChanged")');
    expect(sessions).toContain('suppressed (public surface is a static single tool)');
  });

  it('does not wire the native store/coalescer into the transport yet (Task 37 owns that)', () => {
    for (const source of [discovery, sessions, lifecycle]) {
      expect(source).not.toContain('McpSubscriptionStore');
      expect(source).not.toContain('McpNotificationCoalescer');
      expect(source).not.toContain('resources/updated');
    }
  });
});

describe('Task 34 native subscription store source contracts', () => {
  it('keys subscriptions by explicit session id over the Task 31 allowlist', () => {
    expect(storeHeader).toContain('class FMcpSubscriptionStore');
    expect(storeHeader).toContain('#include "MCP/Primitives/McpResourceRevision.h"');
    expect(storeHeader).toContain('TMap<FString, TArray<FString>> Sessions');
    expect(storeHeader).toContain('struct FMcpSubscribeResult');
    for (const member of ['SetReleaseHook', 'FReleaseHook', 'Subscribe', 'Unsubscribe', 'IsSubscribed', 'Subscriptions', 'SessionsSubscribedTo', 'HasSession', 'ClearSession']) {
      expect(storeHeader).toContain(member);
    }
    // Boundary parse: only allowlisted URIs are stored.
    expect(storeSource).toContain('McpIsSubscribableUri(Uri)');
  });

  it('is idempotent on duplicate subscribe and rejects malformed session/URI without side effects', () => {
    expect(storeSource).toContain('Result.bAlreadySubscribed = true;');
    expect(storeSource).toContain('TEXT("INVALID_SESSION")');
    expect(storeSource).toContain('TEXT("NOT_SUBSCRIBABLE")');
  });

  it('caps a session and evicts the oldest deterministically, releasing it, and drains on clear', () => {
    // Oldest-first eviction is index 0 of the insertion-ordered array.
    expect(storeSource).toContain('Uris.RemoveAt(0);');
    expect(storeSource).toContain('FireRelease(SessionId, EvictedToRelease);');
    // clearSession removes the whole session map and releases each URI.
    expect(storeSource).toContain('Sessions.Remove(SessionId);');
    expect(storeSource).toContain('FireRelease(SessionId, Uri);');
  });
});

describe('Task 34 native notification coalescer source contracts', () => {
  it('composes the store, the Task 31 revision source, and the Task 36 catalog reader', () => {
    expect(coalescerHeader).toContain('class FMcpNotificationCoalescer');
    expect(coalescerHeader).toContain('#include "MCP/Primitives/McpSubscriptionStore.h"');
    expect(coalescerHeader).toContain('#include "MCP/Primitives/IMcpCatalogRevisionReader.h"');
    expect(coalescerHeader).toContain('#include "MCP/Primitives/McpResourceRevision.h"');
    for (const member of ['RecordChange', 'SyncCatalog', 'RecordGlobalChange', 'FlushDue', 'NextDueAt', 'PendingCount', 'DropPending', 'ClearSession']) {
      expect(coalescerHeader).toContain(member);
    }
  });

  it('emits a bounded payload carrying only URI/revision/change kind', () => {
    const payload = sliceBetween(coalescerHeader, 'struct FMcpResourceUpdatedPayload', '};');
    expect(payload).toContain('FString Uri;');
    expect(payload).toContain('FMcpResourceRevision Revision');
    expect(payload).toContain('FString ChangeKind;');
    for (const forbidden of ['Data', 'Body', 'Diff', 'Path', 'Snapshot', 'Contents']) {
      expect(payload).not.toContain(forbidden);
    }
  });

  it('coalesces by session+URI, stamps the revision source, and enforces monotonic + late suppression', () => {
    // Fixed coalescing window off the injected clock.
    expect(coalescerSource).toContain('P.DueAt = Clock() + WindowMs;');
    // Stamp the revision a subsequent resource read returns (Task 31 source).
    expect(coalescerSource).toContain('RevisionSource(Change.Uri)');
    // A flush after unsubscribe/clear is suppressed by re-checking the live subscription.
    expect(coalescerSource).toContain('if (!Store.IsSubscribed(Change.SessionId, Change.Uri))');
    // Monotonic: a stale lower revision never emits.
    expect(coalescerSource).toContain('if (Revision < *Previous)');
    // Catalog cursor is driven by the Task 36 reader.
    expect(coalescerSource).toContain('Catalog.GetCatalogStateRevision(SessionId)');
  });

  it('stays a pure engine: no transport, subsystem, list-changed, unsafe save, or host path', () => {
    for (const source of [storeHeader, storeSource, coalescerHeader, coalescerSource]) {
      for (const forbidden of [
        'McpNativeTransport',
        'Subsystem',
        'notifications/tools/list_changed',
        'BroadcastNotification',
        'UPackage::SavePackage',
        'C:\\',
        '/home/',
        '/Users/',
        '.uproject',
      ]) {
        expect(source).not.toContain(forbidden);
      }
    }
  });

  it('keeps each owned native file within the 250 pure-line ceiling', () => {
    for (const source of [storeHeader, storeSource, coalescerHeader, coalescerSource]) {
      expect(countPureLines(source)).toBeLessThanOrEqual(250);
    }
  });
});

describe('Task 34 TypeScript store + coalescer contracts and cross-surface parity', () => {
  it('implements the pure store: session-keyed, cap-evicting, release-hooked', () => {
    expect(tsStore).toContain('class SubscriptionStore');
    expect(tsStore).toContain('clearSession(sessionId: string): number');
    for (const token of ['maxPerSession', 'onRelease', 'evicted', 'NOT_SUBSCRIBABLE', 'INVALID_SESSION', 'alreadySubscribed']) {
      expect(tsStore).toContain(token);
    }
  });

  it('implements the coalescer over the Task 31 and Task 36 primitives without editing them', () => {
    expect(tsCoalescer).toContain('class NotificationCoalescer');
    expect(tsCoalescer).toContain("from '../resource-revision.js'");
    expect(tsCoalescer).toContain("from '../catalog-revision-reader.js'");
    for (const token of ['RevisionProvider', 'CatalogRevisionReader', 'recordChange', 'syncCatalog', 'flushDue', 'isSubscribed', 'revision < previous']) {
      expect(tsCoalescer).toContain(token);
    }
  });

  it('mirrors the bounded payload shape and the closed change-kind set on both surfaces', () => {
    const tsPayload = sliceBetween(tsTypes, 'interface ResourceUpdatedPayload', '}');
    expect(tsPayload).toContain('uri');
    expect(tsPayload).toContain('revision');
    expect(tsPayload).toContain('changeKind');
    for (const forbidden of ['data', 'body', 'diff']) {
      expect(tsPayload).not.toContain(forbidden);
    }
    for (const kind of ['updated', 'invalidated', 'removed']) {
      expect(tsTypes).toContain(`'${kind}'`);
      expect(coalescerSource).toContain(`TEXT("${kind}")`);
    }
  });
});
