#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BlueprintCreation/McpAutomationBridge_BlueprintCreationHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersNestedPropertyPath.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApply.h"


#include "Engine/Blueprint.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintCreationHandlers {
namespace {

// Every name is accounted for. A struct given as a JSON object ({"RotOscillation": {"Pitch":
// {"Amplitude": 0.8}}}) used to become empty text and vanish, as did an unknown name or a value
// the property refused, while create answered success. An instanced subobject still takes a
// nested object of its own properties; every other value goes through the writer set_property uses.
void ApplyPropertiesToObject(UObject *TargetObject,
                             const TSharedPtr<FJsonObject> &Properties,
                             const FString &Prefix, TArray<FString> &OutApplied,
                             TArray<FString> &OutFailed) {
  if (!TargetObject || !Properties.IsValid()) {
    return;
  }

  for (const auto &Pair : Properties->Values) {
    const FString Name = Prefix + Pair.Key;
    // A dotted key reaches a component or struct member as set_property's paths do
    // ("CapsuleComponent.CapsuleRadius", "BodyInstance.CollisionEnabled"); its first segment may also be a
    // component's object name ("CollisionCylinder.CapsuleRadius"), which is what the editor shows.
    const FString Key(Pair.Key.Len(), *Pair.Key); // 5.8 keys are shared strings, not FString
    FString Head, Tail;
    if (Key.Split(TEXT("."), &Head, &Tail)) {
      UObject *Holder = TargetObject;
      FString Path = Key;
      if (!FindFProperty<FProperty>(TargetObject->GetClass(), FName(*Head))) {
        if (UObject *Subobject = TargetObject->GetDefaultSubobjectByName(FName(*Head))) {
          Holder = Subobject;
          Path = Tail;
        }
      }
      void *Container = nullptr;
      FString Error;
      FProperty *Nested = ResolveNestedPropertyPath(Holder, Path, Container, Error);
      if (Nested && ApplyJsonValueToProperty(Container, Nested, Pair.Value, Error)) {
        OutApplied.Add(Name);
      } else {
        OutFailed.Add(FString::Printf(TEXT("%s: %s"), *Name, *Error));
      }
      continue;
    }
    FProperty *Property =
        TargetObject->GetClass()->FindPropertyByName(*Pair.Key);
    if (!Property) {
      OutFailed.Add(FString::Printf(TEXT("%s: %s has no such property"), *Name,
                                    *TargetObject->GetClass()->GetName()));
      continue;
    }

    FObjectProperty *ObjectProperty = CastField<FObjectProperty>(Property);
    if (ObjectProperty && Pair.Value->Type == EJson::Object) {
      UObject *ChildObject =
          ObjectProperty->GetObjectPropertyValue_InContainer(TargetObject);
      if (ChildObject) {
        ApplyPropertiesToObject(ChildObject, Pair.Value->AsObject(),
                                Name + TEXT("."), OutApplied, OutFailed);
      } else {
        OutFailed.Add(FString::Printf(
            TEXT("%s: holds no object whose properties could be set"), *Name));
      }
      continue;
    }

    FString Error;
    if (ApplyJsonValueToProperty(TargetObject, Property, Pair.Value, Error)) {
      OutApplied.Add(Name);
    } else {
      OutFailed.Add(FString::Printf(TEXT("%s: %s"), *Name, *Error));
    }
  }
}

}

void ApplyBlueprintProperties(UBlueprint *Blueprint,
                              const TSharedPtr<FJsonObject> &Payload,
                              TArray<FString> &OutApplied,
                              TArray<FString> &OutFailed) {
  const TSharedPtr<FJsonObject> *Properties = nullptr;
  if (!Blueprint || !Blueprint->GeneratedClass ||
      !Payload->TryGetObjectField(TEXT("properties"), Properties)) {
    return;
  }

  UObject *ClassDefaultObject =
      Blueprint->GeneratedClass->GetDefaultObject();
  if (ClassDefaultObject) {
    ApplyPropertiesToObject(ClassDefaultObject, *Properties, FString(),
                            OutApplied, OutFailed);
    Blueprint->Modify();
  }
}

}
