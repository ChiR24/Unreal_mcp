#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderResourceLimits.h"

#include "McpAutomationBridgeSettings.h"
#include "MoviePipelineAntiAliasingSetting.h"
#include "MoviePipelineConsoleVariableSetting.h"
#include "MoviePipelineOutputSetting.h"
#include MCP_MOVIE_PIPELINE_CONFIG_HEADER
#include "MoviePipelineQueue.h"

namespace McpSequenceMovieRender {
namespace {

const UMcpAutomationBridgeSettings *SecuritySettings() {
  return GetDefault<UMcpAutomationBridgeSettings>();
}

bool ResourceLimitExceeded(const FString &Message, FString &OutMessage,
                           FString &OutCode) {
  OutMessage = Message;
  OutCode = TEXT("MRQ_RESOURCE_LIMIT_EXCEEDED");
  return false;
}

bool IsAllowlisted(const FString &Name, const TArray<FString> &Allowlist) {
  return Allowlist.ContainsByPredicate(
      [&Name](const FString &Entry) {
        return Name.Equals(Entry.TrimStartAndEnd(), ESearchCase::IgnoreCase);
      });
}

}

bool ValidateResolutionResourceLimits(int32 Width, int32 Height,
                                      FString &OutMessage, FString &OutCode) {
  const UMcpAutomationBridgeSettings *Settings = SecuritySettings();
  if (!Settings)
    return ResourceLimitExceeded(
        TEXT("Movie Render Queue security settings are unavailable."),
        OutMessage, OutCode);
  const int32 MaxDimension =
      FMath::Max(1, Settings->MaxMovieRenderResolutionDimension);
  const int64 MaxPixels =
      FMath::Max<int64>(1, Settings->MaxMovieRenderPixelCount);
  const int64 PixelCount = static_cast<int64>(Width) * Height;
  if (Width <= 0 || Height <= 0 || Width > MaxDimension ||
      Height > MaxDimension ||
      PixelCount > MaxPixels) {
    return ResourceLimitExceeded(
        FString::Printf(
            TEXT("MRQ resolution %dx%d exceeds configured resource limits."),
            Width, Height),
        OutMessage, OutCode);
  }
  return true;
}

bool ValidateFrameResourceLimits(bool bHasCustomRange, int32 StartFrame,
                                 int32 EndFrame, int32 HandleFrameCount,
                                 FString &OutMessage, FString &OutCode) {
  const UMcpAutomationBridgeSettings *Settings = SecuritySettings();
  if (!Settings)
    return ResourceLimitExceeded(
        TEXT("Movie Render Queue security settings are unavailable."),
        OutMessage, OutCode);

  // Per-shot handle frames are bounded independently of the playback range.
  if (HandleFrameCount >
      FMath::Max(0, Settings->MaxMovieRenderHandleFrameCount)) {
    return ResourceLimitExceeded(
        TEXT("MRQ handleFrameCount exceeds the configured resource limit."),
        OutMessage, OutCode);
  }

  // When a custom playback range is provided, the per-job frame count limit
  // (End - Start + 2 * HandleFrames) must not exceed MaxMovieRenderFrameCount.
  // When bHasCustomRange is false, the range is derived from the sequence's
  // playback range and is enforced by ValidateJobResourceLimits via
  // ResolveMovieRenderFrameCount.
  if (bHasCustomRange) {
    const int64 MaxFrames =
        FMath::Max<int64>(1, Settings->MaxMovieRenderFrameCount);
    const int64 RangeSpan =
        static_cast<int64>(EndFrame) - static_cast<int64>(StartFrame);
    if (RangeSpan < 0) {
      OutMessage = TEXT("MRQ custom playback range endFrame is before startFrame.");
      OutCode = TEXT("INVALID_FRAME_RANGE");
      return false;
    }
    const int64 EffectiveFrames = RangeSpan + 2 * static_cast<int64>(HandleFrameCount);
    if (EffectiveFrames > MaxFrames) {
      return ResourceLimitExceeded(
          FString::Printf(
              TEXT("MRQ custom playback range effective frame count %lld exceeds configured limit %lld."),
              EffectiveFrames, MaxFrames),
          OutMessage, OutCode);
    }
  }
  return true;
}

bool ValidateSampleResourceLimits(int32 SpatialSamples, int32 TemporalSamples,
                                  FString &OutMessage, FString &OutCode) {
  const UMcpAutomationBridgeSettings *Settings = SecuritySettings();
  if (!Settings)
    return ResourceLimitExceeded(
        TEXT("Movie Render Queue security settings are unavailable."),
        OutMessage, OutCode);
  const int32 MaxSamples =
      FMath::Max(1, Settings->MaxMovieRenderSampleCount);
  const int64 CombinedSamples =
      static_cast<int64>(SpatialSamples) * TemporalSamples;
  if (SpatialSamples <= 0 || TemporalSamples <= 0 ||
      SpatialSamples > MaxSamples || TemporalSamples > MaxSamples ||
      CombinedSamples >
          FMath::Max(1, Settings->MaxMovieRenderCombinedSampleCount)) {
    return ResourceLimitExceeded(
        TEXT("MRQ anti-aliasing samples exceed configured resource limits."),
        OutMessage, OutCode);
  }
  return true;
}

bool ValidateConsoleVariableResourceLimits(
    const TMap<FString, float> &ConsoleVariables, FString &OutMessage,
    FString &OutCode) {
  const UMcpAutomationBridgeSettings *Settings = SecuritySettings();
  if (!Settings)
    return ResourceLimitExceeded(
        TEXT("Movie Render Queue security settings are unavailable."),
        OutMessage, OutCode);
  if (ConsoleVariables.Num() >
      FMath::Max(0, Settings->MaxMovieRenderConsoleVariables)) {
    return ResourceLimitExceeded(
        TEXT("MRQ console-variable count exceeds the configured limit."),
        OutMessage, OutCode);
  }
  const float MaxMagnitude =
      FMath::Max(0.0f, Settings->MaxMovieRenderConsoleVariableMagnitude);
  for (const TPair<FString, float> &Entry : ConsoleVariables) {
    if (!IsAllowlisted(Entry.Key,
                       Settings->MovieRenderConsoleVariableAllowlist)) {
      OutMessage = FString::Printf(
          TEXT("MRQ console variable is not allowlisted: %s"), *Entry.Key);
      OutCode = TEXT("MRQ_CONSOLE_VARIABLE_NOT_ALLOWED");
      return false;
    }
    if (!FMath::IsFinite(Entry.Value) ||
        FMath::Abs(Entry.Value) > MaxMagnitude) {
      return ResourceLimitExceeded(
          FString::Printf(
              TEXT("MRQ console-variable value exceeds the configured limit: %s"),
              *Entry.Key),
          OutMessage, OutCode);
    }
  }
  return true;
}

bool ValidateRenderTimeoutResourceLimit(double TimeoutMs, FString &OutMessage,
                                        FString &OutCode) {
  const UMcpAutomationBridgeSettings *Settings = SecuritySettings();
  if (!FMath::IsFinite(TimeoutMs) || TimeoutMs <= 0.0) {
    OutMessage = TEXT("timeoutMs must be finite and positive.");
    OutCode = TEXT("INVALID_TIMEOUT");
    return false;
  }
  if (!Settings ||
      TimeoutMs > FMath::Max(1, Settings->MaxMovieRenderTimeoutMs)) {
    return ResourceLimitExceeded(
        TEXT("MRQ timeoutMs exceeds the configured resource limit."),
        OutMessage, OutCode);
  }
  return true;
}

bool ValidateJobResourceLimits(UMoviePipelineExecutorJob *Job,
                               FString &OutMessage, FString &OutCode) {
  MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config =
      Job ? Job->GetConfiguration() : nullptr;
  if (!Config)
    return true;
  UMoviePipelineOutputSetting *Output =
      Cast<UMoviePipelineOutputSetting>(Config->FindSettingByClass(
          UMoviePipelineOutputSetting::StaticClass(), true));
  if (Output &&
      (!ValidateResolutionResourceLimits(
           Output->OutputResolution.X, Output->OutputResolution.Y, OutMessage,
           OutCode) ||
       !ValidateFrameResourceLimits(
           Output->bUseCustomPlaybackRange, Output->CustomStartFrame,
           Output->CustomEndFrame, Output->HandleFrameCount, OutMessage,
           OutCode))) {
    return false;
  }
  const UMcpAutomationBridgeSettings *Settings = SecuritySettings();
  const int64 EffectiveFrameCount =
      ResolveMovieRenderFrameCount(Job, Output);
  if (EffectiveFrameCount == MAX_int64) {
    return ResourceLimitExceeded(
        TEXT("MRQ playback range or effective output frame rate is invalid."),
        OutMessage, OutCode);
  }
  if (EffectiveFrameCount >
      FMath::Max(1, Settings ? Settings->MaxMovieRenderFrameCount : 1)) {
    return ResourceLimitExceeded(
        TEXT("MRQ effective output frame count exceeds the configured resource limit."),
        OutMessage, OutCode);
  }
  UMoviePipelineAntiAliasingSetting *AA =
      Cast<UMoviePipelineAntiAliasingSetting>(Config->FindSettingByClass(
          UMoviePipelineAntiAliasingSetting::StaticClass(), true));
  if (AA &&
      !ValidateSampleResourceLimits(AA->SpatialSampleCount,
                                    AA->TemporalSampleCount, OutMessage,
                                    OutCode)) {
    return false;
  }
  UMoviePipelineConsoleVariableSetting *CVars =
      Cast<UMoviePipelineConsoleVariableSetting>(Config->FindSettingByClass(
          UMoviePipelineConsoleVariableSetting::StaticClass(), true));
  if (!CVars)
    return true;
  if (CVars->ConsoleVariablePresets.Num() > 0 ||
      CVars->StartConsoleCommands.Num() > 0 ||
      CVars->EndConsoleCommands.Num() > 0) {
    OutMessage =
        TEXT("MRQ console-variable presets and console commands are not allowed.");
    OutCode = TEXT("MRQ_CONSOLE_COMMANDS_NOT_ALLOWED");
    return false;
  }
  TMap<FString, float> Values;
  for (const FMoviePipelineConsoleVariableEntry &Entry :
       CVars->GetConsoleVariables()) {
    if (Entry.bIsEnabled)
      Values.Add(Entry.Name, Entry.Value);
  }
  return ValidateConsoleVariableResourceLimits(Values, OutMessage, OutCode);
}

}

#endif
