#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UBlueprint;
namespace McpScsPropagate { struct FDefaults; }

class FSCSHandlers {
public:
  // Whether FinalizeBlueprintSCSChange saves; a dispatcher holds it to the payload's save for the call (TGuardValue).
  static bool bSaveScsChanges;
  static void FinalizeBlueprintSCSChange(UBlueprint *Blueprint,
                                         bool &bOutCompiled, bool &bOutSaved);
  // FinalizeBlueprintSCSChange for a template edit Defaults captured: the new defaults pushed to placed copies before
  // and after the compile (which re-creates them), and the copies that took them written to Result as instancesUpdated
  // and updatedInstances when there are any.
  static void FinalizeTemplateEdit(UBlueprint *Blueprint, const McpScsPropagate::FDefaults &Defaults,
                                   const TSharedPtr<FJsonObject> &Result, bool &bOutCompiled, bool &bOutSaved);

  static TSharedPtr<FJsonObject>
  AddSCSComponent(const FString &BlueprintPath, const FString &ComponentClass,
                  const FString &ComponentName,
                  const FString &ParentComponentName,
                  const FString &MeshPath = FString(),
                  const FString &MaterialPath = FString());

  static TSharedPtr<FJsonObject>
  RemoveSCSComponent(const FString &BlueprintPath,
                     const FString &ComponentName);

  static TSharedPtr<FJsonObject>
  RemoveSCSComponents(const FString &BlueprintPath,
                      const TArray<FString> &ComponentNames);

  static TSharedPtr<FJsonObject>
  ReparentSCSComponent(const FString &BlueprintPath,
                       const FString &ComponentName,
                       const FString &NewParentName);

  static TSharedPtr<FJsonObject>
  SetSCSComponentTransform(const FString &BlueprintPath,
                           const FString &ComponentName,
                           const TSharedPtr<FJsonObject> &TransformData);

  static TSharedPtr<FJsonObject> SetSCSComponentProperty(
      const FString &BlueprintPath, const FString &ComponentName,
      const FString &PropertyName, const TSharedPtr<FJsonValue> &PropertyValue);
};
