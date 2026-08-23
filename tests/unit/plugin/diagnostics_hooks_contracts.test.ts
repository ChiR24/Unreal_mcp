// Todo 9 (BB-005) lane 2 - diagnostics hook wiring source contracts (H1-H8).
// These assertions read the plugin source text (what the compiler sees), so a
// claim that exists only in a comment, a plan, or a TypeScript type cannot
// pass. This file is the ENFORCEABLE RED gate for the hook lane: before the
// H1-H8 edits exist, every call-site assertion below fails.
// Thread/persist rules enforced here:
//   * H1 startup rotation on the game thread after the commandlet early return
//     and before request acceptance (InitializeFromGameThread caches the root;
//     RotateOnStartup persists internally).
//   * H2 admission captures queue depth IN the PendingAutomationRequestsMutex
//     scope (NF-2) and records handshake success on EVERY WebSocket admission,
//     latch-free (NF-1: the string bWsHandshakeRecorded must never appear).
//   * H3 refusals are memory-only at socket-thread call sites; their disk
//     writes coalesce into the next game-thread persist.
//   * H4 pre-dispatch captures the batch count in-lock (NF-3) and persists
//     inline on the game thread inside the ExecutionLock block - the crash
//     anchor.
//   * H5 terminal records in the single response funnel before the transport
//     branch; PersistCurrent is gated on IsInGameThread.
//   * H6 WS close-code discrimination covers 4004 AND 4005 as handshake
//     failures (NF-5); disconnect summaries + deferred game-thread persist.
//   * H7 native session-create records + deferred game-thread persist.
//   * H8 native session-close dedupes via a bounded first-close-wins set
//     (ClaimSessionClose, cap 128, oldest-first eviction) scoped to the
//     retained 128-close window (NF-4); NO ActiveSessions.Contains gate in the
//     funnel (dead code - every caller pre-removes).
//   * NF-7 (CORRECTED3): in the three socket-thread files (SocketEvents,
//     ToolDiscovery, Sessions) every PersistCurrent( open-paren offset must be
//     PROVEN inside the argument/lambda extent of an AsyncTask call whose FIRST
//     argument is ENamedThreads::GameThread, via the bounded lexical extractor
//     below (mask -> locate -> backward AsyncTask search -> forward delimiter
//     match -> strict paren containment + first-argument equality). Bare
//     PersistCurrent is allowed ONLY in Lifecycle (H1 - rotation persists
//     internally, no literal call), RequestQueue (H4, exactly one) and
//     Responses (H5, IsInGameThread-gated).

import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';
import { countPureLines, sliceBetween } from './plugin-contract-fixtures.js';

const PLUGIN = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge');
const LIFECYCLE = resolve(PLUGIN, 'Private/Core/Subsystem/McpAutomationBridgeSubsystemLifecycle.cpp');
const QUEUE = resolve(PLUGIN, 'Private/Core/Subsystem/McpAutomationBridgeSubsystemRequestQueue.cpp');
const RESPONSES = resolve(PLUGIN, 'Private/Core/Subsystem/McpAutomationBridgeSubsystemResponses.cpp');
const SOCKET_EVENTS = resolve(PLUGIN, 'Private/Transport/Connection/McpConnectionManagerSocketEvents.cpp');
const SESSIONS = resolve(PLUGIN, 'Private/MCP/Transport/McpNativeTransportSessions.cpp');
const TOOL_DISCOVERY = resolve(PLUGIN, 'Private/MCP/Transport/McpNativeTransportToolDiscovery.cpp');
const NATIVE_HOOK_TESTS = resolve(PLUGIN, 'Private/Tests/Diagnostics/McpDiagnosticsHookSequenceTests.cpp');
const DIAG_INCLUDE = '#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"';

const read = (path: string): string => {
  expect(existsSync(path), `missing file: ${path}`).toBe(true);
  return readFileSync(path, 'utf8');
};

// Strip comments so a claim in prose cannot satisfy a code contract.
const code = (source: string): string =>
  source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');// NF-7 bounded lexical extractor (CORRECTED3). PASS 1 masks // and slash-star
// comments and double/single-quoted literals with spaces (offsets/newlines
// preserved), so AsyncTask text in a comment/string can never qualify and a
// PersistCurrent inside a comment/string is never located.
function maskLexicalText(s: string): string {
  const out = s.split('');
  const n = out.length;
  let i = 0;
  const blank = (a: number, b: number) => {
    for (let j = a; j < b; ++j) {
      if (out[j] !== '\n') out[j] = ' ';
    }
  };
  while (i < n) {
    const c = s[i];
    const d = s[i + 1];
    if (c === '/' && (d === '/' || d === '*')) {
      let end = d === '/' ? s.indexOf('\n', i) : s.indexOf('*/', i + 2);
      end = end < 0 ? n : d === '/' ? end : end + 2;
      blank(i, end);
      i = end;
    } else if (c === '"' || c === "'") {
      let j = i + 1;
      while (j < n && s[j] !== c) {
        if (s[j] === '\\') ++j;
        ++j;
      }
      const end = j < n ? j + 1 : j;
      blank(i, end);
      i = end;
    } else {
      ++i;
    }
  }
  return out.join('');
}

// PASS 2: open-paren offsets of every PersistCurrent identifier + ( in masked text.
const locatePersistCurrentOpen = (masked: string): number[] => {
  const offsets: number[] = [];
  const re = /(?:^|[^A-Za-z0-9_])PersistCurrent\s*\(/gu;
  // Underscored: the match itself is never read, only its side effect on
  // re.lastIndex, and the linter allows an unused binding under that prefix.
  let _m: RegExpExecArray | null;
  while ((_m = re.exec(masked)) !== null) {
    offsets.push(re.lastIndex - 1);
  }
  return offsets;
};

// PASS 3: for each O, walk BACKWARD to the nearest preceding AsyncTask
// identifier followed by (, delimiter-match forward to its matching close paren
// ((){}[] depth, bounded by the containing statement's terminating ; at depth
// 0), require open < O < close (strict paren containment, AST-style, NOT
// same-line/same-statement text) and the AsyncTask first argument (token run to
// the first top-level comma, trimmed) to equal ENamedThreads::GameThread.
// Returns null when no qualifying extent contains O - a bare PersistCurrent, an
// unrelated earlier AsyncTask, or a helper invoked by AsyncTask but lexically
// outside its lambda all FAIL.
// Lambda-parameter note: `[]() { ... }` is a SINGLE AsyncTask argument. The `[`
// capture list pushes depth and `]` pops it, so the lambda's empty/typed
// parameter pair `(`...`)` would pop depth back to 0 and be mistaken for the
// AsyncTask close. The scan therefore remembers when a `(` is opened
// immediately after a `]` (lambda params) and SKIPS its matching `)` (it stays
// inside the AsyncTask call extent), so the real close is the `)` that follows
// the lambda body. A `(` not preceded by `]` is ordinary call nesting.
function enclosingAsyncTaskGameThread(masked: string, o: number): { open: number; close: number } | null {
  const needle = 'AsyncTask';
  let from = o;
  for (;;) {
    const at = masked.lastIndexOf(needle, from);
    if (at < 0) return null;
    const prev = at > 0 ? masked[at - 1] : '';
    if (!/[A-Za-z0-9_]/u.test(prev)) {
      let cursor = at + needle.length;
      while (cursor < masked.length && /[ \t]/u.test(masked[cursor])) ++cursor;
      if (masked[cursor] === '(') {
        const open = cursor;
        let depth = 0;
        let firstArgEnd = -1;
        let close = -1;
        let lambdaParenBase = -1; // depth BEFORE a lambda-params `(` opened
        let prevChar = '';
        for (let j = open + 1; j < masked.length; ++j) {
          const ch = masked[j];
          if (ch === '(' || ch === '[' || ch === '{') {
            if (ch === '(' && prevChar === ']') lambdaParenBase = depth;
            ++depth;
          } else if (ch === ')' || ch === ']' || ch === '}') {
            if (lambdaParenBase >= 0 && ch === ')' && depth - 1 === lambdaParenBase) {
              --depth; // closing the lambda params pair: stays inside AsyncTask
              lambdaParenBase = -1;
            } else if (depth === 0) {
              if (ch === ')') close = j;
              break;
            } else {
              --depth;
              if (ch === ')' && depth === 0) {
                close = j;
                break;
              }
            }
          } else if (ch === ';' && depth === 0) {
            break;
          } else if (ch === ',' && depth === 0 && firstArgEnd < 0) {
            firstArgEnd = j;
          }
          prevChar = ch;
        }
        if (close >= 0 && open < o && o < close) {
          const firstArg = masked.slice(open + 1, firstArgEnd < 0 ? close : firstArgEnd).trim();
          if (firstArg === 'ENamedThreads::GameThread') return { open, close };
        }
        return null;
      }
    }
    from = at - 1;
  }
}

// Every located PersistCurrent( must be contained in an AsyncTask(GameThread) extent.
const allPersistCurrentContained = (source: string): { located: number; contained: number } => {
  const masked = maskLexicalText(source);
  const offsets = locatePersistCurrentOpen(masked);
  let contained = 0;
  for (const o of offsets) {
    if (enclosingAsyncTaskGameThread(masked, o)) {
      ++contained;
    }
  }
  return { located: offsets.length, contained };
};

// NF-4 executable limitation model: JS mirror of the H8 ClaimSessionClose set.
const modelClaimSessionClose = (cap: number): ((id: string) => boolean) => {
  const window: string[] = [];
  return (id: string): boolean => {
    if (window.includes(id)) return false;
    window.push(id);
    if (window.length > cap) window.shift();
    return true;
  };
};

describe('Todo 9 H1 startup rotation hooks the diagnostics store on the game thread', () => {
  it('initializes the store and rotates after the commandlet return, before request acceptance', () => {
    const lifecycle = code(read(LIFECYCLE));
    for (const needle of [DIAG_INCLUDE, 'FMcpDiagnosticsSnapshot::Get().InitializeFromGameThread();', 'FMcpDiagnosticsSnapshot::Get().RotateOnStartup();']) {
      expect(lifecycle).toContain(needle);
    }
    expect(lifecycle.indexOf('IsRunningCommandlet()')).toBeLessThan(lifecycle.indexOf('InitializeFromGameThread();'));
    expect(lifecycle.indexOf('InitializeFromGameThread();')).toBeLessThan(lifecycle.indexOf('StartAcceptingAutomationRequests();'));
  });
});

describe('Todo 9 H2/H3/H4 queue hooks record at the exact seams with NF-1/NF-2/NF-3 discipline', () => {
  it('H2 admission captures depth in-lock and records handshake success on every WebSocket admission (NF-1, NF-2)', () => {
    const queue = code(read(QUEUE));
    expect(queue).toContain(DIAG_INCLUDE);
    const lockScope = sliceBetween(queue, 'FScopeLock Lock(&PendingAutomationRequestsMutex);', 'FMcpRequestOriginRegistry::Get().Record(');
    expect(lockScope).toContain('const int32 AdmissionDepth = PendingAutomationRequests.Num();');
    expect(lockScope.indexOf('const int32 AdmissionDepth')).toBeLessThan(lockScope.indexOf('PendingAutomationRequests.Add(MoveTemp(Pending));'));
    expect(queue).toContain('FMcpDiagnosticsSnapshot::Get().RecordAdmission(');
    expect(queue).toContain('AdmissionDepth);');
    expect(queue).toContain('if (Origin == ERequestOrigin::WebSocket)');
    expect(queue).toContain('FMcpDiagnosticsSnapshot::Get().RecordHandshake(true);');
    expect(queue).not.toContain('bWsHandshakeRecorded');
  });

  it('H3 records all four refusal classes with the in-lock queue depth', () => {
    const queue = code(read(QUEUE));
    for (const refusal of ['AUTOMATION_NOT_ACCEPTING', 'AUTOMATION_ALREADY_CANCELED', 'AUTOMATION_SESSION_QUEUE_FULL', 'AUTOMATION_QUEUE_FULL']) {
      expect(queue).toContain(`FMcpDiagnosticsSnapshot::Get().RecordRefusal(RequestId, TEXT("${refusal}"), PendingAutomationRequests.Num());`);
    }
  });

  it('H4 captures the post-batch count in-lock and persists on the game thread before dispatch (NF-3)', () => {
    const queue = code(read(QUEUE));
    expect(queue.indexOf('MCP_DISALLOW_SHRINKING);')).toBeLessThan(queue.indexOf('const int32 PendingCountAfterBatch = PendingAutomationRequests.Num();'));
    expect(queue.indexOf('FScopeLock ExecutionLock(&AutomationRequestExecutionMutex);')).toBeLessThan(queue.indexOf('FMcpDiagnosticsSnapshot::Get().RecordPreDispatch(Req.RequestId, PendingCountAfterBatch);'));
    expect(queue.indexOf('FMcpDiagnosticsSnapshot::Get().RecordPreDispatch(Req.RequestId, PendingCountAfterBatch);')).toBeLessThan(queue.indexOf('ProcessAutomationRequest('));
    expect(queue).toContain('FMcpDiagnosticsSnapshot::Get().PersistCurrent();');
    // H4 crash anchor + H3-refusal deferred persist = exactly two textual
    // PersistCurrent() occurrences in this file (adjudicated (b) fix).
    expect(queue.match(/PersistCurrent\(\)/gu)?.length).toBe(2);
    // Guard A (NF-7 containment): the refusal-path persist must be inside an
    // AsyncTask(ENamedThreads::GameThread) extent, never a bare socket-thread
    // call - the file-local PersistSnapshotAsync helper satisfies this.
    expect(allPersistCurrentContained(code(read(QUEUE))).contained).toBeGreaterThanOrEqual(1);
    // Guard B (never under the mutex): no PersistCurrent() text may appear
    // between the queue admission lock and the origin-registry record.
    expect(sliceBetween(queue, 'FScopeLock Lock(&PendingAutomationRequestsMutex);', 'FMcpRequestOriginRegistry::Get().Record(')).not.toContain('PersistCurrent()');
  });
});

describe('Todo 9 H5 terminal hook fires in the single response funnel', () => {
  it('records the terminal class before the transport branch, persisting only on the game thread', () => {
    const responses = code(read(RESPONSES));
    expect(responses).toContain(DIAG_INCLUDE);
    expect(responses.indexOf('FMcpRequestOriginRegistry::Get().Forget(RequestId);')).toBeLessThan(responses.indexOf('FMcpDiagnosticsSnapshot::Get().RecordTerminal(RequestId,'));
    expect(responses.indexOf('FMcpDiagnosticsSnapshot::Get().RecordTerminal(RequestId,')).toBeLessThan(responses.indexOf('if (EffectiveOrigin == ERequestOrigin::NativeHTTP'));
    expect(responses).toContain('if (IsInGameThread()) { FMcpDiagnosticsSnapshot::Get().PersistCurrent(); }');
  });
});

describe('Todo 9 H6 WS disconnect hooks discriminate 4004/4005 (NF-5) and defer persist', () => {
  it('records handshake failure for 4004 AND 4005, maps disconnect reasons, and persists via AsyncTask', () => {
    const events = read(SOCKET_EVENTS);
    for (const needle of ['#include "Async/Async.h"', DIAG_INCLUDE, 'StatusCode == 4004 || StatusCode == 4005', 'FMcpDiagnosticsSnapshot::Get().RecordHandshake(false);', 'FMcpDiagnosticsSnapshot::Get().RecordDisconnect(', '(StatusCode == 1000 || StatusCode == 1001) ? TEXT("closed") : TEXT("error")', 'PersistSnapshotAsync();']) {
      expect(events).toContain(needle);
    }
  });
});

describe('Todo 9 H7/H8 native session hooks record create/close with NF-4 dedupe', () => {
  it('H7 records session creation after the SessionMutex scope with a deferred game-thread persist', () => {
    const discovery = code(read(TOOL_DISCOVERY));
    expect(discovery).toContain(DIAG_INCLUDE);
    expect(discovery).toContain('FMcpDiagnosticsSnapshot::Get().RecordSessionCreated(OutSessionId);');
    expect(discovery).toContain('PersistSnapshotAsync();');
    expect(discovery).not.toContain('#include "Async/Async.h"');
  });

  it('H8 dedupes close records via a bounded first-close-wins set and never gates on ActiveSessions (NF-4)', () => {
    const sessions = code(read(SESSIONS));
    for (const needle of [DIAG_INCLUDE, 'FCriticalSection SessionCloseLock', 'TArray<FString> ClosedSessionIds', 'ClosedSessionIds.Contains(Id)', 'ClosedSessionIds.Add(Id)', 'ClosedSessionIds.Num() > 128', 'ClosedSessionIds.RemoveAt(0)', 'if (ClaimSessionClose(SessionId))', 'FMcpDiagnosticsSnapshot::Get().RecordSessionClosed();']) {
      expect(sessions).toContain(needle);
    }
    const funnel = code(sliceBetween(sessions, 'void FMcpNativeTransport::CloseSessionConnections(', 'void FMcpNativeTransport::OnToolsListChanged'));
    expect(funnel).toContain('ClaimSessionClose(SessionId)');
    expect(funnel).not.toContain('ActiveSessions.Contains');
    expect(sessions).not.toContain('#include "Async/Async.h"');
  });
});

describe('Todo 9 NF-7 keeps PersistCurrent inside AsyncTask(ENamedThreads::GameThread) extents', () => {
  it('the extractor PASSES the two positive fixtures (two-line helper, H8 inline form)', () => {
    const helper = `namespace { void PersistSnapshotAsync() { AsyncTask(ENamedThreads::GameThread,
    []() { FMcpDiagnosticsSnapshot::Get().PersistCurrent(); }); } }`;
    expect(allPersistCurrentContained(helper)).toEqual({ located: 1, contained: 1 });
    const inline = `if (ClaimSessionClose(SessionId)) { FMcpDiagnosticsSnapshot::Get().RecordSessionClosed();
AsyncTask(ENamedThreads::GameThread, []() { FMcpDiagnosticsSnapshot::Get().PersistCurrent(); }); }`;
    expect(allPersistCurrentContained(inline)).toEqual({ located: 1, contained: 1 });
  });

  it('the extractor FAILS the three negative fixtures (unrelated AsyncTask, masked text, helper-outside-lambda)', () => {
    // NEG-1: the exact correction2 false-pass - an unrelated earlier AsyncTask
    // cannot rescue a later bare PersistCurrent in the same brace region.
    const neg1 = `namespace { void Good() { AsyncTask(ENamedThreads::GameThread, []() { DoWork(); }); }
void Bad() { FMcpDiagnosticsSnapshot::Get().PersistCurrent(); } }`;
    expect(allPersistCurrentContained(neg1)).toEqual({ located: 1, contained: 0 });
    // NEG-2: AsyncTask text in a comment/string is masked; only the real bare
    // call is located, and it has no qualifying preceding AsyncTask.
    const neg2 = `// AsyncTask(ENamedThreads::GameThread, []() { FMcpDiagnosticsSnapshot::Get().PersistCurrent(); });
const FString S = TEXT("AsyncTask(ENamedThreads::GameThread, []() { })");
FMcpDiagnosticsSnapshot::Get().PersistCurrent();`;
    expect(allPersistCurrentContained(neg2)).toEqual({ located: 1, contained: 0 });
    // NEG-3: PersistCurrent in a helper invoked by AsyncTask but lexically
    // outside its lambda is not paren-contained.
    const neg3 = `namespace { void PersistNow() { FMcpDiagnosticsSnapshot::Get().PersistCurrent(); }
void Run() { AsyncTask(ENamedThreads::GameThread, []() { PersistNow(); }); } }`;
    expect(allPersistCurrentContained(neg3)).toEqual({ located: 1, contained: 0 });
  });

  it('every PersistCurrent in the three socket-thread files is proven contained (H6/H7/H8)', () => {
    for (const file of [SOCKET_EVENTS, TOOL_DISCOVERY, SESSIONS]) {
      expect(allPersistCurrentContained(read(file))).toEqual({ located: 1, contained: 1 });
    }
  });
});

describe('Todo 9 lane A ships the native hook-sequence test and stays budget-clean', () => {
  it('ships McpDiagnosticsHookSequenceTests.cpp proving the recorder sequences at the store level', () => {
    const tests = read(NATIVE_HOOK_TESTS);
    for (const marker of ['IMPLEMENT_SIMPLE_AUTOMATION_TEST', 'SetRootOverride', 'SetClock', 'RotateOnStartup', 'RecordAdmission', 'RecordPreDispatch', 'RecordTerminal', 'RecordRefusal', 'RecordHandshake', 'RecordDisconnect', 'RecordSessionCreated', 'RecordSessionClosed', 'closed may exceed created', '"closed":3', '"active":0']) {
      expect(tests).toContain(marker);
    }
  });

  it('models the retained-128-window at-most-once claim and its eviction approximation (NF-4)', () => {
    const claim = modelClaimSessionClose(128);
    const ids = Array.from({ length: 128 }, (_, i) => String(i));
    for (const id of ids) expect(claim(id)).toBe(true);
    for (const id of ids) expect(claim(id)).toBe(false);
    // The 129th claim evicts the oldest id; a re-close of an EVICTED id
    // double-counts by design (documented bounded approximation), then the
    // re-claimed id is retained again and deduped until it is evicted anew.
    expect(claim('128')).toBe(true);
    expect(claim('0')).toBe(true);
    expect(claim('0')).toBe(false);
  });

  it('keeps every lane-A C++ file within the 250 pure-line ceiling', () => {
    for (const file of [LIFECYCLE, QUEUE, RESPONSES, SOCKET_EVENTS, SESSIONS, TOOL_DISCOVERY, NATIVE_HOOK_TESTS]) {
      expect(countPureLines(read(file)), file).toBeLessThanOrEqual(250);
    }
  });
});
