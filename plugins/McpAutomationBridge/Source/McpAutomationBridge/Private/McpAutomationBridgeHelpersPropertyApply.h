#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UObject/UnrealType.h"

static inline bool ApplyJsonValueToProperty(void *TargetContainer, FProperty *Property,
                                            const TSharedPtr<FJsonValue> &ValueField,
                                            FString &OutError);

#include "McpAutomationBridgeHelpersPropertyApplyScalars.h"
#include "McpAutomationBridgeHelpersPropertyApplyObjects.h"
#include "McpAutomationBridgeHelpersPropertyApplyArrays.h"

static inline bool ApplyJsonValueToProperty(void *TargetContainer, FProperty *Property,
                                            const TSharedPtr<FJsonValue> &ValueField,
                                            FString &OutError) {
  OutError.Empty();
  if (!TargetContainer || !Property || !ValueField) {
    OutError = TEXT("Invalid target/property/value");
    return false;
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
