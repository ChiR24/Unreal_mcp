#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderInternal.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderResourceLimits.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "LevelSequence.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Modules/ModuleManager.h"
#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelineImageSequenceOutput.h"
#include "MoviePipelineBurnInSetting.h"
#include "MoviePipelineOutputSetting.h"
#include MCP_MOVIE_PIPELINE_CONFIG_HEADER
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"

namespace McpSequenceMovieRender {
namespace {
const TCHAR *JobIdPrefix = TEXT("mcp.renderJobId=");

FString GetString(const TSharedPtr<FJsonObject> &Payload, const TCHAR *First,
                  const TCHAR *Second = nullptr) {
  FString Value;
  if (Payload.IsValid() && Payload->TryGetStringField(First, Value) &&
      !Value.IsEmpty())
    return Value;
  return Payload.IsValid() && Second && Payload->TryGetStringField(Second, Value)
             ? Value
             : FString();
}

FString GetJobId(UMoviePipelineExecutorJob *Job) {
  if (!Job)
    return FString();
  return Job->UserData.StartsWith(JobIdPrefix)
             ? Job->UserData.RightChop(FCString::Strlen(JobIdPrefix))
             : FString();
}

}

bool LoadRequiredModule(const TCHAR *ModuleName, FString &OutMessage,
                        FString &OutCode) {
  FModuleManager &Modules = FModuleManager::Get();
  if (Modules.IsModuleLoaded(ModuleName))
    return true;
  if (Modules.ModuleExists(ModuleName) && Modules.LoadModule(ModuleName))
    return true;
  OutMessage = FString::Printf(
      TEXT("Required Movie Render Queue module is unavailable: %s"),
      ModuleName);
  OutCode = TEXT("MRQ_MODULE_UNAVAILABLE");
  return false;
}

UMoviePipelineQueueSubsystem *GetQueueSubsystem(FString &OutMessage,
                                                FString &OutCode) {
  if (!LoadRequiredModule(TEXT("MovieRenderPipelineEditor"), OutMessage,
                          OutCode))
    return nullptr;
  if (!GEditor) {
    OutMessage = TEXT("Movie Render Queue requires an editor context.");
    OutCode = TEXT("EDITOR_UNAVAILABLE");
    return nullptr;
  }
  UMoviePipelineQueueSubsystem *QueueSubsystem =
      GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
  if (!QueueSubsystem) {
    OutMessage = TEXT("Movie Pipeline Queue subsystem is unavailable.");
    OutCode = TEXT("MRQ_SUBSYSTEM_UNAVAILABLE");
  }
  return QueueSubsystem;
}

UMoviePipelineExecutorJob *ResolveJob(const TSharedPtr<FJsonObject> &Payload,
                                      UMoviePipelineQueue *Queue,
                                      FString &OutMessage, FString &OutCode) {
  if (!Queue) {
    OutMessage = TEXT("Movie Render Queue is unavailable.");
    OutCode = TEXT("MRQ_QUEUE_UNAVAILABLE");
    return nullptr;
  }
  const FString WantedId = GetString(Payload, TEXT("jobId"), TEXT("renderJobId"));
  const FString WantedName =
      GetString(Payload, TEXT("renderJobName"), TEXT("jobName"));
  if (!WantedName.IsEmpty() &&
      !ValidateRenderJobName(WantedName, OutMessage)) {
    OutCode = TEXT("INVALID_RENDER_JOB_NAME");
    return nullptr;
  }
  TArray<UMoviePipelineExecutorJob *> Jobs = Queue->GetJobs();
  for (UMoviePipelineExecutorJob *Job : Jobs) {
    if (!WantedId.IsEmpty() && GetJobId(Job) == WantedId)
      return Job;
    if (!WantedName.IsEmpty() && Job && Job->JobName == WantedName)
      return Job;
  }
  if (WantedId.IsEmpty() && WantedName.IsEmpty() && Jobs.Num() > 0)
    return Jobs.Last();
  OutMessage = WantedId.IsEmpty()
                   ? FString::Printf(TEXT("Movie Render Queue job not found: %s"),
                                     *WantedName)
                   : FString::Printf(TEXT("Movie Render Queue job not found: %s"),
                                     *WantedId);
  OutCode = TEXT("MRQ_JOB_NOT_FOUND");
  return nullptr;
}

MCP_MOVIE_PIPELINE_CONFIG_CLASS *ResolveConfig(UMoviePipelineExecutorJob *Job,
                                           FString &OutMessage,
                                           FString &OutCode) {
  if (!Job || !Job->GetConfiguration()) {
    OutMessage = TEXT("Movie Render Queue job has no primary configuration.");
    OutCode = TEXT("MRQ_CONFIG_UNAVAILABLE");
    return nullptr;
  }
  return Job->GetConfiguration();
}

bool ValidateJobForExecution(UMoviePipelineExecutorJob *Job,
                             bool bUseCurrentLevel, FString &OutMessage,
                             FString &OutCode) {
  if (!Job || !Cast<ULevelSequence>(Job->Sequence.TryLoad())) {
    OutMessage = TEXT("Movie Render Queue job has an invalid sequence.");
    OutCode = TEXT("INVALID_SEQUENCE");
    return false;
  }
  if (!ValidateRenderJobName(Job->JobName, OutMessage)) {
    OutCode = TEXT("INVALID_RENDER_JOB_NAME");
    return false;
  }
  if (!bUseCurrentLevel && !Cast<UWorld>(Job->Map.TryLoad())) {
    OutMessage = TEXT("Movie Render Queue job requires a valid map.");
    OutCode = TEXT("INVALID_MAP");
    return false;
  }
  if (Job->Map.IsValid() && !Cast<UWorld>(Job->Map.TryLoad())) {
    OutMessage = TEXT("Movie Render Queue job map is not a UWorld asset.");
    OutCode = TEXT("INVALID_MAP");
    return false;
  }
  MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config = ResolveConfig(Job, OutMessage, OutCode);
  UMoviePipelineOutputSetting *Output =
      Config ? Cast<UMoviePipelineOutputSetting>(Config->FindSettingByClass(
                   UMoviePipelineOutputSetting::StaticClass(), true))
             : nullptr;
  if (!Output) {
    OutMessage = TEXT("Movie Render Queue job has no output setting.");
    OutCode = TEXT("MRQ_OUTPUT_UNAVAILABLE");
    return false;
  }
  FString ResolvedDirectory;
  if (!ValidateRenderOutputDirectory(Output->OutputDirectory.Path,
                                     ResolvedDirectory, OutMessage)) {
    OutCode = TEXT("MRQ_OUTPUT_PATH_NOT_ALLOWED");
    return false;
  }
  Output->OutputDirectory.Path = ResolvedDirectory;
  if (!ValidateFileNameFormat(Output->FileNameFormat, OutMessage)) {
    OutCode = TEXT("MRQ_FILENAME_FORMAT_NOT_ALLOWED");
    return false;
  }
  return ValidateJobResourceLimits(Job, OutMessage, OutCode);
}

void EnsureEssentialSettings(UMoviePipelineExecutorJob *Job) {
  FString Message, Code;
  MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config = ResolveConfig(Job, Message, Code);
  if (!Config)
    return;
  Config->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass(),
                                  true);
  Config->FindOrAddSettingByClass(
      UMoviePipelineImageSequenceOutput_PNG::StaticClass(), true);
  Config->FindOrAddSettingByClass(UMoviePipelineDeferredPassBase::StaticClass(),
                                  true);
}

TSharedPtr<FJsonObject> BuildJobResult(UMoviePipelineExecutorJob *Job,
                                       UMoviePipelineQueue *Queue) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  if (!Job)
    return Result;
  Result->SetStringField(TEXT("jobId"), GetJobId(Job));
  Result->SetStringField(TEXT("jobName"), Job->JobName);
  Result->SetStringField(TEXT("sequencePath"), Job->Sequence.ToString());
  Result->SetStringField(TEXT("mapPath"), Job->Map.ToString());
  Result->SetBoolField(TEXT("enabled"), Job->IsEnabled());
  Result->SetBoolField(TEXT("consumed"), Job->IsConsumed());
  if (Queue)
    Result->SetNumberField(TEXT("queueJobCount"), Queue->GetJobs().Num());
  if (MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config = Job->GetConfiguration()) {
    if (UMoviePipelineOutputSetting *Output = Cast<UMoviePipelineOutputSetting>(
            Config->FindSettingByClass(UMoviePipelineOutputSetting::StaticClass(),
                                       true))) {
      Result->SetStringField(TEXT("outputDirectory"),
                             Output->OutputDirectory.Path);
      Result->SetStringField(TEXT("fileNameFormat"), Output->FileNameFormat);
      TSharedPtr<FJsonObject> Resolution = McpHandlerUtils::CreateResultObject();
      Resolution->SetNumberField(TEXT("width"), Output->OutputResolution.X);
      Resolution->SetNumberField(TEXT("height"), Output->OutputResolution.Y);
      Result->SetObjectField(TEXT("resolution"), Resolution);
    }
    if (UMoviePipelineBurnInSetting *BurnIn =
            Cast<UMoviePipelineBurnInSetting>(Config->FindSettingByClass(
                UMoviePipelineBurnInSetting::StaticClass(), true))) {
      Result->SetBoolField(TEXT("burnInEnabled"), BurnIn->IsEnabled());
      Result->SetBoolField(TEXT("burnInCompositeOntoFinalImage"),
                           BurnIn->bCompositeOntoFinalImage);
      Result->SetStringField(TEXT("burnInClass"),
                             BurnIn->BurnInClass.ToString());
    }
  }
  return Result;
}

void SendError(UMcpAutomationBridgeSubsystem *Subsystem, const FString &RequestId,
               TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &Message,
               const FString &Code) {
  Subsystem->SendAutomationResponse(Socket, RequestId, false, Message, nullptr,
                                    Code);
}

}

#endif
