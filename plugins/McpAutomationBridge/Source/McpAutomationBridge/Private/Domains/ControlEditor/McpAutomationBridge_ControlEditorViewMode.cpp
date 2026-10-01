#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "ShowFlags.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetViewMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString Mode;
  Payload->TryGetStringField(TEXT("viewMode"), Mode);
  FString LowerMode = Mode.ToLower();
  FString Chosen;
  EViewModeIndex ViewModeIndex = VMI_Lit;
  bool bHasNativeViewMode = true;
  if (LowerMode == TEXT("lit"))
  {
    Chosen = TEXT("Lit");
    ViewModeIndex = VMI_Lit;
  }
  else if (LowerMode == TEXT("unlit"))
  {
    Chosen = TEXT("Unlit");
    ViewModeIndex = VMI_Unlit;
  }
  else if (LowerMode == TEXT("wireframe"))
  {
    Chosen = TEXT("Wireframe");
    ViewModeIndex = VMI_Wireframe;
  }
  else if (LowerMode == TEXT("detaillighting"))
  {
    Chosen = TEXT("DetailLighting");
    ViewModeIndex = VMI_Lit_DetailLighting;
  }
  else if (LowerMode == TEXT("lightingonly"))
  {
    Chosen = TEXT("LightingOnly");
    ViewModeIndex = VMI_LightingOnly;
  }
  else if (LowerMode == TEXT("lightcomplexity"))
  {
    Chosen = TEXT("LightComplexity");
    ViewModeIndex = VMI_LightComplexity;
  }
  else if (LowerMode == TEXT("shadercomplexity"))
  {
    Chosen = TEXT("ShaderComplexity");
    ViewModeIndex = VMI_ShaderComplexity;
  }
  else if (LowerMode == TEXT("lightmapdensity"))
  {
    Chosen = TEXT("LightmapDensity");
    ViewModeIndex = VMI_LightmapDensity;
  }
  else if (LowerMode == TEXT("stationarylightoverlap"))
  {
    Chosen = TEXT("StationaryLightOverlap");
    ViewModeIndex = VMI_StationaryLightOverlap;
  }
  else if (LowerMode == TEXT("reflectionoverride"))
  {
    Chosen = TEXT("ReflectionOverride");
    ViewModeIndex = VMI_ReflectionOverride;
  }
  else if (IsSafeConsoleArgumentToken(Mode))
  {
    Chosen = Mode;
    bHasNativeViewMode = false;
  }
  else {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("Invalid viewMode"), nullptr);
    return true;
  }

  // While the player plays, the game viewport is the one on screen, so the mode goes there. The level viewport hidden
  // behind it took the mode before, and the reply said it was set while the game looked the same.
  if (GEditor->PlayWorld && !GetEjectedPieViewportClientForMcp() && GEditor->GameViewport) {
    if (bHasNativeViewMode) {
      GEditor->GameViewport->ViewModeIndex = ViewModeIndex;
      ApplyViewMode(ViewModeIndex, /*bPerspective=*/true, GEditor->GameViewport->EngineShowFlags);
    } else {
      GEditor->GameViewport->ConsoleCommand(FString::Printf(TEXT("viewmode %s"), *Chosen));
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("viewMode"), Chosen);
    Resp->SetStringField(TEXT("method"), TEXT("game_viewport"));
    SendAutomationResponse(Socket, RequestId, true, TEXT("View mode set on the running game's view"), Resp, FString());
    return true;
  }

  if (bHasNativeViewMode) {
    if (FEditorViewportClient* ViewportClient = GetActiveEditorViewportClientForMcp()) {
      ViewportClient->SetViewMode(ViewModeIndex);
      ViewportClient->Invalidate();

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("viewMode"), Chosen);
      Resp->SetStringField(TEXT("method"), TEXT("viewport_client"));
      SendAutomationResponse(Socket, RequestId, true, TEXT("View mode set"), Resp,
                             FString());
      return true;
    }
  }

  const FString Cmd = FString::Printf(TEXT("viewmode %s"), *Chosen);
  if (GEditor->Exec(nullptr, *Cmd)) {
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
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetCameraFov(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  double Fov = 90.0;
  Payload->TryGetNumberField(TEXT("fov"), Fov);
  if (Fov <= 1.0 || Fov >= 179.0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("fov must be between 1 and 179 degrees"), nullptr);
    return true;
  }

  // While the player plays, the camera on screen is the game's: its field of view is locked to the value until play
  // stops. The level viewport hidden behind it took the value before and nothing on screen changed.
  if (GEditor->PlayWorld && !GetEjectedPieViewportClientForMcp()) {
    APlayerController *Controller = GEditor->PlayWorld->GetFirstPlayerController();
    if (Controller && Controller->PlayerCameraManager) {
      Controller->PlayerCameraManager->SetFOV(static_cast<float>(Fov));
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetNumberField(TEXT("fov"), Fov);
      Resp->SetStringField(TEXT("method"), TEXT("player_camera_manager"));
      SendAutomationResponse(Socket, RequestId, true, TEXT("The running game's camera FOV is locked to the value"), Resp,
                             FString());
      return true;
    }
  }

  if (FEditorViewportClient* ViewportClient = GetActiveEditorViewportClientForMcp()) {
    ViewportClient->ViewFOV = static_cast<float>(Fov);
    ViewportClient->FOVAngle = static_cast<float>(Fov);
    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("fov"), Fov);
    Resp->SetStringField(TEXT("method"), TEXT("viewport_client"));
    SendAutomationResponse(Socket, RequestId, true, TEXT("Camera FOV set"), Resp,
                           FString());
    return true;
  }

  SendStandardErrorResponse(this, Socket, RequestId, TEXT("VIEWPORT_NOT_AVAILABLE"),
                            TEXT("No editor viewport available for FOV update"), nullptr);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetViewportRealtime(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Accept both documented spellings. Reading only `realtime` made a schema-
  // valid {enabled: false} behave as absent/default-true on the native surface
  // while the TypeScript bridge normalized the same call correctly (MCPBB-046).
  bool bRealtime = true;
  if (!Payload->TryGetBoolField(TEXT("realtime"), bRealtime))
  {
    Payload->TryGetBoolField(TEXT("enabled"), bRealtime);
  }

  FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
  TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();

  if (ActiveViewport.IsValid()) {
    FEditorViewportClient& ViewportClient = ActiveViewport->GetAssetViewportClient();
    ViewportClient.SetRealtime(bRealtime);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("realtime"), bRealtime);
    Resp->SetStringField(TEXT("message"), bRealtime ? TEXT("Viewport realtime enabled") : TEXT("Viewport realtime disabled"));

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Viewport realtime updated"), Resp, FString());
    return true;
  }

  // Fallback: use console command
  FString Command = bRealtime ? TEXT("Viewport Realtime") : TEXT("Viewport Realtime 0");
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("realtime"), bRealtime);
  Resp->SetStringField(TEXT("message"), bRealtime ? TEXT("Viewport realtime enabled") : TEXT("Viewport realtime disabled"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Viewport realtime updated"), Resp, FString());
  return true;
}
