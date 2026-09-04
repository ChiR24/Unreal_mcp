#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Safety/McpSafeOperations.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringGuidRegistry.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h"

namespace WidgetAuthoringHelpers
{
static UWidgetBlueprint* LoadWidgetBlueprintRaw(const FString& WidgetPath)
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
// Template actions (create_pause_menu, create_hud_widget, ...) author a widget from
// scratch, so a missing asset is created instead of answering NOT_FOUND (dogfood #26/#187).
UWidgetBlueprint* LoadOrCreateWidgetBlueprint(const FString& WidgetPath, bool* bOutCreated)
{
    if (bOutCreated)
    {
        *bOutCreated = false;
    }
    if (UWidgetBlueprint* Existing = LoadWidgetBlueprint(WidgetPath))
    {
        return Existing;
    }
    FString PackagePath = WidgetPath;
    if (PackagePath.Contains(TEXT(".")))
    {
        PackagePath = PackagePath.Left(PackagePath.Find(TEXT(".")));
    }
    if (!PackagePath.StartsWith(TEXT("/")))
    {
        PackagePath = TEXT("/Game/") + PackagePath;
    }
    const FString SafePackagePath = SanitizeProjectRelativePath(PackagePath);
    if (SafePackagePath.IsEmpty())
    {
        return nullptr;
    }
    const FString AssetName = FPaths::GetBaseFilename(SafePackagePath);
    UPackage* Package = CreatePackage(*SafePackagePath);
    if (!Package)
    {
        return nullptr;
    }
    UWidgetBlueprint* Created = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
        UUserWidget::StaticClass(), Package, FName(*AssetName), BPTYPE_Normal,
        UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
    if (Created)
    {
        FAssetRegistryModule::AssetCreated(Created);
        Package->MarkPackageDirty();
        if (bOutCreated)
        {
            *bOutCreated = true;
        }
    }
    return Created;
}
// Dogfood c22: the widget compiler ensures that every source widget and animation has a
// WidgetVariableNameToGuidMap entry. Assets authored before the registry existed (or renamed
// without moving their entry) trip that ensure on the next compile, so repair the map on load.
UWidgetBlueprint* LoadWidgetBlueprint(const FString& WidgetPath)
{
    UWidgetBlueprint* WidgetBP = LoadWidgetBlueprintRaw(WidgetPath);
    if (WidgetBP)
    {
        RegisterAllWidgetGuids(WidgetBP);
    }
    return WidgetBP;
}
void MarkWidgetBlueprintModifiedAndSave(UWidgetBlueprint* WidgetBP)
{
    if (!WidgetBP)
    {
        return;
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
    McpSafeOperations::McpSafeAssetSave(WidgetBP);
}
}
