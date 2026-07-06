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
UActorComponent *FindComponentByClassFragment(AActor *Actor, const TCHAR *Fragment) {
  if (!Actor) return nullptr;
  TArray<UActorComponent *> Components;
  Actor->GetComponents(Components);
  for (UActorComponent *Component : Components) {
    if (Component && Component->GetClass()->GetName().Contains(Fragment))
      return Component;
  }
  return nullptr;
}

bool ApplyNumber(UObject *Object, const TSharedPtr<FJsonObject> &Params,
                 const TCHAR *Field, const TCHAR *PropertyPath,
                 TArray<FString> &Applied) {
  double Value = 0.0;
  if (!Object || !Params.IsValid() || !Params->TryGetNumberField(Field, Value)) {
    return false;
  }
  void *Container = nullptr;
  FString Error;
  FProperty *Property = ResolveNestedPropertyPath(Object, PropertyPath, Container, Error);
  const bool bOk = ApplyJsonValueToProperty(
      Container, Property, MakeShared<FJsonValueNumber>(Value), Error);
  if (bOk) Applied.Add(PropertyPath);
  return bOk;
}

bool ApplyNestedNumber(UObject *Object, const TSharedPtr<FJsonObject> &Params,
                       const TCHAR *ObjectField, const TCHAR *Field,
                       const TCHAR *PropertyPath, TArray<FString> &Applied) {
  const TSharedPtr<FJsonObject> *Nested = nullptr;
  return Params.IsValid() && Params->TryGetObjectField(ObjectField, Nested) &&
         Nested && ApplyNumber(Object, *Nested, Field, PropertyPath, Applied);
}

void ApplyNumberAliases(UObject *Object, const TSharedPtr<FJsonObject> &Params,
                        const TCHAR *PrimaryField, const TCHAR *AliasField,
                        const TCHAR *NestedObject, const TCHAR *PropertyPath,
                        TArray<FString> &Applied) {
  if (ApplyNumber(Object, Params, PrimaryField, PropertyPath, Applied)) return;
  if (AliasField &&
      ApplyNumber(Object, Params, AliasField, PropertyPath, Applied)) return;
  if (ApplyNestedNumber(Object, Params, NestedObject, PrimaryField, PropertyPath,
                        Applied))
    return;
  if (AliasField)
    ApplyNestedNumber(Object, Params, NestedObject, AliasField, PropertyPath,
                      Applied);
}

int32 ApplyCameraSettings(AActor *Actor, const TSharedPtr<FJsonObject> &Params,
                          TSharedPtr<FJsonObject> &Result) {
  UActorComponent *Camera = FindComponentByClassFragment(Actor, TEXT("CineCameraComponent"));
  TArray<FString> Applied;
  if (!Camera) {
    Result->SetArrayField(TEXT("appliedProperties"), TArray<TSharedPtr<FJsonValue>>());
    return INDEX_NONE;
  }
  ApplyNumberAliases(Camera, Params, TEXT("currentFocalLength"),
                     TEXT("focalLength"), TEXT("lens"),
                     TEXT("CurrentFocalLength"), Applied);
  ApplyNumberAliases(Camera, Params, TEXT("currentAperture"), TEXT("aperture"),
                     TEXT("lens"), TEXT("CurrentAperture"), Applied);
  ApplyNumberAliases(Camera, Params, TEXT("sensorWidth"), nullptr,
                     TEXT("filmback"), TEXT("Filmback.SensorWidth"), Applied);
  ApplyNumberAliases(Camera, Params, TEXT("sensorHeight"), nullptr,
                     TEXT("filmback"), TEXT("Filmback.SensorHeight"), Applied);
  ApplyNumberAliases(Camera, Params, TEXT("manualFocusDistance"),
                     TEXT("focusDistance"), TEXT("focus"),
                     TEXT("FocusSettings.ManualFocusDistance"), Applied);
  TArray<TSharedPtr<FJsonValue>> Values;
  for (const FString &Name : Applied) Values.Add(MakeShared<FJsonValueString>(Name));
  Result->SetArrayField(TEXT("appliedProperties"), Values);
  return Applied.Num();
}

}
#endif

bool HandleCreateCineCameraActor(UMcpAutomationBridgeSubsystem *Self,
                                 const TSharedPtr<FJsonObject> &Params,
                                 TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if !MCP_HAS_CINEMATIC_CAMERA
  OutResult = MakeResult(false, TEXT("create_cine_camera_actor"),
                         TEXT("CinematicCamera module is unavailable"),
                         TEXT("NOT_AVAILABLE"));
  return true;
#elif WITH_EDITOR
  ULevelSequence *Sequence = nullptr;
  const FString ExplicitSequencePath =
      GetString(Params, TEXT("sequencePath"), TEXT("path"));
  if (!ExplicitSequencePath.IsEmpty()) {
    Sequence = LoadSequence(Params, OutResult);
    if (!Sequence) return true;
  }
  UClass *CameraClass =
      LoadClass<AActor>(nullptr, TEXT("/Script/CinematicCamera.CineCameraActor"));
  if (!CameraClass) {
    OutResult = MakeResult(false, TEXT("create_cine_camera_actor"),
                           TEXT("CineCameraActor class is unavailable"),
                           TEXT("CLASS_NOT_AVAILABLE"));
    return true;
  }
  FVector Location = FVector::ZeroVector;
  FRotator Rotation = FRotator::ZeroRotator;
  const TSharedPtr<FJsonObject> *LocationObj = nullptr;
  const TSharedPtr<FJsonObject> *RotationObj = nullptr;
  if (Params->TryGetObjectField(TEXT("location"), LocationObj) && LocationObj)
    McpPropertyReflection::JsonToVector(*LocationObj, Location);
  if (Params->TryGetObjectField(TEXT("rotation"), RotationObj) && RotationObj)
    McpPropertyReflection::JsonToRotator(*RotationObj, Rotation);
  const FString Label = GetString(Params, TEXT("actorName"), TEXT("label"));
  AActor *Actor = SpawnActorInActiveWorld<AActor>(CameraClass, Location, Rotation, Label);
  if (!Actor) {
    OutResult = MakeResult(false, TEXT("create_cine_camera_actor"),
                           TEXT("Failed to spawn CineCameraActor"),
                           TEXT("ACTOR_CREATION_FAILED"));
    return true;
  }
  OutResult = MakeResult(true, TEXT("create_cine_camera_actor"),
                         TEXT("Cine camera actor created"));
  if (ApplyCameraSettings(Actor, Params, OutResult) == INDEX_NONE) {
    Actor->Destroy();
    OutResult = MakeResult(false, TEXT("create_cine_camera_actor"),
                           TEXT("Spawned actor has no CineCameraComponent"),
                           TEXT("CAMERA_COMPONENT_NOT_FOUND"));
    return true;
  }
  OutResult->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
  OutResult->SetStringField(TEXT("actorPath"), Actor->GetPathName());
  if (Sequence) {
    const FGuid Guid = ResolveOrCreateBinding(Sequence, Actor);
    OutResult->SetStringField(TEXT("bindingGuid"), Guid.ToString());
  }
  McpHandlerUtils::AddVerification(OutResult, Actor);
  return true;
#else
  OutResult = MakeResult(false, TEXT("create_cine_camera_actor"),
                         TEXT("Editor build required"), TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool HandleConfigureCameraSettings(UMcpAutomationBridgeSubsystem *Self,
                                   const TSharedPtr<FJsonObject> &Params,
                                   TSharedPtr<FJsonObject> &OutResult) {
  (void)Self;
#if !MCP_HAS_CINEMATIC_CAMERA
  OutResult = MakeResult(false, TEXT("configure_camera_settings"),
                         TEXT("CinematicCamera module is unavailable"),
                         TEXT("NOT_AVAILABLE"));
  return true;
#elif WITH_EDITOR
  AActor *Actor = ResolveActor(Params);
  if (!Actor) {
    OutResult = MakeResult(false, TEXT("configure_camera_settings"),
                           TEXT("camera actor not found"), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }
  OutResult = MakeResult(true, TEXT("configure_camera_settings"),
                         TEXT("Camera settings configured"));
  const int32 AppliedCount = ApplyCameraSettings(Actor, Params, OutResult);
  if (AppliedCount == INDEX_NONE) {
    OutResult = MakeResult(false, TEXT("configure_camera_settings"),
                           TEXT("Actor has no CineCameraComponent"),
                           TEXT("CAMERA_COMPONENT_NOT_FOUND"));
    return true;
  }
  if (AppliedCount == 0) {
    OutResult = MakeResult(false, TEXT("configure_camera_settings"),
                           TEXT("No valid camera settings were provided"),
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  Actor->Modify();
  McpHandlerUtils::AddVerification(OutResult, Actor);
  return true;
#else
  OutResult = MakeResult(false, TEXT("configure_camera_settings"),
                         TEXT("Editor build required"), TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

}
