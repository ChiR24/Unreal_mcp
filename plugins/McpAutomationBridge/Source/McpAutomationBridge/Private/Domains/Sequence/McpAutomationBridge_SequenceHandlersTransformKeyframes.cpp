#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

namespace McpSequenceKeyframes {
namespace {
bool ReadPoint(const TSharedPtr<FJsonObject> &Object, FVector &OutPoint) {
  return Object->TryGetNumberField(TEXT("x"), OutPoint.X) && Object->TryGetNumberField(TEXT("y"), OutPoint.Y) &&
         Object->TryGetNumberField(TEXT("z"), OutPoint.Z);
}

// value.lookAt aims the key: pitch and yaw from its location toward the point, roll as given (else 0). The yaw is
// wound to within half a turn of the key before it, so an orbit of aimed keys turns the short way round.
bool AimAtLookTarget(const TSharedPtr<FJsonObject> &Value, const FMovieSceneDoubleChannel &YawChannel,
                     FFrameNumber TickFrame) {
  const TSharedPtr<FJsonObject> *LookAt = nullptr;
  if (!Value->TryGetObjectField(TEXT("lookAt"), LookAt)) {
    return true;
  }
  const TSharedPtr<FJsonObject> *Location = nullptr;
  FVector From, To;
  if (!Value->TryGetObjectField(TEXT("location"), Location) || !ReadPoint(*Location, From) || !ReadPoint(*LookAt, To)) {
    return false;
  }
  const FRotator Aim = (To - From).Rotation();
  double Yaw = Aim.Yaw;
  const TArrayView<const FFrameNumber> Times = YawChannel.GetTimes();
  const TArrayView<const FMovieSceneDoubleValue> Yaws = YawChannel.GetValues();
  for (int32 Index = Times.Num() - 1; Index >= 0; --Index) {
    if (Times[Index] < TickFrame) {
      Yaw = Yaws[Index].Value + FMath::UnwindDegrees(Yaw - Yaws[Index].Value);
      break;
    }
  }
  double Roll = 0.0;
  const TSharedPtr<FJsonObject> *Given = nullptr;
  if (Value->TryGetObjectField(TEXT("rotation"), Given)) {
    (*Given)->TryGetNumberField(TEXT("roll"), Roll);
  }
  TSharedPtr<FJsonObject> Rotation = MakeShared<FJsonObject>();
  Rotation->SetNumberField(TEXT("roll"), Roll);
  Rotation->SetNumberField(TEXT("pitch"), Aim.Pitch);
  Rotation->SetNumberField(TEXT("yaw"), Yaw);
  Value->SetObjectField(TEXT("rotation"), Rotation);
  return true;
}
}

bool AddTransformKeyframe(UMovieScene *MovieScene, const FGuid &BindingGuid,
                          FFrameNumber TickFrame,
                          const TSharedPtr<FJsonObject> &LocalPayload,
                          const USceneComponent *Bound) {
  UMovieScene3DTransformTrack *Track =
      MovieScene->FindTrack<UMovieScene3DTransformTrack>(BindingGuid,
                                                         FName("Transform"));
  if (!Track) {
    Track = MovieScene->AddTrack<UMovieScene3DTransformTrack>(BindingGuid);
  }

  if (Track) {
    bool bSectionAdded = false;
    UMovieScene3DTransformSection *Section =
        Cast<UMovieScene3DTransformSection>(FindOrAddKeySection(MovieScene, Track, TickFrame, &bSectionAdded));
    if (Section) {
      bool bModified = false;
      const TSharedPtr<FJsonObject> *ValueObj = nullptr;
      FMovieSceneChannelProxy &Proxy = Section->GetChannelProxy();
      TArrayView<FMovieSceneDoubleChannel *> Channels =
          Proxy.GetChannels<FMovieSceneDoubleChannel>();
      if (bSectionAdded && Bound && Channels.Num() >= 9) {
        // A new section held the origin, no rotation and unit scale in every channel a key left out, so keying a
        // scaled actor's position reset its scale to 1; Sequencer starts the section from what the actor holds.
        const FVector Location = Bound->GetRelativeLocation();
        const FRotator Rotation = Bound->GetRelativeRotation();
        const FVector Scale = Bound->GetRelativeScale3D();
        const double Current[9] = {Location.X, Location.Y, Location.Z, Rotation.Roll, Rotation.Pitch,
                                   Rotation.Yaw, Scale.X, Scale.Y, Scale.Z};
        for (int32 Index = 0; Index < 9; ++Index) {
          Channels[Index]->SetDefault(Current[Index]);
        }
      }
      ERichCurveInterpMode Interpolation = RCIM_Cubic;
      ReadKeyInterpolation(LocalPayload, Interpolation);

      if (LocalPayload->TryGetObjectField(TEXT("value"), ValueObj) &&
          ValueObj && Channels.Num() >= 9 && AimAtLookTarget(*ValueObj, *Channels[5], TickFrame)) {
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
            FMovieSceneDoubleValue Key(Value);
            Key.InterpMode = Interpolation;
            // A second key on the same frame used to be added beside the first, so re-keying a frame left two
            // values there and the evaluated one was arbitrary; Sequencer replaces it.
            Channels[Index]->GetData().UpdateOrAddKey(TickFrame, Key);
            // Sequencer recomputes smooth tangents after every key it adds; keys written without it kept flat
            // ones, so a move through several keys stopped dead at each of them.
            Channels[Index]->AutoSetTangents();
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
