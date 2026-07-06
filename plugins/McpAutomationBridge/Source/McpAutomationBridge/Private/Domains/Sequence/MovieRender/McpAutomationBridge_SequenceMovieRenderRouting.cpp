#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRender.h"
#include "McpAutomationBridgeSubsystem.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderInternal.h"
#endif

namespace McpSequenceMovieRender {
namespace {
FString NormalizeAction(const FString &Action,
                        const TSharedPtr<FJsonObject> &Payload) {
  FString Normalized = Action.ToLower();
  if (Normalized == TEXT("manage_sequence") && Payload.IsValid()) {
    FString SubAction;
    if (Payload->TryGetStringField(TEXT("subAction"), SubAction))
      Normalized = SubAction.ToLower();
  }
  if (Normalized.StartsWith(TEXT("sequence_")))
    Normalized.RightChopInline(9);
  return Normalized;
}

bool IsMovieRenderAction(const FString &Action) {
  static const TSet<FString> Actions = {
      TEXT("create_render_job"),
      TEXT("configure_output_settings"),
      TEXT("add_render_pass"),
      TEXT("configure_anti_aliasing"),
      TEXT("configure_console_variables"),
      TEXT("configure_burn_ins"),
      TEXT("queue_render"),
      TEXT("start_render"),
  };
  return Actions.Contains(Action);
}
}

#if MCP_HAS_MOVIE_RENDER_PIPELINE

FString NormalizeMovieRenderAction(const FString &Action,
                                   const TSharedPtr<FJsonObject> &Payload) {
  return NormalizeAction(Action, Payload);
}

bool TryHandle(UMcpAutomationBridgeSubsystem *Subsystem, const FString &RequestId,
               const FString &Action, const TSharedPtr<FJsonObject> &Payload,
               TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Normalized = NormalizeMovieRenderAction(Action, Payload);
  if (!IsMovieRenderAction(Normalized))
    return false;
  if (Normalized == TEXT("create_render_job"))
    return HandleCreateRenderJob(Subsystem, RequestId, Payload, RequestingSocket);
  if (Normalized == TEXT("configure_output_settings"))
    return HandleConfigureOutputSettings(Subsystem, RequestId, Payload,
                                         RequestingSocket);
  if (Normalized == TEXT("add_render_pass"))
    return HandleAddRenderPass(Subsystem, RequestId, Payload, RequestingSocket);
  if (Normalized == TEXT("configure_anti_aliasing"))
    return HandleConfigureAntiAliasing(Subsystem, RequestId, Payload,
                                       RequestingSocket);
  if (Normalized == TEXT("configure_console_variables"))
    return HandleConfigureConsoleVariables(Subsystem, RequestId, Payload,
                                           RequestingSocket);
  if (Normalized == TEXT("configure_burn_ins"))
    return HandleConfigureBurnIns(Subsystem, RequestId, Payload, RequestingSocket);
  if (Normalized == TEXT("queue_render"))
    return HandleQueueRender(Subsystem, RequestId, Payload, RequestingSocket);
  if (Normalized == TEXT("start_render"))
    return HandleStartRender(Subsystem, RequestId, Payload, RequestingSocket);
  return false;
}

#else

bool TryHandle(UMcpAutomationBridgeSubsystem *Subsystem,
               const FString &RequestId, const FString &Action,
               const TSharedPtr<FJsonObject> &Payload,
               TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!IsMovieRenderAction(NormalizeAction(Action, Payload)))
    return false;
  Subsystem->SendAutomationError(
      RequestingSocket, RequestId,
      TEXT("Movie Render Queue is unavailable. Enable the MovieRenderPipeline "
           "plugin and rebuild the MCP Automation Bridge."),
      TEXT("MRQ_UNAVAILABLE"));
  return true;
}

#endif
}
