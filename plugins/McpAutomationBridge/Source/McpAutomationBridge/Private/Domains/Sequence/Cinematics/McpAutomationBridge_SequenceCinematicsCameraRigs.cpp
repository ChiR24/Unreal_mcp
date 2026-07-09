#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

#if WITH_EDITOR
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#endif

namespace McpSequenceCinematics {
#if WITH_EDITOR
namespace {
bool ApplyRigNumber(UObject *Object, const TSharedPtr<FJsonObject> &Params,
                    const TCHAR *Field, const TCHAR *PropertyPath,
                    TArray<FString> &Applied) {
  double Value = 0.0;
  if (!Object || !Params.IsValid() ||
      !Params->TryGetNumberField(Field, Value) || !FMath::IsFinite(Value))
    return false;
  void *Container = nullptr;
  FString Error;
  FProperty *Property =
      ResolveNestedPropertyPath(Object, PropertyPath, Container, Error);
  const bool bApplied = ApplyJsonValueToProperty(
      Container, Property, MakeShared<FJsonValueNumber>(Value), Error);
  if (bApplied)
    Applied.Add(PropertyPath);
  return bApplied;
}

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
    ApplyRigNumber(Actor, Params, TEXT("positionOnRail"),
                   TEXT("CurrentPositionOnRail"), Applied);
  } else {
    ApplyRigNumber(Actor, Params, TEXT("cranePitch"), TEXT("CranePitch"),
                   Applied);
    ApplyRigNumber(Actor, Params, TEXT("craneYaw"), TEXT("CraneYaw"), Applied);
    ApplyRigNumber(Actor, Params, TEXT("craneArmLength"),
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
  OutResult->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
  OutResult->SetStringField(TEXT("actorPath"), Actor->GetPathName());
  TArray<TSharedPtr<FJsonValue>> AppliedValues;
  for (const FString &Property : Applied)
    AppliedValues.Add(MakeShared<FJsonValueString>(Property));
  OutResult->SetArrayField(TEXT("appliedProperties"), AppliedValues);
  McpHandlerUtils::AddVerification(OutResult, Actor);
  return true;
}
}
#endif

bool HandleConfigureCameraRigRail(UMcpAutomationBridgeSubsystem *Self,
                                  const TSharedPtr<FJsonObject> &Params,
                                  TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if !MCP_HAS_CINEMATIC_CAMERA
  OutResult = MakeResult(false, TEXT("configure_camera_rig_rail"),
                         TEXT("CinematicCamera module is unavailable"),
                         TEXT("NOT_AVAILABLE"));
  return true;
#elif WITH_EDITOR
  return ConfigureRig(Params, TEXT("configure_camera_rig_rail"),
                      TEXT("/Script/CinematicCamera.CameraRig_Rail"), true,
                      OutResult);
#else
  OutResult = MakeResult(false, TEXT("configure_camera_rig_rail"),
                         TEXT("Editor build required"), TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool HandleConfigureCameraRigCrane(UMcpAutomationBridgeSubsystem *Self,
                                   const TSharedPtr<FJsonObject> &Params,
                                   TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if !MCP_HAS_CINEMATIC_CAMERA
  OutResult = MakeResult(false, TEXT("configure_camera_rig_crane"),
                         TEXT("CinematicCamera module is unavailable"),
                         TEXT("NOT_AVAILABLE"));
  return true;
#elif WITH_EDITOR
  return ConfigureRig(Params, TEXT("configure_camera_rig_crane"),
                      TEXT("/Script/CinematicCamera.CameraRig_Crane"), false,
                      OutResult);
#else
  OutResult = MakeResult(false, TEXT("configure_camera_rig_crane"),
                         TEXT("Editor build required"), TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
}
