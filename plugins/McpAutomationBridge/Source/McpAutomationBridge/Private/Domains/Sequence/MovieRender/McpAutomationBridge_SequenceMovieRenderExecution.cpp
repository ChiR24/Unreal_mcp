#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRender.h"
#include "McpAutomationBridgeSubsystem.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderCompletion.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderExecutor.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderInternal.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderResourceLimits.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "MoviePipelineExecutor.h"
#include "MoviePipelineInProcessExecutor.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"

namespace McpSequenceMovieRender {

bool HandleQueueRender(UMcpAutomationBridgeSubsystem *Subsystem,
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
  const bool bUseCurrentLevel =
      McpHandlerUtils::GetOptionalBool(Payload, TEXT("useCurrentLevel"), false);
  if (!ValidateJobForExecution(Job, bUseCurrentLevel, Message, Code))
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  const bool bOnlyJob =
      McpHandlerUtils::GetOptionalBool(Payload, TEXT("onlyJob"), false);
  struct FJobState {
    UMoviePipelineExecutorJob *Job = nullptr;
    bool bEnabled = false;
    bool bConsumed = false;
  };
  TArray<FJobState> PreviousStates;
  PreviousStates.Reserve(Queue->GetJobs().Num());
  for (UMoviePipelineExecutorJob *QueuedJob : Queue->GetJobs()) {
    if (QueuedJob)
      PreviousStates.Add(
          {QueuedJob, QueuedJob->IsEnabled(), QueuedJob->IsConsumed()});
  }
  if (bOnlyJob) {
    for (UMoviePipelineExecutorJob *Other : Queue->GetJobs())
      if (Other)
        Other->SetIsEnabled(Other == Job);
  } else {
    Job->SetIsEnabled(true);
  }
  Job->SetConsumed(false);
  if (!ValidateQueueResourceLimits(Queue, Message, Code)) {
    for (const FJobState &Previous : PreviousStates) {
      if (Previous.Job) {
        Previous.Job->SetIsEnabled(Previous.bEnabled);
        Previous.Job->SetConsumed(Previous.bConsumed);
      }
    }
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  }
  MCP_SET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue, true);
  Subsystem->SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("MRQ job queued."),
                                    BuildJobResult(Job, Queue));
  return true;
}

bool HandleStartRender(UMcpAutomationBridgeSubsystem *Subsystem,
                       const FString &RequestId,
                       const TSharedPtr<FJsonObject> &Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString Message, Code;
  UMoviePipelineQueueSubsystem *QueueSubsystem =
      GetQueueSubsystem(Message, Code);
  if (!QueueSubsystem)
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  if (QueueSubsystem->GetActiveExecutor() || QueueSubsystem->IsRendering())
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("Movie Render Queue is already rendering."),
                     TEXT("MRQ_ALREADY_RENDERING")),
           true;
  UMoviePipelineQueue *Queue = QueueSubsystem->GetQueue();
  if (!Queue || Queue->GetJobs().Num() == 0)
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("Movie Render Queue has no jobs to render."),
                     TEXT("MRQ_QUEUE_EMPTY")),
           true;
  UMoviePipelineExecutorJob *Job =
      ResolveJob(Payload, Queue, Message, Code);
  if (!Job)
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  if (!Job->IsEnabled())
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("Selected Movie Render Queue job is not queued."),
                     TEXT("MRQ_JOB_NOT_QUEUED")),
           true;
  const bool bUseCurrentLevel =
      McpHandlerUtils::GetOptionalBool(Payload, TEXT("useCurrentLevel"), false);
  for (UMoviePipelineExecutorJob *QueuedJob : Queue->GetJobs()) {
    if (QueuedJob && QueuedJob->IsEnabled() &&
        !ValidateJobForExecution(QueuedJob, bUseCurrentLevel, Message, Code))
      return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  }
  if (!ValidateQueueResourceLimits(Queue, Message, Code))
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  TSubclassOf<UMoviePipelineExecutorBase> ExecutorClass =
      ResolveExecutorClass(Payload, Message, Code);
  if (!ExecutorClass)
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  double TimeoutMs = 300000.0;
  if (Payload.IsValid())
    Payload->TryGetNumberField(TEXT("timeoutMs"), TimeoutMs);
  if (!ValidateRenderTimeoutResourceLimit(TimeoutMs, Message, Code))
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  UMoviePipelineExecutorBase *Executor =
      NewObject<UMoviePipelineExecutorBase>(QueueSubsystem, ExecutorClass);
  if (!Executor)
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("Movie Render Queue failed to allocate executor."),
                     TEXT("MRQ_START_FAILED")),
           true;
  if (bUseCurrentLevel) {
    if (UMoviePipelineInProcessExecutor *InProcessExecutor =
            Cast<UMoviePipelineInProcessExecutor>(Executor)) {
      InProcessExecutor->bUseCurrentLevel = true;
    }
  }
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(Subsystem);
  TWeakObjectPtr<UMoviePipelineExecutorBase> WeakExecutor(Executor);
  TWeakObjectPtr<UMoviePipelineExecutorJob> WeakJob(Job);
  TWeakObjectPtr<UMoviePipelineQueue> WeakQueue(Queue);
  TWeakObjectPtr<UMoviePipelineQueueSubsystem> WeakQueueSubsystem(
      QueueSubsystem);
  TSharedRef<FRenderWaitState> State = MakeShared<FRenderWaitState>();
  if (!CaptureRenderOutputSnapshot(Job, State, Message, Code))
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  if (!TryAcquireRenderStartOwnership(Executor, State))
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("Movie Render Queue is already starting a render."),
                     TEXT("MRQ_ALREADY_RENDERING")),
           true;
  AttachOutputCapture(Executor, State);
  State->ErrorHandle = Executor->OnExecutorErrored().AddLambda(
      [State, WeakSubsystem, WeakExecutor, WeakJob, WeakQueue, RequestId,
       Socket](UMoviePipelineExecutorBase *, UMoviePipeline *, bool bIsFatal,
               FText ErrorText) {
        if (!bIsFatal)
          return;
        State->bHadFatalError = true;
        ++State->FatalErrorCount;
        State->LastFatalError =
            TEXT("Movie Render Queue reported a fatal executor error.");
        if (State->bTimedOut)
          return;
        SendStartRenderCompletion(State, WeakSubsystem, WeakExecutor, WeakJob,
                                  WeakQueue, RequestId, Socket, false, false);
      });
  State->FinishedHandle = Executor->OnExecutorFinished().AddLambda(
      [State, WeakSubsystem, WeakExecutor, WeakJob, WeakQueue, RequestId,
       Socket](UMoviePipelineExecutorBase *, bool bSuccess) {
        SendStartRenderCompletion(State, WeakSubsystem, WeakExecutor, WeakJob,
                                  WeakQueue, RequestId, Socket, bSuccess,
                                  State->bTimedOut);
      });
  const float TimeoutSeconds =
      static_cast<float>(FMath::Max(TimeoutMs / 1000.0, 0.001));
  State->TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda(
          [State, WeakSubsystem, WeakExecutor, WeakJob, WeakQueue, RequestId,
           Socket](float) {
            BeginTimedOutRenderCancellation(
                State, WeakSubsystem, WeakExecutor, WeakJob, WeakQueue,
                RequestId, Socket);
            return false;
          }),
      TimeoutSeconds);
  if (!Subsystem->RegisterAutomationRequestCancellation(
          RequestId,
          [State, WeakExecutor]()
          {
            CancelStartRender(WeakExecutor.Get(), State);
          }))
  {
    CancelStartRender(Executor, State);
    return true;
  }
  for (UMoviePipelineExecutorJob *QueuedJob : Queue->GetJobs()) {
    if (QueuedJob && QueuedJob->IsEnabled() &&
        !ValidateJobForExecution(QueuedJob, bUseCurrentLevel, Message, Code)) {
      DiscardPreparedRenderStart(Executor, State);
      return SendError(Subsystem, RequestId, Socket, Message, Code), true;
    }
  }
  QueueSubsystem->RenderQueueWithExecutorInstance(Executor);
  State->OutputPathCheckHandle = FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda(
          [State, WeakExecutor](float) {
            if (State->bCompleted)
              return false;
            FString ResolvedDirectory;
            FString ValidationError;
            if (!ValidateRenderOutputDirectory(
                    State->ExpectedOutputDirectory, ResolvedDirectory,
                    ValidationError) ||
                ResolvedDirectory != State->ExpectedOutputDirectory) {
              State->bOutputPathInvalidated = true;
              State->OutputPathValidationError =
                  ValidationError.IsEmpty()
                      ? TEXT("The render output path changed after validation.")
                      : ValidationError;
              State->bCancellationRequested = true;
              State->bCancellationDispatched =
                  RequestRenderCancellation(WeakExecutor.Get());
            }
            return !State->bCompleted;
          }),
      0.1f);
  if (!State->bCompleted) {
    State->StartCheckHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [State, WeakSubsystem, WeakExecutor, WeakJob, WeakQueue,
             WeakQueueSubsystem, RequestId, Socket](float) {
              if (State->bCompleted)
                return false;
              UMoviePipelineExecutorBase *CurrentExecutor = WeakExecutor.Get();
              UMoviePipelineQueueSubsystem *CurrentQueueSubsystem =
                  WeakQueueSubsystem.Get();
              const bool bStillOwnsExecutor =
                  CurrentExecutor && CurrentQueueSubsystem &&
                  CurrentQueueSubsystem->GetActiveExecutor() == CurrentExecutor;
              if (!bStillOwnsExecutor) {
                SendStartRenderCompletion(
                    State, WeakSubsystem, WeakExecutor, WeakJob, WeakQueue,
                    RequestId, Socket, false, State->bTimedOut);
                return false;
              }
              return !CurrentExecutor->IsRendering();
            }),
        2.0f);
  }
  return true;
}
}

#endif
