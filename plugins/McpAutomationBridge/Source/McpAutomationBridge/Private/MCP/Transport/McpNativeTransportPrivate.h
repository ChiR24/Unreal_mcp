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

// Canonical key for a JSON-RPC request id (string or number) so an incoming
// notifications/cancelled requestId correlates to the inflight SSE connection
// whose JsonRpcId matches. Number ids are keyed by exact value (LexToString);
// null/object/array/bool ids are unsupported and key empty.
inline FString McpJsonRpcIdKey(const TSharedPtr<FJsonValue>& Id)
{
	if (!Id.IsValid()) return FString();
	if (Id->Type == EJson::String)
	{
		FString S; Id->TryGetString(S); return S;
	}
	if (Id->Type == EJson::Number)
	{
		return LexToString(Id->AsNumber());
	}
	return FString();
}

// Native MCP protocol-version set. 2026-07-28 (a later RC) is intentionally not supported.
inline const TArray<FString>& McpSupportedProtocolVersions()
{
    static const TArray<FString> Versions =
    {
        TEXT("2025-11-25"),
        TEXT("2025-06-18"),
        TEXT("2025-03-26")
    };
    return Versions;
}

// Latest supported version; the server negotiates down to this for unknown request versions.
inline const FString& McpLatestProtocolVersion()
{
    static const FString Version(TEXT("2025-11-25"));
    return Version;
}

// Used when a post-initialize request omits MCP-Protocol-Version and no session version is known.
inline const FString& McpDefaultProtocolVersion()
{
    static const FString Version(TEXT("2025-03-26"));
    return Version;
}

inline bool McpIsSupportedProtocolVersion(const FString& Version)
{
    return McpSupportedProtocolVersions().Contains(Version);
}
