// McpGenericAssetFactory.h
#pragma once
#include "CoreMinimal.h"

namespace McpGenericAssetFactory
{
    /**
     * Create a UObject asset of the given class at PackagePath/AssetName.
     * MUST be called from Game Thread (wrap at callsite with AsyncTask if needed).
     * After CreateAsset, Configurator is called with the new object so caller can set defaults.
     * Saves via McpSafeAssetSave. Returns nullptr + OutError on failure.
     */
    UObject* CreateAssetOfClass(
        UClass* AssetClass,
        const FString& PackagePath,
        const FString& AssetName,
        TFunction<void(UObject*)> Configurator,
        FString& OutError);
}
