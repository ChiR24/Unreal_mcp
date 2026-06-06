#pragma once

#include "McpAutomationBridge_GASAvailability.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
struct FGASRequestContext
{
    UMcpAutomationBridgeSubsystem* Subsystem;
    FString RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
    TSharedPtr<FJsonObject> Payload;
    FString Name;
    FString Path;
    FString BlueprintPath;
    FString AssetPath;
};
}
#endif
