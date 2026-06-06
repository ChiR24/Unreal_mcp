#include "MCP/McpNativeTransportPrivate.h"

DEFINE_LOG_CATEGORY(LogMcpNativeTransport);


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
