#include "McpAutomationBridge_ControlEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetViewMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
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
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetCameraFov(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  double Fov = 90.0;
  Payload->TryGetNumberField(TEXT("fov"), Fov);
  if (Fov <= 1.0 || Fov >= 179.0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("fov must be between 1 and 179 degrees"), nullptr);
    return true;
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
#else
  return false;
#endif
}


bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetViewportRealtime(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  bool bRealtime = true;
  Payload->TryGetBoolField(TEXT("realtime"), bRealtime);

#if MCP_HAS_LEVEL_EDITOR_MODULE
  // Get the level editor module and active viewport
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
#endif

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
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Viewport realtime requires editor build."), nullptr);
  return true;
#endif
}
