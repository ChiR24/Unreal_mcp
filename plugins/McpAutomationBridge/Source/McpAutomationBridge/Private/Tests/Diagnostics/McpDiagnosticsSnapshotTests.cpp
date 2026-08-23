// Todo 9 (BB-005) lane 1 - diagnostics snapshot store automation tests.
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

#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
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
namespace
{
FString SnapshotMakeTestRoot()
{
	// A GUID, not a timestamp. These tests share one FMcpDiagnosticsSnapshot
	// singleton and each tears down by deleting its root, so a name that only
	// changed once per second handed every test in the suite the same directory:
	// one test read another's leftover snapshots, and a teardown deleted the tree
	// a sibling was still using. That looked like four independent rotation and
	// redaction failures rather than one collision.
	const FString Unique = FString::Printf(
		TEXT("McpDiagnosticsTests_%d_%s"),
		FPlatformProcess::GetCurrentProcessId(),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString Root = FPaths::Combine(FPlatformProcess::UserTempDir(), Unique);
	IFileManager::Get().MakeDirectory(*Root, true);
	return Root;
}

void SnapshotTearDownStore(FMcpDiagnosticsSnapshot& Store, const FString& Root)
{
	Store.Reset();
	IFileManager::Get().DeleteDirectory(*Root, false, true);
}

FString SnapshotReadFileText(const FString& Path)
{
	FString Content;
	FFileHelper::LoadFileToString(Content, *Path);
	return Content;
}

void SnapshotWriteFileText(const FString& Path, const FString& Content)
{
	FFileHelper::SaveStringToFile(Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool SnapshotFileContains(const FString& Content, const TCHAR* Needle)
{
	return Content.Contains(Needle);
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDiagnosticsRotationPromotesCrashedSessionTest,
	"McpAutomationBridge.Foundation.Diagnostics.RotationPromotesCrashedSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDiagnosticsRotationPromotesCrashedSessionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpDiagnosticsSnapshot& Store = FMcpDiagnosticsSnapshot::Get();
	const FString Root = SnapshotMakeTestRoot();
	Store.Reset();
	Store.SetRootOverride(Root);
	double FakeNow = 1000.0;
	Store.SetClock([&FakeNow]() { return FakeNow; });

	// The editor "crashes" right after the last pre-dispatch refresh: admission
	// + pre-dispatch were persisted, no terminal update ever ran.
	Store.RecordAdmission(
		TEXT("req-crash-1"), TEXT("corr-crash-1"),
		TEXT("manage_asset.import_asset"), TEXT("WebSocket"), 2);
	FakeNow += 1.0;
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

	const FString Previous = SnapshotReadFileText(RootDir + TEXT("previous-session.json"));
	TestTrue(TEXT("previous keeps the pre-dispatch request id"),
		SnapshotFileContains(Previous, TEXT("\"requestId\":\"req-crash-1\"")));
	TestTrue(TEXT("previous keeps the canonical action"),
		SnapshotFileContains(Previous, TEXT("\"canonicalAction\":\"manage_asset.import_asset\"")));

	const FString Current = SnapshotReadFileText(RootDir + TEXT("current-session.json"));
	TestFalse(TEXT("fresh current carries no stale request"),
		SnapshotFileContains(Current, TEXT("req-crash-1")));
	TestTrue(TEXT("the previous summary is exposed to presenters"),
		Store.PreviousSummaryJson()->HasField(TEXT("instance")));

	SnapshotTearDownStore(Store, Root);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDiagnosticsEmptySessionNotPromotedTest,
	"McpAutomationBridge.Foundation.Diagnostics.EmptySessionNotPromoted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDiagnosticsEmptySessionNotPromotedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpDiagnosticsSnapshot& Store = FMcpDiagnosticsSnapshot::Get();
	const FString Root = SnapshotMakeTestRoot();
	Store.Reset();
	Store.SetRootOverride(Root);
	double FakeNow = 2000.0;
	Store.SetClock([&FakeNow]() { return FakeNow; });

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

	SnapshotTearDownStore(Store, Root);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDiagnosticsCorruptAndOversizedIgnoredTest,
	"McpAutomationBridge.Foundation.Diagnostics.CorruptAndOversizedIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDiagnosticsCorruptAndOversizedIgnoredTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpDiagnosticsSnapshot& Store = FMcpDiagnosticsSnapshot::Get();
	const FString Root = SnapshotMakeTestRoot();
	Store.Reset();
	Store.SetRootOverride(Root);
	double FakeNow = 3000.0;
	Store.SetClock([&FakeNow]() { return FakeNow; });
	Store.RotateOnStartup(); // seed a healthy fresh current

	const FString RootDir = Root + TEXT("/");
	const FString CurrentPath = RootDir + TEXT("current-session.json");
	const FString PreviousPath = RootDir + TEXT("previous-session.json");

	// Corrupt current: ignored with one typed warning; startup stays healthy.
	SnapshotWriteFileText(CurrentPath, TEXT("{ this is not json"));
	Store.RotateOnStartup();
	const FString AfterCorrupt = SnapshotReadFileText(CurrentPath);
	TestTrue(TEXT("corrupt current is replaced by a fresh record"),
		SnapshotFileContains(AfterCorrupt, TEXT("\"schemaVersion\":1")));
	TestFalse(TEXT("corrupt session is never promoted"),
		FPaths::FileExists(PreviousPath));

	// Oversized current: ignored, never sliced, never quarantined.
	SnapshotWriteFileText(CurrentPath, FString::ChrN(70 * 1024, TEXT('x')));
	Store.RotateOnStartup();
	const FString AfterOversized = SnapshotReadFileText(CurrentPath);
	TestTrue(TEXT("oversized current is replaced by a bounded fresh record"),
		SnapshotFileContains(AfterOversized, TEXT("\"schemaVersion\":1")));
	TestTrue(TEXT("the fresh record stays under the 64 KiB cap"),
		FTCHARToUTF8(AfterOversized).Length() <= McpDiagnosticsSchema::MaxSnapshotBytes);
	TestFalse(TEXT("no quarantine/accumulated previous file appears"),
		FPaths::FileExists(PreviousPath));

	SnapshotTearDownStore(Store, Root);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpDiagnosticsRecordersAndRedactionTest,
	"McpAutomationBridge.Foundation.Diagnostics.RecordersAndRedaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpDiagnosticsRecordersAndRedactionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpDiagnosticsSnapshot& Store = FMcpDiagnosticsSnapshot::Get();
	const FString Root = SnapshotMakeTestRoot();
	Store.Reset();
	Store.SetRootOverride(Root);
	double FakeNow = 4000.0;
	Store.SetClock([&FakeNow]() { return FakeNow; });

	Store.RecordHandshake(true);
	Store.RecordAdmission(
		TEXT("req-typed-1"), TEXT("corr-typed-1"),
		TEXT("some.evil.action"), TEXT("Claude"), 3);
	FakeNow += 0.5;
	Store.RecordPreDispatch(TEXT("req-typed-1"), 0);
	Store.RecordTerminal(TEXT("req-typed-1"), TEXT("success"));
	Store.RecordDisconnect(TEXT("closed"));
	Store.RecordSessionCreated(TEXT("raw-native-session-credential-123"));
	Store.RecordSessionClosed();
	TestTrue(TEXT("typed record persisted"), Store.PersistCurrent());

	const FString Current = SnapshotReadFileText(Root + TEXT("/current-session.json"));
	TestTrue(TEXT("non-canonical action is clamped to the sentinel"),
		SnapshotFileContains(Current, TEXT("\"canonicalAction\":\"non_canonical\"")));
	TestTrue(TEXT("unknown origin is clamped to the sentinel"),
		SnapshotFileContains(Current, TEXT("\"origin\":\"unknown\"")));
	TestTrue(TEXT("handshake summary is serialized"),
		SnapshotFileContains(Current, TEXT("lastHandshake")));
	TestTrue(TEXT("disconnect summary is serialized"),
		SnapshotFileContains(Current, TEXT("lastDisconnect")));
	TestTrue(TEXT("session counters are serialized"),
		SnapshotFileContains(Current, TEXT("\"created\":1")));
	TestTrue(TEXT("terminal class is serialized"),
		SnapshotFileContains(Current, TEXT("\"terminalClass\":\"success\"")));
	TestFalse(TEXT("a raw session credential never reaches disk"),
		SnapshotFileContains(Current, TEXT("raw-native-session-credential-123")));
	TestTrue(TEXT("the truncated SHA-256 session identity is serialized"),
		SnapshotFileContains(Current, TEXT("lastIdentitySha256")));
	TestTrue(TEXT("the on-disk record stays under 64 KiB"),
		FTCHARToUTF8(Current).Length() <= FMcpDiagnosticsSnapshot::MaxSnapshotBytes());

	const TSharedPtr<FJsonObject> Session =
		Store.CurrentSummaryJson()->GetObjectField(TEXT("session"));
	TestTrue(TEXT("the exposed session identity is exactly 32 hex chars"),
		Session.IsValid() && Session->GetStringField(TEXT("lastIdentitySha256")).Len() == 32);

	SnapshotTearDownStore(Store, Root);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
