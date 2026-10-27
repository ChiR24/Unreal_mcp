#include "Foundation/Solvers/McpPratt.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPrattTest, "EngineWire.Foundation.Solvers.Pratt",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers: each assertion as the parser groups it, written back fully parenthesized, and the refusals with what
// was expected where.
bool FMcpPrattTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    struct FCase
    {
        const TCHAR* Source;
        const TCHAR* Printed;
    };
    static const FCase Parsed[] = {
        {TEXT("Lamp on Table"), TEXT("(on Lamp Table)")},
        {TEXT("lamp ON table"), TEXT("(on lamp table)")},
        {TEXT("Shelf nextto Wall"), TEXT("(nextTo Shelf Wall)")},
        {TEXT("Shelf against Wall"), TEXT("(against Shelf Wall)")},
        {TEXT("\"Point Light 2\" inside Room"), TEXT("(inside \"Point Light 2\" Room)")},
        {TEXT("Lamp near Table within 50"), TEXT("(near Lamp Table 50)")},
        {TEXT("Chair facing Table"), TEXT("(facing Chair Table)")},
        {TEXT("Cube.location.z in 0..10"), TEXT("(in Cube.location.z 0 10)")},
        {TEXT("Cube.location.z == -5.5"), TEXT("(== Cube.location.z -5.5)")},
        {TEXT("Lamp.LightComponent.Intensity>1000"), TEXT("(> Lamp.LightComponent.Intensity 1000)")},
        {TEXT("/Game/BP_Door compiles"), TEXT("(compiles /Game/BP_Door)")},
        {TEXT("not Lamp hidden and Door exists"), TEXT("(and (not (hidden Lamp)) (exists Door))")},
        {TEXT("A on B or C on D and E exists"), TEXT("(or (on A B) (and (on C D) (exists E)))")},
        {TEXT("A on B and C on D and E on F"), TEXT("(and (and (on A B) (on C D)) (on E F))")},
        {TEXT("(A on B or A on C) and noWarnings"), TEXT("(and (or (on A B) (on A C)) (noWarnings))")},
    };
    for (const FCase& Case : Parsed)
    {
        TSharedPtr<McpPratt::FNode> Root;
        FString Error;
        const bool bParsed = McpPratt::Parse(Case.Source, Root, Error);
        TestTrue(FString::Printf(TEXT("'%s' parses (%s)"), Case.Source, *Error), bParsed);
        // Case-sensitive: TestEqual on strings ignores case in 5.0, and keywords print in their canonical spelling.
        const FString Printed = McpPratt::Print(Root);
        TestTrue(FString::Printf(TEXT("'%s' groups as %s, read %s"), Case.Source, Case.Printed, *Printed),
                 Printed.Equals(Case.Printed, ESearchCase::CaseSensitive));
    }
    static const FCase Refused[] = {
        {TEXT("Lamp"), TEXT("expected a relation")},
        {TEXT("Lamp on"), TEXT("expected a name at character 8")},
        {TEXT("Cube.location.z in 0..x"), TEXT("expected a number")},
        {TEXT("Lamp near Table within far"), TEXT("expected a number")},
        {TEXT("\"Point Light"), TEXT("never closed")},
        {TEXT("Lamp on Table extra"), TEXT("found 'extra'")},
        {TEXT("(Lamp on Table"), TEXT("expected ')'")},
        {TEXT("Lamp # Table"), TEXT("'#' at character 6")},
    };
    for (const FCase& Case : Refused)
    {
        TSharedPtr<McpPratt::FNode> Root;
        FString Error;
        const bool bParsed = McpPratt::Parse(Case.Source, Root, Error);
        TestFalse(FString::Printf(TEXT("'%s' is refused"), Case.Source), bParsed);
        TestTrue(FString::Printf(TEXT("'%s' says why: %s"), Case.Source, *Error), Error.Contains(Case.Printed));
    }
    return true;
}
#endif
