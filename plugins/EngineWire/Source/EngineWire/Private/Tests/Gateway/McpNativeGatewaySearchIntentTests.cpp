// McpNativeGatewaySearchIntentTests.cpp — known answers for what a search request asks for beyond its words
// (Search/McpSearchIntent.cpp): the effect its first word opens, its lead verb tied to the object, and the action
// words the id rule scores (an alias asked for by a word that sets it apart); and for the phrase rule, which reads a
// request as whole words.

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
	const TSet<FString> NoGeneric;
	McpSearchScoreRecord(Mesh, TEXT("move the crate"), AllWords, MoveCrate, false, NoGeneric, Loose);
	McpSearchScoreRecord(Mesh, TEXT("move the crate"), AllWords, MoveCrate, true, NoGeneric, Tied);
	TestEqual(TEXT("a whole action named in passing loses the full-coverage bonus"), Loose.Score - Tied.Score, McpSearchFullCoverageBonus);

	// Words spread over several aliases name no single action: one alias scores, every held word stays matched.
	FMcpCapabilityRecord Family;
	Family.Id = TEXT("manage_geometry.edit_mesh");
	Family.Domain = TEXT("geometry");
	Family.Aliases = { TEXT("manage_geometry.create_patch"), TEXT("manage_geometry.set_material_id"), TEXT("manage_geometry.set_vertex_color") };
	const TArray<FString> Request = { TEXT("create"), TEXT("material"), TEXT("color") };
	TArray<FString> Held;
	TArray<FString> Scored;
	McpSearchActionWords(Family, Request, false, NoGeneric, Held, Scored);
	TestEqual(TEXT("every word an alias holds is held"), Held.Num(), 3);
	TestEqual(TEXT("three aliases holding a word each score one word"), Scored.Num(), 1);
	Family.Domain = TEXT("material");
	Held.Reset();
	Scored.Reset();
	McpSearchActionWords(Family, Request, false, NoGeneric, Held, Scored);
	TestEqual(TEXT("a word the domain names too scores beside the best alias"), Scored.Num(), 2);

	// A request that opens with a verb asks for an alias only by a word that sets it apart: spline, not mesh or component.
	FMcpCapabilityRecord Spline;
	Spline.Id = TEXT("build_environment.create_spline");
	Spline.Aliases = { TEXT("build_environment.create_spline_mesh_component") };
	const TSet<FString> Generic = { TEXT("create"), TEXT("actor"), TEXT("mesh"), TEXT("component") };
	const TArray<FString> Plain = { TEXT("create"), TEXT("actor"), TEXT("mesh"), TEXT("component") };
	Held.Reset();
	Scored.Reset();
	McpSearchActionWords(Spline, Plain, true, Generic, Held, Scored);
	TestEqual(TEXT("generic words of an alias not asked for are held"), Held.Num(), 3);
	TestEqual(TEXT("and only the action's own verb scores"), Scored.Num(), 1);
	const TArray<FString> Named = { TEXT("create"), TEXT("spline"), TEXT("mesh"), TEXT("component") };
	Held.Reset();
	Scored.Reset();
	McpSearchActionWords(Spline, Named, true, Generic, Held, Scored);
	TestEqual(TEXT("the alias asked for by name scores all its words"), Scored.Num(), 4);

	// A request found inside a longer word of a topic is not that topic's request.
	FMcpCapabilityRecord Unlink;
	Unlink.Id = TEXT("blueprint.delete_node");
	Unlink.Topics = { TEXT("unlink pins") };
	const TArray<FString> LinkPins = { TEXT("link"), TEXT("pin") };
	FMcpSearchMatch Inside;
	McpSearchScoreRecord(Unlink, TEXT("link pins"), LinkPins, LinkPins, false, NoGeneric, Inside);
	TestTrue(TEXT("a phrase inside a longer word earns no phrase bonus"), Inside.Score < McpSearchPhraseTopicBonus);

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
