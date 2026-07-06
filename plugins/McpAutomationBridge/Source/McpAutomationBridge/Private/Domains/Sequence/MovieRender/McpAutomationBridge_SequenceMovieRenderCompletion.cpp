#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderCompletion.h"

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

void ReleaseRenderStartOwnershipInternal(
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

bool TryDispatchRenderCancellation(UMoviePipelineExecutorBase *Executor) {
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
  return TryDispatchRenderCancellation(Executor);
}

void ReleaseRenderStartOwnership(UMoviePipelineExecutorBase *Executor,
                                 TSharedRef<FRenderWaitState> State) {
  ReleaseRenderStartOwnershipInternal(Executor, State);
}

void DiscardPreparedRenderStart(UMoviePipelineExecutorBase *Executor,
                                TSharedRef<FRenderWaitState> State) {
  if (State->StartCheckHandle.IsValid()) {
    FTSTicker::GetCoreTicker().RemoveTicker(State->StartCheckHandle);
    State->StartCheckHandle = FTSTicker::FDelegateHandle();
  }
  if (State->TimeoutHandle.IsValid()) {
    FTSTicker::GetCoreTicker().RemoveTicker(State->TimeoutHandle);
    State->TimeoutHandle = FTSTicker::FDelegateHandle();
  }
  if (State->CancellationHandle.IsValid()) {
    FTSTicker::GetCoreTicker().RemoveTicker(State->CancellationHandle);
    State->CancellationHandle = FTSTicker::FDelegateHandle();
  }
  if (State->OutputPathCheckHandle.IsValid()) {
    FTSTicker::GetCoreTicker().RemoveTicker(State->OutputPathCheckHandle);
    State->OutputPathCheckHandle = FTSTicker::FDelegateHandle();
  }
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
  ReleaseRenderStartOwnershipInternal(Executor, State);
}

void CancelStartRender(UMoviePipelineExecutorBase *Executor,
                       TSharedRef<FRenderWaitState> State) {
  if (State->bCompleted)
    return;
  State->bClientDisconnected = true;
  State->bCancellationRequested = true;
  if (State->TimeoutHandle.IsValid()) {
    FTSTicker::GetCoreTicker().RemoveTicker(State->TimeoutHandle);
    State->TimeoutHandle = FTSTicker::FDelegateHandle();
  }
  if (!Executor || !Executor->IsRendering()) {
    State->bCompleted = true;
    DiscardPreparedRenderStart(Executor, State);
    return;
  }
  State->bCancellationDispatched =
      TryDispatchRenderCancellation(Executor);
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
                    TryDispatchRenderCancellation(CurrentExecutor);
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
      Settings ? Settings->MaxMovieRenderCancellationWaitMs
               : MaximumCancellationWaitMs;
  const double CancellationWaitSeconds =
      FMath::Clamp(ConfiguredCancellationWaitMs, 1,
                   MaximumCancellationWaitMs) /
      1000.0;
  State->CancellationDeadlineSeconds =
      FPlatformTime::Seconds() + CancellationWaitSeconds;
  if (State->TimeoutHandle.IsValid()) {
    FTSTicker::GetCoreTicker().RemoveTicker(State->TimeoutHandle);
    State->TimeoutHandle = FTSTicker::FDelegateHandle();
  }
  State->CancellationHandle = FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda(
          [State, WeakSubsystem, WeakExecutor, WeakJob, WeakQueue, RequestId,
           Socket](float) {
            if (State->bCompleted)
              return false;
            UMoviePipelineExecutorBase *Executor = WeakExecutor.Get();
            if (!Executor) {
              SendStartRenderCompletion(
                  State, WeakSubsystem, WeakExecutor, WeakJob, WeakQueue,
                  RequestId, Socket, false, true);
              return false;
            }
            if (!Executor->IsRendering()) {
              SendStartRenderCompletion(
                  State, WeakSubsystem, WeakExecutor, WeakJob, WeakQueue,
                  RequestId, Socket, false, true);
              return false;
            }
            if (FPlatformTime::Seconds() >=
                State->CancellationDeadlineSeconds) {
              State->bCancellationDeadlineExpired = true;
              SendStartRenderCompletion(
                  State, WeakSubsystem, WeakExecutor, WeakJob, WeakQueue,
                  RequestId, Socket, false, true);
              return false;
            }
            if (!State->bCancellationDispatched) {
              State->bCancellationDispatched =
                  TryDispatchRenderCancellation(Executor);
              return !State->bCompleted;
            }
            return true;
          }),
      0.0f);
}

}

#endif
