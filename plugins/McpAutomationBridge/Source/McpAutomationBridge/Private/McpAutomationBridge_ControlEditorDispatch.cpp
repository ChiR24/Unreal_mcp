#include "McpAutomationBridge_ControlEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("control_editor"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("control_editor")))
    return false;
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("control_editor payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  if (LowerSub == TEXT("play"))
    return HandleControlEditorPlay(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("stop") || LowerSub == TEXT("stop_pie"))
    return HandleControlEditorStop(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("eject"))
    return HandleControlEditorEject(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("possess"))
    return HandleControlEditorPossess(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_view_target") ||
      LowerSub == TEXT("set_game_view_target"))
    return HandleControlEditorSetViewTarget(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("focus_actor"))
    return HandleControlEditorFocusActor(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_camera") ||
      LowerSub == TEXT("set_camera_position") ||
      LowerSub == TEXT("set_viewport_camera"))
    return HandleControlEditorSetCamera(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_view_mode"))
    return HandleControlEditorSetViewMode(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_camera_fov"))
    return HandleControlEditorSetCameraFov(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_game_speed"))
    return HandleControlEditorSetGameSpeed(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("open_asset"))
    return HandleControlEditorOpenAsset(RequestId, Payload, RequestingSocket);
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
