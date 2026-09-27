#include "MCP/Transport/McpNativeTransportPrivate.h"

namespace
{
bool IsLogAutomationEvent(const TSharedPtr<FJsonObject>& Event)
{
	FString EventName;
	return Event.IsValid() &&
		Event->TryGetStringField(TEXT("event"), EventName) &&
		EventName.Equals(TEXT("log"), ESearchCase::CaseSensitive);
}
}
bool FMcpNativeTransport::SetLogEventSubscriptionForRequest(
	const FString& RequestId, const bool bSubscribed)
{
	FString SessionId;
	{
		FScopeLock Lock(&SSEConnectionsMutex);
		const TSharedPtr<FSSEConnection>* Connection = SSEConnections.Find(RequestId);
		if (!Connection || !Connection->IsValid())
		{
			return false;
		}
		SessionId = (*Connection)->SessionId;
	}

	FScopeLock Lock(&LogEventSubscriptionsMutex);
	if (bSubscribed)
	{
		LogEventSubscribedSessions.Add(SessionId);
	}
	else
	{
		LogEventSubscribedSessions.Remove(SessionId);
	}
	return true;
}

bool FMcpNativeTransport::HasLogEventSubscribers() const
{
	FScopeLock Lock(&LogEventSubscriptionsMutex);
	return LogEventSubscribedSessions.Num() > 0;
}

int32 FMcpNativeTransport::BroadcastLogEventNotification(
	const TSharedPtr<FJsonObject>& Params)
{
	if (!IsLogAutomationEvent(Params))
	{
		return 0;
	}

	TSet<FString> SubscribedSessions;
	{
		FScopeLock Lock(&LogEventSubscriptionsMutex);
		SubscribedSessions = LogEventSubscribedSessions;
	}
	if (SubscribedSessions.IsEmpty())
	{
		return 0;
	}

	const FString NotificationJson = FMcpJsonRpc::BuildNotification(
		TEXT("notifications/unreal/automation_event"), Params);
	TArray<TSharedPtr<FNotificationStream>> StreamSnapshot;
	{
		FScopeLock Lock(&NotificationStreamsMutex);
		for (auto& [StreamId, Stream] : NotificationStreams)
		{
			if (Stream.IsValid() && Stream->bReady.load() &&
				!Stream->bMarkedForRemoval.load() &&
				SubscribedSessions.Contains(Stream->SessionId))
			{
				StreamSnapshot.Add(Stream);
			}
		}
	}

	return QueueNotificationEventWrites(StreamSnapshot, NotificationJson);
}

void FMcpNativeTransport::HandleGetMcp(FSocket* ClientSocket, const FString& SessionId,
	const FString& CorsOrigin)
{
	const double Now = FPlatformTime::Seconds();
	TSharedPtr<FNotificationStream> Stream = MakeShared<FNotificationStream>();
	Stream->Socket = ClientSocket;
	Stream->SessionId = SessionId;
	Stream->StreamId = FGuid::NewGuid().ToString();
	Stream->StartTime = Now;
	Stream->LastKeepaliveTime = Now;

	bool bStreamLimitReached = false;
	bool bSessionStreamLimitReached = false;
	bool bSessionInvalid = false;
	{
		// Session validity is checked under SessionMutex; the lock is released
		// before acquiring NotificationStreamsMutex to keep the global lock
		// order documented in McpNativeTransport.h (no nested Session→Stream
		// critical sections). The TOCTOU window (session expiring between
		// check and add) is bounded: CloseSessionConnections removes the
		// stream entry on session timeout.
		FScopeLock SessionLock(&SessionMutex);
		if (!ActiveSessions.Contains(SessionId))
		{
			bSessionInvalid = true;
		}
	}
	if (!bSessionInvalid)
	{
		FScopeLock StreamLock(&NotificationStreamsMutex);
		int32 SessionStreamCount = 0;
		int32 ActiveStreamCount = 0;
		for (const auto& [Id, ExistingStream] : NotificationStreams)
		{
			if (!ExistingStream.IsValid() ||
				ExistingStream->bMarkedForRemoval.load())
			{
				continue;
			}
			++ActiveStreamCount;
			if (ExistingStream->SessionId == SessionId)
			{
				++SessionStreamCount;
			}
		}
		if (ActiveStreamCount >= MaxTotalNotificationStreams)
		{
			bStreamLimitReached = true;
		}
		else if (SessionStreamCount >=
			MaxNotificationStreamsPerSession)
		{
			bSessionStreamLimitReached = true;
		}
		else
		{
			NotificationStreams.Add(Stream->StreamId, Stream);
		}
	}
	// Re-check session validity after the add to close the TOCTOU window;
	// if the session was closed between the initial check and the stream
	// add, the new stream is stale and must be removed before headers
	// are sent. Cleanup uses the captured shared pointer for the socket,
	// so removal from the map is safe.
	if (!bSessionInvalid && !bStreamLimitReached && !bSessionStreamLimitReached)
	{
		bool bClosed;
		{ FScopeLock L(&SessionMutex); bClosed = !ActiveSessions.Contains(SessionId); }
		if (bClosed) {
			FScopeLock L(&NotificationStreamsMutex);
			NotificationStreams.Remove(Stream->StreamId);
			bSessionInvalid = true;
		}
	}
	if (bSessionInvalid)
	{
		SendAndClose(ClientSocket, 404, TEXT("text/plain"),
			TEXT("Invalid or expired session ID"), {}, CorsOrigin);
		return;
	}
	if (bStreamLimitReached || bSessionStreamLimitReached)
	{
		const FString LimitMessage = bStreamLimitReached
			? FString::Printf(
				TEXT("Too Many Requests: max %d notification streams"),
				MaxTotalNotificationStreams)
			: FString::Printf(
				TEXT("Too Many Requests: max %d notification streams per session"),
				MaxNotificationStreamsPerSession);
		SendAndClose(ClientSocket, 429, TEXT("text/plain"),
			LimitMessage, {}, CorsOrigin);
		return;
	}

	bool bHeadersSent = false;
	{
		FScopeLock WriteLock(&Stream->WriteMutex);
		if (Stream->Socket)
		{
			bHeadersSent = SendSSEHeaders(
				Stream->Socket, SessionId, CorsOrigin);
			if (!bHeadersSent)
			{
				CloseSocket(Stream->Socket);
			}
		}
	}
	if (!bHeadersSent)
	{
		{
			FScopeLock Lock(&NotificationStreamsMutex);
			NotificationStreams.Remove(Stream->StreamId);
		}
		return;
	}
	Stream->bReady.store(true);
	TouchSession(SessionId);

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("GET /mcp: notification stream %s opened"),
		*Stream->StreamId);
	// Socket is parked — do NOT close it. Thread pool slot is released.
}

bool FMcpNativeTransport::WriteNotificationEvent(FNotificationStream& Stream, const FString& EventData)
{
	FScopeLock Lock(&Stream.WriteMutex);
	return Stream.Socket && SendSSEFrame(Stream.Socket, EventData);
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
	FScopeLock Lock(&Stream->WriteMutex);
	CloseSocket(Stream->Socket);
}
