#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

// Task 46 gate - the SHARED verdict for "one editor mutation lane, on the game
// thread, exactly once".
//
// Why this is its own header rather than a helper inside one test file: a gate
// nobody has proven can fail is indistinguishable from no gate. The concurrency
// tests feed this oracle observations taken from a REAL subsystem drain, and the
// fault-injection tests feed it hand-built observations carrying a race, an
// off-thread dispatch, a duplicate mutation and a lost request. Both must go
// through the SAME predicate, or the green run is being judged by something
// that was never shown to go red.
//
// The oracle deliberately knows nothing about queues, sessions or transports.
// It takes what a dispatch OBSERVED (the queue's own depth counter, the thread
// it ran on, the request id) plus the set of ids the queue said it ADMITTED,
// and answers four independent yes/no questions. Independent matters: a single
// combined "is it fine" boolean would let one fault mask another.
namespace McpLaneOracle
{
/** One dispatch, as seen from inside the handler. DepthAtDispatch is read from
 * FMcpQueueFairnessState::DispatchDepth - the queue's own counter, never a
 * tally the test maintains, so the observation cannot agree with the test while
 * disagreeing with production. */
struct FDispatchRecord
{
	FString RequestId;
	int32 DepthAtDispatch = 0;
	bool bOnGameThread = false;
};

/** Append-only log of dispatches. The mutex exists because a BROKEN system is
 * exactly the one that would append from two threads at once; recording under a
 * lock means a real race shows up as depth > 1 rather than as heap corruption
 * that takes the whole run down before the verdict is read. */
struct FLaneObservation
{
	mutable FCriticalSection Mutex;
	TArray<FDispatchRecord> Dispatches;
};

/** Four independent detections. IsClean() is a convenience, never the only
 * assertion a caller makes - each flag is asserted on its own so a test failure
 * names the fault that occurred. */
struct FLaneVerdict
{
	bool bRaceDetected = false;
	bool bOffThreadDetected = false;
	bool bDuplicateDetected = false;
	bool bLossDetected = false;

	bool IsClean() const
	{
		return !bRaceDetected && !bOffThreadDetected && !bDuplicateDetected &&
			!bLossDetected;
	}
};

inline void RecordDispatch(
	FLaneObservation& Observation,
	const FString& RequestId,
	int32 DepthAtDispatch,
	bool bOnGameThread)
{
	FScopeLock Lock(&Observation.Mutex);
	FDispatchRecord Record;
	Record.RequestId = RequestId;
	Record.DepthAtDispatch = DepthAtDispatch;
	Record.bOnGameThread = bOnGameThread;
	Observation.Dispatches.Add(MoveTemp(Record));
}

/**
 * Judge one run.
 *
 *  RACE      - any dispatch that saw a depth other than exactly 1. A depth of 2
 *              is two editor mutations in flight; a depth of 0 means the
 *              dispatch ran outside the guarded region, which is the same
 *              invariant broken from the other side. Both are a race.
 *  OFF-THREAD- any dispatch that did not run on the game thread.
 *  DUPLICATE - the same request id dispatched more than once. One admitted
 *              request must produce at most one editor mutation.
 *  LOSS      - an admitted request that never dispatched. A queue that silently
 *              drops accepted work fails the "at most one terminal result" rule
 *              from the other direction, so it is detected here rather than
 *              being written off as "no duplicate, therefore fine".
 */
inline FLaneVerdict Judge(
	const FLaneObservation& Observation,
	const TArray<FString>& AdmittedRequestIds)
{
	FLaneVerdict Verdict;
	TSet<FString> Seen;
	{
		FScopeLock Lock(&Observation.Mutex);
		for (const FDispatchRecord& Record : Observation.Dispatches)
		{
			if (Record.DepthAtDispatch != 1)
			{
				Verdict.bRaceDetected = true;
			}
			if (!Record.bOnGameThread)
			{
				Verdict.bOffThreadDetected = true;
			}
			bool bAlreadySeen = false;
			Seen.Add(Record.RequestId, &bAlreadySeen);
			if (bAlreadySeen)
			{
				Verdict.bDuplicateDetected = true;
			}
		}
	}
	for (const FString& Admitted : AdmittedRequestIds)
	{
		if (!Seen.Contains(Admitted))
		{
			Verdict.bLossDetected = true;
		}
	}
	return Verdict;
}

/** Terminal-path drain verdict. A terminal path (cancel, stale refusal,
 * timeout, disconnect, shutdown) must leave NOTHING behind: the plan's wording
 * is "maps/queues/tasks/subscriptions/ledgers/sockets drain on every terminal
 * path". Each residue is reported by name so a leak identifies its own map. */
struct FDrainVerdict
{
	TArray<FString> Residue;

	bool IsDrained() const
	{
		return Residue.Num() == 0;
	}
};

/** Records one named container's occupancy. Called once per map so the verdict
 * lists exactly which container leaked rather than "something leaked". */
inline void RecordResidue(
	FDrainVerdict& Verdict,
	const TCHAR* ContainerName,
	int32 RemainingNum)
{
	if (RemainingNum != 0)
	{
		Verdict.Residue.Add(
			FString::Printf(TEXT("%s=%d"), ContainerName, RemainingNum));
	}
}
}  // namespace McpLaneOracle
