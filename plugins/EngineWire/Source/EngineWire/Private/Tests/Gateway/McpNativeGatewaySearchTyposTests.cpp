// McpNativeGatewaySearchTyposTests.cpp — known answers for search's typo correction (Search/McpSearchTypos.cpp) and for
// options.aggregate (Execute/Request/McpExecuteAggregate.cpp).

#include "MCP/Gateway/Search/McpSearchTypos.h"
#include "MCP/Execute/Request/McpExecuteAggregate.h"
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
		McpExecuteAggregate::Apply(Spec, nullptr, Page)->GetStringField(TEXT("note")).Contains(TEXT("this page only")));
	Spec->SetStringField(TEXT("field"), TEXT("missing"));
	TestTrue(TEXT("a list the reply lacks is said"), McpExecuteAggregate::Apply(Spec, nullptr, Result)->HasField(TEXT("note")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
