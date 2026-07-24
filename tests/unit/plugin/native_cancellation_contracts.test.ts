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
const gatewayExecute = read('MCP/Execute/McpNativeTransportGatewayExecute.cpp');
const connectionTypes = read('MCP/Transport/McpNativeTransportConnectionTypes.h');
const pending = read('MCP/Transport/McpNativeTransportPendingRequests.cpp');
const connection = read('MCP/Transport/McpNativeTransportConnection.cpp');
const jsonRpcH = read('MCP/Protocol/McpJsonRpc.h');
const jsonRpcCpp = read('MCP/Protocol/McpJsonRpc.cpp');
const jsonRpcDispatch = read('MCP/Transport/McpNativeTransportJsonRpc.cpp');

describe('C4: notifications/cancelled cancellation + late-response suppression', () => {
  it('declares and implements HandleCancelledNotification', () => {
    expect(header).toContain(
      'void HandleCancelledNotification(const TSharedPtr<FJsonObject>& Params, const FString& CallerSessionId);',
    );
    expect(cancellation).toContain(
      'void FMcpNativeTransport::HandleCancelledNotification(',
    );
  });

  it('routes notifications/cancelled from the POST dispatch to the handler with caller session', () => {
    expect(connection).toContain('Rpc.Method == TEXT("notifications/cancelled")');
    expect(connection).toContain('HandleCancelledNotification(Rpc.Params, HttpReq.SessionId);');
  });

  it('correlates the client requestId to the inflight SSE connection', () => {
    // FSSEConnection records the canonical key of the client JSON-RPC id.
    expect(connectionTypes).toContain('FString ClientRequestIdKey;');
    // Gateway stream stamps it from the tools/call id.
    expect(gatewayStream).toContain('Conn->ClientRequestIdKey = McpJsonRpcIdKey(Id);');
    // Cancellation matches the inflight connection by that key.
    expect(cancellation).toContain('Conn->ClientRequestIdKey == ClientIdKey');
    // The id-key helper exists and is shared.
    expect(privateH).toContain('inline FString McpJsonRpcIdKey(');
  });

  it('namespaces string and number JSON-RPC ids so "1" and 1 cannot collide', () => {
    // McpJsonRpcIdKey must key by JSON type, not by value: a string request id
    // "1" and a numeric request id 1 would otherwise collapse to the same key
    // and a notifications/cancelled for one could cancel the other's in-flight
    // SSE connection. The helper prefixes each type so they remain distinct.
    const idKeyFn = privateH.slice(
      privateH.indexOf('inline FString McpJsonRpcIdKey('),
      privateH.indexOf('inline const TArray<FString>& McpSupportedProtocolVersions'),
    );
    // Distinct type tags: a string id becomes "s:<value>", a number "n:<value>".
    expect(idKeyFn).toContain('TEXT("s:")');
    expect(idKeyFn).toContain('TEXT("n:")');
    expect(idKeyFn.indexOf('TEXT("s:")')).not.toBe(idKeyFn.indexOf('TEXT("n:")'));
    // Both the string and number branches are present in the helper.
    expect(idKeyFn).toContain('Id->Type == EJson::String');
    expect(idKeyFn).toContain('Id->Type == EJson::Number');
  });

  it('scopes cancellation to the caller session so one session cannot cancel another', () => {
    // The SSE connection records the owning session.
    expect(connectionTypes).toContain('FString SessionId;  // for touching ActiveSessions during long-running calls');
    // The handler receives the caller session id and matches it.
    expect(cancellation).toContain('const FString& CallerSessionId');
    expect(cancellation).toContain('Conn->SessionId == CallerSessionId');
    // The POST dispatch passes the validated session id into the handler.
    expect(connection).toContain('HandleCancelledNotification(Rpc.Params, HttpReq.SessionId);');
  });

  it('marks the inflight request cancelled and cancels the automation request', () => {
    expect(header).toContain('TSet<FString> CancelledInternalRequestIds;');
    expect(header).toContain('mutable FCriticalSection CancelledRequestsMutex;');
    expect(cancellation).toContain('CancelledInternalRequestIds.Add(InternalRequestId);');
    expect(cancellation).toContain('Subsystem->CancelAutomationRequest(InternalRequestId);');
    // Stops further progress writes immediately.
    expect(cancellation).toContain('bMarkedForRemoval.store(true);');
  });

  it('bounds the cancellation marker maps so they cannot grow without limit', () => {
    // A hard cap constant bounds both maps together.
    expect(cancellation).toContain('constexpr int32 MaxCancelledMarkers');
    // Insertion-order tracking enables deterministic oldest-first eviction.
    expect(header).toContain('TArray<FString> CancelledMarkerOrder;');
    // The handler adds to the order and evicts excess under CancelledRequestsMutex.
    expect(cancellation).toContain('CancelledMarkerOrder.Add(ClientIdKey);');
    expect(cancellation).toContain('while (CancelledMarkerOrder.Num() > MaxCancelledMarkers)');
    // Completion also drops the order entry (preserving the documented lock order).
    expect(pending).toContain('CancelledMarkerOrder.Remove(Conn->ClientRequestIdKey);');
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

  it('tears down all three cancellation markers on the primary bCancelled path (not only the secondary set lookup)', () => {
    // Gating marker removal on `!bCancelled` would leak the marker maps on the
    // atomic-flag (primary) cancellation path until Shutdown/eviction.
    expect(pending).not.toContain('if (!bCancelled)');
    expect(pending).toContain('CancelledInternalRequestIds.Remove(RequestId);');
    expect(pending).toContain('CancelledClientIdToInternal.Remove(Conn->ClientRequestIdKey);');
    expect(pending).toContain('CancelledMarkerOrder.Remove(Conn->ClientRequestIdKey);');
  });
});

describe('C4: client _meta.progressToken preservation through gateway streaming', () => {
  it('captures the progressToken on the SSE connection', () => {
    expect(connectionTypes).toContain('TSharedPtr<FJsonValue> ProgressToken;');
    expect(connectionTypes).toContain('bool bHasProgressToken = false;');
    expect(gatewayStream).toContain('Conn->ProgressToken = ProgressToken;');
    expect(gatewayStream).toContain('Conn->bHasProgressToken = ProgressToken.IsValid();');
  });

  it('threads the client token through the gateway call chain', () => {
    // HandleGatewayModePreDispatch accepts and forwards the token.
    expect(gateway).toContain(
      'const TSharedPtr<FJsonValue>& ProgressToken',
    );
    // HandleGatewayCall forwards it into HandleGatewayExecute, which forwards it
    // into StreamToolCall alongside the resolved capability context.
    expect(gateway).toContain('HandleGatewayExecute(');
    expect(gatewayExecute).toContain(
      'SessionId, CorsOrigin, ProgressToken, Plan.CapabilityId, Plan.OutputSchema,',
    );
    // StreamToolCall declares the token parameter ahead of the gateway
    // capability context, so the trailing comma pins StreamToolCall itself.
    expect(header).toContain(
      'const TSharedPtr<FJsonValue>& ProgressToken,',
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
