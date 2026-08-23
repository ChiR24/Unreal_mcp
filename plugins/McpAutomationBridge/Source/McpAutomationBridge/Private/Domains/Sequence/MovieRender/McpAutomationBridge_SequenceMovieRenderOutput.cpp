#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/McpAutomationBridge_SequencePathSecurity.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceFrameRate.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderInternal.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderResourceLimits.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/FrameRate.h"
#include "MoviePipelineOutputSetting.h"
#include MCP_MOVIE_PIPELINE_CONFIG_HEADER
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"

namespace McpSequenceMovieRender {
namespace {
bool TryGetInt(const TSharedPtr<FJsonObject> &Payload, const TCHAR *Name,
               int32 &Out) {
  return Payload.IsValid() && Payload->TryGetNumberField(Name, Out);
}

bool TryGetSettingsInt(const TSharedPtr<FJsonObject> &Payload, const TCHAR *Name,
                       int32 &Out) {
  const TSharedPtr<FJsonObject> *Settings = nullptr;
  return Payload.IsValid() && Payload->TryGetObjectField(TEXT("settings"), Settings) &&
         Settings && Settings->IsValid() && (*Settings)->TryGetNumberField(Name, Out);
}

bool ParseResolution(const FString &Text, FIntPoint &Out) {
  FString Left, Right;
  if (!Text.Split(TEXT("x"), &Left, &Right) &&
      !Text.Split(TEXT("X"), &Left, &Right))
    return false;
  Out.X = FCString::Atoi(*Left);
  Out.Y = FCString::Atoi(*Right);
  return Out.X > 0 && Out.Y > 0;
}

struct FOutputSettingsSnapshot {
  FString OutputDirectoryPath;
  FString FileNameFormat;
  FIntPoint OutputResolution;
  bool bUseCustomFrameRate;
  FFrameRate OutputFrameRate;
  bool bUseCustomPlaybackRange;
  int32 CustomStartFrame;
  int32 CustomEndFrame;
  int32 HandleFrameCount;
  int32 ZeroPadFrameNumbers;

  explicit FOutputSettingsSnapshot(const UMoviePipelineOutputSetting *Output)
      : OutputDirectoryPath(Output->OutputDirectory.Path),
        FileNameFormat(Output->FileNameFormat),
        OutputResolution(Output->OutputResolution),
        bUseCustomFrameRate(Output->bUseCustomFrameRate),
        OutputFrameRate(Output->OutputFrameRate),
        bUseCustomPlaybackRange(Output->bUseCustomPlaybackRange),
        CustomStartFrame(Output->CustomStartFrame),
        CustomEndFrame(Output->CustomEndFrame),
        HandleFrameCount(Output->HandleFrameCount),
        ZeroPadFrameNumbers(Output->ZeroPadFrameNumbers) {}

  void Restore(UMoviePipelineOutputSetting *Output) const {
    Output->OutputDirectory.Path = OutputDirectoryPath;
    Output->FileNameFormat = FileNameFormat;
    Output->OutputResolution = OutputResolution;
    Output->bUseCustomFrameRate = bUseCustomFrameRate;
    Output->OutputFrameRate = OutputFrameRate;
    Output->bUseCustomPlaybackRange = bUseCustomPlaybackRange;
    Output->CustomStartFrame = CustomStartFrame;
    Output->CustomEndFrame = CustomEndFrame;
    Output->HandleFrameCount = HandleFrameCount;
    Output->ZeroPadFrameNumbers = ZeroPadFrameNumbers;
  }
};

}

UMoviePipelineOutputSetting *ApplyOutputSettings(
    UMoviePipelineExecutorJob *Job, const TSharedPtr<FJsonObject> &Payload,
    FString &OutMessage, FString &OutCode) {
  if (!ValidateOutputSettingsPayload(Payload, OutMessage, OutCode))
    return nullptr;
  MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config = ResolveConfig(Job, OutMessage, OutCode);
  if (!Config)
    return nullptr;
  UMoviePipelineOutputSetting *Output = Cast<UMoviePipelineOutputSetting>(
      Config->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass(),
                                      true));
  if (!Output) {
    OutMessage = TEXT("Movie Render Queue output setting is unavailable.");
    OutCode = TEXT("MRQ_OUTPUT_UNAVAILABLE");
    return nullptr;
  }
  const FOutputSettingsSnapshot Snapshot(Output);

  FString TextValue;
  if (Payload.IsValid() &&
      Payload->TryGetStringField(TEXT("outputDirectory"), TextValue) &&
      !TextValue.IsEmpty()) {
    FString ResolvedDirectory;
    FString ValidationError;
    if (!ValidateRenderOutputDirectory(TextValue, ResolvedDirectory,
                                       ValidationError)) {
      OutMessage = ValidationError;
      OutCode = TEXT("MRQ_OUTPUT_PATH_NOT_ALLOWED");
      return nullptr;
    }
    Output->OutputDirectory.Path = ResolvedDirectory;
  }
  if (Payload.IsValid() &&
      Payload->TryGetStringField(TEXT("fileNameFormat"), TextValue) &&
      !TextValue.IsEmpty())
    Output->FileNameFormat = TextValue;

  FIntPoint Resolution = Output->OutputResolution;
  if (Payload.IsValid() &&
      Payload->TryGetStringField(TEXT("resolution"), TextValue) &&
      !TextValue.IsEmpty()) {
    if (!ParseResolution(TextValue, Resolution)) {
      OutMessage = TEXT("resolution must use WIDTHxHEIGHT format.");
      OutCode = TEXT("INVALID_RESOLUTION");
      return nullptr;
    }
    Output->OutputResolution = Resolution;
  }
  int32 Width = 0, Height = 0;
  if (TryGetInt(Payload, TEXT("width"), Width) &&
      TryGetInt(Payload, TEXT("height"), Height)) {
    if (Width <= 0 || Height <= 0) {
      OutMessage = TEXT("width and height must be positive.");
      OutCode = TEXT("INVALID_RESOLUTION");
      return nullptr;
    }
    Output->OutputResolution = FIntPoint(Width, Height);
  }

  FFrameRate FrameRate;
  FString FrameRateError;
  if (Payload.IsValid() && Payload->HasField(TEXT("frameRate")) &&
      McpSequenceFrameRate::TryParse(
          Payload, TEXT("frameRate"), FrameRate, FrameRateError)) {
    Output->bUseCustomFrameRate = true;
    Output->OutputFrameRate = FrameRate;
  }

  int32 StartFrame = 0, EndFrame = 0;
  if (TryGetInt(Payload, TEXT("startFrame"), StartFrame) &&
      TryGetInt(Payload, TEXT("endFrame"), EndFrame)) {
    // The MRQ playback range is END-EXCLUSIVE, so endFrame == startFrame is an
    // empty render, not a one-frame render. Accepting it queued a job that
    // produced no frames and reported success.
    if (EndFrame <= StartFrame) {
      OutMessage = TEXT("endFrame must be greater than startFrame. The range is "
                        "end-exclusive, so one frame is startFrame..startFrame+1.");
      OutCode = TEXT("INVALID_FRAME_RANGE");
      return nullptr;
    }
    Output->bUseCustomPlaybackRange = true;
    Output->CustomStartFrame = StartFrame;
    Output->CustomEndFrame = EndFrame;
  }

  int32 Value = 0;
  if (TryGetSettingsInt(Payload, TEXT("handleFrameCount"), Value))
    Output->HandleFrameCount = FMath::Max(0, Value);
  if (TryGetSettingsInt(Payload, TEXT("zeroPadFrameNumbers"), Value))
    Output->ZeroPadFrameNumbers = Value;
  if (!ValidateJobResourceLimits(Job, OutMessage, OutCode)) {
    Snapshot.Restore(Output);
    return nullptr;
  }
  Job->Modify();
  Config->Modify();
  return Output;
}

bool HandleConfigureOutputSettings(UMcpAutomationBridgeSubsystem *Subsystem,
                                   const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString Message, Code;
  UMoviePipelineQueueSubsystem *QueueSubsystem =
      GetQueueSubsystem(Message, Code);
  if (!QueueSubsystem)
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  UMoviePipelineQueue *Queue = QueueSubsystem->GetQueue();
  UMoviePipelineExecutorJob *Job =
      ResolveJob(Payload, Queue, Message, Code);
  if (!Job)
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  if (!ApplyOutputSettings(Job, Payload, Message, Code))
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  MCP_SET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue, true);
  Subsystem->SendAutomationResponse(
      Socket, RequestId, true,
      TEXT("Movie Render Queue output settings configured."),
      BuildJobResult(Job, Queue));
  return true;
}
}

#endif
