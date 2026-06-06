#pragma once

#include "CoreMinimal.h"

class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

namespace McpLandscapeCreation {
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
    UMcpAutomationBridgeSubsystem &Subsystem, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FLandscapeCreationRequest &Request);
}
