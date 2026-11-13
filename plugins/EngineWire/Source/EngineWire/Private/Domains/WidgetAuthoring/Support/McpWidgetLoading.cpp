#include "Domains/WidgetAuthoring/Support/McpWidgetBPLoading.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetResolve.h"
#include "Foundation/BridgeHelpers/Security/McpAssetPathCanonical.h"
#include "Safety/McpSafeOperations.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetGuidRegistry.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "WidgetBlueprint.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"

namespace WidgetAuthoringHelpers
{
static UWidgetBlueprint* LoadWidgetBlueprintRaw(const FString& WidgetPath)
{
    // A class path (WBP_X_C) names the generated class, not the Blueprint.
    if (WidgetPath.EndsWith(TEXT("_C")))
    {
        return nullptr;
    }
    // The shared canonicalizer: the /Content alias the gate accepts as /Game's, and a bare path under /Game.
    const FString Path = McpCanonicalizeContentPath(WidgetPath, true);
    FString Loaded;
    return Path.IsEmpty() ? nullptr : McpLoadCanonicalAsset<UWidgetBlueprint>(Path, Loaded);
}
// The widget compiler ensures that every source widget and animation has a
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
UWidgetBlueprint* LoadWidgetBlueprintOrReply(UEngineWireSubsystem& Subsystem, const FString& RequestId,
                                             const TSharedPtr<FMcpBridgeWebSocket>& Socket, const FString& WidgetPath)
{
    UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
    if (!WidgetBP)
    {
        Subsystem.SendAutomationError(Socket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
    }
    return WidgetBP;
}
FString WidgetBlueprintPackagePath(const UWidgetBlueprint* WidgetBP)
{
    return WidgetBP && WidgetBP->GetOutermost() ? WidgetBP->GetOutermost()->GetName() : FString();
}

bool RefreshWidgetBlueprintClass(UWidgetBlueprint* WidgetBP)
{
    if (!WidgetBP || (GEditor && GEditor->PlayWorld))
    {
        return false;
    }
    const bool bCompiled = McpSafeCompileBlueprint(WidgetBP);
    // The engine only relinks the Blueprints that read this one's widgets, mapping each old property to a new one
    // by name, so removing a widget left their compiled code pointing at nothing - and a widget of that name added
    // back did not repair it: a game mode focusing the rebuilt PlayButton failed every play with "Attempted to
    // access missing property" while it still read up to date. Compiled in full, each finds the widget by name again
    // (or reports that it is gone).
    TArray<UBlueprint*> Dependents;
    FBlueprintEditorUtils::GetDependentBlueprints(WidgetBP, Dependents);
    for (UBlueprint* Dependent : Dependents)
    {
        McpSafeCompileBlueprint(Dependent);
    }
    return bCompiled;
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

UWidgetAnimation* ResolveWidgetAnimation(UEngineWireSubsystem& Subsystem, const FString& RequestId,
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
    OutWidgetBP = LoadWidgetBlueprintOrReply(Subsystem, RequestId, Socket, WidgetPath);
    if (!OutWidgetBP)
    {
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
}
