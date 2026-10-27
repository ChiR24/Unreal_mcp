// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Perception/McpPerception.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPerceptionAuditTest, "EngineWire.Perception.AuditMaterials",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
// Forty cells of one look: a base colour (sRGB), a metallic and a roughness.
TArray<FString> AuditFindingsOf(FColor Base, float Metal, float Rough)
{
    TArray<FColor> Bases;
    TArray<float> Metals, Roughs;
    Bases.Init(Base, 40);
    Metals.Init(Metal, 40);
    Roughs.Init(Rough, 40);
    return McpPerception::PbrFindings(Bases, Metals, Roughs);
}
} // namespace

// Known answers: a mid grey rough dielectric is fine; black and white base colours, a dark metal, half metal and a mirror
// are each named; a quarter of the cells is enough, an eighth is not; texel density four times off either way is flagged.
bool FMcpPerceptionAuditTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TestEqual(TEXT("mid grey, rough, not metal"), AuditFindingsOf(FColor(128, 128, 128), 0.f, 0.6f).Num(), 0);
    TestTrue(TEXT("black"), AuditFindingsOf(FColor(10, 10, 10), 0.f, 0.6f).Contains(TEXT("too_dark")));
    TestTrue(TEXT("snow white"), AuditFindingsOf(FColor(250, 250, 250), 0.f, 0.6f).Contains(TEXT("too_bright")));
    TestTrue(TEXT("a dark metal"), AuditFindingsOf(FColor(60, 60, 60), 1.f, 0.4f).Contains(TEXT("dark_metal")));
    TestFalse(TEXT("a dark metal is not a dark dielectric"), AuditFindingsOf(FColor(60, 60, 60), 1.f, 0.4f).Contains(TEXT("too_dark")));
    TestEqual(TEXT("a bright metal is fine, silver included"), AuditFindingsOf(FColor(250, 250, 250), 1.f, 0.3f).Num(), 0);
    TestTrue(TEXT("half metal"), AuditFindingsOf(FColor(128, 128, 128), 0.5f, 0.6f).Contains(TEXT("partial_metallic")));
    TestTrue(TEXT("a mirror"), AuditFindingsOf(FColor(128, 128, 128), 0.f, 0.f).Contains(TEXT("mirror_roughness")));

    TArray<FColor> Bases;
    TArray<float> Metals, Roughs;
    Bases.Init(FColor(128, 128, 128), 40);
    Metals.Init(0.f, 40);
    Roughs.Init(0.6f, 40);
    for (int32 Cell = 0; Cell < 5; ++Cell)
    {
        Bases[Cell] = FColor(5, 5, 5); // a dark eighth: a shadowed seam, not the material
    }
    TestEqual(TEXT("an eighth is not enough"), McpPerception::PbrFindings(Bases, Metals, Roughs).Num(), 0);
    for (int32 Cell = 5; Cell < 10; ++Cell)
    {
        Bases[Cell] = FColor(5, 5, 5);
    }
    TestTrue(TEXT("a quarter is"), McpPerception::PbrFindings(Bases, Metals, Roughs).Contains(TEXT("too_dark")));

    TestEqual(TEXT("ten times sharper"), McpPerception::TexelVerdict(10.0), 1);
    TestEqual(TEXT("four times blurrier"), McpPerception::TexelVerdict(0.25), -1);
    TestEqual(TEXT("twice is within"), McpPerception::TexelVerdict(2.0), 0);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
