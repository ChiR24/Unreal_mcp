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
});
