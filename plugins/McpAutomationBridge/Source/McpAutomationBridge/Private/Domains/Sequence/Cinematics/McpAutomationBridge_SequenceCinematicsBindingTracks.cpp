#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"

#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "MovieScene.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#endif

namespace McpSequenceCinematics {
#if WITH_EDITOR
namespace {
bool LoadSequenceAndBindingForBoundTrack(const TSharedPtr<FJsonObject> &Params,
                            const TCHAR *Action, ULevelSequence *&OutSequence,
                            FGuid &OutGuid, TSharedPtr<FJsonObject> &OutResult) {
  OutSequence = LoadSequence(Params, OutResult);
  if (!OutSequence) return false;
  if (!ReadBindingGuid(Params, OutGuid)) {
    // The canonical records declare actorName, never bindingGuid, so the
    // documented request arrived here with no readable binding and was refused.
    // bindingGuid stays the precedence-winning spelling; actorName resolves (or
    // creates) the binding through the same helpers the camera tracks use.
    if (AActor *BoundActor = ResolveActor(Params)) {
      OutGuid = ResolveOrCreateBinding(OutSequence, BoundActor);
    }
    if (!OutGuid.IsValid()) {
      OutResult = MakeResult(false, Action,
                             TEXT("actorName or bindingGuid is required"),
                             TEXT("INVALID_ARGUMENT"));
      return false;
    }
  }
  return true;
}

UMovieSceneSection *AddSection(ULevelSequence *Sequence, UClass *TrackClass,
                               const FGuid &Guid) {
  UMovieSceneTrack *Track = AddTrackForBinding(Sequence->GetMovieScene(), TrackClass, Guid);
  UMovieSceneSection *Section = Track ? Track->CreateNewSection() : nullptr;
  if (Track && Section) Track->AddSection(*Section);
  if (!Section)
    RemoveTrackAfterSectionFailure(Sequence->GetMovieScene(), Track, true);
  return Section;
}

}
#endif

bool HandleAddSkeletalAnimationTrack(UMcpAutomationBridgeSubsystem *Self,
                                     const TSharedPtr<FJsonObject> &Params,
                                     TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if WITH_EDITOR
  ULevelSequence *Sequence = nullptr;
  FGuid Guid;
  if (!LoadSequenceAndBindingForBoundTrack(Params, TEXT("add_skeletal_animation_track"), Sequence,
                              Guid, OutResult))
    return true;
  FString AnimPath =
      GetString(Params, TEXT("animationSequencePath"), TEXT("animationPath"));
  if (AnimPath.IsEmpty())
    AnimPath = GetString(Params, TEXT("animSequencePath"));
  UAnimSequence *Anim = AnimPath.IsEmpty() ? nullptr : LoadObject<UAnimSequence>(nullptr, *AnimPath);
  if (!Anim) {
    OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                           TEXT("Valid animationSequencePath is required"),
                           TEXT("ANIMATION_NOT_FOUND"));
    return true;
  }
  FFrameNumber AnimationDisplayDuration;
  FString FrameError;
  if (!McpSequenceFrameMath::TrySecondsToFrame(
          Anim->GetPlayLength(),
          Sequence->GetMovieScene()->GetDisplayRate(),
          AnimationDisplayDuration, FrameError)) {
    OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                           FrameError, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!McpSequenceFrameMath::ValidateCinematicFrameRequest(
          Params, Sequence->GetMovieScene(), FrameError,
          FMath::Max(1, AnimationDisplayDuration.Value))) {
    OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                           FrameError, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  UMovieSceneSkeletalAnimationTrack *Track = Cast<UMovieSceneSkeletalAnimationTrack>(
      AddTrackForBinding(Sequence->GetMovieScene(),
                         UMovieSceneSkeletalAnimationTrack::StaticClass(), Guid));
  if (!Track) {
    OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                           TEXT("Failed to create skeletal animation track"),
                           TEXT("TRACK_CREATION_FAILED"));
    return true;
  }
  UMovieSceneSkeletalAnimationSection *Section =
      Cast<UMovieSceneSkeletalAnimationSection>(Track->CreateNewSection());
  if (!Section) {
    RemoveTrackAfterSectionFailure(Sequence->GetMovieScene(), Track, true);
    OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                           TEXT("Failed to create skeletal animation section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  Track->AddSection(*Section);
  Section->Params.Animation = Anim;
  SetSectionRange(
      Sequence->GetMovieScene(), Section, Params,
      FMath::Max(1, AnimationDisplayDuration.Value));
  Sequence->GetMovieScene()->Modify();
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  OutResult = MakeResult(true, TEXT("add_skeletal_animation_track"),
                         TEXT("Skeletal animation track added"));
  OutResult->SetStringField(TEXT("bindingGuid"), Guid.ToString());
  OutResult->SetStringField(TEXT("animationSequencePath"), AnimPath);
  return true;
#else
  OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                         TEXT("Editor build required"), TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool HandleAddTransformTrack(UMcpAutomationBridgeSubsystem *Self,
                             const TSharedPtr<FJsonObject> &Params,
                             TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if WITH_EDITOR
  ULevelSequence *Sequence = nullptr;
  FGuid Guid;
  if (!LoadSequenceAndBindingForBoundTrack(Params, TEXT("add_transform_track"), Sequence, Guid,
                              OutResult))
    return true;
  UMovieSceneSection *Section =
      AddSection(Sequence, UMovieScene3DTransformTrack::StaticClass(), Guid);
  if (!Section) {
    OutResult = MakeResult(false, TEXT("add_transform_track"),
                           TEXT("Failed to create transform section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  SetSectionRange(Sequence->GetMovieScene(), Section, Params, 100);
  Sequence->GetMovieScene()->Modify();
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  OutResult = MakeResult(true, TEXT("add_transform_track"), TEXT("Transform track added"));
  OutResult->SetStringField(TEXT("bindingGuid"), Guid.ToString());
  return true;
#else
  OutResult = MakeResult(false, TEXT("add_transform_track"), TEXT("Editor build required"),
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool HandleAddEventTrack(UMcpAutomationBridgeSubsystem *Self,
                         const TSharedPtr<FJsonObject> &Params,
                         TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if WITH_EDITOR
  ULevelSequence *Sequence = LoadSequence(Params, OutResult);
  if (!Sequence) return true;
  FGuid Guid;
  ReadBindingGuid(Params, Guid);
  UMovieSceneSection *Section =
      AddSection(Sequence, UMovieSceneEventTrack::StaticClass(), Guid);
  if (!Section) {
    OutResult = MakeResult(false, TEXT("add_event_track"),
                           TEXT("Failed to create event section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
  SetSectionRange(Sequence->GetMovieScene(), Section, Params, 100);
  Sequence->GetMovieScene()->Modify();
  Sequence->MarkPackageDirty();
  if (!MaybeSaveSequence(Sequence, Params, OutResult)) return true;
  OutResult = MakeResult(true, TEXT("add_event_track"), TEXT("Event track added"));
  if (Guid.IsValid()) OutResult->SetStringField(TEXT("bindingGuid"), Guid.ToString());
  return true;
#else
  OutResult = MakeResult(false, TEXT("add_event_track"), TEXT("Editor build required"),
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

}
