#include "McpAutomationBridge_AnimationHandlersVehicleConfiguration.h"

#include "UObject/UnrealType.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
int32 ConfigureVehicleWheels(UObject *VehicleMC,
                             const TSharedPtr<FJsonObject> &Payload) {
  int32 ConfiguredWheels = 0;
  const TArray<TSharedPtr<FJsonValue>> *WheelsArray = nullptr;
  if (!VehicleMC || !Payload->TryGetArrayField(TEXT("wheels"), WheelsArray) ||
      !WheelsArray) {
    return ConfiguredWheels;
  }

  FArrayProperty *WheelSetupsProp =
      FindFProperty<FArrayProperty>(VehicleMC->GetClass(), TEXT("WheelSetups"));
  FStructProperty *WheelSetupStruct =
      WheelSetupsProp ? CastField<FStructProperty>(WheelSetupsProp->Inner)
                      : nullptr;
  if (!WheelSetupsProp || !WheelSetupStruct) {
    return ConfiguredWheels;
  }

  void *WheelSetupsPtr = WheelSetupsProp->ContainerPtrToValuePtr<void>(VehicleMC);
  FScriptArrayHelper WheelArrayHelper(WheelSetupsProp, WheelSetupsPtr);
  WheelArrayHelper.EmptyValues();

  for (const TSharedPtr<FJsonValue> &WheelVal : *WheelsArray) {
    if (!WheelVal.IsValid() || WheelVal->Type != EJson::Object) {
      continue;
    }

    const TSharedPtr<FJsonObject> WheelObj = WheelVal->AsObject();
    if (!WheelObj.IsValid()) {
      continue;
    }

    const int32 NewIndex = WheelArrayHelper.AddValue();
    void *WheelSetupPtr = WheelArrayHelper.GetRawPtr(NewIndex);
    WheelSetupStruct->Struct->InitializeStruct(WheelSetupPtr);

    FString BoneName;
    if (WheelObj->TryGetStringField(TEXT("boneName"), BoneName) &&
        !BoneName.IsEmpty()) {
      if (FNameProperty *BoneProp = FindFProperty<FNameProperty>(
              WheelSetupStruct->Struct, TEXT("BoneName"))) {
        BoneProp->SetPropertyValue_InContainer(WheelSetupPtr, FName(*BoneName));
      }
    }

    const TSharedPtr<FJsonObject> *OffsetObj = nullptr;
    if (WheelObj->TryGetObjectField(TEXT("offset"), OffsetObj) && OffsetObj &&
        (*OffsetObj).IsValid()) {
      double X = 0.0, Y = 0.0, Z = 0.0;
      (*OffsetObj)->TryGetNumberField(TEXT("x"), X);
      (*OffsetObj)->TryGetNumberField(TEXT("y"), Y);
      (*OffsetObj)->TryGetNumberField(TEXT("z"), Z);

      if (FStructProperty *OffsetProp = FindFProperty<FStructProperty>(
              WheelSetupStruct->Struct, TEXT("AdditionalOffset"))) {
        if (OffsetProp->Struct == TBaseStructure<FVector>::Get()) {
          if (FVector *OffsetPtr =
                  OffsetProp->ContainerPtrToValuePtr<FVector>(WheelSetupPtr)) {
            *OffsetPtr = FVector(static_cast<float>(X), static_cast<float>(Y),
                                 static_cast<float>(Z));
          }
        }
      }
    }

#if MCP_HAS_VEHICLE_WHEEL
    if (FClassProperty *WheelClassProp = FindFProperty<FClassProperty>(
            WheelSetupStruct->Struct, TEXT("WheelClass"))) {
      WheelClassProp->SetPropertyValue_InContainer(WheelSetupPtr,
                                                   UVehicleWheel::StaticClass());
    }
#endif

    double Radius = 0.0;
    if (WheelObj->TryGetNumberField(TEXT("radius"), Radius)) {
      SetVehicleNumericOnStruct(WheelSetupStruct->Struct, WheelSetupPtr,
                                {TEXT("WheelRadius"), TEXT("Radius")}, Radius);
    }

    double Width = 0.0;
    if (WheelObj->TryGetNumberField(TEXT("width"), Width)) {
      SetVehicleNumericOnStruct(WheelSetupStruct->Struct, WheelSetupPtr,
                                {TEXT("WheelWidth"), TEXT("Width")}, Width);
    }

    double Friction = 0.0;
    if (WheelObj->TryGetNumberField(TEXT("friction"), Friction)) {
      SetVehicleNumericOnStruct(
          WheelSetupStruct->Struct, WheelSetupPtr,
          {TEXT("Friction"), TEXT("FrictionMultiplier"),
           TEXT("TireFrictionScale")},
          Friction);
    }

    ++ConfiguredWheels;
  }

  return ConfiguredWheels;
}

void ConfigureVehicleEngine(UObject *VehicleMC,
                            const TSharedPtr<FJsonObject> &Payload) {
  const TSharedPtr<FJsonObject> *EngineObj = nullptr;
  if (!VehicleMC || !Payload->TryGetObjectField(TEXT("engine"), EngineObj) ||
      !EngineObj || !(*EngineObj).IsValid()) {
    return;
  }

  double MaxRPM = 0.0;
  if ((*EngineObj)->TryGetNumberField(TEXT("maxRPM"), MaxRPM)) {
    SetVehicleNumericOnObject(VehicleMC,
                              {TEXT("MaxEngineRPM"), TEXT("EngineMaxRPM")},
                              MaxRPM);
    SetVehicleNestedNumericOnObject(
        VehicleMC, {TEXT("EngineSetup"), TEXT("EngineConfig")},
        {TEXT("MaxRPM"), TEXT("MaxEngineRPM")}, MaxRPM);
  }

  double MaxTorque = 0.0;
  if ((*EngineObj)->TryGetNumberField(TEXT("maxTorque"), MaxTorque)) {
    SetVehicleNumericOnObject(
        VehicleMC, {TEXT("MaxEngineTorque"), TEXT("EngineMaxTorque")},
        MaxTorque);
    SetVehicleNestedNumericOnObject(
        VehicleMC, {TEXT("EngineSetup"), TEXT("EngineConfig")},
        {TEXT("MaxTorque"), TEXT("PeakTorque"), TEXT("MaxEngineTorque")},
        MaxTorque);
  }

  double EngineGears = 0.0;
  if ((*EngineObj)->TryGetNumberField(TEXT("gears"), EngineGears)) {
    SetVehicleNestedNumericOnObject(
        VehicleMC, {TEXT("TransmissionSetup"), TEXT("TransmissionConfig")},
        {TEXT("NumForwardGears"), TEXT("ForwardGearCount")}, EngineGears);
  }
}

void ConfigureVehicleTransmission(UObject *VehicleMC,
                                  const TSharedPtr<FJsonObject> &Payload) {
  const TSharedPtr<FJsonObject> *TransmissionObj = nullptr;
  if (!VehicleMC ||
      !Payload->TryGetObjectField(TEXT("transmission"), TransmissionObj) ||
      !TransmissionObj || !(*TransmissionObj).IsValid()) {
    return;
  }

  double FinalDrive = 0.0;
  if ((*TransmissionObj)->TryGetNumberField(TEXT("finalDrive"), FinalDrive) ||
      (*TransmissionObj)->TryGetNumberField(TEXT("finalDriveRatio"),
                                            FinalDrive)) {
    SetVehicleNestedNumericOnObject(
        VehicleMC, {TEXT("TransmissionSetup"), TEXT("TransmissionConfig")},
        {TEXT("FinalRatio"), TEXT("FinalDrive"), TEXT("FinalDriveRatio")},
        FinalDrive);
  }

  const TArray<TSharedPtr<FJsonValue>> *GearRatiosArray = nullptr;
  if ((*TransmissionObj)->TryGetArrayField(TEXT("gearRatios"),
                                           GearRatiosArray) &&
      GearRatiosArray) {
    TArray<double> Ratios;
    Ratios.Reserve(GearRatiosArray->Num());
    for (const TSharedPtr<FJsonValue> &RatioVal : *GearRatiosArray) {
      if (RatioVal.IsValid() && RatioVal->Type == EJson::Number) {
        Ratios.Add(RatioVal->AsNumber());
      }
    }
    if (Ratios.Num() > 0) {
      ConfigureVehicleForwardGearRatios(VehicleMC, Ratios);
    }
  }
}
#endif
} // namespace McpAnimationHandlers
