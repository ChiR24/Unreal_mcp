#include "MCP/Transport/McpNativeTransportPrivate.h"

namespace
{
bool TryReserveAsyncNotificationWrite(
	std::atomic<int32>& PendingAsyncWrites,
	const int32 MaxPendingWrites)
{
	int32 CurrentPending = PendingAsyncWrites.load();
	while (CurrentPending < MaxPendingWrites)
	{
		if (PendingAsyncWrites.compare_exchange_weak(
				CurrentPending, CurrentPending + 1))
		{
			return true;
		}
	}
	return false;
}
}

int32 FMcpNativeTransport::QueueNotificationEventWrites(
	const TArray<TSharedPtr<FNotificationStream>>& Streams,
	const FString& NotificationJson)
{
	if (NotificationJson.IsEmpty())
	{
		return 0;
	}

	int32 QueuedCount = 0;
	for (const TSharedPtr<FNotificationStream>& Stream : Streams)
	{
		if (!Stream.IsValid() || Stream->bMarkedForRemoval.load())
		{
			continue;
		}

		if (!TryReserveAsyncNotificationWrite(
				PendingAsyncWrites,
				MaxPendingNotificationWrites))
		{
			continue;
		}
		++QueuedCount;

		Async(EAsyncExecution::ThreadPool,
			[this, Stream, NotificationJson]()
		{
			if (bStopping.load() || !Stream.IsValid() ||
				Stream->bMarkedForRemoval.load())
			{
				PendingAsyncWrites.fetch_sub(1);
				return;
			}

			if (WriteNotificationEvent(*Stream, NotificationJson))
			{
				TouchSession(Stream->SessionId);
			}
			else
			{
				Stream->bMarkedForRemoval.store(true);
			}

			PendingAsyncWrites.fetch_sub(1);
		});
	}

	return QueuedCount;
}
