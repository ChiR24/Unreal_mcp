#pragma once

#include "CoreMinimal.h"

class UMoviePipelineConsoleVariableSetting;
class UMoviePipelineExecutorJob;
class UMoviePipelineOutputSetting;
class UMoviePipelineQueue;
struct FFrameRate;

namespace McpSequenceMovieRender {

// Sets the refusal to Message / MRQ_RESOURCE_LIMIT_EXCEEDED; returns false for "return ResourceLimitExceeded(...)".
inline bool ResourceLimitExceeded(const FString &Message, FString &OutMessage,
                                  FString &OutCode) {
  OutMessage = Message;
  OutCode = TEXT("MRQ_RESOURCE_LIMIT_EXCEEDED");
  return false;
}

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
// CVars' enabled console variables added to Out (CVars may be null); false with
// MRQ_CONSOLE_COMMANDS_NOT_ALLOWED when it carries presets or start/end commands.
bool ReadAllowedConsoleVariables(UMoviePipelineConsoleVariableSetting *CVars,
                                 TMap<FString, float> &Out, FString &OutMessage,
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
