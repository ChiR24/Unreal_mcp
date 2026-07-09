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

bool TryHandleCinematics(UMcpAutomationBridgeSubsystem *Self,
                         const FString &Action,
                         const TSharedPtr<FJsonObject> &Params,
                         TSharedPtr<FJsonObject> &OutResult) {
  const FString Lower = NormalizeAction(Action);
  if (Lower == TEXT("create_master_sequence"))
    return HandleCreateMasterSequence(Self, Params, OutResult);
  if (Lower == TEXT("add_subsequence"))
    return HandleAddSubsequence(Self, Params, OutResult);
  if (Lower == TEXT("add_shot_track"))
    return HandleAddShotTrack(Self, Params, OutResult);
  if (Lower == TEXT("configure_shot_settings"))
    return HandleConfigureShotSettings(Self, Params, OutResult);
  if (Lower == TEXT("create_cine_camera_actor"))
    return HandleCreateCineCameraActor(Self, Params, OutResult);
  if (Lower == TEXT("configure_camera_settings"))
    return HandleConfigureCameraSettings(Self, Params, OutResult);
  if (Lower == TEXT("add_camera_cut_track"))
    return HandleAddCameraCutTrack(Self, Params, OutResult);
  if (Lower == TEXT("add_camera_shake_track"))
    return HandleAddCameraShakeTrack(Self, Params, OutResult);
  if (Lower == TEXT("configure_camera_rig_rail"))
    return HandleConfigureCameraRigRail(Self, Params, OutResult);
  if (Lower == TEXT("configure_camera_rig_crane"))
    return HandleConfigureCameraRigCrane(Self, Params, OutResult);
  if (Lower == TEXT("add_fade_track"))
    return HandleAddFadeTrack(Self, Params, OutResult);
  if (Lower == TEXT("add_level_visibility_track"))
    return HandleAddLevelVisibilityTrack(Self, Params, OutResult);
  if (Lower == TEXT("add_material_parameter_track"))
    return HandleAddMaterialParameterTrack(Self, Params, OutResult);
  if (Lower == TEXT("add_particle_track"))
    return HandleAddParticleTrack(Self, Params, OutResult);
  if (Lower == TEXT("add_skeletal_animation_track"))
    return HandleAddSkeletalAnimationTrack(Self, Params, OutResult);
  if (Lower == TEXT("add_transform_track"))
    return HandleAddTransformTrack(Self, Params, OutResult);
  if (Lower == TEXT("add_event_track"))
    return HandleAddEventTrack(Self, Params, OutResult);
  if (Lower == TEXT("add_property_track"))
    return HandleAddPropertyTrack(Self, Params, OutResult);
  return false;
}
}
