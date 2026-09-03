#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderInternal.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderResourceLimits.h"

#include "Dom/JsonValue.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "MoviePipelineAntiAliasingSetting.h"
#include "MoviePipelineConsoleVariableSetting.h"
#include MCP_MOVIE_PIPELINE_CONFIG_HEADER
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"
#include "String/LexFromString.h"

namespace McpSequenceMovieRender {
namespace {
const TSharedPtr<FJsonObject> *SettingsObject(
    const TSharedPtr<FJsonObject> &Payload) {
  const TSharedPtr<FJsonObject> *Settings = nullptr;
  return Payload.IsValid() && Payload->TryGetObjectField(TEXT("settings"), Settings)
             ? Settings
             : nullptr;
}

bool TryGetIntEither(const TSharedPtr<FJsonObject> &Payload, const TCHAR *Name,
                     int32 &Out) {
  if (Payload.IsValid() && Payload->TryGetNumberField(Name, Out))
    return true;
  if (const TSharedPtr<FJsonObject> *Settings = SettingsObject(Payload))
    return Settings->IsValid() && (*Settings)->TryGetNumberField(Name, Out);
  return false;
}

bool TryGetStringEither(const TSharedPtr<FJsonObject> &Payload, const TCHAR *Name,
                        FString &Out) {
  if (Payload.IsValid() && Payload->TryGetStringField(Name, Out) &&
      !Out.IsEmpty())
    return true;
  if (const TSharedPtr<FJsonObject> *Settings = SettingsObject(Payload))
    return Settings->IsValid() && (*Settings)->TryGetStringField(Name, Out) &&
           !Out.IsEmpty();
  return false;
}

bool ParseAAMethod(const FString &Name, EAntiAliasingMethod &Out) {
  const FString Lower = Name.ToLower();
  if (Lower == TEXT("none")) Out = AAM_None;
  else if (Lower == TEXT("fxaa")) Out = AAM_FXAA;
  else if (Lower == TEXT("taa") || Lower == TEXT("temporal"))
    Out = AAM_TemporalAA;
  else if (Lower == TEXT("msaa")) Out = AAM_MSAA;
  else if (Lower == TEXT("tsr")) Out = AAM_TSR;
#if MCP_HAS_SMAA
  else if (Lower == TEXT("smaa")) Out = AAM_SMAA;
#endif
  else return false;
  return true;
}

bool JsonToFloat(const TSharedPtr<FJsonValue> &Value, float &Out) {
  if (!Value.IsValid())
    return false;
  if (Value->Type == EJson::Number) {
    Out = static_cast<float>(Value->AsNumber());
    return true;
  }
  if (Value->Type == EJson::String) {
    return LexTryParseString(Out, *Value->AsString()) &&
           FMath::IsFinite(Out);
  }
  if (Value->Type == EJson::Boolean) {
    Out = Value->AsBool() ? 1.0f : 0.0f;
    return true;
  }
  return false;
}
}

bool HandleConfigureAntiAliasing(UMcpAutomationBridgeSubsystem *Subsystem,
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
  int32 SpatialSamples = 0;
  const bool bHasSpatial =
      TryGetIntEither(Payload, TEXT("spatialSampleCount"), SpatialSamples);
  int32 TemporalSamples = 0;
  const bool bHasTemporal =
      TryGetIntEither(Payload, TEXT("temporalSampleCount"), TemporalSamples);
  if ((bHasSpatial && SpatialSamples < 1) ||
      (bHasTemporal && TemporalSamples < 1))
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("MRQ sample counts must be positive."),
                     TEXT("INVALID_AA_SAMPLES")),
           true;
  UMoviePipelineAntiAliasingSetting *ExistingAA =
      Config ? Cast<UMoviePipelineAntiAliasingSetting>(
                   Config->FindSettingByClass(
                       UMoviePipelineAntiAliasingSetting::StaticClass(), true))
             : nullptr;
  const int32 EffectiveSpatial =
      bHasSpatial ? SpatialSamples
                  : ExistingAA ? ExistingAA->SpatialSampleCount : 1;
  const int32 EffectiveTemporal =
      bHasTemporal ? TemporalSamples
                   : ExistingAA ? ExistingAA->TemporalSampleCount : 1;
  if (!ValidateSampleResourceLimits(
          EffectiveSpatial, EffectiveTemporal, Message, Code))
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  FString Method;
  EAntiAliasingMethod ParsedMethod = AAM_None;
  const bool bHasMethod =
      TryGetStringEither(Payload, TEXT("antiAliasingMethod"), Method) ||
      TryGetStringEither(Payload, TEXT("method"), Method);
  if (bHasMethod && !ParseAAMethod(Method, ParsedMethod))
    return SendError(Subsystem, RequestId, Socket,
                     FString::Printf(TEXT("Unsupported anti-aliasing method: %s"),
                                     *Method),
                     TEXT("INVALID_AA_METHOD")),
           true;
  UMoviePipelineAntiAliasingSetting *AA =
      Config ? Cast<UMoviePipelineAntiAliasingSetting>(
                   Config->FindOrAddSettingByClass(
                       UMoviePipelineAntiAliasingSetting::StaticClass(), true))
             : nullptr;
  if (!AA)
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("MRQ anti-aliasing setting is unavailable."),
                     TEXT("MRQ_AA_UNAVAILABLE")),
           true;

  if (bHasSpatial)
    AA->SpatialSampleCount = SpatialSamples;
  if (bHasTemporal)
    AA->TemporalSampleCount = TemporalSamples;
  if (bHasMethod) {
    AA->bOverrideAntiAliasing = true;
    AA->AntiAliasingMethod = ParsedMethod;
  }
  Config->Modify();
  MCP_SET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue, true);
  Subsystem->SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("MRQ anti-aliasing configured."),
                                    BuildJobResult(Job, Queue));
  return true;
}

bool HandleConfigureConsoleVariables(UMcpAutomationBridgeSubsystem *Subsystem,
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
  const TSharedPtr<FJsonObject> *Object = nullptr;
  if (!Config || !Payload.IsValid() ||
      !Payload->TryGetObjectField(TEXT("consoleVariables"), Object))
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("configure_console_variables requires consoleVariables."),
                     TEXT("INVALID_CONSOLE_VARIABLES")),
           true;
  TMap<FString, float> ParsedValues;
  UMoviePipelineConsoleVariableSetting *ExistingCVars =
      Cast<UMoviePipelineConsoleVariableSetting>(
          Config->FindSettingByClass(
              UMoviePipelineConsoleVariableSetting::StaticClass(), true));
  if (ExistingCVars) {
    if (ExistingCVars->ConsoleVariablePresets.Num() > 0 ||
        ExistingCVars->StartConsoleCommands.Num() > 0 ||
        ExistingCVars->EndConsoleCommands.Num() > 0)
      return SendError(
                 Subsystem, RequestId, Socket,
                 TEXT("MRQ console-variable presets and console commands are not allowed."),
                 TEXT("MRQ_CONSOLE_COMMANDS_NOT_ALLOWED")),
             true;
    for (const FMoviePipelineConsoleVariableEntry &Entry :
         ExistingCVars->GetConsoleVariables())
      if (Entry.bIsEnabled)
        ParsedValues.Add(Entry.Name, Entry.Value);
  }
  for (const TPair<FString, TSharedPtr<FJsonValue>> Entry : (*Object)->Values) {
    float Value = 0.0f;
    if (Entry.Key.TrimStartAndEnd().IsEmpty() ||
        !JsonToFloat(Entry.Value, Value) || !FMath::IsFinite(Value))
      return SendError(Subsystem, RequestId, Socket,
                       FString::Printf(TEXT("Invalid CVar value for %s."),
                                       *Entry.Key),
                       TEXT("INVALID_CONSOLE_VARIABLE")),
             true;
    ParsedValues.Add(Entry.Key, Value);
  }
  if (!ValidateConsoleVariableResourceLimits(ParsedValues, Message, Code))
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  UMoviePipelineConsoleVariableSetting *CVars =
      ExistingCVars ? ExistingCVars
                    : Cast<UMoviePipelineConsoleVariableSetting>(
                          Config->FindOrAddSettingByClass(
                              UMoviePipelineConsoleVariableSetting::StaticClass(),
                              true));
  if (!CVars)
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("MRQ console-variable setting is unavailable."),
                     TEXT("MRQ_CVAR_UNAVAILABLE")),
           true;
  for (const TPair<FString, float> &Entry : ParsedValues)
    CVars->AddOrUpdateConsoleVariable(Entry.Key, Entry.Value);
  Config->Modify();
  MCP_SET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue, true);
  TSharedPtr<FJsonObject> Result = BuildJobResult(Job, Queue);
  Result->SetNumberField(TEXT("consoleVariableCount"), ParsedValues.Num());
  Subsystem->SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("MRQ console variables configured."),
                                    Result);
  return true;
}

}

#endif
