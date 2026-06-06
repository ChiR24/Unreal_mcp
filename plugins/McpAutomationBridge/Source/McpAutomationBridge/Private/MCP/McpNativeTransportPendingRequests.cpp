#include "MCP/McpNativeTransportPrivate.h"

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
	}

	if (!Conn.IsValid())
	{
		return true;  // Already cleaned up
	}

	// Signal any snapshot holders (e.g. BroadcastToolsListChanged) that
	// this connection is being torn down — prevents them from attempting writes.
	Conn->bMarkedForRemoval.store(true);

	// Build final JSON-RPC result (cheap, no I/O)
	TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
		bSuccess, Message, Result, ErrorCode);
	FString ResponseBody = FMcpJsonRpc::BuildResponse(Conn->JsonRpcId, ToolResult);

	// Offload blocking write + close to thread pool so GameThread is not blocked
	FString CapturedRequestId = RequestId;
	FString CapturedToolName = Conn->ToolName;
	FString CapturedSessionId = Conn->SessionId;
	bool bCapturedSuccess = bSuccess;
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
			FString Frame = FString::Printf(
				TEXT("event: message\ndata: %s\n\n"), *ResponseBody);
			FTCHARToUTF8 Utf8(*Frame);
			bWroteResponse = SendAllBytes(Conn->Socket,
				reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());

			Conn->Socket->Close();
			ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
			if (SocketSub)
			{
				SocketSub->DestroySocket(Conn->Socket);
			}
			Conn->Socket = nullptr;
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

bool FMcpNativeTransport::HasPendingRequest(const FString& RequestId) const
{
	FScopeLock Lock(&SSEConnectionsMutex);
	return SSEConnections.Contains(RequestId);
}

void FMcpNativeTransport::TouchPendingRequest(const FString& RequestId)
{
	FScopeLock Lock(&SSEConnectionsMutex);
	TSharedPtr<FSSEConnection>* Found = SSEConnections.Find(RequestId);
	if (Found && Found->IsValid())
	{
		(*Found)->StartTime = FPlatformTime::Seconds();
	}
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
			|| (*Found)->bMarkedForRemoval.load())
		{
			return;
		}
		Conn = *Found;
		CapturedSessionId = Conn->SessionId;
	}

	// Build progress JSON before offloading (cheap, no I/O)
	FString ProgressJson = FMcpJsonRpc::BuildProgressNotification(
		RequestId, Percent, 100.0f, Message);

	FString CapturedRequestId = RequestId;
	PendingAsyncWrites.fetch_add(1);

	Async(EAsyncExecution::ThreadPool,
		[this, Conn, ProgressJson = MoveTemp(ProgressJson), CapturedRequestId,
		 CapturedSessionId]()
	{
		// Guard: if transport is shutting down, bail out
		if (bStopping.load())
		{
			PendingAsyncWrites.fetch_sub(1);
			return;
		}

		if (WriteSSEEvent(*Conn, ProgressJson))
		{
			// Reset SSE request timeout
			{
				FScopeLock Lock(&SSEConnectionsMutex);
				Conn->StartTime = FPlatformTime::Seconds();
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

		PendingAsyncWrites.fetch_sub(1);
	});
}
