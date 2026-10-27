#include "Domains/Navigation/Analytics/McpLevelLayout.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
// A level drawn on a grid of 100 cm cells, every walkable cell a sample joined to its walkable eight neighbours, as
// McpLevelLayoutSample.cpp reads a flat nav mesh.
McpLevelLayout::FGraph LevelLayoutTestGraph(int32 Width, int32 Depth, TFunctionRef<bool(int32, int32)> Walkable)
{
    McpLevelLayout::FGraph Graph;
    Graph.GridSize = FIntPoint(Width, Depth);
    TMap<FIntPoint, int32> Index;
    for (int32 X = 0; X < Width; ++X)
    {
        for (int32 Y = 0; Y < Depth; ++Y)
        {
            if (Walkable(X, Y))
            {
                Index.Add(FIntPoint(X, Y), Graph.Points.Add(FVector(X * 100.0, Y * 100.0, 0.0)));
                Graph.Cells.Add(FIntPoint(X, Y));
                Graph.Floor.Add(1.0);
            }
        }
    }
    Graph.Adjacency.SetNum(Graph.Points.Num());
    for (int32 Node = 0; Node < Graph.Points.Num(); ++Node)
    {
        for (int32 DX = -1; DX <= 1; ++DX)
        {
            for (int32 DY = -1; DY <= 1; ++DY)
            {
                const int32* Next = (DX != 0 || DY != 0) ? Index.Find(Graph.Cells[Node] + FIntPoint(DX, DY)) : nullptr;
                if (Next)
                {
                    Graph.Adjacency[Node].Add(*Next);
                }
            }
        }
    }
    return Graph;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpLevelLayoutTest, "EngineWire.Navigation.LevelLayout",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers: two 8 x 8 rooms joined by a corridor three cells wide, and a 2 x 3 alcove off the second room, read as
// two rooms and two corridors; the corridor is on more routes than any room (the top choke), the alcove is a dead end,
// and there is no loop.
bool FMcpLevelLayoutTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const McpLevelLayout::FGraph Graph = LevelLayoutTestGraph(24, 8, [](int32 X, int32 Y)
    {
        return X <= 7 || (X >= 14 && X <= 21) || (X >= 8 && X <= 13 && Y >= 3 && Y <= 5) || (X >= 22 && Y >= 5);
    });
    const McpLevelLayout::FLayout Layout = McpLevelLayout::Spaces(Graph, 4);
    int32 Rooms = 0;
    for (const McpLevelLayout::FSpace& Space : Layout.Spaces)
    {
        Rooms += Space.bRoom ? 1 : 0;
    }
    TestEqual(TEXT("two rooms"), Rooms, 2);
    TestEqual(TEXT("and two corridors (the passage and the alcove)"), Layout.Spaces.Num() - Rooms, 2);
    if (Layout.Spaces.Num() != 4)
    {
        return false;
    }
    // Rooms first, then corridors largest first: the passage (18 cells) is space 2, the alcove (6) space 3.
    const McpLevelLayout::FSpace& Passage = Layout.Spaces[2];
    const McpLevelLayout::FSpace& Alcove = Layout.Spaces[3];
    // Every route from the first room to the second room or the alcove passes it; only those between the second room and
    // the alcove do not: about 0.92 of the routes between the others, for these sizes.
    TestTrue(TEXT("the passage is on nearly every route between the others"), Passage.PassShare > 0.85);
    TestTrue(TEXT("and on more of them than any room"), Passage.PassShare > Layout.Spaces[0].PassShare && Passage.PassShare > Layout.Spaces[1].PassShare);
    TestEqual(TEXT("the first room is on no route between the others"), Layout.Spaces[0].PassShare, 0.0, 1e-6);
    TestEqual(TEXT("the alcove has one neighbour: a dead end"), Alcove.Neighbours.Num(), 1);
    TestEqual(TEXT("three doors"), Layout.Doors.Num(), 3);
    TestEqual(TEXT("no loop"), Layout.Loops, 0);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
