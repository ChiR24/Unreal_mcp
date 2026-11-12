#include "Domains/Sequence/Validation/McpSeqFrameMath.h"

#include "MovieScene.h"

namespace McpSequenceFrameMath {
namespace {
// Value as an int32 frame: rounded to the nearest, or down with bFloor.
bool TryInt32Frame(double Value, bool bFloor, int32 &OutValue, FString &OutError) {
  if (!FMath::IsFinite(Value)) {
    OutError = TEXT("Frame value must be finite.");
    return false;
  }
  const double Whole = bFloor ? FMath::FloorToDouble(Value) : FMath::RoundToDouble(Value);
  if (Whole < static_cast<double>(MIN_int32) ||
      Whole > static_cast<double>(MAX_int32)) {
    OutError = TEXT("Frame value exceeds the supported int32 range.");
    return false;
  }
  OutValue = static_cast<int32>(Whole);
  return true;
}

bool ValidateRate(const FFrameRate &Rate, FString &OutError) {
  if (!Rate.IsValid() || Rate.Numerator <= 0 || Rate.Denominator <= 0) {
    OutError = TEXT("Frame rate must be positive and valid.");
    return false;
  }
  return true;
}

// Value (a frame at SourceRate, rounded) at DestinationRate: rounded to the nearest frame, or down with bFloor.
bool TryScaleFrame(double Value, const FFrameRate &SourceRate, const FFrameRate &DestinationRate, bool bFloor,
                   FFrameNumber &OutFrame, FString &OutError) {
  FFrameNumber SourceFrame;
  if (!TryFrameNumber(Value, SourceFrame, OutError) ||
      !ValidateRate(SourceRate, OutError) ||
      !ValidateRate(DestinationRate, OutError)) {
    return false;
  }
  const long double Scaled =
      static_cast<long double>(SourceFrame.Value) *
      static_cast<long double>(DestinationRate.Numerator) *
      static_cast<long double>(SourceRate.Denominator) /
      static_cast<long double>(SourceRate.Numerator) /
      static_cast<long double>(DestinationRate.Denominator);
  int32 Whole = 0;
  if (!TryInt32Frame(static_cast<double>(Scaled), bFloor, Whole, OutError)) {
    return false;
  }
  OutFrame = FFrameNumber(Whole);
  return true;
}
}

bool TryFrameNumber(double Value, FFrameNumber &OutFrame, FString &OutError) {
  int32 Rounded = 0;
  if (!TryInt32Frame(Value, false, Rounded, OutError)) {
    return false;
  }
  OutFrame = FFrameNumber(Rounded);
  return true;
}

bool TryTransformFrame(double Value, const FFrameRate &SourceRate,
                       const FFrameRate &DestinationRate,
                       FFrameNumber &OutFrame, FString &OutError) {
  return TryScaleFrame(Value, SourceRate, DestinationRate, false, OutFrame, OutError);
}

bool TryTransformFrameFloor(double Value, const FFrameRate &SourceRate,
                            const FFrameRate &DestinationRate,
                            FFrameNumber &OutFrame, FString &OutError) {
  return TryScaleFrame(Value, SourceRate, DestinationRate, true, OutFrame, OutError);
}

bool TryAddFrames(int32 Left, int32 Right, int32 &OutValue,
                  FString &OutError) {
  const int64 Sum = static_cast<int64>(Left) + static_cast<int64>(Right);
  if (Sum < MIN_int32 || Sum > MAX_int32) {
    OutError = TEXT("Frame range endpoint exceeds the supported int32 range.");
    return false;
  }
  OutValue = static_cast<int32>(Sum);
  return true;
}

bool TryAddFrames(FFrameNumber Left, int32 Right, FFrameNumber &OutValue,
                  FString &OutError) {
  int32 Sum = 0;
  if (!TryAddFrames(Left.Value, Right, Sum, OutError)) {
    return false;
  }
  OutValue = FFrameNumber(Sum);
  return true;
}

bool TrySubtractFrames(int32 End, int32 Start, int32 &OutValue,
                       FString &OutError) {
  const int64 Difference =
      static_cast<int64>(End) - static_cast<int64>(Start);
  if (Difference < MIN_int32 || Difference > MAX_int32) {
    OutError = TEXT("Frame duration exceeds the supported int32 range.");
    return false;
  }
  OutValue = static_cast<int32>(Difference);
  return true;
}

bool TrySecondsToFrame(double Seconds, const FFrameRate &Rate,
                       FFrameNumber &OutFrame, FString &OutError) {
  if (!FMath::IsFinite(Seconds) || !ValidateRate(Rate, OutError)) {
    if (!FMath::IsFinite(Seconds)) {
      OutError = TEXT("Time value must be finite.");
    }
    return false;
  }
  const long double Scaled =
      static_cast<long double>(Seconds) *
      static_cast<long double>(Rate.Numerator) /
      static_cast<long double>(Rate.Denominator);
  return TryFrameNumber(static_cast<double>(Scaled), OutFrame, OutError);
}

bool CheckPlaybackTime(const UMovieScene *MovieScene, double Seconds, FString &OutError) {
  if (!FMath::IsFinite(Seconds) || Seconds < 0.0) {
    OutError = TEXT("startTime must be a non-negative number of seconds");
    return false;
  }
  if (!MovieScene || !MovieScene->GetPlaybackRange().HasUpperBound()) {
    return true;
  }
  // A time past the end held a frame the sequence never plays while the reply named it as reached; a frame number
  // sent as seconds lands there.
  const FFrameRate Display = MovieScene->GetDisplayRate();
  const FFrameNumber End = MovieScene->GetPlaybackRange().GetUpperBoundValue();
  const double EndSeconds = MovieScene->GetTickResolution().AsSeconds(FFrameTime(End));
  if (Seconds <= EndSeconds) {
    return true;
  }
  OutError = FString::Printf(TEXT("startTime %g s is past the end of the sequence (%g s, frame %d). startTime is in "
                                  "seconds: if %g is a frame number, send %g."),
                             Seconds, EndSeconds, Display.AsFrameNumber(EndSeconds).Value, Seconds,
                             Seconds / Display.AsDecimal());
  return false;
}

bool ValidateCinematicFrameRequest(
    const TSharedPtr<FJsonObject> &Payload, const UMovieScene *MovieScene,
    FString &OutError, int32 DefaultDuration) {
  if (!MovieScene) {
    OutError = TEXT("MovieScene is required for frame validation.");
    return false;
  }
  double StartValue = 0.0;
  Payload->TryGetNumberField(TEXT("startFrame"), StartValue);
  FFrameNumber StartTick;
  if (!TryTransformFrame(StartValue, MovieScene->GetDisplayRate(),
                         MovieScene->GetTickResolution(), StartTick,
                         OutError)) {
    return false;
  }

  int32 DurationDisplay = DefaultDuration;
  double DurationValue = 0.0;
  if (Payload->TryGetNumberField(TEXT("durationFrames"), DurationValue)) {
    FFrameNumber DurationFrame;
    if (!TryFrameNumber(DurationValue, DurationFrame, OutError)) {
      return false;
    }
    DurationDisplay = DurationFrame.Value;
  } else {
    double EndValue = 0.0;
    if (Payload->TryGetNumberField(TEXT("endFrame"), EndValue)) {
      FFrameNumber EndFrame;
      FFrameNumber StartFrame;
      if (!TryFrameNumber(EndValue, EndFrame, OutError) ||
          !TryFrameNumber(StartValue, StartFrame, OutError) ||
          !TrySubtractFrames(EndFrame.Value, StartFrame.Value,
                             DurationDisplay, OutError)) {
        return false;
      }
    }
  }

  FFrameNumber DurationTick;
  if (!TryTransformFrame(DurationDisplay, MovieScene->GetDisplayRate(),
                         MovieScene->GetTickResolution(), DurationTick,
                         OutError)) {
    return false;
  }
  FFrameNumber EndTick;
  return TryAddFrames(StartTick, FMath::Max(1, DurationTick.Value), EndTick,
                      OutError);
}

}
