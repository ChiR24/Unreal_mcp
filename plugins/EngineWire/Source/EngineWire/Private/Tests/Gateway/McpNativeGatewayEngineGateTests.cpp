// McpNativeGatewayEngineGateTests.cpp — a record whose engine role nothing plays on this engine, or whose compiled
// feature this build left out, reads unavailable in search and describe and is refused by execute's availability gate
// (McpNativeGatewayCatalog.cpp).

#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "Core/Compatibility/McpCompiledFeatures.h"
#include "Foundation/Reflect/McpEngineRoles.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
FMcpCapabilityRecord RoleGateTestRecord(std::initializer_list<const TCHAR*> Roles)
{
	FMcpCapabilityRecord Record;
	Record.Id = TEXT("role_gate_test.probe");
	Record.Parent = TEXT("role_gate_test");
	Record.Availability = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Names;
	for (const TCHAR* Role : Roles)
	{
		Names.Add(MakeShared<FJsonValueString>(Role));
	}
	if (Names.Num() > 0)
	{
		Record.Availability->SetArrayField(TEXT("roles"), Names);
	}
	return Record;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeRoleGateTest,
	"EngineWire.Gateway.RoleGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNativeRoleGateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	McpEngineRoles::RefreshAll(); // what search and describe read, published as the editor's first tick does
	FString Code;
	FString Message;

	const FMcpCapabilityRecord Plain = RoleGateTestRecord({});
	TestTrue(TEXT("a record that names no role runs here"), McpRunsOnThisEngine(Plain));
	TestTrue(TEXT("and passes the gate"), McpCheckAvailabilityGate(Plain, Code, Message));

	const FMcpCapabilityRecord Missing = RoleGateTestRecord({TEXT("No.Such.Role")});
	const TSharedPtr<FJsonObject> View = McpCapabilityAvailabilityView(Missing, true);
	TestEqual(TEXT("a role nothing plays reads unavailable"), View->GetStringField(TEXT("status")), FString(TEXT("unavailable")));
	const TArray<TSharedPtr<FJsonValue>>* Reasons = nullptr;
	TestTrue(TEXT("for engine_role"), View->TryGetArrayField(TEXT("reasons"), Reasons) && Reasons &&
		Reasons->Num() == 1 && (*Reasons)[0]->AsString() == TEXT("engine_role"));
	TestEqual(TEXT("naming the role"), View->GetStringField(TEXT("missingRole")), FString(TEXT("No.Such.Role")));
	TestFalse(TEXT("saying why"), View->GetStringField(TEXT("missingRoleWhy")).IsEmpty());
	TestEqual(TEXT("a disabled tool still reads disabled"),
		McpCapabilityAvailabilityView(Missing, false)->GetStringField(TEXT("status")), FString(TEXT("disabled")));
	TestFalse(TEXT("its search row is not runnable"), McpSearchRow(Missing, true)->GetBoolField(TEXT("runnable")));
	TestFalse(TEXT("execute refuses it"), McpCheckAvailabilityGate(Missing, Code, Message));
	TestEqual(TEXT("with FEATURE_UNAVAILABLE"), Code, FString(TEXT("FEATURE_UNAVAILABLE")));
	TestTrue(TEXT("naming the role"), Message.Contains(TEXT("No.Such.Role")));

	TestFalse(TEXT("an empty role name fails closed"), McpRunsOnThisEngine(RoleGateTestRecord({TEXT("")})));
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
