#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Primitives/McpNotificationCoalescer.h"

// Task 37 (native mirror of primitive-notifications.ts): builds the resources/
// updated notification and delivers it over the shared async writer. The Task 34
// coalescer folds a burst of catalog changes into one payload carrying uri +
// revision + change kind internally, but only the URI crosses the wire (the
// client re-reads the resource); the revision and change kind never leak. This
// unit REUSES QueueNotificationEventWrites and never re-declares that writer, and
// drains due notifications from the existing keepalive loop, adding no thread.

void FMcpNativeTransport::SendResourceUpdatedNotification(
	const FString& SessionId, const FString& Uri)
{
	// URI-only wire params per the MCP spec: no revision, no change kind on the
	// wire. The client re-reads the resource to observe the new revision.
	auto Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("uri"), Uri);
	const FString NotificationJson =
		FMcpJsonRpc::BuildNotification(TEXT("notifications/resources/updated"), Params);

	// Resolve the ready notification streams belonging to the subscribed session,
	// then hand them to the shared async writer (no new thread, no editor API).
	TArray<TSharedPtr<FNotificationStream>> StreamSnapshot;
	{
		FScopeLock Lock(&NotificationStreamsMutex);
		for (const auto& Entry : NotificationStreams)
		{
			const TSharedPtr<FNotificationStream>& Stream = Entry.Value;
			if (Stream.IsValid() && Stream->bReady.load() &&
				!Stream->bMarkedForRemoval.load() && Stream->SessionId == SessionId)
			{
				StreamSnapshot.Add(Stream);
			}
		}
	}
	QueueNotificationEventWrites(StreamSnapshot, NotificationJson);
}

void FMcpNativeTransport::FlushDuePrimitiveNotifications()
{
	// Read the coalescer pointer under the primitive lock, then flush outside it
	// so the sink's stream lock is never nested under the primitive lock. The
	// coalescer lives for the transport's lifetime once constructed.
	FMcpNotificationCoalescer* Coalescer = nullptr;
	{
		FScopeLock Lock(&PrimitiveStateMutex);
		Coalescer = NotificationCoalescer.Get();
	}
	if (Coalescer)
	{
		Coalescer->FlushDue(static_cast<int64>(FPlatformTime::Seconds() * 1000.0));
	}
}
