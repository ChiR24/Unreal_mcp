#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EngineWireSubsystem.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMcpConsoleHandlers, Log, All);

namespace ConsoleCommandSecurity
{
bool IsBlockedCommand(const FString& Command);
}

namespace McpConsoleCommandHandlers
{
}

// McpConsoleVariables.cpp: inspect get_console_variables, each named variable's value, type, last setter and help.
namespace McpConsoleVariables
{
bool HandleGet(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
               TSharedPtr<FMcpBridgeWebSocket> Socket);
// True when a whole word of the name is key, token, secret, password, auth or credential (or their plurals).
bool NamesACredential(const FString& Name);
}
