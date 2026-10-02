#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Gateway/McpNativeGatewayExecuteReceiptBuild.h"

namespace
{
// ACTOR_NOT_FOUND from any handler said only "Actor not found" ("Bug1" beside Bug_01..Bug_10).
// A find by the same name answers with the labels it resembles (similar), so the refusal hands
// that call over. Mirrors actorNotFoundGuidance in src/server/gateway/gateway-execute-dispatch.ts.
void AddActorNotFoundGuidance(const TSharedPtr<FJsonObject>& Receipt, const TSharedPtr<FJsonObject>& Arguments)
{
	FString Code, Name;
	if (!Receipt.IsValid() || !Arguments.IsValid() || !Receipt->TryGetStringField(TEXT("errorCode"), Code) ||
		Code != TEXT("ACTOR_NOT_FOUND") || !Arguments->TryGetStringField(TEXT("actorName"), Name) || Name.IsEmpty())
	{
		return;
	}
	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("findBy"), TEXT("name"));
	Params->SetStringField(TEXT("name"), Name);
	TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
	Next->SetStringField(TEXT("operation"), TEXT("execute"));
	Next->SetStringField(TEXT("tool"), TEXT("control_actor"));
	Next->SetStringField(TEXT("action"), TEXT("find"));
	Next->SetObjectField(TEXT("params"), Params);
	Receipt->SetArrayField(TEXT("suggestions"), {MakeShared<FJsonValueString>(FString::Printf(
		TEXT("No actor in the world is labeled or named '%s'; control_actor find by name lists near labels under similar."), *Name))});
	Receipt->SetObjectField(TEXT("nextCall"), Next);
}

// A handler that refuses a call because another call settles it can name that call in its own reply:
// the Fab add turns a second import away while one runs and hands back the read that reports the first.
// Only an executable gateway call is passed on. Mirrors handlerNextCall in
// src/server/gateway/gateway-execute-dispatch.ts.
void AddHandlerNextCall(const TSharedPtr<FJsonObject>& Receipt, const TSharedPtr<FJsonObject>& Result)
{
	const TSharedPtr<FJsonObject>* Next = nullptr;
	const TSharedPtr<FJsonObject>* Params = nullptr;
	FString Operation, Tool, Action;
	if (!Receipt.IsValid() || !Result.IsValid() || !Result->TryGetObjectField(TEXT("nextCall"), Next) || Next == nullptr ||
		!(*Next)->TryGetStringField(TEXT("operation"), Operation) || Operation != TEXT("execute") ||
		!(*Next)->TryGetStringField(TEXT("tool"), Tool) || !(*Next)->TryGetStringField(TEXT("action"), Action) ||
		!(*Next)->TryGetObjectField(TEXT("params"), Params))
	{
		return;
	}
	Receipt->SetObjectField(TEXT("nextCall"), *Next);
}
}

bool FMcpNativeTransport::CompletePendingRequest(
	const FString& RequestId, bool bSuccess, const FString& Message,
	const TSharedPtr<FJsonObject>& Result, const FString& ErrorCode)
{
	TSharedPtr<FSSEConnection> Conn;
	{
		FScopeLock Lock(&SSEConnectionsMutex);
		TSharedPtr<FSSEConnection>* Found = SSEConnections.Find(RequestId);
		if (!Found)
		{
			return false;
		}
		Conn = *Found;
		SSEConnections.Remove(RequestId);
		// Defensive reset: the async SendSSEProgressUpdate writer normally
		// clears this flag at the end of its lambda, but if the lambda is
		// never executed (e.g. Async() dispatch failure on a hot path) the
		// flag would otherwise block every subsequent progress write for
		// this RequestId.
		if (Conn.IsValid())
		{
			Conn->bProgressWritePending.store(false);
		}
	}

	if (!Conn.IsValid())
	{
		return true;  // Already cleaned up
	}

	// Late-response suppression: if this request was cancelled via
	// notifications/cancelled, do not deliver a result to the client. Close the
	// SSE socket so the stream ends cleanly; the subsystem's eventual completion
	// is intentionally dropped (the client already abandoned the request).
	// bCancelled is the race-safe primary signal: it is set by
	// HandleCancelledNotification under SSEConnectionsMutex (the lock we just
	// released after removing this conn), so whichever thread took that mutex
	// first wins deterministically. CancelledInternalRequestIds is consulted as
	// a secondary safeguard. The three marker structures are torn down here so
	// they never outlive the completed/cancelled request.
	bool bCancelled = Conn->bCancelled.load();
	{
		FScopeLock Lock(&CancelledRequestsMutex);
		// SSEConnectionsMutex is already released, so taking CancelledRequestsMutex
		// here preserves the documented lock order and avoids any double-free.
		if (CancelledInternalRequestIds.Contains(RequestId))
		{
			CancelledInternalRequestIds.Remove(RequestId);
			CancelledClientIdToInternal.Remove(Conn->ClientRequestIdKey);
			CancelledMarkerOrder.Remove(Conn->ClientRequestIdKey);
			bCancelled = true;
		}
	}
	Conn->bMarkedForRemoval.store(true);

	if (bCancelled)
	{
		// A claimed slot MUST be released on every exit, not just the one that
		// builds a receipt. The ledger only ever evicts completed entries, so an
		// orphaned in-flight slot is immortal: every later execute with that key
		// answers IDEMPOTENCY_CONFLICT for the life of the editor process.
		// Abandon (not Complete) is correct here - a cancelled call recorded
		// nothing, so the key must stay free for a genuine retry.
		McpSettleIdempotency(Conn->IdempotencySlot, false, nullptr);
		Conn->IdempotencySlot.Reset();

		FScopeLock WriteLock(&Conn->WriteMutex);
		CloseSocket(Conn->Socket);
		return true;
	}

	// Build final JSON-RPC result (cheap, no I/O)
	bool bReportedSuccess = bSuccess;
	FString ReportedMessage = Message;
	FString ReportedErrorCode = ErrorCode;
	TSharedPtr<FJsonObject> ReportedResult = Result;
	if (!Conn->CapabilityId.IsEmpty())
	{
		FMcpReceiptContext Context;
		Context.CorrelationId = Conn->CorrelationId;
		Context.RequestId = Conn->RequestId;
		Context.IdempotencyId = Conn->IdempotencyId;
		Context.StartTimeSeconds = Conn->RequestStartSeconds;
		Context.GatewayWarnings = Conn->GatewayWarnings;
		ReportedResult = McpBuildGatewayExecuteReceipt(
			Conn->CapabilityId, Conn->OutputSchema, Context, bSuccess, Message, Result, ErrorCode);
		if (!bSuccess)
		{
			AddHandlerNextCall(ReportedResult, Result);
		}
		AddActorNotFoundGuidance(ReportedResult, Conn->Arguments);
		bReportedSuccess = McpReceiptSucceeded(ReportedResult);
		ReportedMessage = McpReceiptMessage(ReportedResult);
		ReportedErrorCode.Reset();
		ReportedResult->TryGetStringField(TEXT("errorCode"), ReportedErrorCode);
		McpSettleIdempotency(Conn->IdempotencySlot, bReportedSuccess, ReportedResult);
	}
	else
	{
		// No capability id means no receipt was built, so there is nothing to
		// record - but a slot may still have been claimed upstream and would
		// otherwise leak exactly as the cancel path did.
		McpSettleIdempotency(Conn->IdempotencySlot, false, nullptr);
	}
	Conn->IdempotencySlot.Reset();
	if (Conn->bAnsweredRunning.load())
	{
		// The client was already told it was still running (AnswerStillRunning); the outcome goes to the log.
		UE_LOG(LogMcpNativeTransport, Log, TEXT("tools/call %s finished after its still-running answer (tool=%s, success=%s): %s"),
			*RequestId, *Conn->ToolName, bReportedSuccess ? TEXT("true") : TEXT("false"), *ReportedMessage.Left(400));
	}
	TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
		bReportedSuccess, ReportedMessage, ReportedResult, ReportedErrorCode);
	FString ResponseBody = FMcpJsonRpc::BuildResponse(Conn->JsonRpcId, ToolResult);

	// Offload blocking write + close to thread pool so GameThread is not blocked
	FString CapturedRequestId = RequestId;
	FString CapturedToolName = Conn->ToolName;
	FString CapturedSessionId = Conn->SessionId;
	bool bCapturedSuccess = bReportedSuccess;
	PendingAsyncWrites.fetch_add(1);

	Async(EAsyncExecution::ThreadPool,
		[this, Conn, ResponseBody = MoveTemp(ResponseBody),
		 CapturedRequestId, CapturedToolName, CapturedSessionId, bCapturedSuccess]()
	{
		bool bWroteResponse = false;
		{
			FScopeLock WriteLock(&Conn->WriteMutex);
			if (!Conn->Socket)
			{
				PendingAsyncWrites.fetch_sub(1);
				return;  // Already cleaned up by Shutdown
			}

			// Inline SSE write — we already hold WriteMutex
			bWroteResponse = SendSSEFrame(Conn->Socket, ResponseBody);

			CloseSocket(Conn->Socket);
		}
		if (bWroteResponse)
		{
			TouchSession(CapturedSessionId);
		}

		UE_LOG(LogMcpNativeTransport, Log,
			TEXT("tools/call completed: %s (tool=%s, success=%s)"),
			*CapturedRequestId, *CapturedToolName,
			bCapturedSuccess ? TEXT("true") : TEXT("false"));

		PendingAsyncWrites.fetch_sub(1);
	});

	return true;
}

void FMcpNativeTransport::SendSSEProgressUpdate(
	const FString& RequestId, float Percent, const FString& Message)
{
	TSharedPtr<FSSEConnection> Conn;
	FString CapturedSessionId;
	{
		FScopeLock Lock(&SSEConnectionsMutex);
		TSharedPtr<FSSEConnection>* Found = SSEConnections.Find(RequestId);
		if (!Found || !Found->IsValid() || !(*Found)->Socket
			|| (*Found)->bMarkedForRemoval.load() || (*Found)->bAnsweredRunning.load())
		{
			return;
		}
		Conn = *Found;
		CapturedSessionId = Conn->SessionId;
		// Progress never goes backwards: the cleanup heartbeat reports 0, and a
		// client that saw 60% earlier must not be told the call regressed.
		Percent = FMath::Max(Percent, Conn->LastProgressPercent);
		Conn->LastProgressPercent = Percent;
		bool bExpected = false;
		if (!Conn->bProgressWritePending.compare_exchange_strong(
				bExpected, true))
		{
			return;
		}
	}

	// Build progress JSON before offloading (cheap, no I/O). Echo the client's
	// own progressToken when present so it can correlate the notification to its
	// request; otherwise fall back to the internal request id (preserves the
	// prior behavior for token-less clients).
	TSharedPtr<FJsonValue> Token = (Conn->bHasProgressToken && Conn->ProgressToken.IsValid())
		? Conn->ProgressToken
		: MakeShared<FJsonValueString>(RequestId);
	FString ProgressJson = FMcpJsonRpc::BuildProgressNotification(
		Token, Percent, 100.0f, Message);

	FString CapturedRequestId = RequestId;
	PendingAsyncWrites.fetch_add(1);

	Async(EAsyncExecution::ThreadPool,
		[this, Conn, ProgressJson = MoveTemp(ProgressJson), CapturedRequestId,
		 CapturedSessionId]()
	{
		// Guard: if transport is shutting down, bail out
		if (bStopping.load())
		{
			Conn->bProgressWritePending.store(false);
			PendingAsyncWrites.fetch_sub(1);
			return;
		}

		if (WriteSSEEvent(*Conn, ProgressJson))
		{
			// Progress is activity for the IDLE budget only. StartTime stays at
			// creation: it anchors MaxLifetimeSeconds, and resetting it here let
			// the cleanup heartbeat (which comes through this same path) push
			// the ceiling out forever, so a handler that never answered was
			// never expired.
			{
				FScopeLock Lock(&SSEConnectionsMutex);
				Conn->LastProgressTime = FPlatformTime::Seconds();
			}
			// Touch session so long-running tool calls don't expire the session
			if (!CapturedSessionId.IsEmpty())
			{
				FScopeLock Lock(&SessionMutex);
				double* LastActivity = ActiveSessions.Find(CapturedSessionId);
				if (LastActivity)
				{
					*LastActivity = FPlatformTime::Seconds();
				}
			}
		}
		else
		{
			UE_LOG(LogMcpNativeTransport, Warning,
				TEXT("SSE write failed for request %s — marking for removal"),
				*CapturedRequestId);
			Conn->bMarkedForRemoval.store(true);
		}

		Conn->bProgressWritePending.store(false);
		PendingAsyncWrites.fetch_sub(1);
	});
}
