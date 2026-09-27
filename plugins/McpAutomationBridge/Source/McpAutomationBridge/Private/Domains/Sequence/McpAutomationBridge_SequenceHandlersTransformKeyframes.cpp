#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

namespace McpSequenceKeyframes {
bool AddTransformKeyframe(UMovieScene *MovieScene, const FGuid &BindingGuid,
                          FFrameNumber TickFrame,
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
      Section->Modify();
      // A section added by FindOrAddSection starts with an EMPTY range, so keys
      // written into it cover no time and never evaluate: add_keyframe reported
      // "Keyframe added" and the rendered frames came out byte-identical.
      // Give a new (or empty) section the sequence's playback range, and grow
      // an existing one to cover a key that lands outside it.
      if (bSectionAdded || Section->GetRange().IsEmpty()) {
        Section->SetRange(MovieScene->GetPlaybackRange());
      }
      if (!Section->GetRange().Contains(TickFrame)) {
        Section->SetRange(TRange<FFrameNumber>::Hull(
            Section->GetRange(),
            TRange<FFrameNumber>(TickFrame, TickFrame + 1)));
      }
      bool bModified = false;
      const TSharedPtr<FJsonObject> *ValueObj = nullptr;
      FMovieSceneChannelProxy &Proxy = Section->GetChannelProxy();
      TArrayView<FMovieSceneDoubleChannel *> Channels =
          Proxy.GetChannels<FMovieSceneDoubleChannel>();

      if (LocalPayload->TryGetObjectField(TEXT("value"), ValueObj) &&
          ValueObj && Channels.Num() >= 9) {
        // Channel order: location x/y/z, rotation roll/pitch/yaw, scale x/y/z.
        static const TCHAR *const Fields[9][2] = {
            {TEXT("location"), TEXT("x")}, {TEXT("location"), TEXT("y")}, {TEXT("location"), TEXT("z")},
            {TEXT("rotation"), TEXT("roll")}, {TEXT("rotation"), TEXT("pitch")}, {TEXT("rotation"), TEXT("yaw")},
            {TEXT("scale"), TEXT("x")}, {TEXT("scale"), TEXT("y")}, {TEXT("scale"), TEXT("z")}};
        for (int32 Index = 0; Index < 9; ++Index) {
          const TSharedPtr<FJsonObject> *Part = nullptr;
          double Value = 0.0;
          if ((*ValueObj)->TryGetObjectField(Fields[Index][0], Part) &&
              (*Part)->TryGetNumberField(Fields[Index][1], Value)) {
            Channels[Index]->GetData().AddKey(TickFrame, FMovieSceneDoubleValue(Value));
            bModified = true;
          }
        }
      }

      if (bModified) {
        MovieScene->Modify();
        // Modify() alone never got the change offered for saving (see
        // HandleSequenceRemoveTrack); without the dirty mark a key that
        // evaluated fine all session is gone on the next editor start.
        MovieScene->MarkPackageDirty();
        return true;
      }
    }
  }
  return false;
}
}
