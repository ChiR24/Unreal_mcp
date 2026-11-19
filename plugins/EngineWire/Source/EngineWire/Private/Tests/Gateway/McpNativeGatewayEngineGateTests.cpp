// McpNativeGatewayEngineGateTests.cpp — a record that declares a newer engine than this editor's reads unavailable in
// search and describe and is refused by execute's availability gate (McpNativeGatewayCatalog.cpp).

#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "Core/Compatibility/McpCompiledFeatures.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "Runtime/Launch/Resources/Version.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
FMcpCapabilityRecord EngineGateTestRecord(const FString& MinEngineVersion)
{
	FMcpCapabilityRecord Record;
	Record.Id = TEXT("engine_gate_test.probe");
	Record.Parent = TEXT("engine_gate_test");
	Record.Availability = MakeShared<FJsonObject>();
	Record.Availability->SetStringField(TEXT("minEngineVersion"), MinEngineVersion);
	return Record;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeEngineGateTest,
	"EngineWire.Gateway.EngineGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNativeEngineGateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestFalse(TEXT("5.3 is older than 5.4"), McpEngineVersionAtLeast(TEXT("5.4"), 5, 3));
	TestTrue(TEXT("5.4 meets 5.4"), McpEngineVersionAtLeast(TEXT("5.4"), 5, 4));
	TestTrue(TEXT("5.8 meets 5.4"), McpEngineVersionAtLeast(TEXT("5.4"), 5, 8));
	TestTrue(TEXT("a later major meets it"), McpEngineVersionAtLeast(TEXT("5.4"), 6, 0));
	TestFalse(TEXT("an earlier major does not"), McpEngineVersionAtLeast(TEXT("5.4"), 4, 27));
	TestFalse(TEXT("one number is no version"), McpEngineVersionAtLeast(TEXT("5"), 5, 8));
	TestFalse(TEXT("words are no version"), McpEngineVersionAtLeast(TEXT("five.four"), 5, 8));

	const FString Here = FString::Printf(TEXT("%d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION);
	const FString Next = FString::Printf(TEXT("%d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION + 1);
	FString Code;
	FString Message;

	const FMcpCapabilityRecord Runs = EngineGateTestRecord(Here);
	TestTrue(TEXT("a record for this engine runs here"), McpRunsOnThisEngine(Runs));
	TestEqual(TEXT("and reads available"), McpCapabilityAvailabilityView(Runs, true)->GetStringField(TEXT("status")), FString(TEXT("available")));
	TestTrue(TEXT("and passes the gate"), McpCheckAvailabilityGate(Runs, Code, Message));

	const FMcpCapabilityRecord Newer = EngineGateTestRecord(Next);
	const TSharedPtr<FJsonObject> View = McpCapabilityAvailabilityView(Newer, true);
	TestEqual(TEXT("a newer engine's record reads unavailable"), View->GetStringField(TEXT("status")), FString(TEXT("unavailable")));
	const TArray<TSharedPtr<FJsonValue>>* Reasons = nullptr;
	TestTrue(TEXT("for the engine version"), View->TryGetArrayField(TEXT("reasons"), Reasons) && Reasons &&
		Reasons->Num() == 1 && (*Reasons)[0]->AsString() == TEXT("engine_version"));
	TestEqual(TEXT("naming the version it needs"), View->GetStringField(TEXT("minEngineVersion")), Next);
	TestEqual(TEXT("a disabled tool still reads disabled"),
		McpCapabilityAvailabilityView(Newer, false)->GetStringField(TEXT("status")), FString(TEXT("disabled")));
	TestFalse(TEXT("its search row is not runnable"), McpSearchRow(Newer, true)->GetBoolField(TEXT("runnable")));
	TestFalse(TEXT("execute refuses it"), McpCheckAvailabilityGate(Newer, Code, Message));
	TestEqual(TEXT("with ENGINE_VERSION_UNSUPPORTED"), Code, FString(TEXT("ENGINE_VERSION_UNSUPPORTED")));
	TestTrue(TEXT("naming both versions"), Message.Contains(Next) && Message.Contains(Here));
	return true;
}

// A record's availability.compiled names features this build must have compiled (MCP_COMPILED_FEATURE).
MCP_COMPILED_FEATURE(MCP_GATE_TEST_FEATURE)

namespace McpCompiledGateTestData
{
FMcpCapabilityRecord Record(std::initializer_list<const TCHAR*> Features)
{
	FMcpCapabilityRecord Out;
	Out.Id = TEXT("compiled_gate_test.probe");
	Out.Parent = TEXT("compiled_gate_test");
	Out.Availability = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Names;
	for (const TCHAR* Feature : Features)
	{
		Names.Add(MakeShared<FJsonValueString>(Feature));
	}
	Out.Availability->SetArrayField(TEXT("compiled"), Names);
	return Out;
}
} // namespace McpCompiledGateTestData

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeCompiledGateTest,
	"EngineWire.Gateway.CompiledGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNativeCompiledGateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Code;
	FString Message;
	const FMcpCapabilityRecord Built = McpCompiledGateTestData::Record({TEXT("MCP_GATE_TEST_FEATURE")});
	TestTrue(TEXT("a feature this build registered runs here"), McpRunsOnThisEngine(Built));
	TestEqual(TEXT("and reads available"), McpCapabilityAvailabilityView(Built, true)->GetStringField(TEXT("status")), FString(TEXT("available")));
	TestTrue(TEXT("and passes the gate"), McpCheckAvailabilityGate(Built, Code, Message));

	const FMcpCapabilityRecord Left = McpCompiledGateTestData::Record({TEXT("MCP_GATE_TEST_FEATURE"), TEXT("MCP_NEVER_REGISTERED")});
	const TSharedPtr<FJsonObject> View = McpCapabilityAvailabilityView(Left, true);
	TestEqual(TEXT("one this build left out reads unavailable"), View->GetStringField(TEXT("status")), FString(TEXT("unavailable")));
	TestEqual(TEXT("naming the feature"), View->GetStringField(TEXT("missingFeature")), FString(TEXT("MCP_NEVER_REGISTERED")));
	const TArray<TSharedPtr<FJsonValue>>* Reasons = nullptr;
	TestTrue(TEXT("for not_compiled"), View->TryGetArrayField(TEXT("reasons"), Reasons) && Reasons &&
		Reasons->Num() == 1 && (*Reasons)[0]->AsString() == TEXT("not_compiled"));
	TestFalse(TEXT("its search row is not runnable"), McpSearchRow(Left, true)->GetBoolField(TEXT("runnable")));
	TestFalse(TEXT("execute refuses it"), McpCheckAvailabilityGate(Left, Code, Message));
	TestEqual(TEXT("with FEATURE_UNAVAILABLE"), Code, FString(TEXT("FEATURE_UNAVAILABLE")));
	TestTrue(TEXT("naming the feature"), Message.Contains(TEXT("MCP_NEVER_REGISTERED")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
