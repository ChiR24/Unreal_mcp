// McpExecuteBudgetTests.cpp — options.maxReplyChars on fixed JSON (Request/McpExecuteBudget.h): its bounds, the
// details-first rung and the string-cutting rung. The paging rung is the gateway's own pager (ResultPages tests).

#include "MCP/Execute/Request/McpExecuteBudget.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonValue.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Execute/McpNativeReceiptRedaction.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpExecuteBudgetTest,
    "EngineWire.Gateway.ReplyBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpExecuteBudgetTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    auto Options = [](const TSharedPtr<FJsonValue>& Value)
    {
        const TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetField(TEXT("maxReplyChars"), Value);
        return Out;
    };
    FMcpSemanticError Error;
    TestTrue(TEXT("no budget is valid"), McpExecuteBudget::Validate(MakeShared<FJsonObject>(), Error));
    TestTrue(TEXT("1000 is valid"), McpExecuteBudget::Validate(Options(MakeShared<FJsonValueNumber>(1000)), Error));
    TestFalse(TEXT("999 is not"), McpExecuteBudget::Validate(Options(MakeShared<FJsonValueNumber>(999)), Error));
    TestFalse(TEXT("a fraction is not"), McpExecuteBudget::Validate(Options(MakeShared<FJsonValueNumber>(4000.5)), Error));
    TestFalse(TEXT("text is not"), McpExecuteBudget::Validate(Options(MakeShared<FJsonValueString>(TEXT("4000"))), Error));
    TestEqual(TEXT("Of reads it"), McpExecuteBudget::Of(Options(MakeShared<FJsonValueNumber>(4000))), 4000);

    // A 50k reply: a long code string, bulky details and a short list.
    const TSharedPtr<FJsonObject> Details = MakeShared<FJsonObject>();
    Details->SetStringField(TEXT("log"), FString::ChrN(20000, TEXT('d')));
    const TSharedPtr<FJsonObject> Reply = MakeShared<FJsonObject>();
    Reply->SetBoolField(TEXT("success"), true);
    Reply->SetStringField(TEXT("code"), FString::ChrN(30000, TEXT('c')));
    Reply->SetObjectField(TEXT("details"), Details);
    TArray<FString> Cuts;
    TestTrue(TEXT("under budget nothing goes"), McpExecuteBudget::DropExtras(Reply, nullptr, 100000, Cuts) == Reply && Cuts.Num() == 0);

    const TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
    Schema->SetArrayField(TEXT("required"), {MakeShared<FJsonValueString>(TEXT("details"))});
    McpExecuteBudget::DropExtras(Reply, Schema, 4000, Cuts);
    TestEqual(TEXT("a required details stays"), Cuts.Num(), 0);

    TSharedPtr<FJsonObject> Trimmed = McpExecuteBudget::DropExtras(Reply, nullptr, 4000, Cuts);
    TestFalse(TEXT("details dropped"), Trimmed->HasField(TEXT("details")));
    TestTrue(TEXT("on a copy"), Reply->HasField(TEXT("details")));
    TestTrue(TEXT("and named"), Cuts.Num() == 1 && Cuts[0] == TEXT("dropped details"));

    TestTrue(TEXT("long strings cut until it fits"), McpExecuteBudget::CutStrings(Trimmed, 4000, Cuts));
    TestFalse(TEXT("so it is under the budget"), McpSerializedResultExceeds(Trimmed, 4000));
    TestTrue(TEXT("the cut string says how much went"), Trimmed->GetStringField(TEXT("code")).EndsWith(TEXT("[29000 more characters cut]")));
    TestTrue(TEXT("the cut is named with its field"), Cuts.Num() == 2 && Cuts[1].Contains(TEXT("code (30000 characters)")));
    TestEqual(TEXT("the original is untouched"), Reply->GetStringField(TEXT("code")).Len(), 30000);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
