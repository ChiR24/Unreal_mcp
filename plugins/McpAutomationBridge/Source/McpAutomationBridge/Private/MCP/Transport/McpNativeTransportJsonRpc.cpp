#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Transport/McpNativeTransportArgumentValidation.h"
#include "MCP/Transport/McpNativeTransportTimeoutPolicy.h"

// ─── Tools Call (SSE streaming) ─────────────────────────────────────────────

void FMcpNativeTransport::HandleToolsCall(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	if (!Params.IsValid())
	{
		FString ErrorBody = FMcpJsonRpc::BuildError(
			Id, FMcpJsonRpc::ErrorInvalidParams, TEXT("Missing params"));
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ErrorBody, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	FString ToolName;
	if (!Params->TryGetStringField(TEXT("name"), ToolName))
	{
		FString ErrorBody = FMcpJsonRpc::BuildError(
			Id, FMcpJsonRpc::ErrorInvalidParams, TEXT("Missing tool name"));
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ErrorBody, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	TSharedPtr<FJsonObject> Arguments;
	const TSharedPtr<FJsonValue> ArgsValue = Params->TryGetField(TEXT("arguments"));

	if (ArgsValue.IsValid() && ArgsValue->Type != EJson::Null)
	{
		if (ArgsValue->Type != EJson::Object)
		{
			FString ErrorBody = FMcpJsonRpc::BuildError(
				Id, FMcpJsonRpc::ErrorInvalidParams,
				TEXT("'arguments' must be an object if provided"));
			SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ErrorBody, {}, CorsOrigin);
			ClientSocket->Close();
			if (SocketSub) SocketSub->DestroySocket(ClientSocket);
			return;
		}
		Arguments = ArgsValue->AsObject();
	}

	if (!Arguments.IsValid())
	{
		Arguments = MakeShared<FJsonObject>();
	}

	// Enforce tool enabled check — tools/list filters, tools/call must also enforce
	if (!ToolManager.IsToolEnabled(ToolName))
	{
		TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
			false,
			FString::Printf(TEXT("Tool '%s' is not enabled"), *ToolName),
			nullptr, TEXT("TOOL_DISABLED"));
		FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	if (TryHandleLocalToolCall(
			ToolName, Arguments, Id, ClientSocket, SessionId, CorsOrigin))
	{
		return;
	}

	FMcpToolDefinition* ToolDef = FMcpToolRegistry::Get().FindTool(ToolName);
	FString ArgumentPath, ArgumentErrorCode, ArgumentErrorMessage;
	if (!McpNativeArgumentValidation::ValidateToolArguments(
			ToolDef, Arguments, ArgumentPath, ArgumentErrorCode,
			ArgumentErrorMessage))
	{
		const FString Message = ArgumentErrorMessage.IsEmpty()
			? FString::Printf(
				TEXT("Tool '%s' could not validate its arguments"), *ToolName)
			: ArgumentErrorMessage;
		TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
			false, Message, nullptr,
			ArgumentErrorCode.IsEmpty()
				? TEXT("INVALID_TOOL_ARGUMENT")
				: ArgumentErrorCode);
		const FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendHttpResponse(
			ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	const FString RequestId = FGuid::NewGuid().ToString();
	TSharedPtr<FSSEConnection> Conn = MakeShared<FSSEConnection>();
	Conn->Socket = ClientSocket;
	Conn->JsonRpcId = Id;
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
	bool bPendingLimitReached = false;
	bool bSessionInvalid = false;
	{
		// Session validity is checked under SessionMutex; the lock is released
		// before acquiring SSEConnectionsMutex to keep the global lock order
		// documented in McpNativeTransport.h (no nested Session→SSE critical
		// sections). The TOCTOU window (session expiring between check and add)
		// is bounded: CloseSessionConnections cleans up the dangling SSE entry
		// on session timeout.
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
		const FString Body = FMcpJsonRpc::BuildError(
			Id, FMcpJsonRpc::ErrorInvalidRequest,
			TEXT("Invalid or expired session ID"));
		SendHttpResponse(
			ClientSocket, 404, TEXT("application/json"), Body, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}
	if (bPendingLimitReached)
	{
		TSharedPtr<FJsonObject> ToolResult =
			FMcpJsonRpc::BuildToolResult(
				false, TEXT("Native MCP pending tool-call limit reached"),
				nullptr, TEXT("TOO_MANY_PENDING_TOOL_CALLS"));
		const FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendHttpResponse(
			ClientSocket, 429, TEXT("application/json"), Body, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
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
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("Failed to send SSE headers for tool %s"), *ToolName);
		// Note: DiscardCanceledAutomationRequest would be a no-op here because
		// the request has not been queued with the subsystem yet (queueing
		// happens after this early-return path). The SSE-headers-failed path
		// only needs the SSEConnections cleanup above.
		return;
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("tools/call: %s (RequestId=%s)"),
		*ToolName, *RequestId);

	// Resolve dispatch action using tool definition metadata.
	// Pattern A: pass tool name as Action (handler checks Action == "tool_name")
	// Pattern B: extract sub-action from arguments (handler checks Action.StartsWith("sub_action"))
	FString DispatchAction = ToolName;
	if (ToolDef && !ToolDef->UsesToolNameDispatch())
	{
		// Pattern B: extract action from arguments
		FString ActionField = ToolDef->GetActionFieldName();
		FString Extracted;
		if (Arguments->TryGetStringField(ActionField, Extracted) && !Extracted.IsEmpty())
		{
			if (!IsValidSnakeCaseAction(Extracted))
			{
				CompletePendingRequest(
					RequestId, false,
					FString::Printf(
						TEXT("Invalid '%s' value for tool '%s': must be a snake_case identifier"),
						*ActionField, *ToolName),
					nullptr, TEXT("INVALID_PARAMS"));
				return;
			}
			DispatchAction = Extracted;
		}
		else
		{
			CompletePendingRequest(
				RequestId, false,
				FString::Printf(TEXT("Missing required '%s' field in arguments for tool '%s'"),
					*ActionField, *ToolName),
				nullptr, TEXT("INVALID_PARAMS"));
			return;
		}
	}

	// Normalize: some handlers read "subAction" instead of "action".
	// Ensure both fields exist so handlers find the value regardless of field name.
	if (!Arguments->HasField(TEXT("subAction")) && Arguments->HasField(TEXT("action")))
	{
		FString ActionVal;
		Arguments->TryGetStringField(TEXT("action"), ActionVal);
		Arguments->SetStringField(TEXT("subAction"), ActionVal);
	}

	// Dispatch through the subsystem queue. The queue is drained by the core
	// ticker after world ticking, which is required for safe map transitions.
	TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(Subsystem);
	FString CapturedRequestId = RequestId;
	FString CapturedDispatchAction = DispatchAction;
	TSharedPtr<FJsonObject> CapturedArguments = Arguments;

	if (WeakSubsystem.IsValid())
	{
		bool bSessionActive = false;
		const bool bQueued = QueueAutomationRequestForSession(
			SessionId, CapturedRequestId, CapturedDispatchAction,
			CapturedArguments, bSessionActive);
		if (!bSessionActive)
		{
			CompletePendingRequest(
				CapturedRequestId, false,
				TEXT("Invalid or expired session ID"), nullptr,
				TEXT("INVALID_SESSION"));
		}
		else if (!bQueued)
		{
			CompletePendingRequest(
				CapturedRequestId, false,
				TEXT("Automation request queue is full"), nullptr,
				TEXT("AUTOMATION_QUEUE_FULL"));
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

// ─── SSE Connection Management ──────────────────────────────────────────────
