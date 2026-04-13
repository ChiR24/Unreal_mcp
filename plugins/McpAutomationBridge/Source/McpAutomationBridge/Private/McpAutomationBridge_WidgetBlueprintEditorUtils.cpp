#include "McpAutomationBridge_WidgetBlueprintEditorUtils.h"

#include "McpAutomationBridgeHelpers.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "BlueprintEditor.h"
#include "Components/Widget.h"
#include "Designer/SDesignerView.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#if __has_include("Subsystems/AssetEditorSubsystem.h")
#include "Subsystems/AssetEditorSubsystem.h"
#elif __has_include("AssetEditorSubsystem.h")
#include "AssetEditorSubsystem.h"
#endif
#include "Framework/Docking/TabManager.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"
#endif

namespace McpWidgetBlueprintEditorUtils
{
#if WITH_EDITOR
    namespace
    {
        const FName GMcpWidgetBlueprintDesignerTabId(TEXT("SlatePreview"));

        FString GetTrimmedJsonString(const TSharedPtr<FJsonObject> &Object, const TCHAR *FieldName)
        {
            FString Value;
            if (Object.IsValid())
            {
                Object->TryGetStringField(FieldName, Value);
            }

            return Value.TrimStartAndEnd();
        }

        TSharedPtr<FJsonObject> GetWidgetSelectorObject(const TSharedPtr<FJsonObject> &Payload)
        {
            if (Payload.IsValid() && Payload->HasTypedField<EJson::Object>(TEXT("selector")))
            {
                return Payload->GetObjectField(TEXT("selector"));
            }

            return Payload;
        }

        void RefreshBlueprintEditorSurfaceDiagnostics(IAssetEditorInstance *EditorInstance,
                                                      FMcpResolvedWidgetBlueprintEditorContext &Context)
        {
            if (EditorInstance == nullptr)
            {
                return;
            }

            TSharedPtr<FTabManager> TabManager = EditorInstance->GetAssociatedTabManager();
            auto ApplyTabDiagnostics = [&Context](const TSharedPtr<SDockTab> &DockTab,
                                                  const FString &Source)
            {
                if (!DockTab.IsValid())
                {
                    return;
                }

                Context.TabId = DockTab->GetLayoutIdentifier().ToString();
                if (const TSharedPtr<SWindow> ParentWindow = DockTab->GetParentWindow())
                {
                    Context.WindowTitle = ParentWindow->GetTitle().ToString();
                }
                if (!Source.IsEmpty())
                {
                    Context.ResolvedTargetSource = Source;
                }
            };

            if (TabManager.IsValid())
            {
                if (!Context.RequestedTabId.IsEmpty())
                {
                    if (const TSharedPtr<SDockTab> RequestedTab =
                            TabManager->FindExistingLiveTab(FTabId(FName(*Context.RequestedTabId))))
                    {
                        ApplyTabDiagnostics(RequestedTab, TEXT("tab_id_hint"));
                    }
                }

                if (Context.ResolvedTargetSource.IsEmpty())
                {
                    if (const TSharedPtr<SDockTab> OwnerTab = TabManager->GetOwnerTab())
                    {
                        ApplyTabDiagnostics(OwnerTab,
                                            Context.RequestedWindowTitle.IsEmpty() ? TEXT("asset_path")
                                                                                   : TEXT("window_title_hint"));
                    }
                }
            }

            if (Context.TabId.IsEmpty() && !Context.RequestedTabId.IsEmpty())
            {
                Context.TabId = Context.RequestedTabId;
            }

            if (Context.WindowTitle.IsEmpty() && !Context.RequestedWindowTitle.IsEmpty())
            {
                Context.WindowTitle = Context.RequestedWindowTitle;
            }

            if (Context.ResolvedTargetSource.IsEmpty())
            {
                Context.ResolvedTargetSource = !Context.RequestedTabId.IsEmpty()
                                                   ? TEXT("tab_id_hint")
                                                   : (!Context.RequestedWindowTitle.IsEmpty() ? TEXT("window_title_hint")
                                                                                              : TEXT("asset_path"));
            }
        }

        TSharedPtr<SDesignerView> FindDesignerViewInWidgetTree(const TSharedRef<const SWidget> &RootWidget)
        {
            const TSharedRef<SWidget> MutableRootWidget = ConstCastSharedRef<SWidget>(RootWidget);

            if (RootWidget->GetTypeAsString().Contains(TEXT("SDesignerView"), ESearchCase::IgnoreCase))
            {
                return StaticCastSharedRef<SDesignerView>(MutableRootWidget);
            }

            const FChildren *Children = MutableRootWidget->GetAllChildren();
            if (Children == nullptr)
            {
                return nullptr;
            }

            for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
            {
                if (TSharedPtr<SDesignerView> FoundDesignerView =
                        FindDesignerViewInWidgetTree(Children->GetChildAt(ChildIndex)))
                {
                    return FoundDesignerView;
                }
            }

            return nullptr;
        }

        void ResolveLiveWidgetDesignerSurface(FWidgetBlueprintEditor *WidgetEditor,
                                              TSharedPtr<SDockTab> &OutDesignerTab,
                                              TSharedPtr<SDesignerView> &OutDesignerView)
        {
            OutDesignerTab.Reset();
            OutDesignerView.Reset();

            if (WidgetEditor == nullptr)
            {
                return;
            }

            const TSharedPtr<FTabManager> TabManager = WidgetEditor->GetAssociatedTabManager();
            if (!TabManager.IsValid())
            {
                return;
            }

            OutDesignerTab = TabManager->FindExistingLiveTab(FTabId(GMcpWidgetBlueprintDesignerTabId));
            if (!OutDesignerTab.IsValid())
            {
                return;
            }

            OutDesignerView = FindDesignerViewInWidgetTree(OutDesignerTab->GetContent());
        }
    }
#endif

    FString NormalizeWidgetDesignerPath(FString InPath)
    {
        InPath = InPath.TrimStartAndEnd();
        InPath.ReplaceInline(TEXT("\\"), TEXT("/"));

        while (InPath.StartsWith(TEXT("/")))
        {
            InPath = InPath.Mid(1);
        }

        while (InPath.EndsWith(TEXT("/")))
        {
            InPath.LeftChopInline(1, EAllowShrinking::No);
        }

        return InPath;
    }

    FString BuildWidgetDesignerPath(UWidget *Widget)
    {
        TArray<FString> Segments;
        for (UWidget *CurrentWidget = Widget; CurrentWidget != nullptr; CurrentWidget = CurrentWidget->GetParent())
        {
            Segments.Insert(CurrentWidget->GetName(), 0);
        }

        return FString::Join(Segments, TEXT("/"));
    }

    bool ResolveWidgetBlueprintEditorContext(const TSharedPtr<FJsonObject> &Payload,
                                             FMcpResolvedWidgetBlueprintEditorContext &OutContext,
                                             FString &OutErrorCode,
                                             FString &OutErrorMessage,
                                             bool bOpenEditorIfNeeded,
                                             bool bFocusEditor,
                                             bool bRequireLiveEditor)
    {
        OutErrorCode.Reset();
        OutErrorMessage.Reset();
        OutContext = FMcpResolvedWidgetBlueprintEditorContext();

#if !WITH_EDITOR
        OutErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
        OutErrorMessage = TEXT("Widget Blueprint editor helpers require editor builds");
        return false;
#else
        OutContext.AssetPath = GetTrimmedJsonString(Payload, TEXT("assetPath"));
        if (OutContext.AssetPath.IsEmpty())
        {
            OutContext.AssetPath = GetTrimmedJsonString(Payload, TEXT("widgetPath"));
        }
        OutContext.RequestedGraphName = GetTrimmedJsonString(Payload, TEXT("graphName"));
        OutContext.RequestedTabId = GetTrimmedJsonString(Payload, TEXT("tabId"));
        OutContext.RequestedWindowTitle = GetTrimmedJsonString(Payload, TEXT("windowTitle"));

        if (OutContext.AssetPath.IsEmpty())
        {
            OutErrorCode = TEXT("INVALID_ARGUMENT");
            OutErrorMessage = TEXT("assetPath or widgetPath required");
            return false;
        }

        if (!UEditorAssetLibrary::DoesAssetExist(OutContext.AssetPath))
        {
            OutErrorCode = TEXT("ASSET_NOT_FOUND");
            OutErrorMessage = TEXT("Asset not found");
            return false;
        }

        UObject *Asset = UEditorAssetLibrary::LoadAsset(OutContext.AssetPath);
        if (Asset == nullptr)
        {
            OutErrorCode = TEXT("LOAD_FAILED");
            OutErrorMessage = TEXT("Failed to load asset");
            return false;
        }

        UWidgetBlueprint *WidgetBlueprint = Cast<UWidgetBlueprint>(Asset);
        if (WidgetBlueprint == nullptr)
        {
            OutErrorCode = TEXT("INVALID_ASSET");
            OutErrorMessage = TEXT("Asset must resolve to a Widget Blueprint asset");
            return false;
        }

        OutContext.Asset = Asset;
        OutContext.Blueprint = WidgetBlueprint;
        OutContext.WidgetBlueprint = WidgetBlueprint;

        UAssetEditorSubsystem *AssetEditorSubsystem = GEditor != nullptr
                                                          ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()
                                                          : nullptr;
        if (AssetEditorSubsystem == nullptr)
        {
            if (bRequireLiveEditor || bOpenEditorIfNeeded)
            {
                OutErrorCode = TEXT("SUBSYSTEM_MISSING");
                OutErrorMessage = TEXT("AssetEditorSubsystem not available");
                return false;
            }

            return true;
        }

        IAssetEditorInstance *EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Asset, false);
        if (EditorInstance == nullptr && bOpenEditorIfNeeded)
        {
            if (!AssetEditorSubsystem->OpenEditorForAsset(Asset))
            {
                OutErrorCode = TEXT("OPEN_FAILED");
                OutErrorMessage = TEXT("Failed to open asset editor");
                return false;
            }

            OutContext.bOpenedAssetEditor = true;
            EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Asset, false);
        }

        if (EditorInstance == nullptr)
        {
            if (bRequireLiveEditor)
            {
                OutErrorCode = TEXT("EDITOR_NOT_FOUND");
                OutErrorMessage = TEXT("Failed to resolve a live Widget Blueprint editor");
                return false;
            }

            return true;
        }

        OutContext.EditorType = EditorInstance->GetEditorName().ToString();
        if (!OutContext.EditorType.Contains(TEXT("WidgetBlueprint"), ESearchCase::IgnoreCase))
        {
            OutErrorCode = TEXT("UNSUPPORTED_EDITOR");
            OutErrorMessage = FString::Printf(TEXT("Resolved editor '%s' is not Widget Blueprint-backed"),
                                              *OutContext.EditorType);
            return false;
        }

        if (bFocusEditor)
        {
            EditorInstance->FocusWindow(Asset);
        }

        OutContext.Editor = static_cast<FBlueprintEditor *>(EditorInstance);
        OutContext.WidgetEditor = static_cast<FWidgetBlueprintEditor *>(OutContext.Editor);
        if (OutContext.WidgetEditor == nullptr)
        {
            OutErrorCode = TEXT("EDITOR_NOT_FOUND");
            OutErrorMessage = TEXT("Failed to resolve a live Widget Blueprint editor");
            return false;
        }

        RefreshWidgetBlueprintSurfaceDiagnostics(OutContext);
        return true;
#endif
    }

    void RefreshWidgetBlueprintSurfaceDiagnostics(FMcpResolvedWidgetBlueprintEditorContext &Context)
    {
#if WITH_EDITOR
        Context.bLiveEditorContextFound = Context.WidgetEditor != nullptr;
        Context.CurrentMode = Context.WidgetEditor != nullptr ? Context.WidgetEditor->GetCurrentMode().ToString()
                                                              : Context.CurrentMode;
        Context.DesignerTab.Reset();
        Context.DesignerView.Reset();
        Context.bDesignerTabFound = false;
        Context.bDesignerViewFound = false;

        if (Context.WidgetEditor == nullptr)
        {
            return;
        }

        RefreshBlueprintEditorSurfaceDiagnostics(Context.WidgetEditor, Context);
        ResolveLiveWidgetDesignerSurface(Context.WidgetEditor, Context.DesignerTab, Context.DesignerView);

        Context.bDesignerTabFound = Context.DesignerTab.IsValid();
        Context.bDesignerViewFound = Context.DesignerView.IsValid();

        if (Context.DesignerTab.IsValid())
        {
            Context.TabId = GMcpWidgetBlueprintDesignerTabId.ToString();
            Context.ResolvedTargetSource = TEXT("designer_tab");
            if (const TSharedPtr<SWindow> ParentWindow = Context.DesignerTab->GetParentWindow())
            {
                Context.WindowTitle = ParentWindow->GetTitle().ToString();
            }
        }
#else
        Context.bLiveEditorContextFound = false;
        Context.bDesignerTabFound = false;
        Context.bDesignerViewFound = false;
#endif
    }

    void QueueWidgetBlueprintDesignerAction(
        FMcpResolvedWidgetBlueprintEditorContext &Context,
        TFunction<void(FWidgetBlueprintEditor *)> Action)
    {
#if WITH_EDITOR
        if (Context.WidgetEditor == nullptr)
        {
            return;
        }

        FWidgetBlueprintEditor *WidgetEditor = Context.WidgetEditor;
        Context.bQueuedDesignerAction = true;
        Context.WidgetEditor->AddPostDesignerLayoutAction(
            [WidgetEditor, Action = MoveTemp(Action)]() mutable
            {
                if (WidgetEditor != nullptr)
                {
                    Action(WidgetEditor);
                }
            });
#else
        (void)Context;
        (void)Action;
#endif
    }

    bool ResolveWidgetBlueprintTargetWidget(const TSharedPtr<FJsonObject> &Payload,
                                            FMcpResolvedWidgetBlueprintEditorContext &Context,
                                            UWidget *&OutWidget,
                                            FString &OutErrorCode,
                                            FString &OutErrorMessage)
    {
        OutWidget = nullptr;
        OutErrorCode.Reset();
        OutErrorMessage.Reset();

        const TSharedPtr<FJsonObject> SelectorObject = GetWidgetSelectorObject(Payload);
        Context.RequestedWidgetName = GetTrimmedJsonString(SelectorObject, TEXT("widgetName"));
        Context.RequestedWidgetPath = NormalizeWidgetDesignerPath(GetTrimmedJsonString(SelectorObject, TEXT("widgetPath")));
        Context.RequestedWidgetObjectPath = GetTrimmedJsonString(SelectorObject, TEXT("widgetObjectPath"));
        if (Context.RequestedWidgetObjectPath.IsEmpty())
        {
            Context.RequestedWidgetObjectPath = GetTrimmedJsonString(SelectorObject, TEXT("templateObjectPath"));
        }

        if (Context.RequestedWidgetName.IsEmpty() && Context.RequestedWidgetPath.IsEmpty() &&
            Context.RequestedWidgetObjectPath.IsEmpty())
        {
            OutErrorCode = TEXT("INVALID_ARGUMENT");
            OutErrorMessage = TEXT("widgetName, widgetPath, or widgetObjectPath required");
            return false;
        }

        if (Context.WidgetBlueprint == nullptr || Context.WidgetBlueprint->WidgetTree == nullptr)
        {
            OutErrorCode = TEXT("WIDGET_TREE_NOT_AVAILABLE");
            OutErrorMessage = TEXT("Widget Blueprint does not have a live widget tree");
            return false;
        }

        if (!Context.RequestedWidgetName.IsEmpty())
        {
            Context.WidgetSelectorType = TEXT("widgetName");
            Context.WidgetSelectorValue = Context.RequestedWidgetName;
        }
        else if (!Context.RequestedWidgetPath.IsEmpty())
        {
            Context.WidgetSelectorType = TEXT("widgetPath");
            Context.WidgetSelectorValue = Context.RequestedWidgetPath;
        }
        else
        {
            Context.WidgetSelectorType = TEXT("widgetObjectPath");
            Context.WidgetSelectorValue = Context.RequestedWidgetObjectPath;
        }

        TArray<UWidget *> AllWidgets;
        Context.WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
        if (Context.WidgetBlueprint->WidgetTree->RootWidget != nullptr)
        {
            AllWidgets.AddUnique(Context.WidgetBlueprint->WidgetTree->RootWidget);
        }

        TArray<UWidget *> Matches;
        for (UWidget *Widget : AllWidgets)
        {
            if (Widget == nullptr)
            {
                continue;
            }

            bool bMatches = false;
            if (Context.WidgetSelectorType == TEXT("widgetName"))
            {
                bMatches = Widget->GetName().Equals(Context.RequestedWidgetName, ESearchCase::IgnoreCase);
            }
            else if (Context.WidgetSelectorType == TEXT("widgetPath"))
            {
                bMatches = BuildWidgetDesignerPath(Widget).Equals(Context.RequestedWidgetPath,
                                                                  ESearchCase::IgnoreCase);
            }
            else
            {
                bMatches = Widget->GetPathName().Equals(Context.RequestedWidgetObjectPath,
                                                        ESearchCase::IgnoreCase);
            }

            if (bMatches)
            {
                Matches.Add(Widget);
            }
        }

        if (Matches.Num() == 0)
        {
            OutErrorCode = TEXT("WIDGET_NOT_FOUND");
            OutErrorMessage = FString::Printf(TEXT("Widget selector '%s' did not resolve to a widget"),
                                              *Context.WidgetSelectorValue);
            return false;
        }

        if (Matches.Num() > 1)
        {
            OutErrorCode = TEXT("AMBIGUOUS_WIDGET_SELECTOR");
            OutErrorMessage = FString::Printf(TEXT("Widget selector '%s' matched %d widgets"),
                                              *Context.WidgetSelectorValue,
                                              Matches.Num());
            return false;
        }

        OutWidget = Matches[0];
        Context.ResolvedWidgetName = OutWidget->GetName();
        Context.ResolvedWidgetPath = BuildWidgetDesignerPath(OutWidget);
        Context.ResolvedWidgetObjectPath = OutWidget->GetPathName();
        return true;
    }
}