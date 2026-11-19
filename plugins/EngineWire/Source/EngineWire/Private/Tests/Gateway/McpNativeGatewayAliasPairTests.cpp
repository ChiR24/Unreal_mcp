// McpNativeGatewayAliasPairTests.cpp — a tool + action that names one record's alias (control_actor.delete_actor)
// resolves as that alias does in the capability field (FMcpCanonicalRecordIndex::FindByAliasPair).

#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Execute/McpNativeGatewayExecuteRequest.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeAliasPairTest,
	"EngineWire.Gateway.AliasPair",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNativeAliasPairTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FMcpCanonicalRecordIndex& Index = FMcpCanonicalRecordIndex::Get();
	if (!TestTrue(TEXT("the catalog is loaded"), Index.IsLoaded()))
	{
		return false;
	}
	const FMcpCapabilityRecord* Alias = Index.FindByAliasPair(TEXT("control_actor"), TEXT("delete_actor"));
	TestTrue(TEXT("an alias written as tool and action names its record"), Alias && Alias->Id == TEXT("control_actor.delete"));
	const FMcpCapabilityRecord* Invented = Index.FindByAliasPair(TEXT("system_control"), TEXT("restart_editor"));
	TestTrue(TEXT("another tool's action under the tool callers guess"), Invented && Invented->Id == TEXT("control_editor.restart_editor"));
	TestTrue(TEXT("a tool's own action is no alias pair"), Index.FindByAliasPair(TEXT("control_actor"), TEXT("delete")) == nullptr);
	TestTrue(TEXT("an invented name nothing carries resolves nothing"),
		Index.FindByAliasPair(TEXT("control_actor"), TEXT("no_such_action")) == nullptr);

	TSharedPtr<FJsonObject> Call = MakeShared<FJsonObject>();
	Call->SetStringField(TEXT("tool"), TEXT("control_actor"));
	Call->SetStringField(TEXT("action"), TEXT("delete_actor"));
	FMcpGatewayExecuteRequest Request;
	FMcpSemanticError Error;
	TSharedPtr<FJsonObject> Guidance;
	TestTrue(TEXT("execute takes it"), McpParseGatewayExecuteRequest(Call, Request, Error, Guidance));
	TestEqual(TEXT("as the capability it names"), Request.CapabilityId, FString(TEXT("control_actor.delete")));
	TestTrue(TEXT("and says which name the call used"), Request.Provenance.IsValid() && Request.Provenance->HasField(TEXT("migratedFrom")));

	// An invented id whose action another namespace owns is guided to that capability, not to the nearest spelling.
	Call = MakeShared<FJsonObject>();
	Call->SetStringField(TEXT("capability"), TEXT("control_actor.get_property"));
	TestFalse(TEXT("an invented capability is refused"), McpParseGatewayExecuteRequest(Call, Request, Error, Guidance));
	const TSharedPtr<FJsonObject>* Next = nullptr;
	FString Named;
	TestTrue(TEXT("its nextCall describes the capability with that action"), Guidance.IsValid() &&
		Guidance->TryGetObjectField(TEXT("nextCall"), Next) && (*Next)->TryGetStringField(TEXT("capability"), Named) &&
		Named == TEXT("inspect.get_property"));
	const FMcpUnknownActionGuide Far = GatewayGuideUnknownCapability(TEXT("control_actor.make_a_lamp"), Index.GetCapabilityIds());
	FString Query;
	TestTrue(TEXT("an action nothing is near is searched by its words"), Far.NextCall.IsValid() &&
		Far.NextCall->TryGetStringField(TEXT("query"), Query) && Query == TEXT("make a lamp"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
