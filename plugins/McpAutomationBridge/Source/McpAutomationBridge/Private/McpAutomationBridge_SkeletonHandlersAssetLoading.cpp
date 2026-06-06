#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "McpAutomationBridgeHelpersProjectPaths.h"
#include "PhysicsEngine/PhysicsAsset.h"

namespace McpSkeletonHandlers
{
USkeleton* LoadSkeletonFromPathSkel(const FString& SkeletonPath, FString& OutError)
{
    OutError.Reset();
    if (SkeletonPath.IsEmpty())
    {
        OutError = TEXT("Skeleton path is required");
        return nullptr;
    }

    const FString SanitizedPath = SanitizeProjectRelativePath(SkeletonPath);
    if (SanitizedPath.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Invalid skeleton path '%s': contains traversal sequences"), *SkeletonPath);
        return nullptr;
    }

    UObject* Asset = StaticLoadObject(USkeleton::StaticClass(), nullptr, *SanitizedPath);
    if (!Asset)
    {
        OutError = FString::Printf(TEXT("Failed to load skeleton: %s"), *SkeletonPath);
        return nullptr;
    }

    USkeleton* Skeleton = Cast<USkeleton>(Asset);
    if (!Skeleton)
    {
        OutError = FString::Printf(TEXT("Asset is not a skeleton: %s"), *SkeletonPath);
    }
    return Skeleton;
}

USkeletalMesh* LoadSkeletalMeshFromPathSkel(const FString& MeshPath, FString& OutError)
{
    OutError.Reset();
    if (MeshPath.IsEmpty())
    {
        OutError = TEXT("Skeletal mesh path is required");
        return nullptr;
    }

    const FString SanitizedPath = SanitizeProjectRelativePath(MeshPath);
    if (SanitizedPath.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Invalid skeletal mesh path '%s': contains traversal sequences"), *MeshPath);
        return nullptr;
    }

    UObject* Asset = StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *SanitizedPath);
    if (!Asset)
    {
        OutError = FString::Printf(TEXT("Failed to load skeletal mesh: %s"), *MeshPath);
        return nullptr;
    }

    USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset);
    if (!Mesh)
    {
        OutError = FString::Printf(TEXT("Asset is not a skeletal mesh: %s"), *MeshPath);
    }
    return Mesh;
}

UPhysicsAsset* LoadPhysicsAssetFromPath(const FString& PhysicsPath, FString& OutError)
{
    OutError.Reset();
    if (PhysicsPath.IsEmpty())
    {
        OutError = TEXT("Physics asset path is required");
        return nullptr;
    }

    const FString SanitizedPath = SanitizeProjectRelativePath(PhysicsPath);
    if (SanitizedPath.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Invalid physics asset path '%s': contains traversal sequences"), *PhysicsPath);
        return nullptr;
    }

    UObject* Asset = StaticLoadObject(UPhysicsAsset::StaticClass(), nullptr, *SanitizedPath);
    if (!Asset)
    {
        OutError = FString::Printf(TEXT("Failed to load physics asset: %s"), *PhysicsPath);
        return nullptr;
    }

    UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(Asset);
    if (!PhysicsAsset)
    {
        OutError = FString::Printf(TEXT("Asset is not a physics asset: %s"), *PhysicsPath);
    }
    return PhysicsAsset;
}
}
