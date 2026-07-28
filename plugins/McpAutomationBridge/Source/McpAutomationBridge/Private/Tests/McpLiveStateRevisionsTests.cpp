#include "Foundation/McpLiveStateRevisions.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MCP/Execute/McpNativeGatewayExecuteRequest.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpLiveStateRevisionsPreconditionTest,
	"McpAutomationBridge.Foundation.LiveStateRevisions.Preconditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpLiveStateRevisionsPreconditionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpLiveStateRevisions& Rev = FMcpLiveStateRevisions::Get();
	Rev.Reset();

	EMcpStateKind Kind = EMcpStateKind::Selection;
	int64 Expected = 0;
	int64 Current = 0;

	// Every state starts at the initial revision.
	TestEqual(TEXT("selection starts at initial"), Rev.Current(EMcpStateKind::Selection),
		FMcpLiveStateRevisions::McpInitialStateRevision);

	// UNCHANGED: an expected value equal to the live value passes.
	{
		TMap<EMcpStateKind, int64> Ask;
		Ask.Add(EMcpStateKind::Selection, FMcpLiveStateRevisions::McpInitialStateRevision);
		TestTrue(TEXT("unchanged precondition passes"),
			Rev.CheckPreconditions(Ask, Kind, Expected, Current));
	}

	// An empty ask never refuses (nothing is pinned).
	{
		TMap<EMcpStateKind, int64> Empty;
		TestTrue(TEXT("no pin passes"), Rev.CheckPreconditions(Empty, Kind, Expected, Current));
	}

	Rev.Advance(EMcpStateKind::Selection);
	TestEqual(TEXT("advance bumps selection by one"), Rev.Current(EMcpStateKind::Selection),
		FMcpLiveStateRevisions::McpInitialStateRevision + 1);

	// STALE: the pinned selection no longer matches, so it refuses and names the
	// concrete current reference.
	{
		TMap<EMcpStateKind, int64> Ask;
		Ask.Add(EMcpStateKind::Selection, FMcpLiveStateRevisions::McpInitialStateRevision);
		TestFalse(TEXT("stale selection refuses"),
			Rev.CheckPreconditions(Ask, Kind, Expected, Current));
		TestTrue(TEXT("refusal names selection"), Kind == EMcpStateKind::Selection);
		TestEqual(TEXT("refusal carries expected"), Expected, FMcpLiveStateRevisions::McpInitialStateRevision);
		TestEqual(TEXT("refusal carries current"), Current, FMcpLiveStateRevisions::McpInitialStateRevision + 1);
	}

	// UNRELATED-CHANGE: selection moved, but a pin on the still-unchanged level
	// passes, proving per-key granularity rather than a global epoch.
	{
		TMap<EMcpStateKind, int64> Ask;
		Ask.Add(EMcpStateKind::Level, FMcpLiveStateRevisions::McpInitialStateRevision);
		TestTrue(TEXT("unrelated pin passes despite selection change"),
			Rev.CheckPreconditions(Ask, Kind, Expected, Current));
	}

	// MISMATCH across several states reports the FIRST stale one deterministically
	// (selection before package by the fixed order).
	{
		Rev.Advance(EMcpStateKind::Package);
		TMap<EMcpStateKind, int64> Ask;
		Ask.Add(EMcpStateKind::Package, FMcpLiveStateRevisions::McpInitialStateRevision);
		Ask.Add(EMcpStateKind::Selection, FMcpLiveStateRevisions::McpInitialStateRevision);
		TestFalse(TEXT("multi-state mismatch refuses"),
			Rev.CheckPreconditions(Ask, Kind, Expected, Current));
		TestTrue(TEXT("reports selection first by fixed order"), Kind == EMcpStateKind::Selection);
	}

	// Key round-trips through its wire token.
	{
		EMcpStateKind Parsed = EMcpStateKind::Level;
		TestTrue(TEXT("package key parses"), FMcpLiveStateRevisions::KindFor(TEXT("package"), Parsed));
		TestTrue(TEXT("package key round-trips"), Parsed == EMcpStateKind::Package);
		TestFalse(TEXT("unknown key rejected"), FMcpLiveStateRevisions::KindFor(TEXT("nope"), Parsed));
		TestEqual(TEXT("selection key spelling"),
			FString(FMcpLiveStateRevisions::KeyFor(EMcpStateKind::Selection)), FString(TEXT("selection")));
	}

	Rev.Reset();
	TestEqual(TEXT("reset returns selection to initial"), Rev.Current(EMcpStateKind::Selection),
		FMcpLiveStateRevisions::McpInitialStateRevision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpExpectedRevisionsParseTest,
	"McpAutomationBridge.Foundation.LiveStateRevisions.ExpectedRevisionsParsing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpExpectedRevisionsParseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TMap<EMcpStateKind, int64> Parsed;
	FMcpSemanticError Error;

	// An absent options envelope pins nothing, so it can never refuse.
	TestTrue(TEXT("absent options parse"), McpParseExpectedRevisions(nullptr, Parsed, Error));
	TestEqual(TEXT("absent options pin nothing"), Parsed.Num(), 0);

	// Options present but without the key is equally unpinned.
	{
		TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetStringField(TEXT("idempotencyKey"), TEXT("k1"));
		TestTrue(TEXT("options without the key parse"),
			McpParseExpectedRevisions(Options, Parsed, Error));
		TestEqual(TEXT("no pins recorded"), Parsed.Num(), 0);
	}

	// Every wire key maps to its state kind and keeps its exact value.
	{
		TSharedPtr<FJsonObject> Pins = MakeShared<FJsonObject>();
		Pins->SetNumberField(TEXT("selection"), 3);
		Pins->SetNumberField(TEXT("level"), 7);
		Pins->SetNumberField(TEXT("assetRegistry"), 11);
		Pins->SetNumberField(TEXT("package"), 13);
		TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetObjectField(TEXT("expectedRevisions"), Pins);
		TestTrue(TEXT("all four keys parse"),
			McpParseExpectedRevisions(Options, Parsed, Error));
		TestEqual(TEXT("four pins recorded"), Parsed.Num(), 4);
		TestEqual(TEXT("selection value preserved"),
			Parsed.FindRef(EMcpStateKind::Selection), static_cast<int64>(3));
		TestEqual(TEXT("package value preserved"),
			Parsed.FindRef(EMcpStateKind::Package), static_cast<int64>(13));
	}

	// An unknown pin name is refused as an option error that names what IS
	// supported, and leaves no partially-parsed pins behind.
	{
		TSharedPtr<FJsonObject> Pins = MakeShared<FJsonObject>();
		Pins->SetNumberField(TEXT("selektion"), 2);
		TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetObjectField(TEXT("expectedRevisions"), Pins);
		TestFalse(TEXT("unknown pin refused"),
			McpParseExpectedRevisions(Options, Parsed, Error));
		TestEqual(TEXT("unknown pin is an option error"),
			Error.Code, FString(TEXT("UNSUPPORTED_OPTION")));
		TestTrue(TEXT("refusal lists the supported pins"),
			Error.Supported.Contains(TEXT("selection")));
		TestEqual(TEXT("no partial pins survive a refusal"), Parsed.Num(), 0);
	}

	// A non-object envelope is a validation error pointing at the option itself.
	{
		TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetStringField(TEXT("expectedRevisions"), TEXT("selection=2"));
		TestFalse(TEXT("non-object envelope refused"),
			McpParseExpectedRevisions(Options, Parsed, Error));
		TestEqual(TEXT("pointer names the option"), Error.Pointer,
			FString(TEXT("/options/expectedRevisions")));
	}

	// A revision is a monotonic counter starting at McpInitialStateRevision, so
	// zero, negatives, fractions and non-numbers are all out of range.
	{
		TArray<TPair<FString, TSharedPtr<FJsonValue>>> Bad;
		Bad.Emplace(TEXT("zero"), MakeShared<FJsonValueNumber>(0));
		Bad.Emplace(TEXT("negative"), MakeShared<FJsonValueNumber>(-4));
		Bad.Emplace(TEXT("fractional"), MakeShared<FJsonValueNumber>(2.5));
		Bad.Emplace(TEXT("string"), MakeShared<FJsonValueString>(TEXT("2")));
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Case : Bad)
		{
			TSharedPtr<FJsonObject> Pins = MakeShared<FJsonObject>();
			Pins->SetField(TEXT("selection"), Case.Value);
			TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
			Options->SetObjectField(TEXT("expectedRevisions"), Pins);
			TestFalse(FString::Printf(TEXT("%s revision refused"), *Case.Key),
				McpParseExpectedRevisions(Options, Parsed, Error));
			TestEqual(FString::Printf(TEXT("%s names the field"), *Case.Key),
				Error.Field, FString(TEXT("expectedRevisions.selection")));
		}
	}

	// The bounded option gate must accept expectedRevisions as a known key,
	// otherwise a legitimate pin would be refused as an unsupported option.
	{
		TSharedPtr<FJsonObject> Pins = MakeShared<FJsonObject>();
		Pins->SetNumberField(TEXT("selection"), 1);
		TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetObjectField(TEXT("expectedRevisions"), Pins);
		FMcpSemanticError OptionError;
		TestTrue(TEXT("expectedRevisions is a supported option"),
			McpValidateExecutionOptions(Options, OptionError));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpLiveStateRevisionSnapshotTest,
	"McpAutomationBridge.Foundation.LiveStateRevisions.Snapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpLiveStateRevisionSnapshotTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpLiveStateRevisions& Revisions = FMcpLiveStateRevisions::Get();
	Revisions.Reset();
	Revisions.Advance(EMcpStateKind::Selection);
	Revisions.Advance(EMcpStateKind::Selection);
	Revisions.Advance(EMcpStateKind::Package);

	const FMcpLiveStateRevisionSnapshot Snapshot = Revisions.Snapshot();
	TestEqual(TEXT("selection snapshot"), Snapshot.Selection, static_cast<int64>(3));
	TestEqual(TEXT("level snapshot"), Snapshot.Level, static_cast<int64>(1));
	TestEqual(TEXT("asset registry snapshot"), Snapshot.AssetRegistry, static_cast<int64>(1));
	TestEqual(TEXT("package snapshot"), Snapshot.Package, static_cast<int64>(2));
	TestEqual(TEXT("snapshot maximum"), Snapshot.Max(), static_cast<int64>(3));

	const TSharedRef<FJsonObject> Json = Snapshot.ToJson();
	TestEqual(TEXT("snapshot has four keys"), Json->Values.Num(), 4);
	TestEqual(TEXT("selection JSON value"),
		static_cast<int64>(Json->GetNumberField(TEXT("selection"))), static_cast<int64>(3));
	TestEqual(TEXT("package JSON value"),
		static_cast<int64>(Json->GetNumberField(TEXT("package"))), static_cast<int64>(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpLiveStateTrackerTest,
	"McpAutomationBridge.Foundation.LiveStateRevisions.EditorEventsAdvanceCounters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpLiveStateTrackerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// Proves the delegates are actually BOUND, not merely that the tracker
	// compiles: without this, a pin could only ever compare against the initial
	// revision and every precondition would trivially pass.
	FMcpLiveStateRevisions& Rev = FMcpLiveStateRevisions::Get();

	const int64 PackageBefore = Rev.Current(EMcpStateKind::Package);
	UPackage* Probe = CreatePackage(TEXT("/Temp/McpLiveStateRevisionProbe"));
	TestNotNull(TEXT("probe package created"), Probe);
	if (Probe)
	{
		Probe->SetDirtyFlag(false);
		Probe->MarkPackageDirty();
	}
	TestTrue(TEXT("marking a package dirty advances the package revision"),
		Rev.Current(EMcpStateKind::Package) > PackageBefore);

	// A package event must not move an unrelated counter, otherwise a pin on one
	// state would be invalidated by activity in another.
	const int64 SelectionBefore = Rev.Current(EMcpStateKind::Selection);
	if (Probe)
	{
		Probe->MarkPackageDirty();
	}
	TestEqual(TEXT("package activity leaves selection untouched"),
		Rev.Current(EMcpStateKind::Selection), SelectionBefore);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
