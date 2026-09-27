// Todo 9 lane 1 - diagnostics snapshot store automation tests.
//
// These run IN the editor process against a UNIQUE temp root injected through
// SetRootOverride, so a real user's <Project>/Saved/MCP/diagnostics tree is
// never touched and the store never resolves project paths during a test.
// Every test resets the singleton and deletes its own temp root on the way out;
// a crashed test editor can leave one temp folder in the OS temp dir at worst.
//
// Coverage (foundation store contract only - hook wiring is a later lane):
//   * valid rotation: a session with recorded events is promoted to previous,
//     a fresh current is initialized, and exactly one previous exists
//   * crash preservation: the last pre-dispatch record survives a simulated
//     restart because it was persisted before the terminal update
//   * empty sessions are never promoted (a commandlet/second restart cannot
//     wipe previous crash evidence)
//   * corrupt and oversized current files are ignored with no quarantine, and
//     startup replaces them with a bounded fresh record
//   * typed recorders serialize strict allowlisted values (non-canonical action
//     and unknown origin become sentinels) and a session is recorded only as a
//     truncated SHA-256 identity, never the raw id

#include "Tests/Diagnostics/McpDiagnosticsTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Containers/StringConv.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

// Helpers carry a Snapshot prefix because bUseUnity merges these test
// translation units and a sibling diagnostics suite defines helpers with the
// same bare names; unqualified names would collide in the merged unit.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDiagnosticsRotationPromotesCrashedSessionTest,
	"McpAutomationBridge.Foundation.Diagnostics.RotationPromotesCrashedSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDiagnosticsRotationPromotesCrashedSessionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpDiagnosticsTestStore Fixture(1000.0);
	FMcpDiagnosticsSnapshot& Store = Fixture.Store;
	const FString& Root = Fixture.Root;

	// The editor "crashes" right after the last pre-dispatch refresh: admission
	// + pre-dispatch were persisted, no terminal update ever ran.
	Store.RecordAdmission(
		TEXT("req-crash-1"), TEXT("corr-crash-1"),
		TEXT("manage_asset.import_asset"), TEXT("WebSocket"), 2);
	Fixture.Now += 1.0;
	Store.RecordPreDispatch(TEXT("req-crash-1"), 1);
	TestTrue(TEXT("pre-dispatch record persisted before the crash"), Store.PersistCurrent());

	// Restart: rotate current to previous, then initialize a fresh current.
	Store.RotateOnStartup();

	const FString RootDir = Root + TEXT("/");
	TestTrue(TEXT("previous exists after rotation"),
		FPaths::FileExists(RootDir + TEXT("previous-session.json")));
	TestTrue(TEXT("current re-initialized"),
		FPaths::FileExists(RootDir + TEXT("current-session.json")));
	TestFalse(TEXT("current temp removed after rotation"),
		FPaths::FileExists(RootDir + TEXT("current-session.json.tmp")));
	TestFalse(TEXT("previous temp removed after rotation"),
		FPaths::FileExists(RootDir + TEXT("previous-session.json.tmp")));

	const FString Previous = McpTestReadFile(RootDir + TEXT("previous-session.json"));
	TestTrue(TEXT("previous keeps the pre-dispatch request id"),
		Previous.Contains(TEXT("\"requestId\":\"req-crash-1\"")));
	TestTrue(TEXT("previous keeps the canonical action"),
		Previous.Contains(TEXT("\"canonicalAction\":\"manage_asset.import_asset\"")));

	const FString Current = McpTestReadFile(RootDir + TEXT("current-session.json"));
	TestFalse(TEXT("fresh current carries no stale request"),
		Current.Contains(TEXT("req-crash-1")));
	TestTrue(TEXT("the previous summary is exposed to presenters"),
		Store.PreviousSummaryJson()->HasField(TEXT("instance")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDiagnosticsEmptySessionNotPromotedTest,
	"McpAutomationBridge.Foundation.Diagnostics.EmptySessionNotPromoted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDiagnosticsEmptySessionNotPromotedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpDiagnosticsTestStore Fixture(2000.0);
	FMcpDiagnosticsSnapshot& Store = Fixture.Store;
	const FString& Root = Fixture.Root;

	// Two consecutive starts of an event-less session: a commandlet or a second
	// restart must not promote an empty session over existing crash evidence.
	Store.RotateOnStartup();
	Store.RotateOnStartup();

	const FString RootDir = Root + TEXT("/");
	TestFalse(TEXT("an empty session is never promoted to previous"),
		FPaths::FileExists(RootDir + TEXT("previous-session.json")));
	TestTrue(TEXT("current is still initialized"),
		FPaths::FileExists(RootDir + TEXT("current-session.json")));
	TestFalse(TEXT("no previous summary is exposed"),
		Store.PreviousSummaryJson()->HasField(TEXT("instance")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDiagnosticsCorruptAndOversizedIgnoredTest,
	"McpAutomationBridge.Foundation.Diagnostics.CorruptAndOversizedIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDiagnosticsCorruptAndOversizedIgnoredTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpDiagnosticsTestStore Fixture(3000.0);
	FMcpDiagnosticsSnapshot& Store = Fixture.Store;
	const FString& Root = Fixture.Root;
	Store.RotateOnStartup(); // seed a healthy fresh current

	const FString RootDir = Root + TEXT("/");
	const FString CurrentPath = RootDir + TEXT("current-session.json");
	const FString PreviousPath = RootDir + TEXT("previous-session.json");

	// Corrupt current: ignored with one typed warning; startup stays healthy.
	McpTestWriteFile(CurrentPath, TEXT("{ this is not json"));
	Store.RotateOnStartup();
	const FString AfterCorrupt = McpTestReadFile(CurrentPath);
	TestTrue(TEXT("corrupt current is replaced by a fresh record"),
		AfterCorrupt.Contains(TEXT("\"schemaVersion\":1")));
	TestFalse(TEXT("corrupt session is never promoted"),
		FPaths::FileExists(PreviousPath));

	// Oversized current: ignored, never sliced, never quarantined.
	McpTestWriteFile(CurrentPath, FString::ChrN(70 * 1024, TEXT('x')));
	Store.RotateOnStartup();
	const FString AfterOversized = McpTestReadFile(CurrentPath);
	TestTrue(TEXT("oversized current is replaced by a bounded fresh record"),
		AfterOversized.Contains(TEXT("\"schemaVersion\":1")));
	TestTrue(TEXT("the fresh record stays under the 64 KiB cap"),
		FTCHARToUTF8(AfterOversized).Length() <= McpDiagnosticsSchema::MaxSnapshotBytes);
	TestFalse(TEXT("no quarantine/accumulated previous file appears"),
		FPaths::FileExists(PreviousPath));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDiagnosticsRecordersAndRedactionTest,
	"McpAutomationBridge.Foundation.Diagnostics.RecordersAndRedaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDiagnosticsRecordersAndRedactionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpDiagnosticsTestStore Fixture(4000.0);
	FMcpDiagnosticsSnapshot& Store = Fixture.Store;
	const FString& Root = Fixture.Root;

	Store.RecordHandshake(true);
	Store.RecordAdmission(
		TEXT("req-typed-1"), TEXT("corr-typed-1"),
		TEXT("some.evil.action"), TEXT("Claude"), 3);
	Fixture.Now += 0.5;
	Store.RecordPreDispatch(TEXT("req-typed-1"), 0);
	Store.RecordTerminal(TEXT("req-typed-1"), TEXT("success"));
	Store.RecordDisconnect(TEXT("closed"));
	Store.RecordSessionCreated(TEXT("raw-native-session-credential-123"));
	Store.RecordSessionClosed();
	TestTrue(TEXT("typed record persisted"), Store.PersistCurrent());

	const FString Current = McpTestReadFile(Root + TEXT("/current-session.json"));
	TestTrue(TEXT("non-canonical action is clamped to the sentinel"),
		Current.Contains(TEXT("\"canonicalAction\":\"non_canonical\"")));
	TestTrue(TEXT("unknown origin is clamped to the sentinel"),
		Current.Contains(TEXT("\"origin\":\"unknown\"")));
	TestTrue(TEXT("handshake summary is serialized"),
		Current.Contains(TEXT("lastHandshake")));
	TestTrue(TEXT("disconnect summary is serialized"),
		Current.Contains(TEXT("lastDisconnect")));
	TestTrue(TEXT("session counters are serialized"),
		Current.Contains(TEXT("\"created\":1")));
	TestTrue(TEXT("terminal class is serialized"),
		Current.Contains(TEXT("\"terminalClass\":\"success\"")));
	TestFalse(TEXT("a raw session credential never reaches disk"),
		Current.Contains(TEXT("raw-native-session-credential-123")));
	TestTrue(TEXT("the truncated SHA-256 session identity is serialized"),
		Current.Contains(TEXT("lastIdentitySha256")));
	TestTrue(TEXT("the on-disk record stays under 64 KiB"),
		FTCHARToUTF8(Current).Length() <= FMcpDiagnosticsSnapshot::MaxSnapshotBytes());

	const TSharedPtr<FJsonObject> Session =
		Store.CurrentSummaryJson()->GetObjectField(TEXT("session"));
	TestTrue(TEXT("the exposed session identity is exactly 32 hex chars"),
		Session.IsValid() && Session->GetStringField(TEXT("lastIdentitySha256")).Len() == 32);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
