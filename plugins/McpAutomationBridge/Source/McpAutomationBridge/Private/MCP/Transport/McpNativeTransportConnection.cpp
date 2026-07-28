#include "MCP/Transport/McpNativeTransportPrivate.h"

#include "MCP/Execute/McpNativeGatewayAuthorization.h"

void FMcpNativeTransport::HandleConnection(FSocket* ClientSocket)
{
	FParsedHttpRequest HttpReq;
	if (!ReadHttpRequest(ClientSocket, HttpReq))
	{
		SendAndClose(ClientSocket, 400, TEXT("text/plain"), TEXT("Bad Request"));
		return;
	}

	// Only accept /mcp path
	if (HttpReq.Path != TEXT("/mcp"))
	{
		SendAndClose(ClientSocket, 404, TEXT("text/plain"), TEXT("Not Found"));
		return;
	}

	// Browser access to the native MCP endpoint is only allowed when capability
	// tokens are enabled; non-browser local clients do not require CORS.
	if (HttpReq.Method == TEXT("OPTIONS"))
	{
		if (IsAllowedCorsOrigin(HttpReq.Origin))
		{
			SendAndClose(ClientSocket, 204, TEXT("text/plain"), FString(), {}, HttpReq.Origin);
		}
		else
		{
			SendAndClose(ClientSocket, 403, TEXT("text/plain"),
				TEXT("CORS preflight requires capability-token protection"), {}, HttpReq.Origin);
		}
		return;
	}

	if (!HttpReq.Origin.IsEmpty() && !IsAllowedCorsOrigin(HttpReq.Origin))
	{
		SendAndClose(ClientSocket, 403, TEXT("text/plain"), TEXT("Invalid Origin"), {}, HttpReq.Origin);
		return;
	}

	// Capability token validation (mirrors McpConnectionManager logic)
	{
		const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
		const bool bTokenRequired = Settings && Settings->bRequireCapabilityToken;
		// A token that was PRESENTED is always resolved, even when none is
		// required: otherwise an unresolvable token bound a zero-scope principal
		// to an accepted session and every later request failed as a scope error.
		if (Settings && (bTokenRequired || !HttpReq.CapabilityToken.IsEmpty()))
		{
			// Empty client token is rejected outright; the remaining comparison
			// is constant-time so a timing oracle cannot leak how much of the
			// token matched. The legacy compare is a compat input only — the
			// resolver scans every configured candidate, also in constant time,
			// so a scoped token authenticates here with narrower authority.
			const bool bLegacyTokenMatch =
				McpConstantTimeTokenEquals(HttpReq.CapabilityToken, Settings->CapabilityToken);
			const FMcpCapabilityPrincipal Principal =
				McpResolveNativePrincipal(HttpReq.CapabilityToken);
			if (HttpReq.CapabilityToken.IsEmpty() || !Principal.bAuthenticated)
			{
				UE_LOG(LogMcpNativeTransport, Warning, TEXT("Capability token mismatch - rejecting connection"));
				SendAndClose(ClientSocket, 401, TEXT("application/json"), FMcpJsonRpc::BuildError(MakeShared<FJsonValueNull>(), FMcpJsonRpc::ErrorInvalidRequest, TEXT("Invalid capability token")), {}, HttpReq.Origin);
				return;
			}
			if (bLegacyTokenMatch && Principal.bDeprecated)
			{
				UE_LOG(LogMcpNativeTransport, Warning,
					TEXT("Native /mcp authenticated with the deprecated all-or-nothing "
					     "capability token; migrate to a scoped token."));
			}
		}
	}

	// A token swap on an established session is refused before the session is
	// used for anything, so one token can never initialize and another act.
	if (!HttpReq.SessionId.IsEmpty()
		&& !VerifySessionPrincipal(HttpReq.SessionId, HttpReq.CapabilityToken))
	{
		UE_LOG(LogMcpNativeTransport, Warning, TEXT("Session principal mismatch - rejecting request"));
		SendAndClose(ClientSocket, 401, TEXT("application/json"), FMcpJsonRpc::BuildError(MakeShared<FJsonValueNull>(), FMcpJsonRpc::ErrorInvalidRequest, TEXT("Capability token does not match this session")), {}, HttpReq.Origin);
		return;
	}

	// ── DELETE /mcp — session termination ──
	if (HttpReq.Method == TEXT("DELETE"))
	{
		FString SessionError;
		ESessionValidationResult SessionStatus = ValidateSession(HttpReq.SessionId, SessionError);
		if (SessionStatus != ESessionValidationResult::Valid)
		{
			SendAndClose(ClientSocket, GetSessionValidationStatusCode(SessionStatus),
				TEXT("text/plain"), SessionError, {}, HttpReq.Origin);
			return;
		}

		if (!HttpReq.SessionId.IsEmpty())
		{
			{
				FScopeLock Lock(&SessionMutex);
				if (ActiveSessions.Remove(HttpReq.SessionId) > 0)
				{
					SessionRateStates.Remove(HttpReq.SessionId);
					SessionProtocolVersions.Remove(HttpReq.SessionId);
					SessionPrincipals.Remove(HttpReq.SessionId);
					UE_LOG(LogMcpNativeTransport, Log,
						TEXT("Session terminated by client (remaining: %d)"),
						ActiveSessions.Num());
				}
			}
			CloseSessionConnections(HttpReq.SessionId);
		}
		SendAndClose(ClientSocket, 200, TEXT("text/plain"), FString(), {}, HttpReq.Origin);
		return;
	}

	// ── GET /mcp — persistent SSE notification stream ──
	if (HttpReq.Method == TEXT("GET"))
	{
		if (!HttpReq.Accept.Contains(TEXT("text/event-stream")))
		{
			SendAndClose(ClientSocket, 406, TEXT("text/plain"),
				TEXT("Not Acceptable: requires Accept: text/event-stream"), {}, HttpReq.Origin);
			return;
		}
		FString SessionError;
		ESessionValidationResult SessionStatus = ValidateSession(HttpReq.SessionId, SessionError);
		if (SessionStatus != ESessionValidationResult::Valid)
		{
			SendAndClose(ClientSocket, GetSessionValidationStatusCode(SessionStatus),
				TEXT("text/plain"), SessionError, {}, HttpReq.Origin);
			return;
		}
		if (!GuardProtocolVersionHeader(ClientSocket, HttpReq, nullptr, false)) return;
		HandleGetMcp(ClientSocket, HttpReq.SessionId, HttpReq.Origin);
		return;  // Socket parked — no close here
	}

	// ── POST /mcp — JSON-RPC ──
	if (HttpReq.Method != TEXT("POST"))
	{
		SendAndClose(ClientSocket, 405, TEXT("text/plain"), TEXT("Method Not Allowed"), {}, HttpReq.Origin);
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
		SendAndClose(ClientSocket, 400, TEXT("application/json"), ErrorBody, {}, HttpReq.Origin);
		return;
	}

	if (Rpc.bIsNotification && Rpc.Method == TEXT("initialize"))
	{
		SendAndClose(ClientSocket, 400, TEXT("application/json"), FMcpJsonRpc::BuildError(MakeShared<FJsonValueNull>(), FMcpJsonRpc::ErrorInvalidRequest, TEXT("initialize must include an id")), {}, HttpReq.Origin);
		return;
	}

	// Session validation (skip for initialize)
	if (Rpc.Method != TEXT("initialize"))
	{
		FString SessionError;
		ESessionValidationResult SessionStatus = ValidateSession(HttpReq.SessionId, SessionError);
		if (SessionStatus != ESessionValidationResult::Valid)
		{
			const FString ErrorBody = FMcpJsonRpc::BuildError(
				Rpc.Id, FMcpJsonRpc::ErrorInvalidRequest, SessionError);
			SendAndClose(ClientSocket, GetSessionValidationStatusCode(SessionStatus),
				TEXT("application/json"), ErrorBody, {}, HttpReq.Origin);
			return;
		}
		FString RateLimitError;
		if (!ConsumeSessionRequestBudget(
				HttpReq.SessionId, Rpc.Method == TEXT("tools/call"),
				RateLimitError))
		{
			const FString ErrorBody = FMcpJsonRpc::BuildError(
				Rpc.Id, FMcpJsonRpc::ErrorRateLimited, RateLimitError);
			SendAndClose(ClientSocket, 429, TEXT("application/json"), ErrorBody, {},
				HttpReq.Origin);
			return;
		}
	}

	// MCP-Protocol-Version is required post-initialize; initialize is exempt.
	if (Rpc.Method != TEXT("initialize") && !GuardProtocolVersionHeader(ClientSocket, HttpReq, Rpc.Id, true)) return;

	// Notifications (no id) — 202 Accepted after session validation.
	if (Rpc.bIsNotification)
	{
		UE_LOG(LogMcpNativeTransport, Log,
			TEXT("Received notification: %s"), *Rpc.Method);
		// Cancellation: route notifications/cancelled to the correlation handler.
		if (Rpc.Method == TEXT("notifications/cancelled")) HandleCancelledNotification(Rpc.Params, HttpReq.SessionId);
		SendAndClose(ClientSocket, 202, TEXT("text/plain"), FString(), {}, HttpReq.Origin);
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
			// FSocket::GetAddress returns void as of UE 5.8 (was bool in earlier
			// versions) and populates RemoteAddr unconditionally. A default-created
			// address is invalid until populated, so IsValid() preserves the
			// original "set remote addr only on success" behavior.
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
			SendAndClose(ClientSocket, 429, TEXT("application/json"), ResponseBody, {},
				HttpReq.Origin);
			return;
		}
		// Bind the principal for the life of the session; every later request on
		// this session is verified against it.
		BindSessionPrincipal(NewSessionId, HttpReq.CapabilityToken);
		TMap<FString, FString> Headers;
		Headers.Add(TEXT("Mcp-Session-Id"), NewSessionId);
		const bool bResponseSent = SendAndClose(
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
				SessionProtocolVersions.Remove(NewSessionId);
				SessionPrincipals.Remove(NewSessionId);
			}
			CloseSessionConnections(NewSessionId);
		}
		return;
	}

	if (Rpc.Method == TEXT("tools/list"))
	{
		FString ResponseBody = HandleToolsList(Rpc.Id, HttpReq.SessionId);
		SendAndClose(ClientSocket, 200, TEXT("application/json"), ResponseBody, {}, HttpReq.Origin);
		return;
	}

	if (Rpc.Method == TEXT("tools/call"))
	{
		// HandleToolsCall takes ownership of the socket (SSE streaming)
		HandleToolsCall(Rpc.Params, Rpc.Id, ClientSocket, HttpReq.SessionId, HttpReq.Origin);
		return;  // Socket NOT closed here — parked for SSE
	}

	// MCP primitive methods (resources/*, prompts/*, completion/complete). Routed
	// after tools/call and strictly before the method-not-found fallback so a
	// backed primitive is never reported as an unknown method.
	if (HandlePrimitiveMethod(Rpc.Method, Rpc.Params, Rpc.Id, ClientSocket, HttpReq.SessionId, HttpReq.Origin))
	{
		return;
	}

	// Task 44: tasks/get|list|cancel|result. Session-scoped inside the surface,
	// so one session can never read or cancel another session's task.
	{
		FString TaskBody;
		if (TaskSurface.HandleMethod(Rpc.Method, Rpc.Params, Rpc.Id, HttpReq.SessionId, TaskBody))
		{
			SendAndClose(ClientSocket, 200, TEXT("application/json"), TaskBody, {}, HttpReq.Origin);
			return;
		}
	}

	// Unknown method
	FString ErrorBody = FMcpJsonRpc::BuildError(
		Rpc.Id, FMcpJsonRpc::ErrorMethodNotFound,
		FString::Printf(TEXT("Unknown method: %s"), *Rpc.Method));
	SendAndClose(ClientSocket, 200, TEXT("application/json"), ErrorBody, {}, HttpReq.Origin);
}

// ─── HTTP Parsing ───────────────────────────────────────────────────────────
