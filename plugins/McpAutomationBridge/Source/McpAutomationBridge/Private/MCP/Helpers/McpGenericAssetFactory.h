// McpGenericAssetFactory.h
#pragma once
#include "CoreMinimal.h"

namespace McpGenericAssetFactory
{
    /**
     * Create a UObject asset of the given class at PackagePath/AssetName.
     * MUST be called from Game Thread (wrap at callsite with AsyncTask if needed).
     * After CreateAsset, Configurator is called with the new object so caller can set defaults.
     *
     * Returns the created asset on success (even if save failed), or nullptr on create failure.
     * - OutError is populated on ANY failure (create or save).
     * - bOutSaved is set to true iff McpSafeAssetSave succeeded; callers that need a
     *   persisted-on-disk asset MUST check bOutSaved in addition to the non-null return.
     */
    UObject* CreateAssetOfClass(
        UClass* AssetClass,
        const FString& PackagePath,
        const FString& AssetName,
        TFunction<void(UObject*)> Configurator,
        FString& OutError,
        bool& bOutSaved);
}
