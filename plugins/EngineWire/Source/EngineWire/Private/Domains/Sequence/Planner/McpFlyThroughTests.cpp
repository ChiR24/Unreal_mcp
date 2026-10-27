// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Sequence/Planner/McpFlyThrough.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpFlyThroughTest, "EngineWire.Sequence.FlyThrough",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
// A wall 40 cm thick, 6 m long and 4 m high across the straight line between the two points, standing on a floor at z 0.
const FBox FlyTestWall(FVector(-20.0, -300.0, 0.0), FVector(20.0, 300.0, 400.0));

double FlyTestGap(const FVector& Point)
{
    return FMath::Min(Point.Z, FVector::Max(FVector::Max(FlyTestWall.Min - Point, Point - FlyTestWall.Max), FVector::ZeroVector).Size());
}

McpFlyThrough::FSpace FlyTestSpace()
{
    McpFlyThrough::FSpace Space;
    Space.Radius = 50.0;
    Space.Free = [](const FVector& Point, double Radius) { return FlyTestGap(Point) >= Radius; };
    Space.Clear = [](const FVector& From, const FVector& To, double Radius)
    {
        const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist(From, To) / 2.0));
        for (int32 Step = 0; Step <= Steps; ++Step)
        {
            if (FlyTestGap(FMath::Lerp(From, To, static_cast<double>(Step) / Steps)) < Radius)
            {
                return false;
            }
        }
        return true;
    };
    return Space;
}
} // namespace

// Known answers with no world: a flight between two points on either side of a wall goes over it (shorter than round
// its end), its corner lines keep the whole clearance and its curve at least 0.8 of it, it starts and ends on the
// points, and a point inside the wall is refused by index.
bool FMcpFlyThroughTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const McpFlyThrough::FSpace Space = FlyTestSpace();
    McpFlyThrough::FSearchBudget Budget; // no deadline: the known answer must not depend on the machine
    McpFlyThrough::FPlan Plan = McpFlyThrough::PlanCorners(Space, {FVector(-500.0, 0.0, 150.0), FVector(500.0, 0.0, 150.0)}, Budget);
    TestTrue(TEXT("planned"), Plan.Error.IsEmpty());
    TestTrue(TEXT("over the wall, not through it"), Plan.Points.Num() > 2);
    double Lines = TNumericLimits<double>::Max();
    for (int32 Corner = 0; Corner + 1 < Plan.Points.Num(); ++Corner)
    {
        for (int32 Step = 0; Step <= 200; ++Step)
        {
            Lines = FMath::Min(Lines, FlyTestGap(FMath::Lerp(Plan.Points[Corner], Plan.Points[Corner + 1], Step / 200.0)));
        }
    }
    TestTrue(TEXT("the corner lines keep the whole clearance"), Lines >= Space.Radius - 1.0);
    TArray<McpMinSnap::FLeg> Legs;
    FString Error;
    TestTrue(TEXT("smoothed"), McpFlyThrough::Fly(Space, Plan, 8.0, Budget, Legs, Error));
    double Closest = TNumericLimits<double>::Max();
    for (int32 Step = 0; Step <= 1600; ++Step)
    {
        Closest = FMath::Min(Closest, FlyTestGap(McpFlyThrough::Position(Legs, 8.0 * Step / 1600)));
    }
    TestTrue(TEXT("never nearer than 0.8 of the clearance"), Closest >= 0.8 * Space.Radius - 1.0);
    TestTrue(TEXT("starts on the first point"), McpFlyThrough::Position(Legs, 0.0).Equals(FVector(-500.0, 0.0, 150.0), 1e-3));
    TestTrue(TEXT("ends on the last point"), McpFlyThrough::Position(Legs, 8.0).Equals(FVector(500.0, 0.0, 150.0), 1e-2));

    const McpFlyThrough::FPlan Inside = McpFlyThrough::PlanCorners(Space, {FVector(-500.0, 0.0, 150.0), FVector(0.0, 0.0, 150.0)}, Budget);
    TestTrue(TEXT("a point in the wall is refused"), Inside.bInSolid && Inside.FailedPoint == 1);

    // Behind a wall with no end, the search runs out of cells and says so instead of flying through it.
    McpFlyThrough::FSpace Sealed = Space;
    Sealed.Free = [](const FVector& Point, double Radius) { return FMath::Min(Point.Z, FMath::Abs(Point.X) - 20.0) >= Radius; };
    Sealed.Clear = [Sealed](const FVector& From, const FVector& To, double Radius)
    { return FMath::Sign(From.X) == FMath::Sign(To.X) && Sealed.Free(From, Radius) && Sealed.Free(To, Radius); };
    McpFlyThrough::FSearchBudget Small;
    Small.Cells = 2000;
    const McpFlyThrough::FPlan Blocked = McpFlyThrough::PlanCorners(Sealed, {FVector(-500.0, 0.0, 150.0), FVector(500.0, 0.0, 150.0)}, Small);
    TestTrue(TEXT("no way through: NO_PATH, not a point in solid"), !Blocked.Error.IsEmpty() && !Blocked.bInSolid && Blocked.FailedPoint == 1);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
