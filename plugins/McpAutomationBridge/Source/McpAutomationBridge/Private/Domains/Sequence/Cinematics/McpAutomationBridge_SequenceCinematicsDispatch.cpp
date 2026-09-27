#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpSequenceCinematics {
namespace {
FString NormalizeAction(const FString &Action) {
  FString Lower = Action.ToLower();
  if (Lower.StartsWith(TEXT("sequence_"))) {
    Lower.RightChopInline(9);
  }
  return Lower;
}
}

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
  const FHandler *Handler = Handlers.Find(NormalizeAction(Action));
  return Handler && (*Handler)(Params, OutResult);
}
}
