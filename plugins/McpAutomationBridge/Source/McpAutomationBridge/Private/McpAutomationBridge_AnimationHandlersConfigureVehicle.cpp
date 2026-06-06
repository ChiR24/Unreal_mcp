#include "McpAutomationBridge_AnimationHandlersActionContext.h"
#include "McpAutomationBridge_AnimationHandlersVehicleConfiguration.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool HandleAnimationConfigureVehicleAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;


    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
        ActorName.IsEmpty()) {
      // Backward compatibility for older payloads.
      Payload->TryGetStringField(TEXT("vehicleName"), ActorName);
    }

    if (ActorName.IsEmpty()) {
      Message = TEXT("actorName is required for configure_vehicle");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
#if MCP_HAS_WHEELED_VEHICLE_4W
      AActor *TargetActor = Context.FindActorByName(ActorName);
      if (!TargetActor) {
        Message = FString::Printf(TEXT("Actor not found: %s"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        FString VehicleType = TEXT("WheeledVehicle4W");
        Payload->TryGetStringField(TEXT("vehicleType"), VehicleType);

        UWheeledVehicleMovementComponent4W *VehicleMC =
            TargetActor->FindComponentByClass<UWheeledVehicleMovementComponent4W>();
        bool bCreatedComponent = false;
        if (!VehicleMC) {
          VehicleMC = NewObject<UWheeledVehicleMovementComponent4W>(
              TargetActor, TEXT("MCP_VehicleMovement4W"));
          if (VehicleMC) {
            TargetActor->AddInstanceComponent(VehicleMC);
            VehicleMC->RegisterComponent();
            bCreatedComponent = true;
          }
        }

        if (!VehicleMC) {
          Message = TEXT("Failed to create/get UWheeledVehicleMovementComponent4W");
          ErrorCode = TEXT("COMPONENT_CREATION_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          const int32 ConfiguredWheels = ConfigureVehicleWheels(VehicleMC, Payload);
          ConfigureVehicleEngine(VehicleMC, Payload);
          ConfigureVehicleTransmission(VehicleMC, Payload);

          double Mass = 0.0;
          if (Payload->TryGetNumberField(TEXT("mass"), Mass) && Mass > 0.0) {
            SetVehicleNumericOnObject(VehicleMC, {TEXT("Mass"), TEXT("VehicleMass")},
                               Mass);

            if (UPrimitiveComponent *PrimitiveRoot =
                    Cast<UPrimitiveComponent>(TargetActor->GetRootComponent())) {
              PrimitiveRoot->SetMassOverrideInKg(NAME_None,
                                                 static_cast<float>(Mass), true);
            }
          }

          double DragCoefficient = 0.0;
          if (Payload->TryGetNumberField(TEXT("dragCoefficient"),
                                         DragCoefficient)) {
            SetVehicleNumericOnObject(VehicleMC,
                               {TEXT("DragCoefficient"), TEXT("DragCoeff"),
                                TEXT("AerodynamicDragCoefficient")},
                               DragCoefficient);
            SetVehicleNestedNumericOnObject(
                VehicleMC,
                {TEXT("AerofoilSetup"), TEXT("AerodynamicsSetup")},
                {TEXT("DragCoefficient"), TEXT("DragCoeff")},
                DragCoefficient);

            if (UPrimitiveComponent *PrimitiveRoot =
                    Cast<UPrimitiveComponent>(TargetActor->GetRootComponent())) {
              PrimitiveRoot->SetLinearDamping(static_cast<float>(DragCoefficient));
            }
          }

          VehicleMC->RecreatePhysicsState();

          bSuccess = true;
          Message = FString::Printf(
              TEXT("Vehicle physics configured for actor '%s'"), *ActorName);
          Resp->SetStringField(TEXT("actorName"), ActorName);
          Resp->SetStringField(TEXT("vehicleType"), VehicleType);
          Resp->SetBoolField(TEXT("createdMovementComponent"), bCreatedComponent);
          Resp->SetNumberField(TEXT("configuredWheelCount"), ConfiguredWheels);

#if MCP_HAS_CHAOS_WHEELED_VEHICLE
          Resp->SetBoolField(TEXT("chaosVehicleHeadersAvailable"), true);
#else
          Resp->SetBoolField(TEXT("chaosVehicleHeadersAvailable"), false);
#endif
        }
      }
#else
      Message = TEXT("Wheeled vehicle component headers unavailable in this build");
      ErrorCode = TEXT("NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
#endif
    }
    return false;
}
#endif
} // namespace McpAnimationHandlers
