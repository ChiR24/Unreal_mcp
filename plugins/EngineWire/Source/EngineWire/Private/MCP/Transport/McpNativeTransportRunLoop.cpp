#include "MCP/Transport/McpNativeTransportPrivate.h"

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

bool FMcpNativeTransport::SendSSEFrame(FSocket* Socket, const FString& EventData)
{
	const FTCHARToUTF8 Utf8(*FString::Printf(TEXT("event: message\ndata: %s\n\n"), *EventData));
	return SendAllBytes(Socket, reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
}

void FMcpNativeTransport::CloseSocket(FSocket*& Socket)
{
	if (!Socket)
	{
		return;
	}
	Socket->Close();
	if (ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		SocketSub->DestroySocket(Socket);
	}
	Socket = nullptr;
}

// ─── Accept Loop (FRunnable::Run) ───────────────────────────────────────────

uint32 FMcpNativeTransport::Run()
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	// Every bind-phase failure: drop the socket, tell Start() and end the thread.
	const auto FailBind = [this, SocketSub](const FString& Why) -> uint32
	{
		UE_LOG(LogMcpNativeTransport, Error, TEXT("%s"), *Why);
		if (SocketSub && ListenSocket)
		{
			SocketSub->DestroySocket(ListenSocket);
			ListenSocket = nullptr;
		}
		bBindSuccess.store(false);
		if (BindCompleteEvent) BindCompleteEvent->Trigger();
		return 1;
	};
	if (!SocketSub)
	{
		return FailBind(TEXT("Failed to get socket subsystem"));
	}

	ListenSocket = SocketSub->CreateSocket(NAME_Stream,
		TEXT("McpNativeHTTPListenSocket"), FName());
	if (!ListenSocket)
	{
		return FailBind(TEXT("Failed to create listen socket"));
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
		return FailBind(FString::Printf(TEXT("Failed to bind to %s:%d"), *ListenHost, ListenPort));
	}
	if (!ListenSocket->Listen(5))
	{
		return FailBind(TEXT("Failed to listen on socket"));
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
				SendAndClose(ClientSocket, 503, TEXT("text/plain"), TEXT("Service Unavailable"));
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
	CloseSocket(ListenSocket);

	UE_LOG(LogMcpNativeTransport, Verbose, TEXT("Accept loop exited"));
	return 0;
}

// ─── Connection Handler ─────────────────────────────────────────────────────
