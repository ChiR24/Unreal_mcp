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
}

#if MCP_HAS_MOVIE_RENDER_PIPELINE

bool TryHandle(UMcpAutomationBridgeSubsystem *Subsystem, const FString &RequestId,
               const FString &Action, const TSharedPtr<FJsonObject> &Payload,
               TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  using FHandler = bool (*)(UMcpAutomationBridgeSubsystem *, const FString &,
                            const TSharedPtr<FJsonObject> &, TSharedPtr<FMcpBridgeWebSocket>);
  static const TMap<FString, FHandler> Handlers = {
      {TEXT("create_render_job"), &HandleCreateRenderJob},
      {TEXT("remove_render_job"), &HandleRemoveRenderJob},
      {TEXT("configure_output_settings"), &HandleConfigureOutputSettings},
      {TEXT("add_render_pass"), &HandleAddRenderPass},
      {TEXT("configure_anti_aliasing"), &HandleConfigureAntiAliasing},
      {TEXT("configure_console_variables"), &HandleConfigureConsoleVariables},
      {TEXT("configure_burn_ins"), &HandleConfigureBurnIns},
      {TEXT("queue_render"), &HandleQueueRender},
      {TEXT("start_render"), &HandleStartRender},
  };
  const FHandler *Handler = Handlers.Find(NormalizeAction(Action, Payload));
  return Handler && (*Handler)(Subsystem, RequestId, Payload, RequestingSocket);
}

#else

bool TryHandle(UMcpAutomationBridgeSubsystem *Subsystem,
               const FString &RequestId, const FString &Action,
               const TSharedPtr<FJsonObject> &Payload,
               TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  static const TSet<FString> Actions = {
      TEXT("create_render_job"), TEXT("remove_render_job"), TEXT("configure_output_settings"),
      TEXT("add_render_pass"), TEXT("configure_anti_aliasing"),
      TEXT("configure_console_variables"), TEXT("configure_burn_ins"),
      TEXT("queue_render"), TEXT("start_render"),
  };
  if (!Actions.Contains(NormalizeAction(Action, Payload)))
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
