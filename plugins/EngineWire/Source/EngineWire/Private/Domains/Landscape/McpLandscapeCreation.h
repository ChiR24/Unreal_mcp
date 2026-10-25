#pragma once

#include "CoreMinimal.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Materials/MaterialInterface.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

namespace McpLandscapeCreation {
// The landscape material at a package or object path; LoadObject alone missed a package path, and the landscape
// was then built with no material while the call said nothing.
inline UMaterialInterface *LoadLandscapeMaterial(const FString &Path) {
  UObject *Asset = McpLoadAsset(Path);
  return Cast<UMaterialInterface>(Asset ? Asset : LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
}

struct FLandscapeCreationRequest {
  int32 ComponentsX = 8;
  int32 ComponentsY = 8;
  int32 QuadsPerComponent = 63;
  int32 SectionsPerComponent = 1;
  FVector Location = FVector::ZeroVector;
  FString MaterialPath;
  FString Name;
};

void CreateLandscapeOnGameThread(
    UEngineWireSubsystem &Subsystem, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FLandscapeCreationRequest &Request);
}
