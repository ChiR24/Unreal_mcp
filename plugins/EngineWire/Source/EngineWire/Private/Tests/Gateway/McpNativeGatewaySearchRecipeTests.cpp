// McpNativeGatewaySearchRecipeTests.cpp — search lists a saved recipe whose name and summary name the request's words,
// beside the capabilities, on an unfiltered first page and for a caller that may read the project only
// (McpNativeGatewaySearch.cpp, Domains/Scripts/McpRecipesSearch.cpp).

#include "MCP/Gateway/McpNativeGatewaySearch.h"
#include "Domains/Scripts/McpRecipes.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
/** How many recipe rows a search lists, and the first one's name; bRead is whether the caller may read the project. */
int32 RecipeSearchTestRows(const TCHAR* Query, const TCHAR* Tool, int32 Offset, FString& OutFirst, bool bRead = true)
{
	FMcpDiscoveryQuery Input;
	Input.Query = Query;
	Input.Offset = Offset;
	Input.bMayReadProject = bRead;
	Input.bHasTool = Tool != nullptr;
	Input.Tool = Tool != nullptr ? Tool : TEXT("");
	const TSharedPtr<FJsonObject> Out =
		McpGatewaySearchCapabilities(Input, FMcpCapabilityStore::Get(), [](const FString&) { return true; });
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!Out.IsValid() || !Out->TryGetArrayField(TEXT("recipes"), Rows) || !Rows || Rows->Num() == 0)
	{
		return 0;
	}
	OutFirst = (*Rows)[0]->AsObject()->GetStringField(TEXT("name"));
	return Rows->Num();
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpNativeSearchRecipeTest,
	"EngineWire.Gateway.SearchRecipes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpNativeSearchRecipeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// A recipe of the test's own, removed at the end; its made-up words match nothing else a project keeps.
	const FString Name = TEXT("zz_mcp_search_probe");
	const FString Path = FPaths::Combine(McpRecipes::Dir(), Name + TEXT(".json"));
	IFileManager::Get().MakeDirectory(*McpRecipes::Dir(), true);
	if (!TestTrue(TEXT("the test recipe is written"), FFileHelper::SaveStringToFile(
			TEXT("{\"schema\":1,\"name\":\"zz_mcp_search_probe\",\"summary\":\"Stack every zzqcrate onto the zzqpallet.\",")
			TEXT("\"paramNames\":[\"height\"],\"destructive\":[],\"script\":\"(return 1)\"}"), *Path)))
	{
		return false;
	}
	FString First;
	TestEqual(TEXT("two words it names list it"), RecipeSearchTestRows(TEXT("stack the zzqcrates"), nullptr, 0, First), 1);
	TestEqual(TEXT("by its name"), First, Name);
	TestEqual(TEXT("one of two words does not"), RecipeSearchTestRows(TEXT("zzqcrate sorting"), nullptr, 0, First), 0);
	TestEqual(TEXT("the one word asked for does"), RecipeSearchTestRows(TEXT("zzqpallet"), nullptr, 0, First), 1);
	TestEqual(TEXT("not under a filter"), RecipeSearchTestRows(TEXT("stack the zzqcrates"), TEXT("system_control"), 0, First), 0);
	TestEqual(TEXT("not past the first page"), RecipeSearchTestRows(TEXT("stack the zzqcrates"), nullptr, 5, First), 0);
	TestEqual(TEXT("not for a caller without the read scope"), RecipeSearchTestRows(TEXT("stack the zzqcrates"), nullptr, 0, First, false), 0);
	IFileManager::Get().Delete(*Path);
	TestEqual(TEXT("gone once removed"), RecipeSearchTestRows(TEXT("stack the zzqcrates"), nullptr, 0, First), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
