#include "MCP/Transport/McpNativeTransportPrivate.h"

FMcpNativeTransport::ESessionValidationResult FMcpNativeTransport::ValidateSession(
	const FString& SessionId, FString& OutError)
{
	if (SessionId.IsEmpty())
	{
		OutError = TEXT("Missing Mcp-Session-Id header");
		return ESessionValidationResult::Missing;
	}

	{
		FScopeLock Lock(&SessionMutex);
		double* LastActivity = ActiveSessions.Find(SessionId);
		if (!LastActivity)
		{
			OutError = TEXT("Invalid or expired session ID");
			return ESessionValidationResult::Invalid;
		}

		const double Now = FPlatformTime::Seconds();
		if (Now - *LastActivity <= SessionTimeoutSeconds)
		{
			*LastActivity = Now;
			if (FSessionRateState* State = SessionRateStates.Find(SessionId))
			{
				State->bHasClientActivity = true;
			}
			return ESessionValidationResult::Valid;
		}

		ActiveSessions.Remove(SessionId);
		SessionRateStates.Remove(SessionId);
		SessionProtocolVersions.Remove(SessionId);
		SessionPrincipals.Remove(SessionId);
	}
	CloseSessionConnections(SessionId);
	OutError = TEXT("Invalid or expired session ID");
	return ESessionValidationResult::Invalid;
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

void FMcpNativeTransport::MarkSessionInitializationComplete(
	const FString& SessionId)
{
	FScopeLock Lock(&SessionMutex);
	if (FSessionRateState* State = SessionRateStates.Find(SessionId))
	{
		State->InitializationCompletedAt = FPlatformTime::Seconds();
	}
}

bool FMcpNativeTransport::ConsumeSessionRequestBudget(
	const FString& SessionId, bool bToolCall, FString& OutError)
{
	FScopeLock Lock(&SessionMutex);
	FSessionRateState* State = SessionRateStates.Find(SessionId);
	if (!State)
	{
		OutError = TEXT("Invalid or expired session ID");
		return false;
	}
	return ConsumeClientRequestBudgetLocked(
		State->ClientRateKey, bToolCall, OutError);
}

bool FMcpNativeTransport::ConsumeClientRequestBudgetLocked(
	const FString& ClientRateKey, bool bToolCall, FString& OutError)
{
	const double Now = FPlatformTime::Seconds();
	FClientRateState& State = ClientRateStates.FindOrAdd(ClientRateKey);
	if (State.WindowStart <= 0.0 ||
		Now - State.WindowStart >= SessionRateWindowSeconds)
	{
		State.WindowStart = Now;
		State.RequestCount = 0;
		State.ToolCallCount = 0;
	}
	State.LastActivity = Now;
	// Read the rate limit caps from project settings so they are user-tunable
	// via Edit > Project Settings > Plugins > MCP Automation Bridge. A value
	// of 0 disables that cap (per-cap), so users can opt out of either limit
	// independently. Falls back to the hardcoded constants if settings are
	// unavailable (e.g. in a unit test environment).
	const UMcpAutomationBridgeSettings* RateSettings =
		GetDefault<UMcpAutomationBridgeSettings>();
	const int32 RequestsCap = RateSettings
		? RateSettings->MaxClientRequestsPerMinute
		: MaxClientRequestsPerMinute;
	const int32 ToolCallsCap = RateSettings
		? RateSettings->MaxClientToolCallsPerMinute
		: MaxClientToolCallsPerMinute;
	if ((RequestsCap > 0 && State.RequestCount >= RequestsCap) ||
		(bToolCall && ToolCallsCap > 0 &&
		 State.ToolCallCount >= ToolCallsCap))
	{
		OutError = TEXT("Native MCP client rate limit reached");
		return false;
	}
	++State.RequestCount;
	if (bToolCall)
	{
		++State.ToolCallCount;
	}
	return true;
}

bool FMcpNativeTransport::QueueAutomationRequestForSession(
	const FString& SessionId, const FString& RequestId,
	const FString& DispatchAction,
	const TSharedPtr<FJsonObject>& Arguments,
	bool& bOutSessionActive)
{
	bOutSessionActive = false;
	if (!Subsystem)
	{
		return false;
	}

	FScopeLock SessionLock(&SessionMutex);
	if (bStopping.load())
	{
		return false;
	}
	bOutSessionActive = ActiveSessions.Contains(SessionId);
	return bOutSessionActive &&
		Subsystem->QueueAutomationRequest(
			RequestId, DispatchAction, Arguments, nullptr,
			ERequestOrigin::NativeHTTP) == EAutomationQueueRejection::None;
}

void FMcpNativeTransport::CloseSessionConnections(const FString& SessionId)
{
	if (SessionId.IsEmpty())
	{
		return;
	}

	// Task 37: the single close funnel is where DELETE, init-eviction, failed
	// init, and the inactivity-timeout close converge, so draining the session's
	// MCP primitive state (subscriptions + coalescer pending) here covers all of
	// those teardown moments with one seam.
	ReleaseSessionPrimitives(SessionId);

	{
		FScopeLock Lock(&LogEventSubscriptionsMutex);
		LogEventSubscribedSessions.Remove(SessionId);
	}

	TArray<TSharedPtr<FNotificationStream>> NotificationStreamsToClose;
	{
		FScopeLock Lock(&NotificationStreamsMutex);
		for (auto It = NotificationStreams.CreateIterator(); It; ++It)
		{
			const TSharedPtr<FNotificationStream>& Stream = It.Value();
			if (Stream.IsValid() && Stream->SessionId == SessionId)
			{
				NotificationStreamsToClose.Add(Stream);
				It.RemoveCurrent();
			}
		}
	}
	for (const TSharedPtr<FNotificationStream>& Stream :
		NotificationStreamsToClose)
	{
		Stream->bMarkedForRemoval.store(true);
		CloseNotificationStream(Stream);
	}

	TArray<TPair<FString, TSharedPtr<FSSEConnection>>> PendingCallsToClose;
	{
		FScopeLock Lock(&SSEConnectionsMutex);
		for (auto It = SSEConnections.CreateIterator(); It; ++It)
		{
			const TSharedPtr<FSSEConnection>& Connection = It.Value();
			if (Connection.IsValid() && Connection->SessionId == SessionId)
			{
				PendingCallsToClose.Emplace(It.Key(), Connection);
				It.RemoveCurrent();
			}
		}
	}

	TArray<FString> RequestIds;
	RequestIds.Reserve(PendingCallsToClose.Num());
	for (const TPair<FString, TSharedPtr<FSSEConnection>>& Entry :
		PendingCallsToClose)
	{
		RequestIds.Add(Entry.Key);
	}
	if (Subsystem && !RequestIds.IsEmpty())
	{
		Subsystem->CancelAutomationRequests(RequestIds);
	}

	ISocketSubsystem* SocketSub =
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	for (const TPair<FString, TSharedPtr<FSSEConnection>>& Entry :
		PendingCallsToClose)
	{
		const TSharedPtr<FSSEConnection>& Connection = Entry.Value;
		Connection->bMarkedForRemoval.store(true);
		FScopeLock WriteLock(&Connection->WriteMutex);
		if (Connection->Socket)
		{
			Connection->Socket->Close();
			if (SocketSub)
			{
				SocketSub->DestroySocket(Connection->Socket);
			}
			Connection->Socket = nullptr;
		}
	}
}

// ─── Helpers ────────────────────────────────────────────────────────────────

void FMcpNativeTransport::OnToolsListChanged()
{
	// The public tools/list is permanently the single static 'unreal' gateway
	// tool, so a dynamic-tool visibility change never alters its shape; the
	// notifications/tools/list_changed broadcast is suppressed unconditionally.
	UE_LOG(LogMcpNativeTransport, Verbose,
		TEXT("Tool list changed — suppressed (public surface is a static single tool)"));
}

void FMcpNativeTransport::BroadcastToolsListChanged()
{
	const int32 SentCount = BroadcastNotification(
		TEXT("notifications/tools/list_changed"));

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("Broadcast list_changed to %d notification stream(s)"),
		SentCount);
}

int32 FMcpNativeTransport::GetActiveSessionCount() const
{
	FScopeLock Lock(&SessionMutex);
	return ActiveSessions.Num();
}
