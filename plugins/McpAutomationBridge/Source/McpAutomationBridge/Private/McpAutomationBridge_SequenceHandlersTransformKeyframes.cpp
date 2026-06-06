#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

namespace McpSequenceKeyframes {
bool AddTransformKeyframe(UMovieScene *MovieScene, const FGuid &BindingGuid,
                          double Frame,
                          const TSharedPtr<FJsonObject> &LocalPayload) {
  UMovieScene3DTransformTrack *Track =
      MovieScene->FindTrack<UMovieScene3DTransformTrack>(BindingGuid,
                                                         FName("Transform"));
  if (!Track) {
    Track = MovieScene->AddTrack<UMovieScene3DTransformTrack>(BindingGuid);
  }

  if (Track) {
    bool bSectionAdded = false;
    UMovieScene3DTransformSection *Section =
        Cast<UMovieScene3DTransformSection>(
            Track->FindOrAddSection(0, bSectionAdded));
    if (Section) {
      FFrameRate TickResolution = MovieScene->GetTickResolution();
      FFrameRate DisplayRate = MovieScene->GetDisplayRate();
      FFrameNumber FrameNum = FFrameNumber(static_cast<int32>(Frame));
      FFrameNumber TickFrame =
          FFrameRate::TransformTime(FFrameTime(FrameNum), DisplayRate,
                                    TickResolution)
              .FloorToFrame();

      bool bModified = false;
      const TSharedPtr<FJsonObject> *ValueObj = nullptr;
      FMovieSceneChannelProxy &Proxy = Section->GetChannelProxy();
      TArrayView<FMovieSceneDoubleChannel *> Channels =
          Proxy.GetChannels<FMovieSceneDoubleChannel>();

      if (LocalPayload->TryGetObjectField(TEXT("value"), ValueObj) &&
          ValueObj && Channels.Num() >= 9) {
        const TSharedPtr<FJsonObject> *LocObj = nullptr;
        if ((*ValueObj)->TryGetObjectField(TEXT("location"), LocObj)) {
          double X, Y, Z;
          if ((*LocObj)->TryGetNumberField(TEXT("x"), X)) {
            Channels[0]->GetData().AddKey(TickFrame,
                                          FMovieSceneDoubleValue(X));
            bModified = true;
          }
          if ((*LocObj)->TryGetNumberField(TEXT("y"), Y)) {
            Channels[1]->GetData().AddKey(TickFrame,
                                          FMovieSceneDoubleValue(Y));
            bModified = true;
          }
          if ((*LocObj)->TryGetNumberField(TEXT("z"), Z)) {
            Channels[2]->GetData().AddKey(TickFrame,
                                          FMovieSceneDoubleValue(Z));
            bModified = true;
          }
        }

        const TSharedPtr<FJsonObject> *RotObj = nullptr;
        if ((*ValueObj)->TryGetObjectField(TEXT("rotation"), RotObj)) {
          double P, Yaw, R;
          if ((*RotObj)->TryGetNumberField(TEXT("roll"), R)) {
            Channels[3]->GetData().AddKey(TickFrame,
                                          FMovieSceneDoubleValue(R));
            bModified = true;
          }
          if ((*RotObj)->TryGetNumberField(TEXT("pitch"), P)) {
            Channels[4]->GetData().AddKey(TickFrame,
                                          FMovieSceneDoubleValue(P));
            bModified = true;
          }
          if ((*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw)) {
            Channels[5]->GetData().AddKey(TickFrame,
                                          FMovieSceneDoubleValue(Yaw));
            bModified = true;
          }
        }

        const TSharedPtr<FJsonObject> *ScaleObj = nullptr;
        if ((*ValueObj)->TryGetObjectField(TEXT("scale"), ScaleObj)) {
          double X, Y, Z;
          if ((*ScaleObj)->TryGetNumberField(TEXT("x"), X)) {
            Channels[6]->GetData().AddKey(TickFrame,
                                          FMovieSceneDoubleValue(X));
            bModified = true;
          }
          if ((*ScaleObj)->TryGetNumberField(TEXT("y"), Y)) {
            Channels[7]->GetData().AddKey(TickFrame,
                                          FMovieSceneDoubleValue(Y));
            bModified = true;
          }
          if ((*ScaleObj)->TryGetNumberField(TEXT("z"), Z)) {
            Channels[8]->GetData().AddKey(TickFrame,
                                          FMovieSceneDoubleValue(Z));
            bModified = true;
          }
        }
      }

      if (bModified) {
        MovieScene->Modify();
        return true;
      }
    }
  }
  return false;
}
}
