// Copyright (c) 2024 EngineWire Contributors
//
// create_primitive roof: a hip roof over a footprint, every face rising from its edge at one pitch. The roof's shape is
// the footprint's straight skeleton (McpRoofSkeleton); the faces, lifted by their distance from the outline times the
// pitch's tangent, go through the polygon cage path append_polygons uses, and the new mesh into a DynamicMeshActor.

#include "Domains/Geometry/McpGeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "Domains/Geometry/Roof/McpRoofSkeleton.h"

namespace McpGeometryHandlers
{
namespace
{
constexpr int32 RoofMaxCorners = 64;

bool RoofReadFootprint(const TSharedPtr<FJsonObject>& Payload, TArray<FVector2D>& Out)
{
    const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
    if (!Payload->TryGetArrayField(TEXT("footprint"), Points) || Points->Num() < 3 || Points->Num() > RoofMaxCorners)
    {
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Value : *Points)
    {
        const TSharedPtr<FJsonObject>* Point = nullptr;
        double X = 0.0, Y = 0.0;
        if (!Value.IsValid() || !Value->TryGetObject(Point) || !(*Point)->TryGetNumberField(TEXT("x"), X) || !(*Point)->TryGetNumberField(TEXT("y"), Y) ||
            !FMath::IsFinite(X) || !FMath::IsFinite(Y))
        {
            return false;
        }
        Out.Add(FVector2D(X, Y));
    }
    return true;
}

// Each face (its polygroup is its index + 1) gets normal and UV elements of its own. AppendPolygonFaces shares one normal
// per vertex, as a cage wants, which shaded every ridge, hip and eave round; per face, the recompute gives each planar
// face its own normal. UVs are in metres: u along the face's eave, rightward seen from outside, and v up its slope with
// the image's top toward the ridge, so a tile material lies in rows along every eave; the base, past the outline's
// faces, is projected from below.
void RoofFaceElements(UE::Geometry::FDynamicMesh3& Mesh, const TArray<FVector2D>& Outline, double Pitch)
{
    UE::Geometry::FDynamicMeshUVOverlay* Uvs = Mesh.HasAttributes() ? Mesh.Attributes()->PrimaryUV() : nullptr;
    UE::Geometry::FDynamicMeshNormalOverlay* Normals = Mesh.HasAttributes() ? Mesh.Attributes()->PrimaryNormals() : nullptr;
    if (!Uvs || !Normals)
    {
        return;
    }
    const double Stretch = 1.0 / FMath::Cos(FMath::DegreesToRadians(Pitch));
    TMap<int64, UE::Geometry::FIndex2i> Elements; // (face, vertex) -> {UV element, normal element}
    for (const int32 Triangle : Mesh.TriangleIndicesItr())
    {
        const int32 Face = Mesh.GetTriangleGroup(Triangle) - 1;
        const UE::Geometry::FIndex3i Corners = Mesh.GetTriangle(Triangle);
        UE::Geometry::FIndex3i Uv, Normal;
        for (int32 K = 0; K < 3; ++K)
        {
            const int64 Key = (static_cast<int64>(Face) << 32) | Corners[K];
            const UE::Geometry::FIndex2i* Found = Elements.Find(Key);
            if (!Found)
            {
                const FVector3d Point = Mesh.GetVertex(Corners[K]);
                FVector2D At(Point.X, Point.Y);
                if (Outline.IsValidIndex(Face))
                {
                    const FVector2D From = Outline[Face], Along = (Outline[(Face + 1) % Outline.Num()] - From).GetSafeNormal();
                    At = FVector2D(FVector2D::DotProduct(At - From, Along), -FVector2D::DotProduct(At - From, FVector2D(-Along.Y, Along.X)) * Stretch);
                }
                Found = &Elements.Add(Key, UE::Geometry::FIndex2i(Uvs->AppendElement(FVector2f(static_cast<float>(At.X / 100.0), static_cast<float>(At.Y / 100.0))),
                                                                  Normals->AppendElement(FVector3f(0.0f, 0.0f, 1.0f))));
            }
            Uv[K] = Found->A;
            Normal[K] = Found->B;
        }
        Uvs->SetTriangle(Triangle, Uv);
        Normals->SetTriangle(Triangle, Normal);
    }
}
} // namespace

bool HandleCreateRoof(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty())
    {
        Name = TEXT("GeneratedRoof");
    }
    TArray<FVector2D> Footprint;
    if (!RoofReadFootprint(Payload, Footprint))
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("footprint takes 3 to %d corners as {x, y} in cm, in order around the outline."), RoofMaxCorners), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const double Pitch = FMath::Clamp(GetJsonNumberField(Payload, TEXT("pitch"), 30.0), 5.0, 75.0);
    const double Overhang = FMath::Max(0.0, GetJsonNumberField(Payload, TEXT("overhang"), 0.0));
    const double Rise = FMath::Tan(FMath::DegreesToRadians(Pitch));
    McpRoofSkeleton::FResult Roof = McpRoofSkeleton::Build(Footprint);
    if (Roof.Error.IsEmpty() && Overhang > 0.0)
    {
        // The eaves: the outline moved out by the overhang, the roof planes carried down past the walls to it.
        const TArray<FVector2D> Eaves = McpRoofSkeleton::Offset(Roof.Outline, Overhang);
        Roof = McpRoofSkeleton::CrossesItself(Eaves) ? McpRoofSkeleton::FResult{{}, {}, {}, TEXT("an overhang this wide folds the outline over itself")}
                                                      : McpRoofSkeleton::Build(Eaves);
    }
    if (!Roof.Error.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No roof: %s. Nothing was made."), *Roof.Error), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FAppendPolygonsInput Input;
    double Ridge = 0.0;
    for (const McpRoofSkeleton::FNode& Node : Roof.Nodes)
    {
        Input.Vertices.Add(FVector3d(Node.Point.X, Node.Point.Y, (Node.Time - Overhang) * Rise));
        Ridge = FMath::Max(Ridge, Input.Vertices.Last().Z);
    }
    Input.Faces = Roof.Faces;
    const bool bBase = GetJsonBoolField(Payload, TEXT("base"), true);
    if (bBase)
    {
        TArray<int32>& Base = Input.Faces.AddDefaulted_GetRef(); // the underside, facing down
        for (int32 Corner = Roof.Outline.Num() - 1; Corner >= 0; --Corner)
        {
            Base.Add(Corner);
        }
    }
    for (int32 Face = 0; Face < Input.Faces.Num(); ++Face)
    {
        Input.FaceGroups.Add(Face + 1);
    }
    FString Error;
    int32 VerticesAdded = 0;
    UDynamicMesh* DynMesh = NewObject<UDynamicMesh>(GetTransientPackage());
    bool bBuilt = FinishAppendPolygonsInput(Input, Error);
    DynMesh->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh)
    {
        bBuilt = bBuilt && AppendPolygonFaces(EditMesh, Input, VerticesAdded, Error);
        if (bBuilt)
        {
            RoofFaceElements(EditMesh, Roof.Outline, Pitch);
        }
    });
    if (!bBuilt)
    {
        DynMesh->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("The roof's faces did not make a clean mesh: %s. Nothing was made."), *Error), TEXT("ROOF_FAILED"));
        return true;
    }
    RecomputeMeshNormals(DynMesh);
    bool bClosed = false;
    DynMesh->ProcessMesh([&bClosed](const UE::Geometry::FDynamicMesh3& Mesh) { bClosed = Mesh.IsClosed(); });

    TSharedPtr<FJsonObject> Result;
    AActor* NewActor = SpawnPrimitiveOrReply(Self, RequestId, Socket, ReadTransformFromPayload(Payload), Name, DynMesh, Result);
    if (!NewActor)
    {
        return true;
    }
    Result->SetNumberField(TEXT("faces"), Roof.Faces.Num());
    Result->SetNumberField(TEXT("triangles"), Input.Triangles.Num());
    Result->SetNumberField(TEXT("ridgeHeight"), FMath::RoundToDouble(Ridge * 10.0) / 10.0);
    Result->SetNumberField(TEXT("eaveHeight"), FMath::RoundToDouble(-Overhang * Rise * 10.0) / 10.0);
    Result->SetBoolField(TEXT("watertight"), bClosed);
    McpHandlerUtils::AddVerification(Result, NewActor);
    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Roof of %d faces at %g degrees, its ridge %.1f cm above the walls%s."), Roof.Faces.Num(), Pitch, Ridge,
                        bClosed ? TEXT(", closed underneath") : TEXT("")), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
