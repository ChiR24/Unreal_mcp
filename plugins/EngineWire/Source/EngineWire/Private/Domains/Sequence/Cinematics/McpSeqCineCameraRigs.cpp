#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/Cinematics/McpSeqCine.h"

#include "Domains/Sequence/McpSeqEditorSupport.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Foundation/McpScopedEditorTransaction.h"

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
  // Whether a setting was sent is known from the payload: a call without one is refused here, before the undo step
  // and the spawn (it spawned a rig, destroyed it and left an empty step).
  const auto HasNumber = [&Params](const TCHAR *Field) {
    double Value = 0.0;
    return Params->TryGetNumberField(Field, Value) && FMath::IsFinite(Value);
  };
  if (bRail ? !HasNumber(TEXT("positionOnRail"))
            : !HasNumber(TEXT("cranePitch")) && !HasNumber(TEXT("craneYaw")) && !HasNumber(TEXT("craneArmLength"))) {
    OutResult = MakeResult(false, Action,
                           TEXT("No valid camera rig settings were provided"),
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // Modify() ran after the writes with no transaction open, so undo restored nothing; the spawn and the
  // settings are one undo step now.
  FMcpScopedEditorTransaction Transaction(FText::FromString(bRail ? TEXT("Configure Camera Rail") : TEXT("Configure Camera Crane")),
                                          EMcpMutationDurability::EditorStateOnly, TArray<UObject *>());
  if (Actor) {
    Actor->Modify();
  }
  if (!Actor) {
    Actor = SpawnActorInActiveWorld<AActor>(
        RigClass, FVector::ZeroVector, FRotator::ZeroRotator, Label);
    bSpawned = Actor != nullptr;
  }
  if (!Actor) {
    Transaction.Cancel();
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
    // Every value sent was refused by its property (one past a float's range): known only once tried.
    if (bSpawned)
      Actor->Destroy();
    Transaction.Cancel();
    OutResult = MakeResult(false, Action,
                           TEXT("No valid camera rig settings were provided"),
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  OutResult = MakeResult(true, Action, TEXT("Camera rig configured"));
  OutResult->SetBoolField(TEXT("spawned"), bSpawned);
  Transaction.DescribeInto(OutResult);
  OutResult->SetStringField(TEXT("actorName"), McpActorRef(Actor));
  OutResult->SetStringField(TEXT("actorPath"), Actor->GetPathName());
  OutResult->SetArrayField(TEXT("appliedProperties"), McpHandlerUtils::ToJsonStringArray(Applied));
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
