#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleSequencePlay(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No sequence selected or path provided"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  FString RequestIdArg = RequestId;
  UMcpAutomationBridgeSubsystem *Subsystem = this;
  ULevelSequence *LevelSeq =
      Cast<ULevelSequence>(UEditorAssetLibrary::LoadAsset(SeqPath));
  if (LevelSeq) {
    if (ULevelSequenceEditorBlueprintLibrary::OpenLevelSequence(LevelSeq)) {
      ULevelSequenceEditorBlueprintLibrary::Play();
      Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                        TEXT("Sequence playing"), nullptr);
      return true;
    }
  }
  Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                    TEXT("Failed to open or play sequence"),
                                    nullptr, TEXT("EXECUTION_ERROR"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_play requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetPlaybackSpeed(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  double Speed = 1.0;
  LocalPayload->TryGetNumberField(TEXT("speed"), Speed);
  if (Speed <= 0.0) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid speed (must be > 0)"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_set_playback_speed requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  FString RequestIdArg = RequestId;
  UMcpAutomationBridgeSubsystem *Subsystem = this;
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (GEditor) {
    if (UAssetEditorSubsystem *AssetEditorSS =
            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
      IAssetEditorInstance *Editor =
          AssetEditorSS->FindEditorForAsset(SeqObj, false);
      if (ILevelSequenceEditorToolkit *LSEditor =
              static_cast<ILevelSequenceEditorToolkit *>(Editor)) {
        if (LSEditor->GetSequencer().IsValid()) {
          LSEditor->GetSequencer()->SetPlaybackSpeed(
              static_cast<float>(Speed));
          Subsystem->SendAutomationResponse(
              Socket, RequestIdArg, true,
              FString::Printf(TEXT("Playback speed set to %.2f"), Speed),
              nullptr);
          return true;
        } else {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
                 TEXT("HandleSequenceSetPlaybackSpeed: Sequencer invalid for "
                      "asset %s"),
                 *SeqObj->GetName());
        }
      }
    }
  }

  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false,
      TEXT("Sequence editor not open or interface unavailable"), nullptr,
      TEXT("EDITOR_NOT_OPEN"));
  return true;
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("sequence_set_playback_speed requires editor build."), nullptr,
      TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequencePause(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_pause requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
#if WITH_EDITOR
  FString RequestIdArg = RequestId;
  UMcpAutomationBridgeSubsystem *Subsystem = this;
  ULevelSequence *LevelSeq =
      Cast<ULevelSequence>(UEditorAssetLibrary::LoadAsset(SeqPath));
  if (LevelSeq) {
    if (ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence() ==
        LevelSeq) {
      ULevelSequenceEditorBlueprintLibrary::Pause();
      Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                        TEXT("Sequence paused"), nullptr);
      return true;
    }
  }
  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false,
      TEXT("Sequence not currently open in editor"), nullptr,
      TEXT("EXECUTION_ERROR"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_pause requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceStop(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_stop requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
#if WITH_EDITOR
  FString RequestIdArg = RequestId;
  UMcpAutomationBridgeSubsystem *Subsystem = this;
  ULevelSequence *LevelSeq =
      Cast<ULevelSequence>(UEditorAssetLibrary::LoadAsset(SeqPath));
  if (LevelSeq) {
    if (ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence() ==
        LevelSeq) {
      ULevelSequenceEditorBlueprintLibrary::Pause();
      FMovieSceneSequencePlaybackParams PlaybackParams;
      PlaybackParams.Frame = FFrameTime(0);
      PlaybackParams.UpdateMethod = EUpdatePositionMethod::Scrub;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
      ULevelSequenceEditorBlueprintLibrary::SetGlobalPosition(PlaybackParams);
#else
      ULevelSequenceEditorBlueprintLibrary::SetCurrentTime(0);
#endif
      Subsystem->SendAutomationResponse(
          Socket, RequestIdArg, true, TEXT("Sequence stopped (reset to start)"),
          nullptr);
      return true;
    }
  }
  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false,
      TEXT("Sequence not currently open in editor"), nullptr,
      TEXT("EXECUTION_ERROR"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_stop requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}
