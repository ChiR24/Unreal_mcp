#pragma once

#include "CoreMinimal.h"

class ALandscape;
class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

namespace McpLandscapeHeightmap {
// Under NullRHI the extent and the edit are unsafe: the landscape is validated and the size Payload's region asks
// for reported (1 for an unset side).
void SendNullRhiValidatedResponse(
    UEngineWireSubsystem& Subsystem, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, ALandscape* Landscape,
    const FString& Operation, const TSharedPtr<FJsonObject>& Payload);
}
