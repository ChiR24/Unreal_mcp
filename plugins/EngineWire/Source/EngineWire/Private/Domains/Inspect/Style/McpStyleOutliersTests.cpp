#include "Domains/Inspect/Style/McpStyleOutliers.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpStyleOutliersTest, "EngineWire.Inspect.StyleOutliers",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Ten crates at 1 and one at 3 flag the one; a group of three judges nothing; copies a little apart flag nothing.
bool FMcpStyleOutliersTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpStyleOutliers;
    TArray<FSample> Samples;
    for (int32 Index = 0; Index < 10; ++Index) Samples.Add({TEXT("Crate"), FString::Printf(TEXT("Crate_%d"), Index), 1.0});
    Samples.Add({TEXT("Crate"), TEXT("Crate_Big"), 3.0});
    for (int32 Index = 0; Index < 3; ++Index) Samples.Add({TEXT("Lamp"), FString::Printf(TEXT("Lamp_%d"), Index), Index == 0 ? 5.0 : 1.0});
    for (int32 Index = 0; Index < 8; ++Index) Samples.Add({TEXT("Rock"), FString::Printf(TEXT("Rock_%d"), Index), 0.9 + 0.05 * Index});
    const TArray<FOutlier> Found = Find(Samples);
    TestEqual(TEXT("one outlier"), Found.Num(), 1);
    TestTrue(TEXT("the big crate"), Found.Num() == 1 && Found[0].Name == TEXT("Crate_Big") && FMath::IsNearlyEqual(Found[0].Median, 1.0));
    TestEqual(TEXT("median of an even count"), Median({1.0, 2.0, 3.0, 4.0}), 2.5);
    return true;
}
#endif
