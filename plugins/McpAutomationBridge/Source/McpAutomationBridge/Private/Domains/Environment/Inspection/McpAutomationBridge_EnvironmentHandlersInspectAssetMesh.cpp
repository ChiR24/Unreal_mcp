#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#include "Animation/Skeleton.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "StaticMeshResources.h"

namespace McpEnvironmentHandlers {

namespace {
TSharedPtr<FJsonValue> McpMakeLodEntry(int32 Index, int32 Triangles, int32 Vertices, int32 Sections)
{
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetNumberField(TEXT("lod"), Index);
    Entry->SetNumberField(TEXT("triangles"), Triangles);
    Entry->SetNumberField(TEXT("vertices"), Vertices);
    Entry->SetNumberField(TEXT("sections"), Sections);
    return MakeShared<FJsonValueObject>(Entry);
}

// What one material slot draws in LOD0: its triangles, how many render sections carry them, and where they are in mesh space.
struct FSlotGeometry
{
    int32 Triangles = 0;
    int32 Sections = 0;
    FBox Bounds = FBox(ForceInit);
};

void McpAddSlotPoint(FSlotGeometry &Slot, const FVector3f &Point)
{
    Slot.Bounds += FVector(Point.X, Point.Y, Point.Z);
}

// Static mesh: every LOD0 section draws the slot its MaterialIndex names; its bounds come from the vertices its triangles use.
TArray<FSlotGeometry> McpCollectStaticSlotGeometry(UStaticMesh *Mesh, bool &bOutHasBounds)
{
    TArray<FSlotGeometry> Slots;
    Slots.SetNum(Mesh->GetStaticMaterials().Num());
    bOutHasBounds = false;
    const FStaticMeshRenderData *RenderData = Mesh->GetRenderData();
    if (!RenderData || RenderData->LODResources.Num() == 0)
    {
        return Slots;
    }
    const FStaticMeshLODResources &Lod = RenderData->LODResources[0];
    const FPositionVertexBuffer &Positions = Lod.VertexBuffers.PositionVertexBuffer;
    const FRawStaticIndexBuffer &Indices = Lod.IndexBuffer;
    // The CPU copy of the buffers is kept in the editor; without it only the triangle counts can be read.
    bOutHasBounds = Positions.GetVertexData() != nullptr && Positions.GetNumVertices() > 0 && Indices.GetNumIndices() > 0;
    for (const FStaticMeshSection &Section : Lod.Sections)
    {
        if (!Slots.IsValidIndex(Section.MaterialIndex))
        {
            continue;
        }
        FSlotGeometry &Slot = Slots[Section.MaterialIndex];
        Slot.Triangles += static_cast<int32>(Section.NumTriangles);
        ++Slot.Sections;
        if (!bOutHasBounds)
        {
            continue;
        }
        const uint32 End = Section.FirstIndex + Section.NumTriangles * 3;
        for (uint32 At = Section.FirstIndex; At < End && At < static_cast<uint32>(Indices.GetNumIndices()); ++At)
        {
            const uint32 Vertex = Indices.GetIndex(At);
            if (Vertex < Positions.GetNumVertices())
            {
                McpAddSlotPoint(Slot, Positions.VertexPosition(Vertex));
            }
        }
    }
    return Slots;
}

// Skeletal mesh: a LOD0 render section owns the vertex range BaseVertexIndex..+NumVertices, in the reference pose.
TArray<FSlotGeometry> McpCollectSkeletalSlotGeometry(USkeletalMesh *Mesh, bool &bOutHasBounds)
{
    TArray<FSlotGeometry> Slots;
    Slots.SetNum(Mesh->GetMaterials().Num());
    bOutHasBounds = false;
    const FSkeletalMeshRenderData *RenderData = Mesh->GetResourceForRendering();
    if (!RenderData || RenderData->LODRenderData.Num() == 0)
    {
        return Slots;
    }
    const FSkeletalMeshLODRenderData &Lod = RenderData->LODRenderData[0];
    const FPositionVertexBuffer &Positions = Lod.StaticVertexBuffers.PositionVertexBuffer;
    bOutHasBounds = Positions.GetVertexData() != nullptr && Positions.GetNumVertices() > 0;
    for (const FSkelMeshRenderSection &Section : Lod.RenderSections)
    {
        if (!Slots.IsValidIndex(Section.MaterialIndex))
        {
            continue;
        }
        FSlotGeometry &Slot = Slots[Section.MaterialIndex];
        Slot.Triangles += static_cast<int32>(Section.NumTriangles);
        ++Slot.Sections;
        for (uint32 Offset = 0; bOutHasBounds && Offset < Section.NumVertices; ++Offset)
        {
            const uint32 Vertex = Section.BaseVertexIndex + Offset;
            if (Vertex < Positions.GetNumVertices())
            {
                McpAddSlotPoint(Slot, Positions.VertexPosition(Vertex));
            }
        }
    }
    return Slots;
}

TSharedPtr<FJsonValue> McpMakeMaterialSlotEntry(int32 Index, const FName &SlotName, const UMaterialInterface *Material,
                                                const FSlotGeometry &Geometry)
{
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetNumberField(TEXT("slotIndex"), Index);
    Entry->SetStringField(TEXT("slotName"), SlotName.ToString());
    Entry->SetStringField(TEXT("material"), Material ? Material->GetPathName() : TEXT(""));
    Entry->SetNumberField(TEXT("triangles"), Geometry.Triangles);
    Entry->SetNumberField(TEXT("sections"), Geometry.Sections);
    if (Geometry.Bounds.IsValid != 0)
    {
        Entry->SetObjectField(TEXT("bounds"), McpMakeBoundsObject(Geometry.Bounds));
    }
    return MakeShared<FJsonValueObject>(Entry);
}

void McpDescribeStaticMesh(UStaticMesh *Mesh, TSharedPtr<FJsonObject> Resp)
{
    Resp->SetStringField(TEXT("assetType"), TEXT("StaticMesh"));
    Resp->SetNumberField(TEXT("lodCount"), Mesh->GetNumLODs());
    TArray<TSharedPtr<FJsonValue>> Lods;
    int32 Lod0Triangles = 0;
    if (const FStaticMeshRenderData *RenderData = Mesh->GetRenderData())
    {
        for (int32 Index = 0; Index < RenderData->LODResources.Num(); ++Index)
        {
            const FStaticMeshLODResources &Lod = RenderData->LODResources[Index];
            Lod0Triangles = Index == 0 ? Lod.GetNumTriangles() : Lod0Triangles;
            Lods.Add(McpMakeLodEntry(Index, Lod.GetNumTriangles(), Lod.GetNumVertices(), Lod.Sections.Num()));
        }
    }
    Resp->SetArrayField(TEXT("lods"), Lods);
    Resp->SetNumberField(TEXT("triangleCount"), Lod0Triangles);
    TArray<TSharedPtr<FJsonValue>> Slots;
    bool bHasSlotBounds = false;
    const TArray<FSlotGeometry> Geometry = McpCollectStaticSlotGeometry(Mesh, bHasSlotBounds);
    const TArray<FStaticMaterial> &Materials = Mesh->GetStaticMaterials();
    for (int32 Index = 0; Index < Materials.Num(); ++Index)
    {
        Slots.Add(McpMakeMaterialSlotEntry(Index, Materials[Index].MaterialSlotName, Materials[Index].MaterialInterface, Geometry[Index]));
    }
    Resp->SetArrayField(TEXT("materialSlots"), Slots);
    Resp->SetNumberField(TEXT("materialSlotCount"), Slots.Num());
    Resp->SetBoolField(TEXT("slotBoundsAvailable"), bHasSlotBounds);
    Resp->SetObjectField(TEXT("bounds"), McpMakeBoundsObject(Mesh->GetBounds().GetBox()));
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    Resp->SetBoolField(TEXT("naniteEnabled"), Mesh->IsNaniteEnabled());
#else
    Resp->SetBoolField(TEXT("naniteEnabled"), Mesh->NaniteSettings.bEnabled);
#endif
    const UBodySetup *BodySetup = Mesh->GetBodySetup();
    Resp->SetNumberField(TEXT("collisionPrimitiveCount"), BodySetup ? BodySetup->AggGeom.GetElementCount() : 0);
    Resp->SetStringField(TEXT("collisionComplexity"), BodySetup
        ? StaticEnum<ECollisionTraceFlag>()->GetNameStringByValue(static_cast<int64>(BodySetup->CollisionTraceFlag.GetValue()))
        : FString());
    Resp->SetNumberField(TEXT("socketCount"), Mesh->Sockets.Num());
}

void McpDescribeSkeletalMesh(USkeletalMesh *Mesh, TSharedPtr<FJsonObject> Resp)
{
    Resp->SetStringField(TEXT("assetType"), TEXT("SkeletalMesh"));
    Resp->SetNumberField(TEXT("lodCount"), Mesh->GetLODNum());
    TArray<TSharedPtr<FJsonValue>> Lods;
    if (const FSkeletalMeshRenderData *RenderData = Mesh->GetResourceForRendering())
    {
        for (int32 Index = 0; Index < RenderData->LODRenderData.Num(); ++Index)
        {
            const FSkeletalMeshLODRenderData &Lod = RenderData->LODRenderData[Index];
            Lods.Add(McpMakeLodEntry(Index, Lod.GetTotalFaces(), static_cast<int32>(Lod.GetNumVertices()), Lod.RenderSections.Num()));
        }
    }
    Resp->SetArrayField(TEXT("lods"), Lods);
    TArray<TSharedPtr<FJsonValue>> Slots;
    bool bHasSlotBounds = false;
    const TArray<FSlotGeometry> Geometry = McpCollectSkeletalSlotGeometry(Mesh, bHasSlotBounds);
    const TArray<FSkeletalMaterial> &Materials = Mesh->GetMaterials();
    for (int32 Index = 0; Index < Materials.Num(); ++Index)
    {
        Slots.Add(McpMakeMaterialSlotEntry(Index, Materials[Index].MaterialSlotName, Materials[Index].MaterialInterface, Geometry[Index]));
    }
    Resp->SetArrayField(TEXT("materialSlots"), Slots);
    Resp->SetNumberField(TEXT("materialSlotCount"), Slots.Num());
    Resp->SetBoolField(TEXT("slotBoundsAvailable"), bHasSlotBounds);
    Resp->SetObjectField(TEXT("bounds"), McpMakeBoundsObject(Mesh->GetBounds().GetBox()));
    Resp->SetBoolField(TEXT("naniteEnabled"), false);
    const USkeleton *Skeleton = Mesh->GetSkeleton();
    Resp->SetStringField(TEXT("skeleton"), Skeleton ? Skeleton->GetPathName() : TEXT(""));
    const UPhysicsAsset *PhysicsAsset = Mesh->GetPhysicsAsset();
    Resp->SetStringField(TEXT("physicsAsset"), PhysicsAsset ? PhysicsAsset->GetPathName() : TEXT(""));
    Resp->SetNumberField(TEXT("collisionPrimitiveCount"), PhysicsAsset ? PhysicsAsset->SkeletalBodySetups.Num() : 0);
}

void McpDescribeBlueprintAsset(UBlueprint *Blueprint, TSharedPtr<FJsonObject> Resp)
{
    UClass *Parent = Blueprint->ParentClass;
    Resp->SetStringField(TEXT("assetType"), TEXT("Blueprint"));
    Resp->SetStringField(TEXT("parentClass"), Parent ? Parent->GetName() : TEXT("None"));
    Resp->SetStringField(TEXT("parentClassPath"), Parent ? Parent->GetPathName() : TEXT(""));
    Resp->SetStringField(TEXT("generatedClass"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : TEXT(""));
    Resp->SetStringField(TEXT("blueprintType"), StaticEnum<EBlueprintType>()->GetNameStringByValue(
        static_cast<int64>(Blueprint->BlueprintType.GetValue())));
    const TArray<TSharedPtr<FJsonValue>> Variables = McpCollectBlueprintVariables(Blueprint);
    const TArray<TSharedPtr<FJsonValue>> Components = McpCollectBlueprintComponents(Blueprint);
    Resp->SetArrayField(TEXT("variables"), Variables);
    Resp->SetNumberField(TEXT("variableCount"), Variables.Num());
    Resp->SetArrayField(TEXT("components"), Components);
    Resp->SetNumberField(TEXT("componentCount"), Components.Num());
}
} // namespace

// Asset-type specific facts for inspect_object and its get_material_details /
// get_mesh_details / get_texture_details / get_blueprint_details aliases.
// A no-op for objects that are none of these.
void McpDescribeAssetDetails(UObject *Object, TSharedPtr<FJsonObject> Resp)
{
    if (!Object || !Resp.IsValid())
    {
        return;
    }
    if (UMaterialInterface *Material = Cast<UMaterialInterface>(Object))
    {
        McpDescribeMaterialAsset(Material, Resp);
    }
    else if (UStaticMesh *StaticMesh = Cast<UStaticMesh>(Object))
    {
        McpDescribeStaticMesh(StaticMesh, Resp);
    }
    else if (USkeletalMesh *SkeletalMesh = Cast<USkeletalMesh>(Object))
    {
        McpDescribeSkeletalMesh(SkeletalMesh, Resp);
    }
    else if (UTexture *Texture = Cast<UTexture>(Object))
    {
        McpDescribeTextureAsset(Texture, Resp);
    }
    else if (UBlueprint *Blueprint = Cast<UBlueprint>(Object))
    {
        McpDescribeBlueprintAsset(Blueprint, Resp);
    }
}

} // namespace McpEnvironmentHandlers
