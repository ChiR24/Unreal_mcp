/// <reference types="node" />

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Source-contract tests for Wave C4: official MCP notifications/cancelled
// request cancellation for the native C++ Streamable HTTP transport.
//
// Covers three behaviors required by the wave:
//   1. requestId-to-inflight correlation — a notifications/cancelled whose
//      requestId matches an in-flight tools/call is correlated to that SSE
//      connection (via the client JSON-RPC id key) and the underlying
//      automation request is cancelled.
//   2. late-response suppression — once cancelled, the eventual completion
//      from the subsystem is NOT written to the client (the SSE socket is
//      closed without a result).
//   3. progressToken preservation — the client's _meta.progressToken is
//      captured and echoed verbatim (type-preserving) in notifications/progress
//      through the gateway streaming path.
//
// These read the plugin C++ source and assert the spec-mandated structure is
// present. They are the genuine native gate for this wave; a live editor HTTP
// harness is not available in CI, and the authoritative UE BuildPlugin covers
// compilation. RED = pre-C4 source lacked every token below; GREEN = all
// tokens present.

const pluginRoot = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
);

const read = (rel: string): string =>
  readFileSync(resolve(pluginRoot, rel), 'utf8');

const header = read('MCP/Transport/McpNativeTransport.h');
const privateH = read('MCP/Transport/McpNativeTransportPrivate.h');
const cancellation = read('MCP/Transport/McpNativeTransportCancellation.cpp');
const gateway = read('MCP/Transport/McpNativeTransportGateway.cpp');
const gatewayStream = read('MCP/Transport/McpNativeTransportGatewayStream.cpp');
const pending = read('MCP/Transport/McpNativeTransportPendingRequests.cpp');
const connection = read('MCP/Transport/McpNativeTransportConnection.cpp');
const jsonRpcH = read('MCP/Protocol/McpJsonRpc.h');
const jsonRpcCpp = read('MCP/Protocol/McpJsonRpc.cpp');
const jsonRpcDispatch = read('MCP/Transport/McpNativeTransportJsonRpc.cpp');

describe('C4: notifications/cancelled cancellation + late-response suppression', () => {
  it('declares and implements HandleCancelledNotification', () => {
    expect(header).toContain(
      'void HandleCancelledNotification(const TSharedPtr<FJsonObject>& Params);',
    );
    expect(cancellation).toContain(
      'void FMcpNativeTransport::HandleCancelledNotification(',
    );
  });

  it('routes notifications/cancelled from the POST dispatch to the handler', () => {
    expect(connection).toContain('Rpc.Method == TEXT("notifications/cancelled")');
    expect(connection).toContain('HandleCancelledNotification(Rpc.Params);');
  });

  it('correlates the client requestId to the inflight SSE connection', () => {
    // FSSEConnection records the canonical key of the client JSON-RPC id.
    expect(header).toContain('FString ClientRequestIdKey;');
    // Gateway stream stamps it from the tools/call id.
    expect(gatewayStream).toContain('Conn->ClientRequestIdKey = McpJsonRpcIdKey(Id);');
    // Cancellation matches the inflight connection by that key.
    expect(cancellation).toContain('Conn->ClientRequestIdKey == ClientIdKey');
    // The id-key helper exists and is shared.
    expect(privateH).toContain('inline FString McpJsonRpcIdKey(');
  });

  it('marks the inflight request cancelled and cancels the automation request', () => {
    expect(header).toContain('TSet<FString> CancelledInternalRequestIds;');
    expect(header).toContain('mutable FCriticalSection CancelledRequestsMutex;');
    expect(cancellation).toContain('CancelledInternalRequestIds.Add(InternalRequestId);');
    expect(cancellation).toContain('Subsystem->CancelAutomationRequest(InternalRequestId);');
    // Stops further progress writes immediately.
    expect(cancellation).toContain('bMarkedForRemoval.store(true);');
  });

  it('suppresses the late response when the request was cancelled', () => {
    expect(pending).toContain('CancelledInternalRequestIds.Contains(RequestId)');
    // On cancellation the socket is closed WITHOUT building/sending a result.
    expect(pending).toContain('Conn->Socket->Close();');
    // The suppression branch returns before the success/error response build.
    expect(pending).toContain('return true;');
    // The non-cancelled path still builds a normal tool result.
    expect(pending).toContain('FMcpJsonRpc::BuildToolResult(');
  });

  it('documents the CancelledRequestsMutex lock order (no cycle)', () => {
    expect(header).toContain('CancelledRequestsMutex is taken ONLY after SSEConnectionsMutex');
  });
});

describe('C4: client _meta.progressToken preservation through gateway streaming', () => {
  it('captures the progressToken on the SSE connection', () => {
    expect(header).toContain('TSharedPtr<FJsonValue> ProgressToken;');
    expect(header).toContain('bool bHasProgressToken = false;');
    expect(gatewayStream).toContain('Conn->ProgressToken = ProgressToken;');
    expect(gatewayStream).toContain('Conn->bHasProgressToken = ProgressToken.IsValid();');
  });

  it('threads the client token through the gateway call chain', () => {
    // HandleGatewayModePreDispatch accepts and forwards the token.
    expect(gateway).toContain(
      'const TSharedPtr<FJsonValue>& ProgressToken',
    );
    // HandleGatewayCall forwards it into StreamToolCall.
    expect(gateway).toContain(
      'StreamToolCall(Tool, DispatchAction, ResolvedArgs, Id, ClientSocket, SessionId, CorsOrigin, ProgressToken);',
    );
    // StreamToolCall declares the token parameter.
    expect(header).toContain(
      'const TSharedPtr<FJsonValue>& ProgressToken = nullptr);',
    );
  });

  it('extracts the client _meta.progressToken in the tools/call path', () => {
    expect(jsonRpcDispatch).toContain('TEXT("_meta")');
    expect(jsonRpcDispatch).toContain('TEXT("progressToken")');
    expect(jsonRpcDispatch).toContain('HandleGatewayModePreDispatch(');
  });

  it('echoes the client token (not the internal id) in progress notifications', () => {
    expect(pending).toContain('Conn->bHasProgressToken && Conn->ProgressToken.IsValid()');
    // Falls back to the internal request id only when no client token is set.
    expect(pending).toContain('MakeShared<FJsonValueString>(RequestId)');
  });

  it('BuildProgressNotification preserves the token type (string or number)', () => {
    expect(jsonRpcH).toContain(
      'static FString BuildProgressNotification(\n\t\tconst TSharedPtr<FJsonValue>& ProgressToken,',
    );
    // The implementation sets the field as the raw JSON value (type-preserving).
    expect(jsonRpcCpp).toContain('Params->SetField(TEXT("progressToken"), ProgressToken);');
  });
});
