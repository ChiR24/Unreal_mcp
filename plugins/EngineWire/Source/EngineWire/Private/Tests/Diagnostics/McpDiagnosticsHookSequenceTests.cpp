// Diagnostics hook sequence automation tests.
//
// These run IN the editor process against a UNIQUE temp root injected through
// SetRootOverride, so a real user's <Project>/Saved/MCP/diagnostics tree is
// never touched and the store never resolves project paths during a test.
// Every test resets the singleton and deletes its own temp root on the way out.
//
// Coverage (hook wiring - no source-contract test gates the call sites any
// more; this file proves the exact recorder SEQUENCES the hooks perform at the
// STORE level):
//   * admission -> pre-dispatch -> terminal -> PersistCurrent -> RotateOnStartup
//     leaves the terminal record in previous (the crash-preservation contract)
//   * a refusal increments refusals and coerces terminal (the disk write is
//     coalesced into the next game-thread persist)
//   * handshake + disconnect summaries
//   * window-independent balance truth: the store exposes raw
//     created/closed counters verbatim and clamps SessionsActive >= 0, so
//     closed MAY exceed created at the store level when RecordSessionClosed is
//     called more times than RecordSessionCreated (the store does NOT dedupe -
//     dedupe lives in the transport's session-close funnel, in its window of
//     the last 128 closed ids). First-close-wins inside that window is shown
//     live by deleting one session twice, not here.

#include "Tests/Diagnostics/McpDiagnosticsTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDiagnosticsHookSequenceTerminalInPreviousTest,
	"EngineWire.Diagnostics.HookSequenceTerminalInPrevious",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDiagnosticsHookSequenceTerminalInPreviousTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpDiagnosticsTestStore Fixture(1000.0);
	FMcpDiagnosticsSnapshot& Store = Fixture.Store;
	const FString& Root = Fixture.Root;

	// The editor "crashes" right after the last pre-dispatch refresh: admission
	// + pre-dispatch + terminal were persisted, then the session is rotated on
	// the next boot.
	Store.RecordAdmission(
		TEXT("req-hook-1"), TEXT("corr-hook-1"),
		TEXT("manage_asset.import_asset"), TEXT("WebSocket"), 2);
	Fixture.Now += 1.0;
	Store.RecordPreDispatch(TEXT("req-hook-1"), 1);
	Store.RecordTerminal(TEXT("req-hook-1"), TEXT("success"));
	TestTrue(TEXT("terminal record persisted before rotation"), Store.PersistCurrent());

	Store.RotateOnStartup();

	const FString RootDir = Root + TEXT("/");
	const FString Previous = McpTestReadFile(RootDir + TEXT("previous-session.json"));
	TestTrue(TEXT("previous exists after rotation"),
		FPaths::FileExists(RootDir + TEXT("previous-session.json")));
	TestTrue(TEXT("previous keeps the terminal request id"),
		Previous.Contains(TEXT("\"requestId\":\"req-hook-1\"")));
	TestTrue(TEXT("previous keeps the terminal class"),
		Previous.Contains(TEXT("\"terminalClass\":\"success\"")));

	const FString Current = McpTestReadFile(RootDir + TEXT("current-session.json"));
	TestFalse(TEXT("fresh current carries no stale terminal"),
		Current.Contains(TEXT("req-hook-1")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDiagnosticsHookSequenceRefusalCoercesTerminalTest,
	"EngineWire.Diagnostics.HookSequenceRefusalCoercesTerminal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDiagnosticsHookSequenceRefusalCoercesTerminalTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpDiagnosticsTestStore Fixture(2000.0);
	FMcpDiagnosticsSnapshot& Store = Fixture.Store;
	const FString& Root = Fixture.Root;

	// A queue-full refusal records memory-only at the socket-thread call
	// site; its disk write is COALESCED into the next game-thread persist.
	// AUTOMATION_QUEUE_FULL is not an allowlist terminal class, so it coerces
	// to the store's unknown sentinel while the refusals counter stays exact.
	Store.RecordRefusal(TEXT("req-hook-ref"), TEXT("AUTOMATION_QUEUE_FULL"), 64);
	TestTrue(TEXT("coalesced refusal persisted on next game-thread persist"),
		Store.PersistCurrent());

	const FString Current = McpTestReadFile(Root + TEXT("/current-session.json"));
	TestTrue(TEXT("refusals counter incremented"),
		Current.Contains(TEXT("\"refusals\":1")));
	TestTrue(TEXT("non-allowlist refusal coerces to unknown terminal"),
		Current.Contains(TEXT("\"terminalClass\":\"unknown\"")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDiagnosticsHookSequenceSessionBalanceWindowIndependentTest,
	"EngineWire.Diagnostics.HookSequenceSessionBalanceWindowIndependent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDiagnosticsHookSequenceSessionBalanceWindowIndependentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpDiagnosticsTestStore Fixture(3000.0);
	FMcpDiagnosticsSnapshot& Store = Fixture.Store;
	const FString& Root = Fixture.Root;

	// Summaries: handshake success is overwritten by a later disconnect
	// summary (last-value, not a counter). Created once, closed THREE
	// times: the store passes created/closed verbatim and clamps SessionsActive
	// >= 0, so closed may exceed created at the store level - the store does
	// NOT dedupe; dedupe lives in the transport's window of the last 128 closed ids.
	// On-disk JSON carries the verbatim counters "closed":3 and "active":0.
	Store.RecordHandshake(true);
	Fixture.Now += 1.0;
	Store.RecordDisconnect(TEXT("closed"));
	Store.RecordSessionCreated(TEXT("raw-native-session-credential-123"));
	Store.RecordSessionClosed();
	Store.RecordSessionClosed();
	Store.RecordSessionClosed();
	TestTrue(TEXT("summaries persisted"), Store.PersistCurrent());

	const FString Current = McpTestReadFile(Root + TEXT("/current-session.json"));
	TestTrue(TEXT("handshake summary is serialized"),
		Current.Contains(TEXT("lastHandshake")));
	TestTrue(TEXT("disconnect summary is serialized"),
		Current.Contains(TEXT("lastDisconnect")));
	TestTrue(TEXT("disconnect reason stays in the bounded allowlist"),
		Current.Contains(TEXT("\"reason\":\"closed\"")));
	TestTrue(TEXT("created counter is verbatim"),
		Current.Contains(TEXT("\"created\":1")));
	TestTrue(TEXT("closed may exceed created at the store level"),
		Current.Contains(TEXT("\"closed\":3")));
	TestTrue(TEXT("SessionsActive clamps to zero, never negative"),
		Current.Contains(TEXT("\"active\":0")));
	TestFalse(TEXT("a raw session credential never reaches disk"),
		Current.Contains(TEXT("raw-native-session-credential-123")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
