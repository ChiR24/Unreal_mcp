#include "Foundation/GraphLayout/McpLayered.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpLayeredTest, "EngineWire.Foundation.GraphLayout.Layered",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers: a branch that joins again (a diamond) in three columns, the first-walked side on top, the join between
// the two sides; a loop wired back changes nothing; a pure value sits in the column just left of its reader.
bool FMcpLayeredTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using McpLayered::FBox;
    using McpLayered::FEdge;
    TArray<FBox> Boxes;
    Boxes.Init(FBox{200.0f, 100.0f}, 4);
    TArray<FEdge> Edges = {{0, 1, 0, true}, {0, 2, 1, true}, {1, 3, 0, true}, {2, 3, 0, true}};
    const FVector2D Anchor(1000.0, 500.0);
    const TArray<FVector2D> Diamond = McpLayered::Layout(Boxes, Edges, Anchor);
    TestTrue(TEXT("box 0 stays where it was"), Diamond[0].Equals(Anchor));
    TestTrue(TEXT("the two sides share the next column, the first linked on top"), Diamond[1].Equals(FVector2D(1296.0, 500.0)) && Diamond[2].Equals(FVector2D(1296.0, 640.0)));
    TestTrue(TEXT("the join sits between them, one column on"), Diamond[3].Equals(FVector2D(1592.0, 570.0)));

    Edges.Add({3, 0, 0, true});
    const TArray<FVector2D> Looped = McpLayered::Layout(Boxes, Edges, Anchor);
    bool bSame = true;
    for (int32 At = 0; At < 4; ++At)
    {
        bSame &= Looped[At].Equals(Diamond[At]);
    }
    TestTrue(TEXT("a link back to the start is laid out as if it were not there"), bSame);

    Boxes.Add(FBox{100.0f, 50.0f});
    Edges.Add({4, 3, 0, false});
    const TArray<FVector2D> Valued = McpLayered::Layout(Boxes, Edges, Anchor);
    TestEqual(TEXT("a pure value is in the column before its reader"), Valued[4].X, 1296.0, 1e-6);
    TestTrue(TEXT("below the boxes already there"), Valued[4].Y >= Valued[2].Y + 100.0);
    return true;
}
#endif
