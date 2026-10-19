#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"

namespace {
// An animation section's speed when it is a fixed one: from 5.5 a section can also warp time (a loop, a clamp, a curve).
double ClipPlayRate(const UMovieSceneSkeletalAnimationSection *Clip, bool &bOutFixed) {
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
  bOutFixed = Clip->Params.PlayRate.GetType() == EMovieSceneTimeWarpType::FixedPlayRate;
  return bOutFixed ? Clip->Params.PlayRate.AsFixedPlayRate() : 1.0;
#else
  bOutFixed = true;
  return Clip->Params.PlayRate;
#endif
}

void SetClipPlayRate(UMovieSceneSkeletalAnimationSection *Clip, double Rate) {
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
  Clip->Params.PlayRate = FMovieSceneTimeWarpVariant(Rate);
#else
  Clip->Params.PlayRate = static_cast<float>(Rate);
#endif
  Clip->PreviousPlayRate = static_cast<float>(Rate);
}

// The payload's sequence and its trackName track; replies and returns null on a miss.
UMovieSceneTrack *LoadTrackOrReply(UEngineWireSubsystem *Self, const FString &RequestId,
                                   TSharedPtr<FMcpBridgeWebSocket> Socket,
                                   const TSharedPtr<FJsonObject> &Payload, const TCHAR *Action,
                                   ULevelSequence *&OutSequence, UMovieScene *&OutMovieScene) {
  OutSequence = McpSequence::LoadOrReply(Self, RequestId, Socket, Payload, Action, OutMovieScene);
  if (!OutSequence)
    return nullptr;
  UMovieSceneTrack *Track =
      FindTrackByName(OutMovieScene, GetJsonStringField(Payload, TEXT("trackName")));
  if (!Track)
    Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"), nullptr,
                                 TEXT("TRACK_NOT_FOUND"));
  return Track;
}
}

bool UEngineWireSubsystem::HandleSequenceSetTrackMuted(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  ULevelSequence *Sequence = nullptr;
  UMovieScene *MovieScene = nullptr;
  UMovieSceneTrack *Track = LoadTrackOrReply(this, RequestId, Socket, Payload, TEXT("set_track_muted"), Sequence, MovieScene);
  if (!Track)
    return true;
  const bool bMuted = GetJsonBoolField(Payload, TEXT("muted"), true);
  Track->SetEvalDisabled(bMuted);
  MovieScene->Modify();
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), Track->GetName());
  Resp->SetBoolField(TEXT("muted"), bMuted);
  SendAutomationResponse(Socket, RequestId, true,
                         bMuted ? TEXT("Track muted") : TEXT("Track unmuted"), Resp);
  return true;
}

bool UEngineWireSubsystem::HandleSequenceSetTrackSolo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  ULevelSequence *Sequence = nullptr;
  UMovieScene *MovieScene = nullptr;
  UMovieSceneTrack *SoloTrack = LoadTrackOrReply(this, RequestId, Socket, Payload, TEXT("set_track_solo"), Sequence, MovieScene);
  if (!SoloTrack)
    return true;
  const bool bSolo = GetJsonBoolField(Payload, TEXT("solo"), true);
  TArray<UMovieSceneTrack *> AllTracks;
  CollectTracksByName(MovieScene, FString(), FString(), AllTracks);
  int32 DisabledOtherTrackCount = 0;
  for (UMovieSceneTrack *Track : AllTracks) {
    const bool bDisableTrack = bSolo && Track != SoloTrack;
    Track->SetEvalDisabled(bDisableTrack);
    DisabledOtherTrackCount += bDisableTrack ? 1 : 0;
  }
  MovieScene->Modify();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), SoloTrack->GetName());
  Resp->SetBoolField(TEXT("solo"), bSolo);
  Resp->SetNumberField(TEXT("affectedTrackCount"), AllTracks.Num());
  Resp->SetNumberField(TEXT("disabledOtherTrackCount"), DisabledOtherTrackCount);
  SendAutomationResponse(
      Socket, RequestId, true,
      bSolo ? TEXT("Track solo enabled by disabling evaluation on other tracks")
            : TEXT("Solo disabled; all tracks evaluation-enabled"),
      Resp);
  return true;
}

bool UEngineWireSubsystem::HandleSequenceSetTrackLocked(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  ULevelSequence *Sequence = nullptr;
  UMovieScene *MovieScene = nullptr;
  UMovieSceneTrack *Track = LoadTrackOrReply(this, RequestId, Socket, Payload, TEXT("set_track_locked"), Sequence, MovieScene);
  if (!Track)
    return true;
  const bool bLocked = GetJsonBoolField(Payload, TEXT("locked"), true);
  for (UMovieSceneSection *Section : Track->GetAllSections()) {
    if (Section) {
      Section->SetIsLocked(bLocked);
    }
  }
  MovieScene->Modify();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), Track->GetName());
  Resp->SetBoolField(TEXT("locked"), bLocked);
  SendAutomationResponse(
      Socket, RequestId, true,
      bLocked ? TEXT("Track locked") : TEXT("Track unlocked"), Resp);
  return true;
}

// A section already on a track takes a new start and/or end and a manual ease in and/or out, all in display
// frames: overlapping clips with eased edges crossfade where they otherwise cut on one frame.
bool UEngineWireSubsystem::HandleSequenceSetSection(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  ULevelSequence *Sequence = nullptr;
  UMovieScene *MovieScene = nullptr;
  UMovieSceneTrack *Track = LoadTrackOrReply(this, RequestId, Socket, Payload, TEXT("set_section"), Sequence, MovieScene);
  if (!Track)
    return true;
  double Index = 0.0;
  Payload->TryGetNumberField(TEXT("sectionIndex"), Index);
  const TArray<UMovieSceneSection *> &Sections = Track->GetAllSections();
  UMovieSceneSection *Section = Index >= 0.0 && Index < Sections.Num() ? Sections[static_cast<int32>(Index)] : nullptr;
  if (!Section) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("sectionIndex %g is not a section of %s, which has %d"), Index,
                                           *Track->GetName(), Sections.Num()),
                           nullptr, TEXT("SECTION_NOT_FOUND"));
    return true;
  }
  auto Ticks = [MovieScene](double Frames) {
    return FFrameRate::TransformTime(FFrameTime::FromDecimal(Frames), MovieScene->GetDisplayRate(),
                                     MovieScene->GetTickResolution()).RoundToFrame();
  };
  double Start = 0.0, End = 0.0, EaseIn = 0.0, EaseOut = 0.0, Rate = 0.0;
  const bool bStart = Payload->TryGetNumberField(TEXT("startFrame"), Start);
  const bool bEnd = Payload->TryGetNumberField(TEXT("endFrame"), End);
  const bool bRate = Payload->TryGetNumberField(TEXT("playRate"), Rate);
  UMovieSceneSkeletalAnimationSection *Clip = Cast<UMovieSceneSkeletalAnimationSection>(Section);
  if (bRate && (!Clip || !(Rate > 0.0 && Rate <= 100.0))) {
    SendAutomationResponse(Socket, RequestId, false,
                           Clip ? TEXT("playRate is above 0 and at most 100 (2 plays the clip twice as fast)")
                                : TEXT("playRate is for an animation section; this one plays no animation"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  TRange<FFrameNumber> Range = Section->GetRange();
  bool bFixedRate = false;
  const double OldRate = Clip ? ClipPlayRate(Clip, bFixedRate) : 1.0;
  // A loop, clamp or curve time warp is more than a rate: a fixed playRate written over it changed the frames played.
  if (bRate && !bFixedRate) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("This clip's play rate is a time warp (a loop, clamp or curve), not one rate; edit it in "
                                "Sequencer, or remove the warp before setting playRate."),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // The clip keeps the frames it plays, as the Sequencer's play rate does: twice the rate, half the section. An
  // endFrame given wins, and a time warp that is not a fixed rate leaves the length alone. Setting the rate as a
  // property instead left the section one tick long.
  // A frozen clip (rate 0) plays no frames to keep, so its length stays: scaled by 0 it became one tick.
  if (bRate && !bEnd && bFixedRate && OldRate > 0.0 && Range.HasLowerBound() && Range.HasUpperBound()) {
    const FFrameNumber Low = Range.GetLowerBoundValue();
    const double Length = static_cast<double>((Range.GetUpperBoundValue() - Low).Value);
    Range.SetUpperBound(TRangeBound<FFrameNumber>::Exclusive(Low + FFrameNumber(FMath::Max(1, static_cast<int32>(FMath::RoundToDouble(Length * OldRate / Rate))))));
  }
  if (bStart)
    Range.SetLowerBound(TRangeBound<FFrameNumber>::Inclusive(Ticks(Start)));
  if (bEnd)
    Range.SetUpperBound(TRangeBound<FFrameNumber>::Exclusive(Ticks(End)));
  if ((bStart || bEnd) && Range.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("endFrame must come after startFrame"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  Section->Modify();
  if (bStart || bEnd || bRate)
    Section->SetRange(Range);
  if (bRate)
    SetClipPlayRate(Clip, Rate);
  if (Payload->TryGetNumberField(TEXT("easeInFrames"), EaseIn)) {
    Section->Easing.bManualEaseIn = EaseIn > 0.0;
    Section->Easing.ManualEaseInDuration = Ticks(FMath::Max(EaseIn, 0.0)).Value;
  }
  if (Payload->TryGetNumberField(TEXT("easeOutFrames"), EaseOut)) {
    Section->Easing.bManualEaseOut = EaseOut > 0.0;
    Section->Easing.ManualEaseOutDuration = Ticks(FMath::Max(EaseOut, 0.0)).Value;
  }
  MovieScene->Modify();
  Sequence->MarkPackageDirty();
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), Track->GetName());
  Resp->SetNumberField(TEXT("sectionIndex"), static_cast<int32>(Index));
  if (Clip) {
    bool bFixed = false;
    Resp->SetNumberField(TEXT("playRate"), ClipPlayRate(Clip, bFixed));
  }
  SendAutomationResponse(Socket, RequestId, true, TEXT("Section updated"), Resp);
  return true;
}

bool UEngineWireSubsystem::HandleSequenceRemoveTrack(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  ULevelSequence *Sequence = nullptr;
  UMovieScene *MovieScene = nullptr;
  UMovieSceneTrack *Track = LoadTrackOrReply(this, RequestId, Socket, Payload, TEXT("remove_track"), Sequence, MovieScene);
  if (!Track)
    return true;
  const FString RemovedTrackName = Track->GetName();
  // The Modify() calls recorded nothing: no transaction was open, so undo could not bring the track back.
  // The scope opens one and captures both before the change. MarkPackageDirty is what makes the removal
  // reach disk at all: without it the editor never offered to save, and a restart resurrected the track.
  FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Remove Sequence Track")),
                                          EMcpMutationDurability::EditorStateOnly, {Sequence, MovieScene});
  // RemoveTrack only searches the Tracks array, so it silently fails on the
  // camera cut track, which lives in its own member and needs its own call.
  if (Track == MovieScene->GetCameraCutTrack()) {
    MovieScene->RemoveCameraCutTrack();
  } else {
    MCP_REMOVE_MOVIESCENE_TRACK(MovieScene, *Track);
  }
  Sequence->MarkPackageDirty();
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), RemovedTrackName);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Track removed"), Resp);
  return true;
}
