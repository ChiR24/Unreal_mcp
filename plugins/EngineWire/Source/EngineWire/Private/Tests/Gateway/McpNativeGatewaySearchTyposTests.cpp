// McpNativeGatewaySearchTyposTests.cpp — known answers for search's typo correction (Search/McpSearchTypos.cpp) and for
// options.aggregate (Execute/Request/McpExecuteAggregate.cpp).

#include "MCP/Gateway/Search/McpSearchTypos.h"
#include "MCP/Execute/Request/McpExecuteAggregate.h"
#include "MCP/Execute/McpNativeGatewayValidation.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpSearchTyposTest,
	"EngineWire.Gateway.SearchTypos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpSearchTyposTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestEqual(TEXT("a swap is one edit"), McpEditDistance(TEXT("spwan"), TEXT("spawn"), 1), 1);
	TestEqual(TEXT("a missing letter is one edit"), McpEditDistance(TEXT("delte"), TEXT("delete"), 1), 1);
	TestEqual(TEXT("a slip onto a neighbouring key is one edit"), McpEditDistance(TEXT("lighr"), TEXT("light"), 1), 1);
	TestEqual(TEXT("any other substitution is two"), McpEditDistance(TEXT("carve"), TEXT("curve"), 2), 2);
	TestEqual(TEXT("past the limit reads limit + 1"), McpEditDistance(TEXT("abcdef"), TEXT("uvwxyz"), 1), 2);

	const FMcpCapabilityStore& Store = FMcpCapabilityStore::Get();
	if (!TestTrue(TEXT("capability store is ready"), Store.IsReady()))
	{
		return false;
	}
	TArray<FMcpQueryCorrection> Fixes;
	TestEqual(TEXT("a typo is corrected"), McpCorrectQuery(TEXT("spwan actor"), Store, Fixes), FString(TEXT("spawn actor")));
	TestTrue(TEXT("and said"), Fixes.Num() == 1 && Fixes[0].From == TEXT("spwan") && Fixes[0].To == TEXT("spawn"));
	Fixes.Reset();
	TestEqual(TEXT("a word of four letters by a swap"), McpCorrectQuery(TEXT("lsit actors"), Store, Fixes), FString(TEXT("list actors")));
	Fixes.Reset();
	TestEqual(TEXT("a correct request is left alone"), McpCorrectQuery(TEXT("spawn actor"), Store, Fixes), FString(TEXT("spawn actor")));
	TestEqual(TEXT("a real word the catalog lacks is left alone"), McpCorrectQuery(TEXT("carve valleys"), Store, Fixes), FString(TEXT("carve valleys")));
	TestEqual(TEXT("nothing was corrected"), Fixes.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpExecuteAggregateTest,
	"EngineWire.Gateway.Aggregate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpExecuteAggregateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const auto Item = [](const TCHAR* Name, const TCHAR* Class) -> TSharedPtr<FJsonValue>
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("name"), Name);
		Object->SetStringField(TEXT("class"), Class);
		return MakeShared<FJsonValueObject>(Object);
	};
	TArray<TSharedPtr<FJsonValue>> Tags;
	Tags.Add(MakeShared<FJsonValueString>(TEXT("x")));
	TArray<TSharedPtr<FJsonValue>> Actors;
	Actors.Add(Item(TEXT("A"), TEXT("Cube")));
	Actors.Add(Item(TEXT("B"), TEXT("Light")));
	Actors.Add(Item(TEXT("C"), TEXT("Cube")));
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("tags"), Tags);
	Result->SetArrayField(TEXT("actors"), Actors);
	TSharedPtr<FJsonObject> Spec = MakeShared<FJsonObject>();
	Spec->SetStringField(TEXT("groupBy"), TEXT("class"));
	const TSharedPtr<FJsonObject> Block = McpExecuteAggregate::Apply(Spec, nullptr, Result);
	TestEqual(TEXT("the largest list is counted"), Block->GetStringField(TEXT("field")), FString(TEXT("actors")));
	TestEqual(TEXT("all of it"), static_cast<int32>(Block->GetNumberField(TEXT("total"))), 3);
	const TArray<TSharedPtr<FJsonValue>> Groups = Block->GetArrayField(TEXT("groups"));
	TestTrue(TEXT("most common group first"), Groups.Num() == 2 && Groups[0]->AsObject()->GetStringField(TEXT("value")) == TEXT("Cube") &&
		static_cast<int32>(Groups[0]->AsObject()->GetNumberField(TEXT("count"))) == 2);
	TestTrue(TEXT("sampled by name"), Groups[0]->AsObject()->GetArrayField(TEXT("sample")).Num() == 2 &&
		Groups[0]->AsObject()->GetArrayField(TEXT("sample"))[1]->AsString() == TEXT("C"));
	TestEqual(TEXT("the list leaves the reply"), Result->GetArrayField(TEXT("actors")).Num(), 0);
	TestEqual(TEXT("the other list stays"), Result->GetArrayField(TEXT("tags")).Num(), 1);
	TestFalse(TEXT("a whole list needs no note"), Block->HasField(TEXT("note")));
	TSharedPtr<FJsonObject> Page = MakeShared<FJsonObject>();
	Page->SetArrayField(TEXT("actors"), Actors);
	Page->SetNumberField(TEXT("totalCount"), 10);
	TestTrue(TEXT("a page of a longer list says it counted the page"),
		McpExecuteAggregate::Apply(Spec, nullptr, Page)->GetStringField(TEXT("note")).Contains(TEXT("this page only, of 10 in all")));
	Spec->SetStringField(TEXT("field"), TEXT("missing"));
	TestTrue(TEXT("a list the reply lacks is said"), McpExecuteAggregate::Apply(Spec, nullptr, Result)->HasField(TEXT("note")));

	// The standard envelope ({success, data, warnings}): the list in data is the one counted, as the receipt build lifts it.
	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetArrayField(TEXT("actors"), Actors);
	Payload->SetNumberField(TEXT("count"), 3);
	TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
	Envelope->SetBoolField(TEXT("success"), true);
	Envelope->SetObjectField(TEXT("data"), Payload);
	Envelope->SetArrayField(TEXT("warnings"), TArray<TSharedPtr<FJsonValue>>());
	TSharedPtr<FJsonObject> Lifted = McpLiftReplyPayload(Envelope);
	TestFalse(TEXT("the payload is no longer wrapped"), Lifted->HasField(TEXT("data")));
	TestEqual(TEXT("its fields sit at the root"), static_cast<int32>(Lifted->GetNumberField(TEXT("count"))), 3);
	Spec->RemoveField(TEXT("field"));
	TestEqual(TEXT("the payload's list is counted, not the envelope's empty warnings"),
		McpExecuteAggregate::Apply(Spec, nullptr, Lifted)->GetStringField(TEXT("field")), FString(TEXT("actors")));
	TestTrue(TEXT("a reply with no payload is the same object"), McpLiftReplyPayload(Result) == Result);

	// An actor row names its object (StaticMeshActor_3) and its label (Crate_A): the sample is the label.
	TSharedPtr<FJsonObject> Labelled = MakeShared<FJsonObject>();
	Labelled->SetStringField(TEXT("name"), TEXT("StaticMeshActor_3"));
	Labelled->SetStringField(TEXT("label"), TEXT("Crate_A"));
	TSharedPtr<FJsonObject> Listed = MakeShared<FJsonObject>();
	Listed->SetArrayField(TEXT("actors"), TArray<TSharedPtr<FJsonValue>>({MakeShared<FJsonValueObject>(Labelled)}));
	const TArray<TSharedPtr<FJsonValue>> Sampled =
		McpExecuteAggregate::Apply(MakeShared<FJsonObject>(), nullptr, Listed)->GetArrayField(TEXT("sample"));
	TestTrue(TEXT("an actor is sampled by its label"), Sampled.Num() == 1 && Sampled[0]->AsString() == TEXT("Crate_A"));

	// Counting by label counts copies together: the number the editor gives each one is dropped, nothing else is.
	TArray<TSharedPtr<FJsonValue>> Copies;
	for (const TCHAR* Label : {TEXT("Wall_12"), TEXT("Wall_3"), TEXT("Wall"), TEXT("Stage01"), TEXT("Card_C03"), TEXT("_7")})
	{
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("label"), Label);
		Copies.Add(MakeShared<FJsonValueObject>(Row));
	}
	// A find row names its actor by name alone: grouped by label, it counts by that name.
	TSharedPtr<FJsonObject> FindRow = MakeShared<FJsonObject>();
	FindRow->SetStringField(TEXT("name"), TEXT("Wall_40"));
	Copies.Add(MakeShared<FJsonValueObject>(FindRow));
	TSharedPtr<FJsonObject> Level = MakeShared<FJsonObject>();
	Level->SetArrayField(TEXT("actors"), Copies);
	TSharedPtr<FJsonObject> ByLabel = MakeShared<FJsonObject>();
	ByLabel->SetStringField(TEXT("groupBy"), TEXT("label"));
	TMap<FString, int32> Counted;
	// Held: a range-for over a temporary's member walked the block after the temporary freed it.
	const TSharedPtr<FJsonObject> ByLabelBlock = McpExecuteAggregate::Apply(ByLabel, nullptr, Level);
	for (const TSharedPtr<FJsonValue>& Group : ByLabelBlock->GetArrayField(TEXT("groups")))
	{
		Counted.Add(Group->AsObject()->GetStringField(TEXT("value")), static_cast<int32>(Group->AsObject()->GetNumberField(TEXT("count"))));
	}
	TestEqual(TEXT("Wall_12, Wall_3, Wall and a row named Wall_40 count as Wall"), Counted.FindRef(TEXT("Wall")), 4);
	TestTrue(TEXT("a number that is part of the name stays"), Counted.Contains(TEXT("Stage01")) && Counted.Contains(TEXT("Card_C03")));
	TestTrue(TEXT("a label that is all number stays whole"), Counted.Contains(TEXT("_7")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
