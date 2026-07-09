#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FMcpBridgeWebSocket;
class FJsonObject;
class UMoviePipelineExecutorBase;
class UMoviePipelineExecutorJob;
class UMoviePipelineQueue;
class UMcpAutomationBridgeSubsystem;
struct FMoviePipelineOutputData;

namespace McpSequenceMovieRender {

struct FRenderFileSnapshot {
  int64 Size = -1;
  FDateTime Modified;
};

struct FRenderWaitState {
  bool bCompleted = false;
  bool bHadFatalError = false;
  bool bOwnsRenderStart = false;
  bool bTimedOut = false;
  bool bCancellationRequested = false;
  bool bCancellationDispatched = false;
  bool bCancellationDeadlineExpired = false;
  bool bClientDisconnected = false;
  bool bOutputPathInvalidated = false;
  double CancellationDeadlineSeconds = 0.0;
  int32 FatalErrorCount = 0;
  FString LastFatalError;
  FString ExpectedJobName;
  FString ExpectedSequencePath;
  FString ExpectedFileNameFormat;
  FString ExpectedOutputDirectory;
  FString OutputPathValidationError;
  TMap<FString, FRenderFileSnapshot> OutputFilesBeforeStart;
  TSet<FString> ReportedOutputFiles;
  TSet<FString> ReportedRenderPasses;
  FDelegateHandle JobFinishedHandle;
  FDelegateHandle FinishedHandle;
  FDelegateHandle ErrorHandle;
  FTSTicker::FDelegateHandle StartCheckHandle;
  FTSTicker::FDelegateHandle TimeoutHandle;
  FTSTicker::FDelegateHandle CancellationHandle;
  FTSTicker::FDelegateHandle OutputPathCheckHandle;
};

bool TryAcquireRenderStartOwnership(UMoviePipelineExecutorBase *Executor,
                                    TSharedRef<FRenderWaitState> State);
bool RequestRenderCancellation(UMoviePipelineExecutorBase *Executor);
void ReleaseRenderStartOwnership(UMoviePipelineExecutorBase *Executor,
                                 TSharedRef<FRenderWaitState> State);
void DiscardPreparedRenderStart(UMoviePipelineExecutorBase *Executor,
                                TSharedRef<FRenderWaitState> State);
void CancelStartRender(UMoviePipelineExecutorBase *Executor,
                       TSharedRef<FRenderWaitState> State);
void BeginTimedOutRenderCancellation(
    TSharedRef<FRenderWaitState> State,
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem,
    TWeakObjectPtr<UMoviePipelineExecutorBase> WeakExecutor,
    TWeakObjectPtr<UMoviePipelineExecutorJob> WeakJob,
    TWeakObjectPtr<UMoviePipelineQueue> WeakQueue, FString RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool CaptureRenderOutputSnapshot(UMoviePipelineExecutorJob *Job,
                                 TSharedRef<FRenderWaitState> State,
                                 FString &OutMessage, FString &OutCode);
void CaptureRenderOutputData(const FMoviePipelineOutputData &OutputData,
                             TSharedRef<FRenderWaitState> State);
int32 AppendRenderOutputProof(UMoviePipelineExecutorJob *Job,
                              const FRenderWaitState &State,
                              TSharedPtr<FJsonObject> Result);

void SendStartRenderCompletion(
    TSharedRef<FRenderWaitState> State,
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem,
    TWeakObjectPtr<UMoviePipelineExecutorBase> WeakExecutor,
    TWeakObjectPtr<UMoviePipelineExecutorJob> WeakJob,
    TWeakObjectPtr<UMoviePipelineQueue> WeakQueue, FString RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSuccess, bool bTimedOut);

}
