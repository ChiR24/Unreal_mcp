#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderCompletion.h"

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderInternal.h"

#include "McpAutomationBridgeSubsystem.h"
#include "MoviePipelineExecutor.h"
#include "MoviePipelineQueue.h"

namespace McpSequenceMovieRender {
namespace {
TSharedPtr<FJsonObject> BuildRenderResult(
    UMoviePipelineExecutorBase *Executor, UMoviePipelineExecutorJob *Job,
    UMoviePipelineQueue *Queue, bool bCompleted, bool bRenderSucceeded,
    bool bTimedOut, const FRenderWaitState &State) {
  TSharedPtr<FJsonObject> Result = BuildJobResult(Job, Queue);
  if (Executor) {
    Result->SetStringField(TEXT("executorClass"),
                           Executor->GetClass()->GetPathName());
  }
  Result->SetBoolField(TEXT("renderCompleted"), bCompleted);
  Result->SetBoolField(TEXT("renderSucceeded"), bRenderSucceeded);
  Result->SetBoolField(TEXT("timedOut"), bTimedOut);
  if (State.bOutputPathInvalidated) {
    Result->SetStringField(TEXT("outputProof"),
                           TEXT("path_revalidation_failed"));
    Result->SetNumberField(TEXT("outputFileCount"), 0);
    Result->SetArrayField(TEXT("outputFiles"),
                          TArray<TSharedPtr<FJsonValue>>());
  } else {
    AppendRenderOutputProof(Job, State, Result);
  }
  return Result;
}
}

void SendStartRenderCompletion(
    TSharedRef<FRenderWaitState> State,
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem,
    TWeakObjectPtr<UMoviePipelineExecutorBase> WeakExecutor,
    TWeakObjectPtr<UMoviePipelineExecutorJob> WeakJob,
    TWeakObjectPtr<UMoviePipelineQueue> WeakQueue, FString RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSuccess, bool bTimedOut) {
  if (State->bCompleted) {
    return;
  }
  UMoviePipelineExecutorBase *Executor = WeakExecutor.Get();
  if (State->bClientDisconnected) {
    if (Executor && Executor->IsRendering()) {
      return;
    }
    State->bCompleted = true;
    DiscardPreparedRenderStart(Executor, State);
    return;
  }
  if (State->bTimedOut && Executor && Executor->IsRendering() &&
      !State->bCancellationDeadlineExpired) {
    return;
  }

  const bool bWasTimedOut = bTimedOut || State->bTimedOut;
  State->bCompleted = true;
  const bool bRenderStillActive = Executor && Executor->IsRendering();
  DiscardPreparedRenderStart(Executor, State);
  UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
  if (!Subsystem) {
    return;
  }

  bool bFinalSuccess = bSuccess && !State->bHadFatalError;
  TSharedPtr<FJsonObject> Result =
      BuildRenderResult(Executor, WeakJob.Get(), WeakQueue.Get(), !bWasTimedOut,
                        bFinalSuccess, bWasTimedOut, *State);
  FString OutputProofErrorCode;
  FString OutputProofError;
  Result->TryGetStringField(TEXT("outputProofErrorCode"),
                            OutputProofErrorCode);
  Result->TryGetStringField(TEXT("outputProofError"), OutputProofError);
  if (State->bOutputPathInvalidated) {
    Result->SetBoolField(TEXT("renderSucceeded"), false);
    Result->SetStringField(TEXT("outputPathValidationError"),
                           State->OutputPathValidationError);
    Subsystem->SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Movie Render Queue output path changed during rendering."),
        Result, TEXT("MRQ_OUTPUT_PATH_CHANGED"));
    return;
  }
  if (!OutputProofErrorCode.IsEmpty()) {
    Result->SetBoolField(TEXT("renderSucceeded"), false);
    Subsystem->SendAutomationResponse(
        Socket, RequestId, false,
        OutputProofError.IsEmpty()
            ? TEXT("Movie Render Queue output proof failed.")
            : OutputProofError,
        Result, OutputProofErrorCode);
    return;
  }

  double OutputFileCount = 0.0;
  const bool bHasOutputCount =
      Result->TryGetNumberField(TEXT("outputFileCount"), OutputFileCount);
  if (!bWasTimedOut && bFinalSuccess &&
      (!bHasOutputCount || OutputFileCount <= 0.0)) {
    bFinalSuccess = false;
    Result->SetBoolField(TEXT("renderSucceeded"), false);
    Result->SetStringField(
        TEXT("failureReason"),
        TEXT("Movie Render Queue completed without producing output files."));
  }
  Result->SetNumberField(TEXT("fatalErrorCount"), State->FatalErrorCount);
  if (State->bHadFatalError) {
    Result->SetStringField(TEXT("fatalError"), State->LastFatalError);
  }
  if (bWasTimedOut) {
    Result->SetBoolField(TEXT("cancellationRequested"),
                         State->bCancellationRequested);
    Result->SetBoolField(TEXT("cancellationCompleted"), !bRenderStillActive);
    Result->SetBoolField(TEXT("cancellationDeadlineExpired"),
                         State->bCancellationDeadlineExpired);
    Result->SetBoolField(TEXT("renderContinuesAsynchronously"),
                         bRenderStillActive);
    Subsystem->SendAutomationResponse(
        Socket, RequestId, false,
        bRenderStillActive
            ? TEXT("Movie Render Queue render timed out; cancellation did not settle before the configured deadline.")
            : TEXT("Movie Render Queue render timed out and cancellation completed."),
        Result, TEXT("MRQ_RENDER_TIMEOUT"));
    return;
  }

  const bool bProducedNoOutput =
      bSuccess && !State->bHadFatalError && !bFinalSuccess;
  Subsystem->SendAutomationResponse(
      Socket, RequestId, bFinalSuccess,
      bFinalSuccess ? TEXT("Movie Render Queue render completed.")
                    : (bProducedNoOutput
                           ? TEXT("Movie Render Queue render produced no output files.")
                           : TEXT("Movie Render Queue render failed.")),
      Result, bFinalSuccess ? TEXT("")
                            : (bProducedNoOutput
                                   ? TEXT("MRQ_RENDER_NO_OUTPUT")
                                   : TEXT("MRQ_RENDER_FAILED")));
}

}

#endif
