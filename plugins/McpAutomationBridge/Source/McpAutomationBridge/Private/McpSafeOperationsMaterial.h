#pragma once

#include "McpSafeOperationsAssetSave.h"

namespace McpSafeOperations
{

#if WITH_EDITOR

inline UMaterialInterface* McpLoadMaterialWithFallback(const FString& MaterialPath, bool bSilent = false)
{
    if (!MaterialPath.IsEmpty())
    {
        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
        if (Material)
        {
            return Material;
        }
        if (!bSilent)
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("McpLoadMaterialWithFallback: Requested material not found: %s"), *MaterialPath);
        }
    }

    const TCHAR* FallbackPaths[] = {
        TEXT("/Engine/EngineMaterials/DefaultMaterial"),
        TEXT("/Engine/EngineMaterials/WorldGridMaterial"),
        TEXT("/Engine/EngineMaterials/DefaultDeferredDecalMaterial"),
        TEXT("/Engine/EngineMaterials/DefaultTextMaterialOpaque")
    };

    for (const TCHAR* FallbackPath : FallbackPaths)
    {
        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, FallbackPath);
        if (Material)
        {
            if (!bSilent && !MaterialPath.IsEmpty())
            {
                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("McpLoadMaterialWithFallback: Using fallback '%s' for '%s'"),
                    FallbackPath, *MaterialPath);
            }
            return Material;
        }
    }

    UE_LOG(LogMcpSafeOperations, Error,
        TEXT("McpLoadMaterialWithFallback: All fallback materials unavailable - engine content may be missing"));
    return nullptr;
}

inline bool SaveLoadedAssetThrottled(UObject* Asset, double ThrottleSecondsOverride = -1.0, bool bForce = false)
{
    if (!Asset)
    {
        return false;
    }

    (void)ThrottleSecondsOverride;
    (void)bForce;

    return McpSafeAssetSave(Asset);
}

inline void ScanPathSynchronous(const FString& InPath, bool bRecursive = true)
{
    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FString> PathsToScan;
    PathsToScan.Add(InPath);
    AssetRegistry.ScanPathsSynchronous(PathsToScan, bRecursive);
}

#endif

}
