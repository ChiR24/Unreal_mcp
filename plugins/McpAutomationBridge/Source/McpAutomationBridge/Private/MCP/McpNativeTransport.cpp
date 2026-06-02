#include "MCP/McpNativeTransport.h"
#include "MCP/McpJsonRpc.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpToolDefinition.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeSettings.h"
#include "Misc/Guid.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpNativeTransport, Log, All);

// ─── Lifecycle ──────────────────────────────────────────────────────────────

FMcpNativeTransport::FMcpNativeTransport(UMcpAutomationBridgeSubsystem* InSubsystem)
	: Subsystem(InSubsystem)
{
}

FMcpNativeTransport::~FMcpNativeTransport()
{
	Shutdown();
}

bool FMcpNativeTransport::Start(int32 Port, const FString& PluginDir, bool bLoadAllTools,
	const FString& InUserInstructions, const FString& InListenHost, bool bInAllowNonLoopback)
{
	if (Port <= 0 || Port > 65535)
	{
		UE_LOG(LogMcpNativeTransport, Error,
			TEXT("Invalid Native MCP port %d. Port must be between 1 and 65535."), Port);
		return false;
	}

	ListenPort = Port;
	UserInstructions = InUserInstructions;
	bAllowNonLoopback = bInAllowNonLoopback;

	// Validate listen host against loopback policy
	ListenHost = InListenHost.IsEmpty() ? TEXT("127.0.0.1") : InListenHost;
	if (ListenHost.Equals(TEXT("localhost"), ESearchCase::IgnoreCase))
	{
		ListenHost = TEXT("127.0.0.1");
	}
	else if (ListenHost == TEXT("[::1]"))
	{
		ListenHost = TEXT("::1");
	}

	bool bIsLoopback = ListenHost == TEXT("127.0.0.1") || ListenHost == TEXT("::1");
	if (!bIsLoopback && !bAllowNonLoopback)
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("ListenHost '%s' is not loopback and AllowNonLoopback is false — falling back to 127.0.0.1"),
			*ListenHost);
		ListenHost = TEXT("127.0.0.1");
	}
	else if (!bIsLoopback)
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("SECURITY: Binding to non-loopback address '%s'. Native MCP is exposed to your local network."),
			*ListenHost);
	}

	// Load server identity & instructions from server-info.json
	{
		FString ServerInfoPath = FPaths::Combine(PluginDir, TEXT("Resources/MCP/server-info.json"));
		FString JsonString;
		if (FFileHelper::LoadFileToString(JsonString, *ServerInfoPath))
		{
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				JsonObj->TryGetStringField(TEXT("name"), ServerName);
				JsonObj->TryGetStringField(TEXT("version"), ServerVersion);
				JsonObj->TryGetStringField(TEXT("instructions"), BaseInstructions);

				UE_LOG(LogMcpNativeTransport, Log,
					TEXT("Loaded server-info.json: %s v%s"), *ServerName, *ServerVersion);
			}
			else
			{
				UE_LOG(LogMcpNativeTransport, Warning,
					TEXT("Failed to parse server-info.json -- using defaults"));
			}
		}
		else
		{
			UE_LOG(LogMcpNativeTransport, Warning,
				TEXT("server-info.json not found at %s -- using defaults"), *ServerInfoPath);
		}
	}

	// Initialize dynamic tool manager from self-describing C++ tool registry
	{
		FMcpToolRegistry& Registry = FMcpToolRegistry::Get();
		UE_LOG(LogMcpNativeTransport, Log,
			TEXT("Tool registry: %d self-describing tools registered"), Registry.GetToolCount());
		ToolManager.Initialize(Registry, bLoadAllTools);
	}
	ToolManager.OnToolsChanged.BindRaw(this, &FMcpNativeTransport::OnToolsListChanged);

	// Create stop event and launch accept thread
	StopEvent = FPlatformProcess::GetSynchEventFromPool(true);
	BindCompleteEvent = FPlatformProcess::GetSynchEventFromPool(false);
	bStopping.store(false);
	bBindSuccess.store(false);

	Thread = FRunnableThread::Create(this, TEXT("McpNativeHTTPServer"), 0, TPri_Normal);
	if (!Thread)
	{
		UE_LOG(LogMcpNativeTransport, Error, TEXT("Failed to create HTTP server thread"));
		FPlatformProcess::ReturnSynchEventToPool(BindCompleteEvent);
		BindCompleteEvent = nullptr;
		FPlatformProcess::ReturnSynchEventToPool(StopEvent);
		StopEvent = nullptr;
		return false;
	}

	// Wait for the thread to complete bind/listen before reporting success
	BindCompleteEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(BindCompleteEvent);
	BindCompleteEvent = nullptr;

	if (!bBindSuccess.load())
	{
		UE_LOG(LogMcpNativeTransport, Error,
			TEXT("Failed to start native MCP server on %s:%d — bind/listen failed"), *ListenHost, Port);
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
		FPlatformProcess::ReturnSynchEventToPool(StopEvent);
		StopEvent = nullptr;
		return false;
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("Native MCP server started on http://%s:%d/mcp"), *ListenHost, Port);
	return true;
}

void FMcpNativeTransport::Stop()
{
	// FRunnable::Stop() — lightweight signal, called by Thread->Kill()
	bStopping.store(true);
	if (StopEvent)
	{
		StopEvent->Trigger();
	}
	// Close listen socket to unblock Accept()
	if (ListenSocket)
	{
		ListenSocket->Close();
	}
}

void FMcpNativeTransport::Shutdown()
{
	Stop();  // Signal the accept thread

	// Wait for accept thread to finish
	if (Thread)
	{
		Thread->Kill(true);  // Calls Stop() again (no-op — already signaled)
		delete Thread;
		Thread = nullptr;
	}

	// Wait for in-flight connection handlers and async writes to finish.
	// IMPORTANT: Shutdown runs on GameThread. HandleToolsCall dispatches
	// ProcessAutomationRequest → CompletePendingRequest to GameThread via
	// AsyncTask. If we block GameThread with a plain spin-wait, those tasks
	// can never execute and PendingAsyncWrites never reaches 0 → deadlock.
	// Solution: pump GameThread tasks while waiting so queued handlers drain.
	{
		double WaitStart = FPlatformTime::Seconds();
		constexpr double WarnAfter = 15.0;
		bool bWarned = false;
		while (ActiveConnectionCount.load() > 0 || PendingAsyncWrites.load() > 0)
		{
			if (IsInGameThread())
			{
				FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			}
			FPlatformProcess::Sleep(0.01f);
			double Elapsed = FPlatformTime::Seconds() - WaitStart;
			if (!bWarned && Elapsed > WarnAfter)
			{
				UE_LOG(LogMcpNativeTransport, Warning,
					TEXT("Shutdown stalled: %d active connections, %d pending async writes after %.0fs"),
					ActiveConnectionCount.load(), PendingAsyncWrites.load(), Elapsed);
				bWarned = true;
			}
		}
	}

	if (StopEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(StopEvent);
		StopEvent = nullptr;
	}

	// Close all active SSE connections with error.
	// WriteMutex is taken per-connection to synchronize with in-flight async writes.
	{
		ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		FScopeLock Lock(&SSEConnectionsMutex);
		for (auto& [RequestId, Conn] : SSEConnections)
		{
			if (Conn.IsValid())
			{
				FScopeLock WriteLock(&Conn->WriteMutex);
				if (Conn->Socket)
				{
					// Inline SSE write — we already hold WriteMutex
					FString ErrorJson = FMcpJsonRpc::BuildError(
						Conn->JsonRpcId, FMcpJsonRpc::ErrorInternalError,
						TEXT("Server shutting down"));
					FString Frame = FString::Printf(
						TEXT("event: message\ndata: %s\n\n"), *ErrorJson);
					FTCHARToUTF8 Utf8(*Frame);
					SendAllBytes(Conn->Socket,
						reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());

					Conn->Socket->Close();
					if (SocketSub)
					{
						SocketSub->DestroySocket(Conn->Socket);
					}
					Conn->Socket = nullptr;
				}
			}
		}
		SSEConnections.Empty();
	}

	// Close all persistent notification streams
	{
		FScopeLock Lock(&NotificationStreamsMutex);
		for (auto& [StreamId, Stream] : NotificationStreams)
		{
			if (Stream.IsValid())
			{
				CloseNotificationStream(Stream);
			}
		}
		NotificationStreams.Empty();
	}

	{
		FScopeLock Lock(&SessionMutex);
		ActiveSessions.Empty();
	}

	ListenSocket = nullptr;

	UE_LOG(LogMcpNativeTransport, Log, TEXT("Native MCP server stopped"));
}

// ─── Socket Helper ──────────────────────────────────────────────────────────

bool FMcpNativeTransport::SendAllBytes(FSocket* Socket, const uint8* Data, int32 Length)
{
	static constexpr double WriteTimeoutSeconds = 5.0;
	const double Deadline = FPlatformTime::Seconds() + WriteTimeoutSeconds;

	int32 TotalSent = 0;
	while (TotalSent < Length)
	{
		const double Remaining = Deadline - FPlatformTime::Seconds();
		if (Remaining <= 0.0)
		{
			return false;
		}
		if (!Socket->Wait(ESocketWaitConditions::WaitForWrite,
			FTimespan::FromSeconds(FMath::Min(Remaining, 1.0))))
		{
			return false;
		}
		int32 BytesSent = 0;
		if (!Socket->Send(Data + TotalSent, Length - TotalSent, BytesSent))
		{
			return false;
		}
		if (BytesSent <= 0)
		{
			return false;
		}
		TotalSent += BytesSent;
	}
	return true;
}

// ─── Accept Loop (FRunnable::Run) ───────────────────────────────────────────

uint32 FMcpNativeTransport::Run()
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSub)
	{
		UE_LOG(LogMcpNativeTransport, Error, TEXT("Failed to get socket subsystem"));
		bBindSuccess.store(false);
		if (BindCompleteEvent) BindCompleteEvent->Trigger();
		return 1;
	}

	ListenSocket = SocketSub->CreateSocket(NAME_Stream,
		TEXT("McpNativeHTTPListenSocket"), FName());
	if (!ListenSocket)
	{
		UE_LOG(LogMcpNativeTransport, Error, TEXT("Failed to create listen socket"));
		bBindSuccess.store(false);
		if (BindCompleteEvent) BindCompleteEvent->Trigger();
		return 1;
	}

	ListenSocket->SetReuseAddr(true);
	ListenSocket->SetNonBlocking(false);

	TSharedRef<FInternetAddr> BindAddr = SocketSub->CreateInternetAddr();
	bool bIsValid = false;
	BindAddr->SetIp(*ListenHost, bIsValid);
	BindAddr->SetPort(ListenPort);

	if (!bIsValid)
	{
		UE_LOG(LogMcpNativeTransport, Error,
			TEXT("Invalid listen host: %s — falling back to 127.0.0.1"), *ListenHost);
		BindAddr->SetIp(TEXT("127.0.0.1"), bIsValid);
	}

	if (!ListenSocket->Bind(*BindAddr))
	{
		UE_LOG(LogMcpNativeTransport, Error,
			TEXT("Failed to bind to %s:%d"), *ListenHost, ListenPort);
		SocketSub->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		bBindSuccess.store(false);
		if (BindCompleteEvent) BindCompleteEvent->Trigger();
		return 1;
	}

	if (!ListenSocket->Listen(5))
	{
		UE_LOG(LogMcpNativeTransport, Error, TEXT("Failed to listen on socket"));
		SocketSub->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		bBindSuccess.store(false);
		if (BindCompleteEvent) BindCompleteEvent->Trigger();
		return 1;
	}

	// Signal Start() that bind/listen succeeded
	bBindSuccess.store(true);
	if (BindCompleteEvent) BindCompleteEvent->Trigger();

	UE_LOG(LogMcpNativeTransport, Verbose,
		TEXT("Accept loop started on port %d"), ListenPort);

	while (!bStopping.load())
	{
		FSocket* ClientSocket = ListenSocket->Accept(TEXT("McpNativeHTTPClient"));

		if (bStopping.load() || !ListenSocket)
		{
			if (ClientSocket)
			{
				ClientSocket->Close();
				SocketSub->DestroySocket(ClientSocket);
			}
			break;
		}

		if (ClientSocket)
		{
			ClientSocket->SetNoDelay(true);
			int32 Count = ActiveConnectionCount.fetch_add(1);
			if (Count >= MaxConcurrentConnections)
			{
				ActiveConnectionCount.fetch_sub(1);
				SendHttpResponse(ClientSocket, 503, TEXT("text/plain"), TEXT("Service Unavailable"));
				ClientSocket->Close();
				SocketSub->DestroySocket(ClientSocket);
			}
			else
			{
				// Lifetime safety: ActiveConnectionCount is incremented before dispatch,
				// and Shutdown waits for it to drain before destroying this transport.
				FMcpNativeTransport* Transport = this;
				Async(EAsyncExecution::ThreadPool, [Transport, ClientSocket]()
				{
					Transport->HandleConnection(ClientSocket);
					Transport->ActiveConnectionCount.fetch_sub(1);
				});
			}
		}
		else
		{
			// Accept failed (transient) — brief sleep before retrying
			FPlatformProcess::Sleep(0.01f);
		}
	}

	// Cleanup listen socket
	if (ListenSocket)
	{
		ListenSocket->Close();
		SocketSub->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
	}

	UE_LOG(LogMcpNativeTransport, Verbose, TEXT("Accept loop exited"));
	return 0;
}

// ─── Connection Handler ─────────────────────────────────────────────────────

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
					UE_LOG(LogMcpNativeTransport, Log,
						TEXT("Session %s terminated by client (remaining: %d)"),
						*HttpReq.SessionId, ActiveSessions.Num());
				}
			}

			// Close notification streams belonging to this session
			TArray<FString> StreamsToClose;
			{
				FScopeLock Lock(&NotificationStreamsMutex);
				for (const auto& [StreamId, Stream] : NotificationStreams)
				{
					if (Stream.IsValid() && Stream->SessionId == HttpReq.SessionId)
					{
						StreamsToClose.Add(StreamId);
					}
				}
			}
			for (const FString& StreamId : StreamsToClose)
			{
				TSharedPtr<FNotificationStream> Stream;
				{
					FScopeLock Lock(&NotificationStreamsMutex);
					NotificationStreams.RemoveAndCopyValue(StreamId, Stream);
				}
				CloseNotificationStream(Stream);
				UE_LOG(LogMcpNativeTransport, Log,
					TEXT("Closed notification stream %s (session terminated)"), *StreamId);
			}
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
		FString ResponseBody = HandleInitialize(Rpc.Params, Rpc.Id, NewSessionId);
		TMap<FString, FString> Headers;
		Headers.Add(TEXT("Mcp-Session-Id"), NewSessionId);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ResponseBody, Headers, HttpReq.Origin);
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

bool FMcpNativeTransport::ReadHttpRequest(FSocket* Socket, FParsedHttpRequest& OutRequest)
{
	// Read headers (up to 8KB)
	static constexpr int32 MaxHeaderSize = 8192;
	TArray<uint8> HeaderBuf;
	HeaderBuf.Reserve(MaxHeaderSize);

	const double Deadline = FPlatformTime::Seconds() + 5.0;  // 5s read timeout

	while (HeaderBuf.Num() < MaxHeaderSize)
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP header read timeout"));
			return false;
		}

		uint32 PendingSize = 0;
		if (!Socket->HasPendingData(PendingSize))
		{
			FPlatformProcess::Sleep(0.001f);
			continue;
		}

		uint8 Byte;
		int32 BytesRead = 0;
		if (!Socket->Recv(&Byte, 1, BytesRead) || BytesRead <= 0)
		{
			FPlatformProcess::Sleep(0.001f);
			continue;
		}

		HeaderBuf.Add(Byte);

		// Check for \r\n\r\n
		const int32 Len = HeaderBuf.Num();
		if (Len >= 4
			&& HeaderBuf[Len - 4] == '\r'
			&& HeaderBuf[Len - 3] == '\n'
			&& HeaderBuf[Len - 2] == '\r'
			&& HeaderBuf[Len - 1] == '\n')
		{
			break;
		}
	}

	if (HeaderBuf.Num() >= MaxHeaderSize)
	{
		UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP headers too large"));
		return false;
	}

	// Parse headers
	FUTF8ToTCHAR HeaderConverter(reinterpret_cast<const ANSICHAR*>(HeaderBuf.GetData()), HeaderBuf.Num());
	FString HeaderStr(HeaderConverter.Length(), HeaderConverter.Get());

	TArray<FString> Lines;
	HeaderStr.ParseIntoArray(Lines, TEXT("\r\n"));
	if (Lines.Num() == 0)
	{
		return false;
	}

	// Request line: "POST /mcp HTTP/1.1"
	TArray<FString> RequestParts;
	Lines[0].ParseIntoArrayWS(RequestParts);
	if (RequestParts.Num() < 2)
	{
		return false;
	}
	OutRequest.Method = RequestParts[0];
	OutRequest.Path = RequestParts[1];

	// Parse headers
	OutRequest.ContentLength = 0;
	bool bHasContentLength = false;
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		FString Key, Value;
		if (Lines[i].Split(TEXT(":"), &Key, &Value))
		{
			Key.TrimStartAndEndInline();
			Value.TrimStartAndEndInline();

			if (Key.Equals(TEXT("Content-Length"), ESearchCase::IgnoreCase))
			{
				if (bHasContentLength)
				{
					UE_LOG(LogMcpNativeTransport, Warning, TEXT("Duplicate Content-Length header"));
					return false;
				}

				int32 ParsedContentLength = 0;
				if (!LexTryParseString(ParsedContentLength, *Value))
				{
					UE_LOG(LogMcpNativeTransport, Warning,
						TEXT("Invalid Content-Length header: %s"), *Value);
					return false;
				}

				OutRequest.ContentLength = ParsedContentLength;
				bHasContentLength = true;
			}
			else if (Key.Equals(TEXT("Mcp-Session-Id"), ESearchCase::IgnoreCase))
			{
				OutRequest.SessionId = Value;
			}
			else if (Key.Equals(TEXT("Accept"), ESearchCase::IgnoreCase))
			{
				OutRequest.Accept = Value;
			}
			else if (Key.Equals(TEXT("X-MCP-Capability-Token"), ESearchCase::IgnoreCase))
			{
				OutRequest.CapabilityToken = Value;
			}
			else if (Key.Equals(TEXT("Origin"), ESearchCase::IgnoreCase))
			{
				OutRequest.Origin = Value;
			}
		}
	}

	// Read body
	static constexpr int32 MaxBodySize = 5 * 1024 * 1024;  // 5MB
	if (OutRequest.ContentLength < 0 || OutRequest.ContentLength > MaxBodySize)
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("Invalid HTTP body size: %d bytes"), OutRequest.ContentLength);
		return false;
	}

	if (OutRequest.ContentLength > 0)
	{
		TArray<uint8> BodyBuf;
		BodyBuf.SetNumUninitialized(OutRequest.ContentLength);
		int32 TotalRead = 0;
		uint32 PendingData = 0;

		while (TotalRead < OutRequest.ContentLength)
		{
			if (FPlatformTime::Seconds() > Deadline)
			{
				UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP body read timeout"));
				return false;
			}

			int32 BytesRead = 0;
			if (Socket->Recv(BodyBuf.GetData() + TotalRead,
				OutRequest.ContentLength - TotalRead, BytesRead))
			{
				if (BytesRead > 0)
				{
					TotalRead += BytesRead;
				}
				else if (!Socket->HasPendingData(PendingData))
				{
					UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP body read: peer closed connection (read %d/%d)"), TotalRead, OutRequest.ContentLength);
					return false;
				}
				else
				{
					FPlatformProcess::Sleep(0.001f);
				}
			}
			else
			{
				FPlatformProcess::Sleep(0.001f);
			}
		}

		FUTF8ToTCHAR BodyConverter(reinterpret_cast<const ANSICHAR*>(BodyBuf.GetData()), TotalRead);
		OutRequest.Body = FString(BodyConverter.Length(), BodyConverter.Get());
	}

	return true;
}

// ─── HTTP Response Helpers ──────────────────────────────────────────────────

bool FMcpNativeTransport::SendHttpResponse(FSocket* Socket, int32 StatusCode,
	const FString& ContentType, const FString& Body,
	const TMap<FString, FString>& ExtraHeaders,
	const FString& CorsOrigin)
{
	FString StatusText;
	switch (StatusCode)
	{
	case 200: StatusText = TEXT("OK"); break;
	case 202: StatusText = TEXT("Accepted"); break;
	case 204: StatusText = TEXT("No Content"); break;
	case 400: StatusText = TEXT("Bad Request"); break;
	case 403: StatusText = TEXT("Forbidden"); break;
	case 404: StatusText = TEXT("Not Found"); break;
	case 405: StatusText = TEXT("Method Not Allowed"); break;
	case 401: StatusText = TEXT("Unauthorized"); break;
	case 406: StatusText = TEXT("Not Acceptable"); break;
	case 429: StatusText = TEXT("Too Many Requests"); break;
	case 500: StatusText = TEXT("Internal Server Error"); break;
	case 503: StatusText = TEXT("Service Unavailable"); break;
	default:  StatusText = TEXT("Unknown"); break;
	}

	FTCHARToUTF8 BodyUtf8(*Body);
	const int32 BodyLength = BodyUtf8.Length();

	FString Response = FString::Printf(
		TEXT("HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n"),
		StatusCode, *StatusText, *ContentType, BodyLength);
	AppendCorsHeaders(Response, CorsOrigin);

	for (const auto& [Key, Value] : ExtraHeaders)
	{
		Response += FString::Printf(TEXT("%s: %s\r\n"), *Key, *Value);
	}
	Response += TEXT("\r\n");

	// Send header + body
	FTCHARToUTF8 HeaderUtf8(*Response);
	if (!SendAllBytes(Socket, reinterpret_cast<const uint8*>(HeaderUtf8.Get()),
		HeaderUtf8.Length()))
	{
		return false;
	}

	if (BodyLength > 0)
	{
		if (!SendAllBytes(Socket, reinterpret_cast<const uint8*>(BodyUtf8.Get()),
			BodyLength))
		{
			return false;
		}
	}

	return true;
}

bool FMcpNativeTransport::SendSSEHeaders(FSocket* Socket, const FString& SessionId,
	const FString& CorsOrigin)
{
	FString Headers = FString::Printf(
		TEXT("HTTP/1.1 200 OK\r\n")
		TEXT("Content-Type: text/event-stream\r\n")
		TEXT("Cache-Control: no-cache\r\n")
		TEXT("Connection: keep-alive\r\n")
		TEXT("Mcp-Session-Id: %s\r\n"),
		*SessionId);
	AppendCorsHeaders(Headers, CorsOrigin);
	Headers += TEXT("\r\n");

	FTCHARToUTF8 Utf8(*Headers);
	return SendAllBytes(Socket, reinterpret_cast<const uint8*>(Utf8.Get()),
		Utf8.Length());
}

bool FMcpNativeTransport::IsCorsEnabled() const
{
	const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
	return (ListenHost == TEXT("127.0.0.1") || ListenHost == TEXT("::1"))
		&& Settings
		&& Settings->bRequireCapabilityToken
		&& !Settings->CapabilityToken.IsEmpty();
}

bool FMcpNativeTransport::IsAllowedCorsOrigin(const FString& Origin) const
{
	if (!IsCorsEnabled())
	{
		return false;
	}

	const FString TrimmedOrigin = Origin.TrimStartAndEnd();
	if (TrimmedOrigin.IsEmpty() || TrimmedOrigin.Equals(TEXT("null"), ESearchCase::IgnoreCase) ||
		TrimmedOrigin.Contains(TEXT("\r")) || TrimmedOrigin.Contains(TEXT("\n")))
	{
		return false;
	}

	FString Scheme;
	FString Remainder;
	if (!TrimmedOrigin.Split(TEXT("://"), &Scheme, &Remainder))
	{
		return false;
	}
	if (!Scheme.Equals(TEXT("http"), ESearchCase::IgnoreCase) &&
		!Scheme.Equals(TEXT("https"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	if (Remainder.IsEmpty() || Remainder.Contains(TEXT("/")))
	{
		return false;
	}

	auto IsDigitsOnly = [](const FString& Value) -> bool
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (Character < '0' || Character > '9')
			{
				return false;
			}
		}
		return true;
	};
	auto IsValidPort = [&IsDigitsOnly](const FString& PortText) -> bool
	{
		if (!IsDigitsOnly(PortText))
		{
			return false;
		}

		int32 PortNumber = 0;
		if (!LexTryParseString(PortNumber, *PortText))
		{
			return false;
		}
		return PortNumber > 0 && PortNumber <= 65535;
	};

	FString Host = Remainder;
	if (Host.StartsWith(TEXT("[")))
	{
		int32 EndBracket = INDEX_NONE;
		if (!Host.FindChar(TEXT(']'), EndBracket) || EndBracket <= 1)
		{
			return false;
		}

		const FString PortSuffix = Host.Mid(EndBracket + 1);
		if (!PortSuffix.IsEmpty())
		{
			if (!PortSuffix.StartsWith(TEXT(":")) || !IsValidPort(PortSuffix.Mid(1)))
			{
				return false;
			}
		}

		const FString BracketedHost = Host.Mid(1, EndBracket - 1);
		if (BracketedHost != TEXT("::1"))
		{
			return false;
		}
		Host = BracketedHost;
	}
	else
	{
		FString HostOnly;
		FString Port;
		if (Host.Split(TEXT(":"), &HostOnly, &Port))
		{
			if (!IsValidPort(Port))
			{
				return false;
			}
			Host = HostOnly;
		}
	}

	return Host.Equals(TEXT("localhost"), ESearchCase::IgnoreCase) ||
		Host == TEXT("127.0.0.1") ||
		Host == TEXT("::1");
}

void FMcpNativeTransport::AppendCorsHeaders(FString& Response, const FString& Origin) const
{
	if (!IsAllowedCorsOrigin(Origin))
	{
		return;
	}

	Response += FString::Printf(TEXT("Access-Control-Allow-Origin: %s\r\n"), *Origin.TrimStartAndEnd());
	Response += TEXT("Vary: Origin\r\n");
	Response += TEXT("Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n");
	Response += TEXT("Access-Control-Allow-Headers: Content-Type, Accept, Mcp-Session-Id, MCP-Protocol-Version, X-MCP-Capability-Token\r\n");
	Response += TEXT("Access-Control-Expose-Headers: Mcp-Session-Id, MCP-Protocol-Version\r\n");
}

bool FMcpNativeTransport::WriteSSEEvent(FSSEConnection& Conn, const FString& EventData)
{
	FString Frame = FString::Printf(
		TEXT("event: message\ndata: %s\n\n"), *EventData);

	FTCHARToUTF8 Utf8(*Frame);

	FScopeLock Lock(&Conn.WriteMutex);
	if (!Conn.Socket)
	{
		return false;
	}
	return SendAllBytes(Conn.Socket, reinterpret_cast<const uint8*>(Utf8.Get()),
		Utf8.Length());
}

// ─── Persistent Notification Streams (GET /mcp) ────────────────────────────

void FMcpNativeTransport::HandleGetMcp(FSocket* ClientSocket, const FString& SessionId,
	const FString& CorsOrigin)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	// Check per-session stream limit
	{
		FScopeLock Lock(&NotificationStreamsMutex);
		int32 Count = 0;
		for (const auto& [Id, Stream] : NotificationStreams)
		{
			if (Stream.IsValid() && Stream->SessionId == SessionId
				&& !Stream->bMarkedForRemoval.load())
			{
				++Count;
			}
		}
		if (Count >= MaxNotificationStreamsPerSession)
		{
			Lock.Unlock();
			SendHttpResponse(ClientSocket, 429, TEXT("text/plain"),
				FString::Printf(TEXT("Too Many Requests: max %d notification streams per session"),
					MaxNotificationStreamsPerSession), {}, CorsOrigin);
			ClientSocket->Close();
			SocketSub->DestroySocket(ClientSocket);
			return;
		}
	}

	// Send SSE headers
	if (!SendSSEHeaders(ClientSocket, SessionId, CorsOrigin))
	{
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Park socket as notification stream
	const double Now = FPlatformTime::Seconds();
	TSharedPtr<FNotificationStream> Stream = MakeShared<FNotificationStream>();
	Stream->Socket = ClientSocket;
	Stream->SessionId = SessionId;
	Stream->StreamId = FGuid::NewGuid().ToString();
	Stream->StartTime = Now;
	Stream->LastKeepaliveTime = Now;

	{
		FScopeLock Lock(&NotificationStreamsMutex);
		NotificationStreams.Add(Stream->StreamId, Stream);
	}
	TouchSession(SessionId);

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("GET /mcp: notification stream %s opened for session %s"),
		*Stream->StreamId, *SessionId);
	// Socket is parked — do NOT close it. Thread pool slot is released.
}

bool FMcpNativeTransport::WriteNotificationEvent(FNotificationStream& Stream, const FString& EventData)
{
	FString Frame = FString::Printf(TEXT("event: message\ndata: %s\n\n"), *EventData);
	FTCHARToUTF8 Utf8(*Frame);

	FScopeLock Lock(&Stream.WriteMutex);
	if (!Stream.Socket)
	{
		return false;
	}
	return SendAllBytes(Stream.Socket, reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
}

bool FMcpNativeTransport::WriteNotificationKeepalive(FNotificationStream& Stream)
{
	static const char* KeepaliveFrame = ":keepalive\n\n";
	static const int32 KeepaliveLen = FCStringAnsi::Strlen(KeepaliveFrame);

	FScopeLock Lock(&Stream.WriteMutex);
	if (!Stream.Socket)
	{
		return false;
	}
	return SendAllBytes(Stream.Socket, reinterpret_cast<const uint8*>(KeepaliveFrame), KeepaliveLen);
}

void FMcpNativeTransport::CloseNotificationStream(TSharedPtr<FNotificationStream> Stream)
{
	if (!Stream.IsValid())
	{
		return;
	}
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	FScopeLock Lock(&Stream->WriteMutex);
	if (Stream->Socket)
	{
		Stream->Socket->Close();
		if (SocketSub)
		{
			SocketSub->DestroySocket(Stream->Socket);
		}
		Stream->Socket = nullptr;
	}
}

// ─── Initialize ─────────────────────────────────────────────────────────────

FString FMcpNativeTransport::HandleInitialize(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FString& OutSessionId)
{
	// Extract client info for logging
	FString ClientName = TEXT("unknown");
	FString ClientVersion = TEXT("unknown");
	if (Params.IsValid())
	{
		const TSharedPtr<FJsonObject>* ClientInfoObj = nullptr;
		if (Params->TryGetObjectField(TEXT("clientInfo"), ClientInfoObj) && ClientInfoObj)
		{
			(*ClientInfoObj)->TryGetStringField(TEXT("name"), ClientName);
			(*ClientInfoObj)->TryGetStringField(TEXT("version"), ClientVersion);
		}
	}

	OutSessionId = FGuid::NewGuid().ToString();
	int32 CurrentSessionCount;
	{
		FScopeLock Lock(&SessionMutex);
		ActiveSessions.Add(OutSessionId, FPlatformTime::Seconds());
		CurrentSessionCount = ActiveSessions.Num();
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("protocolVersion"), TEXT("2025-03-26"));

	auto Capabilities = MakeShared<FJsonObject>();
	auto ToolsCap = MakeShared<FJsonObject>();
	ToolsCap->SetBoolField(TEXT("listChanged"), true);
	Capabilities->SetObjectField(TEXT("tools"), ToolsCap);
	Result->SetObjectField(TEXT("capabilities"), Capabilities);

	auto ServerInfo = MakeShared<FJsonObject>();
	ServerInfo->SetStringField(TEXT("name"), ServerName);
	ServerInfo->SetStringField(TEXT("version"), ServerVersion);
	Result->SetObjectField(TEXT("serverInfo"), ServerInfo);

	// Combine base instructions (from server-info.json) + user instructions (from settings)
	FString CombinedInstructions = BaseInstructions;
	if (!UserInstructions.IsEmpty())
	{
		if (!CombinedInstructions.IsEmpty())
		{
			CombinedInstructions += TEXT("\n\n");
		}
		CombinedInstructions += UserInstructions;
	}
	if (!CombinedInstructions.IsEmpty())
	{
		Result->SetStringField(TEXT("instructions"), CombinedInstructions);
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("MCP session initialized: %s (client: %s %s, active sessions: %d)"),
		*OutSessionId, *ClientName, *ClientVersion, CurrentSessionCount);

	return FMcpJsonRpc::BuildResponse(Id, Result);
}

// ─── Tools List ─────────────────────────────────────────────────────────────

FString FMcpNativeTransport::HandleToolsList(const TSharedPtr<FJsonValue>& Id)
{
	TSet<FString> EnabledTools = ToolManager.GetEnabledToolNames();
	TSharedPtr<FJsonObject> ToolsList = FMcpToolRegistry::Get().GetFilteredToolsResponse(EnabledTools);

	if (ToolsList.IsValid())
	{
		return FMcpJsonRpc::BuildResponse(Id, ToolsList);
	}

	auto EmptyResult = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> EmptyArray;
	EmptyResult->SetArrayField(TEXT("tools"), EmptyArray);
	return FMcpJsonRpc::BuildResponse(Id, EmptyResult);
}

int32 FMcpNativeTransport::GetTotalToolCount() const
{
	return FMcpToolRegistry::Get().GetToolCount();
}

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

	// Intercept manage_tools — handle locally (one-shot, no SSE)
	if (ToolName == TEXT("manage_tools"))
	{
		FString Action;
		Arguments->TryGetStringField(TEXT("action"), Action);
		TSharedPtr<FJsonObject> Result = ToolManager.HandleAction(Action, Arguments);
		bool bActionSuccess = false;
		if (Result.IsValid()) { Result->TryGetBoolField(TEXT("success"), bActionSuccess); }
		FString ActionMessage = TEXT("OK");
		if (!bActionSuccess && Result.IsValid())
		{
			Result->TryGetStringField(TEXT("error"), ActionMessage);
		}
		TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
			bActionSuccess, ActionMessage, Result);
		FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
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

	// Send SSE headers — begins the streaming response
	if (!SendSSEHeaders(ClientSocket, SessionId, CorsOrigin))
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("Failed to send SSE headers for tool %s"), *ToolName);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Generate request ID and park the connection
	const FString RequestId = FGuid::NewGuid().ToString();

	{
		FScopeLock Lock(&SSEConnectionsMutex);
		TSharedPtr<FSSEConnection> Conn = MakeShared<FSSEConnection>();
		Conn->Socket = ClientSocket;
		Conn->JsonRpcId = Id;
		Conn->StartTime = FPlatformTime::Seconds();
		Conn->ToolName = ToolName;
		Conn->SessionId = SessionId;
		SSEConnections.Add(RequestId, Conn);
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("tools/call: %s (RequestId=%s)"),
		*ToolName, *RequestId);

	// Resolve dispatch action using tool definition metadata.
	// Pattern A: pass tool name as Action (handler checks Action == "tool_name")
	// Pattern B: extract sub-action from arguments (handler checks Action.StartsWith("sub_action"))
	FString DispatchAction = ToolName;
	FMcpToolDefinition* ToolDef = FMcpToolRegistry::Get().FindTool(ToolName);
	if (ToolDef && !ToolDef->UsesToolNameDispatch())
	{
		// Pattern B: extract action from arguments
		FString ActionField = ToolDef->GetActionFieldName();
		FString Extracted;
		if (Arguments->TryGetStringField(ActionField, Extracted) && !Extracted.IsEmpty())
		{
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

	if (UMcpAutomationBridgeSubsystem* Sub = WeakSubsystem.Get())
	{
		Sub->QueueAutomationRequest(
			CapturedRequestId, CapturedDispatchAction, CapturedArguments, nullptr,
			ERequestOrigin::NativeHTTP);
	}
}

// ─── SSE Connection Management ──────────────────────────────────────────────

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

void FMcpNativeTransport::CleanupStaleRequests()
{
	const double Now = FPlatformTime::Seconds();

	// Clean up timed-out SSE connections
	TArray<FString> Expired;
	{
		FScopeLock Lock(&SSEConnectionsMutex);
		for (const auto& [RequestId, Conn] : SSEConnections)
		{
			if (Conn.IsValid() && (Now - Conn->StartTime > RequestTimeoutSeconds
				|| Conn->bMarkedForRemoval.load()))
			{
				Expired.Add(RequestId);
			}
		}
	}

	for (const FString& RequestId : Expired)
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("SSE request %s timed out after %.0f seconds"),
			*RequestId, RequestTimeoutSeconds);
		CompletePendingRequest(RequestId, false, TEXT("Request timed out"),
			nullptr, TEXT("TIMEOUT"));
	}

	// Clean up inactive sessions
	{
		FScopeLock Lock(&SessionMutex);
		TArray<FString> ExpiredSessions;
		for (const auto& [SessionId, LastActivity] : ActiveSessions)
		{
			if (Now - LastActivity > SessionTimeoutSeconds)
			{
				ExpiredSessions.Add(SessionId);
			}
		}
		for (const FString& SessionId : ExpiredSessions)
		{
			ActiveSessions.Remove(SessionId);
			UE_LOG(LogMcpNativeTransport, Log,
				TEXT("Session %s expired after %.0f min inactivity (remaining: %d)"),
				*SessionId, SessionTimeoutSeconds / 60.0, ActiveSessions.Num());
		}
	}

	// Clean up notification streams: expired, orphaned sessions, keepalive
	{
		// 1. Snapshot stream IDs + session IDs under lock
		TArray<TPair<FString, FString>> StreamSessions;  // StreamId, SessionId
		TArray<FString> MarkedForRemoval;
		{
			FScopeLock Lock(&NotificationStreamsMutex);
			for (const auto& [StreamId, Stream] : NotificationStreams)
			{
				if (!Stream.IsValid())
				{
					continue;
				}
				if (Stream->bMarkedForRemoval.load()
					|| Now - Stream->StartTime > NotificationStreamTimeoutSeconds)
				{
					MarkedForRemoval.Add(StreamId);
				}
				else
				{
					StreamSessions.Emplace(StreamId, Stream->SessionId);
				}
			}
		}

		// 2. Check session validity (separate lock — no nesting)
		{
			FScopeLock Lock(&SessionMutex);
			for (const auto& [StreamId, SessionId] : StreamSessions)
			{
				if (!ActiveSessions.Contains(SessionId))
				{
					MarkedForRemoval.Add(StreamId);
				}
			}
		}

		// 3. Remove dead streams
		for (const FString& StreamId : MarkedForRemoval)
		{
			TSharedPtr<FNotificationStream> Stream;
			{
				FScopeLock Lock(&NotificationStreamsMutex);
				NotificationStreams.RemoveAndCopyValue(StreamId, Stream);
			}
			if (Stream.IsValid())
			{
				CloseNotificationStream(Stream);
				UE_LOG(LogMcpNativeTransport, Log,
					TEXT("Notification stream %s closed (session=%s)"),
					*StreamId, *Stream->SessionId);
			}
		}

		// 4. Keepalive for living streams
		TArray<TSharedPtr<FNotificationStream>> AliveSnapshot;
		{
			FScopeLock Lock(&NotificationStreamsMutex);
			for (auto& [StreamId, Stream] : NotificationStreams)
			{
				if (Stream.IsValid() && !Stream->bMarkedForRemoval.load()
					&& Now - Stream->LastKeepaliveTime >= KeepaliveIntervalSeconds)
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
}

// ─── Session Validation ─────────────────────────────────────────────────────

FMcpNativeTransport::ESessionValidationResult FMcpNativeTransport::ValidateSession(
	const FString& SessionId, FString& OutError)
{
	if (SessionId.IsEmpty())
	{
		OutError = TEXT("Missing Mcp-Session-Id header");
		return ESessionValidationResult::Missing;
	}

	FScopeLock Lock(&SessionMutex);
	double* LastActivity = ActiveSessions.Find(SessionId);
	if (!LastActivity)
	{
		OutError = TEXT("Invalid or expired session ID");
		return ESessionValidationResult::Invalid;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - *LastActivity > SessionTimeoutSeconds)
	{
		ActiveSessions.Remove(SessionId);
		OutError = TEXT("Invalid or expired session ID");
		return ESessionValidationResult::Invalid;
	}

	// Touch session activity
	*LastActivity = Now;
	return ESessionValidationResult::Valid;
}

int32 FMcpNativeTransport::GetSessionValidationStatusCode(ESessionValidationResult Result)
{
	switch (Result)
	{
	case ESessionValidationResult::Missing:
		return 400;
	case ESessionValidationResult::Invalid:
		return 404;
	case ESessionValidationResult::Valid:
	default:
		return 200;
	}
}

void FMcpNativeTransport::TouchSession(const FString& SessionId)
{
	if (SessionId.IsEmpty())
	{
		return;
	}
	FScopeLock Lock(&SessionMutex);
	double* LastActivity = ActiveSessions.Find(SessionId);
	if (LastActivity)
	{
		*LastActivity = FPlatformTime::Seconds();
	}
}

// ─── Helpers ────────────────────────────────────────────────────────────────

void FMcpNativeTransport::OnToolsListChanged()
{
	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("Tool list changed — broadcasting notifications/tools/list_changed"));
	BroadcastToolsListChanged();
}

void FMcpNativeTransport::BroadcastToolsListChanged()
{
	FString NotificationJson = FMcpJsonRpc::BuildNotification(
		TEXT("notifications/tools/list_changed"));

	// Broadcast only to persistent notification streams (GET /mcp). Per-request
	// tools/call streams must contain only progress and the final response.
	TArray<TSharedPtr<FNotificationStream>> NotifSnapshot;
	{
		FScopeLock Lock(&NotificationStreamsMutex);
		NotifSnapshot.Reserve(NotificationStreams.Num());
		for (auto& [StreamId, Stream] : NotificationStreams)
		{
			if (Stream.IsValid() && !Stream->bMarkedForRemoval.load())
			{
				NotifSnapshot.Add(Stream);
			}
		}
	}

	for (const auto& Stream : NotifSnapshot)
	{
		if (!WriteNotificationEvent(*Stream, NotificationJson))
		{
			Stream->bMarkedForRemoval.store(true);
			UE_LOG(LogMcpNativeTransport, Warning,
				TEXT("Failed to send list_changed to notification stream %s — marking for removal"),
				*Stream->StreamId);
		}
		else
		{
			TouchSession(Stream->SessionId);
		}
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("Broadcast list_changed to %d notification stream(s)"),
		NotifSnapshot.Num());
}

int32 FMcpNativeTransport::GetActiveSessionCount() const
{
	FScopeLock Lock(&SessionMutex);
	return ActiveSessions.Num();
}
