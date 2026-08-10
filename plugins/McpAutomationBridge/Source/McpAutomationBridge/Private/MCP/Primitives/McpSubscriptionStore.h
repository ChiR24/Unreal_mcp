// McpSubscriptionStore.h
// Task 34 (native mirror): revisioned per-session resource subscription store.
// Native counterpart of
// src/server/mcp-primitives/subscriptions/subscription-store.ts. Metadata/logic
// only: NO transport wiring and NO session/lifecycle edits (Task 37 supplies
// native session ids and the cleanup calls; Task 42 wires non-catalog producers).
// Each session owns an INDEPENDENT, insertion-ordered set of subscribed URIs from
// the Task 31 C2 allowlist (MCP/Primitives/McpResourceRevision.h); one session's
// subscriptions can never be read or drained as another's. Holds NO timers, NO
// transport, NO revisions — FMcpNotificationCoalescer layers those on top.
#pragma once

#include "CoreMinimal.h"
#include "MCP/Primitives/McpResourceRevision.h"

/**
 * Deterministic outcome of a subscribe request. `Evicted` names the oldest URI
 * dropped to make room under the per-session cap (empty when nothing evicted);
 * `bAlreadySubscribed` marks the idempotent duplicate case. `Reason` is empty on
 * success, else "NOT_SUBSCRIBABLE" or "INVALID_SESSION".
 */
struct FMcpSubscribeResult
{
	bool bAccepted = false;
	bool bAlreadySubscribed = false;
	FString Evicted;
	FString Reason;
};

/**
 * Per-session subscription state keyed by an explicit session id. Visibility of a
 * URI is a plain membership test; the cap evicts the oldest subscription
 * deterministically (insertion order) and fires the release hook so the coalescer
 * pending entry and the native editor delegate drain with it.
 */
class FMcpSubscriptionStore
{
public:
	/** Fired for every (session, URI) released by unsubscribe, eviction, or clear. */
	using FReleaseHook = TFunction<void(const FString& /*SessionId*/, const FString& /*Uri*/)>;

	explicit FMcpSubscriptionStore(int32 InMaxPerSession = 9);

	void SetReleaseHook(FReleaseHook InHook);

	FMcpSubscribeResult Subscribe(const FString& SessionId, const FString& Uri);
	bool Unsubscribe(const FString& SessionId, const FString& Uri);
	bool IsSubscribed(const FString& SessionId, const FString& Uri) const;
	TArray<FString> Subscriptions(const FString& SessionId) const;
	int32 Count(const FString& SessionId) const;
	bool HasSession(const FString& SessionId) const;
	int32 SessionCount() const;
	TArray<FString> SessionsSubscribedTo(const FString& Uri) const;
	int32 ClearSession(const FString& SessionId);

private:
	int32 MaxPerSession;
	FReleaseHook OnRelease;

	/** Overlays and the lock are mutable so const reads stay lock-guarded. */
	mutable FCriticalSection StateMutex;
	TMap<FString, TArray<FString>> Sessions;

	static bool IsValidSession(const FString& SessionId);

	/** Fire the release hook OUTSIDE the lock to keep a single lock ordering. */
	void FireRelease(const FString& SessionId, const FString& Uri);
};
