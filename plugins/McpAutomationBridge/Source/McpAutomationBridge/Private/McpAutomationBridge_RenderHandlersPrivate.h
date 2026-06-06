#pragma once

#include "CoreMinimal.h"
#include "McpAutomationBridgeSubsystem.h"

namespace McpRenderHandlers
{
bool HandleLumenUpdateScene(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
}
