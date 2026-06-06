#pragma once

#include "McpSafeOperationsFolderDeleteAssets.h"
#include "McpSafeOperationsFolderDeleteVerify.h"

namespace McpSafeOperations
{

#if WITH_EDITOR

inline bool McpSafeDeleteFolder(const FString& FolderPath, bool bForce = true)
{
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Starting deletion of '%s' (force=%d)"), *FolderPath, bForce);

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*FolderPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AllAssets;
    AssetRegistry.GetAssets(Filter, AllAssets);

    if (AllAssets.Num() == 0)
    {
        UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: No assets found in '%s'"), *FolderPath);
        return FolderDeleteInternal::DeleteEmptyFolder(FolderPath, AssetRegistry);
    }

    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Found %d assets in '%s'"), AllAssets.Num(), *FolderPath);

    TArray<FAssetData> WorldAssets;
    TArray<FAssetData> OtherAssets;
    FolderDeleteInternal::PartitionWorldAssets(AllAssets, WorldAssets, OtherAssets);

    if (!FolderDeleteInternal::SwitchAwayFromFolderWorldsIfNeeded(FolderPath, WorldAssets))
    {
        return false;
    }

    McpQuiesceAllState();
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Global quiesce completed before deletions"));

    TArray<FAssetData> RiskyAnimationAssets;
    TArray<FAssetData> SafeAssets;
    FolderDeleteInternal::PartitionRiskyAssets(OtherAssets, RiskyAnimationAssets, SafeAssets);

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpSafeDeleteFolder: Partitioned: %d risky special-delete, %d safe, %d world"),
        RiskyAnimationAssets.Num(), SafeAssets.Num(), WorldAssets.Num());

    if (!FolderDeleteInternal::DeleteRiskySpecialAssets(FolderPath, RiskyAnimationAssets, bForce))
    {
        return false;
    }

    if (!FolderDeleteInternal::DeleteSafeAssets(SafeAssets))
    {
        return false;
    }

    if (WorldAssets.Num() > 0)
    {
        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeDeleteFolder: Deleting %d world assets via package/file path"),
            WorldAssets.Num());

        const int32 DeletedWorlds = DeleteWorldPackagesByPath(WorldAssets);
        if (DeletedWorlds == INDEX_NONE)
        {
            return false;
        }
        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeDeleteFolder: Deleted %d/%d world assets via package/file path"),
            DeletedWorlds, WorldAssets.Num());
    }

    FlushRenderingCommands();
    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }
    FlushRenderingCommands();

    const FString ParentFolderPath = FPaths::GetPath(FolderPath);
    if (!ParentFolderPath.IsEmpty())
    {
        ScanPathSynchronous(ParentFolderPath, true);
    }

    FolderDeleteInternal::RemoveRegistryPathsAndDirectory(FolderPath, AssetRegistry);
    return FolderDeleteInternal::VerifyFolderDeleted(FolderPath, AssetRegistry);
}

#endif

}
