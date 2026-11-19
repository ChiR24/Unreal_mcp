// McpMissingReferencesTests.cpp — a soft reference to a path nothing answers to is reported as a warning naming it and
// what holds it; one to an asset that exists is not.

#include "Safety/McpMissingReferences.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "GameMapsSettings.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpMissingReferencesTest,
	"EngineWire.Safety.MissingReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpMissingReferencesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// A content package that never reaches disk: the check skips the transient package and /Temp.
	UPackage* Package = CreatePackage(TEXT("/Game/McpMissingRefsTest"));
	UGameMapsSettings* Holder = NewObject<UGameMapsSettings>(Package, TEXT("Holder"), RF_Transient);
	Holder->EditorStartupMap = FSoftObjectPath(TEXT("/Game/McpNoSuchFolder/NoSuchMap.NoSuchMap"));
	// 0: at least once (the holder's other settings come from the project's config and are not this test's). The
	// patterns are regular expressions on every engine, so they carry no parentheses.
	AddExpectedError(TEXT("McpNoSuchFolder/NoSuchMap.NoSuchMap, which is not an asset in this project"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("held by Holder.EditorStartupMap"), EAutomationExpectedErrorFlags::Contains, 0);
	McpWarnMissingReferences(Holder, EMcpMissingReferenceCheck::Compile);
	// An engine asset that exists: nothing to say about it.
	Holder->EditorStartupMap = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
	McpWarnMissingReferences(Holder, EMcpMissingReferenceCheck::Save);
	Holder->MarkAsGarbage();
	Package->MarkAsGarbage();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
