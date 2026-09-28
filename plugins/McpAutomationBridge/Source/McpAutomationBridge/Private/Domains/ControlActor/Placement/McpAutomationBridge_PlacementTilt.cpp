// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/ControlActor/Placement/McpAutomationBridge_PlacementTilt.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"

namespace McpPlacementTilt {
bool OffVertical(AActor *Actor, double &OutDegrees, double &OutUnits) {
  if (!Actor) {
    return false;
  }
  // Rotation carries meaning for anything that AIMS -- lights, cameras, decals,
  // audio cones. Only solid geometry can be "tipped over", so judge just the
  // actors that actually render a mesh.
  TArray<UStaticMeshComponent *> Meshes;
  Actor->GetComponents<UStaticMeshComponent>(Meshes);
  bool bHasMesh = false;
  for (const UStaticMeshComponent *Mesh : Meshes) {
    if (Mesh != nullptr && Mesh->GetStaticMesh() != nullptr) {
      bHasMesh = true;
      break;
    }
  }
  if (!bHasMesh) {
    return false;
  }
  const double CosLean = FMath::Clamp(
      FVector::DotProduct(Actor->GetActorUpVector(), FVector::UpVector), -1.0, 1.0);
  OutDegrees = FMath::RadiansToDegrees(FMath::Acos(CosLean));
  FVector Origin = FVector::ZeroVector;
  FVector Extent = FVector::ZeroVector;
  Actor->GetActorBounds(true, Origin, Extent);
  OutUnits = 2.0 * Extent.Z *
             FMath::Sin(FMath::DegreesToRadians(OutDegrees) * 0.5);
  return true;
}
} // namespace McpPlacementTilt
