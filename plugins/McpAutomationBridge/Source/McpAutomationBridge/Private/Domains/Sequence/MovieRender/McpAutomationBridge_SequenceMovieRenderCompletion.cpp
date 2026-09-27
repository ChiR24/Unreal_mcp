#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderCompletion.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "McpAutomationBridgeSettings.h"
#include "Editor.h"
#include "HAL/PlatformTime.h"
#include "MoviePipelineExecutor.h"
#include "MoviePipelineInProcessExecutor.h"
#include "MoviePipelinePIEExecutor.h"
#include "MoviePipelineQueue.h"

namespace McpSequenceMovieRender {
namespace {
TWeakObjectPtr<UMoviePipelineExecutorBase> ActiveRenderStartOwner;
constexpr int32 MovieRenderTransportGraceMs = 35000;
constexpr int32 MovieRenderResponseBudgetMs = 5000;
constexpr int32 MaximumCancellationWaitMs =
    MovieRenderTransportGraceMs - MovieRenderResponseBudgetMs;

void ReleaseRenderStartOwnership(
    UMoviePipelineExecutorBase *Executor,
    TSharedRef<FRenderWaitState> State) {
  if (!State->bOwnsRenderStart)
    return;
  if (!ActiveRenderStartOwner.IsValid() ||
      ActiveRenderStartOwner.Get() == Executor) {
    ActiveRenderStartOwner.Reset();
  }
  State->bOwnsRenderStart = false;
}

void RemoveTicker(FTSTicker::FDelegateHandle &Handle) {
  if (Handle.IsValid()) {
    FTSTicker::GetCoreTicker().RemoveTicker(Handle);
    Handle = FTSTicker::FDelegateHandle();
  }
}

}

bool TryAcquireRenderStartOwnership(UMoviePipelineExecutorBase *Executor,
                                    TSharedRef<FRenderWaitState> State) {
  if (!Executor || ActiveRenderStartOwner.IsValid())
    return false;
  ActiveRenderStartOwner = Executor;
  State->bOwnsRenderStart = true;
  return true;
}

bool RequestRenderCancellation(UMoviePipelineExecutorBase *Executor) {
  if (!Executor || !Executor->IsRendering())
    return false;
  if (Cast<UMoviePipelinePIEExecutor>(Executor)) {
    if (!GEditor || !GEditor->PlayWorld)
      return false;
    GEditor->RequestEndPlayMap();
    return true;
  }
  Executor->CancelAllJobs();
  return true;
}

void DiscardPreparedRenderStart(UMoviePipelineExecutorBase *Executor,
                                TSharedRef<FRenderWaitState> State) {
  RemoveTicker(State->StartCheckHandle);
  RemoveTicker(State->TimeoutHandle);
  RemoveTicker(State->CancellationHandle);
  RemoveTicker(State->OutputPathCheckHandle);
  if (Executor && State->FinishedHandle.IsValid()) {
    Executor->OnExecutorFinished().Remove(State->FinishedHandle);
    State->FinishedHandle.Reset();
  }
  if (Executor && State->ErrorHandle.IsValid()) {
    Executor->OnExecutorErrored().Remove(State->ErrorHandle);
    State->ErrorHandle.Reset();
  }
  if (Executor && State->JobFinishedHandle.IsValid()) {
    if (UMoviePipelinePIEExecutor *Pie =
            Cast<UMoviePipelinePIEExecutor>(Executor)) {
      Pie->OnIndividualJobWorkFinished().Remove(State->JobFinishedHandle);
    } else if (UMoviePipelineInProcessExecutor *InProcess =
                   Cast<UMoviePipelineInProcessExecutor>(Executor)) {
      InProcess->OnIndividualJobFinished().Remove(State->JobFinishedHandle);
    }
    State->JobFinishedHandle.Reset();
  }
  // onlyJob's enable toggles were a per-render override; put the queue back
  // the way the caller found it instead of leaving the other jobs disabled.
  RestoreJobEnabledStates(State->OnlyJobPreviousEnabled);
  ReleaseRenderStartOwnership(Executor, State);
}

void RestoreJobEnabledStates(
    TArray<TPair<TWeakObjectPtr<UMoviePipelineExecutorJob>, bool>> &Previous) {
  for (const TPair<TWeakObjectPtr<UMoviePipelineExecutorJob>, bool> &Prev :
       Previous) {
    if (Prev.Key.IsValid()) {
      Prev.Key->SetIsEnabled(Prev.Value);
    }
  }
  Previous.Reset();
}

void CancelStartRender(UMoviePipelineExecutorBase *Executor,
                       TSharedRef<FRenderWaitState> State) {
  if (State->bCompleted)
    return;
  State->bClientDisconnected = true;
  State->bCancellationRequested = true;
  RemoveTicker(State->TimeoutHandle);
  if (!Executor || !Executor->IsRendering()) {
    State->bCompleted = true;
    DiscardPreparedRenderStart(Executor, State);
    return;
  }
  State->bCancellationDispatched =
      RequestRenderCancellation(Executor);
  if (!State->CancellationHandle.IsValid()) {
    TWeakObjectPtr<UMoviePipelineExecutorBase> WeakExecutor(Executor);
    State->CancellationHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [State, WeakExecutor](float) {
              if (State->bCompleted)
                return false;
              UMoviePipelineExecutorBase *CurrentExecutor =
                  WeakExecutor.Get();
              if (!CurrentExecutor || !CurrentExecutor->IsRendering()) {
                State->bCompleted = true;
                DiscardPreparedRenderStart(CurrentExecutor, State);
                return false;
              }
              if (!State->bCancellationDispatched) {
                State->bCancellationDispatched =
                    RequestRenderCancellation(CurrentExecutor);
              }
              return true;
            }),
        0.0f);
  }
}

void BeginTimedOutRenderCancellation(
    TSharedRef<FRenderWaitState> State,
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem,
    TWeakObjectPtr<UMoviePipelineExecutorBase> WeakExecutor,
    TWeakObjectPtr<UMoviePipelineExecutorJob> WeakJob,
    TWeakObjectPtr<UMoviePipelineQueue> WeakQueue, FString RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (State->bCompleted || State->bTimedOut)
    return;
  State->bTimedOut = true;
  State->bCancellationRequested = true;
  const UMcpAutomationBridgeSettings *Settings =
      GetDefault<UMcpAutomationBridgeSettings>();
  const int32 ConfiguredCancellationWaitMs =
      Settings->MaxMovieRenderCancellationWaitMs;
  const double CancellationWaitSeconds =
      FMath::Clamp(ConfiguredCancellationWaitMs, 1,
                   MaximumCancellationWaitMs) /
      1000.0;
  State->CancellationDeadlineSeconds =
      FPlatformTime::Seconds() + CancellationWaitSeconds;
  RemoveTicker(State->TimeoutHandle);
  State->CancellationHandle = FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda(
          [State, WeakSubsystem, WeakExecutor, WeakJob, WeakQueue, RequestId,
           Socket](float) {
            if (State->bCompleted)
              return false;
            UMoviePipelineExecutorBase *Executor = WeakExecutor.Get();
            State->bCancellationDeadlineExpired =
                Executor && Executor->IsRendering() &&
                FPlatformTime::Seconds() >= State->CancellationDeadlineSeconds;
            if (!Executor || !Executor->IsRendering() ||
                State->bCancellationDeadlineExpired) {
              SendStartRenderCompletion(
                  State, WeakSubsystem, WeakExecutor, WeakJob, WeakQueue,
                  RequestId, Socket, false, true);
              return false;
            }
            if (!State->bCancellationDispatched) {
              State->bCancellationDispatched =
                  RequestRenderCancellation(Executor);
              return !State->bCompleted;
            }
            return true;
          }),
      0.0f);
}

}

#endif
