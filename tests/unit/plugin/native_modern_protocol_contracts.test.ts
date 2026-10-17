/**
 * Source contracts for MCP 2026-07-28 on the native /mcp transport: per-request version and capabilities with no
 * initialize or session, the request headers checked against the body, the result fields the revision requires,
 * consent asked through the client (multi round-trip requests) and the tasks extension. The behaviour is covered by
 * the EngineWire.Protocol.Modern automation tests; these pin the wiring the C++ cannot show here.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const privateDir = resolve(process.cwd(), 'plugins/EngineWire/Source/EngineWire/Private');
const code = (file: string): string =>
  readFileSync(resolve(privateDir, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');

describe('native MCP 2026-07-28 requests', () => {
  const connection = code('MCP/Transport/McpNativeTransportConnection.cpp');
  const modern = code('MCP/Protocol/McpNativeTransportModern.cpp');
  const request = code('MCP/Protocol/McpModernRequest.cpp');
  const header = code('MCP/Protocol/McpModernRequest.h');

  it('branches a modern request off after the token check and before any session check', () => {
    const branch = connection.indexOf('if (McpModern::IsModern(Rpc.Params))');
    expect(branch).toBeGreaterThan(connection.indexOf('bTokenRequired'));
    expect(branch).toBeLessThan(connection.indexOf('const bool bSessionless'));
  });

  it('refuses an unsupported version and mismatched headers with 400, and an unknown method with 404', () => {
    expect(modern).toContain('Reply(400, McpModern::UnsupportedVersionError(Rpc.Id, Meta.ProtocolVersion, {FString(McpModern::Version)}));');
    expect(modern).toContain('Reply(400, FMcpJsonRpc::BuildError(Rpc.Id, McpModern::ErrorHeaderMismatch, Mismatch));');
    expect(modern).toContain('Reply(404, FMcpJsonRpc::BuildError(Rpc.Id, FMcpJsonRpc::ErrorMethodNotFound,');
    expect(header).toContain('constexpr int32 ErrorHeaderMismatch = -32020;');
    expect(header).toContain('constexpr int32 ErrorUnsupportedProtocolVersion = -32022;');
    expect(request).toContain('TEXT("=?base64?")');
  });

  it('serves every modern request on the implicit session, rate limits first, and never names that session', () => {
    expect(modern.indexOf('ConsumeSessionRequestBudget(SessionId')).toBeLessThan(modern.indexOf('const McpModern::FScope Scope(Call);'));
    expect(modern).toContain('static const FString Salt = FGuid::NewGuid().ToString();');
    expect(code('MCP/Transport/McpNativeTransportGatewayStream.cpp')).toContain('Conn->bModern ? FString() : SessionId');
    expect(code('MCP/Transport/McpNativeTransportHttpResponses.cpp')).toContain('if (!SessionId.IsEmpty())');
  });

  it('stamps every JSON answer and the streamed result of a modern call', () => {
    expect(code('MCP/Transport/McpNativeTransportHttpResponses.cpp')).toContain('McpModern::StampResponseBody(Body, Modern.Method)');
    expect(code('MCP/Transport/McpNativeTransportPendingRequests.cpp')).toContain('if (Conn->bModern) McpModern::StampResult(TEXT("tools/call"), ToolResult);');
  });

  it('lists 2026-07-28 and the tasks extension in server/discover', () => {
    const discovery = code('MCP/Transport/McpNativeTransportToolDiscovery.cpp');
    expect(discovery).toContain('TArray<TSharedPtr<FJsonValue>> Versions = {MakeShared<FJsonValueString>(McpModern::Version)};');
    expect(discovery).toContain('Extensions->SetObjectField(TEXT("io.modelcontextprotocol/tasks"), MakeShared<FJsonObject>());');
  });
});

describe('native consent through the client', () => {
  const consent = code('MCP/Protocol/McpModernConsent.cpp');
  const execute = code('MCP/Execute/McpNativeTransportGatewayExecute.cpp');

  it('signs requestState with a per-run key over the principal, the call digest and the payload, and checks it in constant time', () => {
    expect(consent).toContain('static const FString Key = McpCapabilityTokenStore::GenerateRandomToken();');
    expect(consent).toContain('FSHA1::HMACBuffer(');
    expect(consent).toContain('McpConstantTimeTokenEquals(Signature, Sign(Principal, Digest, Payload))');
    expect(consent).toContain('const FString Digest = McpCanonicalFingerprint(CapabilityId, Arguments);');
    expect(consent).toContain('Expires >= static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp())');
  });

  it('asks only on a modern tools/call from a client that can elicit, and the yes rides the single-use grant nonce', () => {
    expect(consent).toContain('if (!Call.bActive || Call.Method != TEXT("tools/call"))');
    expect(consent).toContain('if (Call.bElicitationForm)');
    expect(consent).toContain('Out.Grant->SetStringField(TEXT("nonce"), Nonce);');
  });

  it('asks only after scope passed, re-runs the whole gate with the grant, and drops describe after a no', () => {
    expect(execute).toContain('Decision.ErrorCode == McpAuthorizationCodes::ConsentRequired && !AuthRequest.Consent.IsValid()');
    expect(execute).toContain('AuthRequest.Consent = Asked.Grant;');
    expect(execute).toContain('Decision = McpPrequeueGate::Authorize(AuthRequest);');
    expect(execute).toContain('Asked.Declined.IsEmpty() ? Guidance : TSharedPtr<FJsonObject>()');
  });

  it('refuses an echoed requestState that does not verify, as the TypeScript SDK does', () => {
    expect(consent).toContain('Out.bInvalidState = true;');
    expect(consent).toContain('TEXT("Invalid or expired requestState")');
    expect(execute).toContain('McpModernConsent::InvalidStateError(Id)');
  });
});

describe('native tasks extension', () => {
  const keepalive = code('MCP/Transport/McpNativeTransportKeepalive.cpp');
  const modern = code('MCP/Protocol/McpNativeTransportModern.cpp');

  it('answers a task handle at the client time limit when the client declared the extension', () => {
    expect(keepalive).toContain('Conn->bTasks');
    expect(keepalive).toContain('McpModern::TaskObject(RequestId, TEXT("working"),');
  });

  it('keeps the call result for tasks/get and reads tasks only for their principal', () => {
    expect(code('MCP/Transport/McpNativeTransportPendingRequests.cpp')).toContain('ReportedResult), ToolResult);');
    expect(modern).toContain('TaskResults.Find(TaskId, GetSessionPrincipal(SessionId).Identity, Task)');
    expect(code('MCP/DynamicTools/McpTaskResults.cpp')).toContain('Entry->Principal.Equals(Principal, ESearchCase::CaseSensitive)');
  });

  it('cancels only a call still waiting in the queue, after the answer is sent', () => {
    expect(modern).toContain('Subsystem->IsAutomationRequestWaiting(TaskId)');
    expect(modern.indexOf('Reply(200, HandleTaskMethod(')).toBeLessThan(modern.indexOf('Subsystem->CancelAutomationRequest(CancelId);'));
  });
});
