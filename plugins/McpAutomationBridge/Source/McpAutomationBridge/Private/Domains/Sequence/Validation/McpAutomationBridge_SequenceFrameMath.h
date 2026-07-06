#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/FrameRate.h"

class UMovieScene;

namespace McpSequenceFrameMath {

bool TryFrameNumber(double Value, FFrameNumber &OutFrame, FString &OutError);
bool TryTransformFrame(double Value, const FFrameRate &SourceRate,
                       const FFrameRate &DestinationRate,
                       FFrameNumber &OutFrame, FString &OutError);
bool TryTransformFrameFloor(double Value, const FFrameRate &SourceRate,
                            const FFrameRate &DestinationRate,
                            FFrameNumber &OutFrame, FString &OutError);
bool TryAddFrames(int32 Left, int32 Right, int32 &OutValue,
                  FString &OutError);
bool TryAddFrames(FFrameNumber Left, int32 Right, FFrameNumber &OutValue,
                  FString &OutError);
bool TrySubtractFrames(int32 End, int32 Start, int32 &OutValue,
                       FString &OutError);
bool TrySecondsToFrame(double Seconds, const FFrameRate &Rate,
                       FFrameNumber &OutFrame, FString &OutError);
bool ValidateCinematicFrameRequest(
    const TSharedPtr<FJsonObject> &Payload, const UMovieScene *MovieScene,
    FString &OutError, int32 DefaultDuration = 240);

}
