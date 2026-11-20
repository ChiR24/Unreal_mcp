#include "Domains/AI/Reach/McpReach.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpReachMathTest, "EngineWire.AI.ReachMath",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The jump a default Character makes (420 cm/s up, 980 cm/s^2, 600 cm/s): 90 cm high; on level ground it clears
// 600 * 2 * 420 / 980 cm; higher than its apex it reaches nothing; a drop carries it farther; its arc ends on the target.
bool FMcpReachMathTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    McpReach::FMover Mover;
    Mover.JumpZ = 420.0;
    Mover.Gravity = 980.0;
    Mover.Speed = 600.0;
    TestEqual(TEXT("apex"), McpReach::Apex(Mover), 90.0, 0.01);
    TestEqual(TEXT("reach on level ground"), McpReach::Reach(Mover, 0.0), 600.0 * 840.0 / 980.0, 0.01);
    TestTrue(TEXT("nothing above the apex"), McpReach::Reach(Mover, 90.5) < 0.0);
    TestTrue(TEXT("just under the apex still lands"), McpReach::Reach(Mover, 89.0) > 0.0);
    TestTrue(TEXT("a drop carries farther than level ground"), McpReach::Reach(Mover, -300.0) > McpReach::Reach(Mover, 0.0));
    const FVector From(0.0, 0.0, 0.0);
    const FVector To(300.0, 0.0, 50.0);
    const TArray<FVector> Points = McpReach::Arc(Mover, From, To, 8);
    TestEqual(TEXT("eight points"), Points.Num(), 8);
    TestTrue(TEXT("the last is the landing"), Points.Last().Equals(To, 0.01));
    double Highest = 0.0;
    for (const FVector& Point : Points) Highest = FMath::Max(Highest, Point.Z);
    TestTrue(TEXT("it rises above the landing on the way"), Highest > To.Z && Highest <= 90.0 + 0.01);
    return true;
}
#endif
