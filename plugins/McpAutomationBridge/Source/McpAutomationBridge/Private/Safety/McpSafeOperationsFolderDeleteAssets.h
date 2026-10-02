#pragma once

#include "Safety/McpSafeOperationsAnimationDelete.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Safety/McpSafeOperationsAssetDelete.h"
#include "Safety/McpSafeOperationsMapLoad.h"
#include "Safety/McpSafeOperationsWorldDelete.h"

namespace McpSafeOperations
{

/** What a folder delete is doing and how far through the folder it is (0-100), for the caller to pass on. */
using FMcpDeleteProgress = TFunction<void(const FString& Message, float Percent)>;

namespace FolderDeleteInternal
{

inline void PartitionWorldAssets(
    const TArray<FAssetData>& AllAssets,
    TArray<FAssetData>& WorldAssets,
    TArray<FAssetData>& OtherAssets)
{
    for (const FAssetData& AssetData : AllAssets)
    {
        if (IsWorldAsset(AssetData))
        {
            WorldAssets.Add(AssetData);
            UE_LOG(LogMcpSafeOperations, Log, TEXT(" World asset: %s (%s)"),
                *AssetData.AssetName.ToString(), *MCP_ASSET_DATA_GET_CLASS_PATH(AssetData));
        }
        else
        {
            OtherAssets.Add(AssetData);
        }
    }

    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: %d world assets, %d other assets"),
        WorldAssets.Num(), OtherAssets.Num());
}

inline bool SwitchAwayFromFolderWorldsIfNeeded(const FString& FolderPath, const TArray<FAssetData>& WorldAssets)
{
    if (WorldAssets.Num() == 0)
    {
        return true;
    }

    bool bCurrentWorldInFolder = false;
    FString CurrentWorldPath;
    if (GEditor)
    {
        if (UWorld* CurrentEditorWorld = GEditor->GetEditorWorldContext().World())
        {
            CurrentWorldPath = CurrentEditorWorld->GetOutermost()->GetName();
            bCurrentWorldInFolder = CurrentWorldPath.StartsWith(FolderPath, ESearchCase::IgnoreCase);
        }
    }

    const bool bTargetWorldLoaded = HasLoadedWorlds(WorldAssets);
    if (!bCurrentWorldInFolder)
    {
        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeDeleteFolder: Current world is outside '%s'; skipping safe-world switch (currentWorld=%s loadedTargetWorlds=%d)"),
            *FolderPath,
            CurrentWorldPath.IsEmpty() ? TEXT("<none>") : *CurrentWorldPath,
            bTargetWorldLoaded ? 1 : 0);
        return true;
    }

    UE_LOG(LogMcpSafeOperations, Warning,
        TEXT("McpSafeDeleteFolder: Folder contains %d world assets; switching to a transient editor world for safety (currentWorld=%s loadedTargetWorlds=%d inFolder=%d)"),
        WorldAssets.Num(),
        CurrentWorldPath.IsEmpty() ? TEXT("<none>") : *CurrentWorldPath,
        bTargetWorldLoaded ? 1 : 0,
        bCurrentWorldInFolder ? 1 : 0);

    UWorld* SafeWorld = GEditor ? GEditor->NewMap(false) : nullptr;
    if (!SafeWorld)
    {
        UE_LOG(LogMcpSafeOperations, Error,
            TEXT("McpSafeDeleteFolder: Failed to create a transient editor world before deleting world assets"));
        return false;
    }

    McpSafePostDeleteGC();

    UWorld* CurrentEditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const FString SafeWorldPackage = CurrentEditorWorld
        ? CurrentEditorWorld->GetOutermost()->GetName()
        : FString();
    if (!CurrentEditorWorld || SafeWorldPackage.StartsWith(FolderPath, ESearchCase::IgnoreCase))
    {
        UE_LOG(LogMcpSafeOperations, Error,
            TEXT("McpSafeDeleteFolder: Transient world switch did not leave target folder (currentWorld=%s targetFolder=%s)"),
            SafeWorldPackage.IsEmpty() ? TEXT("<none>") : *SafeWorldPackage,
            *FolderPath);
        return false;
    }

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpSafeDeleteFolder: Switched to transient world '%s', target worlds should be unloaded"),
        *SafeWorldPackage);
    return true;
}

inline void PartitionRiskyAssets(
    const TArray<FAssetData>& OtherAssets,
    TArray<FAssetData>& RiskyAnimationAssets,
    TArray<FAssetData>& SafeAssets)
{
    for (const FAssetData& AssetData : OtherAssets)
    {
        if (IsRiskyAnimationAsset(AssetData) || IsAnyBlueprintAsset(AssetData))
        {
            RiskyAnimationAssets.Add(AssetData);
            UE_LOG(LogMcpSafeOperations, Log, TEXT(" Risky special-delete asset: %s (%s)"),
                *AssetData.AssetName.ToString(), *MCP_ASSET_DATA_GET_CLASS_PATH(AssetData));
        }
        else
        {
            SafeAssets.Add(AssetData);
        }
    }
}

inline bool DeleteRiskySpecialAssets(
    const FString& FolderPath,
    const TArray<FAssetData>& RiskyAnimationAssets)
{
    if (RiskyAnimationAssets.Num() == 0)
    {
        return true;
    }

    TArray<FAssetData> OrderedClusterAssets;
    TArray<FAssetData> GenericRiskyAssets;

    const bool bHasMixedCluster = IsMixedAnimationRigCluster(RiskyAnimationAssets);
    for (const FAssetData& AssetData : RiskyAnimationAssets)
    {
        const int32 Priority = GetAnimationRigClusterDeletePriority(AssetData);
        if (bHasMixedCluster && Priority < 4)
        {
            OrderedClusterAssets.Add(AssetData);
        }
        else
        {
            GenericRiskyAssets.Add(AssetData);
        }
    }

    int32 DeletedRisky = 0;
    if (OrderedClusterAssets.Num() > 0)
    {
        UE_LOG(LogMcpSafeOperations, Warning,
            TEXT("McpSafeDeleteFolder: Mixed animation/rig cluster detected; deleting %d cluster assets in explicit order"),
            OrderedClusterAssets.Num());
        const int32 OrderedClusterDeleted = DeleteAnimationRigClusterOrdered(OrderedClusterAssets);
        if (OrderedClusterDeleted == INDEX_NONE)
        {
            UE_LOG(LogMcpSafeOperations, Error,
                TEXT("McpSafeDeleteFolder: Failed to delete AnimBlueprint portion of mixed animation/rig cluster in '%s'"),
                *FolderPath);
            return false;
        }
        DeletedRisky += OrderedClusterDeleted;
    }

    const int32 TotalRisky = GenericRiskyAssets.Num();
    if (TotalRisky > 0)
    {
        UE_LOG(LogMcpSafeOperations, Warning,
            TEXT("McpSafeDeleteFolder: Deleting %d remaining risky special-delete assets via ordered engine-owned deletion"),
            TotalRisky);

        const int32 GenericDeleted = DeleteAnimationRigClusterOrdered(GenericRiskyAssets);
        DeletedRisky += GenericDeleted;

        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeDeleteFolder: Deleted %d/%d remaining risky special-delete assets via ordered engine-owned deletion"),
            GenericDeleted, TotalRisky);
    }

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpSafeDeleteFolder: Deleted %d total risky special-delete assets"), DeletedRisky);
    return true;
}

inline bool DeleteSafeAssets(const TArray<FAssetData>& SafeAssets, const FMcpDeleteProgress& Progress)
{
    if (SafeAssets.Num() == 0)
    {
        return true;
    }

    // Engine passes of up to a twentieth of the set each: a delete per asset ran a referencer scan, a slow task
    // and a garbage collection each (a 391-asset import took minutes), while one pass for thousands of assets
    // said nothing for a quarter of an hour. Each pass reports how far the delete has got.
    const int32 PassSize = FMath::Max(100, SafeAssets.Num() / 20 + 1);
    int32 DeletedByEngine = 0;
    for (int32 First = 0; First < SafeAssets.Num(); First += PassSize)
    {
        const int32 Last = FMath::Min(First + PassSize, SafeAssets.Num());
        Progress(FString::Printf(TEXT("deleting assets %d to %d of %d"), First + 1, Last, SafeAssets.Num()),
            40.0f + 50.0f * First / SafeAssets.Num());
        TArray<UObject*> Objects;
        for (int32 Index = First; Index < Last; ++Index)
        {
            if (UObject* Object = SafeAssets[Index].GetAsset())
            {
                Objects.Add(Object);
            }
        }
        if (Objects.Num() > 0)
        {
            McpQuiesceBeforeBatchDelete(Objects);
            DeletedByEngine += ObjectTools::ForceDeleteObjects(Objects, false);
            McpQuiesceAfterBatchDelete(Objects);
        }
    }
    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpSafeDeleteFolder: Deleted %d/%d safe assets in passes of %d"), DeletedByEngine, SafeAssets.Num(), PassSize);

    // What the batch left (an asset that would not load, a file an open linker kept) goes one by one.
    for (const FAssetData& SafeAsset : SafeAssets)
    {
        const FString SafeAssetPath = SafeAsset.PackageName.ToString();
        const bool bGone = McpAssetExists(SafeAssetPath)
            ? McpDeleteAssetAndFile(SafeAssetPath) && !McpAssetExists(SafeAssetPath)
            : McpRemoveLeftoverPackageFile(SafeAssetPath);
        if (!bGone)
        {
            UE_LOG(LogMcpSafeOperations, Error,
                TEXT("McpSafeDeleteFolder: Failed to delete safe asset '%s'"), *SafeAssetPath);
            return false;
        }
    }
    return true;
}

}

}
