#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
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

TSharedPtr<FJsonValue> McpMakeMaterialSlotEntry(int32 Index, const FName &SlotName, const UMaterialInterface *Material)
{
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetNumberField(TEXT("slotIndex"), Index);
    Entry->SetStringField(TEXT("slotName"), SlotName.ToString());
    Entry->SetStringField(TEXT("material"), Material ? Material->GetPathName() : TEXT(""));
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
    const TArray<FStaticMaterial> &Materials = Mesh->GetStaticMaterials();
    for (int32 Index = 0; Index < Materials.Num(); ++Index)
    {
        Slots.Add(McpMakeMaterialSlotEntry(Index, Materials[Index].MaterialSlotName, Materials[Index].MaterialInterface));
    }
    Resp->SetArrayField(TEXT("materialSlots"), Slots);
    Resp->SetNumberField(TEXT("materialSlotCount"), Slots.Num());
    Resp->SetObjectField(TEXT("bounds"), McpMakeBoundsObject(Mesh->GetBounds().GetBox()));
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
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
    const TArray<FSkeletalMaterial> &Materials = Mesh->GetMaterials();
    for (int32 Index = 0; Index < Materials.Num(); ++Index)
    {
        Slots.Add(McpMakeMaterialSlotEntry(Index, Materials[Index].MaterialSlotName, Materials[Index].MaterialInterface));
    }
    Resp->SetArrayField(TEXT("materialSlots"), Slots);
    Resp->SetNumberField(TEXT("materialSlotCount"), Slots.Num());
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
#endif
