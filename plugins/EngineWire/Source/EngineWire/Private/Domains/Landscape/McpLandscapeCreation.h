#pragma once

#include "CoreMinimal.h"

class FMcpBridgeWebSocket;
class UMaterialInterface;
class UEngineWireSubsystem;
class UWorld;

namespace McpLandscapeCreation {
struct FLandscapeCreationRequest {
  int32 ComponentsX = 8;
  int32 ComponentsY = 8;
  int32 QuadsPerComponent = 63;
  int32 SectionsPerComponent = 1;
  FVector Location = FVector::ZeroVector;
  FString MaterialPath;
  // MaterialPath loaded once, by the handler, before anything is built.
  UMaterialInterface *Material = nullptr;
  FString Name;
  UWorld *World = nullptr;
};

// Spawns, names and imports the landscape Request describes inside one undo step, and replies.
void BuildLandscape(
    UEngineWireSubsystem &Subsystem, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FLandscapeCreationRequest &Request);
}
