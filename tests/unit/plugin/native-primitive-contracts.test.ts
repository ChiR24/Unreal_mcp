import { createHash } from 'node:crypto';
import { readdirSync, readFileSync } from 'node:fs';
import { basename, resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Source-contract lane for plan Task 37, "Wire MCP primitive handlers and
// advertise only the implemented session profile", on the native `/mcp` surface.
//
// Tasks 31-36 already shipped the native primitive MODULES (resource catalog +
// revision, prompt catalog, completion provider, subscription store, notification
// coalescer, session/client profiles) but left them UNWIRED — every one of their
// headers carries a "Task 37 owns wiring" note. This suite is the RED contract for
// that wiring. Like the sibling `mcp_*_contracts` / `task-28-*` suites it reads the
// plugin C++ (and the TS server surface) as text, because no live-editor HTTP
// harness runs in CI; the serialized UE BuildPlugin gate remains the authoritative
// compile proof.
//
// Two independently runnable groups:
//   * "Task 37 BASELINE" pins the seams/baselines Task 37 must PRESERVE (the
//     teardown funnel it hangs the cleanup on, the reusable async writer it must
//     reuse, the byte-identical existing Notifications unit, the Lifecycle ceiling).
//     These PASS on this tree — they prove the harness/paths are sound, so the
//     DESIRED failures below are true contract gaps, not helper bugs.
//   * "Task 37 DESIRED" encodes the wiring/capability/notification/cleanup contract
//     that does NOT exist yet and is therefore deliberately RED until Task 37 lands.
//
// The two new Transport translation units are read through `readMaybe`, which maps
// a not-yet-created file to '' so a missing file fails a token/existence assertion
// (the intended RED) instead of throwing during collection.

const root = process.cwd();
const transportDir = resolve(
  root,
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Transport',
);
const pluginPrivateRoot = resolve(
  root,
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
);

const readTransport = (name: string): string =>
  readFileSync(resolve(transportDir, name), 'utf8');

/** Absent-file-tolerant read: a not-yet-created Task 37 unit reads as ''. */
const readMaybe = (absPath: string): string => {
  try {
    return readFileSync(absPath, 'utf8');
  } catch {
    return '';
  }
};

const sha256 = (text: string): string =>
  createHash('sha256').update(text, 'utf8').digest('hex');

// Repo-canonical pure-line measure (identical to source_structure_contracts):
// non-blank lines that are not a `//` or `#` comment/directive.
const countPureLines = (source: string): number =>
  source
    .split(/\r?\n/u)
    .filter((line) => !/^\s*$/u.test(line) && !/^\s*(?:#|\/\/)/u.test(line))
    .length;

const sliceBetween = (source: string, start: string, end: string): string => {
  const from = source.indexOf(start);
  if (from === -1) {
    return '';
  }
  const to = source.indexOf(end, from + start.length);
  return source.slice(from, to === -1 ? undefined : to);
};

const splitArtifactPattern =
  /Common.*\.(?:cpp|cs|h)$|(?:^|[_-])Part(?:[_-]?\d+)?\.(?:cpp|cs|h)$|(?:^|[_-])\d+\.(?:cpp|cs|h)$|\.in[cl]$/u;

const listAllBasenames = (directory: string): Set<string> => {
  const names = new Set<string>();
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    if (entry.isDirectory()) {
      for (const nested of listAllBasenames(resolve(directory, entry.name))) {
        names.add(nested);
      }
    } else {
      names.add(entry.name);
    }
  }
  return names;
};

const unresolvedMcpIncludes = (source: string, known: Set<string>): string[] =>
  [...source.matchAll(/^\s*#include\s+"([^"]+)"/gmu)]
    .map((match) => basename(match[1] ?? ''))
    .filter(
      (name) =>
        name.startsWith('Mcp') &&
        !name.endsWith('.generated.h') &&
        !known.has(name),
    );

// ─── Recorded facts (baselines Task 37 must not disturb) ─────────────────────

/** The two new Transport translation units Task 37 must add — and only these two. */
const NEW_TRANSPORT_FILES = [
  'McpNativeTransportPrimitives.cpp',
  'McpNativeTransportPrimitiveNotifications.cpp',
];

/** Folder budget: 23 files today (23/25); +2 new = 25, still <= 25. */
const TRANSPORT_FILE_BUDGET = 25;

/** The single primitive JSON-RPC dispatch delegate and the single cleanup seam. */
const PRIMITIVE_DISPATCH = 'HandlePrimitiveMethod';
const CLEANUP_SEAM = 'ReleaseSessionPrimitives';

/**
 * sha256 of `McpNativeTransportNotifications.cpp` as it stands before Task 37.
 * Task 37 puts the new `resources/updated` engine in the NEW
 * `McpNativeTransportPrimitiveNotifications.cpp`, so this existing unit must stay
 * byte-identical. Recomputed from disk and asserted equal — a regression tripwire.
 */
const NOTIFICATIONS_BASELINE_SHA256 =
  'a37f6f36fdd3531f63e5b9446021067d30dd7170a495e0f33b2480c6484d0e1f';

const knownSourceNames = listAllBasenames(pluginPrivateRoot);
const transportFiles = readdirSync(transportDir).filter((name) =>
  /\.(?:cpp|h)$/u.test(name),
);

// Existing units (all present on this tree).
const connection = readTransport('McpNativeTransportConnection.cpp');
const toolDiscovery = readTransport('McpNativeTransportToolDiscovery.cpp');
const sessions = readTransport('McpNativeTransportSessions.cpp');
const lifecycle = readTransport('McpNativeTransportLifecycle.cpp');
const notifications = readTransport('McpNativeTransportNotifications.cpp');
const notificationWrites = readTransport('McpNativeTransportNotificationWrites.cpp');
const header = readTransport('McpNativeTransport.h');
const serverFactory = readFileSync(resolve(root, 'src/server/server-factory.ts'), 'utf8');

// New units (absent until Task 37 — read tolerantly so absence is a RED assertion).
const primitives = readMaybe(resolve(transportDir, 'McpNativeTransportPrimitives.cpp'));
const primitiveNotifications = readMaybe(
  resolve(transportDir, 'McpNativeTransportPrimitiveNotifications.cpp'),
);

// The native `capabilities` object built by HandleInitialize, and the TS mirror.
const nativeCapabilities = sliceBetween(
  toolDiscovery,
  'auto Capabilities = MakeShared<FJsonObject>();',
  'auto ServerInfo = MakeShared<FJsonObject>();',
);
const tsCapabilities = sliceBetween(serverFactory, 'capabilities: {', ');');

describe('Task 37 BASELINE: teardown funnel, reusable writer, and pinned baselines to preserve', () => {
  it('keeps the existing Notifications translation unit byte-identical to the recorded baseline', () => {
    // Task 37 adds the resources/updated engine in the NEW PrimitiveNotifications
    // unit; it must not edit this one.
    expect(sha256(notifications)).toBe(NOTIFICATIONS_BASELINE_SHA256);
    expect(countPureLines(notifications)).toBeLessThanOrEqual(250);
  });

  it('leaves Lifecycle within the 250 pure-line ceiling with headroom for the shutdown seam', () => {
    expect(countPureLines(lifecycle)).toBeLessThanOrEqual(250);
  });

  it('routes all four session-close moments through the CloseSessionConnections funnel', () => {
    // DELETE, init-eviction and failed-init all reach the one close funnel; the
    // inactivity-timeout close calls it from ValidateSession. Task 37 hangs the
    // single cleanup seam inside that funnel, so this routing must stay intact.
    const deleteBranch = sliceBetween(
      connection,
      'HttpReq.Method == TEXT("DELETE")',
      'HttpReq.Method == TEXT("GET")',
    );
    const failedInitBranch = sliceBetween(
      connection,
      'MarkSessionInitializationComplete(NewSessionId);',
      'if (Rpc.Method == TEXT("tools/list"))',
    );
    const evictionBranch = sliceBetween(
      toolDiscovery,
      'if (!EvictedSessionId.IsEmpty())',
      'auto Result = MakeShared<FJsonObject>();',
    );
    expect(deleteBranch).toContain('CloseSessionConnections(HttpReq.SessionId);');
    expect(failedInitBranch).toContain('CloseSessionConnections(NewSessionId);');
    expect(evictionBranch).toContain('CloseSessionConnections(EvictedSessionId);');
    expect(sessions).toContain('void FMcpNativeTransport::CloseSessionConnections(');
    expect(sessions).toContain('CloseSessionConnections(SessionId);');
  });

  it('drains every session on shutdown so the shutdown seam has a home', () => {
    const shutdownBody = sliceBetween(
      lifecycle,
      'void FMcpNativeTransport::Shutdown()',
      '// ─── Socket Helper',
    );
    expect(shutdownBody).toContain('ActiveSessions.Empty();');
  });

  it('owns QueueNotificationEventWrites as the single reusable async writer', () => {
    // Task 37 must REUSE this writer from the new unit, never re-implement it.
    expect(notificationWrites).toContain(
      'int32 FMcpNativeTransport::QueueNotificationEventWrites(',
    );
    expect(header).toContain('int32 QueueNotificationEventWrites(');
  });
});

describe('Task 37 DESIRED: two new primitive Transport units within the folder + line budgets', () => {
  it('adds exactly the two new primitive units and stays within the 25-file folder budget', () => {
    for (const name of NEW_TRANSPORT_FILES) {
      expect(transportFiles).toContain(name);
    }
    expect(transportFiles.length).toBeLessThanOrEqual(TRANSPORT_FILE_BUDGET);
  });

  it('names the new units so they are not mechanical split artifacts', () => {
    for (const name of NEW_TRANSPORT_FILES) {
      expect(splitArtifactPattern.test(name)).toBe(false);
    }
  });

  it('keeps each new primitive unit within the 250 pure-line ceiling', () => {
    expect(transportFiles).toContain('McpNativeTransportPrimitives.cpp');
    expect(transportFiles).toContain('McpNativeTransportPrimitiveNotifications.cpp');
    expect(countPureLines(primitives)).toBeLessThanOrEqual(250);
    expect(countPureLines(primitiveNotifications)).toBeLessThanOrEqual(250);
  });

  it('resolves every local Mcp include in the new units to a real source file', () => {
    expect(transportFiles).toContain('McpNativeTransportPrimitives.cpp');
    expect(transportFiles).toContain('McpNativeTransportPrimitiveNotifications.cpp');
    expect(unresolvedMcpIncludes(primitives, knownSourceNames)).toEqual([]);
    expect(unresolvedMcpIncludes(primitiveNotifications, knownSourceNames)).toEqual([]);
  });
});

describe('Task 37 DESIRED: primitive JSON-RPC dispatch before method-not-found', () => {
  it('delegates through HandlePrimitiveMethod before the ErrorMethodNotFound fallback', () => {
    expect(connection).toContain(`${PRIMITIVE_DISPATCH}(`);
    // Delegation is positioned after tools/call and strictly before the
    // "Unknown method" fallback so an implemented primitive never 404s.
    expect(connection.indexOf(PRIMITIVE_DISPATCH)).toBeGreaterThan(
      connection.indexOf('TEXT("tools/call")'),
    );
    expect(connection.indexOf(PRIMITIVE_DISPATCH)).toBeLessThan(
      connection.indexOf('ErrorMethodNotFound'),
    );
    expect(header).toContain(`${PRIMITIVE_DISPATCH}(`);
  });

  it('implements the primitive method surface and typed-unavailable live reads in the new unit', () => {
    expect(transportFiles).toContain('McpNativeTransportPrimitives.cpp');
    for (const method of [
      'resources/list',
      'resources/templates/list',
      'resources/read',
      'resources/subscribe',
      'resources/unsubscribe',
      'prompts/list',
      'prompts/get',
      'completion/complete',
    ]) {
      expect(primitives).toContain(method);
    }
    // Static capability/project reads compose the Task 31/36 modules...
    for (const include of [
      'MCP/Resources/McpResourceCatalog.h',
      'MCP/Resources/McpResourceUri.h',
      'MCP/Primitives/McpResourceRevision.h',
      'MCP/Primitives/McpPromptCatalog.h',
      'MCP/Primitives/McpCompletionProvider.h',
      'MCP/Primitives/McpSubscriptionStore.h',
    ]) {
      expect(primitives).toContain(`#include "${include}"`);
    }
    // ...and live editor-state reads return a typed-unavailable code, never an
    // off-thread editor scan.
    expect(primitives).toContain('RESOURCE_UNAVAILABLE');
    for (const offThreadEditorApi of [
      'GEditor',
      'GWorld',
      'GetSelectedActors',
      'UEditorActorSubsystem',
      'FSlateApplication',
    ]) {
      expect(primitives).not.toContain(offThreadEditorApi);
    }
  });
});

describe('Task 37 DESIRED: capability advertisement matches the implemented session profile', () => {
  it('advertises tools + resources(subscribe) + prompts + completions on native initialize', () => {
    expect(nativeCapabilities).toContain('SetObjectField(TEXT("tools")');
    expect(nativeCapabilities).toContain('TEXT("resources")');
    expect(nativeCapabilities).toContain('TEXT("subscribe")');
    expect(nativeCapabilities).toContain('TEXT("prompts")');
    expect(nativeCapabilities).toContain('TEXT("completions")');
  });

  it('mirrors that matrix on the TypeScript server surface', () => {
    expect(tsCapabilities).toContain('tools');
    expect(tsCapabilities).toContain('resources');
    expect(tsCapabilities).toContain('subscribe');
    expect(tsCapabilities).toContain('prompts');
    expect(tsCapabilities).toContain('completions');
  });

  it('never advertises logging or any list_changed, and advertises tasks on BOTH surfaces or neither', () => {
    // Match the real advertisement form (Set*Field(TEXT("..."))), not the bare
    // word — HandleInitialize's comment legitimately mentions "listChanged" while
    // documenting that it is omitted.
    for (const unbacked of ['TEXT("logging")', 'TEXT("listChanged")']) {
      expect(toolDiscovery).not.toContain(unbacked);
    }
    for (const unbacked of ['logging', 'listChanged']) {
      expect(tsCapabilities).not.toContain(unbacked);
    }
    // Task 44: tasks is backed on both transports, so both literals must carry
    // it. Asserting the pair together is what makes this a parity gate — either
    // surface advertising Tasks alone is the divergence, and it fails here.
    expect(toolDiscovery).toContain('TEXT("tasks")');
    expect(tsCapabilities).toContain('tasks:');
  });
});

describe('Task 37 DESIRED: resources/updated notifications reuse the writer with a URI-only wire payload', () => {
  it('builds resources/updated through the reused writer over the Task 34 coalescer', () => {
    expect(transportFiles).toContain('McpNativeTransportPrimitiveNotifications.cpp');
    expect(primitiveNotifications).toContain('notifications/resources/updated');
    expect(primitiveNotifications).toContain('QueueNotificationEventWrites(');
    expect(primitiveNotifications).toContain(
      '#include "MCP/Primitives/McpNotificationCoalescer.h"',
    );
    // The new unit calls the writer; it must not re-declare it.
    expect(primitiveNotifications).not.toContain(
      'int32 FMcpNativeTransport::QueueNotificationEventWrites(',
    );
  });

  it('puts only the URI on the wire — no revision, change kind, or list_changed', () => {
    expect(transportFiles).toContain('McpNativeTransportPrimitiveNotifications.cpp');
    expect(primitiveNotifications).toContain('SetStringField(TEXT("uri")');
    // The coalescer payload carries uri/revision/changeKind internally, but the
    // MCP resources/updated wire params are URI-only; the client re-reads.
    expect(primitiveNotifications).not.toContain('TEXT("revision")');
    expect(primitiveNotifications).not.toContain('TEXT("changeKind")');
    // Resources and prompts have no list_changed on this surface.
    expect(primitiveNotifications).not.toContain('resources/list_changed');
    expect(primitiveNotifications).not.toContain('prompts/list_changed');
  });
});

describe('Task 37 DESIRED: a single primitive cleanup seam at every session-teardown moment', () => {
  it('declares the seam plus the per-session primitive state it releases on the transport', () => {
    expect(header).toContain(`${CLEANUP_SEAM}(`);
    expect(header).toContain('FMcpSubscriptionStore');
    expect(header).toContain('FMcpNotificationCoalescer');
  });

  it('defines the one seam in the new primitives unit and clears the per-session state', () => {
    expect(transportFiles).toContain('McpNativeTransportPrimitives.cpp');
    expect(primitives).toContain(`void FMcpNativeTransport::${CLEANUP_SEAM}(`);
    expect(primitives).toContain('ClearSession(');
  });

  it('invokes that one seam at the close funnel and on shutdown, covering all five moments', () => {
    // DELETE, eviction and failed-init all route through CloseSessionConnections
    // (asserted in BASELINE), so the seam inside the funnel covers them; the
    // inactivity-timeout close reaches it there too. Shutdown adds the fifth.
    const closeFunnel = sliceBetween(
      sessions,
      'void FMcpNativeTransport::CloseSessionConnections(',
      '// ─── Helpers',
    );
    const shutdownBody = sliceBetween(
      lifecycle,
      'void FMcpNativeTransport::Shutdown()',
      '// ─── Socket Helper',
    );
    expect(closeFunnel).toContain(`${CLEANUP_SEAM}(`);
    expect(shutdownBody).toContain(`${CLEANUP_SEAM}(`);
  });
});

describe('Task 37 DESIRED: configure → SessionConfigureStore → SyncCatalog → resources/updated is wired', () => {
  // Pre-fix, the configure op routed ONLY through ToolManager.HandleAction, so
  // every FMcpSessionConfigureStore mutator and coalescer SyncCatalog had zero
  // callers (dead code). These assertions are RED on that tree and prove the fix
  // connects the first hop; the later hops (FlushDue → resources/updated) already
  // shipped with Tasks 34-37 and are untouched here.
  const gateway = readTransport('McpNativeTransportGateway.cpp');
  const coalescerCpp = readFileSync(
    resolve(pluginPrivateRoot, 'MCP/Primitives/McpNotificationCoalescer.cpp'),
    'utf8',
  );
  const configureBlock = sliceBetween(
    gateway,
    'Operation == TEXT("configure")',
    'Operation == TEXT("execute")',
  );

  it('references the per-session configure store and the catalog sync from the configure op', () => {
    expect(configureBlock).toContain('SessionConfigureStore');
    expect(configureBlock).toContain('SyncCatalog');
  });

  it('gives coalescer SyncCatalog a real caller on the gateway configure path', () => {
    // Baseline: SyncCatalog is still defined in the coalescer translation unit...
    expect(coalescerCpp).toContain('bool FMcpNotificationCoalescer::SyncCatalog(');
    // ...and Task 37 now invokes it (as a method call) from the gateway. Before
    // the fix it had zero callers, so this match is the dead-branch tripwire.
    expect(gateway).toMatch(/(?:->|\.)\s*SyncCatalog\(/u);
  });

  it('mutates the configure overlay through at least one revisioned store mutator', () => {
    // Effective-change semantics ride the store: a mutator only advances the
    // overlay revision on a real change, so SyncCatalog stays a no-op for a no-op
    // configure. At least one revisioned mutator must be reached from the gateway.
    const mutators = ['SeedFrom(', 'EnableTools(', 'DisableTools(', 'DisableCategory('];
    expect(mutators.some((mutator) => gateway.includes(mutator))).toBe(true);
  });

  it('clears the per-session configure overlay on session teardown', () => {
    const releaseBody = sliceBetween(
      primitives,
      'void FMcpNativeTransport::ReleaseSessionPrimitives(',
      'bool FMcpNativeTransport::HandlePrimitiveMethod(',
    );
    expect(releaseBody).toContain('SessionConfigureStore.ClearSession(');
  });
});
