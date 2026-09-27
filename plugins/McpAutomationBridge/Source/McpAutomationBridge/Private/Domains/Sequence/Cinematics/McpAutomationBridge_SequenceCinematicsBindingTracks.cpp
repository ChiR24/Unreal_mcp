#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"

#include "Animation/AnimSequence.h"
#include "MovieScene.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"

namespace McpSequenceCinematics {
namespace {
UMovieSceneSection *AddSection(ULevelSequence *Sequence, UClass *TrackClass,
                               const FGuid &Guid) {
  UMovieScene *MovieScene = Sequence->GetMovieScene();
  return AddTrackSection(MovieScene, AddTrackForBinding(MovieScene, TrackClass, Guid), true);
}

}

bool HandleAddSkeletalAnimationTrack(const TSharedPtr<FJsonObject> &Params,
                                     TSharedPtr<FJsonObject> &OutResult) {
  ULevelSequence *Sequence = nullptr;
  FGuid Guid;
  if (!LoadSequenceAndBinding(Params, TEXT("add_skeletal_animation_track"), Sequence,
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
      Cast<UMovieSceneSkeletalAnimationSection>(AddTrackSection(Sequence->GetMovieScene(), Track, true));
  if (!Section) {
    OutResult = MakeResult(false, TEXT("add_skeletal_animation_track"),
                           TEXT("Failed to create skeletal animation section"),
                           TEXT("SECTION_CREATION_FAILED"));
    return true;
  }
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
}

bool HandleAddTransformTrack(const TSharedPtr<FJsonObject> &Params,
                             TSharedPtr<FJsonObject> &OutResult) {
  ULevelSequence *Sequence = nullptr;
  FGuid Guid;
  if (!LoadSequenceAndBinding(Params, TEXT("add_transform_track"), Sequence, Guid,
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
}

bool HandleAddEventTrack(const TSharedPtr<FJsonObject> &Params,
                         TSharedPtr<FJsonObject> &OutResult) {
  ULevelSequence *Sequence = LoadSequence(Params, OutResult);
  if (!Sequence) return true;
  // No resolvable actorName/bindingGuid still yields an unbound master track.
  const FGuid Guid = ResolveRequestBinding(Params, Sequence);
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
}

}
