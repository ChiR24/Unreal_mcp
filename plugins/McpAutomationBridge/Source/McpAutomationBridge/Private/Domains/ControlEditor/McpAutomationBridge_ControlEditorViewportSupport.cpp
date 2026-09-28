#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshotSupport.h"
#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

#include "Widgets/Docking/SDockTab.h"
#include "LevelEditorViewport.h"

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
  if (GCurrentLevelEditingViewportClient &&
      GCurrentLevelEditingViewportClient->IsPerspective() &&
      GCurrentLevelEditingViewportClient->IsVisible()) {
    return GCurrentLevelEditingViewportClient;
  }

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

  if (GEditor && GEditor->GetActiveViewport()) {
    return static_cast<FEditorViewportClient *>(
        GEditor->GetActiveViewport()->GetClient());
  }
  return HiddenPerspective;
}

bool BringLevelEditorTabToFrontForMcp() {
  // Opening Fab (or docking an asset editor beside the level editor) puts a
  // major tab over every level viewport. A covered viewport is never painted,
  // so a capture of it read back solid black while answering success.
  // ActivateInParent only switches the tab well; it does not raise or focus
  // the window, so the user's foreground app is left alone.
  const TSharedPtr<SDockTab> LevelEditorTab =
      FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId(FName(TEXT("LevelEditor"))));
  if (!LevelEditorTab.IsValid() || LevelEditorTab->IsForeground()) {
    return false;
  }
  LevelEditorTab->ActivateInParent(ETabActivationCause::SetDirectly);
  return true;
}

