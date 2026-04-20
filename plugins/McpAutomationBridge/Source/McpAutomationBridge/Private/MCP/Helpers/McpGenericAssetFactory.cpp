// McpGenericAssetFactory.cpp
#include "McpGenericAssetFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "HAL/IConsoleManager.h"
#include "McpAutomationBridgeHelpers.h" // for McpSafeAssetSave

namespace McpGenericAssetFactory
{
    UObject* CreateAssetOfClass(
        UClass* AssetClass,
        const FString& PackagePath,
        const FString& AssetName,
        TFunction<void(UObject*)> Configurator,
        FString& OutError,
        bool& bOutSaved)
    {
        check(IsInGameThread());
        bOutSaved = false;
        if (!AssetClass) { OutError = TEXT("Null AssetClass"); return nullptr; }
        if (AssetName.IsEmpty()) { OutError = TEXT("Empty AssetName"); return nullptr; }
        if (PackagePath.IsEmpty()) { OutError = TEXT("Empty PackagePath"); return nullptr; }

        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, AssetClass, nullptr);
        if (!NewAsset)
        {
            OutError = FString::Printf(TEXT("CreateAsset failed for %s/%s"), *PackagePath, *AssetName);
            return nullptr;
        }

        if (Configurator)
        {
            Configurator(NewAsset);
        }

        NewAsset->MarkPackageDirty();
        if (McpSafeAssetSave(NewAsset))
        {
            bOutSaved = true;
        }
        else
        {
            OutError = FString::Printf(TEXT("McpSafeAssetSave failed for %s/%s"), *PackagePath, *AssetName);
            // Do not delete; asset is valid in memory even if save fails.
            // bOutSaved stays false so callers can distinguish this from full success.
        }
        return NewAsset;
    }
}
