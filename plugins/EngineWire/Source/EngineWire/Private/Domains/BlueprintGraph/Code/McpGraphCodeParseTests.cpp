#include "Domains/BlueprintGraph/Code/McpGraphCodeEmit.h"
#include "Domains/BlueprintGraph/Code/McpGraphCodeParse.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpGraphCodeParseTest, "EngineWire.BlueprintGraph.Code.Parse",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers from Epic's tokenizer, parser and literal tests (test_blueprint_dsl.py), and the one deliberate
// difference: a backslash in a string value is escaped, so the tokenizer reads the value back unchanged.
bool FMcpGraphCodeParseTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpGraphCode;
    using namespace McpGraphCode::Emit;
    const auto Parse = [](const FString& Text, FString& Error)
    {
        TArray<FSExpr> Forms;
        return ParseSExpr(Text, Forms, Error) ? Forms : TArray<FSExpr>();
    };
    // The text of item At in the first form, empty when the text does not parse that far.
    const auto ItemText = [&Parse](const FString& Text, int32 At)
    {
        FString Unused;
        const TArray<FSExpr> Forms = Parse(Text, Unused);
        return Forms.Num() > 0 && Forms[0].Items.IsValidIndex(At) ? Forms[0].Items[At].Text : FString();
    };
    FString Error;
    const TArray<FSExpr> Call = Parse(TEXT("(Math|Float|sin(degrees) angle :\"Array Element\" -3 3.14 true \"say \\\"hi\\\"\") ; gone"), Error);
    TestTrue(TEXT("one form"), Call.Num() == 1 && Call[0].Type == FSExpr::EType::List && Call[0].Items.Num() == 7);
    if (Call.Num() == 1 && Call[0].Items.Num() == 7)
    {
        const TArray<FSExpr>& Items = Call[0].Items;
        TestEqual(TEXT("a symbol keeps the balanced parens it runs into"), Items[0].Text, FString(TEXT("Math|Float|sin(degrees)")));
        TestTrue(TEXT("a plain symbol"), Items[1].Type == FSExpr::EType::Symbol && Items[1].Text == TEXT("angle"));
        TestEqual(TEXT(":\"Pin Name\" is one keyword"), Items[2].Text, FString(TEXT(":Array Element")));
        TestTrue(TEXT("negative integer"), Items[3].Type == FSExpr::EType::Int && Items[3].Text == TEXT("-3"));
        TestTrue(TEXT("float"), Items[4].Type == FSExpr::EType::Float);
        TestTrue(TEXT("bool"), Items[5].Type == FSExpr::EType::Bool && Items[5].bTrue);
        TestTrue(TEXT("escaped quotes"), Items[6].Type == FSExpr::EType::String && Items[6].Text == TEXT("say \"hi\""));
    }
    TestEqual(TEXT("Equal(==) stays one symbol"), ItemText(TEXT("(Utilities|Operators|Equal(==) a b)"), 0), FString(TEXT("Utilities|Operators|Equal(==)")));
    TestTrue(TEXT("two top-level forms, comment lines skipped"), Parse(TEXT("; only a comment\n(event A)\n(event B)"), Error).Num() == 2);
    TestTrue(TEXT("an unclosed paren is refused"), Parse(TEXT("(event A"), Error).Num() == 0 && Error == TEXT("Unclosed parenthesis"));
    TestTrue(TEXT("a stray close paren is refused"), Parse(TEXT(")"), Error).Num() == 0 && Error == TEXT("Unexpected )"));
    TestTrue(TEXT("an unterminated string is refused with its line"), Parse(TEXT("(a\n\"open)"), Error).Num() == 0 && Error.StartsWith(TEXT("Line 2: unterminated string")));

    FPin Pin;
    Pin.bBool = true;
    Pin.Value = TEXT("True");
    TestEqual(TEXT("bool"), FormatDefault(Pin), FString(TEXT("true")));
    Pin = FPin();
    Pin.bText = true;
    Pin.Value = TEXT("MyName");
    TestEqual(TEXT("names and strings are quoted"), FormatDefault(Pin), FString(TEXT("\"MyName\"")));
    Pin.Value = TEXT("C:\\Temp\\");
    TestEqual(TEXT("a backslash is escaped"), FormatDefault(Pin), FString(TEXT("\"C:\\\\Temp\\\\\"")));
    TestEqual(TEXT("and reads back as it was"), ItemText(TEXT("(f ") + FormatDefault(Pin) + TEXT(")"), 1), FString(TEXT("C:\\Temp\\")));
    Pin = FPin();
    TestEqual(TEXT("an empty number is 0"), FormatDefault(Pin), FString(TEXT("0")));
    Pin.Value = TEXT("3.14");
    TestEqual(TEXT("numbers pass through"), FormatDefault(Pin), FString(TEXT("3.14")));
    Pin.Value = TEXT("AlwaysSpawn");
    TestEqual(TEXT("an enum value is quoted"), FormatDefault(Pin), FString(TEXT("\"AlwaysSpawn\"")));
    Pin.Value = TEXT("/Script/Engine.StaticMeshActor");
    TestEqual(TEXT("a class path is quoted"), FormatDefault(Pin), FString(TEXT("\"/Script/Engine.StaticMeshActor\"")));
    Pin.Value = TEXT("0, 0, 100");
    TestEqual(TEXT("a vector is quoted"), FormatDefault(Pin), FString(TEXT("\"0, 0, 100\"")));
    TestEqual(TEXT("a whole number range stop"), IncrementExpr(TEXT("4")), FString(TEXT("5")));
    TestEqual(TEXT("any other range stop"), IncrementExpr(TEXT("n")), FString(TEXT("(+ n 1)")));
    TestEqual(TEXT("(not x) turns back to x"), InvertExpr(TEXT("(not x)")), FString(TEXT("x")));
    TestEqual(TEXT("pin names become variable names"), SanitizePinName(TEXT("As Static Mesh Actor")), FString(TEXT("as_static_mesh_actor")));
    TestEqual(TEXT("a pin name with a space is quoted"), FormatPinName(TEXT("Is Valid")), FString(TEXT("\"Is Valid\"")));

    // Node ids: n and the GUID's first hex digits, across every graph read, 6 at least.
    TArray<FGraph> Graphs;
    Graphs.AddDefaulted(2);
    const TCHAR* const Guids[] = {TEXT("ABCDEF0123456789ABCDEF0123456789"), TEXT("ABCDEF0199999999ABCDEF0123456789"), TEXT("1234567890ABCDEF1234567890ABCDEF")};
    for (int32 At = 0; At < 3; ++At)
    {
        Graphs[At == 2 ? 1 : 0].Nodes.AddDefaulted_GetRef().Guid = Guids[At];
    }
    ShortenIds(Graphs);
    TestEqual(TEXT("ids sharing 8 digits take 9"), Graphs[0].Nodes[0].Id, FString(TEXT("nabcdef012")));
    TestEqual(TEXT("and differ there"), Graphs[0].Nodes[1].Id, FString(TEXT("nabcdef019")));
    TestEqual(TEXT("an id in another graph shares nothing and keeps 6"), Graphs[1].Nodes[0].Id, FString(TEXT("n123456")));
    return true;
}
#endif
