#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionContext.h"
#include "Domains/Animation/Physics/McpAutomationBridge_AnimationHandlersVehicleConfiguration.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace McpAnimationHandlers {
bool HandleAnimationConfigureVehicleAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    Context.Fail(TEXT("INVALID_ARGUMENT"), TEXT("actorName is required for configure_vehicle"));
    return false;
  }
#if MCP_HAS_WHEELED_VEHICLE_4W
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  AActor *TargetActor = Context.FindActorByName(ActorName);
  if (!TargetActor) {
    Context.Fail(TEXT("ACTOR_NOT_FOUND"), FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    return false;
  }
  // A wheeled vehicle needs a skeletal mesh (wheel bones) to drive; a static-mesh actor used to be
  // "configured" and left with a dead movement component.
  if (!TargetActor->FindComponentByClass<USkeletalMeshComponent>()) {
    Context.Fail(TEXT("INVALID_TARGET"), FString::Printf(TEXT("Actor '%s' has no SkeletalMeshComponent; configure_vehicle needs a vehicle pawn or skeletal-mesh actor"), *ActorName));
    return false;
  }
  // Only a wheeled vehicle movement component exists to add; any other type
  // used to be echoed back while the 4W component was added regardless.
  FString VehicleType = TEXT("WheeledVehicle4W");
  Payload->TryGetStringField(TEXT("vehicleType"), VehicleType);
  if (!VehicleType.Equals(TEXT("WheeledVehicle4W"), ESearchCase::IgnoreCase) &&
      !VehicleType.Equals(TEXT("WheeledVehicle"), ESearchCase::IgnoreCase)) {
    Context.Fail(TEXT("UNSUPPORTED_VEHICLE_TYPE"),
                 FString::Printf(TEXT("vehicleType '%s' is not supported; configure_vehicle sets up a wheeled vehicle (WheeledVehicle4W)"), *VehicleType));
    return false;
  }

  UWheeledVehicleMovementComponent4W *VehicleMC =
      TargetActor->FindComponentByClass<UWheeledVehicleMovementComponent4W>();
  bool bCreatedComponent = false;
  if (!VehicleMC) {
    VehicleMC = NewObject<UWheeledVehicleMovementComponent4W>(TargetActor, TEXT("MCP_VehicleMovement4W"));
    if (VehicleMC) {
      TargetActor->AddInstanceComponent(VehicleMC);
      VehicleMC->RegisterComponent();
      bCreatedComponent = true;
    }
  }
  if (!VehicleMC) {
    Context.Fail(TEXT("COMPONENT_CREATION_FAILED"), TEXT("Failed to create/get UWheeledVehicleMovementComponent4W"));
    return false;
  }
  const int32 ConfiguredWheels = ConfigureVehicleWheels(VehicleMC, Payload);
  ConfigureVehicleEngine(VehicleMC, Payload);
  ConfigureVehicleTransmission(VehicleMC, Payload);

  UPrimitiveComponent *PrimitiveRoot = Cast<UPrimitiveComponent>(TargetActor->GetRootComponent());
  double Mass = 0.0;
  if (Payload->TryGetNumberField(TEXT("mass"), Mass) && Mass > 0.0) {
    SetVehicleNumericOnObject(VehicleMC, {TEXT("Mass"), TEXT("VehicleMass")}, Mass);
    if (PrimitiveRoot) {
      PrimitiveRoot->SetMassOverrideInKg(NAME_None, static_cast<float>(Mass), true);
    }
  }
  double DragCoefficient = 0.0;
  if (Payload->TryGetNumberField(TEXT("dragCoefficient"), DragCoefficient)) {
    SetVehicleNumericOnObject(VehicleMC,
                              {TEXT("DragCoefficient"), TEXT("DragCoeff"), TEXT("AerodynamicDragCoefficient")},
                              DragCoefficient);
    SetVehicleNestedNumericOnObject(VehicleMC, {TEXT("AerofoilSetup"), TEXT("AerodynamicsSetup")},
                                    {TEXT("DragCoefficient"), TEXT("DragCoeff")}, DragCoefficient);
    if (PrimitiveRoot) {
      PrimitiveRoot->SetLinearDamping(static_cast<float>(DragCoefficient));
    }
  }
  VehicleMC->RecreatePhysicsState();

  Context.bSuccess = true;
  Context.Message = FString::Printf(TEXT("Vehicle physics configured for actor '%s'"), *ActorName);
  Resp->SetStringField(TEXT("actorName"), ActorName);
  Resp->SetStringField(TEXT("vehicleType"), VehicleType);
  Resp->SetStringField(TEXT("movementComponentClass"), VehicleMC->GetClass()->GetName());
  Resp->SetBoolField(TEXT("createdMovementComponent"), bCreatedComponent);
  Resp->SetNumberField(TEXT("configuredWheelCount"), ConfiguredWheels);
  Resp->SetBoolField(TEXT("chaosVehicleHeadersAvailable"), MCP_HAS_CHAOS_WHEELED_VEHICLE != 0);
#else
  Context.Fail(TEXT("NOT_AVAILABLE"), TEXT("Wheeled vehicle component headers unavailable in this build"));
#endif
  return false;
}
} // namespace McpAnimationHandlers
