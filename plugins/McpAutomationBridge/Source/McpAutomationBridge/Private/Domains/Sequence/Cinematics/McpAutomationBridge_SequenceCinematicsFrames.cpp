#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"

#if WITH_EDITOR
#include "MovieScene.h"
#endif

namespace McpSequenceCinematics {
#if WITH_EDITOR
FFrameNumber GetFrame(const TSharedPtr<FJsonObject> &Params,
                      UMovieScene *MovieScene, const TCHAR *Name,
                      double DefaultValue) {
  double Value = DefaultValue;
  if (Params.IsValid()) {
    Params->TryGetNumberField(Name, Value);
  }
  FFrameNumber DisplayFrame;
  FString Error;
  if (!McpSequenceFrameMath::TryFrameNumber(Value, DisplayFrame, Error)) {
    return FFrameNumber();
  }
  if (!MovieScene) {
    return DisplayFrame;
  }
  FFrameNumber TickFrame;
  return McpSequenceFrameMath::TryTransformFrame(
             DisplayFrame.Value, MovieScene->GetDisplayRate(),
             MovieScene->GetTickResolution(), TickFrame, Error)
             ? TickFrame
             : FFrameNumber();
}

int32 GetDuration(const TSharedPtr<FJsonObject> &Params, UMovieScene *MovieScene,
                  int32 DefaultDuration) {
  double Duration = DefaultDuration;
  if (Params.IsValid() &&
      !Params->TryGetNumberField(TEXT("durationFrames"), Duration)) {
    double EndFrame = 0.0;
    if (Params->TryGetNumberField(TEXT("endFrame"), EndFrame)) {
      double StartFrame = 0.0;
      Params->TryGetNumberField(TEXT("startFrame"), StartFrame);
      FFrameNumber End;
      FFrameNumber Start;
      int32 Difference = 0;
      FString Error;
      if (!McpSequenceFrameMath::TryFrameNumber(EndFrame, End, Error) ||
          !McpSequenceFrameMath::TryFrameNumber(StartFrame, Start, Error) ||
          !McpSequenceFrameMath::TrySubtractFrames(
              End.Value, Start.Value, Difference, Error)) {
        return 1;
      }
      Duration = Difference;
    }
  }
  FFrameNumber DisplayDuration;
  FString Error;
  if (!McpSequenceFrameMath::TryFrameNumber(
          Duration, DisplayDuration, Error)) {
    return 1;
  }
  FFrameNumber TickDuration = DisplayDuration;
  if (MovieScene && !McpSequenceFrameMath::TryTransformFrame(
                        DisplayDuration.Value, MovieScene->GetDisplayRate(),
                        MovieScene->GetTickResolution(), TickDuration, Error)) {
    return 1;
  }
  return FMath::Max(1, TickDuration.Value);
}
#endif
}
