#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/Reflection/McpPropertyReflection.h"

namespace McpSequenceCinematics {
namespace {
bool ConfigureRig(const TSharedPtr<FJsonObject> &Params, const TCHAR *Action,
                  const TCHAR *ClassPath, bool bRail,
                  TSharedPtr<FJsonObject> &OutResult) {
  UClass *RigClass = LoadClass<AActor>(nullptr, ClassPath);
  if (!RigClass) {
    OutResult = MakeResult(false, Action,
                           TEXT("Camera rig class is unavailable"),
                           TEXT("CLASS_NOT_AVAILABLE"));
    return true;
  }
  AActor *Actor = ResolveActor(Params);
  bool bSpawned = false;
  const FString Label = GetString(Params, TEXT("actorName"), TEXT("label"));
  if (Actor && !Actor->IsA(RigClass)) {
    OutResult = MakeResult(
        false, Action,
        TEXT("The resolved actor does not match the requested camera rig class"),
        TEXT("RIG_CLASS_MISMATCH"));
    return true;
  }
  if (!Actor) {
    Actor = SpawnActorInActiveWorld<AActor>(
        RigClass, FVector::ZeroVector, FRotator::ZeroRotator, Label);
    bSpawned = Actor != nullptr;
  }
  if (!Actor) {
    OutResult = MakeResult(false, Action,
                           TEXT("Failed to resolve or spawn rig actor"),
                           TEXT("ACTOR_CREATION_FAILED"));
    return true;
  }
  TArray<FString> Applied;
  if (bRail) {
    ApplyNumber(Actor, Params, TEXT("positionOnRail"),
                   TEXT("CurrentPositionOnRail"), Applied);
  } else {
    ApplyNumber(Actor, Params, TEXT("cranePitch"), TEXT("CranePitch"),
                   Applied);
    ApplyNumber(Actor, Params, TEXT("craneYaw"), TEXT("CraneYaw"), Applied);
    ApplyNumber(Actor, Params, TEXT("craneArmLength"),
                   TEXT("CraneArmLength"), Applied);
  }
  if (Applied.IsEmpty()) {
    if (bSpawned)
      Actor->Destroy();
    OutResult = MakeResult(false, Action,
                           TEXT("No valid camera rig settings were provided"),
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  Actor->Modify();
  OutResult = MakeResult(true, Action, TEXT("Camera rig configured"));
  OutResult->SetStringField(TEXT("actorName"), McpActorRef(Actor));
  OutResult->SetStringField(TEXT("actorPath"), Actor->GetPathName());
  TArray<TSharedPtr<FJsonValue>> AppliedValues;
  for (const FString &Property : Applied)
    AppliedValues.Add(MakeShared<FJsonValueString>(Property));
  OutResult->SetArrayField(TEXT("appliedProperties"), AppliedValues);
  McpHandlerUtils::AddVerification(OutResult, Actor);
  return true;
}
}

bool HandleConfigureCameraRigRail(const TSharedPtr<FJsonObject> &Params,
                                  TSharedPtr<FJsonObject> &OutResult) {
#if !MCP_HAS_CINEMATIC_CAMERA
  OutResult = MakeResult(false, TEXT("configure_camera_rig_rail"),
                         TEXT("CinematicCamera module is unavailable"),
                         TEXT("NOT_AVAILABLE"));
  return true;
#else
  return ConfigureRig(Params, TEXT("configure_camera_rig_rail"),
                      TEXT("/Script/CinematicCamera.CameraRig_Rail"), true,
                      OutResult);
#endif
}

bool HandleConfigureCameraRigCrane(const TSharedPtr<FJsonObject> &Params,
                                   TSharedPtr<FJsonObject> &OutResult) {
#if !MCP_HAS_CINEMATIC_CAMERA
  OutResult = MakeResult(false, TEXT("configure_camera_rig_crane"),
                         TEXT("CinematicCamera module is unavailable"),
                         TEXT("NOT_AVAILABLE"));
  return true;
#else
  return ConfigureRig(Params, TEXT("configure_camera_rig_crane"),
                      TEXT("/Script/CinematicCamera.CameraRig_Crane"), false,
                      OutResult);
#endif
}
}
