// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Landscape/Erosion/McpTerrainErosion.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpTerrainErosionTest, "EngineWire.Landscape.TerrainErosion",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
McpTerrainErosion::FGrid TerrainGrid(int32 Width, int32 Height, double Fill)
{
    McpTerrainErosion::FGrid Grid;
    Grid.Width = Width;
    Grid.Height = Height;
    Grid.Heights.Init(Fill, Width * Height);
    return Grid;
}

double TerrainSum(const McpTerrainErosion::FGrid& Grid)
{
    double Sum = 0.0;
    for (const double Height : Grid.Heights)
    {
        Sum += Height;
    }
    return Sum;
}
} // namespace

// Known answers: a pit fills to its spill height and the flat lake it leaves drains and stays level under stream power; a
// tilted plane drains straight down, its catchment growing a cell a column; one implicit step on a two-cell chain halves
// the drop at F = 1; droplets repeat for a seed and never touch the border; a talus slump keeps its material; a V valley
// carries one river from its head to the border.
bool FMcpTerrainErosionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpTerrainErosion;
    {
        FGrid Bowl = TerrainGrid(7, 7, 0.0);
        for (int32 Index = 0; Index < Bowl.Num(); ++Index)
        {
            Bowl.Heights[Index] = Bowl.IsBorder(Index) ? 10.0 : 0.0;
        }
        Bowl.Heights[3 * 7] = 5.0;
        const FFill Filled = Fill(Bowl);
        TestTrue(TEXT("pit: 25 cells raised 5 in one depression"), Filled.Cells == 25 && Filled.Depressions == 1 && FMath::IsNearlyEqual(Filled.MaxRise, 5.0));
        TestEqual(TEXT("pit: at the spill height"), Bowl.Heights[3 * 7 + 3], 5.0);
        const FFlow Lake = Flow(Bowl);
        TestTrue(TEXT("lake: every cell has a receiver and drains"), Lake.Pits == 0 && Undrained(Bowl) == 0);
        StreamPower(Bowl, Lake, 100.0);
        TestEqual(TEXT("lake: level under stream power (no cell an ulp under its receiver)"), Bowl.Heights[2 * 7 + 2], 5.0);
    }
    {
        FGrid Plane = TerrainGrid(8, 5, 0.0);
        for (int32 Index = 0; Index < Plane.Num(); ++Index)
        {
            Plane.Heights[Index] = (Index % 8) * 10.0;
        }
        const FFlow Down = Flow(Plane);
        bool bStraight = true;
        for (int32 Y = 1; Y < 4; ++Y)
        {
            for (int32 X = 1; X < 7; ++X)
            {
                const int32 Cell = Y * 8 + X;
                bStraight &= Down.Receiver[Cell] == Cell - 1 && FMath::IsNearlyEqual(Down.Area[Cell] / (Plane.CellX * Plane.CellY), 7.0 - X);
            }
        }
        TestTrue(TEXT("tilted plane: straight down, catchment a cell per column"), bStraight);
    }
    {
        FGrid Chain = TerrainGrid(3, 3, 0.0);
        Chain.Heights[4] = 10.0;
        StreamPower(Chain, Flow(Chain), 100.0);
        TestEqual(TEXT("implicit step: (10 - 0) / (1 + 1)"), Chain.Heights[4], 5.0, 1e-9);
    }
    {
        FGrid A = TerrainGrid(64, 64, 0.0);
        for (int32 Index = 0; Index < A.Num(); ++Index)
        {
            A.Heights[Index] = 500.0 * FMath::Sin((Index % 64) / 9.0) * FMath::Cos((Index / 64) / 7.0) + 20.0 * (Index % 64);
        }
        FGrid B = A, C = A;
        const FGrid Start = A;
        Droplets(A, 3000, 7, TNumericLimits<double>::Max());
        Droplets(B, 3000, 7, TNumericLimits<double>::Max());
        Droplets(C, 3000, 8, TNumericLimits<double>::Max());
        bool bHeld = true;
        for (int32 Index = 0; Index < A.Num(); ++Index)
        {
            bHeld &= !A.IsBorder(Index) || A.Heights[Index] == Start.Heights[Index];
        }
        TestTrue(TEXT("droplets: the same seed, the same terrain"), A.Heights == B.Heights);
        TestTrue(TEXT("droplets: another seed, another terrain"), A.Heights != C.Heights);
        TestTrue(TEXT("droplets: the border held"), bHeld);
    }
    {
        FGrid Spike = TerrainGrid(15, 15, 0.0);
        Spike.Heights[7 * 15 + 7] = 1000.0;
        const double Mass = TerrainSum(Spike);
        Thermal(Spike, 45.0, 3);
        TestTrue(TEXT("talus: the spike slumps"), Spike.Heights[7 * 15 + 7] < 1000.0);
        TestEqual(TEXT("talus: no material lost inside the grid"), TerrainSum(Spike), Mass, 1e-6);
    }
    {
        // A valley running down to x = 0, its sides falling to the middle row: one river along that row.
        FGrid Valley = TerrainGrid(20, 9, 0.0);
        for (int32 Index = 0; Index < Valley.Num(); ++Index)
        {
            Valley.Heights[Index] = (Index % 20) * 10.0 + FMath::Abs(Index / 20 - 4) * 30.0;
        }
        const TArray<FRiver> Found = Rivers(Valley, Flow(Valley), 10.0 * Valley.CellX * Valley.CellY, 5);
        TestTrue(TEXT("valley: one river, a straight line from its head to the border"),
                 Found.Num() == 1 && Found[0].Points.Num() == 2 && Found[0].Points.Last().X == 0.0 && Found[0].Points.Last().Y == 4.0);
    }
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
