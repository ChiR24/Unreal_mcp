// McpNativeGatewaySearchCamelCaseTests.cpp — known answers for search's CamelCase reading
// (Search/McpSearchCamelCase.cpp), which tests/eval/native-ranker.ts copies.

#include "MCP/Gateway/Search/McpSearchCamelCase.h"
#include "MCP/Gateway/McpNativeGatewaySearchMatch.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpSearchCamelCaseTest,
	"EngineWire.Gateway.SearchCamelCase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpSearchCamelCaseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const auto Split = [](const TCHAR* Text) { return McpSplitCamelCase(Text); };
	TestEqual(TEXT("a class name reads as its words"), Split(TEXT("create GameplayEffect")), FString(TEXT("create Gameplay Effect")));
	TestEqual(TEXT("an upper-case run ends before its last letter"), Split(TEXT("DSLParser PCGGraph")), FString(TEXT("DSL Parser PCG Graph")));
	TestEqual(TEXT("a digit is never a boundary"), Split(TEXT("Texture2D K2Node UE5")), FString(TEXT("Texture2D K2Node UE5")));
	TestEqual(TEXT("a plural s stays on its acronym"), Split(TEXT("LODs NPCs UVs")), FString(TEXT("LODs NPCs UVs")));
	TestEqual(TEXT("a lower-case prefix of an acronym stays on it"), Split(TEXT("sRGB iOS macOS")), FString(TEXT("sRGB iOS macOS")));
	TestEqual(TEXT("a one-letter type prefix stays on its name"), Split(TEXT("UObject AActor FVector")), FString(TEXT("UObject AActor FVector")));
	TestEqual(TEXT("lower case and ids are left alone"), Split(TEXT("manage_gas.create_gas_asset")), FString(TEXT("manage_gas.create_gas_asset")));
	TestEqual(TEXT("punctuation separates independently split runs"), Split(TEXT("[PCGGraphNode]/Texture2DArray")),
		FString(TEXT("[PCG Graph Node]/Texture2D Array")));
	TestEqual(TEXT("an empty query stays empty"), Split(TEXT("")), FString());

	// An engine name a summary spells whole is held for each of the words it joins, and only for those.
	const TArray<FMcpJoinedSpan> Spans = McpSearchJoinedSpans(TEXT("create a sound cues"), { TEXT("create"), TEXT("sound"), TEXT("cue") });
	TestTrue(TEXT("SoundCue holds sound"), McpSearchFieldHolds(TEXT("Make a SoundCue."), TEXT("sound"), Spans, true));
	TestTrue(TEXT("SoundCues holds cue"), McpSearchFieldHolds(TEXT("Edit SoundCues."), TEXT("cue"), Spans, true));
	TestFalse(TEXT("SoundCue does not hold create"), McpSearchFieldHolds(TEXT("Make a SoundCue."), TEXT("create"), Spans, true));
	TestFalse(TEXT("a field name is not an engine name"), McpSearchFieldHolds(TEXT("names soundCue"), TEXT("sound"), Spans, true));
	TestTrue(TEXT("a topic counts any spelling"), McpSearchFieldHolds(TEXT("edit soundcue"), TEXT("sound"), Spans, false));
	TestTrue(TEXT("joined as typed, then folded"), McpSearchFieldHolds(TEXT("its DirectoriesToScan"), TEXT("directory"),
		McpSearchJoinedSpans(TEXT("directories to scan"), { TEXT("directory"), TEXT("scan") }), true));
	TestFalse(TEXT("a lone word is no span"),
		McpSearchFieldHolds(TEXT("LightColor"), TEXT("color"), McpSearchJoinedSpans(TEXT("color"), { TEXT("color") }), true));

	// Through the gateway: an engine name typed whole ranks what its spaced spelling ranks.
	const FMcpCapabilityStore& Store = FMcpCapabilityStore::Get();
	if (!TestTrue(TEXT("capability store is ready"), Store.IsReady()))
	{
		return false;
	}
	const auto First = [&Store](const TCHAR* Query)
	{
		FMcpDiscoveryQuery Input;
		Input.Query = Query;
		Input.Limit = 1;
		const TSharedPtr<FJsonObject> Reply = McpGatewaySearchCapabilities(Input, Store, [](const FString&) { return true; });
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		FString Id;
		if (Reply.IsValid() && Reply->TryGetArrayField(TEXT("results"), Rows) && Rows->Num() > 0)
		{
			(*Rows)[0]->AsObject()->TryGetStringField(TEXT("capability"), Id);
		}
		return Id;
	};
	TestEqual(TEXT("create GameplayEffect"), First(TEXT("create GameplayEffect")), FString(TEXT("manage_gas.create_gas_asset")));
	TestEqual(TEXT("create PlayerController"), First(TEXT("create PlayerController")), First(TEXT("create player controller")));
	TestEqual(TEXT("create MetaSound"), First(TEXT("create MetaSound")), FString(TEXT("manage_audio.edit_metasound")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpSearchJoinedSpanTest,
	"EngineWire.Gateway.SearchCamelCase.JoinedSpans",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpSearchJoinedSpanTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TArray<FString> Content = { TEXT("sound"), TEXT("cue"), TEXT("graph"), TEXT("node"), TEXT("asset") };
	const TArray<FMcpJoinedSpan> Spans = McpSearchJoinedSpans(TEXT("sound cue graph node asset"), Content);
	for (const TCHAR* Word : { TEXT("sound"), TEXT("cue"), TEXT("graph"), TEXT("node") })
	{
		TestTrue(FString::Printf(TEXT("a four-word name covers %s"), Word),
			McpSearchFieldHolds(TEXT("(SoundCueGraphNode)"), Word, Spans, true));
	}
	TestTrue(TEXT("a maximum-length span can end at the last query word"),
		McpSearchFieldHolds(TEXT("CueGraphNodeAsset"), TEXT("asset"), Spans, true));
	TestFalse(TEXT("a five-word name exceeds the span limit"),
		McpSearchFieldHolds(TEXT("SoundCueGraphNodeAsset"), TEXT("sound"), Spans, true));
	TestFalse(TEXT("a span cannot skip intervening words"),
		McpSearchFieldHolds(TEXT("SoundGraph"), TEXT("sound"), Spans, true));
	TestFalse(TEXT("a joined name must be a whole token"),
		McpSearchFieldHolds(TEXT("PreSoundCueGraphNode"), TEXT("cue"), Spans, false));
	const TArray<FMcpJoinedSpan> WithFunctionWord =
		McpSearchJoinedSpans(TEXT("sound and cue"), { TEXT("sound"), TEXT("cue") });
	TestFalse(TEXT("a function word is not covered by a joined name"),
		McpSearchFieldHolds(TEXT("SoundAndCue"), TEXT("and"), WithFunctionWord, true));
	TestFalse(TEXT("a function word cannot be skipped even though it does not score"),
		McpSearchFieldHolds(TEXT("SoundCue"), TEXT("cue"), WithFunctionWord, true));
	TestTrue(TEXT("a literal summary word needs no capital or joined span"),
		McpSearchFieldHolds(TEXT("edit the cue"), TEXT("cue"), {}, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpSearchCamelCaseRecordTest,
	"EngineWire.Gateway.SearchCamelCase.RecordScoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpSearchCamelCaseRecordTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString Query = TEXT("sound cue");
	const TArray<FString> Words = { TEXT("sound"), TEXT("cue") };
	const TArray<FMcpJoinedSpan> Spans = McpSearchJoinedSpans(Query, Words);
	FMcpCapabilityRecord Record;
	Record.Id = TEXT("test.example");
	Record.Topics = { TEXT("soundcue") };
	Record.Summary = TEXT("Returns soundCue.");
	FMcpSearchMatch TopicMatch;
	TestTrue(TEXT("a joined topic makes the record a result"),
		McpSearchScoreRecord(Record, Query, Words, Words, false, {}, Spans, TopicMatch));
	TestTrue(TEXT("the topic is the only reason despite the lower-camel summary"),
		TopicMatch.Reasons == TArray<FString>{ TEXT("topic") });

	Record.Topics.Reset();
	Record.Family = TEXT("soundcue");
	Record.Domain = TEXT("soundcue");
	Record.Parent = TEXT("soundcue");
	FMcpSearchMatch IgnoredMatch;
	TestFalse(TEXT("a summary field and joined identifiers do not make the record a result"),
		McpSearchScoreRecord(Record, Query, Words, Words, false, {}, Spans, IgnoredMatch));

	Record.Summary = TEXT("Returns a SoundCue.");
	FMcpSearchMatch SummaryMatch;
	TestTrue(TEXT("a capitalized summary name makes the record a result"),
		McpSearchScoreRecord(Record, Query, Words, Words, false, {}, Spans, SummaryMatch));
	TestTrue(TEXT("the capitalized name fires only the summary rule"),
		SummaryMatch.Reasons == TArray<FString>{ TEXT("summary") });
	TestTrue(TEXT("the joined topic ranks higher than the joined summary"), TopicMatch.Score > SummaryMatch.Score);

	FMcpSearchMatch ComputedMatch;
	TestTrue(TEXT("scoring without cached spans also recognizes the name"),
		McpSearchScoreRecord(Record, Query, Words, Words, false, {}, ComputedMatch));
	TestEqual(TEXT("the two scoring entry points assign the same score"), ComputedMatch.Score, SummaryMatch.Score);
	TestTrue(TEXT("the two scoring entry points report the same reasons"), ComputedMatch.Reasons == SummaryMatch.Reasons);
	return true;
}
#endif
