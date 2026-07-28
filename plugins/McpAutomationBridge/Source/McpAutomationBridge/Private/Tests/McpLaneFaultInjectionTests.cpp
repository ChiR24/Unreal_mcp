#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Async/Async.h"
#include "Foundation/McpIdempotencyLedger.h"
#include "Misc/AutomationTest.h"
#include "Tests/McpLaneOracle.h"

// Task 46 gate - PROOF THAT THE GATE CAN FAIL.
//
// The acceptance criterion is "injected race, stale state, unauthorized action
// or duplicate mutation is detected". A green concurrency run does not
// establish that: an oracle that returns "clean" unconditionally would produce
// exactly the same green. So every predicate the Task 46 native tests judge a
// run with is driven RED here, deliberately, from a hand-built observation
// carrying the corresponding fault.
//
// These are NOT tests of the subsystem. They are tests OF THE DETECTOR, and
// they are what makes the subsystem's green run mean something. If a future
// change weakens McpLaneOracle::Judge (say, by only comparing dispatch counts),
// the matching case below stops going red and this file fails.
namespace McpLaneFaultInjection
{
static McpLaneOracle::FDispatchRecord Record(
	const TCHAR* RequestId,
	int32 Depth,
	bool bOnGameThread)
{
	McpLaneOracle::FDispatchRecord Out;
	Out.RequestId = RequestId;
	Out.DepthAtDispatch = Depth;
	Out.bOnGameThread = bOnGameThread;
	return Out;
}

/** A clean baseline: one request, depth 1, on the game thread, admitted and
 * dispatched. Every injection below is this observation with ONE fault added,
 * so a red verdict can only be caused by that fault. */
static void SeedClean(McpLaneOracle::FLaneObservation& Observation)
{
	Observation.Dispatches.Add(Record(TEXT("clean-0"), 1, true));
}
}  // namespace McpLaneFaultInjection

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpLaneOracleBaselineTest,
	"McpAutomationBridge.Core.FaultInjection.CleanObservationIsAccepted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpLaneOracleBaselineTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpLaneFaultInjection;
	// Without this, every red below is consistent with an oracle that fails
	// everything - which detects nothing while looking maximally strict.
	McpLaneOracle::FLaneObservation Observation;
	SeedClean(Observation);
	const McpLaneOracle::FLaneVerdict Verdict = McpLaneOracle::Judge(
		Observation, TArray<FString>({TEXT("clean-0")}));
	TestTrue(TEXT("a clean observation is accepted"), Verdict.IsClean());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpLaneOracleInjectedRaceTest,
	"McpAutomationBridge.Core.FaultInjection.InjectedRaceIsDetected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpLaneOracleInjectedRaceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpLaneFaultInjection;
	// Depth 2 is the EXACT shape Task 45 measured when the drain guard was
	// removed: a second dispatch opened inside the first one's execution
	// because FCriticalSection is recursive. This is the historical defect,
	// replayed as an observation.
	McpLaneOracle::FLaneObservation Observation;
	SeedClean(Observation);
	Observation.Dispatches.Add(Record(TEXT("race-1"), 2, true));
	const McpLaneOracle::FLaneVerdict Verdict = McpLaneOracle::Judge(
		Observation, TArray<FString>({TEXT("clean-0"), TEXT("race-1")}));
	TestTrue(TEXT("a second concurrent mutation lane is detected"),
		Verdict.bRaceDetected);
	TestFalse(TEXT("a race is not misreported as a duplicate"),
		Verdict.bDuplicateDetected);
	TestFalse(TEXT("a race is not misreported as a loss"),
		Verdict.bLossDetected);

	// The other side of the same invariant: a dispatch that ran with depth 0
	// executed OUTSIDE the guarded region, so it was never on the single lane
	// at all. An oracle that only looked for depth > 1 would pass this.
	McpLaneOracle::FLaneObservation Unguarded;
	Unguarded.Dispatches.Add(Record(TEXT("unguarded-0"), 0, true));
	TestTrue(TEXT("a dispatch outside the guarded region is detected"),
		McpLaneOracle::Judge(
			Unguarded, TArray<FString>({TEXT("unguarded-0")}))
			.bRaceDetected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpLaneOracleInjectedOffThreadTest,
	"McpAutomationBridge.Core.FaultInjection.InjectedOffThreadMutationIsDetected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpLaneOracleInjectedOffThreadTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpLaneFaultInjection;
	McpLaneOracle::FLaneObservation Observation;
	SeedClean(Observation);
	// Depth 1 and unique - every criterion satisfied EXCEPT the thread. This is
	// the "editor mutation that did not enter through the game thread" case.
	Observation.Dispatches.Add(Record(TEXT("offthread-1"), 1, false));
	const McpLaneOracle::FLaneVerdict Verdict = McpLaneOracle::Judge(
		Observation, TArray<FString>({TEXT("clean-0"), TEXT("offthread-1")}));
	TestTrue(TEXT("an off-game-thread editor mutation is detected"),
		Verdict.bOffThreadDetected);
	TestFalse(TEXT("an off-thread dispatch is not misreported as a race"),
		Verdict.bRaceDetected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpLaneOracleInjectedDuplicateTest,
	"McpAutomationBridge.Core.FaultInjection.InjectedDuplicateMutationIsDetected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpLaneOracleInjectedDuplicateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpLaneFaultInjection;
	McpLaneOracle::FLaneObservation Observation;
	SeedClean(Observation);
	// One admitted request, dispatched twice: the editor mutated twice for one
	// client call. Depth stays 1 on both, so this is invisible to the lane
	// counter alone - it is caught only because identity is tracked.
	Observation.Dispatches.Add(Record(TEXT("clean-0"), 1, true));
	const McpLaneOracle::FLaneVerdict Verdict = McpLaneOracle::Judge(
		Observation, TArray<FString>({TEXT("clean-0")}));
	TestTrue(TEXT("one request dispatched twice is detected"),
		Verdict.bDuplicateDetected);
	TestFalse(TEXT("a duplicate is not misreported as a race"),
		Verdict.bRaceDetected);

	// And the opposite terminal failure: an admitted request that never ran.
	McpLaneOracle::FLaneObservation Lost;
	SeedClean(Lost);
	TestTrue(TEXT("an admitted request that never dispatched is detected"),
		McpLaneOracle::Judge(
			Lost, TArray<FString>({TEXT("clean-0"), TEXT("dropped-1")}))
			.bLossDetected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDrainOracleInjectedLeakTest,
	"McpAutomationBridge.Core.FaultInjection.InjectedDrainLeakIsDetected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDrainOracleInjectedLeakTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	McpLaneOracle::FDrainVerdict Clean;
	McpLaneOracle::RecordResidue(Clean, TEXT("Pending"), 0);
	McpLaneOracle::RecordResidue(Clean, TEXT("InFlight"), 0);
	TestTrue(TEXT("a fully drained terminal path is accepted"),
		Clean.IsDrained());

	McpLaneOracle::FDrainVerdict Leaked;
	McpLaneOracle::RecordResidue(Leaked, TEXT("Pending"), 0);
	McpLaneOracle::RecordResidue(Leaked, TEXT("CanceledIds"), 1);
	TestFalse(TEXT("a single retained id fails the drain gate"),
		Leaked.IsDrained());
	TestEqual(TEXT("the leaking container names itself"),
		Leaked.Residue.Num(), 1);
	TestEqual(TEXT("the residue reports container and count"),
		Leaked.Residue[0], FString(TEXT("CanceledIds=1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpLedgerConcurrentDuplicateTest,
	"McpAutomationBridge.Foundation.IdempotencyLedger.ConcurrentDuplicateAdmitsExactlyOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpLedgerConcurrentDuplicateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// The REAL duplicate-mutation defence, exercised by a REAL race rather than
	// by sequential calls. Sequential Begin/Begin proves the map works; only
	// concurrent Begin proves the claim under the condition the ledger exists
	// for - two clients (or one retrying client) arriving at the same instant.
	FMcpIdempotencyLedger& Ledger = FMcpIdempotencyLedger::Get();
	Ledger.Reset();
	constexpr int32 ThreadNum = 8;
	std::atomic<int32> FirstCount{0};
	std::atomic<int32> RefusedCount{0};
	TArray<TFuture<void>> Racers;
	Racers.Reserve(ThreadNum);
	for (int32 Index = 0; Index < ThreadNum; ++Index)
	{
		Racers.Add(Async(
			EAsyncExecution::Thread,
			[&FirstCount, &RefusedCount, &Ledger]()
			{
				FString Slot;
				FString Replay;
				const EMcpIdempotencyOutcome Outcome = Ledger.Begin(
					TEXT("task46-principal"), TEXT("control_actor.spawn"),
					TEXT("task46-duplicate-key"), TEXT("fingerprint-a"), Slot,
					Replay);
				if (Outcome == EMcpIdempotencyOutcome::First)
				{
					FirstCount.fetch_add(1, std::memory_order_relaxed);
				}
				else
				{
					RefusedCount.fetch_add(1, std::memory_order_relaxed);
				}
			}));
	}
	for (TFuture<void>& Racer : Racers)
	{
		Racer.Wait();
	}

	TestEqual(TEXT("exactly one racer owns the mutation slot"),
		FirstCount.load(), 1);
	TestEqual(TEXT("every other racer is refused as a duplicate"),
		RefusedCount.load(), ThreadNum - 1);
	Ledger.Reset();
	return true;
}

#endif
