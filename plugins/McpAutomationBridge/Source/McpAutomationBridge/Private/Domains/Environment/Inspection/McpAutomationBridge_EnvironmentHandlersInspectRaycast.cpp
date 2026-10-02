#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "StaticMeshAttributes.h"

namespace McpEnvironmentHandlers {

// Rays against a static mesh's source triangles in the mesh's own space: where a surface is, which
// way it faces and which material slot it belongs to, so a part, a print or a decal can be placed on
// a curved surface without a placed actor or collision. The source, not the render data, because a
// Nanite mesh renders from a coarse fallback (the placement audit reads it the same way).
bool HandleInspectRaycastMeshAction(
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
    const TArray<TSharedPtr<FJsonValue>> *Rays = nullptr;
    if (!Payload->TryGetArrayField(TEXT("rays"), Rays) || !Rays || Rays->Num() == 0 || Rays->Num() > 256)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("rays is required: 1-256 entries of {origin, direction} in the mesh's local space."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UE::Geometry::FDynamicMesh3 Shape;
    FMeshDescriptionToDynamicMesh Converter;
    Converter.Convert(Description, Shape);
    const UE::Geometry::FDynamicMeshAABBTree3 Tree(&Shape, true);
    const UE::Geometry::FDynamicMeshMaterialAttribute *MaterialIds =
        Shape.HasAttributes() && Shape.Attributes()->HasMaterialID() ? Shape.Attributes()->GetMaterialID() : nullptr;
    const FStaticMeshConstAttributes Attributes(*Description);
    const TPolygonGroupAttributesConstRef<FName> SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();

    TArray<TSharedPtr<FJsonValue>> Hits;
    int32 HitCount = 0;
    for (const TSharedPtr<FJsonValue> &Value : *Rays)
    {
        const TSharedPtr<FJsonObject> *Ray = nullptr;
        const bool bRay = Value.IsValid() && Value->TryGetObject(Ray) && Ray;
        const FVector Origin = bRay ? ExtractVectorField(*Ray, TEXT("origin"), FVector::ZeroVector) : FVector::ZeroVector;
        const FVector Direction = bRay ? ExtractVectorField(*Ray, TEXT("direction"), FVector::ZeroVector).GetSafeNormal() : FVector::ZeroVector;
        double Distance = 0.0;
        int TriangleId = -1;
        const bool bHit = !Direction.IsZero() && Tree.FindNearestHitTriangle(FRay3d(Origin, Direction), Distance, TriangleId);
        const TSharedPtr<FJsonObject> Hit = MakeShared<FJsonObject>();
        Hit->SetBoolField(TEXT("hit"), bHit);
        if (bHit)
        {
            ++HitCount;
            FVector Normal = Shape.GetTriNormal(TriangleId);
            Normal = FVector::DotProduct(Normal, Direction) > 0.0 ? -Normal : Normal;
            Hit->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Origin + Direction * Distance));
            Hit->SetObjectField(TEXT("normal"), McpHandlerUtils::VectorToJson(Normal));
            Hit->SetNumberField(TEXT("distance"), Distance);
            // A decal printed here reads upright from outside: X projects into the surface, Y is the
            // texture's down (toward -Z, or -X on a surface facing straight up or down), Z its reading
            // direction. Found by trial on a chest print: the identity rotation lays text sideways and mirrored.
            const FVector Up = FMath::Abs(Normal.Z) > 0.99 ? FVector::ForwardVector : FVector::UpVector;
            const FVector Down = -(Up - Normal * FVector::DotProduct(Up, Normal)).GetSafeNormal();
            Hit->SetObjectField(TEXT("decalRotation"), McpHandlerUtils::RotatorToJson(FRotationMatrix::MakeFromXY(-Normal, Down).Rotator()));
            // An SDF or GeometryScript mesh leaves the polygon group slot names empty; the mesh's own
            // slot of that index then names it.
            const int32 GroupIndex = MaterialIds ? MaterialIds->GetValue(TriangleId) : 0;
            const FPolygonGroupID Group(GroupIndex);
            FName Slot = Description->IsPolygonGroupValid(Group) ? SlotNames[Group] : NAME_None;
            if (Slot.IsNone() && Mesh->GetStaticMaterials().IsValidIndex(GroupIndex))
            {
                Slot = Mesh->GetStaticMaterials()[GroupIndex].MaterialSlotName;
            }
            if (!Slot.IsNone())
            {
                Hit->SetStringField(TEXT("materialSlot"), Slot.ToString());
            }
        }
        Hits.Add(MakeShared<FJsonValueObject>(Hit));
    }

    const TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("meshPath"), Mesh->GetPathName());
    Resp->SetNumberField(TEXT("hitCount"), HitCount);
    Resp->SetArrayField(TEXT("hits"), Hits);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("%d of %d rays met the surface"), HitCount, Hits.Num()), Resp, FString());
    return true;
}

} // namespace McpEnvironmentHandlers
