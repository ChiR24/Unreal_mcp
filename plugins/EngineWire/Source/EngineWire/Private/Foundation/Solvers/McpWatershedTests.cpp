// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Solvers/McpWatershed.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
// A grid of Width x Depth cells, Open saying which are floor, four-neighbour adjacency, and each cell's height the hops
// to the nearest floor cell with fewer than four floor neighbours (an edge), as a level's samples are measured.
void WatershedTestGrid(int32 Width, int32 Depth, TFunctionRef<bool(int32, int32)> Open,
                       TArray<TArray<int32>>& OutAdjacency, TArray<int32>& OutHeight)
{
    TArray<int32> Index;
    Index.Init(INDEX_NONE, Width * Depth);
    for (int32 Y = 0; Y < Depth; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            Index[Y * Width + X] = Open(X, Y) ? OutAdjacency.AddDefaulted() : INDEX_NONE;
        }
    }
    const int32 Steps[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    TArray<int32> Queue;
    OutHeight.Init(INDEX_NONE, OutAdjacency.Num());
    for (int32 Y = 0; Y < Depth; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            const int32 Node = Index[Y * Width + X];
            for (const auto& Step : Steps)
            {
                const int32 NX = X + Step[0];
                const int32 NY = Y + Step[1];
                const int32 Next = NX >= 0 && NX < Width && NY >= 0 && NY < Depth ? Index[NY * Width + NX] : INDEX_NONE;
                if (Node != INDEX_NONE && Next != INDEX_NONE)
                {
                    OutAdjacency[Node].Add(Next);
                }
            }
            if (Node != INDEX_NONE && OutAdjacency[Node].Num() < 4)
            {
                OutHeight[Node] = 0;
                Queue.Add(Node);
            }
        }
    }
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        for (const int32 Next : OutAdjacency[Queue[Head]])
        {
            if (OutHeight[Next] == INDEX_NONE)
            {
                OutHeight[Next] = OutHeight[Queue[Head]] + 1;
                Queue.Add(Next);
            }
        }
    }
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpWatershedTest, "EngineWire.Foundation.Solvers.Watershed",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers: two 7 x 7 rooms joined by a one-cell neck are two basins, every cell of each room in its room's basin;
// one 9 x 9 room is one basin; the same input gives the same basins twice.
bool FMcpWatershedTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TArray<TArray<int32>> Adjacency;
    TArray<int32> Height;
    // Rooms at x 0-6 and x 10-16, the neck at y 3 on x 7-9.
    WatershedTestGrid(17, 7, [](int32 X, int32 Y) { return X <= 6 || X >= 10 || Y == 3; }, Adjacency, Height);
    int32 Count = 0;
    const TArray<int32> Basins = McpWatershed::Basins(Adjacency, Height, 0.75, Count);
    TestEqual(TEXT("two rooms joined by a neck are two basins"), Count, 2);
    // Grid order: the first open cell is (0, 0) in room one and the last (16, 6) in room two.
    TestNotEqual(TEXT("the rooms' corners are in different basins"), Basins[0], Basins.Last());
    int32 SecondCount = 0;
    TestTrue(TEXT("the same input gives the same basins"), McpWatershed::Basins(Adjacency, Height, 0.75, SecondCount) == Basins);

    Adjacency.Reset();
    WatershedTestGrid(9, 9, [](int32, int32) { return true; }, Adjacency, Height);
    McpWatershed::Basins(Adjacency, Height, 0.75, Count);
    TestEqual(TEXT("one room is one basin"), Count, 1);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
