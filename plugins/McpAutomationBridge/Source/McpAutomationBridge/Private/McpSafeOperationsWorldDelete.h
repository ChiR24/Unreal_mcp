#pragma once

#include "McpSafeOperationsDeleteCompilation.h"

namespace McpSafeOperations
{

#if WITH_EDITOR

inline bool HasLoadedWorlds(const TArray<FAssetData>& WorldAssets)
{
    for (const FAssetData& AssetData : WorldAssets)
    {
        FString PackageName = AssetData.PackageName.ToString();
        if (FindObject<UPackage>(nullptr, *PackageName))
        {
            return true;
        }
    }
    return false;
}

inline int32 DeleteWorldPackagesByPath(const TArray<FAssetData>& WorldAssets)
{
    int32 DeletedCount = 0;
    IFileManager& FileManager = IFileManager::Get();
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    if (!UnloadLoadedPackagesForAssets(WorldAssets, TEXT("DeleteWorldPackagesByPath")))
    {
        UE_LOG(LogMcpSafeOperations, Error,
            TEXT("DeleteWorldPackagesByPath: Failed to unload one or more loaded world packages before file deletion"));
        return INDEX_NONE;
    }

    for (const FAssetData& AssetData : WorldAssets)
    {
        const FString PackagePath = AssetData.PackageName.ToString();

        FString MapFilename;
        if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath, MapFilename, FPackageName::GetMapPackageExtension()))
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("DeleteWorldPackagesByPath: Could not convert map package to filename: %s"),
                *PackagePath);
            continue;
        }

        FString AbsoluteMapFilename = FPaths::ConvertRelativePathToFull(MapFilename);
        FPaths::NormalizeFilename(AbsoluteMapFilename);

        bool bDeletedMap = false;
        if (FileManager.FileExists(*AbsoluteMapFilename))
        {
            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("DeleteWorldPackagesByPath: Deleting map file: %s"),
                *AbsoluteMapFilename);
            bDeletedMap = FileManager.Delete(*AbsoluteMapFilename, false, true, true);
            if (!bDeletedMap)
            {
                UE_LOG(LogMcpSafeOperations, Warning,
                    TEXT("DeleteWorldPackagesByPath: Failed to delete map file: %s"),
                    *AbsoluteMapFilename);
            }
        }
        else
        {
            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("DeleteWorldPackagesByPath: Map file does not exist: %s"),
                *AbsoluteMapFilename);
        }

        const FString BuiltDataPackagePath = PackagePath + TEXT("_BuiltData");
        FString BuiltDataFilename;
        if (FPackageName::TryConvertLongPackageNameToFilename(BuiltDataPackagePath, BuiltDataFilename, FPackageName::GetAssetPackageExtension()))
        {
            FString AbsoluteBuiltDataFilename = FPaths::ConvertRelativePathToFull(BuiltDataFilename);
            FPaths::NormalizeFilename(AbsoluteBuiltDataFilename);
            if (FileManager.FileExists(*AbsoluteBuiltDataFilename))
            {
                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("DeleteWorldPackagesByPath: Deleting built data file: %s"),
                    *AbsoluteBuiltDataFilename);
                if (!FileManager.Delete(*AbsoluteBuiltDataFilename, false, true, true))
                {
                    UE_LOG(LogMcpSafeOperations, Warning,
                        TEXT("DeleteWorldPackagesByPath: Failed to delete built data file: %s"),
                        *AbsoluteBuiltDataFilename);
                }
            }
        }

        TArray<FString> ScanPaths;
        ScanPaths.Add(FPaths::GetPath(PackagePath));
        AssetRegistry.ScanPathsSynchronous(ScanPaths, false);

        TArray<FAssetData> RemainingWorldAssets;
        AssetRegistry.GetAssetsByPackageName(AssetData.PackageName, RemainingWorldAssets, true);
        const bool bRegistryPackageGone = RemainingWorldAssets.Num() == 0;

        if (bDeletedMap && !FileManager.FileExists(*AbsoluteMapFilename) && bRegistryPackageGone)
        {
            ++DeletedCount;
        }
        else
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("DeleteWorldPackagesByPath: World package still present after delete attempt (package=%s deletedMap=%d fileExists=%d registryCount=%d)"),
                *PackagePath,
                bDeletedMap ? 1 : 0,
                FileManager.FileExists(*AbsoluteMapFilename) ? 1 : 0,
                RemainingWorldAssets.Num());
        }
    }

    FlushRenderingCommands();
    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }
    FlushRenderingCommands();

    return DeletedCount;
}

#endif

}
