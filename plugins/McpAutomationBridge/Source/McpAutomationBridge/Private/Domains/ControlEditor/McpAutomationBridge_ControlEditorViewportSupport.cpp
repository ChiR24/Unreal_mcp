#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshotSupport.h"
#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

#if WITH_EDITOR
#if __has_include("LevelEditorViewport.h")
#include "LevelEditorViewport.h"
#define MCP_HAS_LEVEL_EDITING_VIEWPORT_CLIENT 1
#endif
#endif

#if WITH_EDITOR
FEditorViewportClient *GetActiveEditorViewportClientForMcp() {
  // Resolve the LEVEL viewport deterministically. GetFirstActiveViewport() and
  // GEditor->GetActiveViewport() both follow input focus, so opening any asset
  // editor (a Widget Blueprint designer, a material graph) silently retargets
  // them. set_camera then moved one client while screenshot photographed
  // another, and set_camera still answered locationApplied:true because it had
  // verified the client it moved. Prefer the same client the level editor and
  // UUnrealEditorSubsystem treat as current, so "move the camera, then take a
  // picture" is guaranteed to address one viewport.
  // A client that is not on screen still accepts SetViewLocation, and the
  // four-viewport layout keeps every client alive while only one is shown, so
  // the first perspective client found could be one nobody can see. set_camera
  // then answered locationApplied:true for a camera the screenshot never
  // renders from. Require a VISIBLE client, and only fall back to a hidden one
  // when there is nothing else to address.
#if MCP_HAS_LEVEL_EDITING_VIEWPORT_CLIENT
  if (GCurrentLevelEditingViewportClient &&
      GCurrentLevelEditingViewportClient->IsPerspective() &&
      GCurrentLevelEditingViewportClient->IsVisible()) {
    return GCurrentLevelEditingViewportClient;
  }
#endif

  FEditorViewportClient *HiddenPerspective = nullptr;
  if (GEditor) {
    for (FEditorViewportClient *Client : GEditor->GetAllViewportClients()) {
      if (!Client || !Client->IsPerspective() || !Client->IsLevelEditorClient()) {
        continue;
      }
      if (Client->IsVisible()) {
        return Client;
      }
      if (!HiddenPerspective) {
        HiddenPerspective = Client;
      }
    }
  }

#if MCP_HAS_LEVEL_EDITOR_MODULE
  if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
    if (FLevelEditorModule *LevelEditorModule =
            FModuleManager::GetModulePtr<FLevelEditorModule>(
                TEXT("LevelEditor"))) {
      TSharedPtr<IAssetViewport> ActiveViewport =
          LevelEditorModule->GetFirstActiveViewport();
      if (ActiveViewport.IsValid()) {
        return &ActiveViewport->GetAssetViewportClient();
      }
    }
  }
#endif

  if (GEditor && GEditor->GetActiveViewport()) {
    return static_cast<FEditorViewportClient *>(
        GEditor->GetActiveViewport()->GetClient());
  }
  return HiddenPerspective;
}

TSharedPtr<SWindow> GetAnyVisibleEditorWindowForMcp() {
  // The root-window lookup can come back unusable while PIE owns input, which
  // made full_editor_window answer EDITOR_WINDOW_NOT_AVAILABLE during play -
  // exactly when a caller wants to see the Slate/UMG layer that the game
  // viewport's own pixel read does not contain. Fall back to the enumeration
  // the `window` selector already uses.
  TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
  if (RootWindow.IsValid() && RootWindow->IsVisible() &&
      !RootWindow->IsWindowMinimized()) {
    return RootWindow;
  }

  // Windows[0] used to win by accident of enumeration order, so a notification
  // toast on screen became "the full editor window" - a 352x157 capture the
  // caller had no reason to doubt. Largest-on-screen is the closest honest
  // stand-in for the editor when the main frame cannot be used.
  TArray<TSharedRef<SWindow>> Windows;
  EnumerateEditorSlateWindowsForMcp(Windows);
  TSharedPtr<SWindow> Largest;
  double LargestArea = -1.0;
  for (const TSharedRef<SWindow> &Window : Windows) {
    const FVector2D Size = Window->GetSizeInScreen();
    const double Area = static_cast<double>(Size.X) * static_cast<double>(Size.Y);
    if (Area > LargestArea) {
      LargestArea = Area;
      Largest = Window;
    }
  }
  return Largest;
}

TSharedPtr<FJsonObject> MakeVectorObjectForMcp(const FVector &Vector) {
  TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
  Obj->SetNumberField(TEXT("x"), Vector.X);
  Obj->SetNumberField(TEXT("y"), Vector.Y);
  Obj->SetNumberField(TEXT("z"), Vector.Z);
  return Obj;
}

TSharedPtr<FJsonObject> MakeRotatorObjectForMcp(const FRotator &Rotator) {
  TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
  Obj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
  Obj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
  Obj->SetNumberField(TEXT("roll"), Rotator.Roll);
  return Obj;
}
#endif
