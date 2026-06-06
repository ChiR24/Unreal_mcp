#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

namespace McpProcessRequestDispatch
{
bool DispatchFallbackAutomationRequest(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const FString& Action,
    const FString& LowerAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    FString& OutConsumedHandlerLabel);
}
