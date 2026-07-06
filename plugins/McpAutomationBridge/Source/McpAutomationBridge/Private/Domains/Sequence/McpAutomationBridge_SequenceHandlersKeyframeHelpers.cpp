#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

namespace McpSequenceKeyframes {
FGuid ResolveBindingGuid(UMovieScene *MovieScene, const FString &BindingIdStr,
                         const FString &ActorName) {
  FGuid BindingGuid;
  if (!BindingIdStr.IsEmpty()) {
    FGuid::Parse(BindingIdStr, BindingGuid);
  } else if (!ActorName.IsEmpty()) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      FString BindingName;
      if (FMovieScenePossessable *Possessable =
              MovieScene->FindPossessable(Binding.GetObjectGuid())) {
        BindingName = Possessable->GetName();
      } else if (FMovieSceneSpawnable *Spawnable =
                     MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
        BindingName = Spawnable->GetName();
      }

      if (BindingName.Equals(ActorName, ESearchCase::IgnoreCase)) {
        BindingGuid = Binding.GetObjectGuid();
        break;
      }
    }
  }
  return BindingGuid;
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
    if (Track) {
      bool bSectionAdded = false;
      UMovieSceneFloatSection *Section = Cast<UMovieSceneFloatSection>(
          Track->FindOrAddSection(0, bSectionAdded));
      if (Section) {
        FMovieSceneFloatChannel *Channel =
            Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
        if (Channel) {
          Channel->GetData().UpdateOrAddKey(
              TickFrame, FMovieSceneFloatValue((float)Val->AsNumber()));
          MovieScene->Modify();
          OutMessage = TEXT("Float Keyframe added");
          return true;
        }
      }
    }
  } else if (Val.IsValid() && Val->Type == EJson::Boolean) {
    UMovieSceneBoolTrack *Track =
        MovieScene->FindTrack<UMovieSceneBoolTrack>(BindingGuid,
                                                    FName(*PropertyName));
    if (!Track) {
      Track = MovieScene->AddTrack<UMovieSceneBoolTrack>(BindingGuid);
      if (Track)
        Track->SetPropertyNameAndPath(FName(*PropertyName), PropertyName);
    }
    if (Track) {
      bool bSectionAdded = false;
      UMovieSceneBoolSection *Section = Cast<UMovieSceneBoolSection>(
          Track->FindOrAddSection(0, bSectionAdded));
      if (Section) {
        FMovieSceneBoolChannel *Channel =
            Section->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0);
        if (Channel) {
          Channel->GetData().UpdateOrAddKey(TickFrame, Val->AsBool());
          MovieScene->Modify();
          OutMessage = TEXT("Bool Keyframe added");
          return true;
        }
      }
    }
  }
  return false;
}
}
