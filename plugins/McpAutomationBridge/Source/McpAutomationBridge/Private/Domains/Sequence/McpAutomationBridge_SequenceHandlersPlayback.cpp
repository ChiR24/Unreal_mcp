#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "EngineUtils.h"
#include "LevelSequenceActor.h"
#include "SequencerSettings.h"


namespace {
// The Sequencer editing SequenceAsset, when its asset editor is open.
TSharedPtr<ISequencer> McpFindOpenSequencer(UObject *SequenceAsset) {
  UAssetEditorSubsystem *AssetEditors =
      GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
  IAssetEditorInstance *Editor =
      AssetEditors && SequenceAsset
          ? AssetEditors->FindEditorForAsset(SequenceAsset, false)
          : nullptr;
  if (!Editor) {
    return TSharedPtr<ISequencer>();
  }
  return static_cast<ILevelSequenceEditorToolkit *>(Editor)->GetSequencer();
}
}

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
  FString LoopMode;
  LocalPayload->TryGetStringField(TEXT("loopMode"), LoopMode);
  LoopMode = LoopMode.ToLower();
  double StartTime = 0.0;
  const bool bHasStartTime = LocalPayload->TryGetNumberField(TEXT("startTime"), StartTime);
  if ((!LoopMode.IsEmpty() && LoopMode != TEXT("once") && LoopMode != TEXT("loop")) ||
      (bHasStartTime && (!FMath::IsFinite(StartTime) || StartTime < 0.0))) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("loopMode must be once or loop (Sequencer has no "
                                "ping-pong playback) and startTime a non-negative "
                                "number of seconds"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  ULevelSequence *LevelSeq =
      Cast<ULevelSequence>(UEditorAssetLibrary::LoadAsset(SeqPath));
  UMovieScene *MovieScene = LevelSeq ? LevelSeq->GetMovieScene() : nullptr;
  if (MovieScene && ULevelSequenceEditorBlueprintLibrary::OpenLevelSequence(LevelSeq)) {
    TSharedPtr<ISequencer> Sequencer = McpFindOpenSequencer(LevelSeq);
    if (!Sequencer.IsValid() && (bHasStartTime || !LoopMode.IsEmpty())) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Sequencer did not open, so startTime and "
                                  "loopMode could not be applied"),
                             nullptr, TEXT("EDITOR_NOT_OPEN"));
      return true;
    }
    if (!LoopMode.IsEmpty() && Sequencer->GetSequencerSettings()) {
      Sequencer->GetSequencerSettings()->SetLoopMode(
          LoopMode == TEXT("loop") ? SLM_Loop : SLM_NoLoop);
    }
    const FFrameRate TickRate = MovieScene->GetTickResolution();
    const TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
    const FFrameTime StartTick = bHasStartTime
                                     ? TickRate.AsFrameTime(StartTime)
                                     : FFrameTime(Range.GetLowerBoundValue());
    if (bHasStartTime) {
      Sequencer->SetLocalTime(StartTick);
    }
    ULevelSequenceEditorBlueprintLibrary::Play();
    auto ToDisplay = [MovieScene, TickRate](FFrameTime Tick) {
      return ConvertFrameTime(Tick, TickRate, MovieScene->GetDisplayRate())
          .FloorToFrame()
          .Value;
    };
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("playing"), true);
    // Seconds and display-rate frames; tick values divided by the display
    // rate used to be reported as seconds.
    Resp->SetNumberField(TEXT("startTime"), TickRate.AsSeconds(StartTick));
    Resp->SetNumberField(TEXT("currentFrame"), ToDisplay(StartTick));
    Resp->SetNumberField(TEXT("playbackStart"),
                         ToDisplay(FFrameTime(Range.GetLowerBoundValue())));
    Resp->SetNumberField(TEXT("playbackEnd"),
                         ToDisplay(FFrameTime(Range.GetUpperBoundValue())));
    if (!LoopMode.IsEmpty()) {
      Resp->SetStringField(TEXT("loopMode"), LoopMode);
    }
    SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence playing"),
                           Resp);
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                                    TEXT("Failed to open or play sequence"),
                                    nullptr, TEXT("EXECUTION_ERROR"));
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetPlaybackSpeed(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  double Speed = 1.0;
  LocalPayload->TryGetNumberField(TEXT("speed"), Speed);
  if (!FMath::IsFinite(Speed) || Speed <= 0.0) {
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

  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!Cast<ULevelSequence>(SeqObj)) {
    SendAutomationResponse(Socket, RequestId, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }

  // The play rate lives in PlaybackSettings on each level sequence actor that
  // plays this asset; that is what runs in game and PIE, so it needs no open
  // Sequencer. An open Sequencer gets the preview speed as well.
  TArray<TSharedPtr<FJsonValue>> UpdatedActors;
  if (UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr) {
    for (TActorIterator<ALevelSequenceActor> It(World); It; ++It) {
      ALevelSequenceActor *Actor = *It;
      if (!Actor || Actor->GetSequence() != SeqObj) {
        continue;
      }
      Actor->Modify();
      Actor->PlaybackSettings.PlayRate = static_cast<float>(Speed);
      UpdatedActors.Add(MakeShared<FJsonValueString>(Actor->GetActorLabel()));
    }
  }
  TSharedPtr<ISequencer> Sequencer = McpFindOpenSequencer(SeqObj);
  if (Sequencer.IsValid()) {
    Sequencer->SetPlaybackSpeed(static_cast<float>(Speed));
  }
  if (UpdatedActors.Num() == 0 && !Sequencer.IsValid()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("No level sequence actor in the editor level plays this sequence "
             "and it is not open in Sequencer; place a level sequence actor for "
             "it or open it first"),
        nullptr, TEXT("PLAYBACK_TARGET_NOT_FOUND"));
    return true;
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetNumberField(TEXT("speed"), Speed);
  Resp->SetArrayField(TEXT("levelSequenceActors"), UpdatedActors);
  Resp->SetBoolField(TEXT("sequencerUpdated"), Sequencer.IsValid());
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Playback speed set to %.2f on %d level sequence "
                           "actor(s)%s"),
                      Speed, UpdatedActors.Num(),
                      Sequencer.IsValid() ? TEXT(" and the open Sequencer") : TEXT("")),
      Resp);
  return true;
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
}
