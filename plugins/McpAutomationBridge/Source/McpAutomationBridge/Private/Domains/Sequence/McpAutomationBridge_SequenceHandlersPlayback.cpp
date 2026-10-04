#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "LevelSequenceActor.h"
#include "SequencerSettings.h"
#include "UObject/UObjectIterator.h"


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

// A playing Sequencer ignores a seek: its clock carries on from where playback started, so the playhead stayed put
// while the reply named the frame asked for. Pausing first makes the jump land; play restarts the clock from there.
// The Sequencer poses its meshes on their next tick, and an editor that is minimized or in the background does not
// tick its world, so a screenshot after the seek showed the old frame: pose the meshes the Sequencer drives (it turns
// on their editor animation updates; followers have no instance and follow their leader) right away.
void McpSeekPaused(const TSharedPtr<ISequencer> &Sequencer, UMovieScene *MovieScene, double Seconds) {
  ULevelSequenceEditorBlueprintLibrary::Pause();
  Sequencer->SetLocalTime(MovieScene->GetTickResolution().AsFrameTime(Seconds));
  UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  // A mesh that copies another's pose (a face following its body) could refresh first and copy the old pose,
  // so a held frame showed the head a frame behind; the second pass reads the pose the first one made.
  for (int32 Pass = 0; Pass < 2; ++Pass) {
    for (TObjectIterator<USkeletalMeshComponent> It; It; ++It) {
      if (It->GetWorld() == World && It->GetUpdateAnimationInEditor() && It->GetAnimInstance()) {
        It->TickAnimation(0.f, false);
        It->RefreshBoneTransforms();
#if ENGINE_MINOR_VERSION >= 1
        It->RefreshFollowerComponents();
#else
        It->RefreshSlaveComponents();
#endif
        It->MarkRenderDynamicDataDirty();
      }
    }
  }
}

// The display frame the playhead is really on, read back rather than echoed from the request.
int32 McpPlayheadFrame(const TSharedPtr<ISequencer> &Sequencer, UMovieScene *MovieScene) {
  return Sequencer->GetLocalTime().ConvertTo(MovieScene->GetDisplayRate()).FloorToFrame().Value;
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
  ULevelSequence *LevelSeq = Cast<ULevelSequence>(McpLoadAsset(SeqPath));
  UMovieScene *MovieScene = LevelSeq ? LevelSeq->GetMovieScene() : nullptr;
  FString TimeError;
  if (!LoopMode.IsEmpty() && LoopMode != TEXT("once") && LoopMode != TEXT("loop")) {
    TimeError = TEXT("loopMode must be once or loop (Sequencer has no ping-pong playback)");
  }
  if (!TimeError.IsEmpty() || (bHasStartTime && !McpSequenceFrameMath::CheckPlaybackTime(MovieScene, StartTime, TimeError))) {
    SendAutomationResponse(Socket, RequestId, false, TimeError, nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
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
      McpSeekPaused(Sequencer, MovieScene, StartTime);
    }
    ULevelSequenceEditorBlueprintLibrary::Play();
    auto ToDisplay = [MovieScene, TickRate](FFrameTime Tick) {
      return ConvertFrameTime(Tick, TickRate, MovieScene->GetDisplayRate()).FloorToFrame().Value;
    };
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("playing"), true);
    // Seconds and display-rate frames; tick values divided by the display
    // rate used to be reported as seconds.
    Resp->SetNumberField(TEXT("startTime"), TickRate.AsSeconds(StartTick));
    Resp->SetNumberField(TEXT("currentFrame"), Sequencer.IsValid() ? McpPlayheadFrame(Sequencer, MovieScene)
                                                                   : ToDisplay(StartTick));
    Resp->SetNumberField(TEXT("playbackStart"), ToDisplay(FFrameTime(Range.GetLowerBoundValue())));
    Resp->SetNumberField(TEXT("playbackEnd"), ToDisplay(FFrameTime(Range.GetUpperBoundValue())));
    if (!LoopMode.IsEmpty()) {
      Resp->SetStringField(TEXT("loopMode"), LoopMode);
    }
    SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence playing"), Resp);
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

  UObject *SeqObj = McpLoadAsset(SeqPath);
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
  double HoldAt = 0.0;
  const bool bHold = LocalPayload->TryGetNumberField(TEXT("startTime"), HoldAt);
  ULevelSequence *LevelSeq = Cast<ULevelSequence>(McpLoadAsset(SeqPath));
  FString TimeError;
  if (bHold && !McpSequenceFrameMath::CheckPlaybackTime(LevelSeq ? LevelSeq->GetMovieScene() : nullptr, HoldAt, TimeError)) {
    SendAutomationResponse(Socket, RequestId, false, TimeError, nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // Holding a frame needs Sequencer, so a closed sequence is opened the way play opens it.
  if (LevelSeq && bHold && ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence() != LevelSeq) {
    ULevelSequenceEditorBlueprintLibrary::OpenLevelSequence(LevelSeq);
  }
  if (LevelSeq) {
    if (ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence() ==
        LevelSeq) {
      ULevelSequenceEditorBlueprintLibrary::Pause();
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      // startTime holds that frame, so the viewport shows the scene at that moment.
      if (TSharedPtr<ISequencer> Sequencer = McpFindOpenSequencer(LevelSeq)) {
        if (bHold) {
          McpSeekPaused(Sequencer, LevelSeq->GetMovieScene(), HoldAt);
        }
        Resp->SetNumberField(TEXT("currentFrame"), McpPlayheadFrame(Sequencer, LevelSeq->GetMovieScene()));
      }
      SendAutomationResponse(Socket, RequestId, true,
                                        TEXT("Sequence paused"), Resp);
      return true;
    }
  }
  SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not currently open in editor"), nullptr,
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
      Cast<ULevelSequence>(McpLoadAsset(SeqPath));
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
  SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not currently open in editor"), nullptr,
                         TEXT("EXECUTION_ERROR"));
  return true;
}
