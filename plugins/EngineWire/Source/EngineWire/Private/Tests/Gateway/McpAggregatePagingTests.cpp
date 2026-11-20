// McpAggregatePagingTests.cpp — options.aggregate's groupOffset (Execute/Request/McpExecuteAggregate.cpp): a list of 60
// classes reads 50 groups and the offset of the rest, and that offset reads the other 10.

#include "MCP/Execute/Request/McpExecuteAggregate.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAggregatePagingTest, "EngineWire.Gateway.AggregatePaging",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpAggregatePagingTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const auto List = []()
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (int32 Index = 0; Index < 60; ++Index)
        {
            TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("name"), FString::Printf(TEXT("Mesh%02d"), Index));
            Row->SetStringField(TEXT("class"), FString::Printf(TEXT("Class%02d"), Index));
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetArrayField(TEXT("assets"), Rows);
        return Result;
    };
    TSharedPtr<FJsonObject> Spec = MakeShared<FJsonObject>();
    Spec->SetStringField(TEXT("groupBy"), TEXT("class"));
    TSharedPtr<FJsonObject> First = List();
    const TSharedPtr<FJsonObject> Page1 = McpExecuteAggregate::Apply(Spec, nullptr, First);
    TestEqual(TEXT("fifty groups"), Page1->GetArrayField(TEXT("groups")).Num(), 50);
    TestEqual(TEXT("ten more"), static_cast<int32>(Page1->GetNumberField(TEXT("otherGroups"))), 10);
    TestEqual(TEXT("the offset of the rest"), static_cast<int32>(Page1->GetNumberField(TEXT("nextGroupOffset"))), 50);

    Spec->SetNumberField(TEXT("groupOffset"), 50);
    TSharedPtr<FJsonObject> Second = List();
    const TSharedPtr<FJsonObject> Page2 = McpExecuteAggregate::Apply(Spec, nullptr, Second);
    TestEqual(TEXT("the other ten"), Page2->GetArrayField(TEXT("groups")).Num(), 10);
    TestFalse(TEXT("none after them"), Page2->HasField(TEXT("nextGroupOffset")));

    TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
    Options->SetObjectField(TEXT("aggregate"), Spec);
    FMcpSemanticError Error;
    TestTrue(TEXT("groupOffset 50 is valid"), McpExecuteAggregate::Validate(Options, Error));
    Spec->SetNumberField(TEXT("groupOffset"), -1);
    TestFalse(TEXT("a negative one is not"), McpExecuteAggregate::Validate(Options, Error));
    return true;
}
#endif
