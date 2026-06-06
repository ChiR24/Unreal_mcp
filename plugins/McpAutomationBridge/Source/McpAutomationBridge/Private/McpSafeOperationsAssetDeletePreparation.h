#pragma once

#include "McpSafeOperationsAssetClassification.h"
#include "McpSafeOperationsDeleteCompilation.h"

namespace McpSafeOperations
{

#if WITH_EDITOR

inline bool PrepareAssetBatchForDelete(
    const TArray<FAssetData>& Assets,
    const TCHAR* LogContext,
    TArray<UObject*>& OutFileBackedObjects,
    int32& OutInMemoryOnlyCount)
{
    OutFileBackedObjects.Reset();
    OutInMemoryOnlyCount = 0;

    IFileManager& FileManager = IFileManager::Get();
    TArray<FAssetData> InMemoryOnlyAssets;

    for (const FAssetData& AssetData : Assets)
    {
        const FString PackagePath = AssetData.PackageName.ToString();
        FString AssetFilePath;
        bool bHasBackingFile = false;
        const FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);
        const bool bIsWorldAsset = ClassName.Equals(TEXT("/Script/Engine.World"), ESearchCase::IgnoreCase) ||
                                   ClassName.EndsWith(TEXT(".World"), ESearchCase::IgnoreCase);
        const FString PackageExtension = bIsWorldAsset
            ? FPackageName::GetMapPackageExtension()
            : FPackageName::GetAssetPackageExtension();
        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, AssetFilePath, PackageExtension))
        {
            FString AbsolutePath = FPaths::IsRelative(AssetFilePath)
                ? FPaths::ConvertRelativePathToFull(AssetFilePath)
                : AssetFilePath;
            FPaths::NormalizeFilename(AbsolutePath);
            bHasBackingFile = FileManager.FileExists(*AbsolutePath);

            if (!bHasBackingFile)
            {
                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("%s: File does not exist: %s - asset is in-memory only"),
                    LogContext,
                    *AbsolutePath);
            }
        }

        if (!bHasBackingFile)
        {
            InMemoryOnlyAssets.Add(AssetData);
            ++OutInMemoryOnlyCount;
            continue;
        }

        UObject* Asset = AssetData.GetAsset();
        if (!Asset)
        {
            continue;
        }

        OutFileBackedObjects.Add(Asset);
    }

    if (OutInMemoryOnlyCount > 0)
    {
        if (!UnloadLoadedPackagesForAssets(InMemoryOnlyAssets, LogContext))
        {
            UE_LOG(LogMcpSafeOperations, Error,
                TEXT("%s: Failed to unload one or more in-memory-only packages before delete"),
                LogContext);
            return false;
        }
    }

    return true;
}

#endif

}
