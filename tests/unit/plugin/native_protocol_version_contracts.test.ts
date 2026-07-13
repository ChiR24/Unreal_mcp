/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Source-contract tests for Wave C3: official MCP 2025-11-25 lifecycle protocol
// version negotiation and MCP-Protocol-Version header handling in the native C++
// Streamable HTTP transport.
//
// These read the plugin C++ source and assert the spec-mandated behavior is
// present (and the old hard-coded behavior is gone). They are the genuine native
// gate for this wave: a live editor HTTP harness is not available in CI, and the
// authoritative UE BuildPlugin covers compilation. RED = pre-C3 source lacked
// every token below (HandleInitialize hard-coded 2025-03-26, no header parsing,
// no 400 gate); GREEN = all tokens present.

const pluginRoot = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
);

const read = (rel: string): string =>
  readFileSync(resolve(pluginRoot, rel), 'utf8');

const discovery = read('MCP/Transport/McpNativeTransportToolDiscovery.cpp');
const httpParsing = read('MCP/Transport/McpNativeTransportHttpParsing.cpp');
const connection = read('MCP/Transport/McpNativeTransportConnection.cpp');
const protocolVersion = read('MCP/Transport/McpNativeTransportProtocolVersion.cpp');
const header = read('MCP/Transport/McpNativeTransport.h');
const privateH = read('MCP/Transport/McpNativeTransportPrivate.h');
const jsonRpcH = read('MCP/Protocol/McpJsonRpc.h');

describe('C3: MCP protocol version negotiation + header handling', () => {
  it('declares the supported protocol-version set {2025-11-25, 2025-06-18, 2025-03-26}', () => {
    expect(privateH).toContain('McpSupportedProtocolVersions');
    expect(privateH).toContain('"2025-11-25"');
    expect(privateH).toContain('"2025-06-18"');
    expect(privateH).toContain('"2025-03-26"');
    expect(privateH).toContain('McpLatestProtocolVersion');
    expect(privateH).toContain('McpDefaultProtocolVersion');
  });

  it('negotiates initialize protocolVersion instead of hard-coding it', () => {
    // Old behavior: Result->SetStringField(TEXT("protocolVersion"), TEXT("2025-03-26"))
    expect(discovery).not.toContain(
      'SetStringField(TEXT("protocolVersion"), TEXT("2025-03-26"))',
    );
    // New behavior: negotiation runs and the negotiated value is echoed.
    expect(discovery).toContain('NegotiateInitializeProtocolVersion(');
    expect(discovery).toContain(
      'SetStringField(TEXT("protocolVersion"), NegotiatedVersion)',
    );
  });

  it('echoes a supported version and negotiates an unknown well-formed version to latest', () => {
    expect(protocolVersion).toContain('McpIsSupportedProtocolVersion(Requested)');
    expect(protocolVersion).toContain('OutNegotiated = Requested;');
    expect(protocolVersion).toContain('OutNegotiated = McpLatestProtocolVersion();');
  });

  it('rejects missing/invalid initialize field with JSON-RPC -32602 carrying supported/requested data', () => {
    expect(protocolVersion).toContain(
      "TEXT(\"initialize requires a string 'protocolVersion' field\")",
    );
    expect(protocolVersion).toContain(
      "TEXT(\"initialize 'protocolVersion' must be a non-empty string\")",
    );
    // HandleInitialize surfaces the -32602 with data.
    expect(discovery).toContain('FMcpJsonRpc::ErrorInvalidParams');
    expect(discovery).toContain('TEXT("supported")');
    expect(discovery).toContain('TEXT("requested")');
  });

  it('parses the MCP-Protocol-Version header on every request', () => {
    expect(httpParsing).toContain('MCP-Protocol-Version');
    expect(httpParsing).toContain('OutRequest.ProtocolVersion = Value;');
    expect(header).toContain('FString ProtocolVersion;');
  });

  it('validates the header on POST and GET/SSE; invalid/unsupported -> HTTP 400', () => {
    // POST path (json body) and GET path (text/plain) both guard.
    expect(connection).toContain(
      'GuardProtocolVersionHeader(ClientSocket, HttpReq, Rpc.Id, true)',
    );
    expect(connection).toContain(
      'GuardProtocolVersionHeader(ClientSocket, HttpReq, nullptr, false)',
    );
    expect(protocolVersion).toContain('Unsupported or invalid MCP-Protocol-Version');
    expect(protocolVersion).toContain('return false;');
  });

  it('derives a missing header from the negotiated session version or defaults to 2025-03-26', () => {
    expect(protocolVersion).toContain('SessionProtocolVersions.Find(SessionId)');
    expect(protocolVersion).toContain('McpDefaultProtocolVersion()');
  });

  it('stores the negotiated version per session and cleans it on session removal/shutdown', () => {
    expect(header).toContain('TMap<FString, FString> SessionProtocolVersions;');
    expect(discovery).toContain('SessionProtocolVersions.Add(OutSessionId, NegotiatedVersion);');
    expect(connection).toContain('SessionProtocolVersions.Remove(HttpReq.SessionId);');
    expect(connection).toContain('SessionProtocolVersions.Remove(NewSessionId);');
    expect(read('MCP/Transport/McpNativeTransportSessions.cpp')).toContain(
      'SessionProtocolVersions.Remove(SessionId);',
    );
    const lifecycle = read('MCP/Transport/McpNativeTransportLifecycle.cpp');
    expect(lifecycle).toContain('SessionProtocolVersions.Empty();');
  });

  it('exposes a data-carrying BuildError overload for the -32602 response', () => {
    expect(jsonRpcH).toContain(
      'static FString BuildError(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message, const TSharedPtr<FJsonObject>& Data);',
    );
  });

  it('does NOT implement the 2026-07-28 RC version', () => {
    // Only the quoted form (a listed/implemented version) is forbidden.
    expect(privateH).not.toContain('"2026-07-28"');
    expect(discovery).not.toContain('"2026-07-28"');
    expect(connection).not.toContain('"2026-07-28"');
  });

  it('locks the native set to exactly the three modern versions (no TS legacy, no fictional RC)', () => {
    // The native transport is intentionally stricter than the TypeScript SDK,
    // which also negotiates two older legacy versions. The native surface must
    // NOT implement the TS legacy versions (2024-11-05, 2024-10-07) nor the
    // fictional later RC (2026-07-28). The supported set is exactly:
    // 2025-11-25 (latest), 2025-06-18, 2025-03-26.
    expect(privateH).toContain('"2025-11-25"');
    expect(privateH).toContain('"2025-06-18"');
    expect(privateH).toContain('"2025-03-26"');

    // TS legacy versions must never appear as a listed/implemented native version.
    expect(privateH).not.toContain('"2024-11-05"');
    expect(privateH).not.toContain('"2024-10-07"');

    // Fictional later RC excluded (redundant with the prior test; locked here
    // alongside the legacy exclusions so the asymmetry contract is one assertion
    // group).
    expect(privateH).not.toContain('"2026-07-28"');

    // Exactly three supported versions are enumerated in McpSupportedProtocolVersions.
    const versionsBlock = privateH.slice(
      privateH.indexOf('inline const TArray<FString>& McpSupportedProtocolVersions'),
      privateH.indexOf('inline const FString& McpLatestProtocolVersion'),
    );
    const listed = (versionsBlock.match(/"20\d\d(?:-\d\d){2}"/gmu) ?? []).map(
      (v) => v.replace(/"/gmu, ''),
    );
    expect(listed).toEqual(['2025-11-25', '2025-06-18', '2025-03-26']);
  });
});

// ─── C3b: protocol-version fallback boundary (absent vs present-invalid) ───────
// Focused, TDD-pinned boundary for Todo 4: an ABSENT MCP-Protocol-Version header
// must NEVER yield HTTP 400 (it resolves to the negotiated session version or the
// default), while a PRESENT but unsupported/malformed header MUST yield HTTP 400.
// These assertions are the genuine native gate for the fallback contract; a live
// editor HTTP harness is not required (and is not used — no PID 114496 / external
// editor is contacted). RED = GuardProtocolVersionHeader used the raw
// SendHttpResponse+Close instead of the canonical SendAndClose(...400...) wrapper;
// GREEN = both 400 branches route through SendAndClose and the docs state the
// absent-header boundary explicitly.

describe('C3b: protocol-version fallback boundary (absent vs present-invalid)', () => {
  it('routes a PRESENT unsupported/invalid header to SendAndClose(ClientSocket, 400, ...) (json + text)', () => {
    // POST (json body) path.
    expect(protocolVersion).toContain(
      'SendAndClose(ClientSocket, 400, TEXT("application/json")',
    );
    // GET/SSE (text/plain) path.
    expect(protocolVersion).toContain(
      'SendAndClose(ClientSocket, 400, TEXT("text/plain")',
    );
  });

  it('never emits a 400 on the ABSENT-header branch of ResolveRequestProtocolVersion', () => {
    // Slice the absent-header branch (from the "// Absent header" comment to its
    // return true) and prove it is derivation-only: it resolves the version and
    // returns true, and contains no send/close/error-return that would emit a 400.
    // (We match code tokens, not the literal "400", because the branch's own
    // comment legitimately says "no HTTP 400".)
    const start = protocolVersion.indexOf('// Absent header:');
    const end = protocolVersion.indexOf('return true;', start);
    expect(start).toBeGreaterThan(0);
    expect(end).toBeGreaterThan(start);
    const absentBranch = protocolVersion.slice(start, end);
    // Positive: the branch derives the version (fallback path).
    expect(absentBranch).toContain('SessionProtocolVersions.Find');
    expect(absentBranch).toContain('McpDefaultProtocolVersion()');
    // Negative: no code that would emit a 400 / reject the request.
    expect(absentBranch).not.toContain('SendHttpResponse');
    expect(absentBranch).not.toContain('SendAndClose');
    expect(absentBranch).not.toContain('ClientSocket->Close');
    expect(absentBranch).not.toContain('return false');
    expect(absentBranch).not.toContain(', 400,');
  });

  it('rejects a PRESENT unsupported/invalid header with the 400 error and false return', () => {
    expect(protocolVersion).toContain(
      '"Unsupported or invalid MCP-Protocol-Version: %s"',
    );
    expect(protocolVersion).toContain('return false;');
  });

  it('matches supported versions by exact equality (no normalization beyond header trim)', () => {
    expect(privateH).toContain('McpSupportedProtocolVersions().Contains(Version)');
  });

  it('trims the header value in the HTTP parser so whitespace-padded valid versions are accepted', () => {
    // McpNativeTransportHttpParsing.cpp trims Key/Value before storing the
    // protocol version; a value like " 2025-11-25 " is therefore accepted, while
    // a genuinely unsupported (post-trim) value still fails.
    expect(httpParsing).toContain('Value.TrimStartAndEndInline()');
  });

  it('falls back to McpDefaultProtocolVersion() when the session is absent/stale', () => {
    const start = protocolVersion.indexOf('// Absent header:');
    const end = protocolVersion.indexOf('return true;', start);
    const absentBranch = protocolVersion.slice(start, end);
    expect(absentBranch).toContain('McpDefaultProtocolVersion()');
  });

  it('keeps the native set locked to exactly the three modern versions', () => {
    const versionsBlock = privateH.slice(
      privateH.indexOf('inline const TArray<FString>& McpSupportedProtocolVersions'),
      privateH.indexOf('inline const FString& McpLatestProtocolVersion'),
    );
    const listed = (versionsBlock.match(/"20\d\d(?:-\d\d){2}"/gmu) ?? []).map(
      (v) => v.replace(/"/gmu, ''),
    );
    expect(listed).toEqual(['2025-11-25', '2025-06-18', '2025-03-26']);
    expect(privateH).not.toContain('"2024-11-05"');
    expect(privateH).not.toContain('"2024-10-07"');
    expect(privateH).not.toContain('"2026-07-28"');
  });

  it('rejects adversarial/malformed present versions (no live socket needed): 2099-01-01, 2025-13-99, junk, comma-list', () => {
    // Structural proxy for the live QA "MCP-Protocol-Version: 2099-01-01 -> 400":
    // none of these appear as a listed supported version, and membership is an
    // exact Contains, so each hits the present-invalid 400 branch.
    const versionsBlock = privateH.slice(
      privateH.indexOf('inline const TArray<FString>& McpSupportedProtocolVersions'),
      privateH.indexOf('inline const FString& McpLatestProtocolVersion'),
    );
    for (const bad of [
      '2099-01-01',
      '2025-13-99',
      'not-a-version',
      '2025-11-25, 2024-11-05',
    ]) {
      expect(versionsBlock).not.toContain(`"${bad}"`);
    }
  });
});

// ─── C3c: protocol docs assert advisory fallback/cancellation ─────────────────
// The docs are part of the contract for Todo 4: they must state the absent-header
// boundary (no 400) and that in-flight cancellation is advisory only.

describe('C3c: protocol docs assert advisory fallback/cancellation', () => {
  const protocolDoc = readFileSync(
    resolve(process.cwd(), 'docs/protocol.md'),
    'utf8',
  );

  it('documents that an absent header is NOT rejected and only a present-but-unsupported header returns 400', () => {
    expect(protocolDoc).toContain('MCP-Protocol-Version');
    expect(protocolDoc).toContain('HTTP 400');
    expect(protocolDoc).toContain('NOT rejected');
    expect(protocolDoc).toContain('present but unsupported');
  });

  it('documents that in-flight cancellation is advisory (queued dropped, running editor op completes, late response suppressed)', () => {
    expect(protocolDoc).toContain('advisory');
    expect(protocolDoc).toContain('runs to completion');
  });
});
