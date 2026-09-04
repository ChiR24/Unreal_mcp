#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UObject/UnrealType.h"

static inline bool ApplyJsonValueToProperty(void *TargetContainer, FProperty *Property,
                                            const TSharedPtr<FJsonValue> &ValueField,
                                            FString &OutError);

#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApplyScalars.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApplyObjects.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApplyArrays.h"

static inline bool ApplyJsonValueToProperty(void *TargetContainer, FProperty *Property,
                                            const TSharedPtr<FJsonValue> &ValueField,
                                            FString &OutError) {
  OutError.Empty();
  if (!TargetContainer || !Property || !ValueField) {
    OutError = TEXT("Invalid target/property/value");
    return false;
  }
  // FValueOrBBKey_* (UE 5.5+ behaviour-tree/state-tree settings such as BTTask_Wait::WaitTime)
  // are structs wrapping a DefaultValue plus an optional blackboard key. A scalar JSON value
  // targets the DefaultValue; an object still goes through the generic struct path (dogfood #59).
  if (FStructProperty *StructProperty = CastField<FStructProperty>(Property)) {
    if (StructProperty->Struct && StructProperty->Struct->GetName().StartsWith(TEXT("ValueOrBBKey_")) &&
        ValueField->Type != EJson::Object && ValueField->Type != EJson::Array) {
      if (FProperty *DefaultValueProperty = StructProperty->Struct->FindPropertyByName(TEXT("DefaultValue"))) {
        void *StructContainer = StructProperty->ContainerPtrToValuePtr<void>(TargetContainer);
        return ApplyJsonValueToProperty(StructContainer, DefaultValueProperty, ValueField, OutError);
      }
    }
  }
  if (ApplyJsonScalarValueToProperty(TargetContainer, Property, ValueField, OutError)) {
    return true;
  }
  if (!OutError.IsEmpty()) {
    return false;
  }
  if (ApplyJsonObjectValueToProperty(TargetContainer, Property, ValueField, OutError)) {
    return true;
  }
  if (!OutError.IsEmpty()) {
    return false;
  }
  if (ApplyJsonArrayValueToProperty(TargetContainer, Property, ValueField, OutError)) {
    return true;
  }
  if (!OutError.IsEmpty()) {
    return false;
  }
  OutError = TEXT("Unsupported property type for JSON assignment");
  return false;
}
