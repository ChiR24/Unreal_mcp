#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorPlay(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
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
#if MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS
  PlayParams.EditorPlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
#endif
#if MCP_HAS_LEVEL_EDITOR_MODULE
  if (FLevelEditorModule *LevelEditorModule =
          FModuleManager::GetModulePtr<FLevelEditorModule>(
              TEXT("LevelEditor"))) {
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
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
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
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorEject(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor->PlayWorld) {
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
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);

  // Also try "objectPath" as fallback since schema might use that
  if (ActorName.IsEmpty()) { Payload->TryGetStringField(TEXT("objectPath"), ActorName); }

  if (ActorName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(ActorName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr);
    return true;
  }

  // Only pawns can be possessed; POSSESS silently ignores anything else (dogfood #140).
  if (!Found->IsA<APawn>()) { SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_TARGET"), FString::Printf(TEXT("Actor '%s' is a %s, not a Pawn; only pawns can be possessed"), *ActorName, *Found->GetClass()->GetName()), nullptr); return true; }
  if (GEditor) {
    GEditor->SelectNone(true, true, false);
    GEditor->SelectActor(Found, true, true, true);
    // 'POSSESS' command works on selected actor in PIE
    if (GEditor->PlayWorld) {
      GEditor->Exec(GEditor->PlayWorld, TEXT("POSSESS"));
      SendAutomationResponse(Socket, RequestId, true, TEXT("Possessed actor"),
                             nullptr);
    } else {
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
bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetGameSpeed(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  double Speed = 1.0;
  Payload->TryGetNumberField(TEXT("speed"), Speed);
  if (Speed <= 0.0 || Speed > 20.0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("speed must be greater than 0 and no more than 20"), nullptr);
    return true;
  }

  UWorld* World = GEditor && GEditor->PlayWorld
      ? GEditor->PlayWorld.Get()
      : (GEditor ? GEditor->GetEditorWorldContext().World() : nullptr);
  if (!World || !World->GetWorldSettings()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_WORLD"),
                              TEXT("No world available for game speed update"), nullptr);
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
#else
  return false;
#endif
}
bool UMcpAutomationBridgeSubsystem::HandleControlEditorPause(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
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
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Pause requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorResume(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
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
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Resume requires editor build."), nullptr);
  return true;
#endif
}
bool UMcpAutomationBridgeSubsystem::HandleControlEditorStepFrame(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
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
    const FString Message =
        FString::Printf(TEXT("Stepped %d frame(s)"), Stepped);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
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
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Step frame requires editor build."), nullptr);
  return true;
#endif
}
