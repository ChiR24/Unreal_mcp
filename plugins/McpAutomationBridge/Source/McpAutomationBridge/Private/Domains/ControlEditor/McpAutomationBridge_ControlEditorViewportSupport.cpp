#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshotSupport.h"
#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

#include "Widgets/Docking/SDockTab.h"
#include "LevelEditorViewport.h"

FEditorViewportClient *GetEjectedPieViewportClientForMcp() {
  // Ejected from its pawn, the player is Simulating in Editor: the level viewport that showed the editor world now draws
  // the play world from a free camera (FLevelEditorViewportClient::GetWorld answers the play world for it). While the
  // player still possesses a pawn the game viewport draws, and every editor viewport client is hidden behind it.
  if (!GEditor || !GEditor->PlayWorld || !GEditor->bIsSimulatingInEditor) {
    return nullptr;
  }
  for (FLevelEditorViewportClient *Client : GEditor->GetLevelViewportClients()) {
    if (Client && Client->IsSimulateInEditorViewport()) {
      return Client;
    }
  }
  return nullptr;
}

bool RefuseCameraMoveWhilePieFollowsPawnForMcp(
    UMcpAutomationBridgeSubsystem *Bridge, TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FString &RequestId, const TCHAR *What) {
  if (!GEditor || !GEditor->PlayWorld || GetEjectedPieViewportClientForMcp()) {
    return false;
  }
  // Moving the level viewport here would answer success for a camera nobody is looking through.
  TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
  Details->SetBoolField(TEXT("pieRunning"), true);
  Details->SetBoolField(TEXT("ejected"), false);
  if (const APlayerController *PlayerController = GEditor->PlayWorld->GetFirstPlayerController()) {
    if (const AActor *ViewTarget = PlayerController->GetViewTarget()) {
      Details->SetStringField(TEXT("viewTarget"), ViewTarget->GetPathName());
    }
  }
  SendStandardErrorResponse(
      Bridge, Socket, RequestId, TEXT("PIE_VIEW_NOT_EJECTED"),
      FString::Printf(
          TEXT("%s: Play In Editor is running and the game draws through its view target's camera (the pawn the player "
               "possesses, or an actor set with cameraOp=view_target), so there is no free camera to move. Eject first "
               "(control_editor play control=eject) and the view becomes a free camera, or point the game camera at an "
               "actor (control_editor set_camera cameraOp=view_target with actorName)."),
          What),
      Details);
  return true;
}

FEditorViewportClient *GetActiveEditorViewportClientForMcp() {
  // The ejected play view is the viewport on screen while Play In Editor runs; every other client is hidden behind it.
  if (FEditorViewportClient *Ejected = GetEjectedPieViewportClientForMcp()) {
    return Ejected;
  }

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

