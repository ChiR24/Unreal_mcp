// McpAutomationBridge_GeometryRegion.cpp — the `region` filter that picks triangles by where they are and what they carry.
//
// The face operators (extrude, inset, outset, offset_faces, bevel, chamfer, poke) and set_material_id take
// either explicit triangleIndices or a region: a box the triangle centroids lie in, a direction the triangles
// face, polygroup ids and material ids. Every filter given must hold, so they AND together. A box and a
// direction are in the mesh's own local space, the space get_vertex_position and append_polygons use.
#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
// A {x, y, z} object field of Object; false when it is missing, not an object, or not three finite numbers.
static bool ReadRegionVector(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, FVector3d& OutVector)
{
    const TSharedPtr<FJsonObject>* Vector = nullptr;
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    if (!Object->TryGetObjectField(Name, Vector) || !Vector->IsValid()
        || !(*Vector)->TryGetNumberField(TEXT("x"), X) || !(*Vector)->TryGetNumberField(TEXT("y"), Y)
        || !(*Vector)->TryGetNumberField(TEXT("z"), Z) || !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z))
    {
        return false;
    }
    OutVector = FVector3d(X, Y, Z);
    return true;
}

// An integer-list field of Object into OutIds; false when an entry is not a whole number or the list is empty.
static bool ReadRegionIdSet(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, TSet<int32>& OutIds)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object->TryGetArrayField(Name, Values) || Values->Num() == 0)
    {
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        double Number = 0.0;
        if (!Value.IsValid() || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number) || Number != FMath::FloorToDouble(Number)
            || Number < -1.0e9 || Number > 1.0e9)
        {
            return false;
        }
        OutIds.Add(static_cast<int32>(Number));
    }
    return true;
}

bool ResolveRegionTriangles(UDynamicMesh* Mesh, const TSharedPtr<FJsonObject>& Region, TArray<int32>& OutTriangles, FString& OutError, FString& OutCode)
{
    OutCode = TEXT("INVALID_REGION");
    bool bBox = false;
    FVector3d BoxLow;
    FVector3d BoxHigh;
    const TSharedPtr<FJsonObject>* BoxObject = nullptr;
    if (Region->TryGetObjectField(TEXT("box"), BoxObject) && BoxObject->IsValid())
    {
        FVector3d Corner0;
        FVector3d Corner1;
        if (!ReadRegionVector(*BoxObject, TEXT("min"), Corner0) || !ReadRegionVector(*BoxObject, TEXT("max"), Corner1))
        {
            OutError = TEXT("region.box needs min and max, each an {x, y, z} of finite numbers");
            return false;
        }
        // A box given with its corners swapped still means the box between them.
        BoxLow = FVector3d(FMath::Min(Corner0.X, Corner1.X), FMath::Min(Corner0.Y, Corner1.Y), FMath::Min(Corner0.Z, Corner1.Z));
        BoxHigh = FVector3d(FMath::Max(Corner0.X, Corner1.X), FMath::Max(Corner0.Y, Corner1.Y), FMath::Max(Corner0.Z, Corner1.Z));
        bBox = true;
    }

    bool bNormal = false;
    FVector3d Normal(0.0, 0.0, 1.0);
    double MinDot = 0.0;
    if (Region->HasField(TEXT("normal")))
    {
        if (!ReadRegionVector(Region, TEXT("normal"), Normal) || Normal.SquaredLength() < 1.0e-12)
        {
            OutError = TEXT("region.normal needs a non-zero {x, y, z} direction");
            return false;
        }
        Normal.Normalize();
        MinDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(GetJsonNumberField(Region, TEXT("normalAngle"), 30.0), 0.0, 180.0)));
        bNormal = true;
    }

    TSet<int32> Groups;
    const bool bGroups = Region->HasField(TEXT("groupIds"));
    if (bGroups && !ReadRegionIdSet(Region, TEXT("groupIds"), Groups))
    {
        OutError = TEXT("region.groupIds must be a non-empty list of whole-number polygroup ids");
        return false;
    }
    TSet<int32> Materials;
    const bool bMaterials = Region->HasField(TEXT("materialIds"));
    if (bMaterials && !ReadRegionIdSet(Region, TEXT("materialIds"), Materials))
    {
        OutError = TEXT("region.materialIds must be a non-empty list of whole-number material ids");
        return false;
    }
    if (!bBox && !bNormal && !bGroups && !bMaterials)
    {
        OutError = TEXT("region needs at least one of box, normal, groupIds or materialIds");
        return false;
    }

    UE::Geometry::FAxisAlignedBox3d Bounds;
    int32 TriangleCount = 0;
    Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& ReadMesh)
    {
        const bool bHasGroups = ReadMesh.HasTriangleGroups();
        const UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIds =
            (ReadMesh.HasAttributes() && ReadMesh.Attributes()->HasMaterialID()) ? ReadMesh.Attributes()->GetMaterialID() : nullptr;
        for (const int32 TriangleId : ReadMesh.TriangleIndicesItr())
        {
            if (bBox)
            {
                const FVector3d Centroid = ReadMesh.GetTriCentroid(TriangleId);
                if (Centroid.X < BoxLow.X || Centroid.X > BoxHigh.X || Centroid.Y < BoxLow.Y || Centroid.Y > BoxHigh.Y
                    || Centroid.Z < BoxLow.Z || Centroid.Z > BoxHigh.Z)
                {
                    continue;
                }
            }
            // A mesh with no polygroups is one group, 0; one with no material ids is all material 0.
            if (bNormal && ReadMesh.GetTriNormal(TriangleId).Dot(Normal) < MinDot) continue;
            if (bGroups && !Groups.Contains(bHasGroups ? ReadMesh.GetTriangleGroup(TriangleId) : 0)) continue;
            if (bMaterials && !Materials.Contains(MaterialIds ? MaterialIds->GetValue(TriangleId) : 0)) continue;
            OutTriangles.Add(TriangleId);
        }
        Bounds = ReadMesh.GetBounds();
        TriangleCount = ReadMesh.TriangleCount();
    });

    if (OutTriangles.Num() == 0)
    {
        OutCode = TEXT("REGION_EMPTY");
        OutError = FString::Printf(
            TEXT("region matched none of the mesh's %d triangles. The mesh's local bounds are min (%.1f, %.1f, %.1f) max (%.1f, %.1f, %.1f); "
                 "a box and a normal are in this local space, not world space, and every filter given must hold."),
            TriangleCount, Bounds.Min.X, Bounds.Min.Y, Bounds.Min.Z, Bounds.Max.X, Bounds.Max.Y, Bounds.Max.Z);
        return false;
    }
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
