#include "MCP/Transport/McpNativeTransportPrivate.h"

// Lives beside the reclaim paths rather than in Sessions.cpp: it is only ever
// used to decide what may be recycled, and Sessions.cpp is at its line ceiling.
void FMcpNativeTransport::CollectSessionsWithLiveConnections(
	TSet<FString>& OutSessionIds) const
{
	// Both collection mutexes are taken in SEQUENTIAL (non-nested) scopes:
	// Notification first, then SSE, matching the CloseSessionConnections
	// relative order. SessionMutex is deliberately NOT taken here — callers
	// gather this set before locking the session map, so the two never nest.
	const auto Collect = [&OutSessionIds](const auto& Streams)
	{
		for (const auto& [Key, Stream] : Streams)
		{
			if (Stream.IsValid() && !Stream->SessionId.IsEmpty())
			{
				OutSessionIds.Add(Stream->SessionId);
			}
		}
	};
	{
		FScopeLock Lock(&NotificationStreamsMutex);
		Collect(NotificationStreams);
	}
	FScopeLock Lock(&SSEConnectionsMutex);
	Collect(SSEConnections);
}

void FMcpNativeTransport::CleanupStaleRequests()
{
	const double Now = FPlatformTime::Seconds();
	LastGameThreadHeartbeat.store(Now); // this pass runs on the GameThread (dogfood #79)

	// Clean up timed-out SSE connections
	TMap<FString, double> Expired;
	{
		FScopeLock Lock(&SSEConnectionsMutex);
		for (const auto& [RequestId, Conn] : SSEConnections)
		{
				if (Conn.IsValid() && (Now - Conn->StartTime > Conn->TimeoutSeconds
					|| Conn->bMarkedForRemoval.load()))
			{
				Expired.Add(RequestId, Conn->TimeoutSeconds);
			}
		}
	}

	for (const TPair<FString, double>& Entry : Expired)
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("SSE request %s timed out after %.0f seconds"),
				*Entry.Key, Entry.Value);
		if (Subsystem)
		{
			Subsystem->CancelAutomationRequest(Entry.Key);
		}
		CompletePendingRequest(Entry.Key, false, TEXT("Request timed out"),
			nullptr, TEXT("TIMEOUT"));
	}

	// Clean up inactive sessions
	TArray<FString> ExpiredSessions;
	{
		FScopeLock Lock(&SessionMutex);
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
			SessionRateStates.Remove(SessionId);
			SessionPrincipals.Remove(SessionId);
			UE_LOG(LogMcpNativeTransport, Log,
				TEXT("Session expired after %.0f min inactivity (remaining: %d)"),
				SessionTimeoutSeconds / 60.0, ActiveSessions.Num());
		}
		for (auto It = ClientRateStates.CreateIterator(); It; ++It)
		{
			if (Now - It.Value().LastActivity >
				SessionRateWindowSeconds * 2.0)
			{
				It.RemoveCurrent();
			}
		}
	}
	if (ExpiredSessions.Num() > 0)
	{
		for (const FString& SessionId : ExpiredSessions)
		{
			CloseSessionConnections(SessionId);
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
					TEXT("Notification stream %s closed"), *StreamId);
			}
		}

		// Notification-stream keepalive is handled by the dedicated keepalive thread
		// (RunKeepaliveLoop / SweepNotificationKeepalives) so it keeps firing during
		// long GameThread stalls (recompile, PIE, modal dialog, blocking import).
		// This GameThread sweep now only reaps. See Start()/Shutdown().
	}
}

// ─── Keepalive Loop (dedicated thread) ──────────────────────────────────────
//
// The SSE notification-stream keepalive used to run from this GameThread cleanup
// pass (driven by the subsystem ticker). When the GameThread stalls longer than
// the keepalive interval, no keepalive is sent, the client's stream idles out,
// and the MCP session is re-initialized. Running the sweep on its own thread
// keeps keepalives flowing through GameThread stalls.

void FMcpNativeTransport::RunKeepaliveLoop()
{
	// Tick faster than the keepalive interval so a stream is never more than one
	// tick late. StopEvent is triggered by Stop(), waking us immediately on shutdown.
	static constexpr uint32 TickMs = 5000;
	while (!bStopping.load())
	{
		if (StopEvent)
		{
			StopEvent->Wait(TickMs);
		}
		else
		{
			FPlatformProcess::Sleep(TickMs / 1000.0f);
		}
		if (bStopping.load())
		{
			break;
		}
		SweepNotificationKeepalives();
		// Task 37: drain any due coalesced resources/updated from the same
		// existing keepalive thread — no dedicated primitive-notification thread.
		FlushDuePrimitiveNotifications();
	}
}

void FMcpNativeTransport::SweepNotificationKeepalives()
{
	const double Now = FPlatformTime::Seconds();

	// Snapshot living streams under the map lock, then write outside it — the socket
	// write takes each stream's WriteMutex, so the two locks are never nested.
	TArray<TSharedPtr<FNotificationStream>> AliveSnapshot;
	{
		FScopeLock Lock(&NotificationStreamsMutex);
		for (auto& [StreamId, Stream] : NotificationStreams)
		{
			if (Stream.IsValid() && !Stream->bMarkedForRemoval.load()
				&& Now - Stream->LastKeepaliveTime >= KeepaliveIntervalSeconds
				&& Stream->bReady.load())
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

// ─── Session Validation ─────────────────────────────────────────────────────
