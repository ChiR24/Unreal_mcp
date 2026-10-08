#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

#include "GameFramework/WorldSettings.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorPlay(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (GEditor->PlayWorld) {
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
  PlayParams.EditorPlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
  if (FLevelEditorModule *LevelEditorModule =
          FModuleManager::GetModulePtr<FLevelEditorModule>(
              TEXT("LevelEditor"))) {
    TSharedPtr<IAssetViewport> DestinationViewport =
        LevelEditorModule->GetFirstActiveViewport();
    if (DestinationViewport.IsValid())
      PlayParams.DestinationSlateViewport = DestinationViewport;
  }

  GEditor->RequestPlaySession(PlayParams);
  // The session only starts on a later editor tick, so answering here let the very next call
  // (set_game_speed, simulate_input) find no play world. Reply once it has begun play.
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  const double Deadline = FPlatformTime::Seconds() + 20.0;
  FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Socket, RequestId, Deadline](float) {
    UWorld *World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
    const bool bStarted = World && World->HasBegunPlay();
    if (!WeakThis.IsValid() || (!bStarted && FPlatformTime::Seconds() < Deadline)) {
      return WeakThis.IsValid();
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), bStarted);
    if (World) { Resp->SetStringField(TEXT("pieWorld"), World->GetOutermost()->GetName()); }
    WeakThis->SendAutomationResponse(Socket, RequestId, bStarted,
        bStarted ? TEXT("Play in Editor started") : TEXT("Play in Editor was requested but no play world began within 20 s"),
        Resp, bStarted ? FString() : TEXT("PIE_START_TIMEOUT"));
    return false;
  }), 0.0f);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStop(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!GEditor->PlayWorld) {
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
}

// possess lives in Session/McpAutomationBridge_ControlEditorEject.cpp, beside eject: one switch, two directions.
bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetGameSpeed(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  double Speed = 1.0;
  Payload->TryGetNumberField(TEXT("speed"), Speed);
  if (Speed <= 0.0 || Speed > 20.0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("speed must be greater than 0 and no more than 20"), nullptr);
    return true;
  }

  // A running game only: in edit mode this wrote Time Dilation into the level's
  // own World Settings, which changed nothing on screen and shipped with the
  // next level save.
  UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
  if (!World || !World->GetWorldSettings()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("set_game_speed changes a running game's clock; start Play In Editor first (control_editor play)."), nullptr);
    return true;
  }

  World->GetWorldSettings()->SetTimeDilation(static_cast<float>(Speed));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("speed"), Speed);
  Resp->SetNumberField(TEXT("timeDilation"), World->GetWorldSettings()->GetEffectiveTimeDilation());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Game speed set"), Resp,
                         FString());
  return true;
}
bool UMcpAutomationBridgeSubsystem::HandleControlEditorPause(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  if (!GEditor->PlayWorld) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("No active PIE session to pause"), nullptr);
    return true;
  }

  GEditor->PlayWorld->bDebugPauseExecution = true;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("state"), TEXT("paused"));
  Resp->SetStringField(TEXT("message"), TEXT("PIE session paused"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("PIE session paused"), Resp, FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorResume(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  if (!GEditor->PlayWorld) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("No active PIE session to resume"), nullptr);
    return true;
  }

  GEditor->PlayWorld->bDebugPauseExecution = false;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("state"), TEXT("resumed"));
  Resp->SetStringField(TEXT("message"), TEXT("PIE session resumed"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("PIE session resumed"), Resp, FString());
  return true;
}
bool UMcpAutomationBridgeSubsystem::HandleControlEditorStepFrame(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  if (!GEditor->PlayWorld) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("No active PIE session to step"), nullptr);
    return true;
  }

  // The TypeScript path loops N bridge calls and reports `steps`; the native
  // surface reaches this handler directly, so stepping and reporting have to
  // happen here too or `steps: 10` silently advances one frame and the reply
  // fails its own output schema for want of the `steps` field.
  int32 RequestedSteps = 1;
  if (Payload.IsValid()) {
    Payload->TryGetNumberField(TEXT("steps"), RequestedSteps);
  }
  // A frame can only be stepped by returning to the engine loop, so the count
  // is also the number of ticks this request stays open -- bounded so one call
  // cannot hold a socket indefinitely.
  constexpr int32 MaxStepFrames = 600;
  const int32 TotalSteps = FMath::Clamp(RequestedSteps, 1, MaxStepFrames);

  auto AdvanceOneFrame = [](UWorld *World) {
    World->bDebugFrameStepExecution = true;
    World->bDebugPauseExecution = false;
  };

  // Takes the subsystem explicitly rather than capturing `this`, so the copy
  // held by the ticker below cannot outlive it.
  auto SendStepped = [](UMcpAutomationBridgeSubsystem *Self,
                        TSharedPtr<FMcpBridgeWebSocket> ReplySocket,
                        const FString &ReplyRequestId, int32 Stepped) {
    // A step keeps the slow motion that was set, so "Stepped 6 frame(s)" at speed 0.1 advanced
    // 0.02 s, not the 0.2 s the caller assumed: say the speed, and the game time when steps are fixed.
    const UWorld *PlayWorld = GEditor ? GEditor->PlayWorld.Get() : nullptr;
    const AWorldSettings *Settings = PlayWorld ? PlayWorld->GetWorldSettings() : nullptr;
    const float Dilation = Settings ? Settings->GetEffectiveTimeDilation() : 1.0f;
    FString Message = FString::Printf(TEXT("Stepped %d frame(s)"), Stepped);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    if (!FMath::IsNearlyEqual(Dilation, 1.0f)) {
      Message += FString::Printf(TEXT(" at game speed %.3g"), Dilation);
      Resp->SetNumberField(TEXT("timeDilation"), Dilation);
    }
    if (FApp::UseFixedTimeStep()) {
      Resp->SetNumberField(TEXT("gameSeconds"), Stepped * FApp::GetFixedDeltaTime() * Dilation);
    }
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("steps"), Stepped);
    Resp->SetStringField(TEXT("message"), Message);
    Self->SendAutomationResponse(ReplySocket, ReplyRequestId, true, Message,
                                 Resp, FString());
  };

  AdvanceOneFrame(GEditor->PlayWorld);
  if (TotalSteps == 1) {
    SendStepped(this, Socket, RequestId, 1);
    return true;
  }

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  TSharedRef<int32> Stepped = MakeShared<int32>(1);
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakThis, Socket, RequestId, TotalSteps,
                                     Stepped, AdvanceOneFrame,
                                     SendStepped](float) {
        if (!WeakThis.IsValid()) {
          return false;
        }
        UWorld *World = GEditor ? GEditor->PlayWorld : nullptr;
        if (World && *Stepped < TotalSteps) {
          AdvanceOneFrame(World);
          ++(*Stepped);
          if (*Stepped < TotalSteps) {
            return true;
          }
        }
        // Finished, or PIE ended early -- either way report the frames that
        // actually advanced rather than the count that was asked for.
        SendStepped(WeakThis.Get(), Socket, RequestId, *Stepped);
        return false;
      }),
      0.0f);
  return true;
}
