// McpAutomationBridge_GeometryConversionSlots.cpp — what convert_to_static_mesh and convert_to_nanite give the asset
// they bake: its material slots, from the mesh's material ids and the optional `materials` list, and its collision.
//
// Geometry Script turns every material id into a section of the static mesh and makes max(id) + 1 slots, all
// empty but the first. `materials` fills them: entry i is the material of slot i, so of every triangle with
// material id i; an empty entry leaves that slot on the default material.
#include "Domains/Geometry/McpAutomationBridge_GeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"

namespace McpGeometryHandlers
{
int32 ConversionSlotCount(UDynamicMesh* Mesh)
{
    int32 HighestId = 0;
    Mesh->ProcessMesh([&HighestId](const UE::Geometry::FDynamicMesh3& Source)
    {
        if (Source.HasAttributes() && Source.Attributes()->HasMaterialID())
        {
            const UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIds = Source.Attributes()->GetMaterialID();
            for (const int32 TriangleId : Source.TriangleIndicesItr())
            {
                HighestId = FMath::Max(HighestId, MaterialIds->GetValue(TriangleId));
            }
        }
    });
    return HighestId + 1;
}

bool LoadConversionMaterials(const TSharedPtr<FJsonObject>& Payload, int32 SlotCount, TArray<UMaterialInterface*>& OutMaterials, FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!Payload->TryGetArrayField(TEXT("materials"), Entries) || Entries->Num() == 0)
    {
        return true;
    }
    if (Entries->Num() > SlotCount)
    {
        OutError = FString::Printf(
            TEXT("materials lists %d entries, but the mesh only uses material ids 0 to %d, and entry i is the material of id i. "
                 "Give the triangles their ids with edit_dynamic_mesh set_material_id first, or list fewer materials."),
            Entries->Num(), SlotCount - 1);
        return false;
    }
    for (int32 Slot = 0; Slot < Entries->Num(); ++Slot)
    {
        FString Path;
        if (!(*Entries)[Slot].IsValid() || !(*Entries)[Slot]->TryGetString(Path))
        {
            OutError = FString::Printf(TEXT("materials[%d] must be an asset path string (an empty string keeps the default material)"), Slot);
            return false;
        }
        if (Path.IsEmpty())
        {
            OutMaterials.Add(nullptr);
            continue;
        }
        const FString SafePath = SanitizeProjectRelativePath(Path);
        if (SafePath.IsEmpty())
        {
            OutError = FString::Printf(TEXT("materials[%d]: %s"), Slot, *McpPathRefusalMessage(TEXT("materials"), Path));
            return false;
        }
        UMaterialInterface* Material = Cast<UMaterialInterface>(McpLoadAsset(SafePath));
        if (!Material)
        {
            // McpLoadAsset reads the asset registry; a path it has not scanned yet (some engine content) loads straight from disk.
            Material = LoadObject<UMaterialInterface>(nullptr, *SafePath);
        }
        if (!Material && !SafePath.Contains(TEXT(".")))
        {
            Material = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s.%s"), *SafePath, *FPaths::GetBaseFilename(SafePath)));
        }
        if (!Material)
        {
            OutError = FString::Printf(TEXT("materials[%d]: %s does not load as a material or a material instance"), Slot, *Path);
            return false;
        }
        OutMaterials.Add(Material);
    }
    return true;
}

void ApplyConversionMaterials(UStaticMesh* Mesh, const TArray<UMaterialInterface*>& Materials, int32 SlotCount)
{
    TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
    Mesh->Modify();
    // A slot that is added or given a material is a change the asset has to be told about; a name is only a label.
    bool bChanged = Slots.Num() < SlotCount;
    while (Slots.Num() < SlotCount)
    {
        Slots.Add(FStaticMaterial());
    }
    for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
    {
        if (Materials.IsValidIndex(Slot) && Materials[Slot])
        {
            Slots[Slot].MaterialInterface = Materials[Slot];
            bChanged = true;
        }
        // The sections reference slots by index, so naming an unnamed slot changes nothing but what the editor shows.
        if (Slots[Slot].MaterialSlotName.IsNone())
        {
            Slots[Slot].MaterialSlotName = FName(*FString::Printf(TEXT("Material_%d"), Slot));
#if WITH_EDITORONLY_DATA
            Slots[Slot].ImportedMaterialSlotName = Slots[Slot].MaterialSlotName;
#endif
        }
    }
    if (bChanged)
    {
        Mesh->PostEditChange();
    }
}

void ApplyConversionCollision(UStaticMesh* Mesh, const FString& Mode)
{
    // A freshly created StaticMesh asset has NO collision body, so pawns fell straight through any level geometry built
    // from converted meshes even though the asset itself rendered fine. Give the asset a body (see Mode) and cook it
    // synchronously, so the converted mesh is standable in PIE without a separate round-trip.
    //
    // Built from explicit convex-hull vertices in body space via the long-stable UBodySetup/FKAggregateGeom API
    // rather than version-drifting Geometry Script static-mesh collision helpers.
    UBodySetup* BodySetup = Mesh->GetBodySetup();
    if (!BodySetup)
    {
        BodySetup = NewObject<UBodySetup>(Mesh, NAME_None, RF_Transactional);
        Mesh->SetBodySetup(BodySetup);
    }
    BodySetup->AggGeom.EmptyElements();
    if (Mode == TEXT("complex"))
    {
        // The render triangles are the collision: no shapes, and the complex mesh answers simple queries too.
        BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
    }
    else if (Mode == TEXT("none"))
    {
        BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex; // no shapes to stand in for the complex mesh either
    }
    else
    {
        // box: the bounds as one convex hull, exact for the box primitives and a tight approximation for round ones.
        const FBox Bounds = Mesh->GetBounds().GetBox();
        BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
        FKConvexElem ConvexElem;
        ConvexElem.VertexData.Reset(8);
        for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
        {
            ConvexElem.VertexData.Add(FVector(
                (CornerIndex & 1) ? Bounds.Max.X : Bounds.Min.X,
                (CornerIndex & 2) ? Bounds.Max.Y : Bounds.Min.Y,
                (CornerIndex & 4) ? Bounds.Max.Z : Bounds.Min.Z));
        }
        ConvexElem.UpdateElemBox();
        BodySetup->AggGeom.ConvexElems.Add(ConvexElem);
    }
    BodySetup->CreatePhysicsMeshes();
    Mesh->MarkPackageDirty();
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
