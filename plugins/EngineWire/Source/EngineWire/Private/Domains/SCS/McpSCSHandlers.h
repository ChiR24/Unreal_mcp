#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UBlueprint;

class FSCSHandlers {
public:
  static void FinalizeBlueprintSCSChange(UBlueprint *Blueprint,
                                         bool &bOutCompiled, bool &bOutSaved);

  static TSharedPtr<FJsonObject> GetBlueprintSCS(const FString &BlueprintPath);

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
