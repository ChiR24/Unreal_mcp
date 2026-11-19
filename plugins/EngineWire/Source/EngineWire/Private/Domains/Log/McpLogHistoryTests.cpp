// McpLogHistoryTests.cpp — the kept editor log counts a line repeated within its window on that line's first copy.

#include "Domains/Log/McpLogHistory.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpLogHistoryRepeatTest,
	"EngineWire.Log.RepeatCollapse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpLogHistoryRepeatTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpLogHistory History; // a ring of its own, never added to GLog
	const FName Category(TEXT("LogMcpRepeatProbe"));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		History.Serialize(TEXT("probe line"), ELogVerbosity::Warning, Category);
	}
	History.Serialize(TEXT("other line"), ELogVerbosity::Warning, Category);
	// Another line between them does not split a repeat: the last lines are scanned, not only the newest.
	History.Serialize(TEXT("probe line"), ELogVerbosity::Warning, Category);
	History.Serialize(TEXT("probe line"), ELogVerbosity::Error, Category); // another severity is another line

	int32 Matched = 0;
	const TArray<FString> Lines = History.Read(10, FString(), Category.ToString(), ELogVerbosity::All, Matched);
	TestEqual(TEXT("three rows: the repeated warning, the other line, the error"), Lines.Num(), 3);
	if (Lines.Num() != 3)
	{
		return false;
	}
	TestTrue(TEXT("the warning is kept once with its count"), Lines[0].Contains(TEXT("probe line (4x, last at ")));
	TestFalse(TEXT("a line logged once carries no count"), Lines[1].Contains(TEXT("x, last at")));
	TestTrue(TEXT("the error of the same text is its own row"), Lines[2].Contains(TEXT("Error: probe line")) &&
		!Lines[2].Contains(TEXT("x, last at")));
	TestEqual(TEXT("each row counts once as a match"), Matched, 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
