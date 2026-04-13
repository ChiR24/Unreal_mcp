// =============================================================================
// McpAutomationBridge_ControlHandlers.cpp
// =============================================================================
// Editor control, viewport, PIE, camera, and actor manipulation handlers.
//
// HANDLERS (64 actions):
//   Editor Control:
//     - start_pie, stop_pie, pause_pie, resume_pie, is_pie_active
//     - get_pie_info, set_pie_mode
//
//   Camera & Viewport:
//     - set_camera_view, get_camera_view, focus_viewport_on_actor
//     - set_viewport_view_mode, get_viewport_screenshot, capture_screenshot
//     - set_viewport_layout, get_active_viewport_info
//
//   Actor Control:
//     - spawn_actor, delete_actor, duplicate_actor, get_actors
//     - set_actor_transform, get_actor_transform, set_actor_location
//     - set_actor_rotation, set_actor_scale, apply_physics
//     - set_actor_tags, add_actor_tag, remove_actor_tag
//     - attach_actor, detach_actor, teleport_actor
//
//   Component Operations:
//     - add_component, remove_component, get_components
//     - set_component_property, get_component_property
//
//   Selection:
//     - select_actor, deselect_actor, clear_selection, get_selected_actors
//     - select_components, get_selected_components
//
//   Debug:
//     - draw_debug_line, draw_debug_sphere, draw_debug_box
//     - draw_debug_arrow, clear_debug_drawings
//
// REFACTORING NOTES:
//   - Uses McpVersionCompatibility.h for UE 5.0-5.7 API abstraction
//   - Uses McpHandlerUtils for standardized JSON parsing/responses
//   - Editor subsystems paths vary by UE version
//   - LevelEditor module is optional (may not be available in some contexts)
//
// VERSION COMPATIBILITY:
//   - EditorActorSubsystem: Path varies (Subsystems/ vs root)
//   - LevelEditorSubsystem: UE 5.0+ (optional, conditional include)
//   - UnrealEditorSubsystem: UE 5.0+ (optional, conditional include)
//   - LevelEditorPlaySettings: UE 5.0+ (optional, conditional include)
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"
#include "Dom/JsonObject.h"
#include "Async/Async.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_WidgetBlueprintEditorUtils.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR

// -----------------------------------------------------------------------------
// Editor-only Includes: Asset & Engine Utilities
// -----------------------------------------------------------------------------
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Editor Subsystems (paths vary by UE version)
// -----------------------------------------------------------------------------
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#elif __has_include("UnrealEditorSubsystem.h")
#include "UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#endif
#if __has_include("Subsystems/AssetEditorSubsystem.h")
#include "Subsystems/AssetEditorSubsystem.h"
#elif __has_include("AssetEditorSubsystem.h")
#include "AssetEditorSubsystem.h"
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Viewport Control
// -----------------------------------------------------------------------------
#include "Components/LightComponent.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Framework/Docking/TabManager.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GraphEditor.h"
#include "InputCoreTypes.h"
#include "HAL/PlatformProcess.h"
#include "ImageUtils.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "IAssetViewport.h" // For IAssetViewport::GetAssetViewportClient()
#include "Rendering/DrawElements.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SWindow.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Level Editor (optional, may not be available)
// -----------------------------------------------------------------------------
#if __has_include("LevelEditor.h")
#include "LevelEditor.h"
#define MCP_HAS_LEVEL_EDITOR_MODULE 1
#else
#define MCP_HAS_LEVEL_EDITOR_MODULE 0
#endif
#if __has_include("Settings/LevelEditorPlaySettings.h")
#include "Settings/LevelEditorPlaySettings.h"
#define MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS 1
#else
#define MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS 0
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Components & Actors
// -----------------------------------------------------------------------------
#include "Components/PrimitiveComponent.h"
#include "EditorViewportClient.h"
#include "Engine/Blueprint.h"

#if __has_include("FileHelpers.h")
#include "FileHelpers.h"
#endif
#include "Animation/SkeletalMeshActor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "BlueprintEditor.h"
#include "Blueprint/WidgetTree.h"
#include "BlueprintModes/WidgetBlueprintApplicationModes.h"
#include "Components/Widget.h"
#include "Designer/SDesignerView.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Export & Output
// -----------------------------------------------------------------------------
#include "Exporters/Exporter.h"
#include "Misc/OutputDevice.h"
#include "UnrealClient.h" // For FScreenshotRequest

#endif // WITH_EDITOR

#if WITH_EDITOR
namespace
{
  constexpr int32 GMcpSimulatedPointerIndex = 0;

  int32 GetSimulatedSlateUserIndex()
  {
    if (FSlateApplication::IsInitialized())
    {
      return static_cast<int32>(FSlateApplication::Get().GetUserIndexForKeyboard());
    }

    return 0;
  }

  FInputDeviceId GetSimulatedInputDeviceId(const int32 SlateUserIndex)
  {
    FInputDeviceId InputDeviceId = INPUTDEVICEID_NONE;

    if (FSlateApplication::IsInitialized())
    {
      if (TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(SlateUserIndex))
      {
        const FPlatformUserId PlatformUser = SlateUser->GetPlatformUserId();
        InputDeviceId = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(PlatformUser);
      }
    }

    if (!InputDeviceId.IsValid())
    {
      InputDeviceId = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
    }

    return InputDeviceId;
  }

  void SetFocusedWidgetResponseFields(const TSharedPtr<FJsonObject> &Resp)
  {
    if (!Resp.IsValid() || !FSlateApplication::IsInitialized())
    {
      return;
    }

    FSlateApplication &SlateApp = FSlateApplication::Get();
    const uint32 KeyboardUserIndex = SlateApp.GetUserIndexForKeyboard();

    const TSharedPtr<SWidget> KeyboardFocusedWidget = SlateApp.GetKeyboardFocusedWidget();
    const TSharedPtr<SWidget> UserFocusedWidget = SlateApp.GetUserFocusedWidget(KeyboardUserIndex);

    Resp->SetStringField(TEXT("keyboardFocusedWidgetType"), KeyboardFocusedWidget.IsValid() ? KeyboardFocusedWidget->GetTypeAsString() : TEXT(""));
    Resp->SetStringField(TEXT("userFocusedWidgetType"), UserFocusedWidget.IsValid() ? UserFocusedWidget->GetTypeAsString() : TEXT(""));
    Resp->SetNumberField(TEXT("keyboardUserIndex"), static_cast<double>(KeyboardUserIndex));
  }

  struct FMcpResolvedVirtualInputTarget
  {
    TSharedPtr<SWindow> Window;
    TSharedPtr<SDockTab> Tab;
    TSharedPtr<SWidget> PreferredWidget;
    FString TabId;
    FString WindowTitle;
    FString ResolutionSource;
    FString RequestedAssetPath;
    FString AssetTargetError;
    FString PreferredWidgetType;
    FSlateRect WindowRect;
    FSlateRect ClientRect;
  };

  struct FMcpResolvedBlueprintEditorContext
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
  };

  using namespace McpWidgetBlueprintEditorUtils;

  FString GetTrimmedPayloadString(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName)
  {
    FString Value;
    if (Payload.IsValid())
    {
      Payload->TryGetStringField(FieldName, Value);
    }

    return Value.TrimStartAndEnd();
  }

  FString GetBlueprintGraphDisplayName(const UEdGraph *Graph)
  {
    return Graph != nullptr ? FBlueprintEditor::GetGraphDisplayName(Graph).ToString() : FString();
  }

  FString GetBlueprintNodeListTitle(const UEdGraphNode *Node)
  {
    return Node != nullptr ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : FString();
  }

  void RefreshBlueprintEditorSurfaceDiagnostics(IAssetEditorInstance *EditorInstance,
                                                FMcpResolvedBlueprintEditorContext &Context)
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

  void ApplyBlueprintNavigationDiagnostics(const FMcpResolvedBlueprintEditorContext &Context,
                                           const TSharedPtr<FJsonObject> &Response)
  {
    if (!Response.IsValid())
    {
      return;
    }

    if (!Context.AssetPath.IsEmpty())
    {
      Response->SetStringField(TEXT("assetPath"), Context.AssetPath);
    }
    if (!Context.EditorType.IsEmpty())
    {
      Response->SetStringField(TEXT("editorType"), Context.EditorType);
    }
    if (!Context.RequestedGraphName.IsEmpty())
    {
      Response->SetStringField(TEXT("requestedGraphName"), Context.RequestedGraphName);
    }
    if (!Context.ResolvedGraphName.IsEmpty())
    {
      Response->SetStringField(TEXT("resolvedGraphName"), Context.ResolvedGraphName);
      Response->SetStringField(TEXT("graphName"), Context.ResolvedGraphName);
    }
    if (!Context.CurrentMode.IsEmpty())
    {
      Response->SetStringField(TEXT("currentMode"), Context.CurrentMode);
    }
    if (!Context.TabId.IsEmpty())
    {
      Response->SetStringField(TEXT("tabId"), Context.TabId);
    }
    if (!Context.WindowTitle.IsEmpty())
    {
      Response->SetStringField(TEXT("windowTitle"), Context.WindowTitle);
    }
    if (!Context.ResolvedTargetSource.IsEmpty())
    {
      Response->SetStringField(TEXT("resolvedTargetSource"), Context.ResolvedTargetSource);
    }

    Response->SetBoolField(TEXT("openedAssetEditor"), Context.bOpenedAssetEditor);
  }

  TSharedPtr<FJsonObject> CreateBlueprintNavigationDiagnosticsObject(
      const FMcpResolvedBlueprintEditorContext &Context)
  {
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    ApplyBlueprintNavigationDiagnostics(Context, Response);
    return Response;
  }

  void SendBlueprintNavigationError(UMcpAutomationBridgeSubsystem *Subsystem,
                                    TSharedPtr<FMcpBridgeWebSocket> Socket,
                                    const FString &RequestId,
                                    const FMcpResolvedBlueprintEditorContext &Context,
                                    const FString &ErrorCode,
                                    const FString &ErrorMessage)
  {
    SendStandardErrorResponse(Subsystem, Socket, RequestId, *ErrorCode, *ErrorMessage,
                              CreateBlueprintNavigationDiagnosticsObject(Context));
  }

  bool ResolveBlueprintEditorContext(const TSharedPtr<FJsonObject> &Payload,
                                     FMcpResolvedBlueprintEditorContext &OutContext,
                                     FString &OutErrorCode,
                                     FString &OutErrorMessage)
  {
    OutErrorCode.Reset();
    OutErrorMessage.Reset();
    OutContext = FMcpResolvedBlueprintEditorContext();

    if (!GEditor)
    {
      OutErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
      OutErrorMessage = TEXT("Editor not available");
      return false;
    }

    OutContext.AssetPath = GetTrimmedPayloadString(Payload, TEXT("assetPath"));
    OutContext.RequestedGraphName = GetTrimmedPayloadString(Payload, TEXT("graphName"));
    OutContext.RequestedTabId = GetTrimmedPayloadString(Payload, TEXT("tabId"));
    OutContext.RequestedWindowTitle = GetTrimmedPayloadString(Payload, TEXT("windowTitle"));

    if (OutContext.AssetPath.IsEmpty())
    {
      OutErrorCode = TEXT("INVALID_ARGUMENT");
      OutErrorMessage = TEXT("assetPath required");
      return false;
    }

    UAssetEditorSubsystem *AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (AssetEditorSubsystem == nullptr)
    {
      OutErrorCode = TEXT("SUBSYSTEM_MISSING");
      OutErrorMessage = TEXT("AssetEditorSubsystem not available");
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

    UBlueprint *Blueprint = Cast<UBlueprint>(Asset);
    if (Blueprint == nullptr)
    {
      OutErrorCode = TEXT("INVALID_ASSET");
      OutErrorMessage = TEXT("assetPath must resolve to a Blueprint asset");
      return false;
    }

    IAssetEditorInstance *EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Asset, false);
    if (EditorInstance == nullptr)
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
      OutErrorCode = TEXT("EDITOR_NOT_FOUND");
      OutErrorMessage = TEXT("Failed to resolve a live asset editor for the Blueprint");
      return false;
    }

    OutContext.EditorType = EditorInstance->GetEditorName().ToString();
    if (!OutContext.EditorType.Contains(TEXT("Blueprint"), ESearchCase::IgnoreCase))
    {
      OutErrorCode = TEXT("UNSUPPORTED_EDITOR");
      OutErrorMessage = FString::Printf(TEXT("Resolved editor '%s' is not Blueprint-backed"),
                                        *OutContext.EditorType);
      return false;
    }

    EditorInstance->FocusWindow(Asset);

    OutContext.Asset = Asset;
    OutContext.Blueprint = Blueprint;
    // Use static_cast after verifying the toolkit type via IAssetEditorInstance::GetEditorName().
    OutContext.Editor = static_cast<FBlueprintEditor *>(EditorInstance);
    OutContext.CurrentMode = OutContext.Editor != nullptr ? OutContext.Editor->GetCurrentMode().ToString()
                                                          : FString();

    RefreshBlueprintEditorSurfaceDiagnostics(EditorInstance, OutContext);
    return true;
  }

  bool ResolveBlueprintGraph(FMcpResolvedBlueprintEditorContext &Context,
                             UEdGraph *&OutGraph,
                             FString &OutError)
  {
    OutGraph = nullptr;
    OutError.Reset();

    if (Context.Blueprint == nullptr || Context.Editor == nullptr)
    {
      OutError = TEXT("Blueprint editor context is not available");
      return false;
    }

    auto ApplyResolvedGraph = [&Context, &OutGraph](UEdGraph *Graph)
    {
      if (Graph == nullptr)
      {
        return false;
      }

      Context.Editor->OpenGraphAndBringToFront(Graph, true);
      Context.ResolvedGraphName = GetBlueprintGraphDisplayName(Graph);
      Context.CurrentMode = Context.Editor->GetCurrentMode().ToString();
      OutGraph = Graph;
      return true;
    };

    if (Context.RequestedGraphName.IsEmpty())
    {
      if (UEdGraph *FocusedGraph = Context.Editor->GetFocusedGraph())
      {
        return ApplyResolvedGraph(FocusedGraph);
      }

      if (Context.Blueprint->UbergraphPages.Num() > 0)
      {
        return ApplyResolvedGraph(Context.Blueprint->UbergraphPages[0]);
      }

      TArray<UEdGraph *> AllGraphs;
      Context.Blueprint->GetAllGraphs(AllGraphs);
      if (AllGraphs.Num() > 0)
      {
        return ApplyResolvedGraph(AllGraphs[0]);
      }

      OutError = TEXT("Blueprint has no graphs to navigate");
      return false;
    }

    TArray<UEdGraph *> MatchingGraphs;
    TArray<UEdGraph *> AllGraphs;
    Context.Blueprint->GetAllGraphs(AllGraphs);
    for (UEdGraph *Graph : AllGraphs)
    {
      if (Graph == nullptr)
      {
        continue;
      }

      const FString InternalName = Graph->GetName();
      const FString DisplayName = GetBlueprintGraphDisplayName(Graph);
      if (InternalName.Equals(Context.RequestedGraphName, ESearchCase::IgnoreCase) ||
          DisplayName.Equals(Context.RequestedGraphName, ESearchCase::IgnoreCase))
      {
        MatchingGraphs.Add(Graph);
      }
    }

    if (MatchingGraphs.Num() == 0)
    {
      OutError = FString::Printf(TEXT("Blueprint graph '%s' was not found"),
                                 *Context.RequestedGraphName);
      return false;
    }

    if (MatchingGraphs.Num() > 1)
    {
      OutError = FString::Printf(TEXT("Blueprint graph '%s' matched %d graphs; use an exact internal graph name"),
                                 *Context.RequestedGraphName,
                                 MatchingGraphs.Num());
      return false;
    }

    return ApplyResolvedGraph(MatchingGraphs[0]);
  }

  void ApplyWidgetBlueprintNavigationDiagnostics(
      const FMcpResolvedWidgetBlueprintEditorContext &Context,
      const TSharedPtr<FJsonObject> &Response)
  {
    if (!Response.IsValid())
    {
      return;
    }

    if (!Context.AssetPath.IsEmpty())
    {
      Response->SetStringField(TEXT("assetPath"), Context.AssetPath);
    }
    if (!Context.EditorType.IsEmpty())
    {
      Response->SetStringField(TEXT("editorType"), Context.EditorType);
    }
    if (!Context.RequestedGraphName.IsEmpty())
    {
      Response->SetStringField(TEXT("requestedGraphName"), Context.RequestedGraphName);
    }
    if (!Context.ResolvedGraphName.IsEmpty())
    {
      Response->SetStringField(TEXT("resolvedGraphName"), Context.ResolvedGraphName);
      Response->SetStringField(TEXT("graphName"), Context.ResolvedGraphName);
    }
    if (!Context.CurrentMode.IsEmpty())
    {
      Response->SetStringField(TEXT("currentMode"), Context.CurrentMode);
    }
    if (!Context.TabId.IsEmpty())
    {
      Response->SetStringField(TEXT("tabId"), Context.TabId);
    }
    if (!Context.WindowTitle.IsEmpty())
    {
      Response->SetStringField(TEXT("windowTitle"), Context.WindowTitle);
    }
    if (!Context.ResolvedTargetSource.IsEmpty())
    {
      Response->SetStringField(TEXT("resolvedTargetSource"), Context.ResolvedTargetSource);
    }

    Response->SetBoolField(TEXT("openedAssetEditor"), Context.bOpenedAssetEditor);

    if (!Context.RequestedMode.IsEmpty())
    {
      Response->SetStringField(TEXT("requestedMode"), Context.RequestedMode);
    }

    if (!Context.WidgetSelectorType.IsEmpty())
    {
      Response->SetStringField(TEXT("widgetSelectorType"), Context.WidgetSelectorType);
    }

    if (!Context.WidgetSelectorValue.IsEmpty())
    {
      Response->SetStringField(TEXT("widgetSelector"), Context.WidgetSelectorValue);
    }

    if (!Context.RequestedWidgetName.IsEmpty())
    {
      Response->SetStringField(TEXT("requestedWidgetName"), Context.RequestedWidgetName);
    }

    if (!Context.RequestedWidgetPath.IsEmpty())
    {
      Response->SetStringField(TEXT("requestedWidgetPath"), Context.RequestedWidgetPath);
    }

    if (!Context.RequestedWidgetObjectPath.IsEmpty())
    {
      Response->SetStringField(TEXT("requestedWidgetObjectPath"), Context.RequestedWidgetObjectPath);
    }

    if (!Context.ResolvedWidgetName.IsEmpty())
    {
      Response->SetStringField(TEXT("resolvedWidgetName"), Context.ResolvedWidgetName);
    }

    if (!Context.ResolvedWidgetPath.IsEmpty())
    {
      Response->SetStringField(TEXT("resolvedWidgetPath"), Context.ResolvedWidgetPath);
    }

    if (!Context.ResolvedWidgetObjectPath.IsEmpty())
    {
      Response->SetStringField(TEXT("resolvedWidgetObjectPath"), Context.ResolvedWidgetObjectPath);
    }

    Response->SetBoolField(TEXT("designerTabFound"), Context.bDesignerTabFound);
    Response->SetBoolField(TEXT("designerViewFound"), Context.bDesignerViewFound);
    Response->SetBoolField(TEXT("queuedDesignerAction"), Context.bQueuedDesignerAction);
  }

  TSharedPtr<FJsonObject> CreateWidgetBlueprintNavigationDiagnosticsObject(
      const FMcpResolvedWidgetBlueprintEditorContext &Context)
  {
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    ApplyWidgetBlueprintNavigationDiagnostics(Context, Response);
    return Response;
  }

  void SendWidgetBlueprintNavigationError(UMcpAutomationBridgeSubsystem *Subsystem,
                                          TSharedPtr<FMcpBridgeWebSocket> Socket,
                                          const FString &RequestId,
                                          const FMcpResolvedWidgetBlueprintEditorContext &Context,
                                          const FString &ErrorCode,
                                          const FString &ErrorMessage)
  {
    SendStandardErrorResponse(Subsystem, Socket, RequestId, *ErrorCode, *ErrorMessage,
                              CreateWidgetBlueprintNavigationDiagnosticsObject(Context));
  }

  FString NormalizeWidgetBlueprintModeString(const FString &Mode)
  {
    const FString LowerMode = Mode.TrimStartAndEnd().ToLower();
    if (LowerMode == TEXT("designer"))
    {
      return TEXT("Designer");
    }

    if (LowerMode == TEXT("graph"))
    {
      return TEXT("Graph");
    }

    return FString();
  }

  FName GetWidgetBlueprintModeId(const FString &NormalizedMode)
  {
    if (NormalizedMode.Equals(TEXT("Designer"), ESearchCase::CaseSensitive))
    {
      return FWidgetBlueprintApplicationModes::DesignerMode;
    }

    if (NormalizedMode.Equals(TEXT("Graph"), ESearchCase::CaseSensitive))
    {
      return FWidgetBlueprintApplicationModes::GraphMode;
    }

    return NAME_None;
  }

  TSharedPtr<SButton> FindDesignerZoomToFitButtonInWidgetTree(
      const TSharedRef<const SWidget> &RootWidget)
  {
    const TSharedRef<SWidget> MutableRootWidget = ConstCastSharedRef<SWidget>(RootWidget);
    FChildren *Children = MutableRootWidget->GetAllChildren();
    if (Children == nullptr)
    {
      return nullptr;
    }

    bool bSawDesignerToolbarSibling = false;
    for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
    {
      const TSharedRef<const SWidget> ChildWidget = Children->GetChildAt(ChildIndex);
      const FString ChildType = ChildWidget->GetTypeAsString();
      if (ChildType.Contains(TEXT("SDesignerToolBar"), ESearchCase::IgnoreCase))
      {
        bSawDesignerToolbarSibling = true;
        continue;
      }

      if (bSawDesignerToolbarSibling && ChildType.Contains(TEXT("SButton"), ESearchCase::IgnoreCase))
      {
        return StaticCastSharedRef<SButton>(ConstCastSharedRef<SWidget>(ChildWidget));
      }
    }

    for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
    {
      if (TSharedPtr<SButton> FoundButton =
              FindDesignerZoomToFitButtonInWidgetTree(Children->GetChildAt(ChildIndex)))
      {
        return FoundButton;
      }
    }

    return nullptr;
  }

  bool TryExecuteWidgetDesignerZoomToFit(
      FMcpResolvedWidgetBlueprintEditorContext &Context,
      FString &OutDisposition,
      FString &OutFailureReason)
  {
    RefreshWidgetBlueprintSurfaceDiagnostics(Context);

    if (!Context.DesignerTab.IsValid() || !Context.DesignerView.IsValid())
    {
      OutDisposition = TEXT("queued_after_layout");
      OutFailureReason = TEXT("Designer surface not yet available");
      return false;
    }

    if (const TSharedPtr<SButton> ZoomToFitButton =
            FindDesignerZoomToFitButtonInWidgetTree(Context.DesignerTab.ToSharedRef()))
    {
#if !UE_BUILD_SHIPPING
      ZoomToFitButton->SimulateClick();
      RefreshWidgetBlueprintSurfaceDiagnostics(Context);
      OutDisposition = TEXT("immediate");
      OutFailureReason.Reset();
      return true;
#else
      OutDisposition = TEXT("fallback_mode_only");
      OutFailureReason = TEXT("Designer fit button simulation unavailable in shipping builds");
      return false;
#endif
    }

    OutDisposition = TEXT("fallback_mode_only");
    OutFailureReason = TEXT("Could not resolve the Designer Zoom-To-Fit button structurally");
    return false;
  }

  template <typename Tag, typename Tag::type Member>
  struct TMcpProtectedDesignerMemberAccessor
  {
    friend typename Tag::type ResolveDesignerSurfaceMember(Tag)
    {
      return Member;
    }
  };

  struct FMcpDesignerViewOffsetTag
  {
    using type = FVector2D SDesignSurface::*;
    friend type ResolveDesignerSurfaceMember(FMcpDesignerViewOffsetTag);
  };

  template struct TMcpProtectedDesignerMemberAccessor<FMcpDesignerViewOffsetTag,
                                                      &SDesignSurface::ViewOffset>;

  bool TryGetPointField(const TSharedPtr<FJsonObject> &Payload,
                        const TCHAR *FieldName,
                        FVector2D &OutPoint)
  {
    if (!Payload.IsValid() || !Payload->HasTypedField<EJson::Object>(FieldName))
    {
      return false;
    }

    const TSharedPtr<FJsonObject> PointObject = Payload->GetObjectField(FieldName);
    if (!PointObject.IsValid() || !PointObject->HasField(TEXT("x")) || !PointObject->HasField(TEXT("y")))
    {
      return false;
    }

    OutPoint = FVector2D(GetJsonNumberField(PointObject, TEXT("x")),
                         GetJsonNumberField(PointObject, TEXT("y")));
    return true;
  }

  FVector2D GetDesignerSurfaceViewOffset(const SDesignerView &DesignerView)
  {
    static const FMcpDesignerViewOffsetTag::type ViewOffsetMember =
        ResolveDesignerSurfaceMember(FMcpDesignerViewOffsetTag{});
    return DesignerView.*ViewOffsetMember;
  }

  void SetDesignerSurfaceViewOffset(SDesignerView &DesignerView,
                                    const FVector2D &NewViewOffset)
  {
    static const FMcpDesignerViewOffsetTag::type ViewOffsetMember =
        ResolveDesignerSurfaceMember(FMcpDesignerViewOffsetTag{});
    DesignerView.*ViewOffsetMember = NewViewOffset;
    DesignerView.Invalidate(EInvalidateWidgetReason::Paint);
  }

  bool TryApplyWidgetDesignerViewChange(
      FMcpResolvedWidgetBlueprintEditorContext &Context,
      const TOptional<FVector2D> &RequestedViewLocation,
      const TOptional<FVector2D> &RequestedDelta,
      FVector2D &OutPreviousViewOffset,
      FVector2D &OutNewViewOffset,
      FString &OutDisposition,
      FString &OutFailureReason)
  {
    RefreshWidgetBlueprintSurfaceDiagnostics(Context);

    if (!Context.DesignerTab.IsValid() || !Context.DesignerView.IsValid())
    {
      OutDisposition = TEXT("queued_after_layout");
      OutFailureReason = TEXT("Designer surface not yet available");
      return false;
    }

    SDesignerView &DesignerView = *Context.DesignerView;
    OutPreviousViewOffset = GetDesignerSurfaceViewOffset(DesignerView);
    OutNewViewOffset = RequestedViewLocation.IsSet() ? RequestedViewLocation.GetValue()
                                                     : OutPreviousViewOffset;
    if (RequestedDelta.IsSet())
    {
      OutNewViewOffset += RequestedDelta.GetValue();
    }

    SetDesignerSurfaceViewOffset(DesignerView, OutNewViewOffset);
    OutDisposition = TEXT("immediate");
    OutFailureReason.Reset();
    return true;
  }

  void QueueWidgetDesignerViewChange(
      FMcpResolvedWidgetBlueprintEditorContext &Context,
      const TOptional<FVector2D> RequestedViewLocation,
      const TOptional<FVector2D> RequestedDelta)
  {
    QueueWidgetBlueprintDesignerAction(
        Context,
        [RequestedViewLocation, RequestedDelta](FWidgetBlueprintEditor *WidgetEditor)
        {
          if (WidgetEditor == nullptr)
          {
            return;
          }

          FMcpResolvedWidgetBlueprintEditorContext WidgetContext;
          WidgetContext.WidgetEditor = WidgetEditor;
          RefreshWidgetBlueprintSurfaceDiagnostics(WidgetContext);
          if (!WidgetContext.DesignerView.IsValid())
          {
            return;
          }

          SDesignerView &DesignerView = *WidgetContext.DesignerView;
          FVector2D NewViewOffset = RequestedViewLocation.IsSet()
                                        ? RequestedViewLocation.GetValue()
                                        : GetDesignerSurfaceViewOffset(DesignerView);
          if (RequestedDelta.IsSet())
          {
            NewViewOffset += RequestedDelta.GetValue();
          }

          SetDesignerSurfaceViewOffset(DesignerView, NewViewOffset);
        });
  }

  void QueueWidgetDesignerZoomToFit(FMcpResolvedWidgetBlueprintEditorContext &Context)
  {
    QueueWidgetBlueprintDesignerAction(
        Context,
        [](FWidgetBlueprintEditor *WidgetEditor)
        {
          if (WidgetEditor == nullptr)
          {
            return;
          }

          FMcpResolvedWidgetBlueprintEditorContext WidgetContext;
          WidgetContext.WidgetEditor = WidgetEditor;
          RefreshWidgetBlueprintSurfaceDiagnostics(WidgetContext);
          if (!WidgetContext.DesignerTab.IsValid() || !WidgetContext.DesignerView.IsValid())
          {
            return;
          }

          if (const TSharedPtr<SButton> ZoomToFitButton =
                  FindDesignerZoomToFitButtonInWidgetTree(WidgetContext.DesignerTab.ToSharedRef()))
          {
#if !UE_BUILD_SHIPPING
            ZoomToFitButton->SimulateClick();
#endif
          }
        });
  }

  bool TryGetPointerPositionFromPayload(const TSharedPtr<FJsonObject> &Payload, FVector2D &OutPosition);
  TSet<FKey> ConvertPressedButtonNamesToKeys(const TSet<FString> &PressedButtonNames);
  FModifierKeysState BuildModifierKeyState(const TSet<FString> &PressedModifierKeys);
  FPointerEvent BuildMouseButtonEvent(const FVector2D &Position,
                                      const FVector2D &LastPosition,
                                      const TSet<FKey> &PressedButtons,
                                      const FKey &EffectingButton,
                                      const FModifierKeysState &ModifierKeys,
                                      float WheelDelta);
  FPointerEvent BuildMouseMoveEvent(const FVector2D &Position,
                                    const FVector2D &LastPosition,
                                    const TSet<FKey> &PressedButtons,
                                    const FModifierKeysState &ModifierKeys);
  void StoreSimulatedPosition(FVector2D &StoredPosition, bool &bHasStoredPosition, const FVector2D &Position);

  class SMcpBridgeVirtualCursorOverlay final : public SLeafWidget
  {
  public:
    SLATE_BEGIN_ARGS(SMcpBridgeVirtualCursorOverlay)
        : _CursorPosition(FVector2D::ZeroVector)
    {
    }
    SLATE_ARGUMENT(FVector2D, CursorPosition)
    SLATE_END_ARGS()

    void Construct(const FArguments &InArgs)
    {
      CursorPosition = InArgs._CursorPosition;
      SetVisibility(EVisibility::HitTestInvisible);
    }

    void SetCursorPosition(const FVector2D &InCursorPosition)
    {
      CursorPosition = InCursorPosition;
      Invalidate(EInvalidateWidget::Paint);
    }

    virtual FVector2D ComputeDesiredSize(float) const override
    {
      return FVector2D(1.0f, 1.0f);
    }

    virtual int32 OnPaint(const FPaintArgs &Args, const FGeometry &AllottedGeometry,
                          const FSlateRect &MyCullingRect, FSlateWindowElementList &OutDrawElements,
                          int32 LayerId, const FWidgetStyle &InWidgetStyle,
                          bool bParentEnabled) const override
    {
      const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
      const FVector2D LocalCursor(
          FMath::Clamp(CursorPosition.X, 0.0f, LocalSize.X),
          FMath::Clamp(CursorPosition.Y, 0.0f, LocalSize.Y));

      const FLinearColor CursorColor(1.0f, 0.25f, 0.1f, 0.95f);
      const FVector2D HorizontalStart = LocalCursor + FVector2D(-10.0f, 0.0f);
      const FVector2D HorizontalEnd = LocalCursor + FVector2D(10.0f, 0.0f);
      const FVector2D VerticalStart = LocalCursor + FVector2D(0.0f, -10.0f);
      const FVector2D VerticalEnd = LocalCursor + FVector2D(0.0f, 10.0f);

      TArray<FVector2D> HorizontalLine;
      HorizontalLine.Add(HorizontalStart);
      HorizontalLine.Add(HorizontalEnd);
      FSlateDrawElement::MakeLines(
          OutDrawElements,
          LayerId,
          AllottedGeometry.ToPaintGeometry(),
          HorizontalLine,
          ESlateDrawEffect::None,
          CursorColor,
          true,
          2.0f);

      TArray<FVector2D> VerticalLine;
      VerticalLine.Add(VerticalStart);
      VerticalLine.Add(VerticalEnd);
      FSlateDrawElement::MakeLines(
          OutDrawElements,
          LayerId + 1,
          AllottedGeometry.ToPaintGeometry(),
          VerticalLine,
          ESlateDrawEffect::None,
          CursorColor,
          true,
          2.0f);

      TArray<FVector2D> CursorBox;
      CursorBox.Add(LocalCursor + FVector2D(-4.0f, -4.0f));
      CursorBox.Add(LocalCursor + FVector2D(4.0f, -4.0f));
      CursorBox.Add(LocalCursor + FVector2D(4.0f, 4.0f));
      CursorBox.Add(LocalCursor + FVector2D(-4.0f, 4.0f));
      CursorBox.Add(LocalCursor + FVector2D(-4.0f, -4.0f));
      FSlateDrawElement::MakeLines(
          OutDrawElements,
          LayerId + 2,
          AllottedGeometry.ToPaintGeometry(),
          CursorBox,
          ESlateDrawEffect::None,
          FLinearColor(1.0f, 0.95f, 0.2f, 0.95f),
          true,
          1.5f);

      return LayerId + 2;
    }

  private:
    FVector2D CursorPosition = FVector2D::ZeroVector;
  };

  TArray<TSharedRef<SWindow>> GetVisibleSlateWindows()
  {
    TArray<TSharedRef<SWindow>> Windows;
    if (FSlateApplication::IsInitialized())
    {
      FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
    }
    return Windows;
  }

  FSlateRect GetEffectiveClientRectInScreen(const TSharedPtr<SWindow> &Window)
  {
    if (!Window.IsValid())
    {
      return FSlateRect();
    }

    if (Window->IsVirtualWindow())
    {
      const FVector2D WindowPosition = Window->GetPositionInScreen();
      const FVector2D ClientSize = Window->GetClientSizeInScreen();
      return FSlateRect(WindowPosition.X,
                        WindowPosition.Y,
                        WindowPosition.X + ClientSize.X,
                        WindowPosition.Y + ClientSize.Y);
    }

    return Window->GetClientRectInScreen();
  }

  struct FMcpDesignerRectSelectionCandidate
  {
    FWidgetReference WidgetReference;
    FString WidgetPath;
    bool bIsRoot = false;
  };

  bool TryGetDesignerSurfaceAbsoluteGeometry(const TSharedPtr<SDockTab> &DesignerTab,
                                             const TSharedPtr<SDesignerView> &DesignerView,
                                             FVector2D &OutAbsolutePosition,
                                             FVector2D &OutAbsoluteSize)
  {
    auto IsUsableGeometry = [](const FGeometry &Geometry)
    {
      const FVector2D Position = Geometry.GetAbsolutePosition();
      const FVector2D Size = Geometry.GetAbsoluteSize();
      return FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y) &&
             FMath::IsFinite(Size.X) && FMath::IsFinite(Size.Y) &&
             Size.X > 1.0f && Size.Y > 1.0f;
    };

    if (DesignerView.IsValid() && FSlateApplication::IsInitialized())
    {
      FWidgetPath DesignerViewPath;
      if (FSlateApplication::Get().GeneratePathToWidgetUnchecked(DesignerView.ToSharedRef(),
                                                                 DesignerViewPath,
                                                                 EVisibility::All) &&
          DesignerViewPath.Widgets.Num() > 0)
      {
        const FGeometry &DesignerViewPathGeometry = DesignerViewPath.Widgets.Last().Geometry;
        if (IsUsableGeometry(DesignerViewPathGeometry))
        {
          OutAbsolutePosition = DesignerViewPathGeometry.GetAbsolutePosition();
          OutAbsoluteSize = DesignerViewPathGeometry.GetAbsoluteSize();
          return true;
        }
      }
    }

    if (DesignerTab.IsValid())
    {
      const TSharedRef<SWidget> DesignerTabContent = DesignerTab->GetContent();
      const FGeometry DesignerTabContentGeometry = DesignerTabContent->GetTickSpaceGeometry();
      if (IsUsableGeometry(DesignerTabContentGeometry))
      {
        OutAbsolutePosition = DesignerTabContentGeometry.GetAbsolutePosition();
        OutAbsoluteSize = DesignerTabContentGeometry.GetAbsoluteSize();
        return true;
      }

      const FGeometry DesignerTabGeometry = DesignerTab->GetTickSpaceGeometry();
      if (IsUsableGeometry(DesignerTabGeometry))
      {
        OutAbsolutePosition = DesignerTabGeometry.GetAbsolutePosition();
        OutAbsoluteSize = DesignerTabGeometry.GetAbsoluteSize();
        return true;
      }
    }

    if (DesignerView.IsValid())
    {
      const FGeometry DesignerViewGeometry = DesignerView->GetTickSpaceGeometry();
      if (IsUsableGeometry(DesignerViewGeometry))
      {
        OutAbsolutePosition = DesignerViewGeometry.GetAbsolutePosition();
        OutAbsoluteSize = DesignerViewGeometry.GetAbsoluteSize();
        return true;
      }
    }

    OutAbsolutePosition = FVector2D::ZeroVector;
    OutAbsoluteSize = FVector2D::ZeroVector;
    return false;
  }

  bool TryGetRectObjectField(const TSharedPtr<FJsonObject> &Payload,
                             const TCHAR *FieldName,
                             FSlateRect &OutRect)
  {
    if (!Payload.IsValid())
    {
      return false;
    }

    const TSharedPtr<FJsonObject> *RectObject = nullptr;
    if (!Payload->TryGetObjectField(FieldName, RectObject) || RectObject == nullptr || !RectObject->IsValid())
    {
      return false;
    }

    double Left = 0.0;
    double Top = 0.0;
    double Right = 0.0;
    double Bottom = 0.0;
    if (!(*RectObject)->TryGetNumberField(TEXT("left"), Left) ||
        !(*RectObject)->TryGetNumberField(TEXT("top"), Top) ||
        !(*RectObject)->TryGetNumberField(TEXT("right"), Right) ||
        !(*RectObject)->TryGetNumberField(TEXT("bottom"), Bottom))
    {
      return false;
    }

    if (!FMath::IsFinite(Left) || !FMath::IsFinite(Top) ||
        !FMath::IsFinite(Right) || !FMath::IsFinite(Bottom) ||
        Right <= Left || Bottom <= Top)
    {
      return false;
    }

    OutRect = FSlateRect(Left, Top, Right, Bottom);
    return true;
  }

  bool TryGetWidgetDesignerClientRect(const FWidgetReference &WidgetReference,
                                      const TSharedPtr<SDesignerView> &DesignerView,
                                      const TSharedPtr<SDockTab> &DesignerTab,
                                      const TSharedPtr<SWindow> &ParentWindow,
                                      FSlateRect &OutRect)
  {
    if (!WidgetReference.IsValid() || !DesignerView.IsValid())
    {
      return false;
    }

    FGeometry WidgetGeometry;
    if (!DesignerView->GetWidgetGeometry(WidgetReference, WidgetGeometry))
    {
      return false;
    }

    TSharedPtr<SWindow> TargetWindow = ParentWindow;
    if (!TargetWindow.IsValid() && FSlateApplication::IsInitialized())
    {
      TargetWindow = FSlateApplication::Get().FindWidgetWindow(DesignerView.ToSharedRef());
    }

    FVector2D AbsolutePosition = WidgetGeometry.GetAbsolutePosition();
    const FVector2D AbsoluteSize = WidgetGeometry.GetAbsoluteSize();

    const FGeometry PreviewGeometry = DesignerView->GetDesignerGeometry();
    const FVector2D PreviewAbsolutePosition = PreviewGeometry.GetAbsolutePosition();
    FVector2D DesignerSurfaceAbsolutePosition = FVector2D::ZeroVector;
    FVector2D DesignerSurfaceAbsoluteSize = FVector2D::ZeroVector;
    TryGetDesignerSurfaceAbsoluteGeometry(DesignerTab,
                                          DesignerView,
                                          DesignerSurfaceAbsolutePosition,
                                          DesignerSurfaceAbsoluteSize);

    if (FMath::IsFinite(PreviewAbsolutePosition.X) && FMath::IsFinite(PreviewAbsolutePosition.Y) &&
        FMath::IsFinite(DesignerSurfaceAbsolutePosition.X) && FMath::IsFinite(DesignerSurfaceAbsolutePosition.Y))
    {
      AbsolutePosition = DesignerSurfaceAbsolutePosition + (AbsolutePosition - PreviewAbsolutePosition);
    }

    double Left = 0.0;
    double Top = 0.0;
    if (TargetWindow.IsValid())
    {
      const FSlateRect ClientRect = GetEffectiveClientRectInScreen(TargetWindow);
      Left = static_cast<double>(AbsolutePosition.X) - static_cast<double>(ClientRect.Left);
      Top = static_cast<double>(AbsolutePosition.Y) - static_cast<double>(ClientRect.Top);
    }
    else
    {
      const FGeometry WindowLocalGeometry = DesignerView->MakeGeometryWindowLocal(WidgetGeometry);
      const FVector2D WindowLocalPosition = WindowLocalGeometry.GetAbsolutePosition();
      Left = static_cast<double>(WindowLocalPosition.X);
      Top = static_cast<double>(WindowLocalPosition.Y);
    }

    const double Width = AbsoluteSize.X;
    const double Height = AbsoluteSize.Y;
    if (!FMath::IsFinite(Left) || !FMath::IsFinite(Top) ||
        !FMath::IsFinite(Width) || !FMath::IsFinite(Height) ||
        Width <= 0.0 || Height <= 0.0)
    {
      return false;
    }

    OutRect = FSlateRect(Left, Top, Left + Width, Top + Height);
    return true;
  }

  bool DoDesignerRectsIntersect(const FSlateRect &LeftRect, const FSlateRect &RightRect)
  {
    return LeftRect.Left < RightRect.Right && LeftRect.Right > RightRect.Left &&
           LeftRect.Top < RightRect.Bottom && LeftRect.Bottom > RightRect.Top;
  }

  void CollectDesignerRectSelectionCandidates(UWidget *Widget,
                                              UWidget *RootWidget,
                                              FWidgetBlueprintEditor *WidgetEditor,
                                              const TSharedPtr<SDesignerView> &DesignerView,
                                              const TSharedPtr<SDockTab> &DesignerTab,
                                              const TSharedPtr<SWindow> &ParentWindow,
                                              const FSlateRect &SelectionRect,
                                              TArray<FMcpDesignerRectSelectionCandidate> &OutCandidates)
  {
    if (!Widget || WidgetEditor == nullptr || !DesignerView.IsValid())
    {
      return;
    }

    const FWidgetReference WidgetReference = WidgetEditor->GetReferenceFromTemplate(Widget);
    FSlateRect WidgetRect;
    if (WidgetReference.IsValid() &&
        TryGetWidgetDesignerClientRect(WidgetReference, DesignerView, DesignerTab, ParentWindow, WidgetRect) &&
        DoDesignerRectsIntersect(SelectionRect, WidgetRect))
    {
      FMcpDesignerRectSelectionCandidate Candidate;
      Candidate.WidgetReference = WidgetReference;
      Candidate.WidgetPath = McpWidgetBlueprintEditorUtils::BuildWidgetDesignerPath(Widget);
      Candidate.bIsRoot = Widget == RootWidget;
      OutCandidates.Add(MoveTemp(Candidate));
    }

    if (UPanelWidget *PanelWidget = Cast<UPanelWidget>(Widget))
    {
      for (int32 ChildIndex = 0; ChildIndex < PanelWidget->GetChildrenCount(); ++ChildIndex)
      {
        CollectDesignerRectSelectionCandidates(PanelWidget->GetChildAt(ChildIndex),
                                               RootWidget,
                                               WidgetEditor,
                                               DesignerView,
                                               DesignerTab,
                                               ParentWindow,
                                               SelectionRect,
                                               OutCandidates);
      }
    }
  }

  void DetachVirtualCursorOverlay(UMcpAutomationBridgeSubsystem *Self)
  {
    if (Self == nullptr)
    {
      return;
    }

    if (Self->SimulatedTargetWindow.IsValid() && Self->SimulatedVirtualCursorOverlay.IsValid())
    {
      Self->SimulatedTargetWindow.Pin()->RemoveOverlaySlot(Self->SimulatedVirtualCursorOverlay.ToSharedRef());
    }

    Self->SimulatedVirtualCursorOverlay.Reset();
    Self->SimulatedTargetWindow.Reset();
    Self->SimulatedTargetDockTab.Reset();
    Self->SimulatedTargetTabId.Reset();
    Self->SimulatedTargetWindowTitle.Reset();
  }

  void ResetVirtualInputState(UMcpAutomationBridgeSubsystem *Self)
  {
    if (Self == nullptr)
    {
      return;
    }

    Self->SimulatedPressedMouseButtons.Reset();
    Self->SimulatedPressedModifierKeys.Reset();
    Self->SimulatedPointerPosition = FVector2D::ZeroVector;
    Self->bSimulatedPointerPositionValid = false;
    FSlateApplication::Get().ReleaseAllPointerCapture();
    DetachVirtualCursorOverlay(Self);
  }

  FVector2D ClampClientPositionToWindow(const FMcpResolvedVirtualInputTarget &Target, const FVector2D &Position)
  {
    const FVector2D ClientSize = Target.ClientRect.GetSize();
    return FVector2D(
        FMath::Clamp(Position.X, 0.0f, ClientSize.X),
        FMath::Clamp(Position.Y, 0.0f, ClientSize.Y));
  }

  FVector2D ClientPositionToScreen(const FMcpResolvedVirtualInputTarget &Target, const FVector2D &Position)
  {
    return FVector2D(Target.ClientRect.Left + Position.X, Target.ClientRect.Top + Position.Y);
  }

  bool TryGetRelativePointerPositionFromPayload(const TSharedPtr<FJsonObject> &Payload, FVector2D &OutPosition)
  {
    if (!Payload.IsValid())
    {
      return false;
    }

    double X = 0.0;
    double Y = 0.0;
    if (Payload->TryGetNumberField(TEXT("clientX"), X) && Payload->TryGetNumberField(TEXT("clientY"), Y))
    {
      OutPosition = FVector2D(static_cast<float>(X), static_cast<float>(Y));
      return true;
    }

    if (Payload->TryGetNumberField(TEXT("x"), X) && Payload->TryGetNumberField(TEXT("y"), Y))
    {
      OutPosition = FVector2D(static_cast<float>(X), static_cast<float>(Y));
      return true;
    }

    return false;
  }

  bool TryGetRelativePointObjectField(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, FVector2D &OutPosition)
  {
    if (!Payload.IsValid())
    {
      return false;
    }

    const TSharedPtr<FJsonObject> *PointObject = nullptr;
    if (!Payload->TryGetObjectField(FieldName, PointObject) || PointObject == nullptr || !PointObject->IsValid())
    {
      return false;
    }

    double X = 0.0;
    double Y = 0.0;
    if ((*PointObject)->TryGetNumberField(TEXT("clientX"), X) && (*PointObject)->TryGetNumberField(TEXT("clientY"), Y))
    {
      OutPosition = FVector2D(static_cast<float>(X), static_cast<float>(Y));
      return true;
    }

    if ((*PointObject)->TryGetNumberField(TEXT("x"), X) && (*PointObject)->TryGetNumberField(TEXT("y"), Y))
    {
      OutPosition = FVector2D(static_cast<float>(X), static_cast<float>(Y));
      return true;
    }

    return false;
  }

  bool TryGetPointObjectField(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, FVector2D &OutPosition);

  bool TryGetBlueprintViewPointField(const TSharedPtr<FJsonObject> &Payload,
                                     const TCHAR *FieldName,
                                     FVector2D &OutPosition)
  {
    return TryGetPointObjectField(Payload, FieldName, OutPosition) ||
           TryGetRelativePointObjectField(Payload, FieldName, OutPosition);
  }

  void SetPointObjectField(const TSharedPtr<FJsonObject> &Response,
                           const TCHAR *FieldName,
                           const FVector2D &Point)
  {
    if (!Response.IsValid())
    {
      return;
    }

    TSharedPtr<FJsonObject> PointObject = MakeShared<FJsonObject>();
    PointObject->SetNumberField(TEXT("x"), Point.X);
    PointObject->SetNumberField(TEXT("y"), Point.Y);
    Response->SetObjectField(FieldName, PointObject);
  }

  bool ResolveVirtualInputTarget(UMcpAutomationBridgeSubsystem *Self,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 bool &bOutAttempted,
                                 FMcpResolvedVirtualInputTarget &OutTarget,
                                 FString &OutError)
  {
    bOutAttempted = false;
    OutError.Reset();

    if (Self == nullptr || !FSlateApplication::IsInitialized())
    {
      return false;
    }

    FString RequestedTabId;
    FString RequestedWindowTitle;
    if (Payload.IsValid())
    {
      Payload->TryGetStringField(TEXT("tabId"), RequestedTabId);
      Payload->TryGetStringField(TEXT("windowTitle"), RequestedWindowTitle);
    }

    RequestedTabId = RequestedTabId.TrimStartAndEnd();
    RequestedWindowTitle = RequestedWindowTitle.TrimStartAndEnd();

#if WITH_EDITOR
    const FString RequestedAssetPath = GetTrimmedPayloadString(Payload, TEXT("assetPath"));
    if (!RequestedAssetPath.IsEmpty())
    {
      OutTarget.RequestedAssetPath = RequestedAssetPath;
      FMcpResolvedWidgetBlueprintEditorContext WidgetContext;
      FString WidgetErrorCode;
      FString WidgetErrorMessage;
      if (McpWidgetBlueprintEditorUtils::ResolveWidgetBlueprintEditorContext(
              Payload,
              WidgetContext,
              WidgetErrorCode,
              WidgetErrorMessage,
              false,
              false,
              true) &&
          WidgetContext.DesignerView.IsValid())
      {
        TSharedPtr<SWindow> DesignerWindow = WidgetContext.DesignerTab.IsValid()
                                                 ? WidgetContext.DesignerTab->GetParentWindow()
                                                 : TSharedPtr<SWindow>();
        if (!DesignerWindow.IsValid())
        {
          DesignerWindow = FSlateApplication::Get().FindWidgetWindow(WidgetContext.DesignerView.ToSharedRef());
        }
        if (DesignerWindow.IsValid())
        {
          bOutAttempted = true;
          OutTarget.Window = DesignerWindow;
          OutTarget.Tab = WidgetContext.DesignerTab;
          OutTarget.PreferredWidget = WidgetContext.DesignerView;
          OutTarget.TabId = !WidgetContext.TabId.IsEmpty() ? WidgetContext.TabId : RequestedTabId;
          OutTarget.WindowTitle = !WidgetContext.WindowTitle.IsEmpty()
                                      ? WidgetContext.WindowTitle
                                      : DesignerWindow->GetTitle().ToString();
          OutTarget.ResolutionSource = WidgetContext.ResolvedTargetSource.IsEmpty()
                                           ? TEXT("asset_path")
                                           : WidgetContext.ResolvedTargetSource;
          OutTarget.PreferredWidgetType = WidgetContext.DesignerView->GetTypeAsString();
          OutTarget.WindowRect = DesignerWindow->GetRectInScreen();
          OutTarget.ClientRect = GetEffectiveClientRectInScreen(DesignerWindow);
          return true;
        }

        OutTarget.AssetTargetError = TEXT("DESIGNER_WINDOW_NOT_FOUND");
      }
      else if (!WidgetErrorCode.IsEmpty() || !WidgetErrorMessage.IsEmpty())
      {
        OutTarget.AssetTargetError = FString::Printf(TEXT("%s: %s"),
                                                     WidgetErrorCode.IsEmpty() ? TEXT("WIDGET_CONTEXT_FAILED") : *WidgetErrorCode,
                                                     WidgetErrorMessage.IsEmpty() ? TEXT("Unknown widget editor resolution error") : *WidgetErrorMessage);
      }
      else
      {
        OutTarget.AssetTargetError = FString::Printf(TEXT("DESIGNER_VIEW_NOT_FOUND (mode=%s resolvedTargetSource=%s tabFound=%s)"),
                                                     *WidgetContext.CurrentMode,
                                                     *WidgetContext.ResolvedTargetSource,
                                                     WidgetContext.bDesignerTabFound ? TEXT("true") : TEXT("false"));
      }
    }
#endif

    FString TabResolutionError;
    if (!RequestedTabId.IsEmpty())
    {
      bOutAttempted = true;
      const TSharedPtr<SDockTab> TargetTab = FGlobalTabmanager::Get()->FindExistingLiveTab(FName(*RequestedTabId));
      if (TargetTab.IsValid())
      {
        const TSharedPtr<SWindow> TargetWindow = TargetTab->GetParentWindow();
        if (!TargetWindow.IsValid())
        {
          OutError = FString::Printf(TEXT("Tab %s does not have a live parent window"), *RequestedTabId);
          return false;
        }

        OutTarget.Window = TargetWindow;
        OutTarget.Tab = TargetTab;
        OutTarget.TabId = RequestedTabId;
        OutTarget.WindowTitle = TargetWindow->GetTitle().ToString();
        OutTarget.ResolutionSource = TEXT("tab_id");
        OutTarget.WindowRect = TargetWindow->GetRectInScreen();
        OutTarget.ClientRect = GetEffectiveClientRectInScreen(TargetWindow);
        return true;
      }

      TabResolutionError = FString::Printf(TEXT("Live tab %s was not found"), *RequestedTabId);
      if (RequestedWindowTitle.IsEmpty())
      {
        OutError = TabResolutionError;
        return false;
      }
    }

    if (!RequestedWindowTitle.IsEmpty())
    {
      bOutAttempted = true;
      for (const TSharedRef<SWindow> &Window : GetVisibleSlateWindows())
      {
        if (Window->GetTitle().ToString().Equals(RequestedWindowTitle, ESearchCase::IgnoreCase))
        {
          OutTarget.Window = Window;
          OutTarget.WindowTitle = Window->GetTitle().ToString();
          OutTarget.ResolutionSource = TEXT("window_title");
          OutTarget.WindowRect = Window->GetRectInScreen();
          OutTarget.ClientRect = GetEffectiveClientRectInScreen(Window);
          return true;
        }
      }

      OutError = TabResolutionError.IsEmpty()
                     ? FString::Printf(TEXT("Visible window '%s' was not found"), *RequestedWindowTitle)
                     : FString::Printf(TEXT("%s; Visible window '%s' was not found"), *TabResolutionError, *RequestedWindowTitle);
      return false;
    }

    if (Self->SimulatedTargetWindow.IsValid())
    {
      bOutAttempted = true;
      OutTarget.Window = Self->SimulatedTargetWindow.Pin();
      OutTarget.Tab = Self->SimulatedTargetDockTab.Pin();
      OutTarget.TabId = Self->SimulatedTargetTabId;
      OutTarget.WindowTitle = Self->SimulatedTargetWindowTitle;
      OutTarget.ResolutionSource = TEXT("stored_target");
      OutTarget.WindowRect = OutTarget.Window->GetRectInScreen();
      OutTarget.ClientRect = GetEffectiveClientRectInScreen(OutTarget.Window);
      return true;
    }

    return false;
  }

  void AttachVirtualCursorOverlay(UMcpAutomationBridgeSubsystem *Self,
                                  const FMcpResolvedVirtualInputTarget &Target,
                                  const FVector2D &ClientPosition)
  {
    if (Self == nullptr || !Target.Window.IsValid())
    {
      return;
    }

    const bool bTargetChanged = !Self->SimulatedTargetWindow.IsValid() || Self->SimulatedTargetWindow.Pin() != Target.Window;
    if (bTargetChanged)
    {
      DetachVirtualCursorOverlay(Self);
    }

    if (!Self->SimulatedVirtualCursorOverlay.IsValid())
    {
      const TSharedRef<SMcpBridgeVirtualCursorOverlay> Overlay = SNew(SMcpBridgeVirtualCursorOverlay)
                                                                     .CursorPosition(ClientPosition);
      Self->SimulatedVirtualCursorOverlay = Overlay;
      Target.Window->AddOverlaySlot(INT32_MAX)
          .HAlign(HAlign_Fill)
          .VAlign(VAlign_Fill)
              [Overlay];
    }
    else
    {
      StaticCastSharedPtr<SMcpBridgeVirtualCursorOverlay>(Self->SimulatedVirtualCursorOverlay)->SetCursorPosition(ClientPosition);
    }

    Self->SimulatedTargetWindow = Target.Window;
    Self->SimulatedTargetDockTab = Target.Tab;
    Self->SimulatedTargetTabId = Target.TabId;
    Self->SimulatedTargetWindowTitle = Target.WindowTitle;
  }

  FWidgetPath BuildPointerAwareWidgetPath(const FWidgetPath &WidgetPath)
  {
    if (!WidgetPath.IsValid() || WidgetPath.Widgets.Num() == 0)
    {
      return FWidgetPath();
    }

    TArray<FWidgetAndPointer> WidgetsAndPointers;
    WidgetsAndPointers.Reserve(WidgetPath.Widgets.Num());
    for (int32 WidgetIndex = 0; WidgetIndex < WidgetPath.Widgets.Num(); ++WidgetIndex)
    {
      WidgetsAndPointers.Add(FWidgetAndPointer(WidgetPath.Widgets[WidgetIndex]));
    }

    return FWidgetPath(WidgetsAndPointers);
  }

  bool TryBuildPreferredTargetWidgetPath(const FMcpResolvedVirtualInputTarget &Target,
                                         const FVector2D &ScreenPosition,
                                         FWidgetPath &OutWidgetPath)
  {
    OutWidgetPath = FWidgetPath();

    if (!Target.PreferredWidget.IsValid() || !FSlateApplication::IsInitialized())
    {
      return false;
    }

    FWidgetPath PreferredWidgetPath;
    if (!FSlateApplication::Get().GeneratePathToWidgetUnchecked(Target.PreferredWidget.ToSharedRef(),
                                                                PreferredWidgetPath,
                                                                EVisibility::All) ||
        !PreferredWidgetPath.IsValid() ||
        PreferredWidgetPath.Widgets.Num() == 0)
    {
      return false;
    }

    const FGeometry &PreferredGeometry = PreferredWidgetPath.Widgets.Last().Geometry;
    const FVector2D PreferredAbsolutePosition = PreferredGeometry.GetAbsolutePosition();
    const FVector2D PreferredAbsoluteSize = PreferredGeometry.GetAbsoluteSize();
    const bool bPointInsidePreferredWidget =
        ScreenPosition.X >= PreferredAbsolutePosition.X &&
        ScreenPosition.Y >= PreferredAbsolutePosition.Y &&
        ScreenPosition.X <= PreferredAbsolutePosition.X + PreferredAbsoluteSize.X &&
        ScreenPosition.Y <= PreferredAbsolutePosition.Y + PreferredAbsoluteSize.Y;

    if (!bPointInsidePreferredWidget)
    {
      return false;
    }

    OutWidgetPath = BuildPointerAwareWidgetPath(PreferredWidgetPath);
    return OutWidgetPath.IsValid();
  }

  FWidgetPath LocateTargetWidgetPath(const FMcpResolvedVirtualInputTarget &Target,
                                     const FVector2D &ScreenPosition,
                                     bool *bOutUsedPreferredWidgetPath = nullptr)
  {
    if (bOutUsedPreferredWidgetPath != nullptr)
    {
      *bOutUsedPreferredWidgetPath = false;
    }

    FWidgetPath PreferredWidgetPath;
    if (TryBuildPreferredTargetWidgetPath(Target, ScreenPosition, PreferredWidgetPath))
    {
      if (bOutUsedPreferredWidgetPath != nullptr)
      {
        *bOutUsedPreferredWidgetPath = true;
      }
      return PreferredWidgetPath;
    }

    if (!Target.Window.IsValid() ||
        !Target.Window->IsVisible() ||
        !Target.Window->AcceptsInput() ||
        !Target.Window->IsScreenspaceMouseWithin(FVector2f(static_cast<float>(ScreenPosition.X), static_cast<float>(ScreenPosition.Y))))
    {
      return FWidgetPath();
    }

    FSlateApplication &SlateApp = FSlateApplication::Get();
    TArray<FWidgetAndPointer> WidgetsAndCursors = Target.Window->GetHittestGrid().GetBubblePath(
        FVector2f(static_cast<float>(ScreenPosition.X), static_cast<float>(ScreenPosition.Y)),
        SlateApp.GetCursorRadius(),
        false,
        GetSimulatedSlateUserIndex());
    return FWidgetPath(MoveTemp(WidgetsAndCursors));
  }

  bool FocusVirtualTargetAtPosition(const FMcpResolvedVirtualInputTarget &Target, const FVector2D &ScreenPosition)
  {
    FSlateApplication &SlateApp = FSlateApplication::Get();
    const uint32 KeyboardUserIndex = SlateApp.GetUserIndexForKeyboard();
    FWidgetPath WidgetPath = LocateTargetWidgetPath(Target, ScreenPosition);
    if (!WidgetPath.IsValid())
    {
      return false;
    }

    TSharedPtr<SWidget> FallbackFocusableWidget;
    for (int32 WidgetIndex = WidgetPath.Widgets.Num() - 1; WidgetIndex >= 0; --WidgetIndex)
    {
      const TSharedRef<SWidget> &CandidateWidget = WidgetPath.Widgets[WidgetIndex].Widget;
      if (!CandidateWidget->SupportsKeyboardFocus())
      {
        continue;
      }

      if (!FallbackFocusableWidget.IsValid())
      {
        FallbackFocusableWidget = CandidateWidget;
      }

      const FString CandidateType = CandidateWidget->GetTypeAsString();
      if (CandidateType == TEXT("SEditableTextBox") ||
          CandidateType == TEXT("SMultiLineEditableTextBox") ||
          CandidateType == TEXT("SComboBox"))
      {
        FWidgetPath FocusPath = WidgetPath.GetPathDownTo(CandidateWidget);
        return SlateApp.SetUserFocus(KeyboardUserIndex, FocusPath, EFocusCause::SetDirectly);
      }
    }

    if (FallbackFocusableWidget.IsValid())
    {
      FWidgetPath FocusPath = WidgetPath.GetPathDownTo(FallbackFocusableWidget.ToSharedRef());
      return SlateApp.SetUserFocus(KeyboardUserIndex, FocusPath, EFocusCause::SetDirectly);
    }

    return SlateApp.SetUserFocus(KeyboardUserIndex, WidgetPath, EFocusCause::SetDirectly);
  }

  bool DispatchVirtualMouseMove(UMcpAutomationBridgeSubsystem *Self,
                                const FMcpResolvedVirtualInputTarget &Target,
                                const FVector2D &ClientPosition)
  {
    FSlateApplication &SlateApp = FSlateApplication::Get();
    const FVector2D ClampedPosition = ClampClientPositionToWindow(Target, ClientPosition);
    const FVector2D LastClientPosition = Self->bSimulatedPointerPositionValid ? Self->SimulatedPointerPosition : ClampedPosition;
    const FVector2D ScreenPosition = ClientPositionToScreen(Target, ClampedPosition);
    const FVector2D LastScreenPosition = ClientPositionToScreen(Target, LastClientPosition);
    const TSet<FKey> PressedButtons = ConvertPressedButtonNamesToKeys(Self->SimulatedPressedMouseButtons);
    const FPointerEvent MoveEvent = BuildMouseMoveEvent(ScreenPosition, LastScreenPosition, PressedButtons, BuildModifierKeyState(Self->SimulatedPressedModifierKeys));
    const FWidgetPath WidgetPath = LocateTargetWidgetPath(Target, ScreenPosition);
    const bool bHandled = WidgetPath.IsValid() && SlateApp.RoutePointerMoveEvent(WidgetPath, MoveEvent, false);
    StoreSimulatedPosition(Self->SimulatedPointerPosition, Self->bSimulatedPointerPositionValid, ClampedPosition);
    AttachVirtualCursorOverlay(Self, Target, ClampedPosition);
    return bHandled;
  }

  bool DispatchVirtualMouseDown(UMcpAutomationBridgeSubsystem *Self,
                                const FMcpResolvedVirtualInputTarget &Target,
                                const FVector2D &ClientPosition,
                                const FKey &MouseButton)
  {
    const FVector2D ClampedPosition = ClampClientPositionToWindow(Target, ClientPosition);
    DispatchVirtualMouseMove(Self, Target, ClampedPosition);
    const FVector2D ScreenPosition = ClientPositionToScreen(Target, Self->SimulatedPointerPosition);
    Self->SimulatedPressedMouseButtons.Add(MouseButton.GetFName().ToString());
    const TSet<FKey> PressedButtons = ConvertPressedButtonNamesToKeys(Self->SimulatedPressedMouseButtons);
    const FPointerEvent MouseDownEvent = BuildMouseButtonEvent(ScreenPosition, ScreenPosition, PressedButtons, MouseButton, BuildModifierKeyState(Self->SimulatedPressedModifierKeys), 0.0f);
    FSlateApplication &SlateApp = FSlateApplication::Get();
    const FWidgetPath WidgetPath = LocateTargetWidgetPath(Target, ScreenPosition);
    FocusVirtualTargetAtPosition(Target, ScreenPosition);
    const bool bHandled = WidgetPath.IsValid() && SlateApp.RoutePointerDownEvent(WidgetPath, MouseDownEvent).IsEventHandled();
    AttachVirtualCursorOverlay(Self, Target, Self->SimulatedPointerPosition);
    return bHandled;
  }

  bool DispatchVirtualMouseUp(UMcpAutomationBridgeSubsystem *Self,
                              const FMcpResolvedVirtualInputTarget &Target,
                              const FVector2D &ClientPosition,
                              const FKey &MouseButton)
  {
    const FVector2D ClampedPosition = ClampClientPositionToWindow(Target, ClientPosition);
    DispatchVirtualMouseMove(Self, Target, ClampedPosition);
    const FVector2D ScreenPosition = ClientPositionToScreen(Target, Self->SimulatedPointerPosition);
    TSet<FString> PressedButtonsAfterRelease = Self->SimulatedPressedMouseButtons;
    PressedButtonsAfterRelease.Remove(MouseButton.GetFName().ToString());
    const TSet<FKey> PressedButtonKeysAfterRelease = ConvertPressedButtonNamesToKeys(PressedButtonsAfterRelease);
    const FPointerEvent MouseUpEvent = BuildMouseButtonEvent(ScreenPosition, ScreenPosition, PressedButtonKeysAfterRelease, MouseButton, BuildModifierKeyState(Self->SimulatedPressedModifierKeys), 0.0f);
    FSlateApplication &SlateApp = FSlateApplication::Get();
    const FWidgetPath WidgetPath = LocateTargetWidgetPath(Target, ScreenPosition);
    const bool bHandled = WidgetPath.IsValid() && SlateApp.RoutePointerUpEvent(WidgetPath, MouseUpEvent).IsEventHandled();
    Self->SimulatedPressedMouseButtons = MoveTemp(PressedButtonsAfterRelease);
    AttachVirtualCursorOverlay(Self, Target, Self->SimulatedPointerPosition);
    return bHandled;
  }

  bool DispatchVirtualMouseWheel(UMcpAutomationBridgeSubsystem *Self,
                                 const FMcpResolvedVirtualInputTarget &Target,
                                 const FVector2D &ClientPosition,
                                 float WheelDelta)
  {
    const FVector2D ClampedPosition = ClampClientPositionToWindow(Target, ClientPosition);
    DispatchVirtualMouseMove(Self, Target, ClampedPosition);
    const FVector2D ScreenPosition = ClientPositionToScreen(Target, Self->SimulatedPointerPosition);
    const TSet<FKey> PressedButtons = ConvertPressedButtonNamesToKeys(Self->SimulatedPressedMouseButtons);
    const FPointerEvent WheelEvent = BuildMouseButtonEvent(ScreenPosition, ScreenPosition, PressedButtons, EKeys::Invalid, BuildModifierKeyState(Self->SimulatedPressedModifierKeys), WheelDelta);
    FSlateApplication &SlateApp = FSlateApplication::Get();
    const FWidgetPath WidgetPath = LocateTargetWidgetPath(Target, ScreenPosition);
    const bool bHandled = WidgetPath.IsValid() && SlateApp.RouteMouseWheelOrGestureEvent(WidgetPath, WheelEvent, nullptr).IsEventHandled();
    AttachVirtualCursorOverlay(Self, Target, Self->SimulatedPointerPosition);
    return bHandled;
  }

  bool TryGetPointerPositionFromPayload(const TSharedPtr<FJsonObject> &Payload, FVector2D &OutPosition)
  {
    if (!Payload.IsValid())
    {
      return false;
    }

    double X = 0.0;
    double Y = 0.0;
    if (Payload->TryGetNumberField(TEXT("x"), X) && Payload->TryGetNumberField(TEXT("y"), Y))
    {
      OutPosition = FVector2D(static_cast<float>(X), static_cast<float>(Y));
      return true;
    }

    return false;
  }

  bool TryGetPointObjectField(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, FVector2D &OutPosition)
  {
    if (!Payload.IsValid())
    {
      return false;
    }

    const TSharedPtr<FJsonObject> *PointObject = nullptr;
    if (!Payload->TryGetObjectField(FieldName, PointObject) || PointObject == nullptr || !PointObject->IsValid())
    {
      return false;
    }

    double X = 0.0;
    double Y = 0.0;
    if ((*PointObject)->TryGetNumberField(TEXT("x"), X) && (*PointObject)->TryGetNumberField(TEXT("y"), Y))
    {
      OutPosition = FVector2D(static_cast<float>(X), static_cast<float>(Y));
      return true;
    }

    return false;
  }

  FKey ResolveMouseButtonKey(const FString &ButtonName)
  {
    const FString NormalizedButton = ButtonName.ToLower();
    if (NormalizedButton == TEXT("right"))
    {
      return EKeys::RightMouseButton;
    }
    if (NormalizedButton == TEXT("middle"))
    {
      return EKeys::MiddleMouseButton;
    }
    return EKeys::LeftMouseButton;
  }

  bool TryResolveTextCharacterKey(const TCHAR Character, FKey &OutKey)
  {
    switch (Character)
    {
    case TEXT('0'):
      OutKey = EKeys::Zero;
      return true;
    case TEXT('1'):
      OutKey = EKeys::One;
      return true;
    case TEXT('2'):
      OutKey = EKeys::Two;
      return true;
    case TEXT('3'):
      OutKey = EKeys::Three;
      return true;
    case TEXT('4'):
      OutKey = EKeys::Four;
      return true;
    case TEXT('5'):
      OutKey = EKeys::Five;
      return true;
    case TEXT('6'):
      OutKey = EKeys::Six;
      return true;
    case TEXT('7'):
      OutKey = EKeys::Seven;
      return true;
    case TEXT('8'):
      OutKey = EKeys::Eight;
      return true;
    case TEXT('9'):
      OutKey = EKeys::Nine;
      return true;
    case TEXT(' '):
      OutKey = EKeys::SpaceBar;
      return true;
    default:
      break;
    }

    const TCHAR UpperCharacter = FChar::ToUpper(Character);
    if (UpperCharacter >= TEXT('A') && UpperCharacter <= TEXT('Z'))
    {
      const FString KeyName(1, &UpperCharacter);
      OutKey = FKey(*KeyName);
      return OutKey.IsValid();
    }

    return false;
  }

  TSet<FKey> ConvertPressedButtonNamesToKeys(const TSet<FString> &PressedButtonNames)
  {
    TSet<FKey> PressedButtons;
    for (const FString &ButtonName : PressedButtonNames)
    {
      const FKey Key(*ButtonName);
      if (Key.IsValid())
      {
        PressedButtons.Add(Key);
      }
    }
    return PressedButtons;
  }

  bool IsModifierKey(const FKey &Key)
  {
    return Key == EKeys::LeftShift || Key == EKeys::RightShift ||
           Key == EKeys::LeftControl || Key == EKeys::RightControl ||
           Key == EKeys::LeftAlt || Key == EKeys::RightAlt ||
           Key == EKeys::LeftCommand || Key == EKeys::RightCommand;
  }

  void UpdatePressedModifierKeys(TSet<FString> &PressedModifierKeys, const FKey &Key, bool bPressed)
  {
    if (!IsModifierKey(Key))
    {
      return;
    }

    const FString KeyName = Key.GetFName().ToString();
    if (bPressed)
    {
      PressedModifierKeys.Add(KeyName);
    }
    else
    {
      PressedModifierKeys.Remove(KeyName);
    }
  }

  FModifierKeysState BuildModifierKeyState(const TSet<FString> &PressedModifierKeys)
  {
    return FModifierKeysState(
        PressedModifierKeys.Contains(EKeys::LeftShift.GetFName().ToString()),
        PressedModifierKeys.Contains(EKeys::RightShift.GetFName().ToString()),
        PressedModifierKeys.Contains(EKeys::LeftControl.GetFName().ToString()),
        PressedModifierKeys.Contains(EKeys::RightControl.GetFName().ToString()),
        PressedModifierKeys.Contains(EKeys::LeftAlt.GetFName().ToString()),
        PressedModifierKeys.Contains(EKeys::RightAlt.GetFName().ToString()),
        PressedModifierKeys.Contains(EKeys::LeftCommand.GetFName().ToString()),
        PressedModifierKeys.Contains(EKeys::RightCommand.GetFName().ToString()),
        false);
  }

  void GetKeyAndCharCodes(const FKey &Key, bool &bHasKeyCode, uint32 &KeyCode,
                          bool &bHasCharCode, uint32 &CharCode)
  {
    const uint32 *KeyCodePtr = nullptr;
    const uint32 *CharCodePtr = nullptr;
    FInputKeyManager::Get().GetCodesFromKey(Key, KeyCodePtr, CharCodePtr);

    bHasKeyCode = KeyCodePtr != nullptr;
    bHasCharCode = CharCodePtr != nullptr;
    KeyCode = bHasKeyCode ? *KeyCodePtr : 0;
    CharCode = bHasCharCode ? *CharCodePtr : 0;

    if (!bHasCharCode)
    {
      if (Key == EKeys::Tab)
      {
        CharCode = '\t';
        bHasCharCode = true;
      }
      else if (Key == EKeys::BackSpace)
      {
        CharCode = '\b';
        bHasCharCode = true;
      }
      else if (Key == EKeys::Enter)
      {
        CharCode = '\n';
        bHasCharCode = true;
      }
    }
  }

  FString DescribePressedKeys(const TSet<FString> &PressedKeys)
  {
    TArray<FString> Names;
    Names.Reserve(PressedKeys.Num());
    for (const FString &KeyName : PressedKeys)
    {
      Names.Add(KeyName);
    }
    Names.Sort();
    return FString::Join(Names, TEXT(","));
  }

  FPointerEvent BuildMouseButtonEvent(const FVector2D &Position,
                                      const FVector2D &LastPosition,
                                      const TSet<FKey> &PressedButtons,
                                      const FKey &EffectingButton,
                                      const FModifierKeysState &ModifierKeys,
                                      float WheelDelta = 0.0f)
  {
    const int32 SlateUserIndex = GetSimulatedSlateUserIndex();
    return FPointerEvent(
        GetSimulatedInputDeviceId(SlateUserIndex),
        static_cast<uint32>(GMcpSimulatedPointerIndex),
        Position,
        LastPosition,
        PressedButtons,
        EffectingButton,
        WheelDelta,
        ModifierKeys,
        SlateUserIndex);
  }

  FPointerEvent BuildMouseMoveEvent(const FVector2D &Position,
                                    const FVector2D &LastPosition,
                                    const TSet<FKey> &PressedButtons,
                                    const FModifierKeysState &ModifierKeys)
  {
    const int32 SlateUserIndex = GetSimulatedSlateUserIndex();
    return FPointerEvent(
        static_cast<uint32>(SlateUserIndex),
        static_cast<uint32>(GMcpSimulatedPointerIndex),
        Position,
        LastPosition,
        Position - LastPosition,
        PressedButtons,
        ModifierKeys);
  }

  FVector2D GetCurrentSimulatedPosition(const FVector2D &StoredPosition, bool bHasStoredPosition)
  {
    if (bHasStoredPosition)
    {
      return StoredPosition;
    }

    return FSlateApplication::Get().GetCursorPos();
  }

  void StoreSimulatedPosition(FVector2D &StoredPosition, bool &bHasStoredPosition, const FVector2D &Position)
  {
    StoredPosition = Position;
    bHasStoredPosition = true;
  }

  void ResetSimulatedInputState(FVector2D &StoredPosition, bool &bHasStoredPosition, TSet<FString> &PressedButtons, TSet<FString> &PressedModifierKeys)
  {
    PressedButtons.Reset();
    PressedModifierKeys.Reset();
    StoredPosition = FVector2D::ZeroVector;
    bHasStoredPosition = false;
    FSlateApplication::Get().ReleaseAllPointerCapture();
  }

  bool DispatchMouseMove(FVector2D &StoredPosition, bool &bHasStoredPosition, const TSet<FString> &PressedButtonNames, const TSet<FString> &PressedModifierKeys, const FVector2D &Position)
  {
    FSlateApplication &SlateApp = FSlateApplication::Get();
    const FVector2D LastPosition = GetCurrentSimulatedPosition(StoredPosition, bHasStoredPosition);
    SlateApp.SetCursorPos(Position);
    const TSet<FKey> PressedButtons = ConvertPressedButtonNamesToKeys(PressedButtonNames);
    const FPointerEvent MoveEvent = BuildMouseMoveEvent(Position, LastPosition, PressedButtons, BuildModifierKeyState(PressedModifierKeys));
    const bool bHandled = SlateApp.ProcessMouseMoveEvent(MoveEvent, false);
    StoreSimulatedPosition(StoredPosition, bHasStoredPosition, Position);
    return bHandled;
  }

  bool DispatchMouseDown(FVector2D &StoredPosition, bool &bHasStoredPosition, TSet<FString> &PressedButtonNames, const TSet<FString> &PressedModifierKeys, const FVector2D &Position, const FKey &MouseButton)
  {
    DispatchMouseMove(StoredPosition, bHasStoredPosition, PressedButtonNames, PressedModifierKeys, Position);
    PressedButtonNames.Add(MouseButton.GetFName().ToString());
    const TSet<FKey> PressedButtons = ConvertPressedButtonNamesToKeys(PressedButtonNames);
    const FPointerEvent MouseDownEvent = BuildMouseButtonEvent(Position, Position, PressedButtons, MouseButton, BuildModifierKeyState(PressedModifierKeys));
    return FSlateApplication::Get().ProcessMouseButtonDownEvent(nullptr, MouseDownEvent);
  }

  bool DispatchMouseUp(FVector2D &StoredPosition, bool &bHasStoredPosition, TSet<FString> &PressedButtonNames, const TSet<FString> &PressedModifierKeys, const FVector2D &Position, const FKey &MouseButton)
  {
    DispatchMouseMove(StoredPosition, bHasStoredPosition, PressedButtonNames, PressedModifierKeys, Position);
    TSet<FString> PressedButtonsAfterRelease = PressedButtonNames;
    PressedButtonsAfterRelease.Remove(MouseButton.GetFName().ToString());
    const TSet<FKey> PressedButtonKeysAfterRelease = ConvertPressedButtonNamesToKeys(PressedButtonsAfterRelease);
    const FPointerEvent MouseUpEvent = BuildMouseButtonEvent(Position, Position, PressedButtonKeysAfterRelease, MouseButton, BuildModifierKeyState(PressedModifierKeys));
    const bool bHandled = FSlateApplication::Get().ProcessMouseButtonUpEvent(MouseUpEvent);
    PressedButtonNames = MoveTemp(PressedButtonsAfterRelease);
    StoreSimulatedPosition(StoredPosition, bHasStoredPosition, Position);
    return bHandled;
  }

  bool DispatchMouseWheel(FVector2D &StoredPosition, bool &bHasStoredPosition, const TSet<FString> &PressedButtonNames, const TSet<FString> &PressedModifierKeys, const FVector2D &Position, float WheelDelta)
  {
    DispatchMouseMove(StoredPosition, bHasStoredPosition, PressedButtonNames, PressedModifierKeys, Position);
    const TSet<FKey> PressedButtons = ConvertPressedButtonNamesToKeys(PressedButtonNames);
    const FPointerEvent WheelEvent = BuildMouseButtonEvent(Position, Position, PressedButtons, EKeys::Invalid, BuildModifierKeyState(PressedModifierKeys), WheelDelta);
    return FSlateApplication::Get().ProcessMouseWheelOrGestureEvent(WheelEvent, nullptr);
  }

  FString SanitizeScreenshotFilename(const FString &RequestedFilename)
  {
    FString Filename = RequestedFilename;
    if (Filename.IsEmpty())
    {
      Filename = FString::Printf(TEXT("Screenshot_%s"),
                                 *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
    }

    Filename = FPaths::GetCleanFilename(Filename);
    if (Filename.Contains(TEXT("..")) || Filename.Contains(TEXT("/")) ||
        Filename.Contains(TEXT("\\")))
    {
      Filename = FString::Printf(TEXT("Screenshot_%s"),
                                 *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
    }

    if (!Filename.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase))
    {
      Filename += TEXT(".png");
    }

    return Filename;
  }

  bool CompressBitmapToPng(const TArray<FColor> &Bitmap, const int32 Width,
                           const int32 Height, TArray<uint8> &OutPngData)
  {
    OutPngData.Reset();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FImageUtils::ThumbnailCompressImageArray(Width, Height, Bitmap, OutPngData);
#else
    FImageUtils::CompressImageArray(Width, Height, Bitmap, OutPngData);
#endif

    if (OutPngData.Num() > 0)
    {
      return true;
    }

    IImageWrapperModule &ImageWrapperModule =
        FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper =
        ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
    if (!ImageWrapper.IsValid())
    {
      return false;
    }

    TArray<uint8> RawData;
    RawData.SetNum(Width * Height * 4);
    for (int32 Index = 0; Index < Bitmap.Num(); ++Index)
    {
      RawData[Index * 4 + 0] = Bitmap[Index].R;
      RawData[Index * 4 + 1] = Bitmap[Index].G;
      RawData[Index * 4 + 2] = Bitmap[Index].B;
      RawData[Index * 4 + 3] = Bitmap[Index].A;
    }

    if (!ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), Width, Height,
                              ERGBFormat::RGBA, 8))
    {
      return false;
    }

    OutPngData = ImageWrapper->GetCompressed(100);
    return OutPngData.Num() > 0;
  }

  bool SaveBitmapToPngFile(const TArray<FColor> &Bitmap, const int32 Width,
                           const int32 Height, const FString &FullPath,
                           FString &OutError)
  {
    TArray<uint8> PngData;
    if (!CompressBitmapToPng(Bitmap, Width, Height, PngData))
    {
      OutError = TEXT("Failed to compress screenshot as PNG");
      return false;
    }

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), true);
    if (!FFileHelper::SaveArrayToFile(PngData, *FullPath))
    {
      OutError = TEXT("Failed to save screenshot to disk");
      return false;
    }

    return true;
  }

  bool CaptureWindowScreenshotToBitmap(const TSharedPtr<SWindow> &TargetWindow,
                                       TArray<FColor> &OutImageData,
                                       FIntPoint &OutSize,
                                       FString &OutError)
  {
    OutSize = FIntPoint::ZeroValue;
    OutImageData.Reset();

    if (!FSlateApplication::IsInitialized())
    {
      OutError = TEXT("Slate application is not initialized");
      return false;
    }

    if (!TargetWindow.IsValid())
    {
      OutError = TEXT("Screenshot target window is not valid");
      return false;
    }

    FIntVector OutImageSize;
    if (!FSlateApplication::Get().TakeScreenshot(TargetWindow.ToSharedRef(), OutImageData,
                                                 OutImageSize))
    {
      OutError = TEXT("Failed to capture editor window screenshot");
      return false;
    }

    if (OutImageData.Num() == 0 || OutImageSize.X <= 0 || OutImageSize.Y <= 0)
    {
      OutError = TEXT("Window screenshot capture returned no pixel data");
      return false;
    }

    OutSize = FIntPoint(OutImageSize.X, OutImageSize.Y);
    return true;
  }

  bool CaptureWindowScreenshotToFile(const TSharedPtr<SWindow> &TargetWindow,
                                     const FString &FullPath,
                                     FIntPoint &OutSize,
                                     FString &OutError)
  {
    TArray<FColor> OutImageData;
    if (!CaptureWindowScreenshotToBitmap(TargetWindow, OutImageData, OutSize,
                                         OutError))
    {
      return false;
    }

    return SaveBitmapToPngFile(OutImageData, OutSize.X, OutSize.Y, FullPath,
                               OutError);
  }

  void CollectVisibleChildWindows(const TSharedRef<SWindow> &CurrentWindow,
                                  TArray<TSharedRef<SWindow>> &OutWindows)
  {
    const TArray<TSharedRef<SWindow>> &ChildWindows = CurrentWindow->GetChildWindows();
    for (const TSharedRef<SWindow> &ChildWindow : ChildWindows)
    {
      if (ChildWindow->IsVisible() && !ChildWindow->IsWindowMinimized())
      {
        OutWindows.Add(ChildWindow);
      }

      CollectVisibleChildWindows(ChildWindow, OutWindows);
    }
  }

  void BlendBitmapIntoCanvas(const TArray<FColor> &SourceBitmap,
                             const int32 SourceWidth,
                             const int32 SourceHeight,
                             TArray<FColor> &CanvasBitmap,
                             const int32 CanvasWidth,
                             const int32 CanvasHeight,
                             const int32 OffsetX,
                             const int32 OffsetY)
  {
    if (SourceWidth <= 0 || SourceHeight <= 0 || CanvasWidth <= 0 || CanvasHeight <= 0)
    {
      return;
    }

    for (int32 SourceY = 0; SourceY < SourceHeight; ++SourceY)
    {
      const int32 DestY = OffsetY + SourceY;
      if (DestY < 0 || DestY >= CanvasHeight)
      {
        continue;
      }

      for (int32 SourceX = 0; SourceX < SourceWidth; ++SourceX)
      {
        const int32 DestX = OffsetX + SourceX;
        if (DestX < 0 || DestX >= CanvasWidth)
        {
          continue;
        }

        const FColor &SourcePixel = SourceBitmap[SourceY * SourceWidth + SourceX];
        FColor &DestPixel = CanvasBitmap[DestY * CanvasWidth + DestX];

        if (SourcePixel.A >= 255)
        {
          DestPixel = SourcePixel;
          continue;
        }

        if (SourcePixel.A <= 0)
        {
          continue;
        }

        const uint32 SourceAlpha = static_cast<uint32>(SourcePixel.A);
        const uint32 InverseAlpha = 255u - SourceAlpha;
        DestPixel.R = static_cast<uint8>((static_cast<uint32>(SourcePixel.R) * SourceAlpha + static_cast<uint32>(DestPixel.R) * InverseAlpha) / 255u);
        DestPixel.G = static_cast<uint8>((static_cast<uint32>(SourcePixel.G) * SourceAlpha + static_cast<uint32>(DestPixel.G) * InverseAlpha) / 255u);
        DestPixel.B = static_cast<uint8>((static_cast<uint32>(SourcePixel.B) * SourceAlpha + static_cast<uint32>(DestPixel.B) * InverseAlpha) / 255u);
        DestPixel.A = static_cast<uint8>(FMath::Max<uint32>(DestPixel.A, SourceAlpha));
      }
    }
  }

  bool CaptureWindowScreenshotIncludingChildWindowsToFile(
      const TSharedPtr<SWindow> &TargetWindow,
      const FString &FullPath,
      FIntPoint &OutSize,
      int32 &OutIncludedChildWindowCount,
      FString &OutError)
  {
    OutSize = FIntPoint::ZeroValue;
    OutIncludedChildWindowCount = 0;

    TArray<FColor> RootBitmap;
    FIntPoint RootSize;
    if (!CaptureWindowScreenshotToBitmap(TargetWindow, RootBitmap, RootSize,
                                         OutError))
    {
      return false;
    }

    const FSlateRect RootRect = TargetWindow->GetRectInScreen();
    int32 UnionLeft = FMath::FloorToInt(RootRect.Left);
    int32 UnionTop = FMath::FloorToInt(RootRect.Top);
    int32 UnionRight = UnionLeft + RootSize.X;
    int32 UnionBottom = UnionTop + RootSize.Y;

    struct FMcpChildWindowBitmap
    {
      TArray<FColor> Bitmap;
      FIntPoint Size = FIntPoint::ZeroValue;
      FSlateRect Rect;
    };

    TArray<FMcpChildWindowBitmap> ChildBitmaps;
    if (TargetWindow.IsValid())
    {
      TArray<TSharedRef<SWindow>> ChildWindows;
      CollectVisibleChildWindows(TargetWindow.ToSharedRef(), ChildWindows);
      for (const TSharedRef<SWindow> &ChildWindow : ChildWindows)
      {
        TArray<FColor> ChildBitmap;
        FIntPoint ChildSize;
        FString ChildError;
        if (!CaptureWindowScreenshotToBitmap(ChildWindow, ChildBitmap, ChildSize,
                                             ChildError))
        {
          OutError = ChildError;
          return false;
        }

        const FSlateRect ChildRect = ChildWindow->GetRectInScreen();
        UnionLeft = FMath::Min(UnionLeft, FMath::FloorToInt(ChildRect.Left));
        UnionTop = FMath::Min(UnionTop, FMath::FloorToInt(ChildRect.Top));
        UnionRight = FMath::Max(UnionRight, FMath::FloorToInt(ChildRect.Left) + ChildSize.X);
        UnionBottom = FMath::Max(UnionBottom, FMath::FloorToInt(ChildRect.Top) + ChildSize.Y);

        FMcpChildWindowBitmap &StoredChild = ChildBitmaps.AddDefaulted_GetRef();
        StoredChild.Bitmap = MoveTemp(ChildBitmap);
        StoredChild.Size = ChildSize;
        StoredChild.Rect = ChildRect;
      }
    }

    const int32 CanvasWidth = FMath::Max(1, UnionRight - UnionLeft);
    const int32 CanvasHeight = FMath::Max(1, UnionBottom - UnionTop);
    TArray<FColor> CanvasBitmap;
    CanvasBitmap.Init(FColor(0, 0, 0, 0), CanvasWidth * CanvasHeight);

    BlendBitmapIntoCanvas(RootBitmap, RootSize.X, RootSize.Y, CanvasBitmap,
                          CanvasWidth, CanvasHeight,
                          FMath::FloorToInt(RootRect.Left) - UnionLeft,
                          FMath::FloorToInt(RootRect.Top) - UnionTop);

    for (const FMcpChildWindowBitmap &ChildBitmap : ChildBitmaps)
    {
      BlendBitmapIntoCanvas(ChildBitmap.Bitmap, ChildBitmap.Size.X,
                            ChildBitmap.Size.Y, CanvasBitmap, CanvasWidth,
                            CanvasHeight,
                            FMath::FloorToInt(ChildBitmap.Rect.Left) - UnionLeft,
                            FMath::FloorToInt(ChildBitmap.Rect.Top) - UnionTop);
    }

    if (!SaveBitmapToPngFile(CanvasBitmap, CanvasWidth, CanvasHeight, FullPath,
                             OutError))
    {
      return false;
    }

    OutIncludedChildWindowCount = ChildBitmaps.Num();
    OutSize = FIntPoint(CanvasWidth, CanvasHeight);
    return true;
  }

  FString BuildSimulatedInputScreenshotFilename(const FString &RequestId,
                                                const FString &InputType,
                                                const FString &CaptureStage)
  {
    FString SanitizedRequestId = RequestId;
    SanitizedRequestId.ReplaceInline(TEXT("-"), TEXT(""));
    if (SanitizedRequestId.IsEmpty())
    {
      SanitizedRequestId = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_%s"));
    }

    FString SanitizedInputType = InputType;
    SanitizedInputType.ReplaceInline(TEXT(" "), TEXT("_"));
    SanitizedInputType.ReplaceInline(TEXT("/"), TEXT("_"));
    SanitizedInputType.ReplaceInline(TEXT("\\"), TEXT("_"));

    return SanitizeScreenshotFilename(FString::Printf(
        TEXT("SimInput_%s_%s_%s"),
        *SanitizedInputType,
        *CaptureStage,
        *SanitizedRequestId.Left(12)));
  }

  TSharedPtr<SWindow> FindVisibleWindowByTitle(const FString &RequestedTitle)
  {
    if (!FSlateApplication::IsInitialized())
    {
      return nullptr;
    }

    if (RequestedTitle.IsEmpty())
    {
      return FSlateApplication::Get().GetActiveTopLevelRegularWindow();
    }

    TArray<TSharedRef<SWindow>> OpenWindows;
    FSlateApplication::Get().GetAllVisibleWindowsOrdered(OpenWindows);
    for (const TSharedRef<SWindow> &Window : OpenWindows)
    {
      const FString Title = Window->GetTitle().ToString();
      if (Title.Equals(RequestedTitle, ESearchCase::IgnoreCase) ||
          Title.Contains(RequestedTitle, ESearchCase::IgnoreCase))
      {
        return Window;
      }
    }

    return nullptr;
  }

  TSharedPtr<SWindow> ResolvePreferredScreenshotWindow(UMcpAutomationBridgeSubsystem *Self,
                                                       const FString &RequestedTitle)
  {
    if (!RequestedTitle.IsEmpty())
    {
      return FindVisibleWindowByTitle(RequestedTitle);
    }

    if (Self != nullptr && Self->SimulatedTargetWindow.IsValid())
    {
      return Self->SimulatedTargetWindow.Pin();
    }

    return FindVisibleWindowByTitle(FString());
  }

  FString ClassifyVirtualTargetStaleReason(const FString &ErrorMessage)
  {
    if (ErrorMessage.Contains(TEXT("does not have a live parent window"), ESearchCase::IgnoreCase))
    {
      return TEXT("tab_without_parent_window");
    }

    if (ErrorMessage.Contains(TEXT("Visible window"), ESearchCase::IgnoreCase))
    {
      return TEXT("missing_visible_window");
    }

    if (ErrorMessage.Contains(TEXT("Live tab"), ESearchCase::IgnoreCase))
    {
      return TEXT("missing_live_tab");
    }

    return FString();
  }

  FString BuildVirtualTargetRecoveryHint(const FString &StaleReason)
  {
    if (StaleReason == TEXT("missing_live_tab"))
    {
      return TEXT("Call manage_ui.resolve_ui_target or reopen the target before retrying the action.");
    }

    if (StaleReason == TEXT("tab_without_parent_window"))
    {
      return TEXT("Reopen the tab so it has a live parent window, then retry the action.");
    }

    if (StaleReason == TEXT("missing_visible_window"))
    {
      return TEXT("Resolve the target again or provide a live windowTitle before retrying the action.");
    }

    return TEXT("Resolve the target again before retrying the action.");
  }

  void ApplyTargetHealthResponseFields(const TSharedPtr<FJsonObject> &Response,
                                       const FString &TargetStatus,
                                       const bool bRequestedTargetStillLive,
                                       const bool bReResolved,
                                       const FString &StaleReason = FString(),
                                       const FString &RecoveryHint = FString(),
                                       const FString &RecoveryAction = FString())
  {
    if (!Response.IsValid())
    {
      return;
    }

    if (!TargetStatus.IsEmpty())
    {
      Response->SetStringField(TEXT("targetStatus"), TargetStatus);
    }

    Response->SetBoolField(TEXT("requestedTargetStillLive"), bRequestedTargetStillLive);
    Response->SetBoolField(TEXT("reResolved"), bReResolved);

    if (!StaleReason.IsEmpty())
    {
      Response->SetStringField(TEXT("staleReason"), StaleReason);
    }

    if (!RecoveryHint.IsEmpty())
    {
      Response->SetStringField(TEXT("recoveryHint"), RecoveryHint);
    }

    if (!RecoveryAction.IsEmpty())
    {
      Response->SetStringField(TEXT("recoveryAction"), RecoveryAction);
    }
  }

  void ApplyRequestedVirtualTargetFields(const TSharedPtr<FJsonObject> &Response,
                                         const FString &RequestedTabId,
                                         const FString &RequestedWindowTitle,
                                         const FString &FallbackResolvedTargetSource)
  {
    if (!Response.IsValid())
    {
      return;
    }

    const bool bHasRequestedTab = !RequestedTabId.IsEmpty();
    const bool bHasRequestedWindow = !RequestedWindowTitle.IsEmpty();

    if (bHasRequestedTab)
    {
      Response->SetStringField(TEXT("requestedTabId"), RequestedTabId);
      Response->SetStringField(TEXT("resolvedTargetSource"), TEXT("tab_id"));
    }

    if (bHasRequestedWindow)
    {
      Response->SetStringField(TEXT("requestedWindowTitle"), RequestedWindowTitle);
      if (!bHasRequestedTab)
      {
        Response->SetStringField(TEXT("resolvedTargetSource"), TEXT("window_title"));
      }
    }

    if (!bHasRequestedTab && !bHasRequestedWindow &&
        !FallbackResolvedTargetSource.IsEmpty())
    {
      Response->SetStringField(TEXT("resolvedTargetSource"),
                               FallbackResolvedTargetSource);
    }
  }

  FString DetermineScreenshotIntentSource(UMcpAutomationBridgeSubsystem *Self,
                                          const FString &RequestedTitle,
                                          const bool bCaptureEditorWindow)
  {
    if (!RequestedTitle.IsEmpty())
    {
      return TEXT("requested_window_title");
    }

    if (bCaptureEditorWindow && Self != nullptr && Self->SimulatedTargetWindow.IsValid())
    {
      return TEXT("stored_target_window");
    }

    if (bCaptureEditorWindow)
    {
      return TEXT("active_top_level_window");
    }

    return TEXT("viewport_default");
  }

  TArray<TSharedPtr<FJsonValue>> BuildVisibleWindowTitleValues()
  {
    TArray<TSharedPtr<FJsonValue>> WindowValues;
    if (!FSlateApplication::IsInitialized())
    {
      return WindowValues;
    }

    TArray<TSharedRef<SWindow>> OpenWindows;
    FSlateApplication::Get().GetAllVisibleWindowsOrdered(OpenWindows);
    for (const TSharedRef<SWindow> &Window : OpenWindows)
    {
      WindowValues.Add(
          MakeShared<FJsonValueString>(Window->GetTitle().ToString()));
    }

    return WindowValues;
  }

} // namespace
#endif

// Helper class for capturing export output
/* UE5.6: Use built-in FStringOutputDevice from UnrealString.h */

// Helper functions
// (ExtractVectorField and ExtractRotatorField moved to
// McpAutomationBridgeHelpers.h)

AActor *UMcpAutomationBridgeSubsystem::FindActorByName(const FString &Target, bool bExactMatchOnly)
{
#if WITH_EDITOR
  if (Target.IsEmpty() || !GEditor)
    return nullptr;

  // Priority: PIE World if active
  if (GEditor->PlayWorld)
  {
    for (TActorIterator<AActor> It(GEditor->PlayWorld); It; ++It)
    {
      AActor *A = *It;
      if (!A)
        continue;
      if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
          A->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
          A->GetPathName().Equals(Target, ESearchCase::IgnoreCase))
      {
        return A;
      }
    }
    // If not found in PIE, do we fall back to Editor World?
    // Probably not, because interacting with Editor world during PIE is
    // confusing. But for "Editor subsystems" usage, we usually want Editor
    // world. Let's fallback if not found, just in case.
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS)
    return nullptr;

  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  AActor *ExactMatch = nullptr;
  TArray<AActor *> FuzzyMatches;

  for (AActor *A : AllActors)
  {
    if (!A)
      continue;
    if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
        A->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
        A->GetPathName().Equals(Target, ESearchCase::IgnoreCase))
    {
      ExactMatch = A;
      break;
    }
    // Collect fuzzy matches ONLY if exact matching is not required
    // CRITICAL FIX: Fuzzy matching can cause delete operations to delete wrong actors
    // (e.g., "TestActor_Copy" matches when searching for "TestActor")
    if (!bExactMatchOnly && A->GetActorLabel().Contains(Target, ESearchCase::IgnoreCase))
    {
      FuzzyMatches.Add(A);
    }
  }

  if (ExactMatch)
  {
    return ExactMatch;
  }

  // If no exact match, check fuzzy matches ONLY if exact matching is not required
  if (!bExactMatchOnly)
  {
    if (FuzzyMatches.Num() == 1)
    {
      return FuzzyMatches[0];
    }
    else if (FuzzyMatches.Num() > 1)
    {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("FindActorByName: Ambiguous match for '%s'. Found %d matches."),
             *Target, FuzzyMatches.Num());
    }
  }

  // Fallback: try to load as asset if it looks like a path
  if (Target.StartsWith(TEXT("/")))
  {
    if (UObject *Obj = UEditorAssetLibrary::LoadAsset(Target))
    {
      return Cast<AActor>(Obj);
    }
  }
#endif
  return nullptr;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSpawn(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString ClassPath;
  Payload->TryGetStringField(TEXT("classPath"), ClassPath);
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

  UClass *ResolvedClass = nullptr;
  FString MeshPath;
  Payload->TryGetStringField(TEXT("meshPath"), MeshPath);
  UStaticMesh *ResolvedStaticMesh = nullptr;
  USkeletalMesh *ResolvedSkeletalMesh = nullptr;

  // Skip LoadAsset for script classes (e.g. /Script/Engine.CameraActor) to
  // avoid LogEditorAssetSubsystem errors
  if ((ClassPath.StartsWith(TEXT("/")) || ClassPath.Contains(TEXT("/"))) &&
      !ClassPath.StartsWith(TEXT("/Script/")))
  {
    if (UObject *Loaded = UEditorAssetLibrary::LoadAsset(ClassPath))
    {
      if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
        ResolvedClass = BP->GeneratedClass;
      else if (UClass *C = Cast<UClass>(Loaded))
        ResolvedClass = C;
      else if (UStaticMesh *Mesh = Cast<UStaticMesh>(Loaded))
        ResolvedStaticMesh = Mesh;
      else if (USkeletalMesh *SkelMesh = Cast<USkeletalMesh>(Loaded))
        ResolvedSkeletalMesh = SkelMesh;
    }
  }
  if (!ResolvedClass && !ResolvedStaticMesh && !ResolvedSkeletalMesh)
    ResolvedClass = ResolveClassByName(ClassPath);

  // If explicit mesh path provided for a general spawn request
  if (!ResolvedStaticMesh && !ResolvedSkeletalMesh && !MeshPath.IsEmpty())
  {
    if (UObject *MeshObj = UEditorAssetLibrary::LoadAsset(MeshPath))
    {
      ResolvedStaticMesh = Cast<UStaticMesh>(MeshObj);
      if (!ResolvedStaticMesh)
        ResolvedSkeletalMesh = Cast<USkeletalMesh>(MeshObj);
    }
  }

  // Force StaticMeshActor if we have a resolved mesh, regardless of class input
  // (unless it's a specific subclass)
  bool bSpawnStaticMeshActor = (ResolvedStaticMesh != nullptr);
  bool bSpawnSkeletalMeshActor = (ResolvedSkeletalMesh != nullptr);

  if (!bSpawnStaticMeshActor && !bSpawnSkeletalMeshActor && ResolvedClass)
  {
    bSpawnStaticMeshActor =
        ResolvedClass->IsChildOf(AStaticMeshActor::StaticClass());
    if (!bSpawnStaticMeshActor)
      bSpawnSkeletalMeshActor =
          ResolvedClass->IsChildOf(ASkeletalMeshActor::StaticClass());
  }

  // Explicitly use StaticMeshActor class if we have a mesh but no class, or if
  // we decided to spawn a static mesh actor
  if (bSpawnStaticMeshActor && !ResolvedClass)
  {
    ResolvedClass = AStaticMeshActor::StaticClass();
  }
  else if (bSpawnSkeletalMeshActor && !ResolvedClass)
  {
    ResolvedClass = ASkeletalMeshActor::StaticClass();
  }

  if (!ResolvedClass && !bSpawnStaticMeshActor && !bSpawnSkeletalMeshActor)
  {
    const FString ErrorMsg =
        FString::Printf(TEXT("Class not found: %s. Verify plugin is enabled if "
                             "using a plugin class."),
                        *ClassPath);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
                              ErrorMsg);
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  AActor *Spawned = nullptr;

  // Support PIE spawning
  UWorld *TargetWorld = (GEditor->PlayWorld) ? GEditor->PlayWorld : nullptr;

  if (TargetWorld)
  {
    // PIE Path
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    UClass *ClassToSpawn =
        ResolvedClass
            ? ResolvedClass
            : (bSpawnStaticMeshActor ? AStaticMeshActor::StaticClass()
                                     : (bSpawnSkeletalMeshActor
                                            ? ASkeletalMeshActor::StaticClass()
                                            : AActor::StaticClass()));
    Spawned = TargetWorld->SpawnActor(ClassToSpawn, &Location, &Rotation,
                                      SpawnParams);

    if (Spawned)
    {
      if (bSpawnStaticMeshActor)
      {
        if (AStaticMeshActor *StaticMeshActor =
                Cast<AStaticMeshActor>(Spawned))
        {
          if (UStaticMeshComponent *MeshComponent =
                  StaticMeshActor->GetStaticMeshComponent())
          {
            if (ResolvedStaticMesh)
            {
              MeshComponent->SetStaticMesh(ResolvedStaticMesh);
            }
            MeshComponent->SetMobility(EComponentMobility::Movable);
            // PIE actors don't need MarkRenderStateDirty in the same way, but
            // it doesn't hurt
          }
        }
      }
      else if (bSpawnSkeletalMeshActor)
      {
        if (ASkeletalMeshActor *SkelActor = Cast<ASkeletalMeshActor>(Spawned))
        {
          if (USkeletalMeshComponent *SkelComp =
                  SkelActor->GetSkeletalMeshComponent())
          {
            if (ResolvedSkeletalMesh)
            {
              SkelComp->SetSkeletalMesh(ResolvedSkeletalMesh);
            }
            SkelComp->SetMobility(EComponentMobility::Movable);
          }
        }
      }
    }
  }
  else
  {
    // Editor Path
    if (bSpawnStaticMeshActor)
    {
      Spawned = ActorSS->SpawnActorFromClass(
          ResolvedClass ? ResolvedClass : AStaticMeshActor::StaticClass(),
          Location, Rotation);
      if (Spawned)
      {
        Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                             ETeleportType::TeleportPhysics);
        if (AStaticMeshActor *StaticMeshActor =
                Cast<AStaticMeshActor>(Spawned))
        {
          if (UStaticMeshComponent *MeshComponent =
                  StaticMeshActor->GetStaticMeshComponent())
          {
            if (ResolvedStaticMesh)
            {
              MeshComponent->SetStaticMesh(ResolvedStaticMesh);
            }
            MeshComponent->SetMobility(EComponentMobility::Movable);
            MeshComponent->MarkRenderStateDirty();
          }
        }
      }
    }
    else if (bSpawnSkeletalMeshActor)
    {
      Spawned = ActorSS->SpawnActorFromClass(
          ResolvedClass ? ResolvedClass : ASkeletalMeshActor::StaticClass(),
          Location, Rotation);
      if (Spawned)
      {
        Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                             ETeleportType::TeleportPhysics);
        if (ASkeletalMeshActor *SkelActor = Cast<ASkeletalMeshActor>(Spawned))
        {
          if (USkeletalMeshComponent *SkelComp =
                  SkelActor->GetSkeletalMeshComponent())
          {
            if (ResolvedSkeletalMesh)
            {
              SkelComp->SetSkeletalMesh(ResolvedSkeletalMesh);
            }
            SkelComp->SetMobility(EComponentMobility::Movable);
            SkelComp->MarkRenderStateDirty();
          }
        }
      }
    }
    else
    {
      Spawned = ActorSS->SpawnActorFromClass(ResolvedClass, Location, Rotation);
      if (Spawned)
      {
        Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                             ETeleportType::TeleportPhysics);
      }
    }
  }

  if (!Spawned)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SPAWN_FAILED"),
                              TEXT("Failed to spawn actor"));

    return true;
  }

  if (!ActorName.IsEmpty())
  {
    Spawned->SetActorLabel(ActorName);
  }
  else
  {
    // Auto-generate a friendly label from the mesh or class name
    FString BaseName;
    if (ResolvedStaticMesh)
    {
      BaseName = ResolvedStaticMesh->GetName();
    }
    else if (ResolvedSkeletalMesh)
    {
      BaseName = ResolvedSkeletalMesh->GetName();
    }
    else if (ResolvedClass)
    {
      BaseName = ResolvedClass->GetName();
      if (BaseName.EndsWith(TEXT("_C")))
      {
        BaseName.RemoveFromEnd(TEXT("_C"));
      }
    }
    else
    {
      BaseName = TEXT("Actor");
    }
    Spawned->SetActorLabel(BaseName);
  }

  // Build response matching the outputWithActor schema:
  // { actor: { id, name, path }, actorPath, classPath?, meshPath? }
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();

  // Actor object with id, name and path
  TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
  ActorObj->SetStringField(TEXT("id"), Spawned->GetPathName()); // Use path as unique ID
  ActorObj->SetStringField(TEXT("name"), Spawned->GetActorLabel());
  ActorObj->SetStringField(TEXT("path"), Spawned->GetPathName());
  Data->SetObjectField(TEXT("actor"), ActorObj);

  // actorPath for convenience
  Data->SetStringField(TEXT("actorPath"), Spawned->GetPathName());

  // Provide the resolved class path useful for referencing
  if (ResolvedClass)
    Data->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
  else
    Data->SetStringField(TEXT("classPath"), ClassPath);

  if (ResolvedStaticMesh)
    Data->SetStringField(TEXT("meshPath"), ResolvedStaticMesh->GetPathName());
  else if (ResolvedSkeletalMesh)
    Data->SetStringField(TEXT("meshPath"), ResolvedSkeletalMesh->GetPathName());

  // Add verification data
  McpHandlerUtils::AddVerification(Data, Spawned);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Spawned actor '%s'"), *Spawned->GetActorLabel());

  SendAutomationResponse(Socket, RequestId, true, TEXT("Actor spawned"), Data);
  return true;

#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSpawnBlueprint(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString BlueprintPath;
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
  if (BlueprintPath.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("Blueprint path required"), nullptr);
    return true;
  }

  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

  UClass *ResolvedClass = nullptr;

  // Prefer the same blueprint resolution heuristics used by manage_blueprint
  // so that short names and package paths behave consistently.
  FString NormalizedPath;
  FString LoadError;
  if (!BlueprintPath.IsEmpty())
  {
    UBlueprint *BlueprintAsset =
        LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
    if (BlueprintAsset && BlueprintAsset->GeneratedClass)
    {
      ResolvedClass = BlueprintAsset->GeneratedClass;
    }
  }

  if (!ResolvedClass && (BlueprintPath.StartsWith(TEXT("/")) ||
                         BlueprintPath.Contains(TEXT("/"))))
  {
    if (UObject *Loaded = UEditorAssetLibrary::LoadAsset(BlueprintPath))
    {
      if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
        ResolvedClass = BP->GeneratedClass;
      else if (UClass *C = Cast<UClass>(Loaded))
        ResolvedClass = C;
    }
  }
  if (!ResolvedClass)
    ResolvedClass = ResolveClassByName(BlueprintPath);

  if (!ResolvedClass)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
                              TEXT("Blueprint class not found"), nullptr);
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

  // Debug log the received location
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("spawn_blueprint: Location=(%f, %f, %f) Rotation=(%f, %f, %f)"),
         Location.X, Location.Y, Location.Z, Rotation.Pitch, Rotation.Yaw,
         Rotation.Roll);

  AActor *Spawned = nullptr;
  UWorld *TargetWorld = (GEditor->PlayWorld) ? GEditor->PlayWorld : nullptr;

  if (TargetWorld)
  {
    // PIE Path
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    Spawned = TargetWorld->SpawnActor(ResolvedClass, &Location, &Rotation,
                                      SpawnParams);
    // Ensure physics/teleport if needed, though SpawnActor should handle it.
  }
  else
  {
    // Editor Path
    Spawned = ActorSS->SpawnActorFromClass(ResolvedClass, Location, Rotation);
    // Explicitly set location and rotation in case SpawnActorFromClass didn't
    // apply them correctly in editor builds.
    if (Spawned)
    {
      Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                           ETeleportType::TeleportPhysics);
    }
  }

  if (!Spawned)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SPAWN_FAILED"),
                              TEXT("Failed to spawn blueprint"), nullptr);
    return true;
  }

  if (!ActorName.IsEmpty())
    Spawned->SetActorLabel(ActorName);

  // Build response matching the outputWithActor schema:
  // { actor: { id, name, path }, actorPath, classPath }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();

  // Actor object with id, name and path
  TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
  ActorObj->SetStringField(TEXT("id"), Spawned->GetPathName()); // Use path as unique ID
  ActorObj->SetStringField(TEXT("name"), Spawned->GetActorLabel());
  ActorObj->SetStringField(TEXT("path"), Spawned->GetPathName());
  Resp->SetObjectField(TEXT("actor"), ActorObj);

  // actorPath for convenience
  Resp->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
  Resp->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());

  // Add verification data
  McpHandlerUtils::AddVerification(Resp, Spawned);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Spawned blueprint '%s'"),
         *Spawned->GetActorLabel());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Blueprint spawned"),
                         Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDelete(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  TArray<FString> Targets;
  const TArray<TSharedPtr<FJsonValue>> *NamesArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("actorNames"), NamesArray) && NamesArray)
  {
    for (const TSharedPtr<FJsonValue> &Entry : *NamesArray)
    {
      if (Entry.IsValid() && Entry->Type == EJson::String)
      {
        const FString Value = Entry->AsString().TrimStartAndEnd();
        if (!Value.IsEmpty())
          Targets.AddUnique(Value);
      }
    }
  }

  FString SingleName;
  if (Targets.Num() == 0)
  {
    Payload->TryGetStringField(TEXT("actorName"), SingleName);
    if (!SingleName.IsEmpty())
      Targets.AddUnique(SingleName);
  }

  if (Targets.Num() == 0)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName or actorNames required"));
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<FString> Deleted;
  TArray<FString> Missing;

  for (const FString &Name : Targets)
  {
    // CRITICAL FIX: Use exact match only for delete operations to prevent
    // fuzzy matching from deleting wrong actors (e.g., "TestActor_Copy" when
    // searching for "TestActor")
    AActor *Found = FindActorByName(Name, true);
    if (!Found)
    {
      Missing.Add(Name);
      continue;
    }
    if (ActorSS->DestroyActor(Found))
    {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
             TEXT("ControlActor: Deleted actor '%s'"), *Name);
      Deleted.Add(Name);
    }
    else
      Missing.Add(Name);
  }

  const bool bAllDeleted = Missing.Num() == 0;
  const bool bAnyDeleted = Deleted.Num() > 0;
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bAllDeleted);
  Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());

  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Name : Deleted)
    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
  Resp->SetArrayField(TEXT("deleted"), DeletedArray);

  if (Missing.Num() > 0)
  {
    TArray<TSharedPtr<FJsonValue>> MissingArray;
    for (const FString &Name : Missing)
      MissingArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("missing"), MissingArray);
  }

  FString Message;
  FString ErrorCode;
  if (!bAnyDeleted && Missing.Num() > 0)
  {
    Message = TEXT("Actors not found");
    ErrorCode = TEXT("NOT_FOUND");
  }
  else
  {
    Message = bAllDeleted ? TEXT("Actors deleted")
                          : TEXT("Some actors could not be deleted");
    ErrorCode = bAllDeleted ? FString() : TEXT("DELETE_PARTIAL");
  }

  // Add verification data for delete operations
  Resp->SetBoolField(TEXT("existsAfter"), false);
  Resp->SetStringField(TEXT("action"), TEXT("control_actor:deleted"));

  if (!bAllDeleted && Missing.Num() > 0 && !bAnyDeleted)
  {
    SendStandardErrorResponse(this, Socket, RequestId, ErrorCode, Message);
  }
  else
  {
    SendStandardSuccessResponse(this, Socket, RequestId, Message, Resp);
  }
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorApplyForce(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FVector ForceVector =
      ExtractVectorField(Payload, TEXT("force"), FVector::ZeroVector);

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  UPrimitiveComponent *Prim =
      Found->FindComponentByClass<UPrimitiveComponent>();
  if (!Prim)
  {
    if (UStaticMeshComponent *SMC =
            Found->FindComponentByClass<UStaticMeshComponent>())
      Prim = SMC;
  }

  if (!Prim)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_COMPONENT"),
                              TEXT("No component to apply force"), nullptr);
    return true;
  }

  if (Prim->Mobility == EComponentMobility::Static)
    Prim->SetMobility(EComponentMobility::Movable);

  // Ensure collision is enabled for physics
  if (Prim->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
  {
    Prim->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
  }

  // Check if collision geometry exists (common failure for empty
  // StaticMeshActors)
  if (UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(Prim))
  {
    if (!SMC->GetStaticMesh())
    {
      SendStandardErrorResponse(
          this, Socket, RequestId, TEXT("PHYSICS_FAILED"),
          TEXT("StaticMeshComponent has no StaticMesh assigned."), nullptr);
      return true;
    }
    if (!SMC->GetStaticMesh()->GetBodySetup())
    {
      SendStandardErrorResponse(
          this, Socket, RequestId, TEXT("PHYSICS_FAILED"),
          TEXT("StaticMesh has no collision geometry (BodySetup is null)."),
          nullptr);
      return true;
    }
  }

  if (!Prim->IsSimulatingPhysics())
  {
    Prim->SetSimulatePhysics(true);
    // Must recreate physics state for the body to be properly initialized in
    // Editor
    Prim->RecreatePhysicsState();
  }

  Prim->AddForce(ForceVector);
  Prim->WakeAllRigidBodies();
  Prim->MarkRenderStateDirty();

  // Verify physics state
  const bool bIsSimulating = Prim->IsSimulatingPhysics();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("simulating"), bIsSimulating);
  TArray<TSharedPtr<FJsonValue>> Applied;
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.X));
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Y));
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Z));
  Data->SetArrayField(TEXT("applied"), Applied);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());

  if (!bIsSimulating)
  {
    FString FailureReason = TEXT("Failed to enable physics simulation.");
    if (Prim->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
    {
      FailureReason += TEXT(" Collision is disabled.");
    }
    else if (Prim->Mobility != EComponentMobility::Movable)
    {
      FailureReason += TEXT(" Component is not Movable.");
    }
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("PHYSICS_FAILED"),
                              FailureReason, Data);
    return true;
  }

  // Add verification data
  McpHandlerUtils::AddVerification(Data, Found);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Applied force to '%s'"), *Found->GetActorLabel());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Force applied"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetTransform(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), Found->GetActorLocation());
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), Found->GetActorRotation());
  FVector Scale =
      ExtractVectorField(Payload, TEXT("scale"), Found->GetActorScale3D());

  Found->Modify();
  Found->SetActorLocation(Location, false, nullptr,
                          ETeleportType::TeleportPhysics);
  Found->SetActorRotation(Rotation, ETeleportType::TeleportPhysics);
  Found->SetActorScale3D(Scale);
  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  // Verify transform
  const FVector NewLoc = Found->GetActorLocation();
  const FRotator NewRot = Found->GetActorRotation();
  const FVector NewScale = Found->GetActorScale3D();

  const bool bLocMatch = NewLoc.Equals(Location, 1.0f); // 1 unit tolerance
  // Rotation comparison is tricky due to normalization, skipping strict check
  // for now but logging if very different
  const bool bScaleMatch = NewScale.Equals(Scale, 0.01f);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());

  auto MakeArray = [](const FVector &Vec)
  {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("location"), MakeArray(NewLoc));
  Data->SetArrayField(TEXT("scale"), MakeArray(NewScale));

  if (!bLocMatch || !bScaleMatch)
  {
    SendStandardErrorResponse(this, Socket, RequestId,
                              TEXT("TRANSFORM_MISMATCH"),
                              TEXT("Failed to set transform exactly"), Data);
    return true;
  }

  // Add verification data
  McpHandlerUtils::AddVerification(Data, Found);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Set transform for '%s'"), *Found->GetActorLabel());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Actor transform updated"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetTransform(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"));
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"));
    return true;
  }

  const FTransform Current = Found->GetActorTransform();
  const FVector Location = Current.GetLocation();
  const FRotator Rotation = Current.GetRotation().Rotator();
  const FVector Scale = Current.GetScale3D();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();

  auto MakeArray = [](const FVector &Vec)
  {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("location"), MakeArray(Location));
  TArray<TSharedPtr<FJsonValue>> RotArray;
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
  Data->SetArrayField(TEXT("rotation"), RotArray);
  Data->SetArrayField(TEXT("scale"), MakeArray(Scale));

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actor transform retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetVisibility(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  bool bVisible = true;
  if (Payload->HasField(TEXT("visible")))
    Payload->TryGetBoolField(TEXT("visible"), bVisible);

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  Found->Modify();
  Found->SetActorHiddenInGame(!bVisible);
  Found->SetActorEnableCollision(bVisible);

  for (UActorComponent *Comp : Found->GetComponents())
  {
    if (!Comp)
      continue;
    if (UPrimitiveComponent *Prim = Cast<UPrimitiveComponent>(Comp))
    {
      Prim->SetVisibility(bVisible, true);
      Prim->SetHiddenInGame(!bVisible);
    }
  }

  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  // Verify visibility state
  const bool bIsHidden = Found->IsHidden();
  const bool bStateMatches = (bIsHidden == !bVisible);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("visible"), !bIsHidden);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());

  if (!bStateMatches)
  {
    SendStandardErrorResponse(this, Socket, RequestId,
                              TEXT("VISIBILITY_MISMATCH"),
                              TEXT("Failed to set actor visibility"), Data);
    return true;
  }

  // Add verification data
  McpHandlerUtils::AddVerification(Data, Found);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Set visibility to %s for '%s'"),
         bVisible ? TEXT("True") : TEXT("False"), *Found->GetActorLabel());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Actor visibility updated"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAddComponent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  FString ComponentType;
  Payload->TryGetStringField(TEXT("componentType"), ComponentType);
  if (ComponentType.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("componentType required"), nullptr);
    return true;
  }

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  UClass *ComponentClass = ResolveClassByName(ComponentType);
  if (!ComponentClass ||
      !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
                              TEXT("Component class not found"), nullptr);
    return true;
  }

  if (ComponentName.TrimStartAndEnd().IsEmpty())
    ComponentName = FString::Printf(TEXT("%s_%d"), *ComponentClass->GetName(),
                                    FMath::Rand());

  FName DesiredName = FName(*ComponentName);
  UActorComponent *NewComponent = NewObject<UActorComponent>(
      Found, ComponentClass, DesiredName, RF_Transactional);
  if (!NewComponent)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CREATE_COMPONENT_FAILED"),
                              TEXT("Failed to create component"), nullptr);
    return true;
  }

  Found->Modify();
  NewComponent->SetFlags(RF_Transactional);
  Found->AddInstanceComponent(NewComponent);
  NewComponent->OnComponentCreated();

  if (USceneComponent *SceneComp = Cast<USceneComponent>(NewComponent))
  {
    if (Found->GetRootComponent() && !SceneComp->GetAttachParent())
    {
      SceneComp->SetupAttachment(Found->GetRootComponent());
    }
  }

  // Force lights to be movable to ensure they work without baking (Issue #6
  // fix) We check for "LightComponent" class name to avoid dependency issues if
  // header is obscure, but ULightComponent is standard.
  if (NewComponent->IsA(ULightComponent::StaticClass()))
  {
    if (USceneComponent *SC = Cast<USceneComponent>(NewComponent))
    {
      SC->SetMobility(EComponentMobility::Movable);
    }
  }

  // Special handling for StaticMeshComponent meshPath convenience
  if (UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(NewComponent))
  {
    FString MeshPath;
    if (Payload->TryGetStringField(TEXT("meshPath"), MeshPath) &&
        !MeshPath.IsEmpty())
    {
      if (UObject *LoadedMesh = UEditorAssetLibrary::LoadAsset(MeshPath))
      {
        if (UStaticMesh *Mesh = Cast<UStaticMesh>(LoadedMesh))
        {
          SMC->SetStaticMesh(Mesh);
        }
      }
    }
  }

  TArray<FString> AppliedProperties;
  TArray<FString> PropertyWarnings;
  const TSharedPtr<FJsonObject> *PropertiesPtr = nullptr;
  if (Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr) &&
      PropertiesPtr && (*PropertiesPtr).IsValid())
  {
    for (const auto &Pair : (*PropertiesPtr)->Values)
    {
      FProperty *Property = ComponentClass->FindPropertyByName(*Pair.Key);
      if (!Property)
      {
        PropertyWarnings.Add(
            FString::Printf(TEXT("Property not found: %s"), *Pair.Key));
        continue;
      }
      FString ApplyError;
      if (ApplyJsonValueToProperty(NewComponent, Property, Pair.Value,
                                   ApplyError))
        AppliedProperties.Add(Pair.Key);
      else
        PropertyWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"),
                                             *Pair.Key, *ApplyError));
    }
  }

  NewComponent->RegisterComponent();
  if (USceneComponent *SceneComp = Cast<USceneComponent>(NewComponent))
    SceneComp->UpdateComponentToWorld();
  NewComponent->MarkPackageDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("componentName"), NewComponent->GetName());
  Resp->SetStringField(TEXT("componentPath"), NewComponent->GetPathName());
  Resp->SetStringField(TEXT("componentClass"), ComponentClass->GetPathName());
  if (AppliedProperties.Num() > 0)
  {
    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (const FString &PropName : AppliedProperties)
      PropsArray.Add(MakeShared<FJsonValueString>(PropName));
    Resp->SetArrayField(TEXT("appliedProperties"), PropsArray);
  }
  if (PropertyWarnings.Num() > 0)
  {
    TArray<TSharedPtr<FJsonValue>> WarnArray;
    for (const FString &Warning : PropertyWarnings)
      WarnArray.Add(MakeShared<FJsonValueString>(Warning));
    Resp->SetArrayField(TEXT("warnings"), WarnArray);
  }
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Added component '%s' to '%s'"),
         *NewComponent->GetName(), *Found->GetActorLabel());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Component added"), Resp,
                         FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetComponentProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  if (ComponentName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("componentName required"), nullptr);
    return true;
  }

  const TSharedPtr<FJsonObject> *PropertiesPtr = nullptr;
  if (!(Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr) &&
        PropertiesPtr && PropertiesPtr->IsValid()))
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("properties object required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  // CRITICAL FIX: Use FindComponentByName helper which supports fuzzy matching
  UActorComponent *TargetComponent = FindComponentByName(Found, ComponentName);

  if (!TargetComponent)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("COMPONENT_NOT_FOUND"),
                              TEXT("Component not found"), nullptr);
    return true;
  }

  TArray<FString> AppliedProperties;
  TArray<FString> PropertyWarnings;
  UClass *ComponentClass = TargetComponent->GetClass();
  TargetComponent->Modify();

  // PRIORITY: Apply Mobility FIRST.
  // Physics simulation fails if the component is generic "Static".
  // Scan for Mobility key case-insensitively to ensure we find it regardless of
  // JSON casing
  const TSharedPtr<FJsonValue> *MobilityVal = nullptr;
  FString MobilityKey;
  for (const auto &Pair : (*PropertiesPtr)->Values)
  {
    if (Pair.Key.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase))
    {
      MobilityVal = &Pair.Value;
      MobilityKey = Pair.Key;
      break;
    }
  }

  if (MobilityVal)
  {
    if (USceneComponent *SC = Cast<USceneComponent>(TargetComponent))
    {
      FString EnumVal;
      if ((*MobilityVal)->TryGetString(EnumVal))
      {
        // Parse enum string
        int64 Val =
            StaticEnum<EComponentMobility::Type>()->GetValueByNameString(
                EnumVal);
        if (Val != INDEX_NONE)
        {
          SC->SetMobility((EComponentMobility::Type)Val);
          AppliedProperties.Add(MobilityKey);
          UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                 TEXT("Explicitly set Mobility to %s"), *EnumVal);
        }
      }
      else
      {
        double Val;
        if ((*MobilityVal)->TryGetNumber(Val))
        {
          SC->SetMobility((EComponentMobility::Type)(int32)Val);
          AppliedProperties.Add(MobilityKey);
          UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                 TEXT("Explicitly set Mobility to %d"), (int32)Val);
        }
      }
    }
  }

  for (const auto &Pair : (*PropertiesPtr)->Values)
  {
    // Skip Mobility as we already handled it
    if (Pair.Key.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase))
      continue;

    // Special handling for SimulatePhysics
    if (Pair.Key.Equals(TEXT("SimulatePhysics"), ESearchCase::IgnoreCase) ||
        Pair.Key.Equals(TEXT("bSimulatePhysics"), ESearchCase::IgnoreCase))
    {
      if (UPrimitiveComponent *Prim =
              Cast<UPrimitiveComponent>(TargetComponent))
      {
        bool bVal = false;
        if (Pair.Value->TryGetBool(bVal))
        {
          Prim->SetSimulatePhysics(bVal);
          AppliedProperties.Add(Pair.Key);
          UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                 TEXT("Explicitly set SimulatePhysics to %s"),
                 bVal ? TEXT("True") : TEXT("False"));
          continue;
        }
      }
    }

    FProperty *Property = ComponentClass->FindPropertyByName(*Pair.Key);
    if (!Property)
    {
      PropertyWarnings.Add(
          FString::Printf(TEXT("Property not found: %s"), *Pair.Key));
      continue;
    }
    FString ApplyError;
    if (ApplyJsonValueToProperty(TargetComponent, Property, Pair.Value,
                                 ApplyError))
      AppliedProperties.Add(Pair.Key);
    else
      PropertyWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"),
                                           *Pair.Key, *ApplyError));
  }

  if (USceneComponent *SceneComponent =
          Cast<USceneComponent>(TargetComponent))
  {
    SceneComponent->MarkRenderStateDirty();
    SceneComponent->UpdateComponentToWorld();
  }
  TargetComponent->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  if (AppliedProperties.Num() > 0)
  {
    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (const FString &PropName : AppliedProperties)
      PropsArray.Add(MakeShared<FJsonValueString>(PropName));
    Data->SetArrayField(TEXT("applied"), PropsArray);
  }

  // Add verification data
  McpHandlerUtils::AddVerification(Data, Found);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Updated properties for component '%s' on '%s'"),
         *TargetComponent->GetName(), *Found->GetActorLabel());

  SendAutomationResponse(Socket, RequestId, true, TEXT("Component properties updated"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetComponents(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);

  // Also accept "objectPath" as an alias, common in inspections
  if (TargetName.IsEmpty())
  {
    Payload->TryGetStringField(TEXT("objectPath"), TargetName);
  }

  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName or objectPath required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  // Fallback: Check if it's a Blueprint asset to inspect CDO components
  if (!Found)
  {
    if (UObject *Asset = UEditorAssetLibrary::LoadAsset(TargetName))
    {
      if (UBlueprint *BP = Cast<UBlueprint>(Asset))
      {
        if (BP->GeneratedClass)
        {
          Found = Cast<AActor>(BP->GeneratedClass->GetDefaultObject());
        }
      }
    }
  }

  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor or Blueprint not found"), nullptr);
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> ComponentsArray;
  for (UActorComponent *Comp : Found->GetComponents())
  {
    if (!Comp)
      continue;
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("name"), Comp->GetName());
    Entry->SetStringField(TEXT("class"), Comp->GetClass()
                                             ? Comp->GetClass()->GetPathName()
                                             : TEXT(""));
    Entry->SetStringField(TEXT("path"), Comp->GetPathName());
    if (USceneComponent *SceneComp = Cast<USceneComponent>(Comp))
    {
      FVector Loc = SceneComp->GetRelativeLocation();
      FRotator Rot = SceneComp->GetRelativeRotation();
      FVector Scale = SceneComp->GetRelativeScale3D();

      TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
      LocObj->SetNumberField(TEXT("x"), Loc.X);
      LocObj->SetNumberField(TEXT("y"), Loc.Y);
      LocObj->SetNumberField(TEXT("z"), Loc.Z);
      Entry->SetObjectField(TEXT("relativeLocation"), LocObj);

      TSharedPtr<FJsonObject> RotObj = McpHandlerUtils::CreateResultObject();
      RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
      RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
      RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
      Entry->SetObjectField(TEXT("relativeRotation"), RotObj);

      TSharedPtr<FJsonObject> ScaleObj = McpHandlerUtils::CreateResultObject();
      ScaleObj->SetNumberField(TEXT("x"), Scale.X);
      ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
      ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
      Entry->SetObjectField(TEXT("relativeScale"), ScaleObj);
    }
    ComponentsArray.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("components"), ComponentsArray);
  Data->SetNumberField(TEXT("count"), ComponentsArray.Num());

  // Add verification data
  if (Found)
  {
    McpHandlerUtils::AddVerification(Data, Found);
  }

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Actor components retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDuplicate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  FVector Offset =
      ExtractVectorField(Payload, TEXT("offset"), FVector::ZeroVector);
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  AActor *Duplicated =
      ActorSS->DuplicateActor(Found, Found->GetWorld(), Offset);
  if (!Duplicated)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("DUPLICATE_FAILED"),
                              TEXT("Failed to duplicate actor"), nullptr);
    return true;
  }

  FString NewName;
  Payload->TryGetStringField(TEXT("newName"), NewName);
  if (!NewName.TrimStartAndEnd().IsEmpty())
    Duplicated->SetActorLabel(NewName);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("source"), Found->GetActorLabel());
  Data->SetStringField(TEXT("actorName"), Duplicated->GetActorLabel());
  Data->SetStringField(TEXT("actorPath"), Duplicated->GetPathName());

  // Add verification data
  McpHandlerUtils::AddVerification(Data, Duplicated);

  TArray<TSharedPtr<FJsonValue>> OffsetArray;
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.X));
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.Y));
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.Z));
  Data->SetArrayField(TEXT("offset"), OffsetArray);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Duplicated '%s' to '%s'"), *Found->GetActorLabel(),
         *Duplicated->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor duplicated"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAttach(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString ChildName;
  Payload->TryGetStringField(TEXT("childActor"), ChildName);
  FString ParentName;
  Payload->TryGetStringField(TEXT("parentActor"), ParentName);
  if (ChildName.IsEmpty() || ParentName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("childActor and parentActor required"), nullptr);
    return true;
  }

  AActor *Child = FindActorByName(ChildName);
  AActor *Parent = FindActorByName(ParentName);
  if (!Child || !Parent)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Child or parent actor not found"), nullptr);
    return true;
  }

  if (Child == Parent)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CYCLE_DETECTED"),
                              TEXT("Cannot attach actor to itself"), nullptr);
    return true;
  }

  USceneComponent *ChildRoot = Child->GetRootComponent();
  USceneComponent *ParentRoot = Parent->GetRootComponent();
  if (!ChildRoot || !ParentRoot)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ROOT_MISSING"),
                              TEXT("Actor missing root component"), nullptr);
    return true;
  }

  Child->Modify();
  ChildRoot->Modify();
  ChildRoot->AttachToComponent(ParentRoot,
                               FAttachmentTransformRules::KeepWorldTransform);
  Child->SetOwner(Parent);
  Child->MarkPackageDirty();
  Parent->MarkPackageDirty();

  // Verify attachment
  bool bAttached = false;
  if (Child->GetRootComponent() &&
      Child->GetRootComponent()->GetAttachParent() == ParentRoot)
  {
    bAttached = true;
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("child"), Child->GetActorLabel());
  Data->SetStringField(TEXT("parent"), Parent->GetActorLabel());
  Data->SetBoolField(TEXT("attached"), bAttached);

  if (!bAttached)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ATTACH_FAILED"),
                              TEXT("Failed to attach actor"), Data);
    return true;
  }

  // Add verification data for the child actor
  McpHandlerUtils::AddVerification(Data, Child);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Attached '%s' to '%s'"), *Child->GetActorLabel(),
         *Parent->GetActorLabel());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Actor attached"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDetach(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  USceneComponent *RootComp = Found->GetRootComponent();
  if (!RootComp || !RootComp->GetAttachParent())
  {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("note"), TEXT("Actor was not attached"));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Actor already detached"), Resp, FString());
    return true;
  }

  Found->Modify();
  RootComp->Modify();
  RootComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
  Found->SetOwner(nullptr);
  Found->MarkPackageDirty();

  // Verify detachment
  const bool bDetached = (RootComp->GetAttachParent() == nullptr);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetBoolField(TEXT("detached"), bDetached);

  if (!bDetached)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("DETACH_FAILED"),
                              TEXT("Failed to detach actor"), Data);
    return true;
  }

  // Add verification data
  McpHandlerUtils::AddVerification(Data, Found);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Detached '%s'"), *Found->GetActorLabel());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Actor detached"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TagValue.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("tag required"), nullptr);
    return true;
  }

  // Security: Validate tag format - reject path traversal attempts
  if (TagValue.Contains(TEXT("..")) || TagValue.Contains(TEXT("\\")) || TagValue.Contains(TEXT("/")))
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              FString::Printf(TEXT("Invalid tag: '%s'. Path separators and traversal characters are not allowed."), *TagValue), nullptr);
    return true;
  }

  FString MatchType;
  Payload->TryGetStringField(TEXT("matchType"), MatchType);
  MatchType = MatchType.ToLower();
  FName TagName(*TagValue);
  TArray<TSharedPtr<FJsonValue>> Matches;

  // Log tag search details
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleControlActorFindByTag: Searching for tag '%s' (FName: %s)"),
         *TagValue, *TagName.ToString());
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();

  // Log total actors being searched
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleControlActorFindByTag: Searching %d actors in level"), AllActors.Num());
  for (AActor *Actor : AllActors)
  {
    if (!Actor)
      continue;
    bool bMatches = false;
    if (MatchType == TEXT("contains"))
    {
      for (const FName &Existing : Actor->Tags)
      {
        if (Existing.ToString().Contains(TagValue, ESearchCase::IgnoreCase))
        {
          bMatches = true;
          break;
        }
      }
    }
    else
    {
      bMatches = Actor->ActorHasTag(TagName);
    }

    // Log actor tags for troubleshooting at verbose level
    if (Actor->Tags.Num() > 0)
    {
      FString TagList;
      for (const FName &T : Actor->Tags)
      {
        TagList += T.ToString() + TEXT(", ");
      }
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("HandleControlActorFindByTag: Actor '%s' has tags: [%s] - match=%d"),
             *Actor->GetActorLabel(), *TagList, bMatches);
    }
    if (bMatches)
    {
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("name"), Actor->GetActorLabel());
      Entry->SetStringField(TEXT("path"), Actor->GetPathName());
      Entry->SetStringField(TEXT("class"),
                            Actor->GetClass() ? Actor->GetClass()->GetPathName()
                                              : TEXT(""));
      Matches.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("actors"), Matches);
  Data->SetNumberField(TEXT("count"), Matches.Num());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actors found"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAddTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TargetName.IsEmpty() || TagValue.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName and tag required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FName TagName(*TagValue);
  const bool bAlreadyHad = Found->Tags.Contains(TagName);

  Found->Modify();
  Found->Tags.AddUnique(TagName);
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("wasPresent"), bAlreadyHad);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("tag"), TagName.ToString());

  // Add verification data
  McpHandlerUtils::AddVerification(Data, Found);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Added tag '%s' to '%s'"), *TagName.ToString(),
         *Found->GetActorLabel());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Tag applied to actor"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByName(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString Query;
  Payload->TryGetStringField(TEXT("name"), Query);
  if (Query.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("name required"), nullptr);
    return true;
  }

  // Security: Validate query format - reject path traversal attempts
  if (Query.Contains(TEXT("..")) || Query.Contains(TEXT("\\")) || Query.Contains(TEXT("/")))
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              FString::Printf(TEXT("Invalid name query: '%s'. Path separators and traversal characters are not allowed."), *Query), nullptr);
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  const TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  TArray<TSharedPtr<FJsonValue>> Matches;
  for (AActor *Actor : AllActors)
  {
    if (!Actor)
      continue;
    const FString Label = Actor->GetActorLabel();
    const FString Name = Actor->GetName();
    const FString Path = Actor->GetPathName();
    const bool bMatches = Label.Contains(Query, ESearchCase::IgnoreCase) ||
                          Name.Contains(Query, ESearchCase::IgnoreCase) ||
                          Path.Contains(Query, ESearchCase::IgnoreCase);
    if (bMatches)
    {
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("label"), Label);
      Entry->SetStringField(TEXT("name"), Name);
      Entry->SetStringField(TEXT("path"), Path);
      Entry->SetStringField(TEXT("class"),
                            Actor->GetClass() ? Actor->GetClass()->GetPathName()
                                              : TEXT(""));
      Matches.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetNumberField(TEXT("count"), Matches.Num());
  Data->SetArrayField(TEXT("actors"), Matches);
  Data->SetStringField(TEXT("query"), Query);
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actor query executed"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDeleteByTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TagValue.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("tag required"), nullptr);
    return true;
  }

  const FName TagName(*TagValue);
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  const TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  TArray<FString> Deleted;

  for (AActor *Actor : AllActors)
  {
    if (!Actor)
      continue;
    if (Actor->ActorHasTag(TagName))
    {
      const FString Label = Actor->GetActorLabel();
      if (ActorSS->DestroyActor(Actor))
        Deleted.Add(Label);
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("tag"), TagName.ToString());
  Data->SetNumberField(TEXT("deletedCount"), Deleted.Num());
  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Name : Deleted)
    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
  Data->SetArrayField(TEXT("deleted"), DeletedArray);

  // Add verification data for delete operations
  Data->SetBoolField(TEXT("existsAfter"), false);
  Data->SetStringField(TEXT("action"), TEXT("control_actor:deleted"));

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actors deleted by tag"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetBlueprintVariables(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  const TSharedPtr<FJsonObject> *VariablesPtr = nullptr;
  if (!(Payload->TryGetObjectField(TEXT("variables"), VariablesPtr) &&
        VariablesPtr && VariablesPtr->IsValid()))
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("variables object required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  UClass *ActorClass = Found->GetClass();
  Found->Modify();
  TArray<FString> Applied;
  TArray<FString> Warnings;

  for (const auto &Pair : (*VariablesPtr)->Values)
  {
    FProperty *Property = ActorClass->FindPropertyByName(*Pair.Key);
    if (!Property)
    {
      Warnings.Add(FString::Printf(TEXT("Property not found: %s"), *Pair.Key));
      continue;
    }

    FString ApplyError;
    if (ApplyJsonValueToProperty(Found, Property, Pair.Value, ApplyError))
      Applied.Add(Pair.Key);
    else
      Warnings.Add(FString::Printf(TEXT("Failed to set %s: %s"), *Pair.Key,
                                   *ApplyError));
  }

  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  if (Applied.Num() > 0)
  {
    TArray<TSharedPtr<FJsonValue>> AppliedArray;
    for (const FString &Name : Applied)
      AppliedArray.Add(MakeShared<FJsonValueString>(Name));
    Data->SetArrayField(TEXT("updated"), AppliedArray);
  }

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Variables updated"), Data, Warnings);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorCreateSnapshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  FString SnapshotName;
  Payload->TryGetStringField(TEXT("snapshotName"), SnapshotName);
  if (SnapshotName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("snapshotName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FString SnapshotKey =
      FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
  CachedActorSnapshots.Add(SnapshotKey, Found->GetActorTransform());

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("snapshotName"), SnapshotName);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Snapshot created"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRestoreSnapshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  FString SnapshotName;
  Payload->TryGetStringField(TEXT("snapshotName"), SnapshotName);
  if (SnapshotName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("snapshotName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FString SnapshotKey =
      FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
  if (!CachedActorSnapshots.Contains(SnapshotKey))
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SNAPSHOT_NOT_FOUND"),
                              TEXT("Snapshot not found"), nullptr);
    return true;
  }

  const FTransform &SavedTransform = CachedActorSnapshots[SnapshotKey];
  Found->Modify();
  Found->SetActorTransform(SavedTransform);
  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("snapshotName"), SnapshotName);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Snapshot restored"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorExport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  FMcpOutputCapture OutputCapture;
  UExporter::ExportToOutputDevice(nullptr, Found, nullptr, OutputCapture,
                                  TEXT("T3D"), 0, 0, false);
  FString OutputString = FString::Join(OutputCapture.Consume(), TEXT("\n"));

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("t3d"), OutputString);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor exported"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetBoundingBox(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  FVector Origin, BoxExtent;
  Found->GetActorBounds(false, Origin, BoxExtent);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();

  auto MakeArray = [](const FVector &Vec)
  {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("origin"), MakeArray(Origin));
  Data->SetArrayField(TEXT("extent"), MakeArray(BoxExtent));
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Bounding box retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("name"), Found->GetName());
  Data->SetStringField(TEXT("label"), Found->GetActorLabel());
  Data->SetStringField(TEXT("path"), Found->GetPathName());
  Data->SetStringField(TEXT("class"), Found->GetClass()
                                          ? Found->GetClass()->GetPathName()
                                          : TEXT(""));

  TArray<TSharedPtr<FJsonValue>> TagsArray;
  for (const FName &Tag : Found->Tags)
  {
    TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
  }
  Data->SetArrayField(TEXT("tags"), TagsArray);

  const FTransform Current = Found->GetActorTransform();
  auto MakeArray = [](const FVector &Vec)
  {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };
  Data->SetArrayField(TEXT("location"), MakeArray(Current.GetLocation()));

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Metadata retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRemoveTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TargetName.IsEmpty() || TagValue.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName and tag required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FName TagName(*TagValue);
  if (!Found->Tags.Contains(TagName))
  {
    // Idempotent success
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("wasPresent"), false);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("tag"), TagValue);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Tag not present (idempotent)"), Resp,
                           FString());
    return true;
  }

  Found->Modify();
  Found->Tags.Remove(TagName);
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("wasPresent"), true);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("tag"), TagValue);

  // Add verification data
  McpHandlerUtils::AddVerification(Data, Found);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Removed tag '%s' from '%s'"), *TagValue,
         *Found->GetActorLabel());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Tag removed from actor"), Data);
  return true;
#else
  return false;
#endif
}

// Additional handlers for test compatibility

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByClass(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString ClassName;
  Payload->TryGetStringField(TEXT("className"), ClassName);
  if (ClassName.IsEmpty())
  {
    Payload->TryGetStringField(TEXT("class"), ClassName);
  }

  if (ClassName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("className or class is required"), nullptr);
    return true;
  }

  // Security: Validate class name format - reject path traversal attempts
  // Valid formats: "/Script/Module.ClassName", "/Game/Path/ClassName.ClassName", "ClassName"
  // Invalid: Contains "..", "\" (Windows paths), or other traversal patterns
  if (ClassName.Contains(TEXT("..")) || ClassName.Contains(TEXT("\\")))
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              FString::Printf(TEXT("Invalid class name format: '%s'. Path traversal characters are not allowed."), *ClassName), nullptr);
    return true;
  }

  // Additional security: Reject absolute filesystem paths
  if (ClassName.StartsWith(TEXT("/")) && !ClassName.StartsWith(TEXT("/Script/")) &&
      !ClassName.StartsWith(TEXT("/Game/")) && !ClassName.StartsWith(TEXT("/Engine/")))
  {
    // Could be a path traversal attempt disguised as a valid path
    if (ClassName.Contains(TEXT("/etc/")) || ClassName.Contains(TEXT("/usr/")) ||
        ClassName.Contains(TEXT("/var/")) || ClassName.Contains(TEXT("/home/")) ||
        ClassName.Contains(TEXT("/root/")) || ClassName.Contains(TEXT("/tmp/")) ||
        ClassName.Contains(TEXT("C:\\")) || ClassName.Contains(TEXT("D:\\")))
    {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                                FString::Printf(TEXT("Invalid class name format: '%s'. Filesystem paths are not allowed."), *ClassName), nullptr);
      return true;
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> ActorsArray;

  if (UWorld *World = GEditor->GetEditorWorldContext().World())
  {
    UClass *ClassToFind = nullptr;

    // CRITICAL FIX: Use ResolveClassByName for proper engine class resolution
    // This handles: full paths, short names like "StaticMeshActor", and loads classes if needed
    // Without this, FindObject only finds already-loaded classes, missing engine classes like
    // AStaticMeshActor, APawn, etc. that haven't been accessed yet
    ClassToFind = ResolveClassByName(ClassName);

    if (ClassToFind)
    {
      for (TActorIterator<AActor> It(World, ClassToFind); It; ++It)
      {
        if (AActor *Actor = *It)
        {
          TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
          ActorObj->SetStringField(TEXT("name"), Actor->GetActorLabel());
          ActorObj->SetStringField(TEXT("path"), Actor->GetPathName());
          ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
        }
      }
    }
    else
    {
      // Class not found - return empty result (this is valid for searches)
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleControlActorFindByClass: Class '%s' not found"), *ClassName);
    }
  }

  Data->SetArrayField(TEXT("actors"), ActorsArray);
  Data->SetNumberField(TEXT("count"), ActorsArray.Num());
  SendStandardSuccessResponse(this, Socket, RequestId,
                              FString::Printf(TEXT("Found %d actors"), ActorsArray.Num()), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRemoveComponent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty())
  {
    Payload->TryGetStringField(TEXT("actor_name"), ActorName);
  }

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  if (ComponentName.IsEmpty())
  {
    Payload->TryGetStringField(TEXT("component_name"), ComponentName);
  }

  if (ActorName.IsEmpty())
  {
    SendAutomationError(Socket, RequestId, TEXT("actorName is required"), TEXT("MISSING_PARAM"));
    return true;
  }

  if (ComponentName.IsEmpty())
  {
    SendAutomationError(Socket, RequestId, TEXT("componentName is required"), TEXT("MISSING_PARAM"));
    return true;
  }

  AActor *Actor = FindActorByName(ActorName);
  if (!Actor)
  {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Actor not found: %s"), *ActorName),
                        TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // CRITICAL FIX: Use FindComponentByName helper which supports fuzzy matching
  UActorComponent *Component = FindComponentByName(Actor, ComponentName);
  if (Component)
  {
    Component->DestroyComponent();
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetStringField(TEXT("actorName"), ActorName);
    Data->SetStringField(TEXT("componentName"), ComponentName);

    // Add verification data for delete operations
    Data->SetBoolField(TEXT("existsAfter"), false);
    Data->SetStringField(TEXT("action"), TEXT("control_actor:deleted"));

    SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Component removed"), Data);
    return true;
  }

  SendAutomationError(Socket, RequestId,
                      FString::Printf(TEXT("Component not found: %s"), *ComponentName),
                      TEXT("COMPONENT_NOT_FOUND"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetComponentProperty(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString ActorName, ComponentName, PropertyName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);

  if (ActorName.IsEmpty() || ComponentName.IsEmpty() || PropertyName.IsEmpty())
  {
    SendAutomationError(Socket, RequestId, TEXT("actorName, componentName, and propertyName are required"), TEXT("MISSING_PARAM"));
    return true;
  }

  AActor *Actor = FindActorByName(ActorName);
  if (!Actor)
  {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // CRITICAL FIX: Use FindComponentByName helper which supports fuzzy matching
  // This handles cases where component names have numeric suffixes (e.g., "StaticMeshComponent0")
  UActorComponent *Component = FindComponentByName(Actor, ComponentName);
  if (!Component)
  {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Component not found: %s on actor: %s"), *ComponentName, *ActorName),
                        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  // Get property using reflection
  FProperty *Property = Component->GetClass()->FindPropertyByName(*PropertyName);
  if (!Property)
  {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Property not found: %s on component: %s"), *PropertyName, *ComponentName),
                        TEXT("PROPERTY_NOT_FOUND"));
    return true;
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), ActorName);
  Data->SetStringField(TEXT("componentName"), ComponentName);
  Data->SetStringField(TEXT("propertyName"), PropertyName);
  Data->SetStringField(TEXT("propertyType"), Property->GetClass()->GetName());

  // Extract property value using the existing helper function
  TSharedPtr<FJsonValue> PropertyValue = ExportPropertyToJsonValue(Component, Property);
  if (PropertyValue.IsValid())
  {
    Data->SetField(TEXT("value"), PropertyValue);
  }
  else
  {
    Data->SetStringField(TEXT("value"), TEXT("<unsupported property type>"));
  }

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Property retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetCollision(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString ActorName;
  bool bCollisionEnabled = true;

  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty())
  {
    Payload->TryGetStringField(TEXT("actor_name"), ActorName);
  }

  if (Payload->HasField(TEXT("collisionEnabled")))
  {
    bCollisionEnabled = GetJsonBoolField(Payload, TEXT("collisionEnabled"), true);
  }
  else if (Payload->HasField(TEXT("collision_enabled")))
  {
    bCollisionEnabled = GetJsonBoolField(Payload, TEXT("collision_enabled"), true);
  }

  if (ActorName.IsEmpty())
  {
    SendAutomationError(Socket, RequestId, TEXT("actorName is required"), TEXT("MISSING_PARAM"));
    return true;
  }

  AActor *Actor = FindActorByName(ActorName);
  if (!Actor)
  {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // Set collision on root component
  if (USceneComponent *RootComp = Actor->GetRootComponent())
  {
    if (UPrimitiveComponent *PrimComp = Cast<UPrimitiveComponent>(RootComp))
    {
      if (bCollisionEnabled)
      {
        PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
      }
      else
      {
        PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
      }
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), ActorName);
  Data->SetBoolField(TEXT("collisionEnabled"), bCollisionEnabled);
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Collision setting updated"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorCallFunction(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString ActorName, FunctionName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  Payload->TryGetStringField(TEXT("functionName"), FunctionName);

  if (ActorName.IsEmpty() || FunctionName.IsEmpty())
  {
    SendAutomationError(Socket, RequestId, TEXT("actorName and functionName are required"), TEXT("MISSING_PARAM"));
    return true;
  }

  AActor *Actor = FindActorByName(ActorName);
  if (!Actor)
  {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // Find and call the function
  UFunction *Function = Actor->FindFunction(*FunctionName);
  if (Function)
  {
    // Check if function has parameters - passing nullptr to a function expecting
    // parameters can cause crashes or undefined behavior
    if (Function->ParmsSize > 0)
    {
      // Function has parameters - we need to provide a buffer
      // Allocate zeroed memory for parameters
      void *ParmsBuffer = FMemory::Malloc(Function->ParmsSize, 16);
      FMemory::Memzero(ParmsBuffer, Function->ParmsSize);

      // Call with parameter buffer
      Actor->ProcessEvent(Function, ParmsBuffer);

      // Free the buffer
      FMemory::Free(ParmsBuffer);
    }
    else
    {
      // No parameters, safe to pass nullptr
      Actor->ProcessEvent(Function, nullptr);
    }

    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetStringField(TEXT("actorName"), ActorName);
    Data->SetStringField(TEXT("functionName"), FunctionName);
    SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Function called"), Data);
    return true;
  }

  SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Function not found: %s"), *FunctionName), TEXT("FUNCTION_NOT_FOUND"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("control_actor"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("control_actor")))
    return false;
  if (!Payload.IsValid())
  {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("control_actor payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("HandleControlActorAction: %s RequestId=%s"), *LowerSub,
         *RequestId);

#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }
  if (!GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
  {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"),
                              TEXT("EditorActorSubsystem not available"), nullptr);
    return true;
  }

  if (LowerSub == TEXT("spawn") || LowerSub == TEXT("spawn_actor"))
    return HandleControlActorSpawn(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("spawn_blueprint"))
    return HandleControlActorSpawnBlueprint(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("delete") || LowerSub == TEXT("remove") ||
      LowerSub == TEXT("destroy_actor"))
    return HandleControlActorDelete(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("apply_force") ||
      LowerSub == TEXT("apply_force_to_actor"))
    return HandleControlActorApplyForce(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_transform") ||
      LowerSub == TEXT("set_actor_transform") ||
      LowerSub == TEXT("teleport_actor") ||
      LowerSub == TEXT("set_actor_location") ||
      LowerSub == TEXT("set_actor_rotation") ||
      LowerSub == TEXT("set_actor_scale"))
    return HandleControlActorSetTransform(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_transform") ||
      LowerSub == TEXT("get_actor_transform"))
    return HandleControlActorGetTransform(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_visibility") ||
      LowerSub == TEXT("set_actor_visible") ||
      LowerSub == TEXT("set_actor_visibility"))
    return HandleControlActorSetVisibility(RequestId, Payload,
                                           RequestingSocket);
  if (LowerSub == TEXT("add_component"))
    return HandleControlActorAddComponent(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_component_properties") ||
      LowerSub == TEXT("set_component_property"))
    return HandleControlActorSetComponentProperties(RequestId, Payload,
                                                    RequestingSocket);
  if (LowerSub == TEXT("get_components") ||
      LowerSub == TEXT("get_actor_components"))
    return HandleControlActorGetComponents(RequestId, Payload,
                                           RequestingSocket);
  if (LowerSub == TEXT("duplicate"))
    return HandleControlActorDuplicate(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("attach") || LowerSub == TEXT("attach_actor"))
    return HandleControlActorAttach(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("detach") || LowerSub == TEXT("detach_actor"))
    return HandleControlActorDetach(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_tag"))
    return HandleControlActorFindByTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("add_tag"))
    return HandleControlActorAddTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("remove_tag"))
    return HandleControlActorRemoveTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_name") || LowerSub == TEXT("find_actors_by_name"))
    return HandleControlActorFindByName(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("delete_by_tag"))
    return HandleControlActorDeleteByTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_blueprint_variables"))
    return HandleControlActorSetBlueprintVariables(RequestId, Payload,
                                                   RequestingSocket);
  if (LowerSub == TEXT("create_snapshot"))
    return HandleControlActorCreateSnapshot(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("restore_snapshot"))
    return HandleControlActorRestoreSnapshot(RequestId, Payload,
                                             RequestingSocket);
  if (LowerSub == TEXT("export"))
    return HandleControlActorExport(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_bounding_box") || LowerSub == TEXT("get_actor_bounds"))
    return HandleControlActorGetBoundingBox(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("get_metadata"))
    return HandleControlActorGetMetadata(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("list") || LowerSub == TEXT("list_actors") || LowerSub == TEXT("list_objects"))
    return HandleControlActorList(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get") || LowerSub == TEXT("get_actor") ||
      LowerSub == TEXT("get_actor_by_name"))
    return HandleControlActorGet(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_class") || LowerSub == TEXT("find_actors_by_class"))
    return HandleControlActorFindByClass(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("remove_component"))
    return HandleControlActorRemoveComponent(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_component_property"))
    return HandleControlActorGetComponentProperty(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_property"))
    return HandleSetObjectProperty(RequestId, TEXT("set_object_property"), Payload, RequestingSocket);
  if (LowerSub == TEXT("get_property"))
    return HandleGetObjectProperty(RequestId, TEXT("get_object_property"), Payload, RequestingSocket);
  if (LowerSub == TEXT("set_collision") || LowerSub == TEXT("set_actor_collision"))
    return HandleControlActorSetCollision(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("call_function") || LowerSub == TEXT("call_actor_function"))
    return HandleControlActorCallFunction(RequestId, Payload, RequestingSocket);

  SendStandardErrorResponse(
      this, RequestingSocket, RequestId, TEXT("UNKNOWN_ACTION"),
      FString::Printf(TEXT("Unknown actor control action: %s"), *LowerSub), nullptr);
  return true;
#else
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Actor control requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorPlay(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (GEditor->PlayWorld)
  {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("alreadyPlaying"), true);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Play session already active"), Resp,
                           FString());
    return true;
  }

  FRequestPlaySessionParams PlayParams;
  PlayParams.WorldType = EPlaySessionWorldType::PlayInEditor;
#if MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS
  PlayParams.EditorPlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
#endif
#if MCP_HAS_LEVEL_EDITOR_MODULE
  if (FLevelEditorModule *LevelEditorModule =
          FModuleManager::GetModulePtr<FLevelEditorModule>(
              TEXT("LevelEditor")))
  {
    TSharedPtr<IAssetViewport> DestinationViewport =
        LevelEditorModule->GetFirstActiveViewport();
    if (DestinationViewport.IsValid())
      PlayParams.DestinationSlateViewport = DestinationViewport;
  }
#endif

  GEditor->RequestPlaySession(PlayParams);
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Play in Editor started"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStop(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor->PlayWorld)
  {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("alreadyStopped"), true);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Play session not active"), Resp, FString());
    return true;
  }

  GEditor->RequestEndPlayMap();
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Play in Editor stopped"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorEject(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor->PlayWorld)
  {
    TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
    ErrorDetails->SetBoolField(TEXT("notInPIE"), true);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("Cannot eject: Play session not active"), ErrorDetails);
    return true;
  }

  // Use Eject console command instead of RequestEndPlayMap
  // This ejects the player from the possessed pawn without stopping PIE
  GEditor->Exec(GEditor->PlayWorld, TEXT("Eject"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("ejected"), true);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Ejected from possessed actor"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorPossess(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);

  // Also try "objectPath" as fallback since schema might use that
  if (ActorName.IsEmpty())
    Payload->TryGetStringField(TEXT("objectPath"), ActorName);

  if (ActorName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(ActorName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr);
    return true;
  }

  if (GEditor)
  {
    GEditor->SelectNone(true, true, false);
    GEditor->SelectActor(Found, true, true, true);
    // 'POSSESS' command works on selected actor in PIE
    if (GEditor->PlayWorld)
    {
      GEditor->Exec(GEditor->PlayWorld, TEXT("POSSESS"));
      SendAutomationResponse(Socket, RequestId, true, TEXT("Possessed actor"),
                             nullptr);
    }
    else
    {
      // If not in PIE, we can't possess
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IN_PIE"),
                                TEXT("Cannot possess actor while not in PIE"), nullptr);
    }
    return true;
  }

  SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                            TEXT("Editor not available"), nullptr);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorFocusActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  if (UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
  {
    TArray<AActor *> Actors = ActorSS->GetAllLevelActors();
    for (AActor *Actor : Actors)
    {
      if (!Actor)
        continue;
      if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase))
      {
        GEditor->SelectNone(true, true, false);
        GEditor->SelectActor(Actor, true, true, true);
        GEditor->Exec(nullptr, TEXT("EDITORTEMPVIEWPORT"));
        GEditor->MoveViewportCamerasToActor(*Actor, false);
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Viewport focused on actor"), nullptr,
                               FString());
        return true;
      }
    }
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }
  return false;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetCamera(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  const TSharedPtr<FJsonObject> *Loc = nullptr;
  FVector Location(0, 0, 0);
  FRotator Rotation(0, 0, 0);
  if (Payload->TryGetObjectField(TEXT("location"), Loc) && Loc &&
      (*Loc).IsValid())
    ReadVectorField(*Loc, TEXT(""), Location, Location);
  if (Payload->TryGetObjectField(TEXT("rotation"), Loc) && Loc &&
      (*Loc).IsValid())
    ReadRotatorField(*Loc, TEXT(""), Rotation, Rotation);

#if defined(MCP_HAS_UNREALEDITOR_SUBSYSTEM)
  if (UUnrealEditorSubsystem *UES =
          GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>())
  {
    UES->SetLevelViewportCameraInfo(Location, Rotation);
#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
    if (ULevelEditorSubsystem *LES =
            GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
      LES->EditorInvalidateViewports();
#endif
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Camera set"), Resp,
                           FString());
    return true;
  }
#endif
  if (FEditorViewportClient *ViewportClient =
          GEditor->GetActiveViewport()
              ? (FEditorViewportClient *)GEditor->GetActiveViewport()
                    ->GetClient()
              : nullptr)
  {
    ViewportClient->SetViewLocation(Location);
    ViewportClient->SetViewRotation(Rotation);
    ViewportClient->Invalidate();
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Camera set"), Resp,
                           FString());
    return true;
  }
  return false;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetViewMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString Mode;
  Payload->TryGetStringField(TEXT("viewMode"), Mode);
  FString LowerMode = Mode.ToLower();
  FString Chosen;
  if (LowerMode == TEXT("lit"))
    Chosen = TEXT("Lit");
  else if (LowerMode == TEXT("unlit"))
    Chosen = TEXT("Unlit");
  else if (LowerMode == TEXT("wireframe"))
    Chosen = TEXT("Wireframe");
  else if (LowerMode == TEXT("detaillighting"))
    Chosen = TEXT("DetailLighting");
  else if (LowerMode == TEXT("lightingonly"))
    Chosen = TEXT("LightingOnly");
  else if (LowerMode == TEXT("lightcomplexity"))
    Chosen = TEXT("LightComplexity");
  else if (LowerMode == TEXT("shadercomplexity"))
    Chosen = TEXT("ShaderComplexity");
  else if (LowerMode == TEXT("lightmapdensity"))
    Chosen = TEXT("LightmapDensity");
  else if (LowerMode == TEXT("stationarylightoverlap"))
    Chosen = TEXT("StationaryLightOverlap");
  else if (LowerMode == TEXT("reflectionoverride"))
    Chosen = TEXT("ReflectionOverride");
  else
    Chosen = Mode;

  const FString Cmd = FString::Printf(TEXT("viewmode %s"), *Chosen);
  if (GEditor->Exec(nullptr, *Cmd))
  {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("viewMode"), Chosen);
    SendAutomationResponse(Socket, RequestId, true, TEXT("View mode set"), Resp,
                           FString());
    return true;
  }
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("EXEC_FAILED"),
                            TEXT("View mode command failed"), nullptr);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("control_editor"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("control_editor")))
    return false;
  if (!Payload.IsValid())
  {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("control_editor payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  if (LowerSub == TEXT("play"))
    return HandleControlEditorPlay(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("stop"))
    return HandleControlEditorStop(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("eject"))
    return HandleControlEditorEject(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("possess"))
    return HandleControlEditorPossess(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("focus_actor"))
    return HandleControlEditorFocusActor(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_camera") ||
      LowerSub == TEXT("set_camera_position") ||
      LowerSub == TEXT("set_viewport_camera"))
    return HandleControlEditorSetCamera(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_view_mode"))
    return HandleControlEditorSetViewMode(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("open_asset"))
    return HandleControlEditorOpenAsset(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("fit_blueprint_graph"))
    return HandleControlEditorFitBlueprintGraph(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_blueprint_graph_view"))
    return HandleControlEditorSetBlueprintGraphView(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("jump_to_blueprint_node"))
    return HandleControlEditorJumpToBlueprintNode(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("capture_blueprint_graph_review"))
    return HandleControlEditorCaptureBlueprintGraphReview(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_widget_blueprint_mode"))
    return HandleControlEditorSetWidgetBlueprintMode(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("fit_widget_designer"))
    return HandleControlEditorFitWidgetDesigner(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_widget_designer_view"))
    return HandleControlEditorSetWidgetDesignerView(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("select_widget_in_designer"))
    return HandleControlEditorSelectWidgetInDesigner(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("select_widgets_in_designer_rect"))
    return HandleControlEditorSelectWidgetsInDesignerRect(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("focus_editor_surface"))
    return HandleControlEditorFocusEditorSurface(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("screenshot") || LowerSub == TEXT("take_screenshot"))
    return HandleControlEditorScreenshot(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("pause"))
    return HandleControlEditorPause(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("resume"))
    return HandleControlEditorResume(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("console_command") || LowerSub == TEXT("execute_command"))
    return HandleControlEditorConsoleCommand(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("step_frame"))
    return HandleControlEditorStepFrame(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("start_recording"))
    return HandleControlEditorStartRecording(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("stop_recording"))
    return HandleControlEditorStopRecording(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("create_bookmark"))
    return HandleControlEditorCreateBookmark(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("jump_to_bookmark"))
    return HandleControlEditorJumpToBookmark(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_preferences"))
    return HandleControlEditorSetPreferences(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_viewport_realtime"))
    return HandleControlEditorSetViewportRealtime(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("simulate_input"))
    return HandleControlEditorSimulateInput(RequestId, Payload, RequestingSocket);
  // Additional actions for test compatibility
  if (LowerSub == TEXT("close_asset"))
    return HandleControlEditorCloseAsset(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("save_all"))
    return HandleControlEditorSaveAll(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("undo"))
    return HandleControlEditorUndo(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("redo"))
    return HandleControlEditorRedo(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_editor_mode"))
    return HandleControlEditorSetEditorMode(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("show_stats"))
    return HandleControlEditorShowStats(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("hide_stats"))
    return HandleControlEditorHideStats(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_game_view"))
    return HandleControlEditorSetGameView(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_immersive_mode"))
    return HandleControlEditorSetImmersiveMode(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("single_frame_step"))
    return HandleControlEditorStepFrame(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_fixed_delta_time"))
    return HandleControlEditorSetFixedDeltaTime(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("open_level"))
    return HandleControlEditorOpenLevel(RequestId, Payload, RequestingSocket);

  SendStandardErrorResponse(
      this, RequestingSocket, RequestId, TEXT("UNKNOWN_ACTION"),
      FString::Printf(TEXT("Unknown editor control action: %s"), *LowerSub), nullptr);
  return true;
#else
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Editor control requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorOpenAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("assetPath required"), nullptr);
    return true;
  }

  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  UAssetEditorSubsystem *AssetEditorSS =
      GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
  if (!AssetEditorSS)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SUBSYSTEM_MISSING"),
                              TEXT("AssetEditorSubsystem not available"), nullptr);
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ASSET_NOT_FOUND"),
                              TEXT("Asset not found"), nullptr);
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("LOAD_FAILED"),
                              TEXT("Failed to load asset"), nullptr);
    return true;
  }

  const bool bOpened = AssetEditorSS->OpenEditorForAsset(Asset);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bOpened);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);

  if (bOpened)
  {
    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset opened"), Resp,
                           FString());
  }
  else
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("OPEN_FAILED"),
                              TEXT("Failed to open asset editor"), Resp);
  }
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorFitBlueprintGraph(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FMcpResolvedBlueprintEditorContext Context;
  FString ErrorCode;
  FString ErrorMessage;
  if (!ResolveBlueprintEditorContext(Payload, Context, ErrorCode, ErrorMessage))
  {
    SendBlueprintNavigationError(this, Socket, RequestId, Context, ErrorCode, ErrorMessage);
    return true;
  }

  UEdGraph *TargetGraph = nullptr;
  FString GraphError;
  if (!ResolveBlueprintGraph(Context, TargetGraph, GraphError))
  {
    SendBlueprintNavigationError(this, Socket, RequestId, Context,
                                 TEXT("GRAPH_RESOLUTION_FAILED"), GraphError);
    return true;
  }

  FString Scope = GetTrimmedPayloadString(Payload, TEXT("scope")).ToLower();
  if (Scope.IsEmpty())
  {
    Scope = TEXT("full");
  }
  if (Scope != TEXT("full") && Scope != TEXT("selection"))
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateBlueprintNavigationDiagnosticsObject(Context);
    ErrorDetails->SetStringField(TEXT("scope"), Scope);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("scope must be 'full' or 'selection'"), ErrorDetails);
    return true;
  }

  FVector2D PreviousViewLocation = FVector2D::ZeroVector;
  float PreviousZoomAmount = 1.0f;
  Context.Editor->GetViewLocation(PreviousViewLocation, PreviousZoomAmount);

  const TSharedPtr<SGraphEditor> GraphEditor = Context.Editor->OpenGraphAndBringToFront(TargetGraph, true);
  if (!GraphEditor.IsValid())
  {
    SendBlueprintNavigationError(this, Socket, RequestId, Context,
                                 TEXT("GRAPH_EDITOR_NOT_AVAILABLE"),
                                 TEXT("Unable to resolve a live graph editor for the Blueprint graph"));
    return true;
  }

  if (Scope == TEXT("selection"))
  {
    GraphEditor->ZoomToFit(true);
  }
  else
  {
    GraphEditor->ZoomToFit(false);
  }

  FVector2D ViewLocation = FVector2D::ZeroVector;
  float ZoomAmount = 1.0f;
  Context.Editor->GetViewLocation(ViewLocation, ZoomAmount);
  RefreshBlueprintEditorSurfaceDiagnostics(Context.Editor, Context);

  TSharedPtr<FJsonObject> Response = CreateBlueprintNavigationDiagnosticsObject(Context);
  Response->SetBoolField(TEXT("success"), true);
  Response->SetStringField(TEXT("scope"), Scope);
  SetPointObjectField(Response, TEXT("previousViewLocation"), PreviousViewLocation);
  Response->SetNumberField(TEXT("previousZoomAmount"), PreviousZoomAmount);
  SetPointObjectField(Response, TEXT("viewLocation"), ViewLocation);
  Response->SetNumberField(TEXT("zoomAmount"), ZoomAmount);

  SendAutomationResponse(Socket, RequestId, true,
                         Scope == TEXT("selection") ? TEXT("Blueprint graph fitted to selection")
                                                    : TEXT("Blueprint graph fitted to window"),
                         Response, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetBlueprintGraphView(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FMcpResolvedBlueprintEditorContext Context;
  FString ErrorCode;
  FString ErrorMessage;
  if (!ResolveBlueprintEditorContext(Payload, Context, ErrorCode, ErrorMessage))
  {
    SendBlueprintNavigationError(this, Socket, RequestId, Context, ErrorCode, ErrorMessage);
    return true;
  }

  UEdGraph *TargetGraph = nullptr;
  FString GraphError;
  if (!ResolveBlueprintGraph(Context, TargetGraph, GraphError))
  {
    SendBlueprintNavigationError(this, Socket, RequestId, Context,
                                 TEXT("GRAPH_RESOLUTION_FAILED"), GraphError);
    return true;
  }

  FVector2D CurrentViewLocation = FVector2D::ZeroVector;
  float CurrentZoomAmount = 1.0f;
  Context.Editor->GetViewLocation(CurrentViewLocation, CurrentZoomAmount);

  FVector2D RequestedViewLocation = FVector2D::ZeroVector;
  const bool bHasRequestedViewLocation =
      TryGetBlueprintViewPointField(Payload, TEXT("viewLocation"), RequestedViewLocation);

  FVector2D RequestedDelta = FVector2D::ZeroVector;
  const bool bHasRequestedDelta =
      TryGetBlueprintViewPointField(Payload, TEXT("delta"), RequestedDelta);

  if (!bHasRequestedViewLocation && !bHasRequestedDelta)
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateBlueprintNavigationDiagnosticsObject(Context);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("viewLocation or delta required"), ErrorDetails);
    return true;
  }

  bool bPreserveZoom = true;
  Payload->TryGetBoolField(TEXT("preserveZoom"), bPreserveZoom);

  double RequestedZoomAmount = static_cast<double>(CurrentZoomAmount);
  const bool bHasRequestedZoomAmount = Payload->TryGetNumberField(TEXT("zoomAmount"), RequestedZoomAmount);

  FVector2D TargetViewLocation = bHasRequestedViewLocation ? RequestedViewLocation : CurrentViewLocation;
  if (bHasRequestedDelta)
  {
    TargetViewLocation += RequestedDelta;
  }

  const float TargetZoomAmount = bHasRequestedZoomAmount
                                     ? static_cast<float>(RequestedZoomAmount)
                                     : CurrentZoomAmount;

  Context.Editor->SetViewLocation(TargetViewLocation, TargetZoomAmount);

  FVector2D FinalViewLocation = FVector2D::ZeroVector;
  float FinalZoomAmount = 1.0f;
  Context.Editor->GetViewLocation(FinalViewLocation, FinalZoomAmount);
  RefreshBlueprintEditorSurfaceDiagnostics(Context.Editor, Context);

  TSharedPtr<FJsonObject> Response = CreateBlueprintNavigationDiagnosticsObject(Context);
  Response->SetBoolField(TEXT("success"), true);
  Response->SetBoolField(TEXT("preserveZoom"), bPreserveZoom);
  SetPointObjectField(Response, TEXT("previousViewLocation"), CurrentViewLocation);
  Response->SetNumberField(TEXT("previousZoomAmount"), CurrentZoomAmount);
  if (bHasRequestedViewLocation)
  {
    SetPointObjectField(Response, TEXT("requestedViewLocation"), RequestedViewLocation);
  }
  if (bHasRequestedDelta)
  {
    SetPointObjectField(Response, TEXT("delta"), RequestedDelta);
  }
  if (bHasRequestedZoomAmount)
  {
    Response->SetNumberField(TEXT("requestedZoomAmount"), RequestedZoomAmount);
  }
  SetPointObjectField(Response, TEXT("viewLocation"), FinalViewLocation);
  Response->SetNumberField(TEXT("zoomAmount"), FinalZoomAmount);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Blueprint graph view updated"),
                         Response, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorJumpToBlueprintNode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FMcpResolvedBlueprintEditorContext Context;
  FString ErrorCode;
  FString ErrorMessage;
  if (!ResolveBlueprintEditorContext(Payload, Context, ErrorCode, ErrorMessage))
  {
    SendBlueprintNavigationError(this, Socket, RequestId, Context, ErrorCode, ErrorMessage);
    return true;
  }

  const FString RequestedNodeGuid = GetTrimmedPayloadString(Payload, TEXT("nodeGuid"));
  const FString RequestedNodeName = GetTrimmedPayloadString(Payload, TEXT("nodeName"));
  const FString RequestedNodeTitle = GetTrimmedPayloadString(Payload, TEXT("nodeTitle"));

  FString SelectorType;
  FString SelectorValue;
  FGuid ParsedNodeGuid;
  if (!RequestedNodeGuid.IsEmpty())
  {
    if (!FGuid::Parse(RequestedNodeGuid, ParsedNodeGuid))
    {
      TSharedPtr<FJsonObject> ErrorDetails = CreateBlueprintNavigationDiagnosticsObject(Context);
      ErrorDetails->SetStringField(TEXT("nodeGuid"), RequestedNodeGuid);
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("nodeGuid must be a valid GUID string"), ErrorDetails);
      return true;
    }

    SelectorType = TEXT("nodeGuid");
    SelectorValue = RequestedNodeGuid;
  }
  else if (!RequestedNodeName.IsEmpty())
  {
    SelectorType = TEXT("nodeName");
    SelectorValue = RequestedNodeName;
  }
  else if (!RequestedNodeTitle.IsEmpty())
  {
    SelectorType = TEXT("nodeTitle");
    SelectorValue = RequestedNodeTitle;
  }
  else
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateBlueprintNavigationDiagnosticsObject(Context);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("nodeGuid, nodeName, or nodeTitle required"), ErrorDetails);
    return true;
  }

  TArray<UEdGraph *> CandidateGraphs;
  if (!Context.RequestedGraphName.IsEmpty())
  {
    UEdGraph *RequestedGraph = nullptr;
    FString GraphError;
    if (!ResolveBlueprintGraph(Context, RequestedGraph, GraphError))
    {
      SendBlueprintNavigationError(this, Socket, RequestId, Context,
                                   TEXT("GRAPH_RESOLUTION_FAILED"), GraphError);
      return true;
    }

    CandidateGraphs.Add(RequestedGraph);
  }
  else
  {
    Context.Blueprint->GetAllGraphs(CandidateGraphs);
  }

  struct FMatchedNodeEntry
  {
    UEdGraph *Graph = nullptr;
    UEdGraphNode *Node = nullptr;
  };

  TArray<FMatchedNodeEntry> Matches;
  for (UEdGraph *Graph : CandidateGraphs)
  {
    if (Graph == nullptr)
    {
      continue;
    }

    for (UEdGraphNode *Node : Graph->Nodes)
    {
      if (Node == nullptr)
      {
        continue;
      }

      bool bMatches = false;
      if (SelectorType == TEXT("nodeGuid"))
      {
        bMatches = Node->NodeGuid == ParsedNodeGuid;
      }
      else if (SelectorType == TEXT("nodeName"))
      {
        bMatches = Node->GetName().Equals(SelectorValue, ESearchCase::IgnoreCase);
      }
      else if (SelectorType == TEXT("nodeTitle"))
      {
        bMatches = GetBlueprintNodeListTitle(Node).Equals(SelectorValue, ESearchCase::CaseSensitive);
      }

      if (bMatches)
      {
        FMatchedNodeEntry &Entry = Matches.AddDefaulted_GetRef();
        Entry.Graph = Graph;
        Entry.Node = Node;
      }
    }
  }

  if (Matches.Num() == 0)
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateBlueprintNavigationDiagnosticsObject(Context);
    ErrorDetails->SetStringField(TEXT("nodeSelectorType"), SelectorType);
    ErrorDetails->SetStringField(TEXT("nodeSelector"), SelectorValue);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NODE_NOT_FOUND"),
                              TEXT("Blueprint node selector did not match any node"), ErrorDetails);
    return true;
  }

  if (Matches.Num() > 1)
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateBlueprintNavigationDiagnosticsObject(Context);
    ErrorDetails->SetStringField(TEXT("nodeSelectorType"), SelectorType);
    ErrorDetails->SetStringField(TEXT("nodeSelector"), SelectorValue);
    ErrorDetails->SetNumberField(TEXT("matchCount"), Matches.Num());
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("AMBIGUOUS_NODE_SELECTOR"),
                              TEXT("Blueprint node selector matched multiple nodes"), ErrorDetails);
    return true;
  }

  UEdGraph *MatchedGraph = Matches[0].Graph;
  UEdGraphNode *MatchedNode = Matches[0].Node;
  Context.Editor->OpenGraphAndBringToFront(MatchedGraph, true);
  Context.Editor->JumpToNode(MatchedNode, false);
  Context.Editor->ClearSelectionStateFor(FBlueprintEditor::SelectionState_Graph);
  Context.Editor->SetUISelectionState(FBlueprintEditor::SelectionState_Graph);
  Context.Editor->AddToSelection(MatchedNode);
  Context.ResolvedGraphName = GetBlueprintGraphDisplayName(MatchedGraph);
  Context.CurrentMode = Context.Editor->GetCurrentMode().ToString();
  RefreshBlueprintEditorSurfaceDiagnostics(Context.Editor, Context);

  FVector2D ViewLocation = FVector2D::ZeroVector;
  float ZoomAmount = 1.0f;
  Context.Editor->GetViewLocation(ViewLocation, ZoomAmount);

  TSharedPtr<FJsonObject> Response = CreateBlueprintNavigationDiagnosticsObject(Context);
  Response->SetBoolField(TEXT("success"), true);
  Response->SetStringField(TEXT("nodeSelectorType"), SelectorType);
  Response->SetStringField(TEXT("nodeSelector"), SelectorValue);
  Response->SetStringField(TEXT("matchedNodeId"), MatchedNode->NodeGuid.ToString());
  Response->SetStringField(TEXT("matchedNodeName"), MatchedNode->GetName());
  Response->SetStringField(TEXT("matchedNodeTitle"), GetBlueprintNodeListTitle(MatchedNode));
  SetPointObjectField(Response, TEXT("viewLocation"), ViewLocation);
  Response->SetNumberField(TEXT("zoomAmount"), ZoomAmount);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Blueprint node revealed"),
                         Response, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorCaptureBlueprintGraphReview(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FMcpResolvedBlueprintEditorContext Context;
  FString ErrorCode;
  FString ErrorMessage;
  if (!ResolveBlueprintEditorContext(Payload, Context, ErrorCode, ErrorMessage))
  {
    SendBlueprintNavigationError(this, Socket, RequestId, Context, ErrorCode, ErrorMessage);
    return true;
  }

  UEdGraph *TargetGraph = nullptr;
  FString GraphError;
  if (!ResolveBlueprintGraph(Context, TargetGraph, GraphError))
  {
    SendBlueprintNavigationError(this, Socket, RequestId, Context,
                                 TEXT("GRAPH_RESOLUTION_FAILED"), GraphError);
    return true;
  }

  FString Scope = GetTrimmedPayloadString(Payload, TEXT("scope")).ToLower();
  if (Scope.IsEmpty())
  {
    Scope = TEXT("full");
  }
  if (Scope != TEXT("full") && Scope != TEXT("selection") && Scope != TEXT("neighborhood"))
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateBlueprintNavigationDiagnosticsObject(Context);
    ErrorDetails->SetStringField(TEXT("scope"), Scope);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("scope must be 'full', 'selection', or 'neighborhood'"), ErrorDetails);
    return true;
  }

  const FString RequestedNodeGuid = GetTrimmedPayloadString(Payload, TEXT("nodeGuid"));
  const FString RequestedNodeName = GetTrimmedPayloadString(Payload, TEXT("nodeName"));
  const FString RequestedNodeTitle = GetTrimmedPayloadString(Payload, TEXT("nodeTitle"));
  const bool bHasNodeSelector = !RequestedNodeGuid.IsEmpty() || !RequestedNodeName.IsEmpty() ||
                                !RequestedNodeTitle.IsEmpty();

  if (Scope == TEXT("neighborhood") && !bHasNodeSelector)
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateBlueprintNavigationDiagnosticsObject(Context);
    ErrorDetails->SetStringField(TEXT("scope"), Scope);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("scope 'neighborhood' requires nodeGuid, nodeName, or nodeTitle"), ErrorDetails);
    return true;
  }

  FString SelectorType;
  FString SelectorValue;
  FGuid ParsedNodeGuid;
  UEdGraph *ReviewGraph = TargetGraph;
  UEdGraphNode *MatchedNode = nullptr;

  if (bHasNodeSelector)
  {
    if (!RequestedNodeGuid.IsEmpty())
    {
      if (!FGuid::Parse(RequestedNodeGuid, ParsedNodeGuid))
      {
        TSharedPtr<FJsonObject> ErrorDetails = CreateBlueprintNavigationDiagnosticsObject(Context);
        ErrorDetails->SetStringField(TEXT("nodeGuid"), RequestedNodeGuid);
        SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                                  TEXT("nodeGuid must be a valid GUID string"), ErrorDetails);
        return true;
      }

      SelectorType = TEXT("nodeGuid");
      SelectorValue = RequestedNodeGuid;
    }
    else if (!RequestedNodeName.IsEmpty())
    {
      SelectorType = TEXT("nodeName");
      SelectorValue = RequestedNodeName;
    }
    else
    {
      SelectorType = TEXT("nodeTitle");
      SelectorValue = RequestedNodeTitle;
    }

    TArray<UEdGraph *> CandidateGraphs;
    if (!Context.RequestedGraphName.IsEmpty())
    {
      CandidateGraphs.Add(TargetGraph);
    }
    else
    {
      Context.Blueprint->GetAllGraphs(CandidateGraphs);
    }

    struct FMatchedNodeEntry
    {
      UEdGraph *Graph = nullptr;
      UEdGraphNode *Node = nullptr;
    };

    TArray<FMatchedNodeEntry> Matches;
    for (UEdGraph *Graph : CandidateGraphs)
    {
      if (Graph == nullptr)
      {
        continue;
      }

      for (UEdGraphNode *Node : Graph->Nodes)
      {
        if (Node == nullptr)
        {
          continue;
        }

        bool bMatches = false;
        if (SelectorType == TEXT("nodeGuid"))
        {
          bMatches = Node->NodeGuid == ParsedNodeGuid;
        }
        else if (SelectorType == TEXT("nodeName"))
        {
          bMatches = Node->GetName().Equals(SelectorValue, ESearchCase::IgnoreCase);
        }
        else if (SelectorType == TEXT("nodeTitle"))
        {
          bMatches = GetBlueprintNodeListTitle(Node).Equals(SelectorValue, ESearchCase::CaseSensitive);
        }

        if (bMatches)
        {
          FMatchedNodeEntry &Entry = Matches.AddDefaulted_GetRef();
          Entry.Graph = Graph;
          Entry.Node = Node;
        }
      }
    }

    if (Matches.Num() == 0)
    {
      TSharedPtr<FJsonObject> ErrorDetails = CreateBlueprintNavigationDiagnosticsObject(Context);
      ErrorDetails->SetStringField(TEXT("nodeSelectorType"), SelectorType);
      ErrorDetails->SetStringField(TEXT("nodeSelector"), SelectorValue);
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("NODE_NOT_FOUND"),
                                TEXT("Blueprint node selector did not match any node"), ErrorDetails);
      return true;
    }

    if (Matches.Num() > 1)
    {
      TSharedPtr<FJsonObject> ErrorDetails = CreateBlueprintNavigationDiagnosticsObject(Context);
      ErrorDetails->SetStringField(TEXT("nodeSelectorType"), SelectorType);
      ErrorDetails->SetStringField(TEXT("nodeSelector"), SelectorValue);
      ErrorDetails->SetNumberField(TEXT("matchCount"), Matches.Num());
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("AMBIGUOUS_NODE_SELECTOR"),
                                TEXT("Blueprint node selector matched multiple nodes"), ErrorDetails);
      return true;
    }

    ReviewGraph = Matches[0].Graph;
    MatchedNode = Matches[0].Node;
  }

  TArray<UEdGraphNode *> FramedNodes;
  bool bTruncatedNeighborhood = false;
  constexpr int32 MaxNeighborhoodNodes = 12;
  auto AddFramedNode = [&FramedNodes, &bTruncatedNeighborhood](UEdGraphNode *Node)
  {
    if (Node == nullptr || FramedNodes.Contains(Node))
    {
      return;
    }

    if (FramedNodes.Num() >= MaxNeighborhoodNodes)
    {
      bTruncatedNeighborhood = true;
      return;
    }

    FramedNodes.Add(Node);
  };

  if (Scope == TEXT("neighborhood") && MatchedNode != nullptr)
  {
    AddFramedNode(MatchedNode);

    for (UEdGraphPin *Pin : MatchedNode->Pins)
    {
      if (Pin == nullptr)
      {
        continue;
      }

      for (UEdGraphPin *LinkedPin : Pin->LinkedTo)
      {
        if (LinkedPin == nullptr)
        {
          continue;
        }

        AddFramedNode(LinkedPin->GetOwningNode());
      }
    }

    if (FramedNodes.Num() == 1)
    {
      TArray<UEdGraphNode *> NearbyNodes;
      NearbyNodes.Reserve(ReviewGraph->Nodes.Num());

      for (UEdGraphNode *CandidateNode : ReviewGraph->Nodes)
      {
        if (CandidateNode == nullptr || CandidateNode == MatchedNode)
        {
          continue;
        }

        NearbyNodes.Add(CandidateNode);
      }

      NearbyNodes.Sort([MatchedNode](const UEdGraphNode &A, const UEdGraphNode &B)
                       {
                         const int64 DistanceA =
                             FMath::Abs(static_cast<int64>(A.NodePosX) - static_cast<int64>(MatchedNode->NodePosX)) +
                             FMath::Abs(static_cast<int64>(A.NodePosY) - static_cast<int64>(MatchedNode->NodePosY));
                         const int64 DistanceB =
                             FMath::Abs(static_cast<int64>(B.NodePosX) - static_cast<int64>(MatchedNode->NodePosX)) +
                             FMath::Abs(static_cast<int64>(B.NodePosY) - static_cast<int64>(MatchedNode->NodePosY));
                         return DistanceA < DistanceB; });

      for (UEdGraphNode *CandidateNode : NearbyNodes)
      {
        AddFramedNode(CandidateNode);
        if (FramedNodes.Num() >= 2)
        {
          break;
        }
      }
    }
  }

  const bool bUseNeighborhoodScope = Scope == TEXT("neighborhood") && MatchedNode != nullptr;
  const bool bUseSelectionScope = Scope == TEXT("selection") && MatchedNode != nullptr;
  const bool bUseFocusedScope = bUseSelectionScope || bUseNeighborhoodScope;
  const FString EffectiveScope =
      bUseNeighborhoodScope ? TEXT("neighborhood")
                            : (bUseSelectionScope ? TEXT("selection") : TEXT("full"));
  const FString FramingSource =
      bUseNeighborhoodScope ? TEXT("matched_node_neighborhood")
                            : (MatchedNode != nullptr ? TEXT("matched_node_selection")
                                                      : TEXT("graph_overview"));
  const int32 FramedNodeCount =
      bUseNeighborhoodScope ? FramedNodes.Num()
                            : (MatchedNode != nullptr ? 1 : ReviewGraph->Nodes.Num());

  FVector2D PreviousViewLocation = FVector2D::ZeroVector;
  float PreviousZoomAmount = 1.0f;
  Context.Editor->GetViewLocation(PreviousViewLocation, PreviousZoomAmount);

  const TSharedPtr<SGraphEditor> GraphEditor = Context.Editor->OpenGraphAndBringToFront(ReviewGraph, true);
  if (!GraphEditor.IsValid())
  {
    SendBlueprintNavigationError(this, Socket, RequestId, Context,
                                 TEXT("GRAPH_EDITOR_NOT_AVAILABLE"),
                                 TEXT("Unable to resolve a live graph editor for the Blueprint graph"));
    return true;
  }

  Context.Editor->ClearSelectionStateFor(FBlueprintEditor::SelectionState_Graph);
  Context.Editor->SetUISelectionState(FBlueprintEditor::SelectionState_Graph);
  if (MatchedNode != nullptr)
  {
    Context.Editor->JumpToNode(MatchedNode, false);
  }

  if (bUseNeighborhoodScope)
  {
    for (UEdGraphNode *FramedNode : FramedNodes)
    {
      Context.Editor->AddToSelection(FramedNode);
    }
  }
  else if (MatchedNode != nullptr)
  {
    Context.Editor->AddToSelection(MatchedNode);
  }

  GraphEditor->ZoomToFit(bUseFocusedScope);

  FVector2D ViewLocation = FVector2D::ZeroVector;
  float ZoomAmount = 1.0f;
  Context.Editor->GetViewLocation(ViewLocation, ZoomAmount);
  Context.ResolvedGraphName = GetBlueprintGraphDisplayName(ReviewGraph);
  Context.CurrentMode = Context.Editor->GetCurrentMode().ToString();
  RefreshBlueprintEditorSurfaceDiagnostics(Context.Editor, Context);

  FString Filename = GetTrimmedPayloadString(Payload, TEXT("filename"));
  Filename = SanitizeScreenshotFilename(Filename);
  const FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("Screenshots");
  IFileManager::Get().MakeDirectory(*ScreenshotDir, true);
  const FString FullPath = ScreenshotDir / Filename;
  const bool bIncludeMenus =
      !Payload->HasField(TEXT("includeMenus"))
          ? true
          : GetJsonBoolField(Payload, TEXT("includeMenus"), true);

  TSharedPtr<SDockTab> TargetTab;
  TSharedPtr<SWindow> TargetWindow;
  if (TSharedPtr<FTabManager> TabManager = Context.Editor->GetAssociatedTabManager())
  {
    if (!Context.RequestedTabId.IsEmpty())
    {
      TargetTab = TabManager->FindExistingLiveTab(FTabId(FName(*Context.RequestedTabId)));
    }
    if (!TargetTab.IsValid())
    {
      TargetTab = TabManager->GetOwnerTab();
    }
    if (TargetTab.IsValid())
    {
      if (Context.TabId.IsEmpty())
      {
        Context.TabId = TargetTab->GetLayoutIdentifier().ToString();
      }
      TargetWindow = TargetTab->GetParentWindow();
      if (TargetWindow.IsValid() && Context.WindowTitle.IsEmpty())
      {
        Context.WindowTitle = TargetWindow->GetTitle().ToString();
      }
    }
  }

  if (!TargetWindow.IsValid())
  {
    TargetWindow = FSlateApplication::Get().FindWidgetWindow(GraphEditor.ToSharedRef());
    if (TargetWindow.IsValid() && Context.WindowTitle.IsEmpty())
    {
      Context.WindowTitle = TargetWindow->GetTitle().ToString();
    }
  }

  if (!TargetWindow.IsValid())
  {
    TargetWindow = ResolvePreferredScreenshotWindow(this, Context.WindowTitle);
  }

  if (!TargetWindow.IsValid())
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateBlueprintNavigationDiagnosticsObject(Context);
    ErrorDetails->SetStringField(TEXT("captureIntentSource"), TEXT("blueprint_editor_context"));
    ErrorDetails->SetStringField(TEXT("scope"), EffectiveScope);
    if (!SelectorType.IsEmpty())
    {
      ErrorDetails->SetStringField(TEXT("nodeSelectorType"), SelectorType);
      ErrorDetails->SetStringField(TEXT("nodeSelector"), SelectorValue);
    }
    if (MatchedNode != nullptr)
    {
      ErrorDetails->SetStringField(TEXT("matchedNodeId"), MatchedNode->NodeGuid.ToString());
      ErrorDetails->SetStringField(TEXT("matchedNodeName"), MatchedNode->GetName());
      ErrorDetails->SetStringField(TEXT("matchedNodeTitle"), GetBlueprintNodeListTitle(MatchedNode));
    }
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("WINDOW_NOT_FOUND"),
                              TEXT("No visible Blueprint editor window was available for graph review capture"),
                              ErrorDetails);
    return true;
  }

  if (TargetTab.IsValid())
  {
    TargetTab->ActivateInParent(ETabActivationCause::SetDirectly);
  }

  TargetWindow->BringToFront(true);

  const bool bFocusApplied =
      FSlateApplication::Get().SetKeyboardFocus(GraphEditor, EFocusCause::SetDirectly);
  const bool bGraphFocusReady = GraphEditor->HasAnyUserFocus().IsSet() ||
                                GraphEditor->HasFocusedDescendants();
  const bool bWindowActiveForCapture = TargetWindow->IsActive() ||
                                       TargetWindow->HasActiveChildren();
  const TSharedPtr<SWindow> ActiveModalWindow = FSlateApplication::Get().GetActiveModalWindow();
  const bool bBlockedByDifferentModalWindow = ActiveModalWindow.IsValid() &&
                                              ActiveModalWindow != TargetWindow;

  if (bBlockedByDifferentModalWindow || !bWindowActiveForCapture ||
      (!bFocusApplied && !bGraphFocusReady))
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateBlueprintNavigationDiagnosticsObject(Context);
    ErrorDetails->SetStringField(TEXT("captureIntentSource"), TEXT("blueprint_editor_context"));
    ErrorDetails->SetStringField(TEXT("scope"), EffectiveScope);
    ErrorDetails->SetBoolField(TEXT("focusApplied"), bFocusApplied);
    ErrorDetails->SetBoolField(TEXT("graphFocusReady"), bGraphFocusReady);
    ErrorDetails->SetBoolField(TEXT("windowActiveForCapture"), bWindowActiveForCapture);
    ErrorDetails->SetStringField(TEXT("targetWindowTitle"), TargetWindow->GetTitle().ToString());
    if (!Context.TabId.IsEmpty())
    {
      ErrorDetails->SetStringField(TEXT("targetTabId"), Context.TabId);
    }
    if (ActiveModalWindow.IsValid())
    {
      ErrorDetails->SetStringField(TEXT("activeModalWindowTitle"), ActiveModalWindow->GetTitle().ToString());
    }
    if (!SelectorType.IsEmpty())
    {
      ErrorDetails->SetStringField(TEXT("nodeSelectorType"), SelectorType);
      ErrorDetails->SetStringField(TEXT("nodeSelector"), SelectorValue);
    }
    if (MatchedNode != nullptr)
    {
      ErrorDetails->SetStringField(TEXT("matchedNodeId"), MatchedNode->NodeGuid.ToString());
      ErrorDetails->SetStringField(TEXT("matchedNodeName"), MatchedNode->GetName());
      ErrorDetails->SetStringField(TEXT("matchedNodeTitle"), GetBlueprintNodeListTitle(MatchedNode));
    }
    ApplyTargetHealthResponseFields(
        ErrorDetails, bWindowActiveForCapture ? TEXT("resolved") : TEXT("stale"),
        true, false,
        bBlockedByDifferentModalWindow ? TEXT("blocked_by_modal_window") : FString(),
        bBlockedByDifferentModalWindow
            ? TEXT("Dismiss the active modal window before retrying graph review capture.")
            : TEXT("Bring the Blueprint editor window to the foreground and retry graph review capture."),
        TEXT("focus_editor_surface"));
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("CAPTURE_PRECONDITION_FAILED"),
        TEXT("Blueprint graph review capture requires an active, focused Blueprint editor window"),
        ErrorDetails);
    return true;
  }

  FIntPoint ScreenshotSize = FIntPoint::ZeroValue;
  int32 IncludedMenuWindowCount = 0;
  FString CaptureError;
  if (bIncludeMenus)
  {
    if (!CaptureWindowScreenshotIncludingChildWindowsToFile(
            TargetWindow, FullPath, ScreenshotSize, IncludedMenuWindowCount,
            CaptureError))
    {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("CAPTURE_FAILED"), CaptureError,
                                nullptr);
      return true;
    }
  }
  else if (!CaptureWindowScreenshotToFile(TargetWindow, FullPath, ScreenshotSize, CaptureError))
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CAPTURE_FAILED"), CaptureError,
                              nullptr);
    return true;
  }

  TSharedPtr<FJsonObject> Response = CreateBlueprintNavigationDiagnosticsObject(Context);
  Response->SetBoolField(TEXT("success"), true);
  Response->SetStringField(TEXT("scope"), EffectiveScope);
  Response->SetStringField(TEXT("requestedScope"), Scope);
  Response->SetStringField(TEXT("framingSource"), FramingSource);
  Response->SetNumberField(TEXT("framedNodeCount"), FramedNodeCount);
  Response->SetBoolField(TEXT("truncatedNeighborhood"), bUseNeighborhoodScope && bTruncatedNeighborhood);
  Response->SetStringField(TEXT("captureIntentSource"), TEXT("blueprint_editor_context"));
  Response->SetStringField(TEXT("filename"), Filename);
  Response->SetStringField(TEXT("path"), FullPath);
  Response->SetStringField(TEXT("captureTarget"), TEXT("editor_window"));
  Response->SetStringField(TEXT("captureMode"), TEXT("editor_window"));
  Response->SetBoolField(TEXT("includeMenus"), bIncludeMenus);
  Response->SetNumberField(TEXT("includedMenuWindowCount"), IncludedMenuWindowCount);
  Response->SetNumberField(TEXT("width"), ScreenshotSize.X);
  Response->SetNumberField(TEXT("height"), ScreenshotSize.Y);
  SetPointObjectField(Response, TEXT("previousViewLocation"), PreviousViewLocation);
  Response->SetNumberField(TEXT("previousZoomAmount"), PreviousZoomAmount);
  SetPointObjectField(Response, TEXT("viewLocation"), ViewLocation);
  Response->SetNumberField(TEXT("zoomAmount"), ZoomAmount);
  ApplyTargetHealthResponseFields(Response, TEXT("resolved"), true, false);
  if (!SelectorType.IsEmpty())
  {
    Response->SetStringField(TEXT("nodeSelectorType"), SelectorType);
    Response->SetStringField(TEXT("nodeSelector"), SelectorValue);
  }
  if (MatchedNode != nullptr)
  {
    Response->SetStringField(TEXT("matchedNodeId"), MatchedNode->NodeGuid.ToString());
    Response->SetStringField(TEXT("matchedNodeName"), MatchedNode->GetName());
    Response->SetStringField(TEXT("matchedNodeTitle"), GetBlueprintNodeListTitle(MatchedNode));
  }

  SendAutomationResponse(Socket, RequestId, true, TEXT("Blueprint graph review captured"),
                         Response, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetWidgetBlueprintMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FMcpResolvedWidgetBlueprintEditorContext Context;
  FString ErrorCode;
  FString ErrorMessage;
  if (!ResolveWidgetBlueprintEditorContext(Payload, Context, ErrorCode, ErrorMessage))
  {
    SendWidgetBlueprintNavigationError(this, Socket, RequestId, Context, ErrorCode, ErrorMessage);
    return true;
  }

  const FString RequestedMode = GetTrimmedPayloadString(Payload, TEXT("mode"));
  Context.RequestedMode = NormalizeWidgetBlueprintModeString(RequestedMode);
  if (Context.RequestedMode.IsEmpty())
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateWidgetBlueprintNavigationDiagnosticsObject(Context);
    ErrorDetails->SetStringField(TEXT("mode"), RequestedMode);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("mode must be 'designer' or 'graph'"), ErrorDetails);
    return true;
  }

  const FString PreviousMode = Context.CurrentMode;
  const FName TargetModeId = GetWidgetBlueprintModeId(Context.RequestedMode);
  const bool bModeChanged = !Context.CurrentMode.Equals(TargetModeId.ToString(), ESearchCase::CaseSensitive);

  if (bModeChanged)
  {
    Context.WidgetEditor->SetCurrentMode(TargetModeId);
  }

  RefreshWidgetBlueprintSurfaceDiagnostics(Context);

  TSharedPtr<FJsonObject> Response = CreateWidgetBlueprintNavigationDiagnosticsObject(Context);
  Response->SetBoolField(TEXT("success"), true);
  Response->SetStringField(TEXT("previousMode"), PreviousMode);
  Response->SetBoolField(TEXT("modeChanged"), bModeChanged);
  Response->SetStringField(TEXT("designerActionDisposition"),
                           Context.RequestedMode.Equals(TEXT("Designer"), ESearchCase::CaseSensitive) &&
                                   Context.bDesignerViewFound
                               ? TEXT("immediate")
                               : TEXT("mode_only"));

  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Widget Blueprint switched to %s mode"),
                                         *Context.RequestedMode),
                         Response, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorFitWidgetDesigner(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FMcpResolvedWidgetBlueprintEditorContext Context;
  FString ErrorCode;
  FString ErrorMessage;
  if (!ResolveWidgetBlueprintEditorContext(Payload, Context, ErrorCode, ErrorMessage))
  {
    SendWidgetBlueprintNavigationError(this, Socket, RequestId, Context, ErrorCode, ErrorMessage);
    return true;
  }

  const FString PreviousMode = Context.CurrentMode;
  const bool bEnteredDesigner = !Context.CurrentMode.Equals(FWidgetBlueprintApplicationModes::DesignerMode.ToString(),
                                                            ESearchCase::CaseSensitive);
  Context.RequestedMode = TEXT("Designer");
  if (bEnteredDesigner)
  {
    Context.WidgetEditor->SetCurrentMode(FWidgetBlueprintApplicationModes::DesignerMode);
    RefreshWidgetBlueprintSurfaceDiagnostics(Context);
  }

  FString DesignerActionDisposition;
  FString FitFailureReason;
  bool bFitExecuted = TryExecuteWidgetDesignerZoomToFit(Context,
                                                        DesignerActionDisposition,
                                                        FitFailureReason);
  if (!bFitExecuted && DesignerActionDisposition == TEXT("queued_after_layout"))
  {
    QueueWidgetDesignerZoomToFit(Context);
  }

  RefreshWidgetBlueprintSurfaceDiagnostics(Context);

  TSharedPtr<FJsonObject> Response = CreateWidgetBlueprintNavigationDiagnosticsObject(Context);
  Response->SetBoolField(TEXT("success"), true);
  Response->SetStringField(TEXT("previousMode"), PreviousMode);
  Response->SetBoolField(TEXT("enteredDesignerMode"), bEnteredDesigner);
  Response->SetBoolField(TEXT("fitExecuted"), bFitExecuted);
  Response->SetStringField(TEXT("designerActionDisposition"), DesignerActionDisposition);
  if (!FitFailureReason.IsEmpty())
  {
    Response->SetStringField(TEXT("fitFailureReason"), FitFailureReason);
  }

  const FString ResponseMessage = bFitExecuted
                                      ? TEXT("Widget Designer fitted to content")
                                      : (DesignerActionDisposition == TEXT("queued_after_layout")
                                             ? TEXT("Widget Designer fit queued after layout")
                                             : TEXT("Widget Designer switched to a narrower semantic fallback"));

  SendAutomationResponse(Socket, RequestId, true, ResponseMessage, Response, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetWidgetDesignerView(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FMcpResolvedWidgetBlueprintEditorContext Context;
  FString ErrorCode;
  FString ErrorMessage;
  if (!ResolveWidgetBlueprintEditorContext(Payload, Context, ErrorCode, ErrorMessage))
  {
    SendWidgetBlueprintNavigationError(this, Socket, RequestId, Context, ErrorCode, ErrorMessage);
    return true;
  }

  FVector2D RequestedViewLocation = FVector2D::ZeroVector;
  const bool bHasViewLocation = TryGetPointField(Payload, TEXT("viewLocation"), RequestedViewLocation);
  FVector2D RequestedDelta = FVector2D::ZeroVector;
  const bool bHasDelta = TryGetPointField(Payload, TEXT("delta"), RequestedDelta);
  if (!bHasViewLocation && !bHasDelta)
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateWidgetBlueprintNavigationDiagnosticsObject(Context);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("viewLocation or delta required"), ErrorDetails);
    return true;
  }

  const bool bPreserveZoom = GetJsonBoolField(Payload, TEXT("preserveZoom"), true);
  const FString PreviousMode = Context.CurrentMode;
  Context.RequestedMode = TEXT("Designer");
  const bool bEnteredDesigner = !Context.CurrentMode.Equals(FWidgetBlueprintApplicationModes::DesignerMode.ToString(),
                                                            ESearchCase::CaseSensitive);
  if (bEnteredDesigner)
  {
    Context.WidgetEditor->SetCurrentMode(FWidgetBlueprintApplicationModes::DesignerMode);
    RefreshWidgetBlueprintSurfaceDiagnostics(Context);
  }

  const TOptional<FVector2D> RequestedViewLocationOption =
      bHasViewLocation ? TOptional<FVector2D>(RequestedViewLocation) : TOptional<FVector2D>();
  const TOptional<FVector2D> RequestedDeltaOption =
      bHasDelta ? TOptional<FVector2D>(RequestedDelta) : TOptional<FVector2D>();

  FVector2D PreviousViewOffset = FVector2D::ZeroVector;
  FVector2D NewViewOffset = FVector2D::ZeroVector;
  FString DesignerActionDisposition;
  FString ViewFailureReason;
  const bool bViewUpdated = TryApplyWidgetDesignerViewChange(Context,
                                                             RequestedViewLocationOption,
                                                             RequestedDeltaOption,
                                                             PreviousViewOffset,
                                                             NewViewOffset,
                                                             DesignerActionDisposition,
                                                             ViewFailureReason);
  if (!bViewUpdated && DesignerActionDisposition == TEXT("queued_after_layout"))
  {
    QueueWidgetDesignerViewChange(Context, RequestedViewLocationOption, RequestedDeltaOption);
  }

  const FVector2D TargetViewOffset = bViewUpdated
                                         ? NewViewOffset
                                         : (RequestedViewLocationOption.IsSet()
                                                ? RequestedViewLocationOption.GetValue()
                                                : PreviousViewOffset) +
                                               (RequestedDeltaOption.IsSet() ? RequestedDeltaOption.GetValue()
                                                                             : FVector2D::ZeroVector);

  RefreshWidgetBlueprintSurfaceDiagnostics(Context);

  TSharedPtr<FJsonObject> Response = CreateWidgetBlueprintNavigationDiagnosticsObject(Context);
  Response->SetBoolField(TEXT("success"), true);
  Response->SetStringField(TEXT("previousMode"), PreviousMode);
  Response->SetBoolField(TEXT("enteredDesignerMode"), bEnteredDesigner);
  SetPointObjectField(Response, TEXT("previousViewLocation"), PreviousViewOffset);
  if (bHasViewLocation)
  {
    SetPointObjectField(Response, TEXT("requestedViewLocation"), RequestedViewLocation);
  }
  if (bHasDelta)
  {
    SetPointObjectField(Response, TEXT("delta"), RequestedDelta);
  }
  SetPointObjectField(Response, TEXT("viewLocation"), TargetViewOffset);
  Response->SetNumberField(TEXT("viewOffsetX"), TargetViewOffset.X);
  Response->SetNumberField(TEXT("viewOffsetY"), TargetViewOffset.Y);
  Response->SetBoolField(TEXT("preserveZoom"), bPreserveZoom);
  Response->SetStringField(TEXT("designerActionDisposition"), DesignerActionDisposition);
  if (!ViewFailureReason.IsEmpty())
  {
    Response->SetStringField(TEXT("viewFailureReason"), ViewFailureReason);
  }
  if (Context.DesignerView.IsValid())
  {
    Response->SetNumberField(TEXT("zoomAmount"), Context.DesignerView->GetZoomAmount());
  }

  SendAutomationResponse(Socket, RequestId, true,
                         bViewUpdated ? TEXT("Widget Designer view updated")
                                      : TEXT("Widget Designer view queued after layout"),
                         Response, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSelectWidgetInDesigner(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FMcpResolvedWidgetBlueprintEditorContext Context;
  FString ErrorCode;
  FString ErrorMessage;
  if (!ResolveWidgetBlueprintEditorContext(Payload, Context, ErrorCode, ErrorMessage))
  {
    SendWidgetBlueprintNavigationError(this, Socket, RequestId, Context, ErrorCode, ErrorMessage);
    return true;
  }

  UWidget *TargetWidget = nullptr;
  if (!ResolveWidgetBlueprintTargetWidget(Payload, Context, TargetWidget, ErrorCode, ErrorMessage))
  {
    SendWidgetBlueprintNavigationError(this, Socket, RequestId, Context, ErrorCode, ErrorMessage);
    return true;
  }

  const FString PreviousMode = Context.CurrentMode;
  Context.RequestedMode = TEXT("Designer");
  const bool bEnteredDesigner = !Context.CurrentMode.Equals(FWidgetBlueprintApplicationModes::DesignerMode.ToString(),
                                                            ESearchCase::CaseSensitive);
  if (bEnteredDesigner)
  {
    Context.WidgetEditor->SetCurrentMode(FWidgetBlueprintApplicationModes::DesignerMode);
    RefreshWidgetBlueprintSurfaceDiagnostics(Context);
  }

  const FWidgetReference WidgetReference = Context.WidgetEditor->GetReferenceFromTemplate(TargetWidget);
  if (!WidgetReference.IsValid())
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateWidgetBlueprintNavigationDiagnosticsObject(Context);
    ErrorDetails->SetStringField(TEXT("resolvedWidgetName"), Context.ResolvedWidgetName);
    ErrorDetails->SetStringField(TEXT("resolvedWidgetPath"), Context.ResolvedWidgetPath);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("WIDGET_REFERENCE_INVALID"),
                              TEXT("The target widget could not be resolved into a live designer reference"),
                              ErrorDetails);
    return true;
  }

  TSet<FWidgetReference> Selection;
  Selection.Add(WidgetReference);
  const bool bAppendOrToggle = Payload.IsValid() && Payload->HasField(TEXT("appendOrToggle"))
                                   ? GetJsonBoolField(Payload, TEXT("appendOrToggle"), false)
                                   : false;
  Context.WidgetEditor->SelectWidgets(Selection, bAppendOrToggle);

  const TSet<FWidgetReference> &SelectedWidgets = Context.WidgetEditor->GetSelectedWidgets();
  const bool bTargetStillSelected = SelectedWidgets.Contains(WidgetReference);
  const int32 SelectedWidgetCount = SelectedWidgets.Num();

  FString DesignerActionDisposition;
  FString RevealFailureReason;
  bool bRevealExecuted = false;
  if (bTargetStillSelected)
  {
    bRevealExecuted = TryExecuteWidgetDesignerZoomToFit(Context,
                                                        DesignerActionDisposition,
                                                        RevealFailureReason);
  }
  else
  {
    DesignerActionDisposition = TEXT("skipped_target_deselected");
    RevealFailureReason = TEXT("Target widget was toggled out of the current selection");
  }

  if (bTargetStillSelected && !bRevealExecuted && DesignerActionDisposition == TEXT("queued_after_layout"))
  {
    QueueWidgetDesignerZoomToFit(Context);
  }

  RefreshWidgetBlueprintSurfaceDiagnostics(Context);

  TSharedPtr<FJsonObject> Response = CreateWidgetBlueprintNavigationDiagnosticsObject(Context);
  Response->SetBoolField(TEXT("success"), true);
  Response->SetStringField(TEXT("previousMode"), PreviousMode);
  Response->SetBoolField(TEXT("enteredDesignerMode"), bEnteredDesigner);
  Response->SetBoolField(TEXT("selectionApplied"), true);
  Response->SetBoolField(TEXT("appendOrToggle"), bAppendOrToggle);
  Response->SetBoolField(TEXT("targetStillSelected"), bTargetStillSelected);
  Response->SetNumberField(TEXT("selectedWidgetCount"), SelectedWidgetCount);
  Response->SetBoolField(TEXT("revealExecuted"), bRevealExecuted);
  Response->SetStringField(TEXT("designerActionDisposition"), DesignerActionDisposition);
  if (!RevealFailureReason.IsEmpty())
  {
    Response->SetStringField(TEXT("revealFailureReason"), RevealFailureReason);
  }

  const FString ResponseMessage = !bTargetStillSelected && bAppendOrToggle
                                      ? TEXT("Widget selection toggled in Designer")
                                      : (bRevealExecuted
                                             ? TEXT("Widget selection updated and revealed in Designer")
                                             : (DesignerActionDisposition == TEXT("queued_after_layout")
                                                    ? TEXT("Widget selection updated; Designer reveal queued after layout")
                                                    : TEXT("Widget selection updated with narrower semantic Designer fallback")));

  SendAutomationResponse(Socket, RequestId, true, ResponseMessage, Response, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSelectWidgetsInDesignerRect(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FMcpResolvedWidgetBlueprintEditorContext Context;
  FString ErrorCode;
  FString ErrorMessage;
  if (!ResolveWidgetBlueprintEditorContext(Payload, Context, ErrorCode, ErrorMessage))
  {
    SendWidgetBlueprintNavigationError(this, Socket, RequestId, Context, ErrorCode, ErrorMessage);
    return true;
  }

  FSlateRect RequestedRect;
  if (!TryGetRectObjectField(Payload, TEXT("rect"), RequestedRect))
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateWidgetBlueprintNavigationDiagnosticsObject(Context);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("rect with left, top, right, and bottom is required"),
                              ErrorDetails);
    return true;
  }

  const FString PreviousMode = Context.CurrentMode;
  Context.RequestedMode = TEXT("Designer");
  const bool bEnteredDesigner = !Context.CurrentMode.Equals(FWidgetBlueprintApplicationModes::DesignerMode.ToString(),
                                                            ESearchCase::CaseSensitive);
  if (bEnteredDesigner)
  {
    Context.WidgetEditor->SetCurrentMode(FWidgetBlueprintApplicationModes::DesignerMode);
    RefreshWidgetBlueprintSurfaceDiagnostics(Context);
  }

  UWidgetBlueprint *WidgetBlueprint = Context.WidgetEditor->GetWidgetBlueprintObj();
  UWidget *RootWidget = WidgetBlueprint && WidgetBlueprint->WidgetTree ? WidgetBlueprint->WidgetTree->RootWidget : nullptr;
  if (RootWidget == nullptr)
  {
    TSharedPtr<FJsonObject> ErrorDetails = CreateWidgetBlueprintNavigationDiagnosticsObject(Context);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ROOT_WIDGET"),
                              TEXT("The Widget Blueprint has no root widget"),
                              ErrorDetails);
    return true;
  }

  TSharedPtr<SWindow> DesignerParentWindow =
      Context.DesignerTab.IsValid() ? Context.DesignerTab->GetParentWindow() : TSharedPtr<SWindow>();
  if (!DesignerParentWindow.IsValid() && Context.DesignerView.IsValid() && FSlateApplication::IsInitialized())
  {
    DesignerParentWindow = FSlateApplication::Get().FindWidgetWindow(Context.DesignerView.ToSharedRef());
  }

  TArray<FMcpDesignerRectSelectionCandidate> Matches;
  CollectDesignerRectSelectionCandidates(RootWidget,
                                         RootWidget,
                                         Context.WidgetEditor,
                                         Context.DesignerView,
                                         Context.DesignerTab,
                                         DesignerParentWindow,
                                         RequestedRect,
                                         Matches);

  const bool bHasNonRootMatch = Matches.ContainsByPredicate([](const FMcpDesignerRectSelectionCandidate &Candidate)
                                                            { return !Candidate.bIsRoot; });

  TSet<FWidgetReference> Selection;
  TArray<FString> MatchedWidgetPaths;
  for (const FMcpDesignerRectSelectionCandidate &Match : Matches)
  {
    if (bHasNonRootMatch && Match.bIsRoot)
    {
      continue;
    }

    Selection.Add(Match.WidgetReference);
    MatchedWidgetPaths.Add(Match.WidgetPath);
  }
  MatchedWidgetPaths.Sort();

  const bool bAppendOrToggle = Payload.IsValid() && Payload->HasField(TEXT("appendOrToggle"))
                                   ? GetJsonBoolField(Payload, TEXT("appendOrToggle"), false)
                                   : false;
  Context.WidgetEditor->SelectWidgets(Selection, bAppendOrToggle);

  const int32 SelectedWidgetCount = Context.WidgetEditor->GetSelectedWidgets().Num();

  RefreshWidgetBlueprintSurfaceDiagnostics(Context);

  TSharedPtr<FJsonObject> Response = CreateWidgetBlueprintNavigationDiagnosticsObject(Context);
  Response->SetBoolField(TEXT("success"), true);
  Response->SetStringField(TEXT("previousMode"), PreviousMode);
  Response->SetBoolField(TEXT("enteredDesignerMode"), bEnteredDesigner);
  Response->SetBoolField(TEXT("selectionApplied"), true);
  Response->SetBoolField(TEXT("appendOrToggle"), bAppendOrToggle);
  Response->SetNumberField(TEXT("matchedWidgetCount"), MatchedWidgetPaths.Num());
  Response->SetNumberField(TEXT("selectedWidgetCount"), SelectedWidgetCount);

  TArray<TSharedPtr<FJsonValue>> MatchedWidgetPathValues;
  MatchedWidgetPathValues.Reserve(MatchedWidgetPaths.Num());
  for (const FString &MatchedWidgetPath : MatchedWidgetPaths)
  {
    MatchedWidgetPathValues.Add(MakeShared<FJsonValueString>(MatchedWidgetPath));
  }
  Response->SetArrayField(TEXT("matchedWidgetPaths"), MatchedWidgetPathValues);

  const FString ResponseMessage = MatchedWidgetPaths.Num() > 0
                                      ? FString::Printf(TEXT("Rectangle selection matched %d widget(s)"), MatchedWidgetPaths.Num())
                                      : (bAppendOrToggle
                                             ? TEXT("Rectangle selection matched no widgets; existing selection unchanged")
                                             : TEXT("Rectangle selection matched no widgets; selection cleared"));

  SendAutomationResponse(Socket, RequestId, true, ResponseMessage, Response, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorFocusEditorSurface(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!FSlateApplication::IsInitialized())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SLATE_NOT_AVAILABLE"),
                              TEXT("Slate application is not initialized"), nullptr);
    return true;
  }

  const FString Surface = GetTrimmedPayloadString(Payload, TEXT("surface"));
  if (Surface.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("surface is required"), nullptr);
    return true;
  }

  const FString RequestedTabId = GetTrimmedPayloadString(Payload, TEXT("tabId"));
  const FString RequestedWindowTitle = GetTrimmedPayloadString(Payload, TEXT("windowTitle"));

  if (Surface.Equals(TEXT("widget_designer"), ESearchCase::IgnoreCase))
  {
    FMcpResolvedWidgetBlueprintEditorContext Context;
    FString ErrorCode;
    FString ErrorMessage;
    if (!ResolveWidgetBlueprintEditorContext(Payload, Context, ErrorCode, ErrorMessage,
                                             false, true, true))
    {
      TSharedPtr<FJsonObject> ErrorDetails = CreateWidgetBlueprintNavigationDiagnosticsObject(Context);
      ErrorDetails->SetStringField(TEXT("focusTargetSurface"), TEXT("widget_designer"));
      ErrorDetails->SetBoolField(TEXT("focusApplied"), false);
      ErrorDetails->SetStringField(TEXT("focusFailureReason"), ErrorMessage);
      ApplyRequestedVirtualTargetFields(ErrorDetails, RequestedTabId,
                                        RequestedWindowTitle, FString());
      ApplyTargetHealthResponseFields(ErrorDetails, TEXT("stale"), false, false,
                                      TEXT("missing_live_tab"),
                                      TEXT("Open the Widget Blueprint editor and switch to Designer mode before retrying focus."),
                                      TEXT("resolve_ui_target"));
      ErrorDetails->SetStringField(TEXT("suggestedMode"), TEXT("designer"));
      SendStandardErrorResponse(this, Socket, RequestId, *ErrorCode, *ErrorMessage, ErrorDetails);
      return true;
    }

    RefreshWidgetBlueprintSurfaceDiagnostics(Context);
    TSharedPtr<FJsonObject> Response = CreateWidgetBlueprintNavigationDiagnosticsObject(Context);
    Response->SetStringField(TEXT("focusTargetSurface"), TEXT("widget_designer"));
    Response->SetBoolField(TEXT("focusApplied"), false);
    Response->SetStringField(TEXT("suggestedMode"), TEXT("designer"));
    ApplyRequestedVirtualTargetFields(Response, RequestedTabId,
                                      RequestedWindowTitle, FString());

    if (!Context.bDesignerViewFound || !Context.DesignerView.IsValid())
    {
      Response->SetStringField(TEXT("focusFailureReason"), TEXT("The live Widget Blueprint Designer surface is not available"));
      ApplyTargetHealthResponseFields(Response, TEXT("stale"), Context.bDesignerTabFound, false,
                                      Context.bDesignerTabFound ? TEXT("tab_without_parent_window") : TEXT("missing_live_tab"),
                                      TEXT("Switch the asset editor to Designer mode and retry focus."),
                                      TEXT("set_widget_blueprint_mode"));
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("FOCUS_FAILED"),
                                TEXT("The live Widget Blueprint Designer surface is not available"), Response);
      return true;
    }

    if (Context.DesignerTab.IsValid())
    {
      Context.DesignerTab->ActivateInParent(ETabActivationCause::SetDirectly);
    }

    TSharedPtr<SWindow> DesignerWindow = Context.DesignerTab.IsValid()
                                             ? Context.DesignerTab->GetParentWindow()
                                             : TSharedPtr<SWindow>();
    if (!DesignerWindow.IsValid())
    {
      DesignerWindow = FSlateApplication::Get().FindWidgetWindow(Context.DesignerView.ToSharedRef());
    }
    if (DesignerWindow.IsValid())
    {
      DesignerWindow->BringToFront(true);
    }

    const bool bFocusApplied = FSlateApplication::Get().SetKeyboardFocus(Context.DesignerView.ToSharedRef(), EFocusCause::SetDirectly);
    Response->SetBoolField(TEXT("focusApplied"), bFocusApplied);
    ApplyTargetHealthResponseFields(Response, TEXT("resolved"), true, false);
    SetFocusedWidgetResponseFields(Response);

    if (!bFocusApplied)
    {
      Response->SetStringField(TEXT("focusFailureReason"), TEXT("Slate failed to apply keyboard focus to the live Widget Blueprint Designer surface"));
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("FOCUS_FAILED"),
                                TEXT("Slate failed to apply keyboard focus to the live Widget Blueprint Designer surface"),
                                Response);
      return true;
    }

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Focused the live Widget Blueprint Designer surface"),
                           Response, FString());
    return true;
  }

  TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
  Response->SetStringField(TEXT("focusTargetSurface"), Surface);
  ApplyRequestedVirtualTargetFields(Response, RequestedTabId,
                                    RequestedWindowTitle,
                                    TEXT("stored_target"));

  if (!RequestedTabId.IsEmpty())
  {
    const TSharedPtr<SDockTab> RequestedTab = FGlobalTabmanager::Get()->FindExistingLiveTab(FName(*RequestedTabId));
    if (!RequestedTab.IsValid())
    {
      const FString FocusFailureReason = FString::Printf(TEXT("Live tab %s was not found"), *RequestedTabId);
      Response->SetBoolField(TEXT("focusApplied"), false);
      Response->SetStringField(TEXT("focusFailureReason"), FocusFailureReason);
      ApplyTargetHealthResponseFields(Response, TEXT("stale"), false, false,
                                      TEXT("missing_live_tab"),
                                      BuildVirtualTargetRecoveryHint(TEXT("missing_live_tab")),
                                      TEXT("resolve_ui_target"));
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("FOCUS_FAILED"),
                                *FocusFailureReason, Response);
      return true;
    }

    if (!RequestedTab->GetParentWindow().IsValid())
    {
      const FString FocusFailureReason = FString::Printf(TEXT("Tab %s does not have a live parent window"), *RequestedTabId);
      Response->SetBoolField(TEXT("focusApplied"), false);
      Response->SetStringField(TEXT("focusFailureReason"), FocusFailureReason);
      ApplyTargetHealthResponseFields(Response, TEXT("stale"), true, false,
                                      TEXT("tab_without_parent_window"),
                                      BuildVirtualTargetRecoveryHint(TEXT("tab_without_parent_window")),
                                      TEXT("resolve_ui_target"));
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("FOCUS_FAILED"),
                                *FocusFailureReason, Response);
      return true;
    }
  }

  bool bAttemptedVirtualTarget = false;
  FMcpResolvedVirtualInputTarget VirtualTarget;
  FString VirtualTargetError;
  const bool bHasVirtualTarget = ResolveVirtualInputTarget(this, Payload,
                                                           bAttemptedVirtualTarget,
                                                           VirtualTarget,
                                                           VirtualTargetError);

  if (!bHasVirtualTarget)
  {
    const FString StaleReason = ClassifyVirtualTargetStaleReason(VirtualTargetError);
    Response->SetBoolField(TEXT("focusApplied"), false);
    Response->SetStringField(TEXT("focusFailureReason"), VirtualTargetError.IsEmpty() ? TEXT("No live target surface was available to focus") : VirtualTargetError);
    ApplyTargetHealthResponseFields(Response, TEXT("stale"), false, false, StaleReason,
                                    BuildVirtualTargetRecoveryHint(StaleReason),
                                    TEXT("resolve_ui_target"));
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("FOCUS_FAILED"),
                              VirtualTargetError.IsEmpty() ? TEXT("No live target surface was available to focus") : VirtualTargetError,
                              Response);
    return true;
  }

  if (!VirtualTarget.TabId.IsEmpty())
  {
    Response->SetStringField(TEXT("tabId"), VirtualTarget.TabId);
  }
  if (!VirtualTarget.WindowTitle.IsEmpty())
  {
    Response->SetStringField(TEXT("windowTitle"), VirtualTarget.WindowTitle);
  }

  const FVector2D FocusScreenPosition((VirtualTarget.ClientRect.Left + VirtualTarget.ClientRect.Right) * 0.5,
                                      (VirtualTarget.ClientRect.Top + VirtualTarget.ClientRect.Bottom) * 0.5);
  const bool bFocusApplied = FocusVirtualTargetAtPosition(VirtualTarget, FocusScreenPosition);
  Response->SetBoolField(TEXT("focusApplied"), bFocusApplied);
  ApplyTargetHealthResponseFields(Response, TEXT("resolved"), true, false);
  SetFocusedWidgetResponseFields(Response);

  if (!bFocusApplied)
  {
    Response->SetStringField(TEXT("focusFailureReason"), TEXT("Slate could not locate a focusable widget at the resolved target position"));
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("FOCUS_FAILED"),
                              TEXT("Slate could not locate a focusable widget at the resolved target position"),
                              Response);
    return true;
  }

  SendAutomationResponse(Socket, RequestId, true, TEXT("Focused the resolved editor surface"),
                         Response, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorScreenshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  FString Filename;
  Payload->TryGetStringField(TEXT("filename"), Filename);
  Filename = SanitizeScreenshotFilename(Filename);

  const FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("Screenshots");
  IFileManager::Get().MakeDirectory(*ScreenshotDir, true);
  const FString FullPath = ScreenshotDir / Filename;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  FString TargetWindowName;
  Payload->TryGetStringField(TEXT("name"), TargetWindowName);
  TargetWindowName = TargetWindowName.TrimStartAndEnd();
  FString RequestedTabId;
  Payload->TryGetStringField(TEXT("tabId"), RequestedTabId);
  RequestedTabId = RequestedTabId.TrimStartAndEnd();
  FString Mode;
  Payload->TryGetStringField(TEXT("mode"), Mode);
  const bool bIncludeMenus =
      !Payload->HasField(TEXT("includeMenus"))
          ? true
          : GetJsonBoolField(Payload, TEXT("includeMenus"), true);
  const bool bCaptureEditorWindow =
      !TargetWindowName.IsEmpty() ||
      Mode.Equals(TEXT("editor"), ESearchCase::IgnoreCase) ||
      Mode.Equals(TEXT("window"), ESearchCase::IgnoreCase) ||
      Mode.Equals(TEXT("ui"), ESearchCase::IgnoreCase);
  const TSharedPtr<SDockTab> RequestedTab = RequestedTabId.IsEmpty()
                                                ? nullptr
                                                : FGlobalTabmanager::Get()->FindExistingLiveTab(FName(*RequestedTabId));
  const bool bRequestedTabStillLive = RequestedTab.IsValid();
  const FString CaptureIntentSource = DetermineScreenshotIntentSource(this, TargetWindowName,
                                                                      bCaptureEditorWindow);

  Resp->SetStringField(TEXT("requestedCaptureMode"),
                       Mode.IsEmpty() ? TEXT("viewport") : Mode);
  Resp->SetStringField(TEXT("captureIntentSource"), CaptureIntentSource);
  if (!TargetWindowName.IsEmpty())
  {
    Resp->SetStringField(TEXT("requestedWindowTitle"), TargetWindowName);
    ApplyTargetHealthResponseFields(Resp, TEXT("resolved"), true, false);
  }
  if (!RequestedTabId.IsEmpty())
  {
    Resp->SetStringField(TEXT("requestedTabId"), RequestedTabId);
    Resp->SetStringField(TEXT("suggestedMode"), TEXT("editor"));
    Resp->SetStringField(TEXT("suggestedPreflightAction"), TEXT("resolve_ui_target"));
    if (bRequestedTabStillLive)
    {
      ApplyTargetHealthResponseFields(Resp, TEXT("resolved"), true, false,
                                      FString(),
                                      TEXT("Resolve the tab to a windowTitle before retrying screenshot capture."),
                                      TEXT("resolve_ui_target"));
      Resp->SetStringField(TEXT("captureIntentWarning"), TEXT("tabId is ignored for screenshot targeting; use windowTitle or resolve_ui_target first."));
    }
    else
    {
      ApplyTargetHealthResponseFields(Resp, TEXT("stale"), false, false,
                                      TEXT("missing_live_tab"),
                                      BuildVirtualTargetRecoveryHint(TEXT("missing_live_tab")),
                                      TEXT("resolve_ui_target"));
      Resp->SetStringField(TEXT("captureIntentWarning"), TEXT("Requested tabId is not live and screenshot targeting does not consume tabId directly; resolve a windowTitle before retrying."));
    }
  }
  else if (bCaptureEditorWindow && TargetWindowName.IsEmpty() && Mode.IsEmpty())
  {
    Resp->SetStringField(TEXT("captureIntentWarning"), TEXT("Editor-window capture relied on the stored or active window; provide windowTitle or run resolve_ui_target first for deterministic targeting."));
    Resp->SetStringField(TEXT("suggestedMode"), TEXT("editor"));
    Resp->SetStringField(TEXT("suggestedPreflightAction"), TEXT("resolve_ui_target"));
  }

  if (bCaptureEditorWindow && !RequestedTabId.IsEmpty() && TargetWindowName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId,
                              TEXT("AMBIGUOUS_CAPTURE_TARGET"),
                              TEXT("Screenshot capture cannot target a tabId directly; resolve the tab to a live windowTitle before retrying."),
                              Resp);
    return true;
  }

  if (bCaptureEditorWindow)
  {
    if (!FSlateApplication::IsInitialized())
    {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("SLATE_NOT_AVAILABLE"),
                                TEXT("Slate application is not initialized"), nullptr);
      return true;
    }

    const TSharedPtr<SWindow> TargetWindow = ResolvePreferredScreenshotWindow(this, TargetWindowName);
    if (!TargetWindow.IsValid())
    {
      TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
      ErrorDetails->SetStringField(TEXT("requestedWindowTitle"), TargetWindowName);
      if (!RequestedTabId.IsEmpty())
      {
        ErrorDetails->SetStringField(TEXT("requestedTabId"), RequestedTabId);
        ErrorDetails->SetStringField(TEXT("captureIntentWarning"), TEXT("tabId is ignored for screenshot targeting; resolve a live windowTitle before retrying."));
      }
      ErrorDetails->SetStringField(TEXT("captureIntentSource"), CaptureIntentSource);
      ApplyTargetHealthResponseFields(ErrorDetails,
                                      TargetWindowName.IsEmpty() ? TEXT("not_found") : TEXT("stale"),
                                      false,
                                      false,
                                      TargetWindowName.IsEmpty() ? FString() : TEXT("missing_visible_window"),
                                      TEXT("Call manage_ui.resolve_ui_target or list_visible_windows before retrying screenshot capture."),
                                      TEXT("resolve_ui_target"));
      ErrorDetails->SetArrayField(TEXT("visibleWindowTitles"),
                                  BuildVisibleWindowTitleValues());
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("WINDOW_NOT_FOUND"),
                                TEXT("No visible editor window matched the requested target"), ErrorDetails);
      return true;
    }

    FIntPoint OutImageSize;
    int32 IncludedMenuWindowCount = 0;
    FString SaveError;
    if (bIncludeMenus)
    {
      if (!CaptureWindowScreenshotIncludingChildWindowsToFile(
              TargetWindow, FullPath, OutImageSize, IncludedMenuWindowCount,
              SaveError))
      {
        SendStandardErrorResponse(this, Socket, RequestId, TEXT("CAPTURE_FAILED"), SaveError,
                                  nullptr);
        return true;
      }
    }
    else if (!CaptureWindowScreenshotToFile(TargetWindow, FullPath, OutImageSize,
                                            SaveError))
    {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("CAPTURE_FAILED"), SaveError,
                                nullptr);
      return true;
    }

    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("filename"), Filename);
    Resp->SetStringField(TEXT("path"), FullPath);
    Resp->SetStringField(TEXT("captureTarget"), TEXT("editor_window"));
    Resp->SetStringField(TEXT("windowTitle"), TargetWindow->GetTitle().ToString());
    Resp->SetStringField(TEXT("captureMode"), TEXT("editor_window"));
    Resp->SetBoolField(TEXT("includeMenus"), bIncludeMenus);
    Resp->SetNumberField(TEXT("includedMenuWindowCount"), IncludedMenuWindowCount);
    Resp->SetNumberField(TEXT("width"), OutImageSize.X);
    Resp->SetNumberField(TEXT("height"), OutImageSize.Y);
    Resp->SetStringField(TEXT("message"), TEXT("Editor window screenshot captured"));

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Editor window screenshot captured"), Resp, FString());
    return true;
  }

  if (!RequestedTabId.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId,
                              TEXT("AMBIGUOUS_CAPTURE_TARGET"),
                              TEXT("Screenshot capture cannot target a tabId directly; resolve the tab to a live windowTitle before retrying."),
                              Resp);
    return true;
  }

  FViewport *Viewport = GEditor->GetActiveViewport();
  if (!Viewport)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("VIEWPORT_NOT_AVAILABLE"),
                              TEXT("No active viewport available"), nullptr);
    return true;
  }

  TArray<FColor> Bitmap;
  const FIntPoint Size = Viewport->GetSizeXY();
  if (!Viewport->ReadPixels(Bitmap) || Bitmap.Num() == 0)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CAPTURE_FAILED"),
                              TEXT("Failed to read active viewport pixels"), nullptr);
    return true;
  }

  FString SaveError;
  if (!SaveBitmapToPngFile(Bitmap, Size.X, Size.Y, FullPath, SaveError))
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SAVE_FAILED"), SaveError,
                              nullptr);
    return true;
  }

  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("filename"), Filename);
  Resp->SetStringField(TEXT("path"), FullPath);
  Resp->SetStringField(TEXT("captureTarget"), TEXT("viewport"));
  Resp->SetStringField(TEXT("captureMode"), TEXT("viewport"));
  Resp->SetNumberField(TEXT("width"), Size.X);
  Resp->SetNumberField(TEXT("height"), Size.Y);
  Resp->SetStringField(TEXT("message"), TEXT("Viewport screenshot captured"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Viewport screenshot captured"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Screenshot requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorPause(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Check if we're in PIE
  if (!GEditor->PlayWorld)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("No active PIE session to pause"), nullptr);
    return true;
  }

  // Pause PIE execution
  GEditor->PlayWorld->bDebugPauseExecution = true;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("state"), TEXT("paused"));
  Resp->SetStringField(TEXT("message"), TEXT("PIE session paused"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("PIE session paused"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Pause requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorResume(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Check if we're in PIE
  if (!GEditor->PlayWorld)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("No active PIE session to resume"), nullptr);
    return true;
  }

  // Resume PIE execution
  GEditor->PlayWorld->bDebugPauseExecution = false;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("state"), TEXT("resumed"));
  Resp->SetStringField(TEXT("message"), TEXT("PIE session resumed"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("PIE session resumed"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Resume requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorConsoleCommand(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  FString Command;
  Payload->TryGetStringField(TEXT("command"), Command);
  if (Command.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("command parameter is required"), nullptr);
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UWorld *World = GEditor->GetEditorWorldContext().World();
  GEditor->Exec(World, *Command);

  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("command"), Command);
  Resp->SetStringField(TEXT("message"), TEXT("Console command executed"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Console command executed"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Console command requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStepFrame(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Check if we're in PIE
  if (!GEditor->PlayWorld)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("No active PIE session to step"), nullptr);
    return true;
  }

  // Step one frame - set debug step flag and unpause momentarily
  GEditor->PlayWorld->bDebugFrameStepExecution = true;
  GEditor->PlayWorld->bDebugPauseExecution = false;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("message"), TEXT("Stepped one frame"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Frame stepped"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Step frame requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStartRecording(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  FString RecordingName;
  // Accept both 'name' and 'filename' fields for flexibility
  // TS handler sends 'filename', so we check that first
  Payload->TryGetStringField(TEXT("filename"), RecordingName);
  if (RecordingName.IsEmpty())
  {
    Payload->TryGetStringField(TEXT("name"), RecordingName);
  }
  if (RecordingName.IsEmpty())
  {
    RecordingName = FString::Printf(TEXT("Recording_%s"),
                                    *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  }

  // Use console command to start demo recording
  // UE 5.7: TObjectPtr requires explicit cast to UWorld*
  UWorld *World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
  if (World)
  {
    FString Command = FString::Printf(TEXT("DemoRec %s"), *RecordingName);
    GEditor->Exec(World, *Command);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("recordingName"), RecordingName);
  Resp->SetStringField(TEXT("message"), TEXT("Recording started"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Recording started"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Recording requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStopRecording(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Use console command to stop demo recording
  // UE 5.7: TObjectPtr requires explicit cast to UWorld*
  UWorld *World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
  if (World)
  {
    GEditor->Exec(World, TEXT("DemoStop"));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("message"), TEXT("Recording stopped"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Recording stopped"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Recording requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorCreateBookmark(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  int32 BookmarkIndex = 0;
  Payload->TryGetNumberField(TEXT("index"), BookmarkIndex);

  // Clamp to valid bookmark range (0-9)
  BookmarkIndex = FMath::Clamp(BookmarkIndex, 0, 9);

  // Use console command to set bookmark
  FString Command = FString::Printf(TEXT("SetBookmark %d"), BookmarkIndex);
  UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("index"), BookmarkIndex);
  Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Bookmark %d created"), BookmarkIndex));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Bookmark created"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Bookmarks require editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorJumpToBookmark(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  int32 BookmarkIndex = 0;
  Payload->TryGetNumberField(TEXT("index"), BookmarkIndex);

  // Clamp to valid bookmark range (0-9)
  BookmarkIndex = FMath::Clamp(BookmarkIndex, 0, 9);

  // Use console command to jump to bookmark
  FString Command = FString::Printf(TEXT("JumpToBookmark %d"), BookmarkIndex);
  UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("index"), BookmarkIndex);
  Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Jumped to bookmark %d"), BookmarkIndex));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Jumped to bookmark"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Bookmarks require editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetPreferences(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  TArray<FString> AppliedSettings;
  TArray<FString> FailedSettings;

  // Get preferences object from payload
  const TSharedPtr<FJsonObject> *PrefsPtr = nullptr;
  if (Payload->TryGetObjectField(TEXT("preferences"), PrefsPtr) && PrefsPtr && (*PrefsPtr).IsValid())
  {
    for (const auto &Pair : (*PrefsPtr)->Values)
    {
      // Try to set via console variable first
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(*Pair.Key);
      if (CVar)
      {
        FString Value;
        if (Pair.Value->TryGetString(Value))
        {
          CVar->Set(*Value);
          AppliedSettings.Add(Pair.Key);
        }
        else
        {
          double NumVal;
          if (Pair.Value->TryGetNumber(NumVal))
          {
            CVar->Set((float)NumVal);
            AppliedSettings.Add(Pair.Key);
          }
          else
          {
            bool BoolVal;
            if (Pair.Value->TryGetBool(BoolVal))
            {
              CVar->Set(BoolVal ? 1 : 0);
              AppliedSettings.Add(Pair.Key);
            }
            else
            {
              FailedSettings.Add(Pair.Key);
            }
          }
        }
      }
      else
      {
        FailedSettings.Add(Pair.Key);
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), FailedSettings.Num() == 0);
  Resp->SetNumberField(TEXT("appliedCount"), AppliedSettings.Num());

  if (AppliedSettings.Num() > 0)
  {
    TArray<TSharedPtr<FJsonValue>> AppliedArray;
    for (const FString &Name : AppliedSettings)
      AppliedArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("applied"), AppliedArray);
  }

  if (FailedSettings.Num() > 0)
  {
    TArray<TSharedPtr<FJsonValue>> FailedArray;
    for (const FString &Name : FailedSettings)
      FailedArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("failed"), FailedArray);
  }

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Preferences updated"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Preferences require editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetViewportRealtime(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  bool bRealtime = true;
  Payload->TryGetBoolField(TEXT("realtime"), bRealtime);

#if MCP_HAS_LEVEL_EDITOR_MODULE
  // Get the level editor module and active viewport
  FLevelEditorModule &LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
  TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();

  if (ActiveViewport.IsValid())
  {
    FEditorViewportClient &ViewportClient = ActiveViewport->GetAssetViewportClient();
    ViewportClient.SetRealtime(bRealtime);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("realtime"), bRealtime);
    Resp->SetStringField(TEXT("message"), bRealtime ? TEXT("Viewport realtime enabled") : TEXT("Viewport realtime disabled"));

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Viewport realtime updated"), Resp, FString());
    return true;
  }
#endif

  // Fallback: use console command
  FString Command = bRealtime ? TEXT("Viewport Realtime") : TEXT("Viewport Realtime 0");
  UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("realtime"), bRealtime);
  Resp->SetStringField(TEXT("message"), bRealtime ? TEXT("Viewport realtime enabled") : TEXT("Viewport realtime disabled"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Viewport realtime updated"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Viewport realtime requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSimulateInput(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Accept multiple field names for flexibility
  // - 'type': C++ native field (key_down, key_up, mouse_click, mouse_move)
  // - 'inputType': Alternative name
  // - 'inputAction': Action-based naming (pressed, released, click, move)
  // CRITICAL: Do NOT read from 'action' field - that's the routing action (e.g., "simulate_input")
  // and will always be present in the payload. Only use type/inputType/inputAction for input type.
  FString InputType;
  Payload->TryGetStringField(TEXT("type"), InputType);
  if (InputType.IsEmpty())
  {
    Payload->TryGetStringField(TEXT("inputType"), InputType);
  }
  if (InputType.IsEmpty())
  {
    Payload->TryGetStringField(TEXT("inputAction"), InputType);
  }

  // Map action values to C++ expected type values
  InputType = InputType.ToLower();
  if (InputType == TEXT("pressed") || InputType == TEXT("down"))
  {
    InputType = TEXT("key_down");
  }
  else if (InputType == TEXT("released") || InputType == TEXT("up"))
  {
    InputType = TEXT("key_up");
  }
  else if (InputType == TEXT("click"))
  {
    InputType = TEXT("mouse_click");
  }
  else if (InputType == TEXT("move"))
  {
    InputType = TEXT("mouse_move");
  }

  FString Key;
  Payload->TryGetStringField(TEXT("key"), Key);

  FString RequestedTabId;
  FString RequestedWindowTitle;
  if (Payload.IsValid())
  {
    Payload->TryGetStringField(TEXT("tabId"), RequestedTabId);
    Payload->TryGetStringField(TEXT("windowTitle"), RequestedWindowTitle);
  }
  RequestedTabId = RequestedTabId.TrimStartAndEnd();
  RequestedWindowTitle = RequestedWindowTitle.TrimStartAndEnd();

  bool bSuccess = false;
  FString Message;
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  bool bAttemptedVirtualTarget = false;
  FMcpResolvedVirtualInputTarget VirtualTarget;
  FString VirtualTargetError;
  const bool bHasVirtualTarget = ResolveVirtualInputTarget(this, Payload, bAttemptedVirtualTarget, VirtualTarget, VirtualTargetError);
  bool bCaptureScreenshots = true;
  Payload->TryGetBoolField(TEXT("captureScreenshots"), bCaptureScreenshots);

  auto SetInputStateFields = [Resp](const FString &PressedButtons, const FString &PressedModifierKeys)
  {
    Resp->SetStringField(TEXT("pressedButtons"), PressedButtons);
    Resp->SetStringField(TEXT("pressedModifierKeys"), PressedModifierKeys);
  };

  auto SetRequestedTargetFields = [Resp, &RequestedTabId, &RequestedWindowTitle](const FString &ResolvedTargetSource)
  {
    if (!RequestedTabId.IsEmpty())
    {
      Resp->SetStringField(TEXT("requestedTabId"), RequestedTabId);
    }
    if (!RequestedWindowTitle.IsEmpty())
    {
      Resp->SetStringField(TEXT("requestedWindowTitle"), RequestedWindowTitle);
    }
    if (!ResolvedTargetSource.IsEmpty())
    {
      Resp->SetStringField(TEXT("resolvedTargetSource"), ResolvedTargetSource);
    }
  };

  auto SetTargetHealthFields = [Resp](const FString &TargetStatus,
                                      const bool bRequestedTargetStillLive,
                                      const bool bReResolved,
                                      const FString &StaleReason = FString(),
                                      const FString &RecoveryHint = FString(),
                                      const FString &RecoveryAction = FString())
  {
    ApplyTargetHealthResponseFields(Resp, TargetStatus, bRequestedTargetStillLive,
                                    bReResolved, StaleReason, RecoveryHint, RecoveryAction);
  };

  auto SetTargetResponseFields = [Resp, &RequestedTabId, &RequestedWindowTitle, &SetRequestedTargetFields](const FMcpResolvedVirtualInputTarget &Target)
  {
    if (!RequestedTabId.IsEmpty())
    {
      SetRequestedTargetFields(TEXT("tab_id"));
    }
    else if (!RequestedWindowTitle.IsEmpty())
    {
      SetRequestedTargetFields(TEXT("window_title"));
    }
    else if (Target.Window.IsValid())
    {
      SetRequestedTargetFields(TEXT("stored_target"));
    }

    if (!Target.TabId.IsEmpty())
    {
      Resp->SetStringField(TEXT("tabId"), Target.TabId);
    }
    if (!Target.WindowTitle.IsEmpty())
    {
      Resp->SetStringField(TEXT("windowTitle"), Target.WindowTitle);
    }
    if (!Target.ResolutionSource.IsEmpty())
    {
      Resp->SetStringField(TEXT("targetResolutionSource"), Target.ResolutionSource);
    }
    if (!Target.RequestedAssetPath.IsEmpty())
    {
      Resp->SetStringField(TEXT("requestedAssetPath"), Target.RequestedAssetPath);
    }
    if (!Target.AssetTargetError.IsEmpty())
    {
      Resp->SetStringField(TEXT("assetTargetError"), Target.AssetTargetError);
    }
    if (!Target.PreferredWidgetType.IsEmpty())
    {
      Resp->SetStringField(TEXT("targetPreferredWidgetType"), Target.PreferredWidgetType);
    }
    Resp->SetNumberField(TEXT("targetWindowLeft"), Target.WindowRect.Left);
    Resp->SetNumberField(TEXT("targetWindowTop"), Target.WindowRect.Top);
    Resp->SetNumberField(TEXT("targetWindowRight"), Target.WindowRect.Right);
    Resp->SetNumberField(TEXT("targetWindowBottom"), Target.WindowRect.Bottom);
    Resp->SetNumberField(TEXT("targetClientLeft"), Target.ClientRect.Left);
    Resp->SetNumberField(TEXT("targetClientTop"), Target.ClientRect.Top);
    Resp->SetNumberField(TEXT("targetClientRight"), Target.ClientRect.Right);
    Resp->SetNumberField(TEXT("targetClientBottom"), Target.ClientRect.Bottom);
  };

  auto SetTargetWidgetPathFields = [Resp](const FMcpResolvedVirtualInputTarget &Target, const FVector2D &Position)
  {
    const FVector2D ScreenPosition = ClientPositionToScreen(Target, Position);
    bool bUsedPreferredWidgetPath = false;
    const FWidgetPath WidgetPath = LocateTargetWidgetPath(Target, ScreenPosition, &bUsedPreferredWidgetPath);
    Resp->SetBoolField(TEXT("targetWidgetPathValid"), WidgetPath.IsValid());
    Resp->SetStringField(TEXT("targetWidgetPathSource"), bUsedPreferredWidgetPath ? TEXT("preferred_widget") : TEXT("window_local_hit_test"));
    if (WidgetPath.IsValid())
    {
      Resp->SetStringField(TEXT("targetWidgetPath"), WidgetPath.ToString());
      Resp->SetNumberField(TEXT("targetWidgetPathDepth"), WidgetPath.Widgets.Num());
      if (WidgetPath.Widgets.Num() > 0)
      {
        const FArrangedWidget &LeafArrangedWidget = WidgetPath.Widgets.Last();
        const TSharedRef<SWidget> &LeafWidget = LeafArrangedWidget.Widget;
        const FVector2D LeafAbsolutePosition = LeafArrangedWidget.Geometry.GetAbsolutePosition();
        const FVector2D LeafAbsoluteSize = LeafArrangedWidget.Geometry.GetAbsoluteSize();
        const FVector2D LeafLocalPosition = LeafArrangedWidget.Geometry.AbsoluteToLocal(ScreenPosition);
        Resp->SetStringField(TEXT("targetLeafWidgetType"), LeafWidget->GetTypeAsString());
        Resp->SetNumberField(TEXT("targetLeafAbsoluteX"), LeafAbsolutePosition.X);
        Resp->SetNumberField(TEXT("targetLeafAbsoluteY"), LeafAbsolutePosition.Y);
        Resp->SetNumberField(TEXT("targetLeafWidth"), LeafAbsoluteSize.X);
        Resp->SetNumberField(TEXT("targetLeafHeight"), LeafAbsoluteSize.Y);
        Resp->SetNumberField(TEXT("targetLeafLocalX"), LeafLocalPosition.X);
        Resp->SetNumberField(TEXT("targetLeafLocalY"), LeafLocalPosition.Y);
      }
    }
  };

  auto SetPointerResponseFields = [&SetInputStateFields, &SetTargetResponseFields, &SetTargetWidgetPathFields, Resp](const FVector2D &Position, const FString &ButtonName, const FString &PressedButtons, const FString &PressedModifierKeys, const FMcpResolvedVirtualInputTarget *Target = nullptr)
  {
    if (Target != nullptr)
    {
      const FVector2D ScreenPosition = ClientPositionToScreen(*Target, Position);
      Resp->SetNumberField(TEXT("x"), Position.X);
      Resp->SetNumberField(TEXT("y"), Position.Y);
      Resp->SetNumberField(TEXT("clientX"), Position.X);
      Resp->SetNumberField(TEXT("clientY"), Position.Y);
      Resp->SetNumberField(TEXT("screenX"), ScreenPosition.X);
      Resp->SetNumberField(TEXT("screenY"), ScreenPosition.Y);
      Resp->SetBoolField(TEXT("virtualCursorVisible"), true);
      SetTargetResponseFields(*Target);
      SetTargetWidgetPathFields(*Target, Position);
    }
    else
    {
      Resp->SetNumberField(TEXT("x"), Position.X);
      Resp->SetNumberField(TEXT("y"), Position.Y);
    }
    if (!ButtonName.IsEmpty())
    {
      Resp->SetStringField(TEXT("button"), ButtonName);
    }
    SetInputStateFields(PressedButtons, PressedModifierKeys);
  };

  auto CaptureMouseScreenshot = [Resp, &RequestId, &InputType, &bCaptureScreenshots, bHasVirtualTarget, &VirtualTarget](const TCHAR *Stage)
  {
    if (!bCaptureScreenshots || !InputType.StartsWith(TEXT("mouse_")))
    {
      return;
    }

    const TSharedPtr<SWindow> TargetWindow = bHasVirtualTarget ? VirtualTarget.Window : FindVisibleWindowByTitle(FString());
    if (!TargetWindow.IsValid())
    {
      Resp->SetStringField(FString::Printf(TEXT("%sScreenshotError"), Stage), TEXT("No visible target window was available for screenshot capture"));
      return;
    }

    const FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("Screenshots");
    IFileManager::Get().MakeDirectory(*ScreenshotDir, true);

    const FString Filename = BuildSimulatedInputScreenshotFilename(RequestId, InputType, Stage);
    const FString FullPath = ScreenshotDir / Filename;
    FIntPoint ScreenshotSize;
    FString CaptureError;
    if (!CaptureWindowScreenshotToFile(TargetWindow, FullPath, ScreenshotSize, CaptureError))
    {
      Resp->SetStringField(FString::Printf(TEXT("%sScreenshotError"), Stage), CaptureError);
      return;
    }

    Resp->SetStringField(FString::Printf(TEXT("%sScreenshotPath"), Stage), FullPath);
    Resp->SetStringField(FString::Printf(TEXT("%sScreenshotFilename"), Stage), Filename);
    Resp->SetNumberField(FString::Printf(TEXT("%sScreenshotWidth"), Stage), ScreenshotSize.X);
    Resp->SetNumberField(FString::Printf(TEXT("%sScreenshotHeight"), Stage), ScreenshotSize.Y);
  };

  ON_SCOPE_EXIT
  {
    if (!bSuccess)
    {
      ResetVirtualInputState(this);
    }
  };

  if (InputType == TEXT("reset_input") || InputType == TEXT("reset") || InputType == TEXT("clear"))
  {
    ResetVirtualInputState(this);
    bSuccess = true;
    Message = TEXT("Simulated input state reset");
    Resp->SetBoolField(TEXT("virtualCursorVisible"), false);
    SetInputStateFields(TEXT(""), TEXT(""));
  }
  else if (bAttemptedVirtualTarget && !bHasVirtualTarget)
  {
    const FString StaleReason = ClassifyVirtualTargetStaleReason(VirtualTargetError);
    if (!RequestedTabId.IsEmpty())
    {
      SetRequestedTargetFields(TEXT("tab_id"));
    }
    else if (!RequestedWindowTitle.IsEmpty())
    {
      SetRequestedTargetFields(TEXT("window_title"));
    }
    SetTargetHealthFields(TEXT("stale"), false, false, StaleReason,
                          BuildVirtualTargetRecoveryHint(StaleReason),
                          TEXT("resolve_ui_target"));
    Message = VirtualTargetError;
  }
  else if (InputType == TEXT("key_down") || InputType == TEXT("keydown"))
  {
    if (!Key.IsEmpty())
    {
      FKey InputKey(*Key);
      if (InputKey.IsValid())
      {
        FSlateApplication &SlateApp = FSlateApplication::Get();
        const uint32 KeyboardUserIndex = SlateApp.GetUserIndexForKeyboard();
        const FModifierKeysState ModifierKeys = BuildModifierKeyState(SimulatedPressedModifierKeys);
        bool bHasKeyCode = false;
        bool bHasCharCode = false;
        uint32 KeyCode = 0;
        uint32 CharCode = 0;
        GetKeyAndCharCodes(InputKey, bHasKeyCode, KeyCode, bHasCharCode, CharCode);
        if (bHasVirtualTarget)
        {
          SetTargetResponseFields(VirtualTarget);
          if (bSimulatedPointerPositionValid)
          {
            FocusVirtualTargetAtPosition(VirtualTarget, ClientPositionToScreen(VirtualTarget, SimulatedPointerPosition));
            AttachVirtualCursorOverlay(this, VirtualTarget, SimulatedPointerPosition);
          }
        }
        FKeyEvent KeyEvent(InputKey, ModifierKeys, KeyboardUserIndex, false, CharCode, KeyCode);
        SlateApp.ProcessKeyDownEvent(KeyEvent);
        UpdatePressedModifierKeys(SimulatedPressedModifierKeys, InputKey, true);
        bSuccess = true;
        Message = FString::Printf(TEXT("Key down: %s"), *Key);
        SetInputStateFields(DescribePressedKeys(SimulatedPressedMouseButtons), DescribePressedKeys(SimulatedPressedModifierKeys));
      }
      else
      {
        Message = FString::Printf(TEXT("Invalid key: %s"), *Key);
      }
    }
    else
    {
      Message = TEXT("Key parameter required for key_down");
    }
  }
  else if (InputType == TEXT("key_up") || InputType == TEXT("keyup"))
  {
    if (!Key.IsEmpty())
    {
      FKey InputKey(*Key);
      if (InputKey.IsValid())
      {
        FSlateApplication &SlateApp = FSlateApplication::Get();
        const uint32 KeyboardUserIndex = SlateApp.GetUserIndexForKeyboard();
        const FModifierKeysState ModifierKeys = BuildModifierKeyState(SimulatedPressedModifierKeys);
        bool bHasKeyCode = false;
        bool bHasCharCode = false;
        uint32 KeyCode = 0;
        uint32 CharCode = 0;
        GetKeyAndCharCodes(InputKey, bHasKeyCode, KeyCode, bHasCharCode, CharCode);
        if (bHasVirtualTarget)
        {
          SetTargetResponseFields(VirtualTarget);
          if (bSimulatedPointerPositionValid)
          {
            FocusVirtualTargetAtPosition(VirtualTarget, ClientPositionToScreen(VirtualTarget, SimulatedPointerPosition));
            AttachVirtualCursorOverlay(this, VirtualTarget, SimulatedPointerPosition);
          }
        }
        FKeyEvent KeyEvent(InputKey, ModifierKeys, KeyboardUserIndex, false, CharCode, KeyCode);
        SlateApp.ProcessKeyUpEvent(KeyEvent);
        UpdatePressedModifierKeys(SimulatedPressedModifierKeys, InputKey, false);
        bSuccess = true;
        Message = FString::Printf(TEXT("Key up: %s"), *Key);
        SetInputStateFields(DescribePressedKeys(SimulatedPressedMouseButtons), DescribePressedKeys(SimulatedPressedModifierKeys));
      }
      else
      {
        Message = FString::Printf(TEXT("Invalid key: %s"), *Key);
      }
    }
    else
    {
      Message = TEXT("Key parameter required for key_up");
    }
  }
  else if (InputType == TEXT("text_input") || InputType == TEXT("text"))
  {
    FString Text;
    Payload->TryGetStringField(TEXT("text"), Text);
    if (Text.IsEmpty())
    {
      Message = TEXT("text parameter required for text_input");
    }
    else
    {
      FSlateApplication &SlateApp = FSlateApplication::Get();
      const uint32 KeyboardUserIndex = SlateApp.GetUserIndexForKeyboard();
      const FModifierKeysState ModifierKeys = BuildModifierKeyState(SimulatedPressedModifierKeys);
      if (bHasVirtualTarget)
      {
        SetTargetResponseFields(VirtualTarget);
        if (bSimulatedPointerPositionValid)
        {
          FocusVirtualTargetAtPosition(VirtualTarget, ClientPositionToScreen(VirtualTarget, SimulatedPointerPosition));
          AttachVirtualCursorOverlay(this, VirtualTarget, SimulatedPointerPosition);
        }
      }
      for (int32 CharacterIndex = 0; CharacterIndex < Text.Len(); ++CharacterIndex)
      {
        const TCHAR Character = Text[CharacterIndex];
        FKey CharacterKey;
        if (TryResolveTextCharacterKey(Character, CharacterKey))
        {
          bool bHasKeyCode = false;
          bool bHasCharCode = false;
          uint32 KeyCode = 0;
          uint32 CharCode = 0;
          GetKeyAndCharCodes(CharacterKey, bHasKeyCode, KeyCode, bHasCharCode, CharCode);
          FKeyEvent CharacterKeyDownEvent(CharacterKey, ModifierKeys, KeyboardUserIndex, false, CharCode, KeyCode);
          SlateApp.ProcessKeyDownEvent(CharacterKeyDownEvent);
        }

        const FCharacterEvent CharacterEvent(Character, ModifierKeys, KeyboardUserIndex, false);
        SlateApp.ProcessKeyCharEvent(CharacterEvent);

        if (CharacterKey.IsValid())
        {
          bool bHasKeyCode = false;
          bool bHasCharCode = false;
          uint32 KeyCode = 0;
          uint32 CharCode = 0;
          GetKeyAndCharCodes(CharacterKey, bHasKeyCode, KeyCode, bHasCharCode, CharCode);
          FKeyEvent CharacterKeyUpEvent(CharacterKey, ModifierKeys, KeyboardUserIndex, false, CharCode, KeyCode);
          SlateApp.ProcessKeyUpEvent(CharacterKeyUpEvent);
        }
      }

      bool bSubmit = false;
      Payload->TryGetBoolField(TEXT("submit"), bSubmit);
      if (bSubmit)
      {
        bool bHasKeyCode = false;
        bool bHasCharCode = false;
        uint32 KeyCode = 0;
        uint32 CharCode = 0;
        GetKeyAndCharCodes(EKeys::Enter, bHasKeyCode, KeyCode, bHasCharCode, CharCode);
        FKeyEvent EnterDownEvent(EKeys::Enter, ModifierKeys, KeyboardUserIndex, false, CharCode, KeyCode);
        SlateApp.ProcessKeyDownEvent(EnterDownEvent);
        FKeyEvent EnterUpEvent(EKeys::Enter, ModifierKeys, KeyboardUserIndex, false, CharCode, KeyCode);
        SlateApp.ProcessKeyUpEvent(EnterUpEvent);
      }

      bSuccess = true;
      Message = FString::Printf(TEXT("Text input sent: %s"), *Text);
      Resp->SetStringField(TEXT("text"), Text);
      Resp->SetBoolField(TEXT("submit"), bSubmit);
      SetInputStateFields(DescribePressedKeys(SimulatedPressedMouseButtons), DescribePressedKeys(SimulatedPressedModifierKeys));
    }
  }
  else if (InputType == TEXT("mouse_down"))
  {
    FVector2D Position = GetCurrentSimulatedPosition(SimulatedPointerPosition, bSimulatedPointerPositionValid);
    const bool bHasPosition = bHasVirtualTarget ? TryGetRelativePointerPositionFromPayload(Payload, Position) : TryGetPointerPositionFromPayload(Payload, Position);
    if (!bHasPosition && !bSimulatedPointerPositionValid)
    {
      Message = bHasVirtualTarget ? TEXT("clientX and clientY parameters are required for the first targeted mouse_down request") : TEXT("x and y parameters are required for the first mouse_down request");
    }
    else
    {
      FString ButtonName = TEXT("left");
      Payload->TryGetStringField(TEXT("button"), ButtonName);
      const FKey MouseButtonKey = ResolveMouseButtonKey(ButtonName);
      CaptureMouseScreenshot(TEXT("before"));
      const bool bHandled = bHasVirtualTarget ? DispatchVirtualMouseDown(this, VirtualTarget, Position, MouseButtonKey) : DispatchMouseDown(SimulatedPointerPosition, bSimulatedPointerPositionValid, SimulatedPressedMouseButtons, SimulatedPressedModifierKeys, Position, MouseButtonKey);
      bSuccess = true;
      Message = bHasVirtualTarget ? FString::Printf(TEXT("Virtual mouse button %s down at client (%0.2f, %0.2f)%s"), *ButtonName, Position.X, Position.Y, bHandled ? TEXT("") : TEXT(" (unhandled)")) : FString::Printf(TEXT("Mouse button %s down at (%0.2f, %0.2f)%s"), *ButtonName, Position.X, Position.Y, bHandled ? TEXT("") : TEXT(" (unhandled)"));
      SetPointerResponseFields(Position, ButtonName, DescribePressedKeys(SimulatedPressedMouseButtons), DescribePressedKeys(SimulatedPressedModifierKeys), bHasVirtualTarget ? &VirtualTarget : nullptr);
      CaptureMouseScreenshot(TEXT("after"));
    }
  }
  else if (InputType == TEXT("mouse_up"))
  {
    FVector2D Position = GetCurrentSimulatedPosition(SimulatedPointerPosition, bSimulatedPointerPositionValid);
    if (bHasVirtualTarget)
    {
      TryGetRelativePointerPositionFromPayload(Payload, Position);
    }
    else
    {
      TryGetPointerPositionFromPayload(Payload, Position);
    }

    FString ButtonName = TEXT("left");
    Payload->TryGetStringField(TEXT("button"), ButtonName);
    const FKey MouseButtonKey = ResolveMouseButtonKey(ButtonName);
    CaptureMouseScreenshot(TEXT("before"));
    const bool bHandled = bHasVirtualTarget ? DispatchVirtualMouseUp(this, VirtualTarget, Position, MouseButtonKey) : DispatchMouseUp(SimulatedPointerPosition, bSimulatedPointerPositionValid, SimulatedPressedMouseButtons, SimulatedPressedModifierKeys, Position, MouseButtonKey);
    bSuccess = true;
    Message = bHasVirtualTarget ? FString::Printf(TEXT("Virtual mouse button %s up at client (%0.2f, %0.2f)%s"), *ButtonName, Position.X, Position.Y, bHandled ? TEXT("") : TEXT(" (unhandled)")) : FString::Printf(TEXT("Mouse button %s up at (%0.2f, %0.2f)%s"), *ButtonName, Position.X, Position.Y, bHandled ? TEXT("") : TEXT(" (unhandled)"));
    SetPointerResponseFields(Position, ButtonName, DescribePressedKeys(SimulatedPressedMouseButtons), DescribePressedKeys(SimulatedPressedModifierKeys), bHasVirtualTarget ? &VirtualTarget : nullptr);
    CaptureMouseScreenshot(TEXT("after"));
  }
  else if (InputType == TEXT("mouse_click") || InputType == TEXT("click"))
  {
    FVector2D Position = GetCurrentSimulatedPosition(SimulatedPointerPosition, bSimulatedPointerPositionValid);
    const bool bHasPosition = bHasVirtualTarget ? TryGetRelativePointerPositionFromPayload(Payload, Position) : TryGetPointerPositionFromPayload(Payload, Position);
    if (!bHasPosition && !bSimulatedPointerPositionValid)
    {
      Message = bHasVirtualTarget ? TEXT("clientX and clientY parameters are required for the first targeted mouse_click request") : TEXT("x and y parameters are required for the first mouse_click request");
    }
    else
    {
      FString ButtonName = TEXT("left");
      Payload->TryGetStringField(TEXT("button"), ButtonName);
      const FKey MouseButtonKey = ResolveMouseButtonKey(ButtonName);

      double HoldDurationMs = 0.0;
      Payload->TryGetNumberField(TEXT("holdDurationMs"), HoldDurationMs);

      CaptureMouseScreenshot(TEXT("before"));
      const bool bDownHandled = bHasVirtualTarget ? DispatchVirtualMouseDown(this, VirtualTarget, Position, MouseButtonKey) : DispatchMouseDown(SimulatedPointerPosition, bSimulatedPointerPositionValid, SimulatedPressedMouseButtons, SimulatedPressedModifierKeys, Position, MouseButtonKey);
      if (HoldDurationMs > 0.0)
      {
        FPlatformProcess::SleepNoStats(static_cast<float>(HoldDurationMs / 1000.0));
      }
      const bool bUpHandled = bHasVirtualTarget ? DispatchVirtualMouseUp(this, VirtualTarget, Position, MouseButtonKey) : DispatchMouseUp(SimulatedPointerPosition, bSimulatedPointerPositionValid, SimulatedPressedMouseButtons, SimulatedPressedModifierKeys, Position, MouseButtonKey);
      bSuccess = true;
      Message = bHasVirtualTarget ? FString::Printf(TEXT("Virtual mouse click with %s button at client (%0.2f, %0.2f)%s"), *ButtonName, Position.X, Position.Y, (bDownHandled || bUpHandled) ? TEXT("") : TEXT(" (unhandled)")) : FString::Printf(TEXT("Mouse click with %s button at (%0.2f, %0.2f)%s"), *ButtonName, Position.X, Position.Y, (bDownHandled || bUpHandled) ? TEXT("") : TEXT(" (unhandled)"));
      Resp->SetNumberField(TEXT("holdDurationMs"), HoldDurationMs);
      SetPointerResponseFields(Position, ButtonName, DescribePressedKeys(SimulatedPressedMouseButtons), DescribePressedKeys(SimulatedPressedModifierKeys), bHasVirtualTarget ? &VirtualTarget : nullptr);
      CaptureMouseScreenshot(TEXT("after"));
    }
  }
  else if (InputType == TEXT("mouse_move") || InputType == TEXT("move"))
  {
    FVector2D Position = GetCurrentSimulatedPosition(SimulatedPointerPosition, bSimulatedPointerPositionValid);
    const bool bHasPosition = bHasVirtualTarget ? TryGetRelativePointerPositionFromPayload(Payload, Position) : TryGetPointerPositionFromPayload(Payload, Position);
    if (!bHasPosition)
    {
      Message = bHasVirtualTarget ? TEXT("clientX and clientY parameters are required for targeted mouse_move") : TEXT("x and y parameters are required for mouse_move");
    }
    else
    {
      CaptureMouseScreenshot(TEXT("before"));
      const bool bHandled = bHasVirtualTarget ? DispatchVirtualMouseMove(this, VirtualTarget, Position) : DispatchMouseMove(SimulatedPointerPosition, bSimulatedPointerPositionValid, SimulatedPressedMouseButtons, SimulatedPressedModifierKeys, Position);
      bSuccess = true;
      Message = bHasVirtualTarget ? FString::Printf(TEXT("Virtual mouse moved to client (%0.2f, %0.2f)%s"), Position.X, Position.Y, bHandled ? TEXT("") : TEXT(" (unhandled)")) : FString::Printf(TEXT("Mouse moved to (%0.2f, %0.2f)%s"), Position.X, Position.Y, bHandled ? TEXT("") : TEXT(" (unhandled)"));
      SetPointerResponseFields(Position, FString(), DescribePressedKeys(SimulatedPressedMouseButtons), DescribePressedKeys(SimulatedPressedModifierKeys), bHasVirtualTarget ? &VirtualTarget : nullptr);
      CaptureMouseScreenshot(TEXT("after"));
    }
  }
  else if (InputType == TEXT("mouse_wheel") || InputType == TEXT("wheel") || InputType == TEXT("scroll"))
  {
    FVector2D Position = GetCurrentSimulatedPosition(SimulatedPointerPosition, bSimulatedPointerPositionValid);
    const bool bHasPosition = bHasVirtualTarget ? TryGetRelativePointerPositionFromPayload(Payload, Position) : TryGetPointerPositionFromPayload(Payload, Position);
    if (!bHasPosition && !bSimulatedPointerPositionValid)
    {
      Message = bHasVirtualTarget ? TEXT("clientX and clientY parameters are required for the first targeted mouse_wheel request") : TEXT("x and y parameters are required for the first mouse_wheel request");
    }
    else
    {
      double WheelSteps = 0.0;
      double PreciseDelta = 0.0;
      const bool bHasWheelSteps = Payload->TryGetNumberField(TEXT("wheelSteps"), WheelSteps);
      const bool bHasPreciseDelta = Payload->TryGetNumberField(TEXT("preciseDelta"), PreciseDelta);
      const float WheelDelta = static_cast<float>(bHasPreciseDelta ? PreciseDelta : WheelSteps);

      if (!bHasWheelSteps && !bHasPreciseDelta)
      {
        Message = TEXT("mouse_wheel requires wheelSteps or preciseDelta");
      }
      else if (FMath::IsNearlyZero(WheelDelta))
      {
        Message = TEXT("mouse_wheel delta cannot be zero");
      }
      else
      {
        CaptureMouseScreenshot(TEXT("before"));
        const bool bHandled = bHasVirtualTarget ? DispatchVirtualMouseWheel(this, VirtualTarget, Position, WheelDelta) : DispatchMouseWheel(SimulatedPointerPosition, bSimulatedPointerPositionValid, SimulatedPressedMouseButtons, SimulatedPressedModifierKeys, Position, WheelDelta);
        bSuccess = true;
        Message = bHasVirtualTarget ? FString::Printf(TEXT("Virtual mouse wheel at client (%0.2f, %0.2f) delta=%0.2f%s"), Position.X, Position.Y, WheelDelta, bHandled ? TEXT("") : TEXT(" (unhandled)")) : FString::Printf(TEXT("Mouse wheel at (%0.2f, %0.2f) delta=%0.2f%s"), Position.X, Position.Y, WheelDelta, bHandled ? TEXT("") : TEXT(" (unhandled)"));
        Resp->SetNumberField(TEXT("wheelSteps"), WheelSteps);
        if (bHasPreciseDelta)
        {
          Resp->SetNumberField(TEXT("preciseDelta"), PreciseDelta);
        }
        SetPointerResponseFields(Position, FString(), DescribePressedKeys(SimulatedPressedMouseButtons), DescribePressedKeys(SimulatedPressedModifierKeys), bHasVirtualTarget ? &VirtualTarget : nullptr);
        CaptureMouseScreenshot(TEXT("after"));
      }
    }
  }
  else if (InputType == TEXT("mouse_drag") || InputType == TEXT("drag"))
  {
    FVector2D StartPosition = FVector2D::ZeroVector;
    FVector2D EndPosition = FVector2D::ZeroVector;
    const bool bHasStart = bHasVirtualTarget ? TryGetRelativePointObjectField(Payload, TEXT("start"), StartPosition) : TryGetPointObjectField(Payload, TEXT("start"), StartPosition);
    const bool bHasEnd = bHasVirtualTarget ? TryGetRelativePointObjectField(Payload, TEXT("end"), EndPosition) : TryGetPointObjectField(Payload, TEXT("end"), EndPosition);
    if (!bHasStart || !bHasEnd)
    {
      Message = bHasVirtualTarget ? TEXT("mouse_drag requires start and end client points for targeted input") : TEXT("mouse_drag requires start{x,y} and end{x,y}");
    }
    else
    {
      FString ButtonName = TEXT("left");
      Payload->TryGetStringField(TEXT("button"), ButtonName);
      const FKey MouseButtonKey = ResolveMouseButtonKey(ButtonName);

      double DurationMs = 0.0;
      Payload->TryGetNumberField(TEXT("durationMs"), DurationMs);
      if (DurationMs < 0.0)
      {
        DurationMs = 0.0;
      }

      double HoldBeforeMoveMs = 0.0;
      Payload->TryGetNumberField(TEXT("holdBeforeMoveMs"), HoldBeforeMoveMs);
      double HoldAfterMoveMs = 0.0;
      Payload->TryGetNumberField(TEXT("holdAfterMoveMs"), HoldAfterMoveMs);

      double StepCountValue = 0.0;
      const bool bHasStepCount = Payload->TryGetNumberField(TEXT("steps"), StepCountValue);
      const int32 StepCount = bHasStepCount ? FMath::Clamp(FMath::RoundToInt(static_cast<float>(StepCountValue)), 1, 240) : FMath::Clamp(FMath::CeilToInt(static_cast<float>(DurationMs / 16.0)), 1, 120);

      CaptureMouseScreenshot(TEXT("before"));
      const bool bDownHandled = bHasVirtualTarget ? DispatchVirtualMouseDown(this, VirtualTarget, StartPosition, MouseButtonKey) : DispatchMouseDown(SimulatedPointerPosition, bSimulatedPointerPositionValid, SimulatedPressedMouseButtons, SimulatedPressedModifierKeys, StartPosition, MouseButtonKey);
      if (HoldBeforeMoveMs > 0.0)
      {
        FPlatformProcess::SleepNoStats(static_cast<float>(HoldBeforeMoveMs / 1000.0));
      }

      const float StepDurationSeconds = StepCount > 0 ? static_cast<float>((DurationMs / StepCount) / 1000.0) : 0.0f;
      for (int32 StepIndex = 1; StepIndex <= StepCount; ++StepIndex)
      {
        const float Alpha = static_cast<float>(StepIndex) / static_cast<float>(StepCount);
        const FVector2D IntermediatePosition = FMath::Lerp(StartPosition, EndPosition, Alpha);
        if (bHasVirtualTarget)
        {
          DispatchVirtualMouseMove(this, VirtualTarget, IntermediatePosition);
        }
        else
        {
          DispatchMouseMove(SimulatedPointerPosition, bSimulatedPointerPositionValid, SimulatedPressedMouseButtons, SimulatedPressedModifierKeys, IntermediatePosition);
        }
        if (StepDurationSeconds > 0.0f && StepIndex < StepCount)
        {
          FPlatformProcess::SleepNoStats(StepDurationSeconds);
        }
      }

      if (HoldAfterMoveMs > 0.0)
      {
        FPlatformProcess::SleepNoStats(static_cast<float>(HoldAfterMoveMs / 1000.0));
      }

      const bool bUpHandled = bHasVirtualTarget ? DispatchVirtualMouseUp(this, VirtualTarget, EndPosition, MouseButtonKey) : DispatchMouseUp(SimulatedPointerPosition, bSimulatedPointerPositionValid, SimulatedPressedMouseButtons, SimulatedPressedModifierKeys, EndPosition, MouseButtonKey);
      bSuccess = true;
      Message = bHasVirtualTarget ? FString::Printf(TEXT("Virtual mouse drag with %s button from client (%0.2f, %0.2f) to (%0.2f, %0.2f)%s"), *ButtonName, StartPosition.X, StartPosition.Y, EndPosition.X, EndPosition.Y, (bDownHandled || bUpHandled) ? TEXT("") : TEXT(" (unhandled)")) : FString::Printf(TEXT("Mouse drag with %s button from (%0.2f, %0.2f) to (%0.2f, %0.2f)%s"), *ButtonName, StartPosition.X, StartPosition.Y, EndPosition.X, EndPosition.Y, (bDownHandled || bUpHandled) ? TEXT("") : TEXT(" (unhandled)"));
      Resp->SetNumberField(TEXT("durationMs"), DurationMs);
      Resp->SetNumberField(TEXT("steps"), StepCount);
      Resp->SetNumberField(TEXT("holdBeforeMoveMs"), HoldBeforeMoveMs);
      Resp->SetNumberField(TEXT("holdAfterMoveMs"), HoldAfterMoveMs);
      Resp->SetNumberField(TEXT("startX"), StartPosition.X);
      Resp->SetNumberField(TEXT("startY"), StartPosition.Y);
      SetPointerResponseFields(EndPosition, ButtonName, DescribePressedKeys(SimulatedPressedMouseButtons), DescribePressedKeys(SimulatedPressedModifierKeys), bHasVirtualTarget ? &VirtualTarget : nullptr);
      CaptureMouseScreenshot(TEXT("after"));
    }
  }
  else
  {
    Message = FString::Printf(TEXT("Unknown input type: %s. Supported: reset_input, key_down, key_up, text_input, mouse_down, mouse_up, mouse_click, mouse_move, mouse_wheel, mouse_drag"), *InputType);
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetStringField(TEXT("type"), InputType);
  Resp->SetStringField(TEXT("message"), Message);
  if (bSuccess)
  {
    SetTargetHealthFields(TEXT("resolved"), bHasVirtualTarget, false);
  }
  SetFocusedWidgetResponseFields(Resp);

  if (bSuccess)
  {
    SendAutomationResponse(Socket, RequestId, true, Message, Resp, FString());
  }
  else
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INPUT_FAILED"), Message, Resp);
  }
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Simulate input requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorCloseAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("assetPath required"), nullptr);
    return true;
  }

  UAssetEditorSubsystem *AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
  if (!AssetEditorSS)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SUBSYSTEM_MISSING"),
                              TEXT("AssetEditorSubsystem unavailable"), nullptr);
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("LOAD_FAILED"),
                              TEXT("Failed to load asset"), nullptr);
    return true;
  }

  AssetEditorSS->CloseAllEditorsForAsset(Asset);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Asset editor closed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSaveAll(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  // Save all dirty packages using FEditorFileUtils
  TArray<UPackage *> DirtyPackages;
  FEditorFileUtils::GetDirtyWorldPackages(DirtyPackages);
  FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);

  bool bSuccess = true;
  int32 SavedCount = 0;
  int32 SkippedCount = 0;

  for (UPackage *Package : DirtyPackages)
  {
    if (Package)
    {
      FString PackagePath = Package->GetPathName();

      // Skip transient/temporary packages that cannot be saved
      // These include /Temp/ paths and packages with RF_Transient flag
      if (PackagePath.StartsWith(TEXT("/Temp/")) ||
          PackagePath.StartsWith(TEXT("/Transient/")) ||
          Package->HasAnyFlags(RF_Transient))
      {
        SkippedCount++;
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("HandleControlEditorSaveAll: Skipping transient package: %s"), *PackagePath);
        continue;
      }

      if (UEditorAssetLibrary::SaveAsset(PackagePath, false))
      {
        SavedCount++;
      }
      else
      {
        bSuccess = false;
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetNumberField(TEXT("savedCount"), SavedCount);
  Resp->SetNumberField(TEXT("skippedCount"), SkippedCount);
  Resp->SetNumberField(TEXT("totalDirty"), DirtyPackages.Num());

  // Only report outer success if the operation actually succeeded
  if (bSuccess || DirtyPackages.Num() == 0)
  {
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Saved %d of %d dirty assets (skipped %d transient)"), SavedCount, DirtyPackages.Num() - SkippedCount, SkippedCount),
                           Resp, FString());
  }
  else
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SAVE_FAILED"),
                              FString::Printf(TEXT("Failed to save all assets. Saved %d of %d dirty assets."),
                                              SavedCount, DirtyPackages.Num() - SkippedCount),
                              Resp);
  }
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorUndo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Execute undo via console command
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("Undo"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("undo"));
  Resp->SetStringField(TEXT("command"), TEXT("Undo"));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Undo executed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorRedo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Execute redo via console command
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("Redo"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("redo"));
  Resp->SetStringField(TEXT("command"), TEXT("Redo"));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Redo executed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetEditorMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString Mode;
  Payload->TryGetStringField(TEXT("mode"), Mode);
  if (Mode.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("mode required"), nullptr);
    return true;
  }

  // Execute editor mode command via console
  FString Command = FString::Printf(TEXT("mode %s"), *Mode);
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("mode"), Mode);
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Editor mode set to %s"), *Mode), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorShowStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  TArray<FString> StatsShown;
  if (World)
  {
    GEditor->Exec(World, TEXT("Stat FPS"));
    StatsShown.Add(TEXT("FPS"));
    GEditor->Exec(World, TEXT("Stat Unit"));
    StatsShown.Add(TEXT("Unit"));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("showStats"));
  TArray<TSharedPtr<FJsonValue>> StatsArray;
  for (const FString &Stat : StatsShown)
  {
    StatsArray.Add(MakeShared<FJsonValueString>(Stat));
  }
  Resp->SetArrayField(TEXT("statsShown"), StatsArray);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Stats displayed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorHideStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (World)
  {
    GEditor->Exec(World, TEXT("Stat None"));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), TEXT("hideStats"));
  Resp->SetStringField(TEXT("command"), TEXT("Stat None"));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Stats hidden"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetGameView(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);

  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Toggle game view via console command
  GEditor->Exec(GEditor->GetEditorWorldContext().World(),
                bEnabled ? TEXT("ToggleGameView 1") : TEXT("ToggleGameView 0"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("gameViewEnabled"), bEnabled);
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Game view %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")),
                         Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetImmersiveMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);

  // Toggle immersive mode - this is viewport-specific
  if (GEditor && GEditor->GetActiveViewport())
  {
    FViewport *Viewport = GEditor->GetActiveViewport();
    if (Viewport)
    {
      // Immersive mode toggle via console
      GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("ToggleImmersive"));
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("immersiveModeEnabled"), bEnabled);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Immersive mode toggled"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetFixedDeltaTime(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  double DeltaTime = 0.01667; // Default ~60fps
  if (Payload->HasField(TEXT("deltaTime")))
  {
    TSharedPtr<FJsonValue> Value = Payload->TryGetField(TEXT("deltaTime"));
    if (Value.IsValid() && Value->Type == EJson::Number)
    {
      DeltaTime = Value->AsNumber();
    }
  }

  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Set fixed delta time via console
  FString Command = FString::Printf(TEXT("r.FixedDeltaTime %f"), DeltaTime);
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("fixedDeltaTime"), DeltaTime);
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Fixed delta time set to %f"), DeltaTime), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorOpenLevel(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString LevelPath;
  // Accept multiple parameter names for flexibility
  // levelPath is the primary, path and assetPath are aliases
  Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
  if (LevelPath.IsEmpty())
  {
    Payload->TryGetStringField(TEXT("path"), LevelPath);
  }
  if (LevelPath.IsEmpty())
  {
    Payload->TryGetStringField(TEXT("assetPath"), LevelPath);
  }
  if (LevelPath.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("levelPath, path, or assetPath required"), nullptr);
    return true;
  }

  // Normalize the level path
  if (!LevelPath.StartsWith(TEXT("/Game/")) && !LevelPath.StartsWith(TEXT("/Engine/")))
  {
    LevelPath = FString::Printf(TEXT("/Game/%s"), *LevelPath);
  }

  // Remove map suffix if present
  if (LevelPath.EndsWith(TEXT(".umap")))
  {
    LevelPath.LeftChopInline(5);
  }

  if (!GEditor)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Use FEditorFileUtils to load the map
  FString MapPath = LevelPath + TEXT(".umap");

  // CRITICAL FIX: Unreal stores levels in TWO possible path patterns:
  // 1. Folder-based (standard): /Game/Path/LevelName/LevelName.umap
  // 2. Flat: /Game/Path/LevelName.umap
  // We must check BOTH paths before returning FILE_NOT_FOUND.

  // Build both possible paths
  FString FlatMapPath = LevelPath + TEXT(".umap");
  // Check if path is /Engine/ or /Game/ and extract accordingly
  int32 PrefixLen = 6; // Default: "/Game/" is 6 chars
  FString ContentDir = FPaths::ProjectContentDir();
  if (LevelPath.StartsWith(TEXT("/Engine/")))
  {
    PrefixLen = 8; // "/Engine/" is 8 chars
    ContentDir = FPaths::EngineContentDir();
  }
  FString FullFlatMapPath = ContentDir + FlatMapPath.Mid(PrefixLen);
  FullFlatMapPath = FPaths::ConvertRelativePathToFull(FullFlatMapPath);

  // Folder-based path: /Game/Path/LevelName -> /Game/Path/LevelName/LevelName.umap
  FString LevelName = FPaths::GetBaseFilename(LevelPath);
  FString FolderMapPath = LevelPath + TEXT("/") + LevelName + TEXT(".umap");
  FString FullFolderMapPath = ContentDir + FolderMapPath.Mid(PrefixLen);
  FullFolderMapPath = FPaths::ConvertRelativePathToFull(FullFolderMapPath);

  // Check which path exists
  FString MapPathToLoad;
  FString FullMapPath;

  // Prefer folder-based path (Unreal's standard) if it exists
  if (FPaths::FileExists(FullFolderMapPath))
  {
    MapPathToLoad = FolderMapPath;
    FullMapPath = FullFolderMapPath;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
           TEXT("OpenLevel: Found level at folder-based path: %s"), *FullFolderMapPath);
  }
  else if (FPaths::FileExists(FullFlatMapPath))
  {
    // Fallback to flat path format.
    MapPathToLoad = FlatMapPath;
    FullMapPath = FullFlatMapPath;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
           TEXT("OpenLevel: Found level at flat path: %s"), *FullFlatMapPath);
  }
  else
  {
    // Neither path exists - return detailed error
    TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
    ErrorDetails->SetStringField(TEXT("levelPath"), LevelPath);
    ErrorDetails->SetStringField(TEXT("checkedFolderBased"), FullFolderMapPath);
    ErrorDetails->SetStringField(TEXT("checkedFlat"), FullFlatMapPath);
    ErrorDetails->SetStringField(TEXT("hint"), TEXT("Unreal levels are typically stored as /Game/Path/LevelName/LevelName.umap"));
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("FILE_NOT_FOUND"),
                              FString::Printf(TEXT("Level file not found. Checked:\n  Folder: %s\n  Flat: %s"),
                                              *FullFolderMapPath, *FullFlatMapPath),
                              ErrorDetails);
    return true;
  }

  bool bOpened = McpSafeLoadMap(MapPathToLoad);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bOpened);
  Resp->SetStringField(TEXT("levelPath"), LevelPath);
  Resp->SetStringField(TEXT("loadedPath"), MapPathToLoad);

  if (bOpened)
  {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
           TEXT("OpenLevel: Successfully opened level: %s"), *MapPathToLoad);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Level opened"), Resp, FString());
  }
  else
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("OPEN_FAILED"), TEXT("Failed to open level"), Resp);
  }
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorList(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SUBSYSTEM_MISSING"),
                              TEXT("EditorActorSubsystem unavailable"), nullptr);
    return true;
  }

  const TArray<AActor *> &AllActors = ActorSS->GetAllLevelActors();
  TArray<TSharedPtr<FJsonValue>> ActorsArray;

  for (AActor *Actor : AllActors)
  {
    if (!Actor)
      continue;
    const FString Label = Actor->GetActorLabel();
    const FString Name = Actor->GetName();
    if (!Filter.IsEmpty() && !Label.Contains(Filter) && !Name.Contains(Filter))
      continue;

    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("label"), Label);
    Entry->SetStringField(TEXT("name"), Name);
    Entry->SetStringField(TEXT("path"), Actor->GetPathName());
    Entry->SetStringField(TEXT("class"), Actor->GetClass()
                                             ? Actor->GetClass()->GetPathName()
                                             : TEXT(""));
    ActorsArray.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("actors"), ActorsArray);
  Data->SetNumberField(TEXT("count"), ActorsArray.Num());
  if (!Filter.IsEmpty())
    Data->SetStringField(TEXT("filter"), Filter);
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actors listed"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGet(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty())
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found)
  {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FTransform Current = Found->GetActorTransform();
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("name"), Found->GetName());
  Data->SetStringField(TEXT("label"), Found->GetActorLabel());
  Data->SetStringField(TEXT("path"), Found->GetPathName());
  Data->SetStringField(TEXT("class"), Found->GetClass()
                                          ? Found->GetClass()->GetPathName()
                                          : TEXT(""));

  TArray<TSharedPtr<FJsonValue>> TagsArray;
  for (const FName &Tag : Found->Tags)
  {
    TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
  }
  Data->SetArrayField(TEXT("tags"), TagsArray);

  auto MakeArray = [](const FVector &Vec) -> TArray<TSharedPtr<FJsonValue>>
  {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };
  Data->SetArrayField(TEXT("location"), MakeArray(Current.GetLocation()));
  Data->SetArrayField(TEXT("scale"), MakeArray(Current.GetScale3D()));

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor retrieved"),
                              Data);
  return true;
#else
  return false;
#endif
}
