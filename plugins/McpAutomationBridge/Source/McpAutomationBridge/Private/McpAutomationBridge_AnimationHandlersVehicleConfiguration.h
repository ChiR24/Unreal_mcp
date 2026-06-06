#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#if __has_include("PhysicsEngine/WheeledVehicleMovementComponent4W.h")
#include "PhysicsEngine/WheeledVehicleMovementComponent4W.h"
#define MCP_HAS_WHEELED_VEHICLE_4W 1
#else
#define MCP_HAS_WHEELED_VEHICLE_4W 0
#endif

#if __has_include("PhysicsEngine/VehicleWheel.h")
#include "PhysicsEngine/VehicleWheel.h"
#define MCP_HAS_VEHICLE_WHEEL 1
#else
#define MCP_HAS_VEHICLE_WHEEL 0
#endif

#if __has_include("ChaosWheeledVehicleMovementComponent.h")
#include "ChaosWheeledVehicleMovementComponent.h"
#define MCP_HAS_CHAOS_WHEELED_VEHICLE 1
#elif __has_include("Chaos/ChaosWheeledVehicleMovementComponent.h")
#include "Chaos/ChaosWheeledVehicleMovementComponent.h"
#define MCP_HAS_CHAOS_WHEELED_VEHICLE 1
#else
#define MCP_HAS_CHAOS_WHEELED_VEHICLE 0
#endif

#if MCP_HAS_CHAOS_WHEELED_VEHICLE && !MCP_HAS_WHEELED_VEHICLE_4W
#undef MCP_HAS_WHEELED_VEHICLE_4W
#define MCP_HAS_WHEELED_VEHICLE_4W 1
#define UWheeledVehicleMovementComponent4W UChaosWheeledVehicleMovementComponent
#endif
#endif

namespace McpAnimationHandlers {
#if WITH_EDITOR
bool SetVehicleNumericOnStruct(UStruct *StructType, void *Container,
                               const TArray<FString> &PropertyNames,
                               double Value);
bool SetVehicleNumericOnObject(UObject *Obj,
                               const TArray<FString> &PropertyNames,
                               double Value);
bool SetVehicleNestedNumericOnObject(UObject *Obj,
                                     const TArray<FString> &OuterNames,
                                     const TArray<FString> &InnerNames,
                                     double Value);
bool ConfigureVehicleForwardGearRatios(UObject *Obj,
                                       const TArray<double> &Ratios);
int32 ConfigureVehicleWheels(UObject *VehicleMC,
                             const TSharedPtr<FJsonObject> &Payload);
void ConfigureVehicleEngine(UObject *VehicleMC,
                            const TSharedPtr<FJsonObject> &Payload);
void ConfigureVehicleTransmission(UObject *VehicleMC,
                                  const TSharedPtr<FJsonObject> &Payload);
#endif
} // namespace McpAnimationHandlers
