// McpSearchRowDietTests.cpp — a search row carries what a model needs to choose (McpNativeGatewayCatalog.cpp): the
// summary's first sentence, the availability block only when the capability does not run here, the policy only when
// the call needs a grant, and no output list.

#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpSearchRowDietTest,
	"EngineWire.Gateway.SearchRowDiet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpSearchRowDietTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestEqual(TEXT("the first sentence"), McpOneLineSummary(TEXT("Create a thing, and save it. parentClass is a class.")),
		FString(TEXT("Create a thing, and save it.")));
	TestEqual(TEXT("not cut inside e.g."), McpOneLineSummary(TEXT("Read a value (e.g. a name) from it. More here.")),
		FString(TEXT("Read a value (e.g. a name) from it.")));
	TestEqual(TEXT("one sentence stays whole"), McpOneLineSummary(TEXT("Read a value.")), FString(TEXT("Read a value.")));

	FMcpCapabilityRecord Record;
	Record.Id = TEXT("row_diet_test.probe");
	Record.Parent = TEXT("row_diet_test");
	Record.Summary = TEXT("Do the thing. Then a long tail no row needs.");
	Record.Availability = MakeShared<FJsonObject>();
	Record.Policy = MakeShared<FJsonObject>();
	Record.Policy->SetStringField(TEXT("consent"), TEXT("none"));
	Record.OutputSchema = MakeShared<FJsonObject>();
	Record.OutputSchema->SetObjectField(TEXT("properties"), MakeShared<FJsonObject>());

	const TSharedPtr<FJsonObject> Plain = McpSearchRow(Record, true);
	TestEqual(TEXT("a row's summary is its first sentence"), Plain->GetStringField(TEXT("summary")), FString(TEXT("Do the thing.")));
	TestFalse(TEXT("no availability block when it runs here"), Plain->HasField(TEXT("availability")));
	TestFalse(TEXT("no policy when no grant is needed"), Plain->HasField(TEXT("policy")));
	TestFalse(TEXT("no output list"), Plain->HasField(TEXT("outputs")));
	TestTrue(TEXT("it says it is runnable"), Plain->GetBoolField(TEXT("runnable")));

	Record.Policy->SetStringField(TEXT("consent"), TEXT("explicit"));
	TestTrue(TEXT("a call that needs a grant shows its policy"), McpSearchRow(Record, true)->HasField(TEXT("policy")));
	TestTrue(TEXT("a disabled tool's row says why it does not run"), McpSearchRow(Record, false)->HasField(TEXT("availability")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
