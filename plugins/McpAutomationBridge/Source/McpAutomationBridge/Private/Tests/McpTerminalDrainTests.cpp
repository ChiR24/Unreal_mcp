#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Foundation/McpIdempotencyLedger.h"
#include "Foundation/McpLiveStateRevisions.h"
#include "MCP/Primitives/McpSubscriptionStore.h"
#include "MCP/Primitives/McpTaskStore.h"
#include "McpConnectionManager.h"
#include "Misc/AutomationTest.h"
#include "Tests/McpLaneOracle.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

// Task 46 gate - "maps/queues/tasks/subscriptions/ledgers/sockets drain on
// EVERY terminal path".
//
// The existing shutdown test proves the cancellation CALLBACK runs. That is a
// different claim from "nothing was retained": a queue can run every callback
// and still leave a request id in a set, and the residue is invisible to any
// assertion about behaviour. So each test below drives one terminal path and
// then reads every container the request could have touched, by name, through
// the shared drain oracle. A leak names the container it leaked from.
//
// The ugly paths are the point. A request that completes normally is the easy
// case; the ones that historically retain state are cancel-while-queued,
// cancel-while-in-flight, a pre-dispatch refusal, and shutdown.
namespace McpTerminalDrain
{
static const TMap<EMcpStateKind, int64> NoRevisions;

/** Reads every container the automation queue can retain a request in. Called
 * only after a terminal path has completed, so any non-zero count is residue. */
static McpLaneOracle::FDrainVerdict SnapshotQueue(
	UMcpAutomationBridgeSubsystem* Subsystem)
{
	McpLaneOracle::FDrainVerdict Verdict;
	FScopeLock Lock(&Subsystem->PendingAutomationRequestsMutex);
	McpLaneOracle::RecordResidue(
		Verdict, TEXT("PendingAutomationRequests"),
		Subsystem->PendingAutomationRequests.Num());
	McpLaneOracle::RecordResidue(
		Verdict, TEXT("InFlightAutomationRequestIds"),
		Subsystem->InFlightAutomationRequestIds.Num());
	McpLaneOracle::RecordResidue(
		Verdict, TEXT("ActiveAutomationRequestIds"),
		Subsystem->ActiveAutomationRequestIds.Num());
	McpLaneOracle::RecordResidue(
		Verdict, TEXT("CanceledAutomationRequestIds"),
		Subsystem->CanceledAutomationRequestIds.Num());
	McpLaneOracle::RecordResidue(
		Verdict, TEXT("AutomationRequestCancellationCallbacks"),
		Subsystem->AutomationRequestCancellationCallbacks.Num());
	return Verdict;
}

static UMcpAutomationBridgeSubsystem* MakeSubsystem(
	const TCHAR* Action,
	TArray<FString>& OutDispatched)
{
	UMcpAutomationBridgeSubsystem* Subsystem =
		NewObject<UMcpAutomationBridgeSubsystem>();
	Subsystem->RegisterHandler(
		Action,
		[&OutDispatched](
			const FString& RequestId, const FString&,
			const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>)
		{
			OutDispatched.Add(RequestId);
			return true;
		});
	return Subsystem;
}
}  // namespace McpTerminalDrain

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDrainCancelQueuedTest,
	"McpAutomationBridge.Core.Lifecycle.Drain.CancelWhileQueued",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDrainCancelQueuedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpTerminalDrain;
	const TCHAR* Action = TEXT("task46_drain_cancel_queued");
	TArray<FString> Dispatched;
	UMcpAutomationBridgeSubsystem* Subsystem = MakeSubsystem(Action, Dispatched);

	Subsystem->QueueAutomationRequest(
		TEXT("drain-queued-0"), Action, MakeShared<FJsonObject>(), nullptr,
		ERequestOrigin::WebSocket, NoRevisions, TEXT("session-drain"));
	Subsystem->RegisterAutomationRequestCancellation(
		TEXT("drain-queued-0"), []() {});
	TestTrue(TEXT("cancelling a queued request does real work"),
		Subsystem->CancelAutomationRequest(TEXT("drain-queued-0")));
	Subsystem->ProcessPendingAutomationRequests();

	TestEqual(TEXT("a cancelled queued request never reaches the editor"),
		Dispatched.Num(), 0);
	const McpLaneOracle::FDrainVerdict Verdict = SnapshotQueue(Subsystem);
	TestTrue(
		FString::Printf(TEXT("cancel-while-queued drains every container (residue: %s)"),
			*FString::Join(Verdict.Residue, TEXT(","))),
		Verdict.IsDrained());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDrainCancelInFlightTest,
	"McpAutomationBridge.Core.Lifecycle.Drain.CancelWhileInFlight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDrainCancelInFlightTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpTerminalDrain;
	const TCHAR* Action = TEXT("task46_drain_cancel_inflight");
	TArray<FString> Dispatched;
	UMcpAutomationBridgeSubsystem* Subsystem =
		NewObject<UMcpAutomationBridgeSubsystem>();
	// The request cancels ITSELF from inside its own dispatch. That is the
	// advisory-cancellation window: the id is in InFlight AND Active, so the
	// cancel records a marker that only the post-dispatch cleanup can clear.
	// It is the one ordering where a retained marker would survive.
	Subsystem->RegisterHandler(
		Action,
		[Subsystem, &Dispatched](
			const FString& RequestId, const FString&,
			const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>)
		{
			Dispatched.Add(RequestId);
			Subsystem->CancelAutomationRequest(RequestId);
			return true;
		});

	Subsystem->QueueAutomationRequest(
		TEXT("drain-inflight-0"), Action, MakeShared<FJsonObject>(), nullptr,
		ERequestOrigin::WebSocket, NoRevisions, TEXT("session-drain"));
	Subsystem->ProcessPendingAutomationRequests();

	TestEqual(TEXT("in-flight work is not interrupted, only marked"),
		Dispatched.Num(), 1);
	const McpLaneOracle::FDrainVerdict Verdict = SnapshotQueue(Subsystem);
	TestTrue(
		FString::Printf(TEXT("cancel-while-in-flight drains every container (residue: %s)"),
			*FString::Join(Verdict.Residue, TEXT(","))),
		Verdict.IsDrained());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDrainStaleRefusalTest,
	"McpAutomationBridge.Core.Lifecycle.Drain.StaleStateRefusal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDrainStaleRefusalTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpTerminalDrain;
	const TCHAR* Action = TEXT("task46_drain_stale");
	TArray<FString> Dispatched;
	UMcpAutomationBridgeSubsystem* Subsystem = MakeSubsystem(Action, Dispatched);
	FMcpLiveStateRevisions& Revisions = FMcpLiveStateRevisions::Get();
	Revisions.Reset();

	// A refusal raised BETWEEN dequeue and dispatch: the id has already been
	// added to InFlight and Active by the batch selection, so this path has its
	// own cleanup that is easy to forget and invisible to a behaviour test.
	FMcpExpectedRevisions Expected;
	Expected.Add(
		EMcpStateKind::Selection, Revisions.Current(EMcpStateKind::Selection));
	Subsystem->QueueAutomationRequest(
		TEXT("drain-stale-0"), Action, MakeShared<FJsonObject>(), nullptr,
		ERequestOrigin::WebSocket, Expected, TEXT("session-drain"));
	Revisions.Advance(EMcpStateKind::Selection);
	Subsystem->ProcessPendingAutomationRequests();

	TestEqual(TEXT("a stale request never reaches the editor"),
		Dispatched.Num(), 0);
	const McpLaneOracle::FDrainVerdict Verdict = SnapshotQueue(Subsystem);
	TestTrue(
		FString::Printf(TEXT("a pre-dispatch refusal drains every container (residue: %s)"),
			*FString::Join(Verdict.Residue, TEXT(","))),
		Verdict.IsDrained());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDrainShutdownTest,
	"McpAutomationBridge.Core.Lifecycle.Drain.Shutdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDrainShutdownTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpTerminalDrain;
	const TCHAR* Action = TEXT("task46_drain_shutdown");
	TArray<FString> Dispatched;
	UMcpAutomationBridgeSubsystem* Subsystem = MakeSubsystem(Action, Dispatched);

	// Deinitialize() stops admission and then calls CancelAllAutomationRequests.
	// The admission half (a late request refused as NotAccepting) is already
	// pinned by Core.RequestQueue.ShutdownCancellation; what is NOT covered
	// there, and is asserted here, is that the cancel-everything half leaves no
	// residue in any container. Multiple sessions and a registered callback are
	// outstanding, so every map has something to leak.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Subsystem->QueueAutomationRequest(
			FString::Printf(TEXT("drain-shutdown-%d"), Index), Action,
			MakeShared<FJsonObject>(), nullptr, ERequestOrigin::WebSocket,
			NoRevisions, FString::Printf(TEXT("session-%d"), Index));
	}
	Subsystem->RegisterAutomationRequestCancellation(
		TEXT("drain-shutdown-async"), []() {});
	Subsystem->CancelAllAutomationRequests();
	Subsystem->ProcessPendingAutomationRequests();

	TestEqual(TEXT("shutdown dispatches no queued editor work"),
		Dispatched.Num(), 0);
	const McpLaneOracle::FDrainVerdict Verdict = SnapshotQueue(Subsystem);
	TestTrue(
		FString::Printf(TEXT("shutdown drains every container (residue: %s)"),
			*FString::Join(Verdict.Residue, TEXT(","))),
		Verdict.IsDrained());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDrainSessionTeardownTest,
	"McpAutomationBridge.Core.Lifecycle.Drain.SessionTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDrainSessionTeardownTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// The per-session half of the same criterion: tasks, subscriptions, sockets
	// and the ledger. Each is populated for TWO sessions and torn down for ONE,
	// so a teardown that over-drains (taking the neighbour's state with it) is
	// caught as well as one that under-drains.
	//
	// EVERY setup below is asserted before the drain is judged. A drain proof
	// over a container that was never populated is vacuous - it reads 0 both
	// before and after and passes against a teardown that does nothing at all.
	// The first version of this test subscribed to a URI outside the closed
	// subscribable allowlist, so both subscriptions were silently refused and
	// the "drained" assertion held over an empty store; only the neighbour
	// control caught it.
	FMcpTaskStore Tasks;
	FMcpTaskRecord Created;
	TestEqual(TEXT("task setup: session a has a task to drain"),
		Tasks.CreateTask(TEXT("s-a"), false, 0, Created),
		EMcpTaskStoreError::None);
	TestEqual(TEXT("task setup: session b has a task that must survive"),
		Tasks.CreateTask(TEXT("s-b"), false, 0, Created),
		EMcpTaskStoreError::None);
	TestEqual(TEXT("task setup: session a is non-empty before teardown"),
		Tasks.SessionSize(TEXT("s-a")), 1);
	Tasks.CloseSession(TEXT("s-a"));
	McpLaneOracle::FDrainVerdict Verdict;
	McpLaneOracle::RecordResidue(
		Verdict, TEXT("TaskStore"), Tasks.SessionSize(TEXT("s-a")));

	FMcpSubscriptionStore Subscriptions;
	TestTrue(TEXT("subscription setup: session a subscribe accepted"),
		Subscriptions.Subscribe(TEXT("s-a"), TEXT("ue://selection")).bAccepted);
	TestTrue(TEXT("subscription setup: session b subscribe accepted"),
		Subscriptions.Subscribe(TEXT("s-b"), TEXT("ue://selection")).bAccepted);
	TestEqual(TEXT("subscription setup: session a is non-empty before teardown"),
		Subscriptions.Count(TEXT("s-a")), 1);
	Subscriptions.ClearSession(TEXT("s-a"));
	McpLaneOracle::RecordResidue(
		Verdict, TEXT("SubscriptionStore"), Subscriptions.Count(TEXT("s-a")));
	McpLaneOracle::RecordResidue(
		Verdict, TEXT("SubscriptionSession"),
		Subscriptions.HasSession(TEXT("s-a")) ? 1 : 0);

	FMcpConnectionManager Manager;
	TSharedPtr<FMcpBridgeWebSocket> Socket = MakeShared<FMcpBridgeWebSocket>(0);
	Manager.RegisterRequestSocket(TEXT("teardown-req"), Socket);
	TestTrue(TEXT("socket setup: the request is tracked before teardown"),
		Manager.PendingRequestsToSockets.Contains(TEXT("teardown-req")));
	Manager.HandleClosed(Socket, 1000, TEXT("task46-teardown"), true);
	McpLaneOracle::RecordResidue(
		Verdict, TEXT("PendingRequestsToSockets"),
		Manager.PendingRequestsToSockets.Contains(TEXT("teardown-req")) ? 1 : 0);

	FMcpIdempotencyLedger& Ledger = FMcpIdempotencyLedger::Get();
	Ledger.Reset();
	FString Slot;
	FString Replay;
	TestEqual(TEXT("ledger setup: the slot is claimed before abandon"),
		Ledger.Begin(TEXT("s-a"), TEXT("control_actor.spawn"), TEXT("k"),
			TEXT("fp"), Slot, Replay),
		EMcpIdempotencyOutcome::First);
	TestEqual(TEXT("ledger setup: the entry exists before abandon"),
		Ledger.GetEntryCount(), 1);
	// Abandon is the failure terminal path: it must leave NOTHING, so the key
	// stays retryable rather than replaying a failure as a success.
	Ledger.Abandon(Slot);
	McpLaneOracle::RecordResidue(
		Verdict, TEXT("IdempotencyLedger"), Ledger.GetEntryCount());

	TestTrue(
		FString::Printf(TEXT("session teardown drains every container (residue: %s)"),
			*FString::Join(Verdict.Residue, TEXT(","))),
		Verdict.IsDrained());
	TestEqual(TEXT("teardown does not drain a neighbouring session's tasks"),
		Tasks.SessionSize(TEXT("s-b")), 1);
	TestEqual(TEXT("teardown does not drain a neighbouring subscription"),
		Subscriptions.Count(TEXT("s-b")), 1);
	return true;
}

#endif
