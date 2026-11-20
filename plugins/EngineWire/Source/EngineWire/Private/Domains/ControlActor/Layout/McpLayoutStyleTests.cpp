#include "Domains/ControlActor/Layout/McpLayoutStyle.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpLayoutStyleTest, "EngineWire.ControlActor.LayoutStyle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Two tables, each with four chairs beside it facing it and a lamp on one: chairs come first (four per table), at the
// distance and turn seen most; a mug seen once among ten tables is left out; the counts survive a JSON round trip.
bool FMcpLayoutStyleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpLayoutStyle;
    FLevelStats Level;
    Level.Copies.Add(TEXT("Table"), 2);
    for (int32 Table = 0; Table < 2; ++Table)
    {
        for (int32 Side = 0; Side < 4; ++Side)
        {
            Add(Level, {TEXT("Table"), TEXT("Chair"), TEXT("nextTo"), 90.0, Side * 90.0, 180.0 + Side * 90.0});
        }
    }
    Add(Level, {TEXT("Table"), TEXT("Lamp"), TEXT("on"), 10.0, 0.0, 0.0});
    FLevelStats Hall;
    Hall.Copies.Add(TEXT("Table"), 10);
    Add(Hall, {TEXT("Table"), TEXT("Mug"), TEXT("on"), 10.0, 0.0, 0.0});
    const TArray<FSuggestion> Found = Suggest({Level, FromJson(ToJson(Level)), Hall}, TEXT("Table"), 5);
    TestEqual(TEXT("chair and lamp, not the mug"), Found.Num(), 2);
    TestTrue(TEXT("chairs first, four per table"), Found.Num() > 0 && Found[0].Mesh == TEXT("Chair") && Found[0].Kind == TEXT("nextTo") &&
             FMath::IsNearlyEqual(Found[0].PerAnchor, 16.0 / 14.0) && FMath::IsNearlyEqual(Found[0].Distance, 100.0));
    TestEqual(TEXT("a turn of -45 lands in the 315 bin"), AngleBin(-45.0), 7);
    TestEqual(TEXT("past 400 cm is the last distance bin"), DistanceBin(1000.0), DistanceBins - 1);
    return true;
}
#endif
