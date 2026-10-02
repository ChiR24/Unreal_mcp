#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "Foundation/McpScreenshotResample.h" // McpShaderWaitMaxSeconds

// ─── Keepalive Loop (dedicated thread) ──────────────────────────────────────
//
// Everything here runs on its own thread, never the game thread, so it keeps going while a handler, a modal or
// the engine (a delete, a shader compile, an import) holds the game thread for minutes. The notification-stream
// keepalive used to run from the game-thread cleanup pass, and a long stall let the client's stream idle out.

namespace
{
// A call still open this long answers with what Unreal is doing instead of timing out: clients commonly give a
// call 30 s and cut it off then, progress or not. Above McpShaderWaitMaxSeconds, so a capture that waits out its
// shaders still answers with its own result.
// ponytail: one fixed limit for every client; take it from the client (or a setting) when one allows longer calls.
constexpr double AnswerRunningAfterSeconds = 27.0;
static_assert(AnswerRunningAfterSeconds >= McpShaderWaitMaxSeconds + 2.0, "a shader-waiting capture must answer first");
// The longest an open call goes without hearing from the editor.
constexpr double PingSeconds = 8.0;
}

void FMcpNativeTransport::RunKeepaliveLoop()
{
	// A one-second tick: the call answer has to land between its limit and the client's. StopEvent is triggered by
	// Stop(), waking us immediately on shutdown.
	static constexpr uint32 TickMs = 1000;
	while (!bStopping.load())
	{
		if (StopEvent)
		{
			StopEvent->Wait(TickMs);
		}
		else
		{
			FPlatformProcess::Sleep(TickMs / 1000.0f);
		}
		if (bStopping.load())
		{
			break;
		}
		SweepNotificationKeepalives();
		SweepRequestProgress();
	}
}

void FMcpNativeTransport::SweepNotificationKeepalives()
{
	const double Now = FPlatformTime::Seconds();

	// Snapshot living streams under the map lock, then write outside it — the socket
	// write takes each stream's WriteMutex, so the two locks are never nested.
	TArray<TSharedPtr<FNotificationStream>> AliveSnapshot;
	{
		FScopeLock Lock(&NotificationStreamsMutex);
		for (auto& [StreamId, Stream] : NotificationStreams)
		{
			if (Stream.IsValid() && !Stream->bMarkedForRemoval.load()
				&& Now - Stream->LastKeepaliveTime >= KeepaliveIntervalSeconds
				&& Stream->bReady.load())
			{
				AliveSnapshot.Add(Stream);
			}
		}
	}

	for (const auto& Stream : AliveSnapshot)
	{
		if (!WriteNotificationKeepalive(*Stream))
		{
			Stream->bMarkedForRemoval.store(true);
		}
		else
		{
			Stream->LastKeepaliveTime = Now;
			TouchSession(Stream->SessionId);
		}
	}
}

void FMcpNativeTransport::SweepRequestProgress()
{
	const double Now = FPlatformTime::Seconds();
	TArray<FString> Ping;
	TArray<FString> Answer;
	{
		FScopeLock Lock(&SSEConnectionsMutex);
		for (const auto& [RequestId, Conn] : SSEConnections)
		{
			if (!Conn.IsValid() || Conn->bMarkedForRemoval.load() || Conn->bCancelled.load()
				|| Conn->bAnsweredRunning.load())
			{
				continue;
			}
			if (Now - Conn->StartTime >= AnswerRunningAfterSeconds)
			{
				Answer.Add(RequestId);
			}
			else if (Now - FMath::Max(Conn->LastProgressTime, Conn->StartTime) >= PingSeconds)
			{
				Ping.Add(RequestId);
			}
		}
	}
	// Sent outside the lock: both take SSEConnectionsMutex. A ping that is written counts as activity for the
	// idle budget, exactly as the game-thread heartbeat it replaces did; a client that stopped reading fails the
	// write and is reaped.
	for (const FString& RequestId : Ping)
	{
		const FString Work = McpAutomationBridge::DescribeEditorWork(RequestId);
		SendSSEProgressUpdate(RequestId, 0.0f, Work.IsEmpty() ? TEXT("still working") : Work);
	}
	for (const FString& RequestId : Answer)
	{
		AnswerStillRunning(RequestId);
	}
}

void FMcpNativeTransport::AnswerStillRunning(const FString& RequestId)
{
	TSharedPtr<FSSEConnection> Conn;
	float Percent = 0.0f;
	{
		FScopeLock Lock(&SSEConnectionsMutex);
		const TSharedPtr<FSSEConnection>* Found = SSEConnections.Find(RequestId);
		if (!Found || !Found->IsValid() || (*Found)->bMarkedForRemoval.load() || (*Found)->bCancelled.load()
			|| (*Found)->bAnsweredRunning.exchange(true))
		{
			return;
		}
		Conn = *Found;
		Percent = Conn->LastProgressPercent;
	}

	const double Elapsed = FPlatformTime::Seconds() - Conn->StartTime;
	const bool bWaiting = Subsystem && Subsystem->IsAutomationRequestWaiting(RequestId);
	const FString Work = McpAutomationBridge::DescribeEditorWork(RequestId);
	const FString Message = bWaiting
		? FString::Printf(TEXT("Queued for %.0f s and not started: Unreal is %s. It runs by itself once the editor is free; do not send it again."),
			Elapsed, Work.IsEmpty() ? TEXT("busy") : *Work)
		: FString::Printf(TEXT("Still running after %.0f s: %s. Unreal keeps going; this answer does not stop it. Until it is done every call answers with its progress; then read the result back instead of sending this call again."),
			Elapsed, Work.IsEmpty() ? TEXT("Unreal is still working on it") : *Work);

	// A success receipt whose task is still running (the receipt algebra's own long-call shape); the idempotency
	// slot stays claimed until the work completes and CompletePendingRequest settles it.
	TSharedPtr<FJsonObject> Task = MakeShared<FJsonObject>();
	Task->SetStringField(TEXT("taskId"), RequestId);
	Task->SetStringField(TEXT("state"), bWaiting ? TEXT("queued") : TEXT("running"));
	if (Percent > 0.0f)
	{
		Task->SetNumberField(TEXT("progress"), FMath::Min(Percent, 100.0f) / 100.0f);
	}
	TSharedPtr<FJsonObject> Raw = MakeShared<FJsonObject>();
	Raw->SetObjectField(TEXT("task"), Task);
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), Message);
	TSharedPtr<FJsonObject> Result = Data;
	if (!Conn->CapabilityId.IsEmpty())
	{
		FMcpReceiptContext Context;
		Context.CorrelationId = Conn->CorrelationId;
		Context.RequestId = Conn->RequestId;
		Context.IdempotencyId = Conn->IdempotencyId;
		Context.StartTimeSeconds = Conn->RequestStartSeconds;
		Context.GatewayWarnings = Conn->GatewayWarnings;
		Result = McpBuildSuccessReceipt(Conn->CapabilityId, Data, Context, Raw, Message);
	}
	const FString Body = FMcpJsonRpc::BuildResponse(
		Conn->JsonRpcId, FMcpJsonRpc::BuildToolResult(true, Message, Result, FString()));
	{
		FScopeLock WriteLock(&Conn->WriteMutex);
		if (Conn->Socket)
		{
			SendSSEFrame(Conn->Socket, Body);
			CloseSocket(Conn->Socket);
		}
	}
	TouchSession(Conn->SessionId);
	UE_LOG(LogMcpNativeTransport, Log, TEXT("tools/call %s answered while %s after %.0f s (tool=%s): %s"),
		*RequestId, bWaiting ? TEXT("queued") : TEXT("running"), Elapsed, *Conn->ToolName, *Work);
}
