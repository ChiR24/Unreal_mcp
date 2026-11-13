#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/MovieRender/McpSeqMRQ.h"
#include "EngineWireSubsystem.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE
#include "Domains/Sequence/MovieRender/McpSeqMRQInternal.h"
#endif

namespace McpSequenceMovieRender {

#if MCP_HAS_MOVIE_RENDER_PIPELINE

bool TryHandle(UEngineWireSubsystem *Subsystem, const FString &RequestId,
               const FString &Action, const TSharedPtr<FJsonObject> &Payload,
               TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  using FHandler = bool (*)(UEngineWireSubsystem *, const FString &,
                            const TSharedPtr<FJsonObject> &, TSharedPtr<FMcpBridgeWebSocket>);
  static const TMap<FString, FHandler> Handlers = {
      {TEXT("create_render_job"), &HandleCreateRenderJob},
      {TEXT("remove_render_job"), &HandleRemoveRenderJob},
      {TEXT("cancel_render"), &HandleCancelRender},
      {TEXT("configure_output_settings"), &HandleConfigureOutputSettings},
      {TEXT("add_render_pass"), &HandleAddRenderPass},
      {TEXT("configure_anti_aliasing"), &HandleConfigureAntiAliasing},
      {TEXT("configure_render_camera"), &HandleConfigureRenderCamera},
      {TEXT("configure_console_variables"), &HandleConfigureConsoleVariables},
      {TEXT("configure_burn_ins"), &HandleConfigureBurnIns},
      {TEXT("queue_render"), &HandleQueueRender},
      {TEXT("start_render"), &HandleStartRender},
  };
  const FHandler *Handler = Handlers.Find(Action);
  return Handler && (*Handler)(Subsystem, RequestId, Payload, RequestingSocket);
}

#else

bool TryHandle(UEngineWireSubsystem *Subsystem,
               const FString &RequestId, const FString &Action,
               const TSharedPtr<FJsonObject> &Payload,
               TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  static const TSet<FString> Actions = {
      TEXT("create_render_job"), TEXT("remove_render_job"), TEXT("configure_output_settings"),
      TEXT("add_render_pass"), TEXT("configure_anti_aliasing"), TEXT("configure_render_camera"),
      TEXT("configure_console_variables"), TEXT("configure_burn_ins"),
      TEXT("queue_render"), TEXT("start_render"), TEXT("cancel_render"),
  };
  if (!Actions.Contains(Action))
    return false;
  Subsystem->SendAutomationError(
      RequestingSocket, RequestId,
      TEXT("Movie Render Queue is unavailable. Enable the MovieRenderPipeline "
           "plugin and rebuild the EngineWire."),
      TEXT("MRQ_UNAVAILABLE"));
  return true;
}

#endif
}
