#pragma once

#include "MCP/Transport/McpNativeTransport.h"
#include "MCP/Protocol/McpJsonRpc.h"
#include "MCP/Registry/McpToolRegistry.h"
#include "MCP/Registry/McpToolDefinition.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeSettings.h"
#include "Misc/Guid.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMcpNativeTransport, Log, All);

// Defense-in-depth validation that an action string is a snake_case
// identifier. Used by HandleToolsCall to reject unexpected action values
// (paths, URLs, arbitrary strings) before they reach the dispatch queue.
// Mirrors the TS-side regex in message-handler.ts::enforceActionMatch.
inline bool IsValidSnakeCaseAction(const FString& S)
{
    if (S.IsEmpty() || S.Len() > 128) return false;
    if (!FChar::IsAlpha(S[0]) || FChar::IsUpper(S[0])) return false;
    for (int32 i = 1; i < S.Len(); ++i)
        if (!FChar::IsAlnum(S[i]) && S[i] != TEXT('_')) return false;
    return true;
}
