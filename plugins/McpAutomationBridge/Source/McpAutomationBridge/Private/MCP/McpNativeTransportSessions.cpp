#include "MCP/McpNativeTransportPrivate.h"

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
