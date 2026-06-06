#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHelpers
{
UWidgetBlueprint* LoadWidgetBlueprint(const FString& WidgetPath)
{
    FString Path = WidgetPath;
    if (Path.EndsWith(TEXT("_C")))
    {
        return nullptr;
    }
    if (!Path.StartsWith(TEXT("/")))
    {
        Path = TEXT("/Game/") + Path;
    }

    FString ObjectPath = Path;
    FString PackagePath = Path;
    if (Path.Contains(TEXT(".")))
    {
        PackagePath = Path.Left(Path.Find(TEXT(".")));
    }
    else
    {
        FString AssetName = FPaths::GetBaseFilename(Path);
        ObjectPath = Path + TEXT(".") + AssetName;
    }

    FString AssetName = FPaths::GetBaseFilename(PackagePath);
    if (UWidgetBlueprint* WB = FindObject<UWidgetBlueprint>(nullptr, *ObjectPath))
    {
        return WB;
    }
    if (UPackage* Package = FindPackage(nullptr, *PackagePath))
    {
        if (UWidgetBlueprint* WB = FindObject<UWidgetBlueprint>(Package, *AssetName))
        {
            return WB;
        }
    }

    for (TObjectIterator<UWidgetBlueprint> It; It; ++It)
    {
        UWidgetBlueprint* WB = *It;
        if (!WB)
        {
            continue;
        }
        FString WBPath = WB->GetPathName();
        if (WBPath.Equals(ObjectPath, ESearchCase::IgnoreCase) ||
            WBPath.Equals(PackagePath, ESearchCase::IgnoreCase) ||
            WBPath.Equals(Path, ESearchCase::IgnoreCase))
        {
            return WB;
        }
        FString WBPackagePath = WBPath;
        if (WBPackagePath.Contains(TEXT(".")))
        {
            WBPackagePath = WBPackagePath.Left(WBPackagePath.Find(TEXT(".")));
        }
        if (WBPackagePath.Equals(PackagePath, ESearchCase::IgnoreCase))
        {
            return WB;
        }
    }

    IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FAssetData AssetData = Registry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
#else
    FAssetData AssetData = Registry.GetAssetByObjectPath(FName(*ObjectPath));
#endif
    if (AssetData.IsValid())
    {
        if (UWidgetBlueprint* WB = Cast<UWidgetBlueprint>(AssetData.GetAsset()))
        {
            return WB;
        }
    }
    if (UWidgetBlueprint* WB = Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *ObjectPath)))
    {
        return WB;
    }
    return Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *PackagePath));
}
}
