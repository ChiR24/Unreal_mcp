#pragma once

#include "Safety/McpSafeOperationsDeleteCompilation.h"
#include "Safety/McpSafeOperationsMaterial.h"

namespace McpSafeOperations
{

namespace FolderDeleteInternal
{

inline void RemoveRegistryPathsAndDirectory(const FString& FolderPath, IAssetRegistry& AssetRegistry)
{
    TArray<FString> SubPathsToRemove;
    AssetRegistry.GetSubPaths(FolderPath, SubPathsToRemove, true);
    SubPathsToRemove.Sort([](const FString& A, const FString& B)
    {
        return A.Len() > B.Len();
    });
    for (const FString& SubPath : SubPathsToRemove)
    {
        AssetRegistry.RemovePath(SubPath);
    }
    AssetRegistry.RemovePath(FolderPath);

    FString LocalPath;
    if (FPackageName::TryConvertLongPackageNameToFilename(FolderPath, LocalPath))
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (PlatformFile.DirectoryExists(*LocalPath))
        {
            PlatformFile.DeleteDirectoryRecursively(*LocalPath);
            UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Deleted physical directory '%s'"), *LocalPath);

            if (PlatformFile.DirectoryExists(*LocalPath))
            {
                McpSafePostDeleteGC();
                FPlatformProcess::Sleep(0.05f);

                PlatformFile.DeleteDirectoryRecursively(*LocalPath);
                UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Retried physical directory deletion '%s'"), *LocalPath);
            }
        }
    }
}

// A registry entry left in the folder is a survivor when its file is still on disk or its asset is
// still loaded and alive (an unsaved one counted as deleted before, and the caller was told success);
// an entry whose object is already marked for collection is only stale. OutRemaining names survivors.
inline bool VerifyFolderDeleted(const FString& FolderPath, IAssetRegistry& AssetRegistry, TArray<FString>* OutRemaining = nullptr)
{
    FARFilter RemainingFilter;
    RemainingFilter.PackagePaths.Add(FName(*FolderPath));
    RemainingFilter.bRecursivePaths = true;

    TArray<FAssetData> RemainingAssets;
    AssetRegistry.GetAssets(RemainingFilter, RemainingAssets);

    TArray<FAssetData> Survivors;
    for (const FAssetData& RemainingAsset : RemainingAssets)
    {
        const FString ObjectPath = MCP_ASSET_DATA_GET_SOFT_PATH(RemainingAsset);
        if (McpPackageHasBackingFile(RemainingAsset.PackageName.ToString())
            || (!ObjectPath.IsEmpty() && IsValid(FindObject<UObject>(nullptr, *ObjectPath))))
        {
            Survivors.Add(RemainingAsset);
        }
    }

    TArray<FString> RemainingSubPaths;
    AssetRegistry.GetSubPaths(FolderPath, RemainingSubPaths, true);

    bool bDirectoryExistsOnDisk = false;
    FString VerifyLocalPath;
    if (FPackageName::TryConvertLongPackageNameToFilename(FolderPath, VerifyLocalPath))
    {
        bDirectoryExistsOnDisk = FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*VerifyLocalPath);
    }

    if (Survivors.Num() == 0 && RemainingSubPaths.Num() == 0 && !bDirectoryExistsOnDisk)
    {
        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeDeleteFolder: Successfully deleted '%s' (%d stale registry entries for objects pending collection)"),
            *FolderPath, RemainingAssets.Num());
        return true;
    }

    UE_LOG(LogMcpSafeOperations, Warning,
        TEXT("McpSafeDeleteFolder: Directory still exists after deletion attempt (remainingAssets=%d survivors=%d remainingSubPaths=%d existsOnDisk=%d)"),
        RemainingAssets.Num(), Survivors.Num(), RemainingSubPaths.Num(), bDirectoryExistsOnDisk ? 1 : 0);

    for (const FAssetData& Survivor : Survivors)
    {
        const FString SurvivorPath = MCP_ASSET_DATA_GET_SOFT_PATH(Survivor);
        UE_LOG(LogMcpSafeOperations, Warning, TEXT("McpSafeDeleteFolder: Remaining asset: %s (%s)"),
            *SurvivorPath, *MCP_ASSET_DATA_GET_CLASS_PATH(Survivor));
        if (OutRemaining) { OutRemaining->Add(SurvivorPath); }
    }

    for (const FString& RemainingSubPath : RemainingSubPaths)
    {
        UE_LOG(LogMcpSafeOperations, Warning,
            TEXT("McpSafeDeleteFolder: Remaining subpath: %s"),
            *RemainingSubPath);
        if (OutRemaining) { OutRemaining->Add(RemainingSubPath + TEXT("/")); }
    }
    return false;
}

}

}
