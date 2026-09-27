#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "CoreMinimal.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "UObject/SoftObjectPath.h"

namespace McpSafeOperations
{


inline bool IsAnyBlueprintAsset(const FAssetData& AssetData)
{
    return MCP_ASSET_DATA_GET_CLASS_PATH(AssetData).Contains(TEXT("Blueprint"));
}

inline bool IsRiskyAnimationAsset(const FAssetData& AssetData)
{
    FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);

    static const TArray<FString> RiskyAnimationClasses = {
        TEXT("AnimBlueprint"),
        TEXT("AnimSequence"),
        TEXT("AnimMontage"),
        TEXT("AnimComposite"),
        TEXT("IKRigDefinition"),
        TEXT("IKRetargeter"),
        TEXT("ControlRigBlueprint"),
        TEXT("AimOffsetBlendSpace"),
        TEXT("BlendSpace"),
        TEXT("BlendSpace1D"),
        TEXT("BlendSpaceBase"),
        TEXT("PoseAsset"),
        TEXT("Skeleton")
    };

    for (const FString& RiskyClass : RiskyAnimationClasses)
    {
        if (ClassName.Contains(RiskyClass))
        {
            return true;
        }
    }

    return false;
}

inline int32 GetAnimationRigClusterDeletePriority(const FAssetData& AssetData)
{
    const FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);

    if (ClassName.Contains(TEXT("AnimBlueprint")))
    {
        return 0;
    }
    if (ClassName.Contains(TEXT("IKRigDefinition")))
    {
        return 1;
    }
    if (ClassName.Contains(TEXT("AnimSequence")))
    {
        return 2;
    }
    if (ClassName.Contains(TEXT("ControlRigBlueprint")))
    {
        return 3;
    }

    return 4;
}

// Whether Assets holds at least two of the cluster types (AnimBlueprint, IKRigDefinition, AnimSequence,
// ControlRigBlueprint), which have to be deleted in priority order.
inline bool IsMixedAnimationRigCluster(const TArray<FAssetData>& Assets)
{
    TSet<int32> ClusterTypes;
    for (const FAssetData& AssetData : Assets)
    {
        const int32 Priority = GetAnimationRigClusterDeletePriority(AssetData);
        if (Priority < 4)
        {
            ClusterTypes.Add(Priority);
        }
    }
    return ClusterTypes.Num() >= 2;
}

inline bool IsWorldAsset(const FAssetData& AssetData)
{
    FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);
    return ClassName.Equals(TEXT("/Script/Engine.World"), ESearchCase::IgnoreCase) ||
           ClassName.EndsWith(TEXT(".World"), ESearchCase::IgnoreCase);
}


}
