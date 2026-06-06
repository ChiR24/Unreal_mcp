#include "MCP/McpNativeTransportPrivate.h"

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
