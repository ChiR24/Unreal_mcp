#pragma once

#include "CoreMinimal.h"

class UMoviePipelineExecutorJob;
class UMoviePipelineOutputSetting;
class UMoviePipelineQueue;
struct FFrameRate;

namespace McpSequenceMovieRender {

bool ValidateResolutionResourceLimits(int32 Width, int32 Height,
                                      FString &OutMessage, FString &OutCode);
bool ValidateFrameResourceLimits(bool bHasCustomRange, int32 StartFrame,
                                 int32 EndFrame, int32 HandleFrameCount,
                                 FString &OutMessage, FString &OutCode);
bool ValidateSampleResourceLimits(int32 SpatialSamples, int32 TemporalSamples,
                                  FString &OutMessage, FString &OutCode);
bool ValidateConsoleVariableResourceLimits(
    const TMap<FString, float> &ConsoleVariables, FString &OutMessage,
    FString &OutCode);
bool ValidateRenderTimeoutResourceLimit(double TimeoutMs, FString &OutMessage,
                                        FString &OutCode);
bool ValidateJobResourceLimits(UMoviePipelineExecutorJob *Job,
                               FString &OutMessage, FString &OutCode);
int64 ResolveMovieRenderFrameCount(UMoviePipelineExecutorJob *Job,
                                   UMoviePipelineOutputSetting *Output);
int64 CalculateMovieRenderFrameCount(int64 SourceFrameCount,
                                     const FFrameRate &SourceRate,
                                     const FFrameRate &TickResolution,
                                     const FFrameRate &EffectiveOutputRate);
bool IsMovieRenderEffectiveFrameRateAllowed(
    const FFrameRate &EffectiveOutputRate, int32 MaximumFrameRate);
bool ValidateQueueResourceLimits(UMoviePipelineQueue *Queue,
                                 FString &OutMessage, FString &OutCode);

}
