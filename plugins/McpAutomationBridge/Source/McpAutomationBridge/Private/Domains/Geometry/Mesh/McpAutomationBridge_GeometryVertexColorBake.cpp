// McpAutomationBridge_GeometryVertexColorBake.cpp — bake_vertex_colors: AO, edge, cavity and height masks into vertex colours.
//
// Every channel is a mask in 0..1, white where it has no effect, so a mesh that was never baked and a mesh whose
// masks are all quiet read the same in a material's VertexColor node:
//   R  ambient occlusion: 1 open, 0 fully occluded (hemisphere rays against the mesh's own AABB tree)
//   G  1 - convex edge: 1 on flat or concave surface, lower along an outward fold such as a rim or corner
//   B  1 - concave cavity: 1 on flat or convex surface, lower in a crease or crevice
//   A  height: 0 at the bottom of the bounds, 1 at the top, along the mesh's local Z
// The colours go into the mesh's primary colour overlay, the one the static-mesh conversion reads. Geometry
// Script converts with bTransformVtxColorsSRGBToLinear on (UE 5.1 and later), which cancels the sRGB encoding
// the static-mesh build applies, so the byte a vertex ends up with is the mask times 255.
#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "Async/ParallelFor.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"

namespace McpGeometryHandlers
{
static constexpr int32 MinAoRays = 8;
static constexpr int32 MaxAoRays = 256;
static constexpr int32 DefaultAoRays = 32;
static constexpr int32 MaxBakeBlurIterations = 16;
// Vertices times rays one bake may trace; past that the rays are cut (never below the minimum) to keep the call short.
static constexpr int64 MaxAoRayTests = 16000000;
static constexpr double BakePi = 3.14159265358979323846;

// Area-weighted vertex normals, indexed by vertex id; a vertex no triangle uses stays zero.
static void BakeVertexNormals(const UE::Geometry::FDynamicMesh3& Mesh, TArray<FVector3d>& OutNormals)
{
    OutNormals.Init(FVector3d(0.0, 0.0, 0.0), Mesh.MaxVertexID());
    for (const int32 TriangleId : Mesh.TriangleIndicesItr())
    {
        const UE::Geometry::FIndex3i Triangle = Mesh.GetTriangle(TriangleId);
        const FVector3d Weighted = Mesh.GetTriNormal(TriangleId) * Mesh.GetTriArea(TriangleId);
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            OutNormals[Triangle[Corner]] += Weighted;
        }
    }
    for (const int32 VertexId : Mesh.VertexIndicesItr())
    {
        OutNormals[VertexId] = OutNormals[VertexId].GetSafeNormal(1.0e-20);
    }
}

// Open-ness at every vertex: the share of Rays cosine-weighted hemisphere rays, started just off the surface and
// no longer than Distance, that hit nothing. The ray directions are a fixed spiral, so a bake repeats exactly.
static void BakeAmbientOcclusion(const UE::Geometry::FDynamicMesh3& Mesh, const TArray<FVector3d>& Normals, int32 Rays, double Distance,
                                 TArray<float>& OutOpenness)
{
    OutOpenness.Init(1.0f, Mesh.MaxVertexID());
    TArray<FVector3d> Directions;
    Directions.Reserve(Rays);
    const double GoldenAngle = BakePi * (3.0 - FMath::Sqrt(5.0));
    for (int32 Index = 0; Index < Rays; ++Index)
    {
        const double U = (Index + 0.5) / Rays;
        Directions.Add(FVector3d(FMath::Sqrt(U) * FMath::Cos(Index * GoldenAngle), FMath::Sqrt(U) * FMath::Sin(Index * GoldenAngle), FMath::Sqrt(1.0 - U)));
    }
    TArray<int32> VertexIds;
    VertexIds.Reserve(Mesh.VertexCount());
    for (const int32 VertexId : Mesh.VertexIndicesItr())
    {
        VertexIds.Add(VertexId);
    }
    UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh);
    const double Bias = FMath::Max(Distance * 1.0e-3, 1.0e-3);
    ParallelFor(VertexIds.Num(), [&](int32 Index)
    {
        const int32 VertexId = VertexIds[Index];
        const FVector3d Normal = Normals[VertexId];
        if (Normal.SquaredLength() < 0.5)
        {
            return; // a vertex no triangle uses has no side to look from
        }
        FVector3d AxisX;
        FVector3d AxisY;
        Normal.FindBestAxisVectors(AxisX, AxisY);
        const FVector3d Origin = Mesh.GetVertex(VertexId) + Normal * Bias;
        UE::Geometry::FDynamicMeshAABBTree3::FQueryOptions Options;
        Options.MaxDistance = Distance;
        int32 Blocked = 0;
        for (const FVector3d& Local : Directions)
        {
            if (Tree.TestAnyHitTriangle(FRay3d(Origin, AxisX * Local.X + AxisY * Local.Y + Normal * Local.Z), Options))
            {
                ++Blocked;
            }
        }
        OutOpenness[VertexId] = 1.0f - static_cast<float>(Blocked) / static_cast<float>(Rays);
    });
}

// The sharpest fold at every vertex among its edges, in radians: OutConvex where the surface bends away from
// its outward side (a rim), OutConcave where it bends toward it (a crease). A fold is the angle between the two
// triangle normals of an edge, and it is convex when the far corner of the second triangle sits behind the first.
static void BakeFolds(const UE::Geometry::FDynamicMesh3& Mesh, TArray<float>& OutConvex, TArray<float>& OutConcave)
{
    OutConvex.Init(0.0f, Mesh.MaxVertexID());
    OutConcave.Init(0.0f, Mesh.MaxVertexID());
    for (const int32 EdgeId : Mesh.EdgeIndicesItr())
    {
        const UE::Geometry::FIndex2i Triangles = Mesh.GetEdgeT(EdgeId);
        if (Triangles.B == UE::Geometry::FDynamicMesh3::InvalidID)
        {
            continue; // a boundary edge has no fold
        }
        const FVector3d NormalA = Mesh.GetTriNormal(Triangles.A);
        const FVector3d NormalB = Mesh.GetTriNormal(Triangles.B);
        if (NormalA.SquaredLength() < 0.5 || NormalB.SquaredLength() < 0.5)
        {
            continue; // a degenerate triangle has no normal to fold against
        }
        const double Angle = FMath::Acos(FMath::Clamp(NormalA.Dot(NormalB), -1.0, 1.0));
        const UE::Geometry::FIndex2i Ends = Mesh.GetEdgeV(EdgeId);
        const UE::Geometry::FIndex2i Far = Mesh.GetEdgeOpposingV(EdgeId);
        const bool bConvex = NormalA.Dot(Mesh.GetVertex(Far.B) - Mesh.GetVertex(Ends.A)) < 0.0;
        TArray<float>& Folds = bConvex ? OutConvex : OutConcave;
        Folds[Ends.A] = FMath::Max(Folds[Ends.A], static_cast<float>(Angle));
        Folds[Ends.B] = FMath::Max(Folds[Ends.B], static_cast<float>(Angle));
    }
}

// Averages every vertex with its one-ring neighbours, Iterations times, over the three masks stored as a vector.
static void BakeBlurMasks(const UE::Geometry::FDynamicMesh3& Mesh, int32 Iterations, TArray<FVector3f>& InOutMasks)
{
    TArray<FVector3f> Next = InOutMasks;
    for (int32 Pass = 0; Pass < Iterations; ++Pass)
    {
        for (const int32 VertexId : Mesh.VertexIndicesItr())
        {
            FVector3f Sum = InOutMasks[VertexId];
            int32 Count = 1;
            for (const int32 Neighbour : Mesh.VtxVerticesItr(VertexId))
            {
                Sum += InOutMasks[Neighbour];
                ++Count;
            }
            Next[VertexId] = Sum / static_cast<float>(Count);
        }
        Swap(InOutMasks, Next);
    }
}

bool HandleBakeVertexColors(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 Rays = FMath::Clamp(GetJsonIntField(Payload, TEXT("aoRays"), DefaultAoRays), MinAoRays, MaxAoRays);
    const int32 Blur = FMath::Clamp(GetJsonIntField(Payload, TEXT("blurIterations"), 1), 0, MaxBakeBlurIterations);
    const double CurvatureScale = FMath::Max(0.0, GetJsonNumberField(Payload, TEXT("curvatureScale"), 1.0));
    if (Payload->HasField(TEXT("aoDistance")) && !(GetJsonNumberField(Payload, TEXT("aoDistance"), 0.0) > 0.0))
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("aoDistance must be a positive number of centimetres."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    if (Target->Mesh->GetTriangleCount() == 0)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("The mesh has no triangles to bake."), TEXT("MESH_EMPTY"));
        return true;
    }
    if (!GuardMeshBudget(Self, RequestId, Socket, 0, TEXT("Bake vertex colors"))) return true;

    TArray<FVector4f> Colors;
    double Distance = 0.0;
    int32 VertexCount = 0;
    FVector4f Mean(0.0f, 0.0f, 0.0f, 0.0f);
    Target->Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Source)
    {
        const UE::Geometry::FAxisAlignedBox3d Bounds = Source.GetBounds();
        VertexCount = Source.VertexCount();
        Distance = Payload->HasField(TEXT("aoDistance")) ? GetJsonNumberField(Payload, TEXT("aoDistance"), 0.0) : (Bounds.Max - Bounds.Min).Length() * 0.15;
        Rays = static_cast<int32>(FMath::Clamp<int64>(MaxAoRayTests / FMath::Max(1, VertexCount), MinAoRays, Rays));

        TArray<FVector3d> Normals;
        TArray<float> Openness;
        TArray<float> Convex;
        TArray<float> Concave;
        BakeVertexNormals(Source, Normals);
        BakeAmbientOcclusion(Source, Normals, Rays, Distance, Openness);
        BakeFolds(Source, Convex, Concave);

        // A fold of a quarter turn is a full edge; curvatureScale makes shallower folds read as stronger or weaker.
        const double PerRadian = CurvatureScale * 2.0 / BakePi;
        TArray<FVector3f> Masks;
        Masks.Init(FVector3f(1.0f, 1.0f, 1.0f), Source.MaxVertexID());
        for (const int32 VertexId : Source.VertexIndicesItr())
        {
            Masks[VertexId] = FVector3f(Openness[VertexId],
                                        1.0f - FMath::Clamp(static_cast<float>(Convex[VertexId] * PerRadian), 0.0f, 1.0f),
                                        1.0f - FMath::Clamp(static_cast<float>(Concave[VertexId] * PerRadian), 0.0f, 1.0f));
        }
        BakeBlurMasks(Source, Blur, Masks);

        const double Bottom = Bounds.Min.Z;
        const double Span = Bounds.Max.Z - Bounds.Min.Z;
        Colors.Init(FVector4f(1.0f, 1.0f, 1.0f, 1.0f), Source.MaxVertexID());
        for (const int32 VertexId : Source.VertexIndicesItr())
        {
            const float Height = Span > 1.0e-6 ? static_cast<float>(FMath::Clamp((Source.GetVertex(VertexId).Z - Bottom) / Span, 0.0, 1.0)) : 0.0f;
            Colors[VertexId] = FVector4f(Masks[VertexId].X, Masks[VertexId].Y, Masks[VertexId].Z, Height);
            Mean += Colors[VertexId];
        }
    });
    Mean /= static_cast<float>(FMath::Max(1, VertexCount));

    Target->Mesh->EditMesh([&Colors](UE::Geometry::FDynamicMesh3& EditMesh)
    {
        EditMesh.EnableAttributes();
        if (!EditMesh.Attributes()->HasPrimaryColors())
        {
            EditMesh.Attributes()->EnablePrimaryColors();
        }
        UE::Geometry::FDynamicMeshColorOverlay* Overlay = EditMesh.Attributes()->PrimaryColors();
        Overlay->ClearElements();
        TArray<int32> ElementIds;
        ElementIds.Init(INDEX_NONE, EditMesh.MaxVertexID());
        for (const int32 VertexId : EditMesh.VertexIndicesItr())
        {
            ElementIds[VertexId] = Overlay->AppendElement(Colors[VertexId]);
        }
        for (const int32 TriangleId : EditMesh.TriangleIndicesItr())
        {
            const UE::Geometry::FIndex3i Triangle = EditMesh.GetTriangle(TriangleId);
            Overlay->SetTriangle(TriangleId, UE::Geometry::FIndex3i(ElementIds[Triangle.A], ElementIds[Triangle.B], ElementIds[Triangle.C]));
        }
    });
    Target->Component->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Averages = McpHandlerUtils::CreateResultObject();
    Averages->SetNumberField(TEXT("r"), Mean.X);
    Averages->SetNumberField(TEXT("g"), Mean.Y);
    Averages->SetNumberField(TEXT("b"), Mean.Z);
    Averages->SetNumberField(TEXT("a"), Mean.W);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetNumberField(TEXT("vertexCount"), VertexCount);
    Result->SetNumberField(TEXT("aoRays"), Rays);
    Result->SetNumberField(TEXT("aoDistance"), Distance);
    Result->SetNumberField(TEXT("curvatureScale"), CurvatureScale);
    Result->SetNumberField(TEXT("blurIterations"), Blur);
    Result->SetObjectField(TEXT("averages"), Averages);
    Result->SetStringField(TEXT("channels"), TEXT("r ambient occlusion (1 open), g 1 minus convex edge, b 1 minus concave cavity, a height (0 bottom, 1 top); white means no effect except in height"));
    McpHandlerUtils::AddVerification(Result, Target->Actor);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vertex colours baked"), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
