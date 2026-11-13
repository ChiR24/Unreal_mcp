#include "Domains/Sequence/Cinematics/McpSeqCine.h"

#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpSequenceCinematics {
TSharedPtr<FJsonObject> MakeResult(bool bSuccess, const FString &Action,
                                   const FString &Message,
                                   const FString &ErrorCode) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetStringField(TEXT("action"), Action);
  Result->SetStringField(bSuccess ? TEXT("message") : TEXT("error"), Message);
  if (!ErrorCode.IsEmpty()) {
    Result->SetStringField(TEXT("errorCode"), ErrorCode);
  }
  return Result;
}

ULevelSequence *LoadSequenceOrSend(UEngineWireSubsystem &Bridge, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                   const FString &RequestId, const TSharedPtr<FJsonObject> &Params) {
  TSharedPtr<FJsonObject> Failure;
  ULevelSequence *Sequence = LoadSequence(Params, Failure);
  if (!Sequence) {
    FString Message;
    FString Code = TEXT("INVALID_SEQUENCE");
    if (Failure.IsValid()) {
      Failure->TryGetStringField(TEXT("error"), Message);
      Failure->TryGetStringField(TEXT("errorCode"), Code);
    }
    Bridge.SendAutomationError(Socket, RequestId, Message.IsEmpty() ? TEXT("sequencePath names no Level Sequence.") : Message, Code);
  }
  return Sequence;
}

bool TryHandleCinematics(const FString &Action,
                         const TSharedPtr<FJsonObject> &Params,
                         TSharedPtr<FJsonObject> &OutResult) {
  using FHandler = bool (*)(const TSharedPtr<FJsonObject> &, TSharedPtr<FJsonObject> &);
  static const TMap<FString, FHandler> Handlers = {
      {TEXT("create_master_sequence"), &HandleCreateMasterSequence},
      {TEXT("add_subsequence"), &HandleAddSubsequence},
      {TEXT("add_shot_track"), &HandleAddShotTrack},
      {TEXT("configure_shot_settings"), &HandleConfigureShotSettings},
      {TEXT("create_cine_camera_actor"), &HandleCreateCineCameraActor},
      {TEXT("configure_camera_settings"), &HandleConfigureCameraSettings},
      {TEXT("add_camera_cut_track"), &HandleAddCameraCutTrack},
      {TEXT("add_camera_shake_track"), &HandleAddCameraShakeTrack},
      {TEXT("configure_camera_rig_rail"), &HandleConfigureCameraRigRail},
      {TEXT("configure_camera_rig_crane"), &HandleConfigureCameraRigCrane},
      {TEXT("add_fade_track"), &HandleAddFadeTrack},
      {TEXT("add_level_visibility_track"), &HandleAddLevelVisibilityTrack},
      {TEXT("add_material_parameter_track"), &HandleAddMaterialParameterTrack},
      {TEXT("add_particle_track"), &HandleAddParticleTrack},
      {TEXT("add_skeletal_animation_track"), &HandleAddSkeletalAnimationTrack},
      {TEXT("add_transform_track"), &HandleAddTransformTrack},
      {TEXT("add_event_track"), &HandleAddEventTrack},
      {TEXT("add_property_track"), &HandleAddPropertyTrack},
  };
  const FHandler *Handler = Handlers.Find(Action);
  return Handler && (*Handler)(Params, OutResult);
}
}
