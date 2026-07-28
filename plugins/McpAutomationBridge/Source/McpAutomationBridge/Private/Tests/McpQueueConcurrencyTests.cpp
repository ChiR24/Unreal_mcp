#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/McpLaneOracle.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

// Task 46 gate - CONCURRENCY half of "prove all editor mutations enter through
// the subsystem queue / game thread".
//
// Task 45 already proved the RE-ENTRANT case (one thread, a handler that pumps
// the drain from inside its own dispatch). That is not the same hazard as real
// threads pushing work in while the drain runs, and the two fail differently:
// re-entrancy defeats a recursive FCriticalSection, contention defeats a
// missing one. This file covers the second, and it judges the run with the
// SHARED oracle in McpLaneOracle.h so the verdict is the same predicate the
// injected-fault tests are proven to fail.
namespace McpQueueConcurrencyTestUtils
{
static const TMap<EMcpStateKind, int64> NoRevisions;

/** Worker fan-out. 4 sessions x 8 requests stays under the per-session cap of
 * 16, so every enqueue below is expected to be ADMITTED - a refusal here would
 * be a cap artifact rather than the contention this test is aiming at. */
constexpr int32 WorkerThreadNum = 4;
constexpr int32 RequestsPerWorker = 8;

/** Collects what each dispatch actually observed. Written only from inside a
 * dispatch (game thread, single lane), read after the drain completes. */
struct FDispatchLog
{
	FCriticalSection Mutex;
	TArray<FString> AdmittedIds;
	McpLaneOracle::FLaneObservation Observation;
};

static void RunWorkers(
	UMcpAutomationBridgeSubsystem* Subsystem,
	const TCHAR* Action,
	FDispatchLog& Log,
	TFunction<void()> GameThreadPump)
{
	TArray<TFuture<void>> Workers;
	Workers.Reserve(WorkerThreadNum);
	for (int32 Worker = 0; Worker < WorkerThreadNum; ++Worker)
	{
		Workers.Add(Async(
			EAsyncExecution::Thread,
			[Subsystem, Action, &Log, Worker]()
			{
				const FString SessionKey =
					FString::Printf(TEXT("session-%d"), Worker);
				for (int32 Index = 0; Index < RequestsPerWorker; ++Index)
				{
					const FString RequestId =
						FString::Printf(TEXT("w%d-r%d"), Worker, Index);
					const EAutomationQueueRejection Rejection =
						Subsystem->QueueAutomationRequest(
							RequestId, Action, MakeShared<FJsonObject>(),
							nullptr, ERequestOrigin::WebSocket, NoRevisions,
							SessionKey);
					if (Rejection == EAutomationQueueRejection::None)
					{
						FScopeLock Lock(&Log.Mutex);
						Log.AdmittedIds.Add(RequestId);
					}
				}
			}));
	}
	// The game thread keeps draining WHILE the workers are still enqueueing.
	// That overlap is the point: a drain that only ran after every worker had
	// joined would never exercise the contended path at all.
	while (GameThreadPump)
	{
		GameThreadPump();
		bool bAllDone = true;
		for (const TFuture<void>& Worker : Workers)
		{
			bAllDone &= Worker.IsReady();
		}
		if (bAllDone)
		{
			break;
		}
	}
	for (TFuture<void>& Worker : Workers)
	{
		Worker.Wait();
	}
}
}  // namespace McpQueueConcurrencyTestUtils

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpQueueConcurrentEnqueueLaneTest,
	"McpAutomationBridge.Core.RequestQueue.Concurrency.ConcurrentEnqueueHoldsSingleGameThreadLane",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpQueueConcurrentEnqueueLaneTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpQueueConcurrencyTestUtils;
	const TCHAR* Action = TEXT("task46_concurrency_fixture");
	FDispatchLog Log;
	UMcpAutomationBridgeSubsystem* Subsystem =
		NewObject<UMcpAutomationBridgeSubsystem>();
	Subsystem->AddToRoot();
	ON_SCOPE_EXIT { Subsystem->RemoveFromRoot(); };

	Subsystem->RegisterHandler(
		Action,
		[Subsystem, &Log](
			const FString& RequestId, const FString&,
			const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>)
		{
			// Read the depth the QUEUE ITSELF is holding, not a counter this
			// test maintains. A test-local counter would only prove the test
			// is consistent with itself; this proves the production guard.
			McpLaneOracle::RecordDispatch(
				Log.Observation, RequestId,
				Subsystem->QueueFairness.DispatchDepth, IsInGameThread());
			return true;
		});

	RunWorkers(
		Subsystem, Action, Log,
		[Subsystem]() { Subsystem->ProcessPendingAutomationRequests(); });
	// Workers may have enqueued after the final pump above; drain to quiescence.
	for (int32 Drain = 0; Drain < 8; ++Drain)
	{
		Subsystem->ProcessPendingAutomationRequests();
	}

	const int32 ExpectedNum = WorkerThreadNum * RequestsPerWorker;
	TestEqual(TEXT("every concurrent enqueue is admitted under the caps"),
		Log.AdmittedIds.Num(), ExpectedNum);

	const McpLaneOracle::FLaneVerdict Verdict =
		McpLaneOracle::Judge(Log.Observation, Log.AdmittedIds);
	TestFalse(TEXT("no concurrent editor mutation lane opened"),
		Verdict.bRaceDetected);
	TestFalse(TEXT("no editor mutation ran off the game thread"),
		Verdict.bOffThreadDetected);
	TestFalse(TEXT("no request was dispatched twice"),
		Verdict.bDuplicateDetected);
	TestFalse(TEXT("no admitted request was lost"), Verdict.bLossDetected);
	TestTrue(TEXT("the concurrent run is clean by every lane criterion"),
		Verdict.IsClean());

	// The queue's own high-water mark, asserted separately from the oracle so a
	// bug in the oracle cannot hide a bug in the queue (and vice versa).
	TestEqual(TEXT("the queue's lane counter never exceeded one"),
		Subsystem->QueueFairness.MaxObservedDispatchDepth, 1);
	TestEqual(TEXT("the lane depth unwinds to zero"),
		Subsystem->QueueFairness.DispatchDepth, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpQueueOffThreadDrainTest,
	"McpAutomationBridge.Core.RequestQueue.Concurrency.OffThreadDrainMarshalsToGameThread",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpQueueOffThreadDrainTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpQueueConcurrencyTestUtils;
	const TCHAR* Action = TEXT("task46_offthread_fixture");
	FDispatchLog Log;
	UMcpAutomationBridgeSubsystem* Subsystem =
		NewObject<UMcpAutomationBridgeSubsystem>();
	Subsystem->AddToRoot();
	ON_SCOPE_EXIT { Subsystem->RemoveFromRoot(); };

	Subsystem->RegisterHandler(
		Action,
		[Subsystem, &Log](
			const FString& RequestId, const FString&,
			const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>)
		{
			McpLaneOracle::RecordDispatch(
				Log.Observation, RequestId,
				Subsystem->QueueFairness.DispatchDepth, IsInGameThread());
			return true;
		});
	Subsystem->QueueAutomationRequest(
		TEXT("offthread-0"), Action, MakeShared<FJsonObject>(), nullptr,
		ERequestOrigin::WebSocket, NoRevisions, TEXT("session-offthread"));

	// A worker thread asks for a drain. ProcessPendingAutomationRequests must
	// bounce that to the game thread rather than dispatching where it stands -
	// this is the entry point every transport reaches the editor through.
	Async(
		EAsyncExecution::Thread,
		[Subsystem]() { Subsystem->ProcessPendingAutomationRequests(); })
		.Wait();

	TestEqual(TEXT("a worker thread dispatches nothing itself"),
		Log.Observation.Dispatches.Num(), 0);
	TestTrue(TEXT("the request is still queued, not lost"),
		Subsystem->PendingAutomationRequests.Num() == 1);

	// Pump the game thread so the marshalled task actually lands, then judge it.
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

	TestEqual(TEXT("the marshalled drain dispatched the request exactly once"),
		Log.Observation.Dispatches.Num(), 1);
	const McpLaneOracle::FLaneVerdict Verdict = McpLaneOracle::Judge(
		Log.Observation, TArray<FString>({TEXT("offthread-0")}));
	TestFalse(TEXT("the marshalled dispatch ran on the game thread"),
		Verdict.bOffThreadDetected);
	TestTrue(TEXT("the marshalled drain is clean by every lane criterion"),
		Verdict.IsClean());
	return true;
}

#endif
