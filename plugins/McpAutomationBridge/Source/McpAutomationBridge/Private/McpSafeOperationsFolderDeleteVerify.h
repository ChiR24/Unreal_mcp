#pragma once

#include "McpSafeOperationsMaterial.h"

namespace McpSafeOperations
{

#if WITH_EDITOR
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
                FlushRenderingCommands();
                if (GEditor)
                {
                    GEditor->ForceGarbageCollection(true);
                }
                FlushRenderingCommands();
                FPlatformProcess::Sleep(0.05f);

                PlatformFile.DeleteDirectoryRecursively(*LocalPath);
                UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Retried physical directory deletion '%s'"), *LocalPath);
            }
        }
    }
}

inline bool AssetDataHasBackingFile(const FAssetData& AssetData)
{
    const FString PackagePath = AssetData.PackageName.ToString();

    FString AssetFilename;
    if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, AssetFilename, FPackageName::GetAssetPackageExtension()))
    {
        if (IFileManager::Get().FileExists(*FPaths::ConvertRelativePathToFull(AssetFilename)))
        {
            return true;
        }
    }

    FString MapFilename;
    if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, MapFilename, FPackageName::GetMapPackageExtension()))
    {
        if (IFileManager::Get().FileExists(*FPaths::ConvertRelativePathToFull(MapFilename)))
        {
            return true;
        }
    }

    return false;
}

inline bool VerifyFolderDeleted(const FString& FolderPath, IAssetRegistry& AssetRegistry)
{
    FARFilter RemainingFilter;
    RemainingFilter.PackagePaths.Add(FName(*FolderPath));
    RemainingFilter.bRecursivePaths = true;

    TArray<FAssetData> RemainingAssets;
    AssetRegistry.GetAssets(RemainingFilter, RemainingAssets);

    TArray<FAssetData> RemainingFileBackedAssets;
    for (const FAssetData& RemainingAsset : RemainingAssets)
    {
        if (AssetDataHasBackingFile(RemainingAsset))
        {
            RemainingFileBackedAssets.Add(RemainingAsset);
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

    if (RemainingAssets.Num() == 0 && RemainingSubPaths.Num() == 0 && !bDirectoryExistsOnDisk)
    {
        UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Successfully deleted '%s'"), *FolderPath);
        return true;
    }

    if (RemainingFileBackedAssets.Num() == 0 && RemainingSubPaths.Num() == 0 && !bDirectoryExistsOnDisk)
    {
        UE_LOG(LogMcpSafeOperations, Warning,
            TEXT("McpSafeDeleteFolder: Physical folder '%s' deleted; only %d in-memory package(s) without backing files remain"),
            *FolderPath, RemainingAssets.Num());
        return true;
    }

    UE_LOG(LogMcpSafeOperations, Warning,
        TEXT("McpSafeDeleteFolder: Directory still exists after deletion attempt (remainingAssets=%d remainingFileBackedAssets=%d remainingSubPaths=%d existsOnDisk=%d)"),
        RemainingAssets.Num(), RemainingFileBackedAssets.Num(), RemainingSubPaths.Num(), bDirectoryExistsOnDisk ? 1 : 0);

    for (const FAssetData& RemainingAsset : RemainingAssets)
    {
        UE_LOG(LogMcpSafeOperations, Warning, TEXT("McpSafeDeleteFolder: Remaining asset: %s (%s)"),
            *MCP_ASSET_DATA_GET_SOFT_PATH(RemainingAsset),
            *MCP_ASSET_DATA_GET_CLASS_PATH(RemainingAsset));
    }

    for (const FString& RemainingSubPath : RemainingSubPaths)
    {
        UE_LOG(LogMcpSafeOperations, Warning,
            TEXT("McpSafeDeleteFolder: Remaining subpath: %s"),
            *RemainingSubPath);
    }
    return false;
}

}
#endif

}
