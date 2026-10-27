// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Environment/Roads/McpRoadPlan.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpRoadPlanTest, "EngineWire.Environment.RoadPlan",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
constexpr double RoadTestRadius = 1500.0;
constexpr double RoadTestCell = 720.0;  // 0.48 x the radius, as plan_road chooses

McpRoadPlan::FRoadField RoadTestField(int32 Width, int32 Height, uint8 Open)
{
    McpRoadPlan::FRoadField Field;
    Field.Width = Width;
    Field.Height = Height;
    Field.Cell = RoadTestCell;
    Field.Heights.Init(0.0, Width * Height);
    Field.Open.Init(Open, Width * Height);
    return Field;
}

// Searched, straightened and measured as plan_road does, half a 600 cm road kept clear of closed cells.
TArray<FIntPoint> RoadTestRoute(McpRoadPlan::FRoadField& Field, FIntPoint From, FIntPoint To, double MaxGrade, McpRoadPlan::FMeasure& OutMeasure)
{
    McpRoadPlan::Grow(Field, 1, {From, To});
    int32 Searched = 0;
    bool bTimedOut = false;
    const TArray<FIntPoint> Lattice = McpRoadPlan::Search(Field, From, To, MaxGrade, RoadTestRadius, TNumericLimits<double>::Max(), Searched, bTimedOut);
    const TArray<FIntPoint> Route = McpRoadPlan::Smooth(Field, Lattice, MaxGrade, RoadTestRadius);
    OutMeasure = McpRoadPlan::Measure(Field, Route);
    return Route;
}
} // namespace

// Known answers: one heading notch between one-cell edges turns at 2.12 cells; an L corridor's 90-degree bend keeps the
// radius on the final points; a 40 m flat-topped hill whose sides are 23% steep is climbed by a winding road under 12%;
// a box across the straight line is passed round; growing obstacles keeps the ground beside the two ends.
bool FMcpRoadPlanTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpRoadPlan;
    TestEqual(TEXT("one notch, straight edges: 2.118 cells"), CornerRadius(RoadTestCell, RoadTestCell, FMath::Atan2(1.0, 2.0)) / RoadTestCell, 2.118, 0.001);
    {
        FRoadField Corridor = RoadTestField(80, 80, 0);
        for (int32 Y = 5; Y <= 76; ++Y)
        {
            for (int32 X = 5; X <= 72; ++X)
            {
                Corridor.Open[Y * 80 + X] = (Y <= 14 || X >= 61) ? 1 : 0;
            }
        }
        FMeasure Laid;
        const TArray<FIntPoint> Route = RoadTestRoute(Corridor, FIntPoint(8, 9), FIntPoint(66, 72), 0.12, Laid);
        TestTrue(TEXT("L corridor: a road, clear, its corners at the radius or wider"), Route.Num() > 1 && Laid.bClear && Laid.MinRadiusCm >= RoadTestRadius - 1e-6);
    }
    {
        FRoadField Hill = RoadTestField(100, 100, 1);
        for (int32 Y = 0; Y < 100; ++Y)
        {
            for (int32 X = 0; X < 100; ++X)
            {
                const double Distance = FMath::Sqrt(static_cast<double>(FMath::Square(X - 50) + FMath::Square(Y - 50)));
                Hill.Heights[Y * 100 + X] = FMath::Clamp(4000.0 * (30.0 - Distance) / 24.0, 0.0, 4000.0);
            }
        }
        FMeasure Laid;
        const TArray<FIntPoint> Route = RoadTestRoute(Hill, FIntPoint(5, 50), FIntPoint(50, 50), 0.12, Laid);
        TestTrue(TEXT("hill: a road to the top"), Route.Num() > 1 && Route.Last() == FIntPoint(50, 50));
        TestTrue(TEXT("hill: no grade over 12% and no corner under the radius"), Laid.Grade <= 0.12 + 1e-9 && Laid.MinRadiusCm >= RoadTestRadius - 1e-6);
        TestTrue(TEXT("hill: it winds (the straight climb is 23%)"), Laid.Switchbacks >= 1 && Laid.LengthCm > 40.0 * RoadTestCell);
    }
    {
        FRoadField Field = RoadTestField(80, 40, 1);
        for (int32 Y = 10; Y < 30; ++Y)
        {
            for (int32 X = 38; X < 42; ++X)
            {
                Field.Open[Y * 80 + X] = 0;
            }
        }
        FMeasure Laid;
        const TArray<FIntPoint> Route = RoadTestRoute(Field, FIntPoint(5, 20), FIntPoint(75, 20), 0.1, Laid);
        TestTrue(TEXT("box: passed round, clear"), Laid.bClear && Route.ContainsByPredicate([](const FIntPoint& Point) { return Point.Y < 10 || Point.Y >= 30; }));
    }
    {
        FRoadField Door = RoadTestField(20, 20, 1);
        Door.Open[10 * 20 + 12] = 0;
        Grow(Door, 2, {FIntPoint(10, 10)});
        TestTrue(TEXT("grow: the ground beside an end stays open"), Door.IsOpen(10, 10) && Door.IsOpen(11, 10) && !Door.IsOpen(14, 10));
    }
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
