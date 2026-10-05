#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderInternal.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderResourceLimits.h"

#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "LevelSequence.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/Guid.h"
#include "MoviePipelineEditorBlueprintLibrary.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"

namespace McpSequenceMovieRender {
namespace {
const TCHAR *CreationJobIdPrefix = TEXT("mcp.renderJobId=");

FString GetCreationString(const TSharedPtr<FJsonObject> &Payload,
                          const TCHAR *First,
                          const TCHAR *Second = nullptr) {
  FString Value;
  if (Payload.IsValid() && Payload->TryGetStringField(First, Value) &&
      !Value.IsEmpty())
    return Value;
  return Payload.IsValid() && Second && Payload->TryGetStringField(Second, Value)
             ? Value
             : FString();
}
}

bool HandleCreateRenderJob(UMcpAutomationBridgeSubsystem *Subsystem,
                           const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString Message, Code;
  UMoviePipelineQueueSubsystem *QueueSubsystem =
      GetQueueSubsystem(Message, Code);
  if (!QueueSubsystem)
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  const FString SequencePath =
      GetCreationString(Payload, TEXT("sequencePath"), TEXT("path"));
  ULevelSequence *Sequence =
      Cast<ULevelSequence>(McpLoadAsset(SequencePath));
  if (!Sequence)
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("create_render_job requires a valid sequencePath."),
                     TEXT("INVALID_SEQUENCE")),
           true;

  UMoviePipelineQueue *Queue = QueueSubsystem->GetQueue();
  if (!Queue)
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("Movie Render Queue is unavailable."),
                     TEXT("MRQ_QUEUE_UNAVAILABLE")),
           true;
  if (!ValidateQueueResourceLimits(Queue, Message, Code))
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  FSoftObjectPath MapObjectPath;
  const FString MapPath = GetCreationString(Payload, TEXT("mapPath"));
  if (!MapPath.IsEmpty()) {
    UWorld *MapAsset =
        Cast<UWorld>(McpLoadAsset(MapPath));
    if (!MapAsset)
      return SendError(Subsystem, RequestId, Socket,
                       FString::Printf(TEXT("Map is not a valid UWorld: %s"),
                                       *MapPath),
                       TEXT("INVALID_MAP")),
             true;
    MapObjectPath = FSoftObjectPath(MapAsset);
  }
  const FString JobName =
      GetCreationString(Payload, TEXT("renderJobName"), TEXT("jobName"));
  if (!JobName.IsEmpty() && !ValidateRenderJobName(JobName, Message))
    return SendError(Subsystem, RequestId, Socket, Message,
                     TEXT("INVALID_RENDER_JOB_NAME")),
           true;
  if (!ValidateOutputSettingsPayload(Payload, Message, Code))
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;

  const bool bWasDirty = MCP_GET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue);
  UMoviePipelineExecutorJob *Job =
      UMoviePipelineEditorBlueprintLibrary::CreateJobFromSequence(Queue,
                                                                  Sequence);
  if (!Job)
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("Failed to allocate Movie Render Queue job."),
                     TEXT("MRQ_JOB_CREATE_FAILED")),
           true;
  const auto RollBackJob = [Queue, Job, bWasDirty]() {
    Queue->DeleteJob(Job);
    MCP_SET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue, bWasDirty);
  };
  UMoviePipelineEditorBlueprintLibrary::EnsureJobHasDefaultSettings(Job);
  if (!JobName.IsEmpty())
    Job->JobName = JobName;
  if (MapObjectPath.IsValid())
    Job->Map = MapObjectPath;
  Job->UserData = FString::Printf(TEXT("%s%s"), CreationJobIdPrefix,
                                  *FGuid::NewGuid().ToString());
  EnsureEssentialSettings(Job);
  if (!ApplyOutputSettings(Job, Payload, Message, Code)) {
    RollBackJob();
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  }
  if (!ValidateQueueResourceLimits(Queue, Message, Code)) {
    RollBackJob();
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  }
  MCP_SET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue, true);
  Subsystem->SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("Movie Render Queue job created."),
                                    BuildJobResult(Job, Queue));
  return true;
}

// Takes one job (jobId or renderJobName) or every job (allJobs) out of the queue; rendered files stay on disk.
bool HandleRemoveRenderJob(UMcpAutomationBridgeSubsystem *Subsystem,
                           const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString Message, Code;
  UMoviePipelineQueueSubsystem *QueueSubsystem = GetQueueSubsystem(Message, Code);
  if (!QueueSubsystem)
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  if (QueueSubsystem->GetActiveExecutor() || QueueSubsystem->IsRendering())
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("The queue cannot change while a render runs; remove jobs once it finishes."),
                     TEXT("MRQ_ALREADY_RENDERING")),
           true;
  UMoviePipelineQueue *Queue = QueueSubsystem->GetQueue();
  int32 Removed = 0;
  if (Queue && GetJsonBoolField(Payload, TEXT("allJobs"), false)) {
    Removed = Queue->GetJobs().Num();
    Queue->DeleteAllJobs();
  } else {
    UMoviePipelineExecutorJob *Job = ResolveRequestJob(Subsystem, RequestId, Socket, Payload, Queue);
    if (!Job)
      return true;
    Queue->DeleteJob(Job);
    Removed = 1;
  }
  MCP_SET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue, true);
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetNumberField(TEXT("removedCount"), Removed);
  Result->SetNumberField(TEXT("queueJobCount"), Queue->GetJobs().Num());
  Subsystem->SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Removed %d job(s) from the Movie Render Queue."), Removed), Result);
  return true;
}
}

#endif
