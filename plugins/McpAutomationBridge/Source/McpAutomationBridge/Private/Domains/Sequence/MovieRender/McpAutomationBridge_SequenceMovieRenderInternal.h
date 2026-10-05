#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRender.h"

class UMoviePipelineExecutorJob;
class UMoviePipelineMasterConfig;
class UMoviePipelinePrimaryConfig;
class UMoviePipelineQueue;
class UMoviePipelineQueueSubsystem;
class UMoviePipelineOutputSetting;

#include "String/LexFromString.h"

namespace McpSequenceMovieRender {
// Payload.settings.<Name> as an int.
inline bool TryGetSettingsInt(const TSharedPtr<FJsonObject> &Payload, const TCHAR *Name, int32 &Out) {
  const TSharedPtr<FJsonObject> *Settings = nullptr;
  return Payload.IsValid() && Payload->TryGetObjectField(TEXT("settings"), Settings) &&
         Settings && Settings->IsValid() && (*Settings)->TryGetNumberField(Name, Out);
}

// "WIDTHxHEIGHT" with two positive integers.
inline bool TryParseResolution(const FString &Text, FIntPoint &Out) {
  FString Left, Right;
  if (!Text.Split(TEXT("x"), &Left, &Right) && !Text.Split(TEXT("X"), &Left, &Right))
    return false;
  return LexTryParseString(Out.X, *Left) && LexTryParseString(Out.Y, *Right) && Out.X > 0 && Out.Y > 0;
}

bool LoadRequiredModule(const TCHAR *ModuleName, FString &OutMessage,
                        FString &OutCode);
UMoviePipelineQueueSubsystem *GetQueueSubsystem(FString &OutMessage,
                                                FString &OutCode);
UMoviePipelineExecutorJob *ResolveJob(const TSharedPtr<FJsonObject> &Payload,
                                      UMoviePipelineQueue *Queue,
                                      FString &OutMessage, FString &OutCode);
MCP_MOVIE_PIPELINE_CONFIG_CLASS *ResolveConfig(
    UMoviePipelineExecutorJob *Job, FString &OutMessage, FString &OutCode);
UMoviePipelineOutputSetting *ApplyOutputSettings(
    UMoviePipelineExecutorJob *Job, const TSharedPtr<FJsonObject> &Payload,
    FString &OutMessage, FString &OutCode);
bool ValidateOutputSettingsPayload(const TSharedPtr<FJsonObject> &Payload,
                                   FString &OutMessage, FString &OutCode);
bool ValidateRenderOutputDirectory(const FString &OutputDirectory,
                                   FString &OutResolvedDirectory,
                                   FString &OutValidationError);
bool ValidateFileNameFormat(const FString &FileNameFormat,
                            FString &OutValidationError);
bool ValidateRenderJobName(const FString &JobName,
                           FString &OutValidationError);
bool ValidateJobForExecution(UMoviePipelineExecutorJob *Job,
                             bool bUseCurrentLevel, FString &OutMessage,
                             FString &OutCode);
void EnsureEssentialSettings(UMoviePipelineExecutorJob *Job);
TSharedPtr<FJsonObject> BuildJobResult(UMoviePipelineExecutorJob *Job,
                                       UMoviePipelineQueue *Queue);
void SendError(UMcpAutomationBridgeSubsystem *Subsystem, const FString &RequestId,
               TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &Message,
               const FString &Code);
// The queue subsystem's queue and the payload's job; sends the refusal and returns null on a miss.
UMoviePipelineExecutorJob *ResolveRequestJob(UMcpAutomationBridgeSubsystem *Subsystem,
                                             const FString &RequestId,
                                             TSharedPtr<FMcpBridgeWebSocket> Socket,
                                             const TSharedPtr<FJsonObject> &Payload,
                                             UMoviePipelineQueue *&OutQueue);

bool HandleCreateRenderJob(UMcpAutomationBridgeSubsystem *Subsystem,
                           const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleRemoveRenderJob(UMcpAutomationBridgeSubsystem *Subsystem,
                           const FString &RequestId,
                           const TSharedPtr<FJsonObject> &Payload,
                           TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureOutputSettings(UMcpAutomationBridgeSubsystem *Subsystem,
                                   const FString &RequestId,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddRenderPass(UMcpAutomationBridgeSubsystem *Subsystem,
                         const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureAntiAliasing(UMcpAutomationBridgeSubsystem *Subsystem,
                                 const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureConsoleVariables(UMcpAutomationBridgeSubsystem *Subsystem,
                                     const FString &RequestId,
                                     const TSharedPtr<FJsonObject> &Payload,
                                     TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureBurnIns(UMcpAutomationBridgeSubsystem *Subsystem,
                            const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload,
                            TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleQueueRender(UMcpAutomationBridgeSubsystem *Subsystem,
                       const FString &RequestId,
                       const TSharedPtr<FJsonObject> &Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleStartRender(UMcpAutomationBridgeSubsystem *Subsystem,
                       const FString &RequestId,
                       const TSharedPtr<FJsonObject> &Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket);
}
