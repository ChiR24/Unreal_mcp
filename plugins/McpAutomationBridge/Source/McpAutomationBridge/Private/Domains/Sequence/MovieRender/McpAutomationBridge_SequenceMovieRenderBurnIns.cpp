#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderInternal.h"

#include "McpAutomationBridgeSettings.h"
#include "McpAutomationBridgeSubsystem.h"
#include "MoviePipelineBurnInSetting.h"
#include "MoviePipelineBurnInWidget.h"
#include MCP_MOVIE_PIPELINE_CONFIG_HEADER
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"

namespace McpSequenceMovieRender {

bool HandleConfigureBurnIns(UMcpAutomationBridgeSubsystem *Subsystem,
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
  MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config = ResolveConfig(Job, Message, Code);
  const TSharedPtr<FJsonObject> *BurnInObj = nullptr;
  if (!Config || !Payload.IsValid() ||
      !Payload->TryGetObjectField(TEXT("burnIn"), BurnInObj) ||
      !BurnInObj || !BurnInObj->IsValid())
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("configure_burn_ins requires burnIn."),
                     TEXT("INVALID_BURN_IN")),
           true;
  bool bEnabled = true;
  (*BurnInObj)->TryGetBoolField(TEXT("enabled"), bEnabled);
  bool bComposite = true;
  (*BurnInObj)->TryGetBoolField(TEXT("compositeOntoFinalImage"), bComposite);
  FString ClassPath;
  const bool bHasClass =
      (*BurnInObj)->TryGetStringField(TEXT("classPath"), ClassPath) &&
      !ClassPath.IsEmpty();
  if (bHasClass) {
    const UMcpAutomationBridgeSettings *Settings =
        GetDefault<UMcpAutomationBridgeSettings>();
    const bool bAllowlisted =
        Settings &&
        Settings->MovieRenderBurnInClassAllowlist.ContainsByPredicate(
            [&ClassPath](const FString &AllowedClass) {
              return ClassPath.Equals(AllowedClass.TrimStartAndEnd(),
                                      ESearchCase::CaseSensitive);
            });
    if (!bAllowlisted)
      return SendError(Subsystem, RequestId, Socket,
                       TEXT("Burn-in class is not allowlisted."),
                       TEXT("BURN_IN_CLASS_NOT_ALLOWED")),
             true;
    if (!FSoftClassPath(ClassPath)
             .TryLoadClass<UMoviePipelineBurnInWidget>())
      return SendError(Subsystem, RequestId, Socket,
                       TEXT("Burn-in class could not be loaded."),
                       TEXT("INVALID_BURN_IN_CLASS")),
             true;
  }
  UMoviePipelineBurnInSetting *BurnIn =
      Cast<UMoviePipelineBurnInSetting>(Config->FindOrAddSettingByClass(
          UMoviePipelineBurnInSetting::StaticClass(), true));
  if (!BurnIn)
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("MRQ burn-in setting is unavailable."),
                     TEXT("MRQ_BURN_IN_UNAVAILABLE")),
           true;
  BurnIn->SetIsEnabled(bEnabled);
  BurnIn->bCompositeOntoFinalImage = bComposite;
  if (bHasClass)
    BurnIn->BurnInClass = FSoftClassPath(ClassPath);
  Config->Modify();
  MCP_SET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue, true);
  Subsystem->SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("MRQ burn-ins configured."),
                                    BuildJobResult(Job, Queue));
  return true;
}

}

#endif
