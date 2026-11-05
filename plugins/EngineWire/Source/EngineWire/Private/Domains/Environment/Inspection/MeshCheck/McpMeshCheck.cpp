// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Environment/Inspection/MeshCheck/McpMeshCheck.h"

namespace McpMeshCheck
{
namespace
{
// Union-find over welded corners, for the parts and the hole outlines.
int32 FindRoot(TArray<int32>& Parent, int32 Index)
{
    while (Parent[Index] != Index)
    {
        Parent[Index] = Parent[Parent[Index]];
        Index = Parent[Index];
    }
    return Index;
}

void Join(TArray<int32>& Parent, int32 A, int32 B)
{
    Parent[FindRoot(Parent, A)] = FindRoot(Parent, B);
}

uint64 EdgeKey(int32 A, int32 B)
{
    return (static_cast<uint64>(FMath::Min(A, B)) << 32) | static_cast<uint32>(FMath::Max(A, B));
}

// A source edge: how many faces run it, the corner the first one runs it from, and whether the second runs it the
// same way (which leaves the two faces wound opposite ways).
struct FEdgeUse
{
    int32 From = 0;
    int32 Faces = 0;
    bool bSameWay = false;
};

// An edge with a face on one side only, in welded corners, as that face runs it: a second one on the same two places
// is the other side of a seam.
struct FOpenRun
{
    int32 From = 0, To = 0, Count = 0;
};
} // namespace

FReport Analyze(const TArray<FVector3f>& Positions, const TArray<int32>& Corners)
{
    FReport Report;
    if (Corners.ContainsByPredicate([&Positions](int32 Corner) { return !Positions.IsValidIndex(Corner); }))
    {
        return Report;
    }
    Report.Triangles = Corners.Num() / 3;

    // Corners at one place (to 0.01 cm) count as one vertex: a mesh whose faces are split (flat shading, some
    // exporters' UV seams) keeps a vertex per face corner, and the edges between those faces are seams, not holes.
    TArray<int32> Weld;
    Weld.Init(INDEX_NONE, Positions.Num());
    TMap<FIntVector, int32> WeldIds;
    for (const int32 Corner : Corners)
    {
        if (Weld[Corner] == INDEX_NONE)
        {
            const FVector3f& P = Positions[Corner];
            const FIntVector Key(FMath::RoundToInt(P.X * 100.0f), FMath::RoundToInt(P.Y * 100.0f), FMath::RoundToInt(P.Z * 100.0f));
            const int32* Found = WeldIds.Find(Key);
            Weld[Corner] = Found ? *Found : WeldIds.Add(Key, WeldIds.Num());
        }
    }
    Report.Vertices = WeldIds.Num();
    TArray<int32> PartParent, HoleParent;
    for (int32 Index = 0; Index < WeldIds.Num(); ++Index)
    {
        PartParent.Add(Index);
        HoleParent.Add(Index);
    }

    // Faces: area, the parts they join, the ones with no area (a sliver or a repeated corner), and the edges they run.
    TMap<uint64, FEdgeUse> Edges;
    Edges.Reserve(Corners.Num());
    for (int32 Triangle = 0; Triangle < Report.Triangles; ++Triangle)
    {
        const int32* Corner = &Corners[Triangle * 3];
        const FVector A(Positions[Corner[0]]), B(Positions[Corner[1]]), C(Positions[Corner[2]]);
        const double TwiceArea = FVector::CrossProduct(B - A, C - A).Size();
        Report.SurfaceArea += TwiceArea * 0.5;
        Report.DegenerateTriangles += TwiceArea < 1e-6 ? 1 : 0;
        Report.Bounds += A;
        Report.Bounds += B;
        Report.Bounds += C;
        Join(PartParent, Weld[Corner[0]], Weld[Corner[1]]);
        Join(PartParent, Weld[Corner[0]], Weld[Corner[2]]);
        for (int32 Side = 0; Side < 3; ++Side)
        {
            const int32 From = Corner[Side], To = Corner[(Side + 1) % 3];
            if (From == To)
            {
                continue;
            }
            FEdgeUse& Use = Edges.FindOrAdd(EdgeKey(From, To));
            if (Use.Faces == 0)
            {
                Use.From = From;
            }
            else if (Use.Faces == 1)
            {
                Use.bSameWay = Use.From == From;
            }
            ++Use.Faces;
        }
    }

    // Edges: three or more faces is non-manifold, and two faces that both run an edge the same way have opposite
    // windings, so one of them faces inward. An edge with a face on one side only is open, unless another open
    // edge lies on the same two places: that pair is a seam, checked for winding the same way.
    TSet<int32> Unclosed; // the parts with an open or a non-manifold edge
    TMap<uint64, FOpenRun> OpenRuns;
    for (const TPair<uint64, FEdgeUse>& Pair : Edges)
    {
        const FEdgeUse& Use = Pair.Value;
        const int32 Low = static_cast<int32>(Pair.Key >> 32), High = static_cast<int32>(Pair.Key & 0xffffffffu);
        const int32 From = Weld[Use.From], To = Weld[Use.From == Low ? High : Low];
        if (Use.Faces > 2)
        {
            ++Report.NonManifoldEdges;
            Unclosed.Add(FindRoot(PartParent, From));
        }
        else if (Use.Faces == 2)
        {
            Report.FlippedEdges += Use.bSameWay && From != To ? 1 : 0;
        }
        else
        {
            FOpenRun& Seen = OpenRuns.FindOrAdd(EdgeKey(From, To));
            Report.FlippedEdges += Seen.Count == 1 && Seen.From == From ? 1 : 0;
            Seen = {From, To, Seen.Count + 1};
        }
    }
    for (const TPair<uint64, FOpenRun>& Pair : OpenRuns)
    {
        if (Pair.Value.Count == 1)
        {
            ++Report.OpenEdges;
            Join(HoleParent, Pair.Value.From, Pair.Value.To);
            Unclosed.Add(FindRoot(PartParent, Pair.Value.From));
        }
        else
        {
            ++Report.SeamEdges;
        }
    }
    TSet<int32> Holes;
    for (const TPair<uint64, FOpenRun>& Pair : OpenRuns)
    {
        if (Pair.Value.Count == 1)
        {
            Holes.Add(FindRoot(HoleParent, Pair.Value.From));
        }
    }
    Report.Holes = Holes.Num();

    // Parts: each closed one encloses a volume of its own, summed from one of its own corners so a part far from the
    // origin keeps its precision. Unreal is left-handed: a face's outward normal is (C - A) x (B - A) (VectorUtil::Normal),
    // so a part wound outward comes out positive here and one wound inside out negative.
    TMap<int32, int32> PartOf;
    TArray<FVector> Origins;
    for (int32 Triangle = 0; Triangle < Report.Triangles; ++Triangle)
    {
        const int32* Corner = &Corners[Triangle * 3];
        const FVector A(Positions[Corner[0]]), B(Positions[Corner[1]]), C(Positions[Corner[2]]);
        const int32 Root = FindRoot(PartParent, Weld[Corner[0]]);
        const int32* Known = PartOf.Find(Root);
        const int32 Index = Known ? *Known : PartOf.Add(Root, Report.Parts.AddDefaulted());
        if (!Known)
        {
            Report.Parts[Index].bClosed = !Unclosed.Contains(Root);
            Origins.Add(A);
        }
        FPart& Part = Report.Parts[Index];
        const FVector O = Origins[Index];
        Part.Triangles += 1;
        Part.Box += A;
        Part.Box += B;
        Part.Box += C;
        Part.Volume += FVector::DotProduct(A - O, FVector::CrossProduct(C - O, B - O)) / 6.0;
    }
    for (FPart& Part : Report.Parts)
    {
        // A closed sheet lying on itself (a plane's two sides) encloses nothing: only a volume past rounding counts.
        const double Size = Part.Box.GetSize().GetMax();
        Part.bInsideOut = Part.bClosed && Part.Volume < -1e-9 * Size * Size * Size;
        Report.InsideOutParts += Part.bInsideOut ? 1 : 0;
        Report.Volume += Part.bClosed ? FMath::Abs(Part.Volume) : 0.0;
    }
    Report.Parts.Sort([](const FPart& L, const FPart& R)
    {
        return L.bInsideOut != R.bInsideOut ? L.bInsideOut : L.Triangles > R.Triangles;
    });
    return Report;
}
} // namespace McpMeshCheck
