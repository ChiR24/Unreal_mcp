// McpNativeGatewayFoldAliasTests.cpp — an alias a folded member declared runs that member. Its pair carries the alias
// (legacyIds[n].aliases), so McpRequestedLegacyAction names the pair and its pins and dispatch apply as they do for the
// old name: blueprint.add_component_to_blueprint (add_scs_component's) was refused for the selector edit_scs requires.

#include "MCP/DynamicTools/McpDynamicToolManager.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Execute/McpNativeGatewayValidation.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayFolding.h"
#include "MCP/Registry/McpToolRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

namespace
{
// An execute call by add_scs_component's alias, in the capability field or as tool and action, with what that
// variant requires and, when given, a selector value of the caller's own.
TSharedPtr<FJsonObject> FoldAliasCall(bool bAsToolAndAction, const TCHAR* Edit = nullptr)
{
	TSharedPtr<FJsonObject> Call = MakeShared<FJsonObject>();
	if (bAsToolAndAction)
	{
		Call->SetStringField(TEXT("tool"), TEXT("blueprint"));
		Call->SetStringField(TEXT("action"), TEXT("add_component_to_blueprint"));
	}
	else
	{
		Call->SetStringField(TEXT("capability"), TEXT("blueprint.add_component_to_blueprint"));
	}
	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("blueprintPath"), TEXT("/Game/FoldAliasProbe/BP_Probe"));
	Params->SetStringField(TEXT("componentName"), TEXT("Mesh"));
	Params->SetStringField(TEXT("componentClass"), TEXT("StaticMeshComponent"));
	if (Edit)
	{
		Params->SetStringField(TEXT("edit"), Edit);
	}
	Call->SetObjectField(TEXT("params"), Params);
	return Call;
}

// The plan execute makes of a call; empty when it is taken, else the refusal's errorCode.
FString FoldAliasPlan(const TSharedPtr<FJsonObject>& Call, FMcpGatewayExecutePlan& Plan)
{
	FMcpDynamicToolManager Tools;
	Tools.Initialize(FMcpToolRegistry::Get(), true);
	FMcpReceiptContext Context;
	const TSharedPtr<FJsonObject> Refusal = ValidateAndResolveGatewayExecute(Call, FMcpToolRegistry::Get(), Tools, Context, Plan);
	FString Code;
	if (Refusal.IsValid() && !Refusal->TryGetStringField(TEXT("errorCode"), Code))
	{
		Code = TEXT("REFUSED");
	}
	return Code;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeFoldAliasTest,
	"EngineWire.Gateway.FoldAlias",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNativeFoldAliasTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FMcpCanonicalRecordIndex& Index = FMcpCanonicalRecordIndex::Get();
	if (!TestTrue(TEXT("the catalog is loaded"), Index.IsLoaded()))
	{
		return false;
	}
	const bool Forms[] = {false, true};
	for (const bool bAsToolAndAction : Forms)
	{
		FMcpGatewayExecutePlan Plan;
		TestEqual(TEXT("execute takes the alias with no selector"), FoldAliasPlan(FoldAliasCall(bAsToolAndAction), Plan), FString());
		TestEqual(TEXT("as its family"), Plan.CapabilityId, FString(TEXT("blueprint.edit_scs")));
		TestEqual(TEXT("running the variant that declared it, by that variant's own action"), Plan.LegacyAction, FString(TEXT("add_scs_component")));
		FString Edit;
		TestTrue(TEXT("with the selector its pair pins"),
			Plan.Arguments.IsValid() && Plan.Arguments->TryGetStringField(TEXT("edit"), Edit) && Edit == TEXT("add_scs_component"));
	}
	FMcpGatewayExecutePlan Contradicted;
	TestEqual(TEXT("a selector the alias contradicts is refused, as it is for the old name"),
		FoldAliasPlan(FoldAliasCall(false, TEXT("reparent")), Contradicted), FString(TEXT("INVALID_PARAMETER_VALUE")));

	// Unchanged: a pair the action names, the primary's own alias, and an alias whose last segment names a pair.
	const FMcpCapabilityRecord* Scs = Index.FindById(TEXT("blueprint.edit_scs"));
	const FMcpCapabilityRecord* Delete = Index.FindById(TEXT("control_actor.delete"));
	const FMcpCapabilityRecord* Volume = Index.FindById(TEXT("manage_level_structure.create_volume"));
	if (!TestTrue(TEXT("the records are in the catalog"), Scs && Delete && Volume))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Named = FoldAliasCall(false);
	Named->SetStringField(TEXT("tool"), TEXT("manage_blueprint"));
	Named->SetStringField(TEXT("action"), TEXT("modify_scs"));
	TestEqual(TEXT("an action naming a pair wins over the alias"), McpRequestedLegacyAction(Named, *Scs), FString(TEXT("modify_scs")));
	TSharedPtr<FJsonObject> PrimaryAlias = MakeShared<FJsonObject>();
	PrimaryAlias->SetStringField(TEXT("capability"), TEXT("control_actor.delete_actor"));
	TestEqual(TEXT("the primary's own alias runs the primary"),
		McpResolveDispatchAction(*Delete, McpRequestedLegacyAction(PrimaryAlias, *Delete), MakeShared<FJsonObject>(), TEXT("delete")),
		FString(TEXT("delete")));
	TSharedPtr<FJsonObject> PairAlias = MakeShared<FJsonObject>();
	PairAlias->SetStringField(TEXT("capability"), TEXT("manage_level_structure.create_trigger_volume"));
	TestEqual(TEXT("an alias whose last segment names a pair keeps that pair"),
		McpRequestedLegacyAction(PairAlias, *Volume), FString(TEXT("create_trigger_volume")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
