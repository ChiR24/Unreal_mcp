#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"

// check_mesh: whether a static mesh's geometry is sound, read from its source triangles (the mesh as authored or
// imported, not a Nanite fallback). A model cannot see a mesh; an imported or generated one with a hole, an edge
// shared by three faces or faces turned inside out passed every other check.
namespace McpEnvironmentHandlers {
namespace {
constexpr int32 McpMaxListedParts = 8;

// Union-find over welded vertices, for the connected parts and the hole outlines.
int32 McpFindRoot(TArray<int32> &Parent, int32 Index)
{
    while (Parent[Index] != Index)
    {
        Parent[Index] = Parent[Parent[Index]];
        Index = Parent[Index];
    }
    return Index;
}

void McpJoin(TArray<int32> &Parent, int32 A, int32 B)
{
    Parent[McpFindRoot(Parent, A)] = McpFindRoot(Parent, B);
}

TArray<TSharedPtr<FJsonValue>> McpRoundedVec(const FVector &V)
{
    return McpHandlerUtils::VectorToJsonArray(FVector(FMath::RoundToDouble(V.X * 10.0) / 10.0,
        FMath::RoundToDouble(V.Y * 10.0) / 10.0, FMath::RoundToDouble(V.Z * 10.0) / 10.0));
}

uint64 McpEdgeKey(int32 A, int32 B)
{
    return (static_cast<uint64>(FMath::Min(A, B)) << 32) | static_cast<uint32>(FMath::Max(A, B));
}

struct FMcpMeshPart
{
    int32 Triangles = 0;
    FBox Box = FBox(ForceInit);
};

// An open edge as its one face runs it, in welded vertices.
struct FMcpOpenEdge
{
    int32 From = 0, To = 0, Count = 0;
};
} // namespace

bool HandleInspectMeshCheckAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString MeshPath = GetJsonStringField(Payload, TEXT("meshPath"));
    UStaticMesh *Mesh = MeshPath.IsEmpty() ? nullptr : Cast<UStaticMesh>(McpHandlerUtils::ResolveObjectFromPath(MeshPath));
    const FMeshDescription *Description = Mesh ? Mesh->GetMeshDescription(0) : nullptr;
    if (!Description)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("meshPath '%s' is not a static mesh with source geometry."), *MeshPath), TEXT("NOT_FOUND"));
        return true;
    }

    // Corners at one place (to 0.01 cm) count as one vertex: a mesh whose faces are split (flat shading, some
    // exporters' UV seams) keeps a vertex per face corner, and the edges between those faces are seams, not holes.
    const TVertexAttributesRef<const FVector3f> Positions = Description->GetVertexPositions();
    TArray<int32> Weld;
    Weld.Init(INDEX_NONE, Description->Vertices().GetArraySize());
    TMap<FIntVector, int32> WeldIds;
    for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
    {
        const FVector3f P = Positions[Vertex];
        const FIntVector Key(FMath::RoundToInt(P.X * 100.0f), FMath::RoundToInt(P.Y * 100.0f), FMath::RoundToInt(P.Z * 100.0f));
        const int32 *Found = WeldIds.Find(Key);
        Weld[Vertex.GetValue()] = Found ? *Found : WeldIds.Add(Key, WeldIds.Num());
    }
    TArray<int32> PartParent, HoleParent;
    for (int32 Index = 0; Index < WeldIds.Num(); ++Index)
    {
        PartParent.Add(Index);
        HoleParent.Add(Index);
    }

    // Faces: area, the parts they join, and the ones with no area (a sliver or a repeated corner).
    double Area = 0.0, SignedVolume = 0.0;
    int32 Degenerate = 0;
    FBox Bounds(ForceInit);
    for (const FTriangleID Triangle : Description->Triangles().GetElementIDs())
    {
        const TArrayView<const FVertexID> Corners = Description->GetTriangleVertices(Triangle);
        const FVector A(Positions[Corners[0]]), B(Positions[Corners[1]]), C(Positions[Corners[2]]);
        const double TwiceArea = FVector::CrossProduct(B - A, C - A).Size();
        Area += TwiceArea * 0.5;
        SignedVolume += FVector::DotProduct(A, FVector::CrossProduct(B, C)) / 6.0;
        Degenerate += TwiceArea < 1e-6 ? 1 : 0;
        Bounds += A;
        Bounds += B;
        Bounds += C;
        McpJoin(PartParent, Weld[Corners[0].GetValue()], Weld[Corners[1].GetValue()]);
        McpJoin(PartParent, Weld[Corners[0].GetValue()], Weld[Corners[2].GetValue()]);
    }

    // The direction a face runs one of its edges, in welded vertices.
    auto Runs = [Description, &Weld](FTriangleID Face, FVertexID From, FVertexID To) -> FMcpOpenEdge
    {
        const TArrayView<const FVertexID> Corners = Description->GetTriangleVertices(Face);
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            if (Corners[Corner] == From && Corners[(Corner + 1) % 3] == To)
            {
                return {Weld[From.GetValue()], Weld[To.GetValue()], 1};
            }
        }
        return {Weld[To.GetValue()], Weld[From.GetValue()], 1};
    };

    // Edges: three or more faces is non-manifold, and two faces that both run an edge the same way have opposite
    // windings, so one of them faces inward. An edge with a face on one side only is open, unless another open
    // edge lies on the same two places: that pair is a seam, checked for winding the same way.
    int32 NonManifold = 0, Flipped = 0;
    TMap<uint64, FMcpOpenEdge> OpenEdges;
    for (const FEdgeID Edge : Description->Edges().GetElementIDs())
    {
        const TArrayView<const FTriangleID> Faces = Description->GetEdgeConnectedTriangleIDs(Edge);
        const FVertexID From = Description->GetEdgeVertex(Edge, 0), To = Description->GetEdgeVertex(Edge, 1);
        if (Faces.Num() > 2)
        {
            ++NonManifold;
        }
        else if (Faces.Num() == 2)
        {
            const FMcpOpenEdge First = Runs(Faces[0], From, To);
            Flipped += First.From != First.To && First.From == Runs(Faces[1], From, To).From ? 1 : 0;
        }
        else if (Faces.Num() == 1)
        {
            const FMcpOpenEdge Run = Runs(Faces[0], From, To);
            FMcpOpenEdge &Seen = OpenEdges.FindOrAdd(McpEdgeKey(Run.From, Run.To));
            Flipped += Seen.Count == 1 && Seen.From == Run.From ? 1 : 0;
            Seen = {Run.From, Run.To, Seen.Count + 1};
        }
    }
    int32 Open = 0, Seams = 0;
    for (const TPair<uint64, FMcpOpenEdge> &Pair : OpenEdges)
    {
        if (Pair.Value.Count == 1)
        {
            ++Open;
            McpJoin(HoleParent, Pair.Value.From, Pair.Value.To);
        }
        else
        {
            Seams += 1;
        }
    }
    TSet<int32> Holes;
    for (const TPair<uint64, FMcpOpenEdge> &Pair : OpenEdges)
    {
        if (Pair.Value.Count == 1)
        {
            Holes.Add(McpFindRoot(HoleParent, Pair.Value.From));
        }
    }

    TMap<int32, FMcpMeshPart> Parts;
    for (const FTriangleID Triangle : Description->Triangles().GetElementIDs())
    {
        const TArrayView<const FVertexID> Corners = Description->GetTriangleVertices(Triangle);
        FMcpMeshPart &Part = Parts.FindOrAdd(McpFindRoot(PartParent, Weld[Corners[0].GetValue()]));
        Part.Triangles += 1;
        for (const FVertexID Corner : Corners)
        {
            Part.Box += FVector(Positions[Corner]);
        }
    }
    TArray<FMcpMeshPart> Sorted;
    Parts.GenerateValueArray(Sorted);
    Sorted.Sort([](const FMcpMeshPart &L, const FMcpMeshPart &R) { return L.Triangles > R.Triangles; });
    TArray<TSharedPtr<FJsonValue>> Listed;
    for (int32 Index = 0; Index < FMath::Min(Sorted.Num(), McpMaxListedParts); ++Index)
    {
        TSharedPtr<FJsonObject> Part = MakeShared<FJsonObject>();
        Part->SetNumberField(TEXT("triangles"), Sorted[Index].Triangles);
        Part->SetArrayField(TEXT("size"), McpRoundedVec(Sorted[Index].Box.GetSize()));
        Part->SetArrayField(TEXT("center"), McpRoundedVec(Sorted[Index].Box.GetCenter()));
        Listed.Add(MakeShared<FJsonValueObject>(Part));
    }

    const int32 TriangleCount = Description->Triangles().Num();
    const bool bClosed = TriangleCount > 0 && Open == 0 && NonManifold == 0;
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("meshPath"), Mesh->GetPathName());
    Result->SetNumberField(TEXT("triangles"), TriangleCount);
    Result->SetNumberField(TEXT("vertices"), WeldIds.Num());
    Result->SetArrayField(TEXT("size"), McpRoundedVec(Bounds.IsValid ? Bounds.GetSize() : FVector::ZeroVector));
    Result->SetNumberField(TEXT("surfaceArea"), FMath::RoundToDouble(Area));
    Result->SetBoolField(TEXT("closed"), bClosed);
    if (bClosed)
    {
        // Only a closed surface encloses a volume; inside out it comes out negative, so its size is what counts.
        Result->SetNumberField(TEXT("volume"), FMath::RoundToDouble(FMath::Abs(SignedVolume)));
    }
    Result->SetNumberField(TEXT("openEdges"), Open);
    Result->SetNumberField(TEXT("holes"), Holes.Num());
    Result->SetNumberField(TEXT("seamEdges"), Seams);
    Result->SetNumberField(TEXT("nonManifoldEdges"), NonManifold);
    Result->SetNumberField(TEXT("flippedEdges"), Flipped);
    Result->SetNumberField(TEXT("degenerateTriangles"), Degenerate);
    Result->SetNumberField(TEXT("parts"), Parts.Num());
    Result->SetArrayField(TEXT("largestParts"), Listed);
    McpHandlerUtils::MarkNoAssetsChanged(Result);

    FString Message = FString::Printf(TEXT("%s: %d triangles in %d part(s), %s"), *Mesh->GetName(), TriangleCount,
        Parts.Num(), bClosed ? TEXT("closed") : *FString::Printf(TEXT("open (%d open edges in %d hole(s))"), Open, Holes.Num()));
    if (NonManifold + Flipped + Degenerate > 0)
    {
        Message += FString::Printf(TEXT("; %d non-manifold edges, %d flipped edges, %d degenerate triangles"),
            NonManifold, Flipped, Degenerate);
    }
    if (Seams > 0)
    {
        Message += FString::Printf(TEXT("; its faces are split along %d seam edges (corners at one place are separate "
            "vertices): it renders whole, but booleans, remeshing and collision built from it need it welded"), Seams);
    }
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true, Message + TEXT("."), Result);
    return true;
}
} // namespace McpEnvironmentHandlers
