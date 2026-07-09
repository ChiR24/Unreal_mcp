#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderExecutor.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderCompletion.h"
#include "McpAutomationBridgeSettings.h"
#include "MoviePipelineExecutor.h"
#include "MoviePipelineInProcessExecutor.h"
#include "MoviePipelinePIEExecutor.h"
#include "MovieRenderPipelineSettings.h"

namespace McpSequenceMovieRender {
namespace {
bool ValidateExecutorClassAllowlist(const FString &ExecutorClassPath,
                                    FString &OutMessage,
                                    FString &OutCode) {
  const UMcpAutomationBridgeSettings *BridgeSettings =
      GetDefault<UMcpAutomationBridgeSettings>();
  const bool bAllowlisted =
      BridgeSettings &&
      BridgeSettings->MovieRenderExecutorClassAllowlist.ContainsByPredicate(
          [&ExecutorClassPath](const FString &AllowedClass) {
            return ExecutorClassPath.Equals(
                AllowedClass.TrimStartAndEnd(), ESearchCase::CaseSensitive);
          });
  if (bAllowlisted)
    return true;
  OutMessage = FString::Printf(
      TEXT("MRQ executor class is not allowlisted: %s"),
      *ExecutorClassPath);
  OutCode = TEXT("MRQ_EXECUTOR_NOT_ALLOWED");
  return false;
}
}

TSubclassOf<UMoviePipelineExecutorBase> ResolveExecutorClass(
    const TSharedPtr<FJsonObject> &Payload, FString &OutMessage,
    FString &OutCode) {
  FString ExecutorClassPath;
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("executorClass"), ExecutorClassPath);
  if (!ExecutorClassPath.IsEmpty()) {
    if (!ValidateExecutorClassAllowlist(ExecutorClassPath, OutMessage,
                                        OutCode))
      return nullptr;
    UClass *Loaded =
        LoadClass<UMoviePipelineExecutorBase>(nullptr, *ExecutorClassPath);
    if (Loaded && Loaded->IsChildOf(UMoviePipelineExecutorBase::StaticClass()))
      return Loaded;
    OutMessage =
        FString::Printf(TEXT("MRQ executor class not found: %s"),
                        *ExecutorClassPath);
    OutCode = TEXT("MRQ_EXECUTOR_UNAVAILABLE");
    return nullptr;
  }

  const UMovieRenderPipelineProjectSettings *Settings =
      GetDefault<UMovieRenderPipelineProjectSettings>();
  TSubclassOf<UMoviePipelineExecutorBase> DefaultExecutor =
      Settings ? Settings->DefaultLocalExecutor
                       .TryLoadClass<UMoviePipelineExecutorBase>()
               : nullptr;
  if (DefaultExecutor &&
      ValidateExecutorClassAllowlist(DefaultExecutor->GetPathName(),
                                     OutMessage, OutCode))
    return DefaultExecutor;
  if (DefaultExecutor)
    return nullptr;
  OutMessage = TEXT("Default Movie Render Queue local executor is unavailable.");
  OutCode = TEXT("MRQ_EXECUTOR_UNAVAILABLE");
  return nullptr;
}

void AttachOutputCapture(UMoviePipelineExecutorBase *Executor,
                         TSharedRef<FRenderWaitState> State) {
  const auto Capture = [State](FMoviePipelineOutputData OutputData) {
    CaptureRenderOutputData(OutputData, State);
  };
  if (UMoviePipelinePIEExecutor *Pie =
          Cast<UMoviePipelinePIEExecutor>(Executor)) {
    State->JobFinishedHandle =
        Pie->OnIndividualJobWorkFinished().AddLambda(Capture);
  } else if (UMoviePipelineInProcessExecutor *InProcess =
                 Cast<UMoviePipelineInProcessExecutor>(Executor)) {
    State->JobFinishedHandle =
        InProcess->OnIndividualJobFinished().AddLambda(Capture);
  }
}
}

#endif
