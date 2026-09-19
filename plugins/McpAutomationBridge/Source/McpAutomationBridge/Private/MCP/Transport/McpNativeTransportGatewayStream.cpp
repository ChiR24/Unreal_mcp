// McpNativeTransportGatewayStream.cpp — shared SSE streaming + subsystem queue path

#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Transport/McpNativeTransportTimeoutPolicy.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"

void FMcpNativeTransport::StreamToolCall(
	const FString& ToolName, const FString& DispatchAction,
	const TSharedPtr<FJsonObject>& Arguments, const TSharedPtr<FJsonValue>& Id,
	FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin,
	const TSharedPtr<FJsonValue>& ProgressToken, const FString& CapabilityId,
	const TSharedPtr<FJsonObject>& OutputSchema, const FMcpReceiptContext& Context)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	// Minted by the caller before the pre-queue gate ran, so the consent burn it
	// recorded is keyed to the same id the response funnel will settle on.
	const FString RequestId = Context.QueueRequestId.IsEmpty()
		? FGuid::NewGuid().ToString()
		: Context.QueueRequestId;
	TSharedPtr<FSSEConnection> Conn = MakeShared<FSSEConnection>();
	Conn->Socket = ClientSocket;
	Conn->JsonRpcId = Id;
	Conn->ClientRequestIdKey = McpJsonRpcIdKey(Id);
	Conn->ProgressToken = ProgressToken;
	Conn->bHasProgressToken = ProgressToken.IsValid();
	Conn->StartTime = FPlatformTime::Seconds();
	const UMcpAutomationBridgeSettings* Settings =
		GetDefault<UMcpAutomationBridgeSettings>();
	Conn->TimeoutSeconds =
		McpNativeTransportTimeoutPolicy::ResolveToolCallTimeoutSeconds(
			ToolName, Arguments,
			Settings ? Settings->MaxMovieRenderTimeoutMs : 3600000,
			Settings ? Settings->MaxMovieRenderCancellationWaitMs : 30000);
	Conn->ToolName = ToolName;
	Conn->SessionId = SessionId;
	Conn->CapabilityId = CapabilityId;
	Conn->CorrelationId = Context.CorrelationId;
	Conn->RequestId = Context.RequestId;
	Conn->IdempotencyId = Context.IdempotencyId;
	Conn->IdempotencySlot = Context.IdempotencySlot;
	Conn->RequestStartSeconds = Context.StartTimeSeconds;
	Conn->OutputSchema = OutputSchema;
	bool bPendingLimitReached = false;
	bool bSessionInvalid = false;
	{
		FScopeLock SessionLock(&SessionMutex);
		if (!ActiveSessions.Contains(SessionId))
		{
			bSessionInvalid = true;
		}
	}
	if (!bSessionInvalid)
	{
		FScopeLock ConnectionLock(&SSEConnectionsMutex);
		int32 SessionPendingCount = 0;
		for (const TPair<FString, TSharedPtr<FSSEConnection>>& Entry :
			 SSEConnections)
		{
			if (Entry.Value.IsValid() &&
				Entry.Value->SessionId == SessionId)
			{
				++SessionPendingCount;
			}
		}
		if (SSEConnections.Num() >= MaxPendingToolCalls ||
			SessionPendingCount >= MaxPendingToolCallsPerSession)
		{
			bPendingLimitReached = true;
		}
		else
		{
			SSEConnections.Add(RequestId, Conn);
		}
	}
	if (bSessionInvalid)
	{
		// Every exit that abandons the request must also release the ledger slot
		// claimed for it. The ledger evicts only COMPLETED entries, so a slot
		// left in-flight here is never reclaimed and that idempotency key
		// answers IDEMPOTENCY_CONFLICT for the rest of the process lifetime.
		McpSettleIdempotency(Conn->IdempotencySlot, false, nullptr);
		Conn->IdempotencySlot.Reset();
		const FString Body = FMcpJsonRpc::BuildError(
			Id, FMcpJsonRpc::ErrorInvalidRequest,
			TEXT("Invalid or expired session ID"));
		SendAndClose(
			ClientSocket, 404, TEXT("application/json"), Body, {}, CorsOrigin);
		return;
	}
	if (bPendingLimitReached)
	{
		// Reached by a 5th concurrent keyed execute on one session
		// (MaxPendingToolCallsPerSession = 4). Refusing with 429 and keeping the
		// slot would wedge that key permanently for a call that never ran.
		McpSettleIdempotency(Conn->IdempotencySlot, false, nullptr);
		Conn->IdempotencySlot.Reset();
		TSharedPtr<FJsonObject> ToolResult =
			FMcpJsonRpc::BuildToolResult(
				false, TEXT("Native MCP pending tool-call limit reached"),
				nullptr, TEXT("TOO_MANY_PENDING_TOOL_CALLS"));
		const FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendAndClose(
			ClientSocket, 429, TEXT("application/json"), Body, {}, CorsOrigin);
		return;
	}

	bool bHeadersSent = false;
	{
		FScopeLock WriteLock(&Conn->WriteMutex);
		if (Conn->Socket)
		{
			bHeadersSent = SendSSEHeaders(
				Conn->Socket, SessionId, CorsOrigin);
			if (!bHeadersSent)
			{
				Conn->Socket->Close();
				if (SocketSub) SocketSub->DestroySocket(Conn->Socket);
				Conn->Socket = nullptr;
			}
		}
	}
	if (!bHeadersSent)
	{
		{
			FScopeLock Lock(&SSEConnectionsMutex);
			SSEConnections.Remove(RequestId);
		}
		// The client closed its socket after POSTing. Nothing was dispatched, so
		// the slot must go back rather than pin the key forever.
		McpSettleIdempotency(Conn->IdempotencySlot, false, nullptr);
		Conn->IdempotencySlot.Reset();
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("Failed to send SSE headers for tool %s"), *ToolName);
		return;
	}

	// Log the capability being dispatched, not just the parent tool. A silent
	// editor death leaves only this line behind, and "tool=control_editor" is
	// twenty different actions — useless for attributing a crash. Operation,
	// tool and action are the three fields that identify the leaf.
	FString CallDetail = DispatchAction;
	if (Arguments.IsValid())
	{
		FString Op, InnerTool, InnerAction;
		Arguments->TryGetStringField(TEXT("operation"), Op);
		Arguments->TryGetStringField(TEXT("tool"), InnerTool);
		Arguments->TryGetStringField(TEXT("action"), InnerAction);
		// The native surface does not always carry operation/tool (ToolName is
		// already logged separately), so build the tightest label the payload
		// supports rather than padding absent fields with '?'.
		if (!InnerTool.IsEmpty() && !InnerAction.IsEmpty())
		{
			CallDetail = InnerTool + TEXT(".") + InnerAction;
		}
		else if (!InnerAction.IsEmpty())
		{
			CallDetail = InnerAction;
		}
		if (!Op.IsEmpty())
		{
			CallDetail = Op + TEXT(" ") + CallDetail;
		}
	}
	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("tools/call: %s [%s] (RequestId=%s)"),
		*ToolName, *CallDetail, *RequestId);

	TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(Subsystem);
	FString CapturedRequestId = RequestId;
	FString CapturedDispatchAction = DispatchAction;
	TSharedPtr<FJsonObject> CapturedArguments = Arguments;

	if (WeakSubsystem.IsValid())
	{
		bool bSessionActive = false;
		EAutomationQueueRejection QueueRejection =
			EAutomationQueueRejection::None;
		const bool bQueued = QueueAutomationRequestForSession(
			SessionId, CapturedRequestId, CapturedDispatchAction,
			CapturedArguments, bSessionActive, QueueRejection,
			Context.ExpectedRevisions);
		if (!bSessionActive)
		{
			CompletePendingRequest(
				CapturedRequestId, false,
				TEXT("Invalid or expired session ID"), nullptr,
				TEXT("INVALID_SESSION"));
		}
		else if (!bQueued)
		{
			// Mirror the WebSocket surface's per-code refusal
			// (SendAutomationRejection): every queue rejection answers with one
			// typed response and never entered the queue.
			const TCHAR* ErrorCode = TEXT("AUTOMATION_QUEUE_FULL");
			const TCHAR* RefusalMessage =
				TEXT("Automation request rejected: queue is full");
			switch (QueueRejection)
			{
			case EAutomationQueueRejection::GameThreadStalled:
				ErrorCode = TEXT("EDITOR_BLOCKED");
				RefusalMessage = TEXT("Automation request rejected: the editor game thread has not ticked for over 15 s (a modal dialog or a blocking operation is holding it); dismiss it and retry");
				break;
			case EAutomationQueueRejection::NotAccepting:
				ErrorCode = TEXT("AUTOMATION_NOT_ACCEPTING");
				RefusalMessage = TEXT(
					"Automation request rejected: subsystem is not accepting requests");
				break;
			case EAutomationQueueRejection::AlreadyCanceled:
				ErrorCode = TEXT("AUTOMATION_ALREADY_CANCELED");
				RefusalMessage = TEXT(
					"Automation request rejected: request was already canceled");
				break;
			case EAutomationQueueRejection::SessionQueueFull:
				ErrorCode = TEXT("AUTOMATION_SESSION_QUEUE_FULL");
				RefusalMessage = TEXT(
					"Automation request rejected: this session already has the maximum number of queued requests; retry after your queued work drains");
				break;
			case EAutomationQueueRejection::QueueFull:
			case EAutomationQueueRejection::None:
			default:
				break;
			}
			CompletePendingRequest(
				CapturedRequestId, false, RefusalMessage, nullptr,
				ErrorCode);
		}
	}
	else
	{
		CompletePendingRequest(
			CapturedRequestId, false,
			TEXT("Automation subsystem is unavailable"), nullptr,
			TEXT("NOT_AVAILABLE"));
	}
}
