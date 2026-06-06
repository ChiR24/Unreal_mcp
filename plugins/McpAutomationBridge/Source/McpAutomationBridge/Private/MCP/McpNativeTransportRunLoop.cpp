#include "MCP/McpNativeTransportPrivate.h"

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
