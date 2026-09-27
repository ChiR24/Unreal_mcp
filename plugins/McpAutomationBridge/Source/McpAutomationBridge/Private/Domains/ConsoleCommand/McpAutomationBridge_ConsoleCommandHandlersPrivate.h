#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMcpConsoleHandlers, Log, All);

namespace ConsoleCommandSecurity
{
bool IsBlockedCommand(const FString& Command);
}

namespace McpConsoleCommandHandlers
{
}
