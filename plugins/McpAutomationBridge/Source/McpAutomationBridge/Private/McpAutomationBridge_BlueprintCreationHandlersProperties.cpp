#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BlueprintCreationHandlersPrivate.h"

#if WITH_EDITOR

#include "Engine/Blueprint.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintCreationHandlers {
namespace {

void ApplyPropertiesToObject(UObject *TargetObject,
                             const TSharedPtr<FJsonObject> &Properties) {
  if (!TargetObject || !Properties.IsValid()) {
    return;
  }

  for (const auto &Pair : Properties->Values) {
    FProperty *Property =
        TargetObject->GetClass()->FindPropertyByName(*Pair.Key);
    if (!Property) {
      continue;
    }

    if (FObjectProperty *ObjectProperty =
            CastField<FObjectProperty>(Property)) {
      if (Pair.Value->Type == EJson::Object) {
        UObject *ChildObject =
            ObjectProperty->GetObjectPropertyValue_InContainer(TargetObject);
        if (ChildObject) {
          ApplyPropertiesToObject(ChildObject, Pair.Value->AsObject());
        }
        continue;
      }
    }

    FString TextValue;
    if (Pair.Value->Type == EJson::String) {
      TextValue = Pair.Value->AsString();
    } else if (Pair.Value->Type == EJson::Number) {
      const double Value = Pair.Value->AsNumber();
      if (Property->IsA<FIntProperty>() ||
          Property->IsA<FInt64Property>() ||
          Property->IsA<FByteProperty>()) {
        TextValue = FString::Printf(TEXT("%lld"), (long long)Value);
      } else {
        TextValue = FString::SanitizeFloat(Value);
      }
    } else if (Pair.Value->Type == EJson::Boolean) {
      TextValue = Pair.Value->AsBool() ? TEXT("True") : TEXT("False");
    }

    if (!TextValue.IsEmpty()) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      Property->ImportText_Direct(
          *TextValue, Property->ContainerPtrToValuePtr<void>(TargetObject),
          TargetObject, 0);
#else
      Property->ImportText(
          *TextValue, Property->ContainerPtrToValuePtr<void>(TargetObject),
          PPF_None, TargetObject);
#endif
    }
  }
}

}

void ApplyBlueprintProperties(
    UBlueprint *Blueprint, const TSharedPtr<FJsonObject> &Payload) {
  if (!Blueprint || !Blueprint->GeneratedClass) {
    return;
  }

  const TSharedPtr<FJsonObject> *Properties = nullptr;
  if (!Payload->TryGetObjectField(TEXT("properties"), Properties)) {
    return;
  }

  UObject *ClassDefaultObject =
      Blueprint->GeneratedClass->GetDefaultObject();
  if (ClassDefaultObject) {
    ApplyPropertiesToObject(ClassDefaultObject, *Properties);
    Blueprint->Modify();
  }
}

}

#endif
