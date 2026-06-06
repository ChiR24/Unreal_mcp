#include "MCP/McpNativeTransportPrivate.h"

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
