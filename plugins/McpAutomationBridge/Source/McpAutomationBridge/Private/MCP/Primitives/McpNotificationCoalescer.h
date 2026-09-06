// McpNotificationCoalescer.h
// Task 34 (native mirror): bounded, debounced/coalesced resource-notification
// engine. Native counterpart of
// src/server/mcp-primitives/subscriptions/notification-coalescer.ts. A PURE state
// machine over FMcpSubscriptionStore: it records change signals per session+URI,
// folds a burst into one pending entry inside a fixed coalescing window (injected
// clock), and on flush emits ONE bounded payload carrying only URI/revision/change
// kind. It NEVER writes a transport — the injected sink receives the payload and
// Task 37 owns SSE/session/real-timer wiring. Revisions come from the Task 31 C2
// revision source (so a notification matches a later resource read) and the
// catalog cursor from the Task 36 C1 reader (IMcpCatalogRevisionReader).
#pragma once

#include "CoreMinimal.h"
#include "MCP/Primitives/IMcpCatalogRevisionReader.h"
#include "MCP/Primitives/McpResourceRevision.h"
#include "MCP/Primitives/McpSubscriptionStore.h"

/**
 * Bounded resources/updated payload: URI + revision + change kind ONLY. No body,
 * diff, host path, or editor internal — the client re-reads the resource for
 * detail. Mirrors the TypeScript `ResourceUpdatedPayload`.
 */
struct FMcpResourceUpdatedPayload
{
	FString Uri;
	FMcpResourceRevision Revision = McpInitialResourceRevision;
	FString ChangeKind;
};

class FMcpNotificationCoalescer
{
public:
	using FClock = TFunction<int64()>;
	using FRevisionSource = TFunction<FMcpResourceRevision(const FString& /*Uri*/)>;
	using FSink = TFunction<void(const FString& /*SessionId*/, const FMcpResourceUpdatedPayload& /*Payload*/)>;

	FMcpNotificationCoalescer(
		FMcpSubscriptionStore& InStore,
		FRevisionSource InRevisionSource,
		const IMcpCatalogRevisionReader& InCatalog,
		FSink InSink,
		FClock InClock,
		int64 InWindowMs = 50);

	bool RecordChange(const FString& SessionId, const FString& Uri, const FString& ChangeKind = TEXT("updated"));
	bool SyncCatalog(const FString& SessionId);
	int32 RecordGlobalChange(const FString& Uri, const FString& ChangeKind = TEXT("updated"));
	int32 FlushDue(int64 Now);
	bool NextDueAt(int64& OutDueAt) const;
	int32 PendingCount() const;
	void DropPending(const FString& SessionId, const FString& Uri);
	void ClearSession(const FString& SessionId);

private:
	struct FPending
	{
		FString SessionId;
		FString Uri;
		FString ChangeKind;
		int64 DueAt = 0;
	};

	static bool IsChangeKind(const FString& ChangeKind);
	static FString Key(const FString& SessionId, const FString& Uri);

	FMcpSubscriptionStore& Store;
	FRevisionSource RevisionSource;
	const IMcpCatalogRevisionReader& Catalog;
	FSink Sink;
	FClock Clock;
	int64 WindowMs;

	mutable FCriticalSection StateMutex;
	TMap<FString, FPending> Pending;
	TMap<FString, FMcpResourceRevision> LastEmitted;
	TMap<FString, uint64> CatalogCursor;
};
