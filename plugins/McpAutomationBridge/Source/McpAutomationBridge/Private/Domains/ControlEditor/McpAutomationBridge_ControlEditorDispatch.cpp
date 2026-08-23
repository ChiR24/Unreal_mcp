#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

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
  if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty()) {
    Payload->TryGetStringField(TEXT("action"), SubAction);
  }
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
  if (LowerSub == TEXT("open_editor_tab"))
    return HandleOpenEditorTab(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("describe_reflected_api"))
    return HandleDescribeReflectedApi(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("invoke_reflected_function"))
    return HandleInvokeReflectedFunction(RequestId, Payload, RequestingSocket);
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
  if (LowerSub == TEXT("set_viewport_resolution")) {
    // The record routes this to console_command with r.SetRes, and the
    // TypeScript gateway honours that. The native gateway resolves the wire
    // action from the parent tool's dispatch pattern alone, so the cross-parent
    // hop was lost here and a published capability answered UNKNOWN_ACTION on
    // one surface only. Rebuild the same console call the other surface makes.
    double Width = 0.0;
    double Height = 0.0;
    if (!Payload->TryGetNumberField(TEXT("width"), Width) ||
        !Payload->TryGetNumberField(TEXT("height"), Height) || Width <= 0.0 ||
        Height <= 0.0) {
      SendStandardErrorResponse(this, RequestingSocket, RequestId,
                                TEXT("VALIDATION_ERROR"),
                                TEXT("Width and height must be positive numbers"),
                                nullptr);
      return true;
    }
    TSharedPtr<FJsonObject> ConsolePayload = MakeShared<FJsonObject>();
    ConsolePayload->Values = Payload->Values;
    ConsolePayload->SetStringField(
        TEXT("command"), FString::Printf(TEXT("r.SetRes %dx%d"),
                                         static_cast<int32>(Width),
                                         static_cast<int32>(Height)));
    return HandleControlEditorConsoleCommand(RequestId, ConsolePayload,
                                             RequestingSocket);
  }

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
