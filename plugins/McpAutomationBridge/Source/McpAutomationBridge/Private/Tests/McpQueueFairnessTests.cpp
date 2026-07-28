#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Foundation/McpLiveStateRevisions.h"
#include "Misc/AutomationTest.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

namespace McpQueueFairnessTestUtils
{
static const TMap<EMcpStateKind, int64> NoRevisions;

static UMcpAutomationBridgeSubsystem* MakeSubsystemRecording(
	const TCHAR* Action,
	TArray<FString>& OutDispatchOrder)
{
	UMcpAutomationBridgeSubsystem* Subsystem =
		NewObject<UMcpAutomationBridgeSubsystem>();
	Subsystem->RegisterHandler(
		Action,
		[&OutDispatchOrder](
			const FString& RequestId, const FString&,
			const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>)
		{
			OutDispatchOrder.Add(RequestId);
			return true;
		});
	return Subsystem;
}

static EAutomationQueueRejection Queue(
	UMcpAutomationBridgeSubsystem* Subsystem,
	const FString& RequestId,
	const TCHAR* Action,
	const FString& SessionKey)
{
	return Subsystem->QueueAutomationRequest(
		RequestId, Action, MakeShared<FJsonObject>(), nullptr,
		ERequestOrigin::WebSocket, NoRevisions, SessionKey);
}
}  // namespace McpQueueFairnessTestUtils

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpQueueFairnessNoStarvationTest,
	"McpAutomationBridge.Core.RequestQueue.Fairness.NoSessionStarvation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpQueueFairnessNoStarvationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpQueueFairnessTestUtils;
	const TCHAR* Action = TEXT("task45_fairness_fixture");
	TArray<FString> DispatchOrder;
	UMcpAutomationBridgeSubsystem* Subsystem =
		MakeSubsystemRecording(Action, DispatchOrder);

	// Session A floods the queue up to its per-session cap, THEN session B
	// enqueues a single request behind all of it. Under the strict-FIFO queue
	// this replaces, B sat at index 16 and was not reachable within the 16
	// requests one drain dispatches: A's entire backlog had to clear first.
	const int32 FloodNum = FMcpQueueFairnessState::MaxPendingRequestsPerSession;
	for (int32 Index = 0; Index < FloodNum; ++Index)
	{
		Queue(Subsystem, FString::Printf(TEXT("a-%d"), Index), Action,
			TEXT("session-a"));
	}
	TestEqual(TEXT("session B is admitted behind a flooding session"),
		Queue(Subsystem, TEXT("b-0"), Action, TEXT("session-b")),
		EAutomationQueueRejection::None);

	Subsystem->ProcessPendingAutomationRequests();

	const int32 ServedAt = DispatchOrder.IndexOfByKey(FString(TEXT("b-0")));
	TestTrue(TEXT("session B is dispatched in the very first drain"),
		ServedAt != INDEX_NONE);
	// The bound is the number of sessions holding queued work, not the depth of
	// the flood: B waits behind at most one request per competing session.
	TestTrue(TEXT("session B waits at most one dequeue per competing session"),
		ServedAt >= 0 && ServedAt <= 1);

	TArray<FString> SessionAOrder;
	for (const FString& RequestId : DispatchOrder)
	{
		if (RequestId.StartsWith(TEXT("a-")))
		{
			SessionAOrder.Add(RequestId);
		}
	}
	TestEqual(TEXT("fairness preserves FIFO within a session (first)"),
		SessionAOrder[0], FString(TEXT("a-0")));
	TestEqual(TEXT("fairness preserves FIFO within a session (second)"),
		SessionAOrder[1], FString(TEXT("a-1")));
	TestEqual(TEXT("a full batch is still drained per tick"),
		DispatchOrder.Num(),
		UMcpAutomationBridgeSubsystem::MaxAutomationRequestsPerTick);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpQueueFairnessRotationTest,
	"McpAutomationBridge.Core.RequestQueue.Fairness.RotationAcrossTicks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpQueueFairnessRotationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpQueueFairnessTestUtils;
	const TCHAR* Action = TEXT("task45_rotation_fixture");
	TArray<FString> DispatchOrder;
	UMcpAutomationBridgeSubsystem* Subsystem =
		MakeSubsystemRecording(Action, DispatchOrder);

	// Three sessions, one request each, drained one at a time so every dequeue
	// is a fresh scheduling decision. Round-robin must visit all three before
	// revisiting any, which is the property that makes progress bounded rather
	// than merely likely.
	// Session a is enqueued twice up front on purpose: FIFO would dispatch its
	// second request before session b and c had run at all, so this order is
	// what makes the assertions below discriminate round-robin from FIFO.
	Queue(Subsystem, TEXT("a-0"), Action, TEXT("session-a"));
	Queue(Subsystem, TEXT("a-1"), Action, TEXT("session-a"));
	Queue(Subsystem, TEXT("b-0"), Action, TEXT("session-b"));
	Queue(Subsystem, TEXT("c-0"), Action, TEXT("session-c"));

	Subsystem->ProcessPendingAutomationRequests();

	TestEqual(TEXT("every session is served before any session repeats"),
		DispatchOrder.Num(), 4);
	TestEqual(TEXT("round 1 serves session a"), DispatchOrder[0],
		FString(TEXT("a-0")));
	TestEqual(TEXT("round 1 serves session b"), DispatchOrder[1],
		FString(TEXT("b-0")));
	TestEqual(TEXT("round 1 serves session c"), DispatchOrder[2],
		FString(TEXT("c-0")));
	TestEqual(TEXT("session a repeats only in round 2"), DispatchOrder[3],
		FString(TEXT("a-1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpQueueFairnessCapsTest,
	"McpAutomationBridge.Core.RequestQueue.Fairness.PendingCaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpQueueFairnessCapsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpQueueFairnessTestUtils;
	const TCHAR* Action = TEXT("task45_caps_fixture");
	TArray<FString> DispatchOrder;
	UMcpAutomationBridgeSubsystem* Subsystem =
		MakeSubsystemRecording(Action, DispatchOrder);

	const int32 SessionCap = FMcpQueueFairnessState::MaxPendingRequestsPerSession;
	bool bAllAdmitted = true;
	for (int32 Index = 0; Index < SessionCap; ++Index)
	{
		bAllAdmitted &= Queue(Subsystem,
			FString::Printf(TEXT("a-%d"), Index), Action, TEXT("session-a")) ==
			EAutomationQueueRejection::None;
	}
	TestTrue(TEXT("a session is admitted up to its cap"), bAllAdmitted);
	TestEqual(TEXT("crossing the per-session cap is a typed refusal"),
		Queue(Subsystem, TEXT("a-over"), Action, TEXT("session-a")),
		EAutomationQueueRejection::SessionQueueFull);
	TestEqual(TEXT("a capped session does not block a different session"),
		Queue(Subsystem, TEXT("b-0"), Action, TEXT("session-b")),
		EAutomationQueueRejection::None);

	// Fill the remaining global slots with further distinct sessions, so the
	// next request is refused by the GLOBAL cap and reports that distinctly.
	const int32 GlobalCap =
		UMcpAutomationBridgeSubsystem::MaxPendingAutomationRequests;
	int32 QueuedNum = SessionCap + 1;
	for (int32 Session = 2; QueuedNum < GlobalCap; ++Session)
	{
		for (int32 Index = 0; Index < SessionCap && QueuedNum < GlobalCap;
			 ++Index, ++QueuedNum)
		{
			Queue(Subsystem, FString::Printf(TEXT("s%d-%d"), Session, Index),
				Action, FString::Printf(TEXT("session-%d"), Session));
		}
	}
	TestEqual(TEXT("crossing the global cap is a typed refusal"),
		Queue(Subsystem, TEXT("z-0"), Action, TEXT("session-z")),
		EAutomationQueueRejection::QueueFull);

	// A refusal must be a refusal, not an accepted-then-dropped request: the
	// over-cap ids must never reach a handler no matter how long we drain.
	for (int32 Drain = 0; Drain < 8; ++Drain)
	{
		Subsystem->ProcessPendingAutomationRequests();
	}
	TestFalse(TEXT("a per-session refusal is never silently dispatched"),
		DispatchOrder.Contains(FString(TEXT("a-over"))));
	TestFalse(TEXT("a global refusal is never silently dispatched"),
		DispatchOrder.Contains(FString(TEXT("z-0"))));
	TestEqual(TEXT("every admitted request is dispatched exactly once"),
		DispatchOrder.Num(), GlobalCap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpQueueSingleMutationLaneTest,
	"McpAutomationBridge.Core.RequestQueue.Fairness.SingleMutationLane",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpQueueSingleMutationLaneTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpQueueFairnessTestUtils;
	const TCHAR* Action = TEXT("task45_lane_fixture");
	TArray<FString> DispatchOrder;
	UMcpAutomationBridgeSubsystem* Subsystem =
		NewObject<UMcpAutomationBridgeSubsystem>();
	int32 ConcurrentDispatches = 0;
	int32 MaxConcurrentDispatches = 0;

	// The handler re-enters the drain from INSIDE its own dispatch, which is
	// what a handler that pumps the game thread (Slate tick, modal-free wait)
	// does in production. AutomationRequestExecutionMutex is recursive and
	// would let that second drain through, so this is the exact shape that
	// would open a second editor mutation lane.
	Subsystem->RegisterHandler(
		Action,
		[&](const FString& RequestId, const FString&,
			const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>)
		{
			DispatchOrder.Add(RequestId);
			++ConcurrentDispatches;
			MaxConcurrentDispatches =
				FMath::Max(MaxConcurrentDispatches, ConcurrentDispatches);
			Subsystem->ProcessPendingAutomationRequests();
			--ConcurrentDispatches;
			return true;
		});

	// More than one batch must stay queued, otherwise the re-entrant drain
	// finds an empty queue and proves nothing: a batch is removed from
	// PendingAutomationRequests BEFORE the first request of it is dispatched.
	const int32 SessionCap = FMcpQueueFairnessState::MaxPendingRequestsPerSession;
	for (int32 Index = 0; Index < SessionCap; ++Index)
	{
		Queue(Subsystem, FString::Printf(TEXT("a-%d"), Index), Action,
			TEXT("session-a"));
		Queue(Subsystem, FString::Printf(TEXT("b-%d"), Index), Action,
			TEXT("session-b"));
	}
	const int32 QueuedNum = SessionCap * 2;

	Subsystem->ProcessPendingAutomationRequests();
	Subsystem->ProcessPendingAutomationRequests();

	TestEqual(TEXT("no two handlers are ever in flight at once"),
		MaxConcurrentDispatches, 1);
	TestEqual(TEXT("the queue's own lane counter never exceeds one"),
		Subsystem->QueueFairness.MaxObservedDispatchDepth, 1);
	TestEqual(TEXT("the lane depth unwinds to zero"),
		Subsystem->QueueFairness.DispatchDepth, 0);
	TestEqual(TEXT("a re-entrant drain neither drops nor delays work"),
		DispatchOrder.Num(), QueuedNum);
	TestEqual(TEXT("a re-entrant drain never dispatches a request twice"),
		TSet<FString>(DispatchOrder).Num(), QueuedNum);
	return true;
}

#endif
