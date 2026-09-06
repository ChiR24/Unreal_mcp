#include "MCP/Primitives/McpNotificationCoalescer.h"
#include "Misc/ScopeLock.h"

namespace McpNotificationCoalescerInternal
{
	// Unit separator: cannot appear in a session id or an allowlisted URI, so a
	// session-prefix drain in ClearSession never collides with another session.
	static const FString KeySeparator = TEXT("\x1F");

	static bool IsKnownChangeKind(const FString& Kind)
	{
		return Kind == TEXT("updated") || Kind == TEXT("invalidated") || Kind == TEXT("removed");
	}
}

FMcpNotificationCoalescer::FMcpNotificationCoalescer(
	FMcpSubscriptionStore& InStore,
	FRevisionSource InRevisionSource,
	const IMcpCatalogRevisionReader& InCatalog,
	FSink InSink,
	FClock InClock,
	int64 InWindowMs)
	: Store(InStore)
	, RevisionSource(MoveTemp(InRevisionSource))
	, Catalog(InCatalog)
	, Sink(MoveTemp(InSink))
	, Clock(MoveTemp(InClock))
	, WindowMs(InWindowMs)
{
}

bool FMcpNotificationCoalescer::IsChangeKind(const FString& ChangeKind)
{
	return McpNotificationCoalescerInternal::IsKnownChangeKind(ChangeKind);
}

FString FMcpNotificationCoalescer::Key(const FString& SessionId, const FString& Uri)
{
	return SessionId + McpNotificationCoalescerInternal::KeySeparator + Uri;
}

bool FMcpNotificationCoalescer::RecordChange(const FString& SessionId, const FString& Uri, const FString& ChangeKind)
{
	if (!IsChangeKind(ChangeKind))
	{
		return false;
	}
	// An unsubscribed or disconnected session, or a non-allowlisted URI, gets nothing.
	if (!Store.IsSubscribed(SessionId, Uri))
	{
		return false;
	}

	FScopeLock Lock(&StateMutex);
	const FString K = Key(SessionId, Uri);
	if (FPending* Existing = Pending.Find(K))
	{
		// Coalesce: keep the original due time (the window does not slide), latest kind wins.
		Existing->ChangeKind = ChangeKind;
	}
	else
	{
		FPending P;
		P.SessionId = SessionId;
		P.Uri = Uri;
		P.ChangeKind = ChangeKind;
		P.DueAt = Clock() + WindowMs;
		Pending.Add(K, P);
	}
	return true;
}

bool FMcpNotificationCoalescer::SyncCatalog(const FString& SessionId)
{
	const uint64 Current = Catalog.GetCatalogStateRevision(SessionId);
	{
		FScopeLock Lock(&StateMutex);
		const uint64 Last = CatalogCursor.FindRef(SessionId);
		if (Current <= Last)
		{
			return false;
		}
		CatalogCursor.Add(SessionId, Current);
	}
	return RecordChange(SessionId, TEXT("ue://capability/catalog"), TEXT("updated"));
}

int32 FMcpNotificationCoalescer::RecordGlobalChange(const FString& Uri, const FString& ChangeKind)
{
	int32 Recorded = 0;
	for (const FString& SessionId : Store.SessionsSubscribedTo(Uri))
	{
		if (RecordChange(SessionId, Uri, ChangeKind))
		{
			++Recorded;
		}
	}
	return Recorded;
}

int32 FMcpNotificationCoalescer::FlushDue(int64 Now)
{
	TArray<TPair<FString, FMcpResourceUpdatedPayload>> ToEmit;
	{
		FScopeLock Lock(&StateMutex);
		TArray<FString> DueKeys;
		for (const auto& Pair : Pending)
		{
			if (Pair.Value.DueAt <= Now)
			{
				DueKeys.Add(Pair.Key);
			}
		}
		for (const FString& K : DueKeys)
		{
			const FPending Change = Pending.FindAndRemoveChecked(K);
			// A timer that fires after unsubscribe/clear is suppressed here.
			if (!Store.IsSubscribed(Change.SessionId, Change.Uri))
			{
				continue;
			}
			const FMcpResourceRevision Revision = RevisionSource(Change.Uri);
			if (const FMcpResourceRevision* Previous = LastEmitted.Find(K))
			{
				// Monotonic: a stale, lower revision never emits.
				if (Revision < *Previous)
				{
					continue;
				}
			}
			LastEmitted.Add(K, Revision);
			FMcpResourceUpdatedPayload Payload;
			Payload.Uri = Change.Uri;
			Payload.Revision = Revision;
			Payload.ChangeKind = Change.ChangeKind;
			ToEmit.Add(TPair<FString, FMcpResourceUpdatedPayload>(Change.SessionId, Payload));
		}
	}

	for (const auto& Pair : ToEmit)
	{
		if (Sink)
		{
			Sink(Pair.Key, Pair.Value);
		}
	}
	return ToEmit.Num();
}

bool FMcpNotificationCoalescer::NextDueAt(int64& OutDueAt) const
{
	FScopeLock Lock(&StateMutex);
	bool bFound = false;
	for (const auto& Pair : Pending)
	{
		if (!bFound || Pair.Value.DueAt < OutDueAt)
		{
			OutDueAt = Pair.Value.DueAt;
			bFound = true;
		}
	}
	return bFound;
}

int32 FMcpNotificationCoalescer::PendingCount() const
{
	FScopeLock Lock(&StateMutex);
	return Pending.Num();
}

void FMcpNotificationCoalescer::DropPending(const FString& SessionId, const FString& Uri)
{
	FScopeLock Lock(&StateMutex);
	const FString K = Key(SessionId, Uri);
	Pending.Remove(K);
	LastEmitted.Remove(K);
}

void FMcpNotificationCoalescer::ClearSession(const FString& SessionId)
{
	FScopeLock Lock(&StateMutex);
	const FString Prefix = SessionId + McpNotificationCoalescerInternal::KeySeparator;
	for (auto It = Pending.CreateIterator(); It; ++It)
	{
		if (It.Key().StartsWith(Prefix))
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = LastEmitted.CreateIterator(); It; ++It)
	{
		if (It.Key().StartsWith(Prefix))
		{
			It.RemoveCurrent();
		}
	}
	CatalogCursor.Remove(SessionId);
}
