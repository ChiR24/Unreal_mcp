#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

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
  ULevelSequence *LevelSeq =
      Cast<ULevelSequence>(UEditorAssetLibrary::LoadAsset(SeqPath));
  if (LevelSeq) {
    if (ULevelSequenceEditorBlueprintLibrary::OpenLevelSequence(LevelSeq)) {
      ULevelSequenceEditorBlueprintLibrary::Play();
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("playing"), true);
      if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
        TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
        const double Start = static_cast<double>(Range.GetLowerBoundValue().Value);
        const double End = static_cast<double>(Range.GetUpperBoundValue().Value);
        FFrameRate FR = MovieScene->GetDisplayRate();
        Resp->SetNumberField(TEXT("startTime"), Start / FR.AsDecimal());
        Resp->SetNumberField(TEXT("currentFrame"), Start);
        Resp->SetNumberField(TEXT("playbackStart"), Start);
        Resp->SetNumberField(TEXT("playbackEnd"), End);
      }
      SendAutomationResponse(Socket, RequestId, true,
                                        TEXT("Sequence playing"), Resp);
      return true;
    }
  }
  SendAutomationResponse(Socket, RequestId, false,
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
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false,
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
          SendAutomationResponse(
              Socket, RequestId, true,
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

  SendAutomationResponse(
      Socket, RequestId, false,
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
  ULevelSequence *LevelSeq =
      Cast<ULevelSequence>(UEditorAssetLibrary::LoadAsset(SeqPath));
  if (LevelSeq) {
    if (ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence() ==
        LevelSeq) {
      ULevelSequenceEditorBlueprintLibrary::Pause();
      SendAutomationResponse(Socket, RequestId, true,
                                        TEXT("Sequence paused"), nullptr);
      return true;
    }
  }
  SendAutomationResponse(
      Socket, RequestId, false,
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
      SendAutomationResponse(
          Socket, RequestId, true, TEXT("Sequence stopped (reset to start)"),
          nullptr);
      return true;
    }
  }
  SendAutomationResponse(
      Socket, RequestId, false,
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
