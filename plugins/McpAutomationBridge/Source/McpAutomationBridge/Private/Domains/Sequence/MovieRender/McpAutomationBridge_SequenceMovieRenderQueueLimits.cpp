#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderResourceLimits.h"

#include "LevelSequence.h"
#include "McpAutomationBridgeSettings.h"
#include "Misc/FrameRate.h"
#include "MoviePipelineAntiAliasingSetting.h"
#include "MoviePipelineOutputSetting.h"
#include MCP_MOVIE_PIPELINE_CONFIG_HEADER
#include "MoviePipelineQueue.h"
#include "MovieScene.h"

namespace McpSequenceMovieRender {
namespace {
constexpr int32 MaximumEffectiveFrameRate = 240;

bool QueueResourceLimitExceeded(const FString &Message, FString &OutMessage,
                                FString &OutCode) {
  OutMessage = Message;
  OutCode = TEXT("MRQ_RESOURCE_LIMIT_EXCEEDED");
  return false;
}

int64 SaturatingMultiply(int64 Left, int64 Right) {
  if (Left <= 0 || Right <= 0)
    return 0;
  return Left > MAX_int64 / Right ? MAX_int64 : Left * Right;
}

int64 SaturatingAdd(int64 Left, int64 Right) {
  if (Left < 0 || Right < 0)
    return MAX_int64;
  return Left > MAX_int64 - Right ? MAX_int64 : Left + Right;
}

int64 EstimateJobWork(UMoviePipelineExecutorJob *Job) {
  if (!Job || !Job->Sequence.TryLoad())
    return 0;
  MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config =
      Job->GetConfiguration();
  if (!Config)
    return 0;
  UMoviePipelineOutputSetting *Output =
      Cast<UMoviePipelineOutputSetting>(Config->FindSettingByClass(
          UMoviePipelineOutputSetting::StaticClass(), true));
  const int64 Pixels =
      Output ? SaturatingMultiply(Output->OutputResolution.X,
                                  Output->OutputResolution.Y)
             : 1;
  const int64 Frames = ResolveMovieRenderFrameCount(Job, Output);
  UMoviePipelineAntiAliasingSetting *AA =
      Cast<UMoviePipelineAntiAliasingSetting>(Config->FindSettingByClass(
          UMoviePipelineAntiAliasingSetting::StaticClass(), true));
  const int64 Samples =
      AA ? SaturatingMultiply(FMath::Max(1, AA->SpatialSampleCount),
                              FMath::Max(1, AA->TemporalSampleCount))
         : 1;
  return SaturatingMultiply(SaturatingMultiply(Pixels, Frames), Samples);
}
}

bool IsMovieRenderEffectiveFrameRateAllowed(
    const FFrameRate &EffectiveOutputRate, int32 MaximumFrameRate) {
  if (!EffectiveOutputRate.IsValid() ||
      EffectiveOutputRate.Numerator <= 0) {
    return false;
  }
  const double FramesPerSecond = EffectiveOutputRate.AsDecimal();
  return FMath::IsFinite(FramesPerSecond) && FramesPerSecond > 0.0 &&
         FramesPerSecond <= FMath::Max(1, MaximumFrameRate);
}

int64 CalculateMovieRenderFrameCount(
    int64 SourceFrameCount, const FFrameRate &SourceRate,
    const FFrameRate &TickResolution,
    const FFrameRate &EffectiveOutputRate) {
  if (SourceFrameCount <= 0 || SourceFrameCount > MAX_int32 ||
      !SourceRate.IsValid() || SourceRate.Numerator <= 0 ||
      !TickResolution.IsValid() || TickResolution.Numerator <= 0 ||
      !EffectiveOutputRate.IsValid() ||
      EffectiveOutputRate.Numerator <= 0) {
    return MAX_int64;
  }
  const double SourceFps = SourceRate.AsDecimal();
  const double TickFps = TickResolution.AsDecimal();
  const double OutputFps = EffectiveOutputRate.AsDecimal();
  const double EstimatedTicks =
      static_cast<double>(SourceFrameCount) / SourceFps * TickFps;
  const double EstimatedFrames =
      static_cast<double>(SourceFrameCount) / SourceFps * OutputFps;
  if (!FMath::IsFinite(SourceFps) || !FMath::IsFinite(TickFps) ||
      !FMath::IsFinite(OutputFps) || SourceFps <= 0.0 || TickFps <= 0.0 ||
      OutputFps <= 0.0 || !FMath::IsFinite(EstimatedTicks) ||
      EstimatedTicks > MAX_int32 || !FMath::IsFinite(EstimatedFrames) ||
      EstimatedFrames > MAX_int32) {
    return MAX_int64;
  }
  const FFrameTime TickDuration = FFrameRate::TransformTime(
      FFrameTime(FFrameNumber(static_cast<int32>(SourceFrameCount))),
      SourceRate, TickResolution);
  const FFrameTime OutputDuration = FFrameRate::TransformTime(
      TickDuration, TickResolution, EffectiveOutputRate);
  return FMath::Max<int64>(1, OutputDuration.CeilToFrame().Value);
}

int64 ResolveMovieRenderFrameCount(UMoviePipelineExecutorJob *Job,
                                   UMoviePipelineOutputSetting *Output) {
  ULevelSequence *Sequence =
      Job ? Cast<ULevelSequence>(Job->Sequence.TryLoad()) : nullptr;
  UMovieScene *MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
  MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config =
      Job ? Job->GetConfiguration() : nullptr;
  if (!MovieScene || !Config)
    return MAX_int64;
  const FFrameRate TickResolution = MovieScene->GetTickResolution();
  const FFrameRate DisplayRate = MovieScene->GetDisplayRate();
  const FFrameRate EffectiveOutputRate =
      Config->GetEffectiveFrameRate(Sequence);
  const UMcpAutomationBridgeSettings *Settings =
      GetDefault<UMcpAutomationBridgeSettings>();
  const int32 MaximumFrameRate = FMath::Clamp(
      Settings ? Settings->MaxMovieRenderEffectiveFrameRate
               : MaximumEffectiveFrameRate,
      1, MaximumEffectiveFrameRate);
  if (!IsMovieRenderEffectiveFrameRateAllowed(EffectiveOutputRate,
                                               MaximumFrameRate)) {
    return MAX_int64;
  }

  int64 Frames = MAX_int64;
  if (Output && Output->bUseCustomPlaybackRange) {
    const int64 DisplayFrames =
        static_cast<int64>(Output->CustomEndFrame) -
        Output->CustomStartFrame + 1;
    Frames = CalculateMovieRenderFrameCount(
        DisplayFrames, DisplayRate, TickResolution, EffectiveOutputRate);
  } else {
    const TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
    if (!Range.HasLowerBound() || !Range.HasUpperBound())
      return MAX_int64;
    const int64 TickFrames =
        static_cast<int64>(Range.GetUpperBoundValue().Value) -
        Range.GetLowerBoundValue().Value;
    Frames = CalculateMovieRenderFrameCount(
        TickFrames, TickResolution, TickResolution, EffectiveOutputRate);
  }
  if (Frames == MAX_int64)
    return MAX_int64;
  const int64 HandleFrames =
      Output ? SaturatingMultiply(FMath::Max(0, Output->HandleFrameCount), 2)
             : 0;
  return SaturatingAdd(Frames, HandleFrames);
}

bool ValidateQueueResourceLimits(UMoviePipelineQueue *Queue,
                                 FString &OutMessage, FString &OutCode) {
  const UMcpAutomationBridgeSettings *Settings =
      GetDefault<UMcpAutomationBridgeSettings>();
  if (!Queue || !Settings)
    return QueueResourceLimitExceeded(
        TEXT("Movie Render Queue security settings are unavailable."),
        OutMessage, OutCode);
  const int32 MaxJobs = FMath::Max(1, Settings->MaxMovieRenderEnabledJobs);
  const int32 MaxQueueJobs =
      FMath::Max(MaxJobs, Settings->MaxMovieRenderQueueJobs);
  if (Queue->GetJobs().Num() > MaxQueueJobs)
    return QueueResourceLimitExceeded(
        TEXT("MRQ total job count exceeds the configured queue limit."),
        OutMessage, OutCode);
  const int64 MaxWork =
      FMath::Max<int64>(1, Settings->MaxMovieRenderAggregateWork);
  int32 EnabledJobs = 0;
  int64 AggregateWork = 0;
  for (UMoviePipelineExecutorJob *Job : Queue->GetJobs()) {
    if (!Job || !Job->IsEnabled())
      continue;
    if (++EnabledJobs > MaxJobs)
      return QueueResourceLimitExceeded(
          TEXT("MRQ enabled job count exceeds the configured limit."),
          OutMessage, OutCode);
    const int64 JobWork = EstimateJobWork(Job);
    if (JobWork == MAX_int64 || AggregateWork > MaxWork - JobWork)
      return QueueResourceLimitExceeded(
          TEXT("MRQ aggregate render work exceeds the configured limit."),
          OutMessage, OutCode);
    AggregateWork += JobWork;
  }
  return true;
}
}

#endif
