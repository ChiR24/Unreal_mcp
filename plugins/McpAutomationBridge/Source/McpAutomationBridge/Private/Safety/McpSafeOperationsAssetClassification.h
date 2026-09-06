#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "UObject/SoftObjectPath.h"
#endif

namespace McpSafeOperations
{

#if WITH_EDITOR

inline bool IsAnyBlueprintAsset(const FAssetData& AssetData)
{
    FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);
    return ClassName.Contains(TEXT("Blueprint")) ||
           ClassName.Contains(TEXT("WidgetBlueprint")) ||
           ClassName.Contains(TEXT("ControlRigBlueprint"));
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

inline bool IsMixedAnimationRigCluster(const TArray<FAssetData>& Assets)
{
    bool bHasIKRigDefinition = false;
    bool bHasAnimSequence = false;
    bool bHasAnimBlueprint = false;
    bool bHasControlRigBlueprint = false;

    for (const FAssetData& AssetData : Assets)
    {
        const int32 Priority = GetAnimationRigClusterDeletePriority(AssetData);
        switch (Priority)
        {
        case 0:
            bHasAnimBlueprint = true;
            break;
        case 1:
            bHasIKRigDefinition = true;
            break;
        case 2:
            bHasAnimSequence = true;
            break;
        case 3:
            bHasControlRigBlueprint = true;
            break;
        default:
            break;
        }
    }

    const int32 ClusterTypeCount =
        (bHasIKRigDefinition ? 1 : 0) +
        (bHasAnimSequence ? 1 : 0) +
        (bHasAnimBlueprint ? 1 : 0) +
        (bHasControlRigBlueprint ? 1 : 0);

    return ClusterTypeCount >= 2;
}

inline bool IsWorldAsset(const FAssetData& AssetData)
{
    FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);
    return ClassName.Equals(TEXT("/Script/Engine.World"), ESearchCase::IgnoreCase) ||
           ClassName.EndsWith(TEXT(".World"), ESearchCase::IgnoreCase);
}

#endif

}
