#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"

#include "MovieScene.h"

namespace McpSequenceFrameMath {
namespace {
bool TryRoundedInt32(double Value, int32 &OutValue, FString &OutError) {
  if (!FMath::IsFinite(Value)) {
    OutError = TEXT("Frame value must be finite.");
    return false;
  }
  const double Rounded = FMath::RoundToDouble(Value);
  if (Rounded < static_cast<double>(MIN_int32) ||
      Rounded > static_cast<double>(MAX_int32)) {
    OutError = TEXT("Frame value exceeds the supported int32 range.");
    return false;
  }
  OutValue = static_cast<int32>(Rounded);
  return true;
}

bool TryFlooredInt32(double Value, int32 &OutValue, FString &OutError) {
  if (!FMath::IsFinite(Value)) {
    OutError = TEXT("Frame value must be finite.");
    return false;
  }
  const double Floored = FMath::FloorToDouble(Value);
  if (Floored < static_cast<double>(MIN_int32) ||
      Floored > static_cast<double>(MAX_int32)) {
    OutError = TEXT("Frame value exceeds the supported int32 range.");
    return false;
  }
  OutValue = static_cast<int32>(Floored);
  return true;
}

bool ValidateRate(const FFrameRate &Rate, FString &OutError) {
  if (!Rate.IsValid() || Rate.Numerator <= 0 || Rate.Denominator <= 0) {
    OutError = TEXT("Frame rate must be positive and valid.");
    return false;
  }
  return true;
}
}

bool TryFrameNumber(double Value, FFrameNumber &OutFrame, FString &OutError) {
  int32 Rounded = 0;
  if (!TryRoundedInt32(Value, Rounded, OutError)) {
    return false;
  }
  OutFrame = FFrameNumber(Rounded);
  return true;
}

bool TryTransformFrame(double Value, const FFrameRate &SourceRate,
                       const FFrameRate &DestinationRate,
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
  const double ScaledValue = static_cast<double>(Scaled);
  if (!FMath::IsFinite(ScaledValue)) {
    OutError = TEXT("Transformed frame value must be finite.");
    return false;
  }
  return TryFrameNumber(ScaledValue, OutFrame, OutError);
}

bool TryTransformFrameFloor(double Value, const FFrameRate &SourceRate,
                            const FFrameRate &DestinationRate,
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
  int32 Floored = 0;
  if (!TryFlooredInt32(static_cast<double>(Scaled), Floored, OutError)) {
    return false;
  }
  OutFrame = FFrameNumber(Floored);
  return true;
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

bool ValidateCinematicFrameRequest(
    const TSharedPtr<FJsonObject> &Payload, const UMovieScene *MovieScene,
    FString &OutError, int32 DefaultDuration) {
  if (!MovieScene) {
    OutError = TEXT("MovieScene is required for frame validation.");
    return false;
  }
  double StartValue = 0.0;
  if (Payload.IsValid()) {
    Payload->TryGetNumberField(TEXT("startFrame"), StartValue);
  }
  FFrameNumber StartTick;
  if (!TryTransformFrame(StartValue, MovieScene->GetDisplayRate(),
                         MovieScene->GetTickResolution(), StartTick,
                         OutError)) {
    return false;
  }

  int32 DurationDisplay = DefaultDuration;
  double DurationValue = 0.0;
  if (Payload.IsValid() &&
      Payload->TryGetNumberField(TEXT("durationFrames"), DurationValue)) {
    FFrameNumber DurationFrame;
    if (!TryFrameNumber(DurationValue, DurationFrame, OutError)) {
      return false;
    }
    DurationDisplay = DurationFrame.Value;
  } else {
    double EndValue = 0.0;
    if (Payload.IsValid() &&
        Payload->TryGetNumberField(TEXT("endFrame"), EndValue)) {
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
