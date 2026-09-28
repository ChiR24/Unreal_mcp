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
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

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
    FAssetData AssetData = Registry.GetAssetByObjectPath(MCP_ASSET_REGISTRY_OBJECT_PATH(ObjectPath));
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
bool MarkWidgetBlueprintModifiedAndSave(UWidgetBlueprint* WidgetBP)
{
    if (!WidgetBP)
    {
        return false;
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
    return McpSafeOperations::McpSafeAssetSave(WidgetBP);
}

UWidgetAnimation* FindWidgetAnimation(UWidgetBlueprint* WidgetBP, const FString& AnimationName)
{
    for (UWidgetAnimation* Anim : WidgetBP->Animations)
    {
        if (Anim && Anim->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
        {
            return Anim;
        }
    }
    return nullptr;
}

UWidgetAnimation* ResolveWidgetAnimation(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
                                         TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
                                         UWidgetBlueprint*& OutWidgetBP)
{
    const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
    const FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
    if (WidgetPath.IsEmpty() || AnimationName.IsEmpty())
    {
        Subsystem.SendAutomationError(Socket, RequestId,
            TEXT("Missing required parameters: widgetPath, animationName"), TEXT("MISSING_PARAMETER"));
        return nullptr;
    }
    OutWidgetBP = LoadWidgetBlueprint(WidgetPath);
    if (!OutWidgetBP)
    {
        Subsystem.SendAutomationError(Socket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
        return nullptr;
    }
    UWidgetAnimation* Animation = FindWidgetAnimation(OutWidgetBP, AnimationName);
    if (!Animation)
    {
        Subsystem.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Animation '%s' not found; create it with create_widget_animation first"), *AnimationName),
            TEXT("ANIMATION_NOT_FOUND"));
    }
    return Animation;
}

UWidget* FindWidgetByName(UWidgetTree* Tree, const FString& WidgetName)
{
    UWidget* Found = nullptr;
    Tree->ForEachWidget([&](UWidget* W) {
        if (!Found && W && W->GetFName().ToString().Equals(WidgetName, ESearchCase::IgnoreCase))
        {
            Found = W;
        }
    });
    return Found;
}
}
