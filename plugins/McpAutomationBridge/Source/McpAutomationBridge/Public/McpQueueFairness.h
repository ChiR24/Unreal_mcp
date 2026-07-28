#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"

// Task 45 - fairness policy for the SERIALIZED editor request queue.
//
// THE INVARIANT THIS FILE MUST NEVER BREAK: there is exactly ONE game-thread
// editor mutation lane. Nothing here executes, parallelises, or overlaps
// requests. It only decides (a) the ORDER in which the single drain picks
// queued work and (b) how much work one session may keep queued. Selection is
// a pure function over session keys and takes no locks — the caller holds the
// queue mutex across the whole call.
//
// Why this exists: PendingAutomationRequests is a TArray that was appended at
// the tail and drained strictly from the head ([0, BatchSize)). A session that
// filled the queue was therefore served to completion before any other
// session's first request ran, and — with MaxPendingAutomationRequests as the
// only cap — a single session could also occupy every global slot and turn
// every other session's request into a QueueFull refusal. Those are the two
// starvation modes: ordering starvation and admission starvation.
namespace McpQueueFairness
{
/**
 * Shared lane for requests that carry no session identity. Only in-process
 * re-queues land here (a deferral from ProcessAutomationRequest during
 * save/GC/async-load re-enters QueueAutomationRequest without the key). No
 * remote client can reach this lane: a WebSocket request always carries its
 * socket and a native MCP request always carries its session id.
 */
inline const TCHAR* AnonymousSessionKey()
{
	return TEXT("anonymous");
}

/**
 * Resolves the stable per-session identity used for both fairness and the
 * per-session admission cap. An explicit key (the native MCP session id) wins;
 * otherwise the owning socket address identifies the WebSocket session.
 */
inline FString ResolveSessionKey(
	const FString& ExplicitSessionKey,
	const void* SocketAddress)
{
	if (!ExplicitSessionKey.IsEmpty())
	{
		return ExplicitSessionKey;
	}
	if (SocketAddress != nullptr)
	{
		return FString::Printf(
			TEXT("ws:%llu"),
			static_cast<uint64>(reinterpret_cast<UPTRINT>(SocketAddress)));
	}
	return AnonymousSessionKey();
}

/**
 * Picks up to BatchSize queue positions in round-robin order across sessions.
 *
 * Guarantees:
 *  - FIFO WITHIN a session (scanning always starts at the queue head, so a
 *    session's older request is always taken before its newer one).
 *  - Bounded wait ACROSS sessions: a session with queued work is served within
 *    one round, i.e. within (number of distinct sessions holding queued work)
 *    dequeues — never after the whole backlog of a flooding session.
 *  - Rotation continues ACROSS ticks: LastServedSessionKey is seeded as
 *    already-served for the first round, so the session that ended the previous
 *    batch cannot also take the head slot of the next one.
 *
 * OutSelectedIndices are indices into SessionKeys, in dispatch order.
 */
inline void SelectFairBatch(
	TArrayView<const FString> SessionKeys,
	int32 BatchSize,
	const FString& LastServedSessionKey,
	TArray<int32>& OutSelectedIndices)
{
	OutSelectedIndices.Reset();
	const int32 PendingNum = SessionKeys.Num();
	if (BatchSize <= 0 || PendingNum <= 0)
	{
		return;
	}

	TArray<bool> bTaken;
	bTaken.Init(false, PendingNum);
	TSet<FString> ServedThisRound;
	if (!LastServedSessionKey.IsEmpty())
	{
		ServedThisRound.Add(LastServedSessionKey);
	}

	int32 RemainingNum = PendingNum;
	while (OutSelectedIndices.Num() < BatchSize && RemainingNum > 0)
	{
		int32 BestIndex = INDEX_NONE;
		for (int32 Index = 0; Index < PendingNum; ++Index)
		{
			if (!bTaken[Index] && !ServedThisRound.Contains(SessionKeys[Index]))
			{
				BestIndex = Index;
				break;
			}
		}
		if (BestIndex == INDEX_NONE)
		{
			// Every session holding queued work has been served once: open a
			// new round. This cannot spin — with the round cleared and
			// RemainingNum > 0, the next iteration always finds a candidate,
			// so a reset is always followed by a selection.
			ServedThisRound.Reset();
			continue;
		}
		bTaken[BestIndex] = true;
		--RemainingNum;
		ServedThisRound.Add(SessionKeys[BestIndex]);
		OutSelectedIndices.Add(BestIndex);
	}
}
}  // namespace McpQueueFairness

/**
 * Scheduler state owned by the subsystem and mutated only under
 * PendingAutomationRequestsMutex (rotation) or on the game thread inside the
 * drain (lane guard). DispatchDepth/MaxObservedDispatchDepth exist so the
 * single-mutation-lane invariant is observable by a test instead of merely
 * assumed: the drain asserts DispatchDepth == 1 around every dispatch, and
 * MaxObservedDispatchDepth records the high-water mark across a whole run.
 */
struct FMcpQueueFairnessState
{
	/**
	 * Per-session admission cap. Deliberately equal to the per-tick batch, so
	 * one session can never hold more than a single tick's worth of the queue
	 * and at most a quarter of MaxPendingAutomationRequests (64).
	 */
	static constexpr int32 MaxPendingRequestsPerSession = 16;

	FString LastServedSessionKey;
	bool bDraining = false;
	int32 DispatchDepth = 0;
	int32 MaxObservedDispatchDepth = 0;
};
