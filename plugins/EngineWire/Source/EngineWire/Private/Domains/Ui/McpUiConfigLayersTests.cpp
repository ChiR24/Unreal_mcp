// McpUiConfigLayersTests.cpp — the INI line actions a config layer applies (McpUiConfigLayers.h), and which keys are
// withheld as secrets.

#include "Domains/Ui/McpUiConfigLayers.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpConfigLayersTest,
    "EngineWire.Ui.ConfigLayers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpConfigLayersTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FString Section = TEXT("/Script/Engine.RendererSettings");
    TArray<FString> Values;
    TestTrue(TEXT("a plain line sets the key"), McpConfigLayers::Apply(TEXT("[/Script/Engine.RendererSettings]\nr.Foo=1\n"), Section, TEXT("r.Foo"), Values));
    TestEqual(TEXT("to its value"), Values, TArray<FString>({TEXT("1")}));
    TestFalse(TEXT("another section does not"), McpConfigLayers::Apply(TEXT("[Other]\nr.Foo=2\n"), Section, TEXT("r.Foo"), Values));
    McpConfigLayers::Apply(TEXT("[/Script/Engine.RendererSettings]\n+r.Foo=2\n+r.Foo=2\n.r.Foo=2\n"), Section, TEXT("r.Foo"), Values);
    TestEqual(TEXT("+ adds once, . adds again"), Values, TArray<FString>({TEXT("1"), TEXT("2"), TEXT("2")}));
    McpConfigLayers::Apply(TEXT("[/Script/Engine.RendererSettings]\n-r.Foo=1\n"), Section, TEXT("r.Foo"), Values);
    TestEqual(TEXT("- removes"), Values, TArray<FString>({TEXT("2"), TEXT("2")}));
    McpConfigLayers::Apply(TEXT("; comment\n[/Script/Engine.RendererSettings]\n!r.Foo=ClearArray\n"), Section, TEXT("r.Foo"), Values);
    TestEqual(TEXT("! clears"), Values.Num(), 0);
    McpConfigLayers::Apply(TEXT("[/Script/Engine.RendererSettings]\nr.Foo=\"quoted\"\n"), Section, TEXT("r.Foo"), Values);
    TestEqual(TEXT("quotes come off"), Values, TArray<FString>({TEXT("quoted")}));

    TestTrue(TEXT("a token is a secret"), McpConfigLayers::IsSecretKey(TEXT("AuthToken")));
    TestTrue(TEXT("an API key is a secret"), McpConfigLayers::IsSecretKey(TEXT("ServiceApiKey")));
    TestFalse(TEXT("a console key is not"), McpConfigLayers::IsSecretKey(TEXT("ConsoleKeys")));
    TestFalse(TEXT("an author is not"), McpConfigLayers::IsSecretKey(TEXT("Author")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
