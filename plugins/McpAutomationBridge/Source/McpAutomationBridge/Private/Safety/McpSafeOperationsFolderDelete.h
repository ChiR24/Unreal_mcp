#pragma once

#include "Safety/McpSafeOperationsFolderDeleteAssets.h"
#include "Safety/McpSafeOperationsFolderDeleteVerify.h"

namespace McpSafeOperations
{


inline bool McpSafeDeleteFolder(const FString& FolderPath, TArray<FString>* OutRemaining = nullptr)
{
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Starting deletion of '%s'"), *FolderPath);

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
        FolderDeleteInternal::RemoveRegistryPathsAndDirectory(FolderPath, AssetRegistry);
        return FolderDeleteInternal::VerifyFolderDeleted(FolderPath, AssetRegistry, OutRemaining);
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

    if (!FolderDeleteInternal::DeleteRiskySpecialAssets(FolderPath, RiskyAnimationAssets))
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

    McpSafePostDeleteGC();

    const FString ParentFolderPath = FPaths::GetPath(FolderPath);
    if (!ParentFolderPath.IsEmpty())
    {
        ScanPathSynchronous(ParentFolderPath, true);
    }

    FolderDeleteInternal::RemoveRegistryPathsAndDirectory(FolderPath, AssetRegistry);
    return FolderDeleteInternal::VerifyFolderDeleted(FolderPath, AssetRegistry, OutRemaining);
}


}
