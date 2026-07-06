#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

namespace McpSequenceMovieRender {
bool TryHandle(UMcpAutomationBridgeSubsystem *Subsystem, const FString &RequestId,
               const FString &Action, const TSharedPtr<FJsonObject> &Payload,
               TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}
