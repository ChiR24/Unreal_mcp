// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Environment/Modules/McpModuleKit.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "DynamicMesh/DynamicMesh3.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpModuleKitTest, "EngineWire.Environment.ModuleKit",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The helpers live in McpModuleKit's own anonymous namespace: a using-directive in the global one would reach every
// later file of the unity blob.
namespace McpModuleKit
{
namespace
{
constexpr double ModuleTestCell = 400.0;

// A box from Low to High in units of the cell (0-1), appended as 12 triangles.
void ModuleTestBox(UE::Geometry::FDynamicMesh3& Mesh, const FVector& Low, const FVector& High)
{
    int32 Corner[8];
    for (int32 Index = 0; Index < 8; ++Index)
    {
        Corner[Index] = Mesh.AppendVertex(FVector3d((Index & 1 ? High.X : Low.X) * ModuleTestCell, (Index & 2 ? High.Y : Low.Y) * ModuleTestCell,
                                                    (Index & 4 ? High.Z : Low.Z) * ModuleTestCell));
    }
    const int32 Faces[12][3] = {{0, 2, 1}, {1, 2, 3}, {4, 5, 6}, {5, 7, 6}, {0, 1, 4}, {1, 5, 4},
                                {2, 6, 3}, {3, 6, 7}, {0, 4, 2}, {2, 4, 6}, {1, 3, 5}, {3, 7, 5}};
    for (const int32* Face : Faces)
    {
        Mesh.AppendTriangle(Corner[Face[0]], Corner[Face[1]], Corner[Face[2]]);
    }
}

// A corridor piece made of boxes, corner pivot: a slab in the middle with an arm out to each side it opens on (+X, -X,
// +Y, -Y), every turn of it a variant; none at all is the empty cell.
void ModuleTestPiece(TArray<FModuleVariant>& Out, int32 Module, const TArray<int32>& Arms, double Weight)
{
    const FVector Cell(ModuleTestCell);
    if (Arms.Num() == 0)
    {
        Out.AddDefaulted_GetRef().Weight = Weight;
        return;
    }
    UE::Geometry::FDynamicMesh3 Mesh;
    ModuleTestBox(Mesh, FVector(0.3, 0.3, 0.0), FVector(0.7, 0.7, 0.3));
    const FVector ArmLow[4] = {FVector(0.7, 0.3, 0.0), FVector(0.0, 0.3, 0.0), FVector(0.3, 0.7, 0.0), FVector(0.3, 0.0, 0.0)};
    const FVector ArmHigh[4] = {FVector(1.0, 0.7, 0.3), FVector(0.3, 0.7, 0.3), FVector(0.7, 1.0, 0.3), FVector(0.7, 0.3, 0.3)};
    for (const int32 Arm : Arms)
    {
        ModuleTestBox(Mesh, ArmLow[Arm], ArmHigh[Arm]);
    }
    const UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh, true);
    for (int32 Turn = 0; Turn < 4; ++Turn)
    {
        FModuleVariant& Variant = Out.AddDefaulted_GetRef();
        Variant.Module = Module;
        Variant.Turns = Turn;
        Variant.Weight = Weight / 4.0;
        SampleFaces(Tree, Cell, CellFromMesh(Cell / 2.0, Turn), Variant.Faces);
    }
}

// The 16 x 16 occupancy of a strip along the bottom of a face, from Low to High across it.
FModuleFace ModuleTestStrip(double Low, double High)
{
    FModuleFace Profile;
    for (int32 Row = 0; Row < ProfileSide && (Row + 0.5) / ProfileSide <= 0.3; ++Row)
    {
        for (int32 Column = 0; Column < ProfileSide; ++Column)
        {
            const double Across = (Column + 0.5) / ProfileSide;
            if (Across >= Low && Across <= High)
            {
                Profile.Set(Column, Row);
            }
        }
    }
    return Profile;
}
} // namespace
} // namespace McpModuleKit

// Known answers: faces match by IoU, so a 30 cm and a 40 cm corridor stay apart; a piece turned a quarter turn about Z
// carries its arm from +X to +Y, read through the transform its instance is placed with; a corridor kit lays out a grid
// with no open face, the same seed the same layout; a kit of straights alone fits no closed grid.
bool FMcpModuleKitTest::RunTest(const FString& Parameters)
{
    using namespace McpModuleKit;
    const FModuleFace Air;
    TestTrue(TEXT("air meets air"), Matches(Air, Air));
    TestFalse(TEXT("air never meets a corridor"), Matches(Air, ModuleTestStrip(0.35, 0.65)));
    TestTrue(TEXT("the same corridor meets itself"), Matches(ModuleTestStrip(0.35, 0.65), ModuleTestStrip(0.35, 0.65)));
    TestFalse(TEXT("a corridor 0.3 of the face wide never meets one 0.4 wide (IoU 0.67)"), Matches(ModuleTestStrip(0.35, 0.65), ModuleTestStrip(0.3, 0.7)));

    // A wall 1/40 of the cell thick along the +X side shows on the +Y face as a strip thinner than a square: one ray at
    // each square's middle misses it, 4 x 4 do not.
    UE::Geometry::FDynamicMesh3 Wall;
    ModuleTestBox(Wall, FVector(0.975, 0.0, 0.0), FVector(1.0, 1.0, 1.0));
    const UE::Geometry::FDynamicMeshAABBTree3 WallTree(&Wall, true);
    FModuleFace WallFaces[6];
    SampleFaces(WallTree, FVector(ModuleTestCell), CellFromMesh(FVector(ModuleTestCell / 2.0), 0), WallFaces);
    TestFalse(TEXT("a wall 1/40 of the cell thick shows on the face beside it"), WallFaces[2].IsAir());
    TestTrue(TEXT("and covers its own face"), Matches(WallFaces[0], WallFaces[0]) && !WallFaces[0].IsAir());

    // The arm tip of a piece pointing +X, a quarter turn on, points +Y.
    const FVector Tip = CellFromMesh(FVector(ModuleTestCell / 2.0), 1).TransformPosition(FVector(ModuleTestCell, ModuleTestCell / 2.0, 0.0));
    TestTrue(TEXT("a quarter turn carries +X to +Y"), Tip.Equals(FVector(0.0, ModuleTestCell / 2.0, -ModuleTestCell / 2.0), 0.01));

    FModuleProblem Kit;
    Kit.Size = FIntVector(8, 6, 1);
    ModuleTestPiece(Kit.Variants, 0, {0}, 1.0);              // end
    ModuleTestPiece(Kit.Variants, 1, {0, 1}, 2.0);           // straight
    ModuleTestPiece(Kit.Variants, 2, {0, 2}, 1.0);           // corner
    ModuleTestPiece(Kit.Variants, 3, {0, 1, 2}, 1.0);        // tee
    ModuleTestPiece(Kit.Variants, 4, {0, 1, 2, 3}, 1.0);     // cross
    ModuleTestPiece(Kit.Variants, INDEX_NONE, {}, 2.0);      // empty
    const FModuleVariant& EndTurned = Kit.Variants[1];
    TestTrue(TEXT("an end turned 90 has its arm on +Y"), !EndTurned.Faces[2].IsAir() && EndTurned.Faces[0].IsAir());
    TestTrue(TEXT("its arm meets a straight's along Y"), Matches(EndTurned.Faces[2], Kit.Variants[5].Faces[3]));
    TestTrue(TEXT("every corridor piece stands on the floor"), !Kit.Variants[0].Faces[5].IsAir() && Kit.Variants[0].Faces[4].IsAir());
    TestEqual(TEXT("no face of the corridor kit goes unmatched"), UnmatchedFaces(Kit).Num(), 0);

    Kit.Seed = 7;
    Kit.Deadline = TNumericLimits<double>::Max();
    const FModuleLayout First = Solve(Kit);
    TestTrue(TEXT("the corridor kit lays out an 8 x 6 grid"), First.bSolved);
    TestEqual(TEXT("with no open face"), OpenFaces(Kit, First.Placed), 0);
    TestTrue(TEXT("the same seed gives the same layout"), Solve(Kit).Placed == First.Placed);
    int32 Corridors = 0;
    for (const int32 Placed : First.Placed)
    {
        Corridors += Kit.Variants[Placed].Module != INDEX_NONE ? 1 : 0;
    }
    TestTrue(TEXT("and places corridors, not only empty cells"), Corridors > 0);

    FModuleProblem Straights;
    Straights.Size = FIntVector(4, 4, 1);
    ModuleTestPiece(Straights.Variants, 0, {0, 1}, 1.0);
    Straights.Deadline = TNumericLimits<double>::Max();
    TestFalse(TEXT("straights alone fit no grid closed on its sides"), Solve(Straights).bSolved);
    return true;
}
#endif
