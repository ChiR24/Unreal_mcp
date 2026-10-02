/**
 * Source contracts for long calls on the native /mcp surface. A folder delete held the game thread for a quarter
 * of an hour: its own call was cut off by the client at 30 s with nothing said, and every other call answered a
 * bare EDITOR_BLOCKED. Now the editor says what it is working on and how far it has got: the open call hears so
 * from a thread the game thread cannot block, answers with it before the client's limit, a call refused meanwhile
 * says it too, and the delete reports as it goes. The C++ cannot run here, so these pin those rules.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const bridge = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge');
/** Comment bodies explain the rules, so rule checks ignore them. */
const code = (file: string): string =>
  readFileSync(resolve(bridge, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, '').replace(/^[ \t]*\/\/.*$/gmu, '');
const body = (source: string, signature: string): string => {
  const start = source.indexOf(signature);
  expect(start, signature).toBeGreaterThan(-1);
  const next = source.indexOf('\nvoid FMcpNativeTransport::', start + signature.length);
  return source.slice(start, next < 0 ? undefined : next);
};

describe('an open call hears from the editor even while the game thread is held', () => {
  const keepalive = code('Private/MCP/Transport/McpNativeTransportKeepalive.cpp');
  const cleanup = code('Private/MCP/Transport/McpNativeTransportCleanup.cpp');

  it('sweeps requests from the keepalive thread, not the game-thread cleanup pass', () => {
    expect(body(keepalive, 'void FMcpNativeTransport::RunKeepaliveLoop()')).toContain('SweepRequestProgress();');
    expect(cleanup).not.toContain('SendSSEProgressUpdate(');
    expect(cleanup).not.toContain('SweepRequestProgress');
  });

  it('answers before a 30 s client limit, after a shader-waiting capture has answered itself', () => {
    expect(keepalive).toContain('constexpr double AnswerRunningAfterSeconds = 27.0;');
    expect(keepalive).toContain('static_assert(AnswerRunningAfterSeconds >= McpShaderWaitMaxSeconds + 2.0');
    expect(body(keepalive, 'void FMcpNativeTransport::RunKeepaliveLoop()')).toContain('TickMs = 1000;');
  });

  it('pings with what the editor is doing, and answers with it as a running or queued task', () => {
    const sweep = body(keepalive, 'void FMcpNativeTransport::SweepRequestProgress()');
    expect(sweep).toContain('McpAutomationBridge::DescribeEditorWork(RequestId)');
    const answer = body(keepalive, 'void FMcpNativeTransport::AnswerStillRunning(');
    expect(answer).toContain('bAnsweredRunning.exchange(true)');
    expect(answer).toContain('Subsystem->IsAutomationRequestWaiting(RequestId)');
    expect(answer).toContain('bWaiting ? TEXT("queued") : TEXT("running")');
    expect(answer).toContain('McpBuildSuccessReceipt(Conn->CapabilityId, Data, Context, Raw, Message)');
    // The idempotency slot is settled by the work's own completion, never by the early answer.
    expect(answer).not.toContain('McpSettleIdempotency');
  });

  it('keeps an answered call until its work completes, writing nothing more to it', () => {
    expect(cleanup).toMatch(/Now - Conn->StartTime > Conn->MaxLifetimeSeconds\s*\|\| \(!Conn->bAnsweredRunning\.load\(\)/u);
    const pending = code('Private/MCP/Transport/McpNativeTransportPendingRequests.cpp');
    expect(pending).toContain('|| (*Found)->bMarkedForRemoval.load() || (*Found)->bAnsweredRunning.load())');
    expect(pending).toContain('McpSettleIdempotency(Conn->IdempotencySlot, bReportedSuccess, ReportedResult);');
  });

  it('never takes the execution lock a running handler holds', () => {
    const header = code('Public/McpAutomationBridgeSubsystem.h');
    const waiting = header.slice(header.indexOf('bool IsAutomationRequestWaiting('));
    expect(waiting.slice(0, waiting.indexOf('}'))).not.toContain('AutomationRequestExecutionMutex');
    expect(keepalive).not.toContain('CancelAutomationRequest');
  });
});

describe('every call says what the editor is busy with', () => {
  const responses = code('Private/Core/Subsystem/McpAutomationBridgeSubsystemResponses.cpp');

  it('names the work and its progress when a call is refused for a held game thread', () => {
    const stalled = responses.slice(responses.indexOf('case EAutomationQueueRejection::GameThreadStalled:'));
    expect(stalled.slice(0, stalled.indexOf('break;'))).toContain('McpAutomationBridge::DescribeEditorWork()');
  });

  it('records progress for the work in flight and sends it to whichever transport holds the call', () => {
    const send = responses.slice(responses.indexOf('void UMcpAutomationBridgeSubsystem::SendProgressUpdate('));
    expect(send).toContain('McpAutomationBridge::ReportInFlightProgress(RequestId, Percent, Message);');
    expect(send).toContain('NativeTransport->SendSSEProgressUpdate(RequestId, Percent, Message);');
    expect(send).not.toContain('Origin == ERequestOrigin::NativeHTTP');
  });

  it('describes the action in flight, how long it has run, its last progress and the shader queue', () => {
    const process = code('Private/Core/Requests/McpAutomationBridge_ProcessRequest.cpp');
    expect(process).toContain('if (GInFlightAction.IsEmpty() || RequestId != GInFlightRequestId) return;');
    expect(process).toContain('const int32 Shaders = McpShaderJobsRemaining();');
    expect(process).toContain('McpAutomationBridge::SetInFlightAction(RequestId,');
  });
});

describe('a folder delete reports as it goes', () => {
  const folder = code('Private/Safety/McpSafeOperationsFolderDelete.h');
  const assets = code('Private/Safety/McpSafeOperationsFolderDeleteAssets.h');

  it('removes unloaded files nothing outside the delete references, without loading them', () => {
    expect(folder).toContain('FindObject<UPackage>(nullptr, *PackageName)');
    expect(folder).toContain('return !Deleted(Ref.PackageName.ToString());');
    expect(folder).toContain('Registry.ScanModifiedAssetFiles(Removed);');
    expect(folder).toContain('FolderDeleteInternal::DeleteUnloadedAssetFiles(FolderPath, AlsoDeleted, OtherAssets, Progress);');
  });

  it('runs the engine delete in passes, each reporting how far it has got', () => {
    expect(assets).toContain('const int32 PassSize = FMath::Max(100, SafeAssets.Num() / 20 + 1);');
    expect(assets).toMatch(/for \(int32 First = 0; First < SafeAssets\.Num\(\); First \+= PassSize\)\s*\{[\s\S]*?Progress\(/u);
  });

  it('passes the progress and the rest of the request from the delete handler', () => {
    const handler = code('Private/Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowAssetMutation.cpp');
    expect(handler).toContain('McpSafeOperations::McpSafeDeleteFolder(SafePath, &Remaining, Report, DeleteSet)');
    expect(handler).toMatch(/SendProgressUpdate\(RequestId, \(PathIndex \+ Percent \/ 100\.0f\) \* 100\.0f \/ Total,/u);
  });
});
