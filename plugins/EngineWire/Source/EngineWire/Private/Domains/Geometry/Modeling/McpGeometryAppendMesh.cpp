// McpGeometryAppendMesh.cpp — append_mesh: copies of a static mesh asset laid into a dynamic mesh in one call.
//
// A part already modelled in detail (a corrugated container, a window frame, a chair, a bolt) is copied where each
// placement puts it, repeated on up to three axes as append_cuboids repeats a box, and each of its material slots
// takes the target material id the placement maps it to; a list of ids cycles copy by copy, so a stack of one part
// comes out in mixed colours. The copies keep the asset's own normals and UVs. A model assembled from detailed parts
// took one actor per part before, and could not be baked into one asset.
#include "Domains/Geometry/McpGeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "DynamicMeshEditor.h"
#include "TransformTypes.h"

namespace McpGeometryHandlers
{
namespace
{
constexpr int32 MaxMeshPlacements = 500;
constexpr int32 MaxMeshCopies = 2500;

struct FMeshPlacement
{
    FTransform Transform;
    FMcpRepeatGrid Grid;
    TArray<TArray<int32>> SlotIds; // per source slot, the ids its copies take in turn; empty keeps the slot's own index
};

// materialIds: one entry per material slot of the part, an id or a list of ids that copies take in turn.
bool ReadSlotIds(const TSharedPtr<FJsonObject>& Placement, int32 Slots, TArray<TArray<int32>>& Out)
{
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!Placement->HasField(TEXT("materialIds"))) return true;
    if (!Placement->TryGetArrayField(TEXT("materialIds"), Entries) || Entries->Num() != Slots) return false;
    for (const TSharedPtr<FJsonValue>& Entry : *Entries)
    {
        const TArray<TSharedPtr<FJsonValue>> Values = Entry.IsValid() && Entry->Type == EJson::Array ? Entry->AsArray() : TArray<TSharedPtr<FJsonValue>>{Entry};
        TArray<int32>& Ids = Out.AddDefaulted_GetRef();
        for (const TSharedPtr<FJsonValue>& Value : Values)
        {
            double Number = -1.0;
            if (!Value.IsValid() || !Value->TryGetNumber(Number) || Number != FMath::FloorToDouble(Number) || Number < 0 || Number > MAX_MATERIAL_ID) return false;
            Ids.Add(static_cast<int32>(Number));
        }
        if (Ids.Num() == 0 || Ids.Num() > 256) return false;
    }
    return true;
}
} // namespace

bool HandleAppendMesh(UEngineWireSubsystem* Self, const FString& RequestId,
                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString MeshPath = GetJsonStringField(Payload, TEXT("meshPath"));
    const TArray<TSharedPtr<FJsonValue>>* Placements = nullptr;
    if (MeshPath.IsEmpty() || !Payload->TryGetArrayField(TEXT("placements"), Placements) || Placements->Num() == 0 || Placements->Num() > MaxMeshPlacements)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(
            TEXT("meshPath and placements (1 to %d) required: the static mesh to copy and where each copy goes"), MaxMeshPlacements), TEXT("INVALID_MESH_PLACEMENT"));
        return true;
    }
    const TOptional<FMcpGeometryTarget> Target = ResolveGeometryTarget(Self, RequestId, ActorName, Socket);
    if (!Target) return true;
    auto [TargetActor, DMC, Mesh] = *Target;

    UStaticMesh* Source = Cast<UStaticMesh>(McpLoadAsset(SanitizeProjectRelativePath(MeshPath)));
    UDynamicMesh* Part = NewObject<UDynamicMesh>(GetTransientPackage());
    EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
    // lod render copies the mesh as it draws: for a Nanite mesh its fallback, often a twentieth of the source triangles.
    const bool bRender = GetJsonStringField(Payload, TEXT("lod")) == TEXT("render");
    FGeometryScriptMeshReadLOD ReadLOD;
    ReadLOD.LODType = bRender ? EGeometryScriptLODType::RenderData : EGeometryScriptLODType::MaxAvailable;
    if (Source)
    {
        UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(Source, Part, FGeometryScriptCopyMeshFromAssetOptions(), ReadLOD, Outcome);
    }
    if (Outcome != EGeometryScriptOutcomePins::Success)
    {
        Part->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, Source ? FString::Printf(TEXT("Could not read geometry from %s"), *MeshPath)
                                                            : FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath),
                                  Source ? TEXT("MESH_READ_FAILED") : TEXT("ASSET_NOT_FOUND"));
        return true;
    }
    const int32 Slots = FMath::Max(1, Source->GetStaticMaterials().Num());

    TArray<FMeshPlacement> Parsed;
    int64 Copies = 0;
    for (int32 Index = 0; Index < Placements->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Placement = nullptr;
        FMeshPlacement& Out = Parsed.AddDefaulted_GetRef();
        if (!(*Placements)[Index]->TryGetObject(Placement) || !Placement->IsValid() || !ReadRepeatGrid(*Placement, Out.Grid)
            || !ReadSlotIds(*Placement, Slots, Out.SlotIds))
        {
            Part->MarkAsGarbage();
            Self->SendAutomationError(Socket, RequestId, FString::Printf(
                TEXT("placements[%d]: materialIds takes one entry per material slot of %s (%d), each an id 0 to %d or a list of them, and repeat count, rows and layers 1 to 1000"),
                Index, *MeshPath, Slots, MAX_MATERIAL_ID), TEXT("INVALID_MESH_PLACEMENT"));
            return true;
        }
        Out.Transform = ReadTransformFromPayload(*Placement);
        Copies += Out.Grid.Num();
    }
    const int32 PartTriangles = Part->GetTriangleCount();
    if (Copies > MaxMeshCopies)
    {
        Part->MarkAsGarbage();
        Self->SendAutomationError(Socket, RequestId, FString::Printf(
            TEXT("placements come to %lld copies once repeated, more than %d; append them in several calls"), Copies, MaxMeshCopies), TEXT("INVALID_MESH_PLACEMENT"));
        return true;
    }
    const int64 Estimated = static_cast<int64>(Mesh->GetTriangleCount()) + PartTriangles * Copies;
    if (Estimated > MAX_TRIANGLES_PER_DYNAMIC_MESH)
    {
        Part->MarkAsGarbage();
        const TCHAR* Remedy = bRender ? TEXT("append fewer copies")
                                      : TEXT("lod \"render\" copies the mesh as it draws (a Nanite mesh's much lighter fallback), or append fewer copies");
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%lld copies of %d triangles would bring the mesh to ~%lld triangles (max %d): %s"),
            Copies, PartTriangles, Estimated, MAX_TRIANGLES_PER_DYNAMIC_MESH, Remedy), TEXT("POLYGON_LIMIT_EXCEEDED"));
        return true;
    }
    if (!GuardMeshBudget(Self, RequestId, Socket, Estimated, TEXT("Append mesh")))
    {
        Part->MarkAsGarbage();
        return true;
    }

    Part->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& PartMesh)
    {
        const UE::Geometry::FDynamicMeshMaterialAttribute* PartIds = PartMesh.HasAttributes() ? PartMesh.Attributes()->GetMaterialID() : nullptr;
        Mesh->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh)
        {
            EditMesh.EnableAttributes();
            if (!EditMesh.Attributes()->HasMaterialID())
            {
                EditMesh.Attributes()->EnableMaterialID();
            }
            UE::Geometry::FDynamicMeshMaterialAttribute* Ids = EditMesh.Attributes()->GetMaterialID();
            UE::Geometry::FDynamicMeshEditor Editor(&EditMesh);
            for (const FMeshPlacement& Placement : Parsed)
            {
                for (int32 Copy = 0; Copy < static_cast<int32>(Placement.Grid.Num()); ++Copy)
                {
                    FTransform Transform = Placement.Transform;
                    Transform.AddToTranslation(FVector(Placement.Grid.At(Copy)));
                    const UE::Geometry::FTransformSRT3d XForm(Transform);
                    const bool bMirrored = Transform.GetDeterminant() < 0.0;
                    UE::Geometry::FMeshIndexMappings Mappings;
                    Editor.AppendMesh(&PartMesh, Mappings, [&XForm](int, const FVector3d& Position) { return XForm.TransformPosition(Position); },
                                      [&XForm](int, const FVector3d& Normal) { return XForm.TransformNormal(Normal); });
                    for (const int32 PartTriangle : PartMesh.TriangleIndicesItr())
                    {
                        const int32 Added = Mappings.GetNewTriangle(PartTriangle);
                        if (!EditMesh.IsTriangle(Added)) continue;
                        // A mirroring scale turns the copy inside out unless its triangles turn back.
                        if (bMirrored) EditMesh.ReverseTriOrientation(Added);
                        const int32 Slot = PartIds ? FMath::Clamp(PartIds->GetValue(PartTriangle), 0, Slots - 1) : 0;
                        Ids->SetValue(Added, Placement.SlotIds.Num() > 0 ? Placement.SlotIds[Slot][Copy % Placement.SlotIds[Slot].Num()] : Slot);
                    }
                }
            }
        });
    });
    Part->MarkAsGarbage();
    DMC->NotifyMeshUpdated();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("meshPath"), MeshPath);
    Result->SetNumberField(TEXT("copies"), static_cast<double>(Copies));
    Result->SetNumberField(TEXT("sourceSlots"), Slots);
    Result->SetNumberField(TEXT("trianglesAdded"), static_cast<double>(PartTriangles * Copies));
    Result->SetNumberField(TEXT("vertexCount"), Mesh->GetMeshRef().VertexCount());
    Result->SetNumberField(TEXT("triangleCount"), Mesh->GetTriangleCount());
    Result->SetArrayField(TEXT("materialIdsInUse"), MaterialIdsInUseJson(Mesh));
    McpHandlerUtils::AddVerification(Result, TargetActor);
    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Appended %lld copies of %s (%lld triangles)"), Copies, *MeshPath, PartTriangles * Copies), Result);
    return true;
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
