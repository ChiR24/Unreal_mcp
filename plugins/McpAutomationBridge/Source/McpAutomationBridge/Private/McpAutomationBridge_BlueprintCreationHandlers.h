#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UMcpAutomationBridgeSubsystem;
class FMcpBridgeWebSocket;

class FBlueprintCreationHandlers
{
public:
    static bool HandleBlueprintCreate(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& LocalPayload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
    static bool HandleBlueprintProbeSubobjectHandle(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& LocalPayload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
};
