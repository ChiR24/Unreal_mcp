// McpTaskStoreTests.cpp — Task 44: the native half of the bounded task store.
//
// Mirrors tests/unit/task-44/bounded-task-store.test.ts. Every temporal
// assertion advances an injected fake clock; nothing here sleeps, so an expiry
// case cannot pass by being slow or fail by being fast.

#include "MCP/Primitives/McpTaskMethods.h"
#include "MCP/Primitives/McpTaskStore.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
struct FFakeClock
{
	TSharedRef<int64> Now = MakeShared<int64>(1000000);
	TFunction<int64()> Fn() const
	{
		TSharedRef<int64> Ref = Now;
		return [Ref]() -> int64 { return *Ref; };
	}
	void Advance(int64 Ms) const { *Now += Ms; }
};

FMcpTaskStoreConfig SmallConfig(int32 Cap, int64 Ttl = 60000)
{
	FMcpTaskStoreConfig Config;
	Config.MaxTasksPerSession = Cap;
	Config.DefaultTtlMs = Ttl;
	Config.MaxTtlMs = 300000;
	return Config;
}

FString CreateFinished(FMcpTaskStore& Store, const FString& Session)
{
	FMcpTaskRecord Record;
	if (Store.CreateTask(Session, false, 0, Record) != EMcpTaskStoreError::None) return FString();
	Store.StoreResult(Session, Record.TaskId, EMcpTaskStatus::Completed, MakeShared<FJsonObject>());
	return Record.TaskId;
}

bool Exists(FMcpTaskStore& Store, const FString& Session, const FString& TaskId)
{
	FMcpTaskRecord Record;
	return Store.GetTask(Session, TaskId, Record) == EMcpTaskStoreError::None;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpTaskStoreCapacityTest,
	"McpAutomationBridge.Primitives.TaskStore.Capacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpTaskStoreCapacityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FFakeClock Clock;
	FMcpTaskStore Store(SmallConfig(3));
	Store.SetClock(Clock.Fn());

	const FString First = CreateFinished(Store, TEXT("a"));
	const FString Second = CreateFinished(Store, TEXT("a"));
	const FString Third = CreateFinished(Store, TEXT("a"));
	const FString Fourth = CreateFinished(Store, TEXT("a"));

	TestEqual(TEXT("cap is never exceeded"), Store.SessionSize(TEXT("a")), 3);
	TestFalse(TEXT("oldest terminal task is evicted first"), Exists(Store, TEXT("a"), First));
	TestTrue(TEXT("second survives the first eviction"), Exists(Store, TEXT("a"), Second));
	TestTrue(TEXT("newest survives"), Exists(Store, TEXT("a"), Fourth));

	CreateFinished(Store, TEXT("a"));
	TestFalse(TEXT("eviction order is insertion order"), Exists(Store, TEXT("a"), Second));
	TestTrue(TEXT("third still survives"), Exists(Store, TEXT("a"), Third));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpTaskStoreNeverEvictsLiveTest,
	"McpAutomationBridge.Primitives.TaskStore.NeverEvictsLive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpTaskStoreNeverEvictsLiveTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FFakeClock Clock;
	FMcpTaskStore Store(SmallConfig(2));
	Store.SetClock(Clock.Fn());

	FMcpTaskRecord Live;
	FMcpTaskRecord Second;
	Store.CreateTask(TEXT("a"), false, 0, Live);
	Store.CreateTask(TEXT("a"), false, 0, Second);

	FMcpTaskRecord Refused;
	TestEqual(TEXT("refuses rather than evicting a running task"),
		Store.CreateTask(TEXT("a"), false, 0, Refused), EMcpTaskStoreError::AtCapacity);
	TestTrue(TEXT("the live handle survives the refusal"), Exists(Store, TEXT("a"), Live.TaskId));
	TestEqual(TEXT("nothing was added"), Store.SessionSize(TEXT("a")), 2);

	// A terminal neighbour is evicted in preference to refusing, even when the
	// running task is older.
	Store.StoreResult(TEXT("a"), Second.TaskId, EMcpTaskStatus::Completed, MakeShared<FJsonObject>());
	FMcpTaskRecord Fresh;
	TestEqual(TEXT("terminal neighbour makes room"),
		Store.CreateTask(TEXT("a"), false, 0, Fresh), EMcpTaskStoreError::None);
	TestFalse(TEXT("the terminal task was the one evicted"), Exists(Store, TEXT("a"), Second.TaskId));
	TestTrue(TEXT("the older running task is untouched"), Exists(Store, TEXT("a"), Live.TaskId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpTaskStoreExpiryTest,
	"McpAutomationBridge.Primitives.TaskStore.Expiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpTaskStoreExpiryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FFakeClock Clock;
	FMcpTaskStore Store(SmallConfig(8, 10000));
	Store.SetClock(Clock.Fn());

	const FString Finished = CreateFinished(Store, TEXT("a"));
	FMcpTaskRecord Working;
	Store.CreateTask(TEXT("a"), false, 0, Working);

	Clock.Advance(9999);
	TestTrue(TEXT("still retained one tick before the TTL"), Exists(Store, TEXT("a"), Finished));
	Clock.Advance(1);
	TestFalse(TEXT("terminal task expires on its TTL"), Exists(Store, TEXT("a"), Finished));
	TestFalse(TEXT("a running task expires on its TTL too"), Exists(Store, TEXT("a"), Working.TaskId));
	TestEqual(TEXT("expiry frees the partition"), Store.SessionSize(TEXT("a")), 0);

	FMcpTaskRecord Clamped;
	Store.CreateTask(TEXT("a"), true, 10 * 60000, Clamped);
	TestEqual(TEXT("a requested TTL is clamped to the ceiling"), Clamped.TtlMs, static_cast<int64>(300000));
	FMcpTaskRecord Honoured;
	Store.CreateTask(TEXT("a"), true, 30000, Honoured);
	TestEqual(TEXT("a TTL under the ceiling is honoured"), Honoured.TtlMs, static_cast<int64>(30000));
	FMcpTaskRecord Defaulted;
	Store.CreateTask(TEXT("a"), false, 0, Defaulted);
	TestEqual(TEXT("an absent TTL never means unlimited"), Defaulted.TtlMs, static_cast<int64>(10000));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpTaskStoreSessionIsolationTest,
	"McpAutomationBridge.Primitives.TaskStore.SessionIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpTaskStoreSessionIsolationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FFakeClock Clock;
	FMcpTaskStore Store(SmallConfig(2));
	Store.SetClock(Clock.Fn());

	FMcpTaskRecord Owned;
	Store.CreateTask(TEXT("a"), false, 0, Owned);

	FMcpTaskRecord Peek;
	TestEqual(TEXT("another session cannot READ the task"),
		Store.GetTask(TEXT("b"), Owned.TaskId, Peek), EMcpTaskStoreError::NotFound);
	TestEqual(TEXT("another session cannot CANCEL the task"),
		Store.UpdateStatus(TEXT("b"), Owned.TaskId, EMcpTaskStatus::Cancelled, TEXT("nope")),
		EMcpTaskStoreError::NotFound);
	TestEqual(TEXT("another session cannot WRITE a result into the task"),
		Store.StoreResult(TEXT("b"), Owned.TaskId, EMcpTaskStatus::Completed, MakeShared<FJsonObject>()),
		EMcpTaskStoreError::NotFound);
	TSharedPtr<FJsonObject> Stolen;
	TestEqual(TEXT("another session cannot READ the result"),
		Store.GetResult(TEXT("b"), Owned.TaskId, Stolen), EMcpTaskStoreError::NotFound);

	FMcpTaskRecord Still;
	Store.GetTask(TEXT("a"), Owned.TaskId, Still);
	TestEqual(TEXT("the owner's task is untouched"), Still.Status, EMcpTaskStatus::Working);

	// Session B overflowing its own partition must not evict session A's tasks.
	const FString AliveA = CreateFinished(Store, TEXT("a"));
	for (int32 Index = 0; Index < 6; ++Index) CreateFinished(Store, TEXT("b"));
	TestTrue(TEXT("another session cannot EVICT this session's task"), Exists(Store, TEXT("a"), AliveA));
	TestEqual(TEXT("each partition is capped independently"), Store.SessionSize(TEXT("b")), 2);

	TArray<FMcpTaskRecord> Listed;
	Store.ListTasks(TEXT("b"), Listed);
	for (const FMcpTaskRecord& Record : Listed)
	{
		TestNotEqual(TEXT("list never leaks another session's task"), Record.TaskId, AliveA);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpTaskStoreTerminalTest,
	"McpAutomationBridge.Primitives.TaskStore.SingleTerminalState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpTaskStoreTerminalTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FFakeClock Clock;
	FMcpTaskStore Store(SmallConfig(4));
	Store.SetClock(Clock.Fn());

	FMcpTaskRecord Task;
	Store.CreateTask(TEXT("a"), false, 0, Task);
	auto FirstResult = MakeShared<FJsonObject>();
	FirstResult->SetBoolField(TEXT("first"), true);
	TestEqual(TEXT("the first terminal transition is accepted"),
		Store.StoreResult(TEXT("a"), Task.TaskId, EMcpTaskStatus::Completed, FirstResult),
		EMcpTaskStoreError::None);
	TestEqual(TEXT("a duplicate terminal result is refused"),
		Store.StoreResult(TEXT("a"), Task.TaskId, EMcpTaskStatus::Failed, MakeShared<FJsonObject>()),
		EMcpTaskStoreError::AlreadyTerminal);
	TestEqual(TEXT("no transition out of a terminal state"),
		Store.UpdateStatus(TEXT("a"), Task.TaskId, EMcpTaskStatus::Working, FString()),
		EMcpTaskStoreError::AlreadyTerminal);

	TSharedPtr<FJsonObject> Retained;
	Store.GetResult(TEXT("a"), Task.TaskId, Retained);
	TestTrue(TEXT("the published result is the one that survives"),
		Retained.IsValid() && Retained->HasField(TEXT("first")));

	// A LATE result after cancellation must not resurrect the task as completed.
	FMcpTaskRecord Cancelled;
	Store.CreateTask(TEXT("a"), false, 0, Cancelled);
	Store.UpdateStatus(TEXT("a"), Cancelled.TaskId, EMcpTaskStatus::Cancelled, TEXT("client cancelled"));
	TestEqual(TEXT("a late result after cancellation is refused"),
		Store.StoreResult(TEXT("a"), Cancelled.TaskId, EMcpTaskStatus::Completed, MakeShared<FJsonObject>()),
		EMcpTaskStoreError::AlreadyTerminal);
	FMcpTaskRecord AfterLate;
	Store.GetTask(TEXT("a"), Cancelled.TaskId, AfterLate);
	TestEqual(TEXT("the task still reads cancelled"), AfterLate.Status, EMcpTaskStatus::Cancelled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpTaskStoreCleanupTest,
	"McpAutomationBridge.Primitives.TaskStore.Cleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpTaskStoreCleanupTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FFakeClock Clock;
	FMcpTaskStore Store(SmallConfig(4));
	Store.SetClock(Clock.Fn());

	const FString Owned = CreateFinished(Store, TEXT("a"));
	const FString Other = CreateFinished(Store, TEXT("b"));

	Store.CloseSession(TEXT("a"));
	TestFalse(TEXT("disconnect drops the session's tasks"), Exists(Store, TEXT("a"), Owned));
	TestTrue(TEXT("disconnect leaves other sessions alone"), Exists(Store, TEXT("b"), Other));
	TSharedPtr<FJsonObject> Gone;
	TestEqual(TEXT("no retained result survives the disconnect"),
		Store.GetResult(TEXT("a"), Owned, Gone), EMcpTaskStoreError::NotFound);

	Store.Clear();
	TestEqual(TEXT("shutdown drops everything"), Store.TotalSize(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpTaskCheckpointPolicyTest,
	"McpAutomationBridge.Primitives.TaskStore.CheckpointPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpTaskCheckpointPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestTrue(TEXT("search may be task-augmented"), McpTaskCheckpointOperationAllowed(TEXT("search")));
	TestTrue(TEXT("describe may be task-augmented"), McpTaskCheckpointOperationAllowed(TEXT("describe")));
	TestFalse(TEXT("execute may NOT be task-augmented"), McpTaskCheckpointOperationAllowed(TEXT("execute")));
	TestFalse(TEXT("configure may NOT be task-augmented"), McpTaskCheckpointOperationAllowed(TEXT("configure")));
	TestFalse(TEXT("an unknown operation is refused"), McpTaskCheckpointOperationAllowed(TEXT("")));
	TestEqual(TEXT("exactly two operations are offered"), McpTaskCheckpointOperations().Num(), 2);
	return true;
}

#endif  // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
