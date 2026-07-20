// McpNativeTransportGatewayStream.cpp — shared SSE streaming + subsystem queue path

#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Transport/McpNativeTransportTimeoutPolicy.h"

void FMcpNativeTransport::StreamToolCall(
	const FString& ToolName, const FString& DispatchAction,
	const TSharedPtr<FJsonObject>& Arguments, const TSharedPtr<FJsonValue>& Id,
	FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin,
	const TSharedPtr<FJsonValue>& ProgressToken, const FString& CapabilityId,
	const TSharedPtr<FJsonObject>& OutputSchema, const FString& CorrelationId)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	const FString RequestId = FGuid::NewGuid().ToString();
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
	Conn->CorrelationId = CorrelationId;
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
		return;
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("tools/call: %s (RequestId=%s)"),
		*ToolName, *RequestId);

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
