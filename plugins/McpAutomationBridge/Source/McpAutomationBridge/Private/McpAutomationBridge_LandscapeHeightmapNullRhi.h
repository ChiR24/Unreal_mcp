#pragma once

#include "CoreMinimal.h"

class ALandscape;
class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

namespace McpLandscapeHeightmap {
struct FHeightmapRegion {
  int32 MinX = -1;
  int32 MinY = -1;
  int32 MaxX = -1;
  int32 MaxY = -1;
};

void SendNullRhiValidatedResponse(
    UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, ALandscape* Landscape,
    const FString& Operation, const FHeightmapRegion& Region);
}
