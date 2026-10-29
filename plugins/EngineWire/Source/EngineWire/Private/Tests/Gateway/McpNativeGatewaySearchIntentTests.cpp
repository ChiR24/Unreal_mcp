// McpNativeGatewaySearchIntentTests.cpp — known answers for what a search request asks for beyond its words
// (Search/McpSearchIntent.cpp): the effect its first word opens, and its lead verb tied to the object.

#include "MCP/Gateway/Search/McpSearchIntent.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"
#include "MCP/Gateway/McpNativeGatewaySearchMatch.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpSearchIntentTest,
	"EngineWire.Gateway.SearchIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpSearchIntentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestEqual(TEXT("a remove opens a delete"), McpSearchIntendedEffect(TEXT("remove the crate")), FString(TEXT("destructive")));
	TestEqual(TEXT("a list opens a read"), McpSearchIntendedEffect(TEXT("list actors")), FString(TEXT("read")));
	TestEqual(TEXT("a spawn opens neither"), McpSearchIntendedEffect(TEXT("spawn a cube")), FString());

	// A record that names the verb in one topic and the object in another.
	FMcpCapabilityRecord Mesh;
	Mesh.Id = TEXT("manage_geometry.edit_mesh");
	Mesh.Effect = TEXT("write");
	Mesh.Topics = { TEXT("move a vertex"), TEXT("stack of crates") };
	const TArray<FString> AllWords = { TEXT("move"), TEXT("the"), TEXT("crate") };
	const TArray<FString> MoveCrate = { TEXT("move"), TEXT("crate") };
	TestFalse(TEXT("two topics never tie move to crate"), McpSearchLeadTied(Mesh, MoveCrate));
	TestTrue(TEXT("one topic ties move to vertex"), McpSearchLeadTied(Mesh, { TEXT("move"), TEXT("vertex") }));
	FMcpSearchMatch Loose;
	FMcpSearchMatch Tied;
	McpSearchScoreRecord(Mesh, TEXT("move the crate"), AllWords, MoveCrate, false, Loose);
	McpSearchScoreRecord(Mesh, TEXT("move the crate"), AllWords, MoveCrate, true, Tied);
	TestEqual(TEXT("a whole action named in passing loses the full-coverage bonus"), Loose.Score - Tied.Score, McpSearchFullCoverageBonus);

	const FMcpCapabilityStore& Store = FMcpCapabilityStore::Get();
	if (!TestTrue(TEXT("capability store is ready"), Store.IsReady()))
	{
		return false;
	}
	TestTrue(TEXT("move is a whole action"), McpSearchIsWholeAction(TEXT("move"), Store));
	TestFalse(TEXT("rotate is not"), McpSearchIsWholeAction(TEXT("rotate"), Store));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
