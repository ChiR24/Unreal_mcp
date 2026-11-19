// McpExecuteSelectTests.cpp — options.select / omit keep or drop fields of a reply's data by dotted path, across lists,
// leave the data they were given alone, keep the undo block, warn on a path that matched nothing, and refuse an index.

#include "MCP/Execute/Request/McpExecuteSelect.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpExecuteSelectTest,
	"EngineWire.Gateway.SelectOmit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace McpSelectTestData
{
TSharedPtr<FJsonObject> Actor(const TCHAR* Label, const TCHAR* Class)
{
	TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
	Item->SetStringField(TEXT("label"), Label);
	Item->SetStringField(TEXT("class"), Class);
	Item->SetNumberField(TEXT("z"), 1.0);
	return Item;
}

TSharedPtr<FJsonObject> Options(const TCHAR* Key, const TArray<FString>& Paths)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FString& Path : Paths)
	{
		Values.Add(MakeShared<FJsonValueString>(Path));
	}
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetArrayField(Key, Values);
	return Out;
}

TSharedPtr<FJsonObject> Item(const TSharedPtr<FJsonObject>& Data, int32 Index)
{
	return Data->GetArrayField(TEXT("actors"))[Index]->AsObject();
}
} // namespace McpSelectTestData

bool FMcpExecuteSelectTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpSelectTestData;
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Actors;
	Actors.Add(MakeShared<FJsonValueObject>(Actor(TEXT("Wall_1"), TEXT("StaticMeshActor"))));
	Actors.Add(MakeShared<FJsonValueObject>(Actor(TEXT("Lamp"), TEXT("PointLight"))));
	Data->SetArrayField(TEXT("actors"), Actors);
	Data->SetNumberField(TEXT("count"), 2.0);
	TSharedPtr<FJsonObject> Undo = MakeShared<FJsonObject>();
	Undo->SetBoolField(TEXT("undoable"), true);
	Data->SetObjectField(TEXT("undo"), Undo);

	TArray<FString> Warnings;
	const TSharedPtr<FJsonObject> Picked = McpExecuteSelect::Apply(
		Options(TEXT("select"), {TEXT("actors.label"), TEXT("actors[].class")}), Data, Warnings);
	TestEqual(TEXT("two paths into one list merge per item"), Item(Picked, 1)->Values.Num(), 2);
	TestEqual(TEXT("the label is kept"), Item(Picked, 0)->GetStringField(TEXT("label")), FString(TEXT("Wall_1")));
	TestFalse(TEXT("a field not selected is gone"), Picked->HasField(TEXT("count")));
	TestTrue(TEXT("the undo block stays"), Picked->HasField(TEXT("undo")));
	TestEqual(TEXT("every path matched"), Warnings.Num(), 0);

	const TSharedPtr<FJsonObject> Pruned = McpExecuteSelect::Apply(
		Options(TEXT("omit"), {TEXT("actors.z"), TEXT("count"), TEXT("nothing.here")}), Data, Warnings);
	TestFalse(TEXT("an omitted field inside a list is gone"), Item(Pruned, 0)->HasField(TEXT("z")));
	TestTrue(TEXT("the other fields stay"), Item(Pruned, 0)->HasField(TEXT("class")));
	TestFalse(TEXT("an omitted top field is gone"), Pruned->HasField(TEXT("count")));
	TestTrue(TEXT("the data given is left alone"), Item(Data, 0)->HasField(TEXT("z")) && Data->HasField(TEXT("count")));
	TestEqual(TEXT("a path that matched nothing is one warning"), Warnings.Num(), 1);

	FMcpSemanticError Error;
	TestTrue(TEXT("plain paths are valid"), McpExecuteSelect::Validate(Options(TEXT("select"), {TEXT("actors[].label")}), Error));
	TestFalse(TEXT("an element index is refused"), McpExecuteSelect::Validate(Options(TEXT("omit"), {TEXT("actors[2].label")}), Error));
	TestEqual(TEXT("as INVALID_OPTIONS"), Error.GatewayCode, FString(TEXT("INVALID_OPTIONS")));
	TestFalse(TEXT("another mask is another fingerprint"),
		McpExecuteSelect::FingerprintPart(Options(TEXT("select"), {TEXT("a")})) ==
		McpExecuteSelect::FingerprintPart(Options(TEXT("omit"), {TEXT("a")})));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
