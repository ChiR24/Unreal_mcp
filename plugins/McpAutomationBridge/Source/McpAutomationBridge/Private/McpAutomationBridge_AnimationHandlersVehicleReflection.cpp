#include "McpAutomationBridge_AnimationHandlersVehicleConfiguration.h"

#include "UObject/UnrealType.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool SetVehicleNumericOnStruct(UStruct *StructType, void *Container,
                               const TArray<FString> &PropertyNames,
                               double Value) {
  if (!StructType || !Container) {
    return false;
  }

  for (const FString &PropName : PropertyNames) {
    if (PropName.IsEmpty()) {
      continue;
    }

    FProperty *Prop = StructType->FindPropertyByName(*PropName);
    if (!Prop) {
      continue;
    }

    if (FNumericProperty *NumericProp = CastField<FNumericProperty>(Prop)) {
      void *ValuePtr = Prop->ContainerPtrToValuePtr<void>(Container);
      if (!ValuePtr) {
        continue;
      }

      if (NumericProp->IsFloatingPoint()) {
        NumericProp->SetFloatingPointPropertyValue(ValuePtr, Value);
      } else {
        NumericProp->SetIntPropertyValue(ValuePtr, static_cast<int64>(Value));
      }
      return true;
    }
  }

  return false;
}

bool SetVehicleNumericOnObject(UObject *Obj,
                               const TArray<FString> &PropertyNames,
                               double Value) {
  if (!Obj) {
    return false;
  }
  return SetVehicleNumericOnStruct(Obj->GetClass(), Obj, PropertyNames, Value);
}

bool SetVehicleNestedNumericOnObject(UObject *Obj,
                                     const TArray<FString> &OuterNames,
                                     const TArray<FString> &InnerNames,
                                     double Value) {
  if (!Obj) {
    return false;
  }

  for (const FString &OuterName : OuterNames) {
    FStructProperty *OuterStructProp =
        FindFProperty<FStructProperty>(Obj->GetClass(), *OuterName);
    if (!OuterStructProp) {
      continue;
    }

    void *OuterData = OuterStructProp->ContainerPtrToValuePtr<void>(Obj);
    if (!OuterData) {
      continue;
    }

    if (SetVehicleNumericOnStruct(OuterStructProp->Struct, OuterData,
                                  InnerNames, Value)) {
      return true;
    }
  }

  return false;
}

bool ConfigureVehicleForwardGearRatios(UObject *Obj,
                                       const TArray<double> &Ratios) {
  if (!Obj) {
    return false;
  }

  const TArray<FString> TransmissionOuterNames = {
      TEXT("TransmissionSetup"), TEXT("TransmissionConfig")};
  const TArray<FString> GearArrayNames = {TEXT("ForwardGears"),
                                          TEXT("GearRatios")};

  for (const FString &OuterName : TransmissionOuterNames) {
    FStructProperty *TransmissionProp =
        FindFProperty<FStructProperty>(Obj->GetClass(), *OuterName);
    if (!TransmissionProp) {
      continue;
    }

    void *TransmissionData = TransmissionProp->ContainerPtrToValuePtr<void>(Obj);
    if (!TransmissionData) {
      continue;
    }

    for (const FString &ArrayName : GearArrayNames) {
      FArrayProperty *GearArrayProp =
          FindFProperty<FArrayProperty>(TransmissionProp->Struct, *ArrayName);
      if (!GearArrayProp) {
        continue;
      }

      void *GearArrayPtr =
          GearArrayProp->ContainerPtrToValuePtr<void>(TransmissionData);
      FScriptArrayHelper GearArrayHelper(GearArrayProp, GearArrayPtr);
      GearArrayHelper.EmptyValues();

      for (int32 GearIndex = 0; GearIndex < Ratios.Num(); ++GearIndex) {
        const int32 NewIdx = GearArrayHelper.AddValue();
        void *GearElemPtr = GearArrayHelper.GetRawPtr(NewIdx);

        if (FNumericProperty *NumericInner =
                CastField<FNumericProperty>(GearArrayProp->Inner)) {
          if (NumericInner->IsFloatingPoint()) {
            NumericInner->SetFloatingPointPropertyValue(GearElemPtr,
                                                        Ratios[GearIndex]);
          } else {
            NumericInner->SetIntPropertyValue(
                GearElemPtr,
                static_cast<int64>(FMath::RoundToInt(Ratios[GearIndex])));
          }
          continue;
        }

        if (FStructProperty *StructInner =
                CastField<FStructProperty>(GearArrayProp->Inner)) {
          StructInner->Struct->InitializeStruct(GearElemPtr);
          FProperty *RatioProp =
              StructInner->Struct->FindPropertyByName(TEXT("Ratio"));
          if (FNumericProperty *RatioNumeric =
                  CastField<FNumericProperty>(RatioProp)) {
            void *RatioPtr = RatioProp->ContainerPtrToValuePtr<void>(GearElemPtr);
            if (RatioNumeric->IsFloatingPoint()) {
              RatioNumeric->SetFloatingPointPropertyValue(RatioPtr,
                                                          Ratios[GearIndex]);
            } else {
              RatioNumeric->SetIntPropertyValue(
                  RatioPtr,
                  static_cast<int64>(FMath::RoundToInt(Ratios[GearIndex])));
            }
          }
        }
      }

      return true;
    }
  }

  return false;
}
#endif
} // namespace McpAnimationHandlers
