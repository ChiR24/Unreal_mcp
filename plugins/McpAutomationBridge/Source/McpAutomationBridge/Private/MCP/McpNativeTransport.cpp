#include "MCP/McpNativeTransport.h"
#include "MCP/McpJsonRpc.h"
#include "McpAutomationBridgeSubsystem.h"
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
	ListenPort = Port;
	UserInstructions = InUserInstructions;
	bAllowNonLoopback = bInAllowNonLoopback;

	// Validate listen host against loopback policy
	ListenHost = InListenHost.IsEmpty() ? TEXT("127.0.0.1") : InListenHost;
	bool bIsLoopback = ListenHost == TEXT("127.0.0.1") || ListenHost == TEXT("localhost")
		|| ListenHost == TEXT("::1");
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

	// Load tool schemas
	if (!SchemaLoader.LoadFromFile(PluginDir))
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("Tool schemas not loaded — tools/list will return empty. Continuing startup."));
	}

	// Initialize dynamic tool manager
	ToolManager.Initialize(SchemaLoader, bLoadAllTools);
	ToolManager.OnToolsChanged.BindRaw(this, &FMcpNativeTransport::OnToolsListChanged);

	// Create stop event and launch accept thread
	StopEvent = FPlatformProcess::GetSynchEventFromPool(true);
	bStopping.store(false);

	Thread = FRunnableThread::Create(this, TEXT("McpNativeHTTPServer"), 0, TPri_Normal);
	if (!Thread)
	{
		UE_LOG(LogMcpNativeTransport, Error, TEXT("Failed to create HTTP server thread"));
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

	if (StopEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(StopEvent);
		StopEvent = nullptr;
	}

	// Close all active SSE connections with error
	{
		ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		FScopeLock Lock(&SSEConnectionsMutex);
		for (auto& [RequestId, Conn] : SSEConnections)
		{
			if (Conn.IsValid() && Conn->Socket)
			{
				// Send error as final SSE event before closing
				FString ErrorJson = FMcpJsonRpc::BuildError(
					Conn->JsonRpcId, FMcpJsonRpc::ErrorInternalError,
					TEXT("Server shutting down"));
				WriteSSEEvent(Conn->Socket, ErrorJson, Conn->WriteMutex);

				Conn->Socket->Close();
				if (SocketSub)
				{
					SocketSub->DestroySocket(Conn->Socket);
				}
				Conn->Socket = nullptr;
			}
		}
		SSEConnections.Empty();
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
	int32 TotalSent = 0;
	while (TotalSent < Length)
	{
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
		return 1;
	}

	ListenSocket = SocketSub->CreateSocket(NAME_Stream,
		TEXT("McpNativeHTTPListenSocket"), FName());
	if (!ListenSocket)
	{
		UE_LOG(LogMcpNativeTransport, Error, TEXT("Failed to create listen socket"));
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
		return 1;
	}

	if (!ListenSocket->Listen(5))
	{
		UE_LOG(LogMcpNativeTransport, Error, TEXT("Failed to listen on socket"));
		SocketSub->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		return 1;
	}

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
			HandleConnection(ClientSocket, SocketSub);
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

void FMcpNativeTransport::HandleConnection(FSocket* ClientSocket, ISocketSubsystem* SocketSub)
{
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

	// ── DELETE /mcp — session termination ──
	if (HttpReq.Method == TEXT("DELETE"))
	{
		if (!HttpReq.SessionId.IsEmpty())
		{
			FScopeLock Lock(&SessionMutex);
			if (ActiveSessions.Remove(HttpReq.SessionId) > 0)
			{
				UE_LOG(LogMcpNativeTransport, Log,
					TEXT("Session %s terminated by client (remaining: %d)"),
					*HttpReq.SessionId, ActiveSessions.Num());
			}
		}
		SendHttpResponse(ClientSocket, 200, TEXT("text/plain"), FString());
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// ── POST /mcp — JSON-RPC ──
	if (HttpReq.Method != TEXT("POST"))
	{
		SendHttpResponse(ClientSocket, 405, TEXT("text/plain"), TEXT("Method Not Allowed"));
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	FMcpJsonRpcRequest Rpc = FMcpJsonRpc::ParseRequest(HttpReq.Body);
	if (!Rpc.bValid)
	{
		FString ErrorBody = FMcpJsonRpc::BuildError(
			0, FMcpJsonRpc::ErrorParseError, TEXT("Invalid JSON-RPC request"));
		SendHttpResponse(ClientSocket, 400, TEXT("application/json"), ErrorBody);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Notifications (no id) — 202 Accepted
	if (Rpc.bIsNotification)
	{
		UE_LOG(LogMcpNativeTransport, Log,
			TEXT("Received notification: %s"), *Rpc.Method);
		SendHttpResponse(ClientSocket, 202, TEXT("text/plain"), FString());
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Session validation (skip for initialize)
	if (Rpc.Method != TEXT("initialize"))
	{
		FString SessionError;
		if (!ValidateSession(HttpReq.SessionId, SessionError))
		{
			FString ErrorBody = FMcpJsonRpc::BuildError(
				Rpc.Id, FMcpJsonRpc::ErrorInvalidRequest, SessionError);
			SendHttpResponse(ClientSocket, 400, TEXT("application/json"), ErrorBody);
			ClientSocket->Close();
			SocketSub->DestroySocket(ClientSocket);
			return;
		}
	}

	// ── Method dispatch ──

	if (Rpc.Method == TEXT("initialize"))
	{
		FString NewSessionId;
		FString ResponseBody = HandleInitialize(Rpc.Params, Rpc.Id, NewSessionId);
		TMap<FString, FString> Headers;
		Headers.Add(TEXT("Mcp-Session-Id"), NewSessionId);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ResponseBody, Headers);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	if (Rpc.Method == TEXT("tools/list"))
	{
		FString ResponseBody = HandleToolsList(Rpc.Id);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ResponseBody);
		ClientSocket->Close();
		SocketSub->DestroySocket(ClientSocket);
		return;
	}

	if (Rpc.Method == TEXT("tools/call"))
	{
		// HandleToolsCall takes ownership of the socket (SSE streaming)
		HandleToolsCall(Rpc.Params, Rpc.Id, ClientSocket, HttpReq.SessionId);
		return;  // Socket NOT closed here — parked for SSE
	}

	// Unknown method
	FString ErrorBody = FMcpJsonRpc::BuildError(
		Rpc.Id, FMcpJsonRpc::ErrorMethodNotFound,
		FString::Printf(TEXT("Unknown method: %s"), *Rpc.Method));
	SendHttpResponse(ClientSocket, 400, TEXT("application/json"), ErrorBody);
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
	FString HeaderStr = FString(HeaderBuf.Num(),
		reinterpret_cast<const char*>(HeaderBuf.GetData()));

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
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		FString Key, Value;
		if (Lines[i].Split(TEXT(":"), &Key, &Value))
		{
			Key.TrimStartAndEndInline();
			Value.TrimStartAndEndInline();

			if (Key.Equals(TEXT("Content-Length"), ESearchCase::IgnoreCase))
			{
				OutRequest.ContentLength = FCString::Atoi(*Value);
			}
			else if (Key.Equals(TEXT("Mcp-Session-Id"), ESearchCase::IgnoreCase))
			{
				OutRequest.SessionId = Value;
			}
			else if (Key.Equals(TEXT("Accept"), ESearchCase::IgnoreCase))
			{
				OutRequest.Accept = Value;
			}
		}
	}

	// Read body
	static constexpr int32 MaxBodySize = 5 * 1024 * 1024;  // 5MB
	if (OutRequest.ContentLength > MaxBodySize)
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("HTTP body too large: %d bytes"), OutRequest.ContentLength);
		return false;
	}

	if (OutRequest.ContentLength > 0)
	{
		TArray<uint8> BodyBuf;
		BodyBuf.SetNumUninitialized(OutRequest.ContentLength);
		int32 TotalRead = 0;

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

		OutRequest.Body = FString(TotalRead,
			reinterpret_cast<const char*>(BodyBuf.GetData()));
	}

	return true;
}

// ─── HTTP Response Helpers ──────────────────────────────────────────────────

bool FMcpNativeTransport::SendHttpResponse(FSocket* Socket, int32 StatusCode,
	const FString& ContentType, const FString& Body,
	const TMap<FString, FString>& ExtraHeaders)
{
	FString StatusText;
	switch (StatusCode)
	{
	case 200: StatusText = TEXT("OK"); break;
	case 202: StatusText = TEXT("Accepted"); break;
	case 400: StatusText = TEXT("Bad Request"); break;
	case 404: StatusText = TEXT("Not Found"); break;
	case 405: StatusText = TEXT("Method Not Allowed"); break;
	case 500: StatusText = TEXT("Internal Server Error"); break;
	default:  StatusText = TEXT("OK"); break;
	}

	FTCHARToUTF8 BodyUtf8(*Body);
	const int32 BodyLength = BodyUtf8.Length();

	FString Response = FString::Printf(
		TEXT("HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n"),
		StatusCode, *StatusText, *ContentType, BodyLength);

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

bool FMcpNativeTransport::SendSSEHeaders(FSocket* Socket, const FString& SessionId)
{
	FString Headers = FString::Printf(
		TEXT("HTTP/1.1 200 OK\r\n")
		TEXT("Content-Type: text/event-stream\r\n")
		TEXT("Cache-Control: no-cache\r\n")
		TEXT("Connection: keep-alive\r\n")
		TEXT("Mcp-Session-Id: %s\r\n")
		TEXT("\r\n"),
		*SessionId);

	FTCHARToUTF8 Utf8(*Headers);
	return SendAllBytes(Socket, reinterpret_cast<const uint8*>(Utf8.Get()),
		Utf8.Length());
}

bool FMcpNativeTransport::WriteSSEEvent(FSocket* Socket, const FString& EventData,
	FCriticalSection& WriteMutex)
{
	FString Frame = FString::Printf(
		TEXT("event: message\ndata: %s\n\n"), *EventData);

	FTCHARToUTF8 Utf8(*Frame);

	FScopeLock Lock(&WriteMutex);
	return SendAllBytes(Socket, reinterpret_cast<const uint8*>(Utf8.Get()),
		Utf8.Length());
}

// ─── Initialize ─────────────────────────────────────────────────────────────

FString FMcpNativeTransport::HandleInitialize(
	const TSharedPtr<FJsonObject>& Params, int32 Id, FString& OutSessionId)
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
	{
		FScopeLock Lock(&SessionMutex);
		ActiveSessions.Add(OutSessionId, FPlatformTime::Seconds());
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
		*OutSessionId, *ClientName, *ClientVersion, ActiveSessions.Num());

	return FMcpJsonRpc::BuildResponse(Id, Result);
}

// ─── Tools List ─────────────────────────────────────────────────────────────

FString FMcpNativeTransport::HandleToolsList(int32 Id)
{
	TSet<FString> EnabledTools = ToolManager.GetEnabledToolNames();
	TSharedPtr<FJsonObject> ToolsList = SchemaLoader.GetFilteredToolsResponse(EnabledTools);

	if (ToolsList.IsValid())
	{
		return FMcpJsonRpc::BuildResponse(Id, ToolsList);
	}

	auto EmptyResult = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> EmptyArray;
	EmptyResult->SetArrayField(TEXT("tools"), EmptyArray);
	return FMcpJsonRpc::BuildResponse(Id, EmptyResult);
}

// ─── Tools Call (SSE streaming) ─────────────────────────────────────────────

void FMcpNativeTransport::HandleToolsCall(
	const TSharedPtr<FJsonObject>& Params, int32 Id,
	FSocket* ClientSocket, const FString& SessionId)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	if (!Params.IsValid())
	{
		FString ErrorBody = FMcpJsonRpc::BuildError(
			Id, FMcpJsonRpc::ErrorInvalidParams, TEXT("Missing params"));
		SendHttpResponse(ClientSocket, 400, TEXT("application/json"), ErrorBody);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	FString ToolName;
	if (!Params->TryGetStringField(TEXT("name"), ToolName))
	{
		FString ErrorBody = FMcpJsonRpc::BuildError(
			Id, FMcpJsonRpc::ErrorInvalidParams, TEXT("Missing tool name"));
		SendHttpResponse(ClientSocket, 400, TEXT("application/json"), ErrorBody);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	const TSharedPtr<FJsonObject>* ArgumentsPtr = nullptr;
	TSharedPtr<FJsonObject> Arguments;
	if (Params->TryGetObjectField(TEXT("arguments"), ArgumentsPtr) && ArgumentsPtr)
	{
		Arguments = *ArgumentsPtr;
	}
	else
	{
		Arguments = MakeShared<FJsonObject>();
	}

	// Intercept manage_tools — handle locally (one-shot, no SSE)
	if (ToolName == TEXT("manage_tools"))
	{
		FString Action;
		Arguments->TryGetStringField(TEXT("action"), Action);
		TSharedPtr<FJsonObject> Result = ToolManager.HandleAction(Action, Arguments);
		TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
			true, TEXT("OK"), Result);
		FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), Body);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Send SSE headers — begins the streaming response
	if (!SendSSEHeaders(ClientSocket, SessionId))
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
		SSEConnections.Add(RequestId, Conn);
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("tools/call: %s (RequestId=%s)"),
		*ToolName, *RequestId);

	// Dispatch to handler on GameThread
	TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(Subsystem);
	FString CapturedRequestId = RequestId;
	FString CapturedToolName = ToolName;
	TSharedPtr<FJsonObject> CapturedArguments = Arguments;

	AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, CapturedRequestId,
		CapturedToolName, CapturedArguments]()
	{
		if (UMcpAutomationBridgeSubsystem* Sub = WeakSubsystem.Get())
		{
			Sub->ProcessAutomationRequest(
				CapturedRequestId, CapturedToolName, CapturedArguments, nullptr);
		}
	});
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

	if (!Conn.IsValid() || !Conn->Socket)
	{
		return true;  // Already cleaned up
	}

	// Build final JSON-RPC result
	TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
		bSuccess, Message, Result, ErrorCode);
	FString ResponseBody = FMcpJsonRpc::BuildResponse(Conn->JsonRpcId, ToolResult);

	// Send as final SSE event
	WriteSSEEvent(Conn->Socket, ResponseBody, Conn->WriteMutex);

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("tools/call completed: %s (tool=%s, success=%s)"),
		*RequestId, *Conn->ToolName, bSuccess ? TEXT("true") : TEXT("false"));

	// Close the connection
	Conn->Socket->Close();
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSub)
	{
		SocketSub->DestroySocket(Conn->Socket);
	}
	Conn->Socket = nullptr;

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
	FScopeLock Lock(&SSEConnectionsMutex);
	TSharedPtr<FSSEConnection>* Found = SSEConnections.Find(RequestId);
	if (!Found || !Found->IsValid() || !(*Found)->Socket)
	{
		return;
	}

	TSharedPtr<FSSEConnection>& Conn = *Found;

	FString ProgressJson = FMcpJsonRpc::BuildProgressNotification(
		RequestId, Percent, 100.0f, Message);

	if (!WriteSSEEvent(Conn->Socket, ProgressJson, Conn->WriteMutex))
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("SSE write failed for request %s — client may have disconnected"),
			*RequestId);
		// Don't remove here — CleanupStaleRequests or CompletePendingRequest will handle it
	}

	// Reset timeout
	Conn->StartTime = FPlatformTime::Seconds();
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
			if (Conn.IsValid() && Now - Conn->StartTime > RequestTimeoutSeconds)
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
}

// ─── Session Validation ─────────────────────────────────────────────────────

bool FMcpNativeTransport::ValidateSession(
	const FString& SessionId, FString& OutError)
{
	if (SessionId.IsEmpty())
	{
		OutError = TEXT("Missing Mcp-Session-Id header");
		return false;
	}

	FScopeLock Lock(&SessionMutex);
	double* LastActivity = ActiveSessions.Find(SessionId);
	if (!LastActivity)
	{
		OutError = TEXT("Invalid or expired session ID");
		return false;
	}

	// Touch session activity
	*LastActivity = FPlatformTime::Seconds();
	return true;
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

	// Snapshot under lock, I/O outside (same pattern as CompletePendingRequest)
	TArray<TSharedPtr<FSSEConnection>> Snapshot;
	{
		FScopeLock Lock(&SSEConnectionsMutex);
		Snapshot.Reserve(SSEConnections.Num());
		for (auto& [ReqId, Conn] : SSEConnections)
		{
			if (Conn.IsValid() && Conn->Socket)
			{
				Snapshot.Add(Conn);
			}
		}
	}

	for (const auto& Conn : Snapshot)
	{
		if (!WriteSSEEvent(Conn->Socket, NotificationJson, Conn->WriteMutex))
		{
			UE_LOG(LogMcpNativeTransport, Warning,
				TEXT("Failed to send list_changed to SSE connection (tool=%s)"),
				*Conn->ToolName);
		}
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("Broadcast list_changed to %d SSE connection(s)"), Snapshot.Num());
}

int32 FMcpNativeTransport::GetActiveSessionCount() const
{
	FScopeLock Lock(&SessionMutex);
	return ActiveSessions.Num();
}
