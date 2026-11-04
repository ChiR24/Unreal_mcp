// Copyright (c) 2024 EngineWire Contributors

#include "Domains/ControlActor/Placement/McpCoplanarFaces.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpCoplanarPlanTest, "EngineWire.ControlActor.CoplanarPlan",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
// A box filled on every side, as a cube mesh scaled to it reads.
McpCoplanar::FMcpCoplanarPiece CoplanarTestBox(int32 Owner, const FVector& Min, const FVector& Max)
{
    McpCoplanar::FMcpCoplanarPiece Piece;
    Piece.Owner = Owner;
    Piece.Bounds = FBox(Min, Max);
    Piece.Volume = Piece.Bounds.GetVolume();
    McpCoplanar::FMcpCoplanarFill Full;
    for (double& Share : Full.Share)
    {
        Share = 1.0;
    }
    McpCoplanar::AddBoxFaces(Piece.Bounds, FTransform::Identity, Full, Piece);
    return Piece;
}

// One fix_coplanar pass planned over every pair of Pieces, the pieces moved where it puts them, and the pairs still in
// one plane afterwards.
TArray<McpCoplanar::FMcpCoplanarPair> CoplanarTestPass(TArray<McpCoplanar::FMcpCoplanarPiece>& Pieces,
                                                       McpCoplanar::FMcpCoplanarSettle& OutSettle)
{
    OutSettle = McpCoplanar::SettleCoplanar(Pieces, McpCoplanar::FindPairs(Pieces, false), 1.0);
    for (int32 Index = 0; Index < Pieces.Num(); ++Index)
    {
        Pieces[Index].Bounds = Pieces[Index].Bounds.ShiftBy(OutSettle.Offsets[Index]);
        for (McpCoplanar::FMcpCoplanarFace& Face : Pieces[Index].Faces)
        {
            Face.Center += OutSettle.Offsets[Index];
        }
    }
    return McpCoplanar::FindPairs(Pieces, false);
}
} // namespace

// Known answers. A|B|C in a row: B's -X end lies partly in A's -X face and its +X end holds C's +X end whole. C comes
// forward first; B going back off A, as the rule prefers, would carry its +X end into C's new plane, so B comes forward
// instead. Four walls of a room overlapping at the corners share sixteen patches (two side planes, the tops and the
// bottoms at each corner); one pass parts them all whichever order the walls come in. A pair of two pinned parts is
// left, with both reasons.
bool FMcpCoplanarPlanTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpCoplanar;
    {
        TArray<FMcpCoplanarPiece> Row = {CoplanarTestBox(0, FVector(0.0, 50.0, 10.0), FVector(100.0, 250.0, 300.0)),
                                         CoplanarTestBox(1, FVector(0.0, 0.0, 0.0), FVector(400.0, 100.0, 100.0)),
                                         CoplanarTestBox(2, FVector(350.0, 10.0, 10.0), FVector(400.0, 90.0, 90.0))};
        TestEqual(TEXT("A|B|C: B in one plane with A, and its other end with C"), FindPairs(Row, false).Num(), 2);
        FMcpCoplanarSettle Settle;
        TestEqual(TEXT("A|B|C: no pair left in one plane"), CoplanarTestPass(Row, Settle).Num(), 0);
        TestEqual(TEXT("A|B|C: no pair handed back"), Settle.Left.Num(), 0);
        TestTrue(TEXT("A|B|C: A stays"), Settle.Offsets[0].IsZero());
        TestTrue(TEXT("A|B|C: B comes forward out of A's face, not into C's plane"), Settle.Offsets[1].Equals(FVector(-1.0, 0.0, 0.0)));
        TestTrue(TEXT("A|B|C: C comes forward out of B's end"), Settle.Offsets[2].Equals(FVector(1.0, 0.0, 0.0)));
    }
    for (const bool bReversed : {false, true})
    {
        // The parts of one Blueprint: north, south, east and west walls 10 thick, each the room's full length.
        TArray<FMcpCoplanarPiece> Room = {CoplanarTestBox(0, FVector(-500.0, 490.0, 0.0), FVector(500.0, 500.0, 300.0)),
                                          CoplanarTestBox(0, FVector(-500.0, -500.0, 0.0), FVector(500.0, -490.0, 300.0)),
                                          CoplanarTestBox(0, FVector(490.0, -500.0, 0.0), FVector(500.0, 500.0, 300.0)),
                                          CoplanarTestBox(0, FVector(-500.0, -500.0, 0.0), FVector(-490.0, 500.0, 300.0))};
        if (bReversed)
        {
            Swap(Room[0], Room[3]);
            Swap(Room[1], Room[2]);
        }
        const TCHAR* Order = bReversed ? TEXT("west first") : TEXT("north first");
        TestEqual(FString::Printf(TEXT("room, %s: four patches at each corner"), Order), FindPairs(Room, false).Num(), 16);
        FMcpCoplanarSettle Settle;
        TestEqual(FString::Printf(TEXT("room, %s: no pair left in one plane"), Order), CoplanarTestPass(Room, Settle).Num(), 0);
        TestEqual(FString::Printf(TEXT("room, %s: no pair handed back"), Order), Settle.Left.Num(), 0);
    }
    {
        TArray<FMcpCoplanarPiece> Parts = {CoplanarTestBox(0, FVector(0.0, 0.0, 0.0), FVector(100.0, 100.0, 100.0)),
                                           CoplanarTestBox(0, FVector(50.0, 10.0, 0.0), FVector(150.0, 90.0, 50.0))};
        Parts[0].PinnedWhy = TEXT("Base is its root");
        Parts[1].PinnedWhy = TEXT("Lid is not a part its Blueprint defines");
        FMcpCoplanarSettle Settle;
        TestEqual(TEXT("pinned: the pair stays"), CoplanarTestPass(Parts, Settle).Num(), 1);
        TestTrue(TEXT("pinned: handed back with both reasons"),
                 Settle.Left.Num() == 1 && Settle.Left[0].Value.Contains(TEXT("root")) &&
                     Settle.Left[0].Value.Contains(TEXT("not a part")));
    }
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
