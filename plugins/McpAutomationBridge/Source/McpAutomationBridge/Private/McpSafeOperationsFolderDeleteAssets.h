#pragma once

#include "McpSafeOperationsAnimationDelete.h"
#include "McpSafeOperationsMapLoad.h"
#include "McpSafeOperationsWorldDelete.h"

namespace McpSafeOperations
{

#if WITH_EDITOR
namespace FolderDeleteInternal
{

inline bool DeleteEmptyFolder(const FString& FolderPath, IAssetRegistry& AssetRegistry)
{
    TArray<FString> EmptySubPaths;
    AssetRegistry.GetSubPaths(FolderPath, EmptySubPaths, true);
    EmptySubPaths.Sort([](const FString& A, const FString& B)
    {
        return A.Len() > B.Len();
    });
    for (const FString& SubPath : EmptySubPaths)
    {
        AssetRegistry.RemovePath(SubPath);
    }
    AssetRegistry.RemovePath(FolderPath);

    FString EmptyLocalPath;
    bool bDirectoryExistsOnDisk = false;
    if (FPackageName::TryConvertLongPackageNameToFilename(FolderPath, EmptyLocalPath))
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (PlatformFile.DirectoryExists(*EmptyLocalPath))
        {
            PlatformFile.DeleteDirectoryRecursively(*EmptyLocalPath);
        }
        bDirectoryExistsOnDisk = PlatformFile.DirectoryExists(*EmptyLocalPath);
    }

    TArray<FString> RemainingEmptySubPaths;
    AssetRegistry.GetSubPaths(FolderPath, RemainingEmptySubPaths, true);
    const bool bDeleted = RemainingEmptySubPaths.Num() == 0 && !bDirectoryExistsOnDisk;
    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpSafeDeleteFolder: Empty folder deletion result for '%s' (remainingSubPaths=%d existsOnDisk=%d)"),
        *FolderPath, RemainingEmptySubPaths.Num(), bDirectoryExistsOnDisk ? 1 : 0);
    return bDeleted;
}

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

    FlushRenderingCommands();
    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }
    FlushRenderingCommands();

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
    const TArray<FAssetData>& RiskyAnimationAssets,
    bool bForce)
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
        const int32 OrderedClusterDeleted = DeleteAnimationRigClusterOrdered(OrderedClusterAssets, bForce);
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

        const int32 GenericDeleted = DeleteAnimationRigClusterOrdered(GenericRiskyAssets, bForce);
        DeletedRisky += GenericDeleted;

        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeDeleteFolder: Deleted %d/%d remaining risky special-delete assets via ordered engine-owned deletion"),
            GenericDeleted, TotalRisky);
    }

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpSafeDeleteFolder: Deleted %d total risky special-delete assets"), DeletedRisky);
    return true;
}

inline bool DeleteSafeAssets(const TArray<FAssetData>& SafeAssets)
{
    if (SafeAssets.Num() == 0)
    {
        return true;
    }

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpSafeDeleteFolder: Deleting %d safe assets via UEditorAssetLibrary::DeleteAsset"),
        SafeAssets.Num());

    int32 DeletedSafeAssets = 0;
    for (const FAssetData& SafeAsset : SafeAssets)
    {
        const FString SafeAssetPath = SafeAsset.PackageName.ToString();
        if (SafeAssetPath.IsEmpty())
        {
            continue;
        }

        const bool bDeletedSafeAsset = UEditorAssetLibrary::DeleteAsset(SafeAssetPath);
        const bool bExistsAfterDelete = UEditorAssetLibrary::DoesAssetExist(SafeAssetPath);
        if (bDeletedSafeAsset && !bExistsAfterDelete)
        {
            ++DeletedSafeAssets;
        }
        else
        {
            UE_LOG(LogMcpSafeOperations, Error,
                TEXT("McpSafeDeleteFolder: Failed to delete safe asset '%s' (deleteResult=%d existsAfter=%d)"),
                *SafeAssetPath, bDeletedSafeAsset ? 1 : 0, bExistsAfterDelete ? 1 : 0);
            return false;
        }
    }

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpSafeDeleteFolder: Deleted %d/%d safe assets"),
        DeletedSafeAssets, SafeAssets.Num());
    return true;
}

}
#endif

}
