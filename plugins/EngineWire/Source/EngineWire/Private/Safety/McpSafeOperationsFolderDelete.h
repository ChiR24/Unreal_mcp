#pragma once

#include "Safety/McpSafeOperationsFolderDeleteAssets.h"
#include "Safety/McpSafeOperationsFolderDeleteVerify.h"

namespace McpSafeOperations
{

namespace FolderDeleteInternal
{

/**
 * Deletes from disk, without loading them, the folder's assets that are not in memory and that nothing outside
 * what is being deleted (the folder, and AlsoDeleted: the rest of the same request) references: there is nothing
 * in memory to fix up, and loading thousands of packages only to delete them held the game thread for a quarter
 * of an hour. Loaded or outside-referenced assets stay in Assets for the engine delete, as does a file the OS
 * refused. The registry forgets the removed files before this returns.
 */
inline void DeleteUnloadedAssetFiles(const FString& FolderPath, const TArray<FString>& AlsoDeleted,
    TArray<FAssetData>& Assets, const FMcpDeleteProgress& Progress)
{
    IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
    const auto Deleted = [&FolderPath, &AlsoDeleted](const FString& Package)
    {
        return Package.StartsWith(FolderPath + TEXT("/")) || AlsoDeleted.ContainsByPredicate([&Package](const FString& Path)
            { return Package == Path || Package.StartsWith(Path + TEXT("/")); });
    };
    TMap<FName, FString> Files; // package -> file
    for (const FAssetData& Asset : Assets)
    {
        const FString PackageName = Asset.PackageName.ToString();
        FString Filename;
        if (Files.Contains(Asset.PackageName) || FindObject<UPackage>(nullptr, *PackageName)
            || !FPackageName::DoesPackageExist(PackageName, &Filename))
        {
            continue;
        }
        TArray<FAssetIdentifier> Refs;
        Registry.GetReferencers(FAssetIdentifier(Asset.PackageName), Refs, UE::AssetRegistry::EDependencyCategory::Package);
        if (!Refs.ContainsByPredicate([&Deleted](const FAssetIdentifier& Ref) { return !Deleted(Ref.PackageName.ToString()); }))
        {
            Files.Add(Asset.PackageName, FPaths::ConvertRelativePathToFull(Filename));
        }
    }
    TSet<FName> Gone;
    TArray<FString> Removed;
    int32 Tried = 0;
    for (const TPair<FName, FString>& File : Files)
    {
        if (Tried++ % 250 == 0)
        {
            Progress(FString::Printf(TEXT("removing unloaded asset files: %d of %d"), Tried - 1, Files.Num()),
                40.0f * (Tried - 1) / Files.Num());
        }
        if (IFileManager::Get().Delete(*File.Value, false, true, true))
        {
            Gone.Add(File.Key);
            Removed.Add(File.Value);
        }
    }
    Assets.RemoveAll([&Gone](const FAssetData& Asset) { return Gone.Contains(Asset.PackageName); });
    if (Removed.Num() > 0)
    {
        Registry.ScanModifiedAssetFiles(Removed);
    }
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Removed %d of %d unloaded asset files without loading them"),
        Removed.Num(), Files.Num());
}

}


inline bool McpSafeDeleteFolder(const FString& FolderPath, TArray<FString>* OutRemaining = nullptr,
    const FMcpDeleteProgress& OnProgress = nullptr, const TArray<FString>& AlsoDeleted = TArray<FString>())
{
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Starting deletion of '%s'"), *FolderPath);
    const FMcpDeleteProgress Progress = OnProgress ? OnProgress : FMcpDeleteProgress([](const FString&, float) {});

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
    FolderDeleteInternal::DeleteUnloadedAssetFiles(FolderPath, AlsoDeleted, OtherAssets, Progress);

    TArray<FAssetData> RiskyAnimationAssets;
    TArray<FAssetData> SafeAssets;
    FolderDeleteInternal::PartitionRiskyAssets(OtherAssets, RiskyAnimationAssets, SafeAssets);

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpSafeDeleteFolder: Partitioned: %d risky special-delete, %d safe, %d world"),
        RiskyAnimationAssets.Num(), SafeAssets.Num(), WorldAssets.Num());

    if (RiskyAnimationAssets.Num() > 0)
    {
        Progress(FString::Printf(TEXT("deleting %d loaded Blueprint and animation assets in order"), RiskyAnimationAssets.Num()), 40.0f);
    }
    if (!FolderDeleteInternal::DeleteRiskySpecialAssets(FolderPath, RiskyAnimationAssets))
    {
        return false;
    }

    if (!FolderDeleteInternal::DeleteSafeAssets(SafeAssets, Progress))
    {
        return false;
    }
    Progress(TEXT("collecting garbage and checking the folder is gone"), 90.0f);

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
