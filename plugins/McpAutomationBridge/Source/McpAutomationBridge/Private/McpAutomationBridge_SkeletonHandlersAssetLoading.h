#pragma once

#include "CoreMinimal.h"

class UPhysicsAsset;
class USkeletalMesh;
class USkeleton;

namespace McpSkeletonHandlers
{
USkeleton* LoadSkeletonFromPathSkel(const FString& SkeletonPath, FString& OutError);
USkeletalMesh* LoadSkeletalMeshFromPathSkel(const FString& MeshPath, FString& OutError);
UPhysicsAsset* LoadPhysicsAssetFromPath(const FString& PhysicsPath, FString& OutError);
}
