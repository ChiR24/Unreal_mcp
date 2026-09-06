#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UPhysicsAsset;
class USkeletalMesh;
class USkeleton;

namespace McpSkeletonHandlers
{
USkeleton* LoadSkeletonFromPathSkel(const FString& SkeletonPath, FString& OutError);
USkeletalMesh* LoadSkeletalMeshFromPathSkel(const FString& MeshPath, FString& OutError);
// The skeleton at Path, or the skeleton of the skeletal mesh at Path.
USkeleton* LoadSkeletonOrMeshSkeleton(const FString& Path, FString& OutError);
UPhysicsAsset* LoadPhysicsAssetFromPath(const FString& PhysicsPath, FString& OutError);

// The skeletal mesh a skeleton-addressed request can read or write: the
// skeleton's preview mesh, else the first compatible mesh the asset registry
// knows. Null when no mesh uses the skeleton.
USkeletalMesh* FindSkeletalMeshForSkeleton(USkeleton* Skeleton);

// The mesh/skeleton pair a bone request addresses. Mesh may be null when only
// a skeleton without any mesh was named; Error/ErrorCode are set when nothing
// loaded at all.
struct FSkeletonMeshTarget
{
    USkeletalMesh* Mesh = nullptr;
    USkeleton* Skeleton = nullptr;
    FString SourcePath;
    FString Error;
    FString ErrorCode;

    bool IsValid() const { return Error.IsEmpty(); }
};

// Resolves skeletalMeshPath (alias meshPath) or skeletonPath. A skeletonPath
// may name a USkeleton (Mesh = preview/compatible mesh) or, for older callers,
// a USkeletalMesh.
FSkeletonMeshTarget ResolveSkeletonMeshTarget(const TSharedPtr<FJsonObject>& Payload);
}
