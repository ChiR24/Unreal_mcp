#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpSeqMRQInternal.h"

#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "MoviePipelineCameraSetting.h"
#include MCP_MOVIE_PIPELINE_CONFIG_HEADER
#include "MoviePipelineQueue.h"

namespace McpSequenceMovieRender {
// The job's camera setting: when the shutter opens around each frame, the overscan rendered past the frame, and
// whether every camera of a cut renders. FrameCenter, the default, opens the shutter half a frame early, so with
// motion blur the first frame after a camera cut blends in the shot before it; FrameOpen starts at the frame.
bool HandleConfigureRenderCamera(UEngineWireSubsystem *Subsystem,
                                 const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString Message, Code;
  UMoviePipelineQueue *Queue = nullptr;
  UMoviePipelineExecutorJob *Job = ResolveRequestJob(Subsystem, RequestId, Socket, Payload, Queue);
  if (!Job)
    return true;
  MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config = ResolveConfig(Job, Message, Code);
  if (!Config)
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;

  // Every value is checked before any is written.
  FString Shutter;
  const bool bShutter = Payload->TryGetStringField(TEXT("shutterTiming"), Shutter);
  const int64 Timing = bShutter ? StaticEnum<EMoviePipelineShutterTiming>()->GetValueByNameString(Shutter) : INDEX_NONE;
  if (bShutter && Timing == INDEX_NONE)
    return SendError(Subsystem, RequestId, Socket,
                     FString::Printf(TEXT("shutterTiming is FrameOpen, FrameCenter or FrameClose, not %s."), *Shutter),
                     TEXT("INVALID_SHUTTER_TIMING")),
           true;
  double Overscan = 0.0;
  const bool bOverscan = Payload->TryGetNumberField(TEXT("overscanPercentage"), Overscan);
  if (bOverscan && !(Overscan >= 0.0 && Overscan <= 1.0))
    return SendError(Subsystem, RequestId, Socket, TEXT("overscanPercentage is a fraction from 0 to 1 (0.1 renders 10% more)."),
                     TEXT("INVALID_OVERSCAN")),
           true;
  bool bAllCameras = false;
  const bool bHasAllCameras = Payload->TryGetBoolField(TEXT("renderAllCameras"), bAllCameras);
  if (!bShutter && !bOverscan && !bHasAllCameras)
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("configure_render_camera needs shutterTiming, overscanPercentage or renderAllCameras."),
                     TEXT("MISSING_PARAMETER")),
           true;
  // Newer engines added the overscan toggle and renderAllCameras; they are found by name, so an older one says so.
  FBoolProperty *AllCamerasProperty = FindFProperty<FBoolProperty>(UMoviePipelineCameraSetting::StaticClass(), TEXT("bRenderAllCameras"));
  if (bHasAllCameras && !AllCamerasProperty)
    return SendError(Subsystem, RequestId, Socket, TEXT("renderAllCameras is not in this engine's Movie Render Queue camera setting."),
                     TEXT("NOT_SUPPORTED_ON_ENGINE")),
           true;
  UMoviePipelineCameraSetting *Camera = Cast<UMoviePipelineCameraSetting>(
      Config->FindOrAddSettingByClass(UMoviePipelineCameraSetting::StaticClass(), true));
  if (!Camera)
    return SendError(Subsystem, RequestId, Socket, TEXT("The MRQ camera setting is unavailable."), TEXT("MRQ_CAMERA_UNAVAILABLE")),
           true;

  Camera->Modify();
  if (bShutter)
    Camera->ShutterTiming = static_cast<EMoviePipelineShutterTiming>(Timing);
  if (bOverscan) {
    Camera->OverscanPercentage = static_cast<float>(Overscan);
    if (FBoolProperty *Override = FindFProperty<FBoolProperty>(UMoviePipelineCameraSetting::StaticClass(), TEXT("bOverrideCameraOverscan")))
      Override->SetPropertyValue_InContainer(Camera, true);
  }
  if (bHasAllCameras)
    AllCamerasProperty->SetPropertyValue_InContainer(Camera, bAllCameras);
  Config->Modify();
  MCP_SET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue, true);

  TSharedPtr<FJsonObject> Result = BuildJobResult(Job, Queue);
  Result->SetStringField(TEXT("shutterTiming"), StaticEnum<EMoviePipelineShutterTiming>()->GetNameStringByValue(static_cast<int64>(Camera->ShutterTiming)));
  Result->SetNumberField(TEXT("overscanPercentage"), McpPropertyReflection::ReadableFloat(Camera->OverscanPercentage));
  if (AllCamerasProperty)
    Result->SetBoolField(TEXT("renderAllCameras"), AllCamerasProperty->GetPropertyValue_InContainer(Camera));
  Subsystem->SendAutomationResponse(Socket, RequestId, true, TEXT("MRQ camera setting configured."), Result);
  return true;
}
}

#endif
