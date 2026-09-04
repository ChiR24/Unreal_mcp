#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h"
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

USkeletalMesh* FindSkeletalMeshForSkeleton(USkeleton* Skeleton)
{
    if (!Skeleton)
    {
        return nullptr;
    }
    if (USkeletalMesh* PreviewMesh = Skeleton->GetPreviewMesh(false))
    {
        return PreviewMesh;
    }
#if WITH_EDITORONLY_DATA
    return Skeleton->FindCompatibleMesh();
#else
    return nullptr;
#endif
}

FSkeletonMeshTarget ResolveSkeletonMeshTarget(const TSharedPtr<FJsonObject>& Payload)
{
    FSkeletonMeshTarget Target;
    FString MeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    if (MeshPath.IsEmpty())
    {
        MeshPath = GetJsonStringField(Payload, TEXT("meshPath"));
    }
    const FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));

    if (!MeshPath.IsEmpty())
    {
        Target.SourcePath = MeshPath;
        Target.Mesh = LoadSkeletalMeshFromPathSkel(MeshPath, Target.Error);
        if (!Target.Mesh)
        {
            Target.ErrorCode = TEXT("MESH_NOT_FOUND");
            return Target;
        }
        Target.Skeleton = Target.Mesh->GetSkeleton();
        return Target;
    }

    if (SkeletonPath.IsEmpty())
    {
        Target.Error = TEXT("skeletalMeshPath (or skeletonPath) is required");
        Target.ErrorCode = TEXT("MISSING_PARAM");
        return Target;
    }

    Target.SourcePath = SkeletonPath;
    Target.Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Target.Error);
    if (Target.Skeleton)
    {
        Target.Mesh = FindSkeletalMeshForSkeleton(Target.Skeleton);
        return Target;
    }

    // Older callers passed a skeletal mesh through skeletonPath; honour that
    // before reporting the skeleton as missing.
    FString MeshError;
    Target.Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, MeshError);
    if (!Target.Mesh)
    {
        Target.ErrorCode = TEXT("SKELETON_NOT_FOUND");
        return Target;
    }
    Target.Error.Reset();
    Target.Skeleton = Target.Mesh->GetSkeleton();
    return Target;
}
}
