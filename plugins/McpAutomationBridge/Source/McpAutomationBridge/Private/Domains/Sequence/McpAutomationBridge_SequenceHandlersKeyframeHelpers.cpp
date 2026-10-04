#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

#include "Components/SceneComponent.h"
#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"
#include "Tracks/MovieSceneVisibilityTrack.h"

namespace McpSequenceKeyframes {
FGuid ResolveBindingGuid(UMovieScene *MovieScene, const FString &BindingIdStr,
                         const FString &ActorName) {
  FGuid BindingGuid;
  if (!BindingIdStr.IsEmpty()) {
    FGuid::Parse(BindingIdStr, BindingGuid);
  } else if (!ActorName.IsEmpty()) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      FString BindingName = GetBindingName(MovieScene, Binding.GetObjectGuid());

      if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase)) {
        BindingGuid = Binding.GetObjectGuid();
        break;
      }
    }
  }
  return BindingGuid;
}

bool ReadKeyInterpolation(const TSharedPtr<FJsonObject> &Payload, ERichCurveInterpMode &OutMode) {
  FString Name = TEXT("auto");
  Payload->TryGetStringField(TEXT("interpolation"), Name);
  OutMode = Name.Equals(TEXT("linear"), ESearchCase::IgnoreCase)     ? RCIM_Linear
            : Name.Equals(TEXT("constant"), ESearchCase::IgnoreCase) ? RCIM_Constant
                                                                     : RCIM_Cubic;
  return OutMode != RCIM_Cubic || Name.Equals(TEXT("auto"), ESearchCase::IgnoreCase);
}

UMovieSceneSection *FindOrAddKeySection(UMovieScene *MovieScene, UMovieScenePropertyTrack *Track,
                                        FFrameNumber TickFrame, bool *bOutAdded) {
  bool bSectionAdded = false;
  UMovieSceneSection *Section = Track ? Track->FindOrAddSection(0, bSectionAdded) : nullptr;
  if (bOutAdded) {
    *bOutAdded = bSectionAdded;
  }
  if (!Section) {
    return nullptr;
  }
  Section->Modify();
  // A section added by FindOrAddSection spans the one frame it was asked for, so keys written into it covered no
  // time and never evaluated: add_keyframe reported success and nothing changed. Give a new (or empty) section
  // the sequence's playback range, and grow an existing one to cover a key that lands outside it.
  if (bSectionAdded || Section->GetRange().IsEmpty()) {
    Section->SetRange(MovieScene->GetPlaybackRange());
  }
  if (!Section->GetRange().Contains(TickFrame)) {
    Section->SetRange(TRange<FFrameNumber>::Hull(Section->GetRange(), TRange<FFrameNumber>(TickFrame, TickFrame + 1)));
  }
  return Section;
}

bool AddPropertyKeyframe(UMovieScene *MovieScene, const FGuid &BindingGuid,
                         const FString &PropertyName, FFrameNumber TickFrame,
                         const TSharedPtr<FJsonObject> &LocalPayload,
                         FString &OutMessage) {
  const TSharedPtr<FJsonValue> Val = LocalPayload->TryGetField(TEXT("value"));
  if (Val.IsValid() && Val->Type == EJson::Number) {
    UMovieSceneFloatTrack *Track =
        MovieScene->FindTrack<UMovieSceneFloatTrack>(BindingGuid,
                                                     FName(*PropertyName));
    if (!Track) {
      Track = MovieScene->AddTrack<UMovieSceneFloatTrack>(BindingGuid);
      if (Track)
        Track->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
    }
    UMovieSceneFloatSection *Section = Cast<UMovieSceneFloatSection>(FindOrAddKeySection(MovieScene, Track, TickFrame));
    FMovieSceneFloatChannel *Channel = Section ? Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0) : nullptr;
    if (!Channel) {
      return false;
    }
    ERichCurveInterpMode Interpolation = RCIM_Cubic;
    ReadKeyInterpolation(LocalPayload, Interpolation);
    FMovieSceneFloatValue Key((float)Val->AsNumber());
    Key.InterpMode = Interpolation;
    Channel->GetData().UpdateOrAddKey(TickFrame, Key);
    // Sequencer recomputes smooth tangents after every key it adds; keys written without it kept flat ones.
    Channel->AutoSetTangents();
    OutMessage = TEXT("Float Keyframe added");
  } else if (Val.IsValid() && Val->Type == EJson::Boolean) {
    // Visibility keys the binding's Visibility track (true shows it), which hides through SetActorHiddenInGame so
    // renders honour it; a bool track on the hidden flag only wrote the field and the actor stayed drawn.
    const bool bVisibility = PropertyName.Equals(TEXT("Visibility"), ESearchCase::IgnoreCase);
    // From 5.4 the Visibility track is no bool track, so the two meet at the property track.
    UMovieScenePropertyTrack *Track =
        bVisibility ? static_cast<UMovieScenePropertyTrack *>(MovieScene->FindTrack<UMovieSceneVisibilityTrack>(BindingGuid))
                    : MovieScene->FindTrack<UMovieSceneBoolTrack>(BindingGuid, FName(*PropertyName));
    if (!Track) {
      Track = bVisibility ? static_cast<UMovieScenePropertyTrack *>(MovieScene->AddTrack<UMovieSceneVisibilityTrack>(BindingGuid))
                          : MovieScene->AddTrack<UMovieSceneBoolTrack>(BindingGuid);
      // Named as Sequencer names its own: an actor's bHidden, a component's bHiddenInGame.
      const bool bComponent = Cast<USceneComponent>(McpSequenceCinematics::GetBindingTemplate(MovieScene, BindingGuid)) != nullptr;
      const FString Name = !bVisibility ? PropertyName : FString(bComponent ? TEXT("bHiddenInGame") : TEXT("bHidden"));
      if (Track)
        Track->SetPropertyNameAndPath(FName(*Name), Name);
    }
    UMovieSceneBoolSection *Section = Cast<UMovieSceneBoolSection>(FindOrAddKeySection(MovieScene, Track, TickFrame));
    FMovieSceneBoolChannel *Channel = Section ? Section->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0) : nullptr;
    if (!Channel) {
      return false;
    }
    Channel->GetData().UpdateOrAddKey(TickFrame, Val->AsBool());
    OutMessage = bVisibility ? TEXT("Visibility Keyframe added") : TEXT("Bool Keyframe added");
  } else {
    return false;
  }
  MovieScene->Modify();
  MovieScene->MarkPackageDirty();
  return true;
}
}
