#include "MCP/Transport/McpNativeTransportPrivate.h"

void FMcpNativeTransport::HandleConnection(FSocket* ClientSocket)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	FParsedHttpRequest HttpReq;
	if (!ReadHttpRequest(ClientSocket, HttpReq))
	{
		SendHttpResponse(ClientSocket, 400, TEXT("text/plain"), TEXT("Bad Request"));
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Only accept /mcp path
	if (HttpReq.Path != TEXT("/mcp"))
	{
		SendHttpResponse(ClientSocket, 404, TEXT("text/plain"), TEXT("Not Found"));
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Browser access to the native MCP endpoint is only allowed when capability
	// tokens are enabled; non-browser local clients do not require CORS.
	if (HttpReq.Method == TEXT("OPTIONS"))
	{
		if (IsAllowedCorsOrigin(HttpReq.Origin))
		{
			SendHttpResponse(ClientSocket, 204, TEXT("text/plain"), FString(), {}, HttpReq.Origin);
		}
		else
		{
			SendHttpResponse(ClientSocket, 403, TEXT("text/plain"),
				TEXT("CORS preflight requires capability-token protection"));
		}
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	if (!HttpReq.Origin.IsEmpty() && !IsAllowedCorsOrigin(HttpReq.Origin))
	{
		SendHttpResponse(ClientSocket, 403, TEXT("text/plain"), TEXT("Invalid Origin"));
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Capability token validation (mirrors McpConnectionManager logic)
	{
		const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
		if (Settings && Settings->bRequireCapabilityToken)
		{
			if (HttpReq.CapabilityToken.IsEmpty() || HttpReq.CapabilityToken != Settings->CapabilityToken)
			{
				UE_LOG(LogMcpNativeTransport, Warning, TEXT("Capability token mismatch - rejecting connection"));
				FString ErrorBody = FMcpJsonRpc::BuildError(
					MakeShared<FJsonValueNull>(), FMcpJsonRpc::ErrorInvalidRequest,
					TEXT("Invalid capability token"));
				SendHttpResponse(ClientSocket, 401, TEXT("application/json"), ErrorBody, {}, HttpReq.Origin);
				ClientSocket->Close();
				SocketSub->DestroySocket(ClientSocket);
				return;
			}
		}
	}

	// ── DELETE /mcp — session termination ──
	if (HttpReq.Method == TEXT("DELETE"))
	{
		FString SessionError;
		ESessionValidationResult SessionStatus = ValidateSession(HttpReq.SessionId, SessionError);
		if (SessionStatus != ESessionValidationResult::Valid)
		{
			SendHttpResponse(ClientSocket, GetSessionValidationStatusCode(SessionStatus),
				TEXT("text/plain"), SessionError, {}, HttpReq.Origin);
			ClientSocket->Close();
			SocketSub->DestroySocket(ClientSocket);
			return;
		}

		if (!HttpReq.SessionId.IsEmpty())
		{
			{
				FScopeLock Lock(&SessionMutex);
				if (ActiveSessions.Remove(HttpReq.SessionId) > 0)
				{
					SessionRateStates.Remove(HttpReq.SessionId);
					UE_LOG(LogMcpNativeTransport, Log,
						TEXT("Session terminated by client (remaining: %d)"),
						ActiveSessions.Num());
				}
			}
			CloseSessionConnections(HttpReq.SessionId);
		}
		SendHttpResponse(ClientSocket, 200, TEXT("text/plain"), FString(), {}, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// ── GET /mcp — persistent SSE notification stream ──
	if (HttpReq.Method == TEXT("GET"))
	{
		if (!HttpReq.Accept.Contains(TEXT("text/event-stream")))
		{
			SendHttpResponse(ClientSocket, 406, TEXT("text/plain"),
				TEXT("Not Acceptable: requires Accept: text/event-stream"), {}, HttpReq.Origin);
			ClientSocket->Close();
			SocketSub->DestroySocket(ClientSocket);
			return;
		}
		FString SessionError;
		ESessionValidationResult SessionStatus = ValidateSession(HttpReq.SessionId, SessionError);
		if (SessionStatus != ESessionValidationResult::Valid)
		{
			SendHttpResponse(ClientSocket, GetSessionValidationStatusCode(SessionStatus),
				TEXT("text/plain"), SessionError, {}, HttpReq.Origin);
			ClientSocket->Close();
			SocketSub->DestroySocket(ClientSocket);
			return;
		}
		HandleGetMcp(ClientSocket, HttpReq.SessionId, HttpReq.Origin);
		return;  // Socket parked — no close here
	}

	// ── POST /mcp — JSON-RPC ──
	if (HttpReq.Method != TEXT("POST"))
	{
		SendHttpResponse(ClientSocket, 405, TEXT("text/plain"), TEXT("Method Not Allowed"), {}, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	FMcpJsonRpcRequest Rpc = FMcpJsonRpc::ParseRequest(HttpReq.Body);
	if (!Rpc.bValid)
	{
		int32 ErrorCode = (Rpc.ErrorType == EMcpJsonRpcError::ParseError)
			? FMcpJsonRpc::ErrorParseError
			: FMcpJsonRpc::ErrorInvalidRequest;
		// Echo id for InvalidRequest if available; null for ParseError per JSON-RPC 2.0
		TSharedPtr<FJsonValue> ErrorId = (Rpc.ErrorType == EMcpJsonRpcError::ParseError)
			? MakeShared<FJsonValueNull>()
			: (Rpc.Id.IsValid() ? Rpc.Id : MakeShared<FJsonValueNull>());
		FString ErrorBody = FMcpJsonRpc::BuildError(ErrorId, ErrorCode,
			(Rpc.ErrorType == EMcpJsonRpcError::ParseError)
				? TEXT("Parse error") : TEXT("Invalid Request"));
		SendHttpResponse(ClientSocket, 400, TEXT("application/json"), ErrorBody, {}, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	if (Rpc.bIsNotification && Rpc.Method == TEXT("initialize"))
	{
		FString ErrorBody = FMcpJsonRpc::BuildError(
			MakeShared<FJsonValueNull>(), FMcpJsonRpc::ErrorInvalidRequest,
			TEXT("initialize must include an id"));
		SendHttpResponse(ClientSocket, 400, TEXT("application/json"), ErrorBody, {}, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Session validation (skip for initialize)
	if (Rpc.Method != TEXT("initialize"))
	{
		FString SessionError;
		ESessionValidationResult SessionStatus = ValidateSession(HttpReq.SessionId, SessionError);
		if (SessionStatus != ESessionValidationResult::Valid)
		{
			FString ErrorBody = FMcpJsonRpc::BuildError(
				Rpc.Id, FMcpJsonRpc::ErrorInvalidRequest, SessionError);
			SendHttpResponse(ClientSocket, GetSessionValidationStatusCode(SessionStatus),
				TEXT("application/json"), ErrorBody, {}, HttpReq.Origin);
			ClientSocket->Close();
			SocketSub->DestroySocket(ClientSocket);
			return;
		}
		FString RateLimitError;
		if (!ConsumeSessionRequestBudget(
				HttpReq.SessionId, Rpc.Method == TEXT("tools/call"),
				RateLimitError))
		{
			const FString ErrorBody = FMcpJsonRpc::BuildError(
				Rpc.Id, FMcpJsonRpc::ErrorRateLimited, RateLimitError);
			SendHttpResponse(
				ClientSocket, 429, TEXT("application/json"), ErrorBody, {},
				HttpReq.Origin);
			ClientSocket->Close();
			SocketSub->DestroySocket(ClientSocket);
			return;
		}
	}

	// Notifications (no id) — 202 Accepted after session validation.
	if (Rpc.bIsNotification)
	{
		UE_LOG(LogMcpNativeTransport, Log,
			TEXT("Received notification: %s"), *Rpc.Method);
		SendHttpResponse(ClientSocket, 202, TEXT("text/plain"), FString(), {}, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// ── Method dispatch ──

	if (Rpc.Method == TEXT("initialize"))
	{
		FString NewSessionId;
		// Extract the remote IP:port for the rate-limit key. ClientSocket
		// may be null in test harnesses; fall back to an empty string so the
		// build key degrades to a clientInfo-only hash (less precise, but
		// never weaker than the prior behavior).
		FString ConnectionRemoteAddr;
		if (ClientSocket)
		{
			TSharedRef<FInternetAddr> RemoteAddr =
				ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
			// FSocket::GetAddress is void as of UE 5.8 (was bool in earlier
			// versions); it populates RemoteAddr unconditionally, so there is
			// no success check to branch on anymore.
			ClientSocket->GetAddress(*RemoteAddr);
			if (RemoteAddr->IsValid())
			{
				ConnectionRemoteAddr = RemoteAddr->ToString(true);
			}
		}
		FString ResponseBody = HandleInitialize(
			Rpc.Params, Rpc.Id, NewSessionId, ConnectionRemoteAddr);
		if (NewSessionId.IsEmpty())
		{
			SendHttpResponse(
				ClientSocket, 429, TEXT("application/json"), ResponseBody, {},
				HttpReq.Origin);
			ClientSocket->Close();
			SocketSub->DestroySocket(ClientSocket);
			return;
		}
		TMap<FString, FString> Headers;
		Headers.Add(TEXT("Mcp-Session-Id"), NewSessionId);
		const bool bResponseSent = SendHttpResponse(
			ClientSocket, 200, TEXT("application/json"), ResponseBody, Headers,
			HttpReq.Origin);
		if (bResponseSent)
		{
			MarkSessionInitializationComplete(NewSessionId);
		}
		else
		{
			{
				FScopeLock Lock(&SessionMutex);
				ActiveSessions.Remove(NewSessionId);
				SessionRateStates.Remove(NewSessionId);
			}
			CloseSessionConnections(NewSessionId);
		}
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	if (Rpc.Method == TEXT("tools/list"))
	{
		FString ResponseBody = HandleToolsList(Rpc.Id);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ResponseBody, {}, HttpReq.Origin);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	if (Rpc.Method == TEXT("tools/call"))
	{
		// HandleToolsCall takes ownership of the socket (SSE streaming)
		HandleToolsCall(Rpc.Params, Rpc.Id, ClientSocket, HttpReq.SessionId, HttpReq.Origin);
		return;  // Socket NOT closed here — parked for SSE
	}

	// Unknown method
	FString ErrorBody = FMcpJsonRpc::BuildError(
		Rpc.Id, FMcpJsonRpc::ErrorMethodNotFound,
		FString::Printf(TEXT("Unknown method: %s"), *Rpc.Method));
	SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ErrorBody, {}, HttpReq.Origin);
	ClientSocket->Close();
	SocketSub->DestroySocket(ClientSocket);
}

// ─── HTTP Parsing ───────────────────────────────────────────────────────────
