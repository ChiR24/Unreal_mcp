// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Environment/Inspection/MeshCheck/McpMeshCheck.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpMeshCheckTest, "EngineWire.Inspect.MeshCheck",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
// A 100 cm cube at Offset: its corner K sits at x = K & 1, y = K & 2, z = K & 4. Each face's four corners are listed so
// that its triangles (a, b, c) and (a, c, d) face out as Unreal draws a front face: (c - a) x (b - a) points out.
// Reversed winds every triangle the other way; Split gives each face its own four corners (flat shading), which
// leaves every edge a seam; OpenTop leaves the top face out.
struct FMcpCheckCube
{
    FVector3f Offset = FVector3f::ZeroVector;
    bool bReversed = false;
    bool bSplit = false;
    bool bOpenTop = false;
};

void McpAppendCheckCube(const FMcpCheckCube& Cube, TArray<FVector3f>& Positions, TArray<int32>& Corners)
{
    static const int32 Faces[6][4] = {{4, 6, 7, 5}, {0, 1, 3, 2}, {1, 5, 7, 3}, {0, 2, 6, 4}, {2, 3, 7, 6}, {0, 4, 5, 1}};
    const auto Place = [&Cube, &Positions](int32 K)
    {
        Positions.Add(Cube.Offset + FVector3f((K & 1) ? 100.0f : 0.0f, (K & 2) ? 100.0f : 0.0f, (K & 4) ? 100.0f : 0.0f));
        return Positions.Num() - 1;
    };
    int32 Shared[8] = {};
    for (int32 K = 0; K < 8 && !Cube.bSplit; ++K)
    {
        Shared[K] = Place(K);
    }
    for (int32 Face = Cube.bOpenTop ? 1 : 0; Face < 6; ++Face)
    {
        int32 Quad[4];
        for (int32 K = 0; K < 4; ++K)
        {
            Quad[K] = Cube.bSplit ? Place(Faces[Face][K]) : Shared[Faces[Face][K]];
        }
        for (const int32 Third : {2, 3})
        {
            Corners.Append({Quad[0], Cube.bReversed ? Quad[Third] : Quad[Third - 1], Cube.bReversed ? Quad[Third - 1] : Quad[Third]});
        }
    }
}

McpMeshCheck::FReport McpCheckCubes(std::initializer_list<FMcpCheckCube> Cubes)
{
    TArray<FVector3f> Positions;
    TArray<int32> Corners;
    for (const FMcpCheckCube& Cube : Cubes)
    {
        McpAppendCheckCube(Cube, Positions, Corners);
    }
    return McpMeshCheck::Analyze(Positions, Corners);
}
} // namespace

// Known answers: a cube wound outward is one closed part, 1 m3, none inside out; the same cube with every triangle
// reversed is closed and consistent (no flipped edge) and one part inside out; split at every edge (flat shading) it reads
// the same through its seams; with its top left out it is open with one hole and judged neither way; of two cubes the
// inside-out one is listed first.
bool FMcpMeshCheckTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const McpMeshCheck::FReport Out = McpCheckCubes({FMcpCheckCube()});
    TestTrue(TEXT("outward: one closed part of 12 triangles"), Out.IsClosed() && Out.Parts.Num() == 1 && Out.Triangles == 12);
    TestEqual(TEXT("outward: none inside out"), Out.InsideOutParts, 0);
    TestEqual(TEXT("outward: no flipped edge"), Out.FlippedEdges, 0);
    TestEqual(TEXT("outward: 1 m3"), Out.Volume, 1000000.0, 1.0);

    FMcpCheckCube Reversed;
    Reversed.bReversed = true;
    const McpMeshCheck::FReport In = McpCheckCubes({Reversed});
    TestTrue(TEXT("reversed: still closed, its windings consistent"), In.IsClosed() && In.FlippedEdges == 0);
    TestEqual(TEXT("reversed: one part inside out"), In.InsideOutParts, 1);
    TestTrue(TEXT("reversed: the part is marked"), In.Parts.Num() == 1 && In.Parts[0].bInsideOut);
    TestEqual(TEXT("reversed: its size still counts as its volume"), In.Volume, 1000000.0, 1.0);

    FMcpCheckCube Split = Reversed;
    Split.bSplit = true;
    const McpMeshCheck::FReport Seams = McpCheckCubes({Split});
    TestTrue(TEXT("split: every edge a seam, closed"), Seams.IsClosed() && Seams.SeamEdges == 12 && Seams.Vertices == 8);
    TestEqual(TEXT("split: inside out through its seams"), Seams.InsideOutParts, 1);

    FMcpCheckCube Open;
    Open.bOpenTop = true;
    const McpMeshCheck::FReport Box = McpCheckCubes({Open});
    TestTrue(TEXT("open box: four open edges in one hole"), !Box.IsClosed() && Box.OpenEdges == 4 && Box.Holes == 1);
    TestEqual(TEXT("open box: an open part is not judged"), Box.InsideOutParts, 0);
    Open.bReversed = true;
    TestEqual(TEXT("open box reversed: still not judged"), McpCheckCubes({Open}).InsideOutParts, 0);

    FMcpCheckCube Far = Reversed;
    Far.Offset = FVector3f(100000.0f, 0.0f, 0.0f);
    const McpMeshCheck::FReport Two = McpCheckCubes({FMcpCheckCube(), Far});
    TestTrue(TEXT("two cubes: two parts, one inside out"), Two.Parts.Num() == 2 && Two.InsideOutParts == 1);
    TestTrue(TEXT("two cubes: the inside-out one is listed first, at its place"),
             Two.Parts.Num() == 2 && Two.Parts[0].bInsideOut && Two.Parts[0].Box.GetCenter().X > 50000.0);

    TestEqual(TEXT("a corner past the positions reads nothing"), McpMeshCheck::Analyze({FVector3f::ZeroVector}, {0, 0, 1}).Triangles, 0);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
