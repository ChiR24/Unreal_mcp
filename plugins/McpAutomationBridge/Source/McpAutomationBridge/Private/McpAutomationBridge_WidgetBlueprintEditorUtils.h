#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

class FBlueprintEditor;
class FWidgetBlueprintEditor;
class SDockTab;
class SDesignerView;
class UBlueprint;
class UObject;
class UWidget;
class UWidgetBlueprint;

struct FMcpResolvedWidgetBlueprintEditorContext
{
    UObject *Asset = nullptr;
    UBlueprint *Blueprint = nullptr;
    FBlueprintEditor *Editor = nullptr;
    FString AssetPath;
    FString RequestedGraphName;
    FString RequestedTabId;
    FString RequestedWindowTitle;
    FString ResolvedGraphName;
    FString EditorType;
    FString CurrentMode;
    FString TabId;
    FString WindowTitle;
    FString ResolvedTargetSource;
    bool bOpenedAssetEditor = false;

    UWidgetBlueprint *WidgetBlueprint = nullptr;
    FWidgetBlueprintEditor *WidgetEditor = nullptr;
    TSharedPtr<SDockTab> DesignerTab;
    TSharedPtr<SDesignerView> DesignerView;
    FString RequestedMode;
    FString RequestedWidgetName;
    FString RequestedWidgetPath;
    FString RequestedWidgetObjectPath;
    FString WidgetSelectorType;
    FString WidgetSelectorValue;
    FString ResolvedWidgetName;
    FString ResolvedWidgetPath;
    FString ResolvedWidgetObjectPath;
    bool bDesignerTabFound = false;
    bool bDesignerViewFound = false;
    bool bQueuedDesignerAction = false;
    bool bLiveEditorContextFound = false;
};

namespace McpWidgetBlueprintEditorUtils
{
    FString NormalizeWidgetDesignerPath(FString InPath);
    FString BuildWidgetDesignerPath(UWidget *Widget);

    bool ResolveWidgetBlueprintEditorContext(const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResolvedWidgetBlueprintEditorContext &OutContext,
                                             FString &OutErrorCode,
                                             FString &OutErrorMessage,
                                             bool bOpenEditorIfNeeded = true,
                                             bool bFocusEditor = true,
                                             bool bRequireLiveEditor = true);

    void RefreshWidgetBlueprintSurfaceDiagnostics(FMcpResolvedWidgetBlueprintEditorContext &Context);

    void QueueWidgetBlueprintDesignerAction(
        FMcpResolvedWidgetBlueprintEditorContext &Context,
        TFunction<void(FWidgetBlueprintEditor *)> Action);

    bool ResolveWidgetBlueprintTargetWidget(const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResolvedWidgetBlueprintEditorContext &Context,
                                            UWidget *&OutWidget,
                                            FString &OutErrorCode,
                                            FString &OutErrorMessage);
}