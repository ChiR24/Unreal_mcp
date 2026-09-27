#pragma once

#include "MCP/Transport/McpNativeTransport.h"
#include "MCP/Protocol/McpJsonRpc.h"
// McpCanonicalizeRequestId keys an in-flight call by its JSON-RPC id; "str:"/"num:"
// namespacing keeps a string "1" and a numeric 1 from cross-cancelling.
#include "MCP/Execute/McpNativeReceiptEnrichment.h"
#include "Foundation/McpSecureTokenCompare.h"
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


// Native MCP protocol-version set.
//
// Intentional legacy asymmetry: the native C++ transport deliberately supports
// ONLY the three modern protocol versions (2025-11-25, 2025-06-18, 2025-03-26).
// The TypeScript SDK's negotiation table additionally carries OLDER legacy
// versions for backward compatibility with long-lived client installs; the
// native surface does NOT, because every native client negotiates fresh at
// initialize and we refuse to maintain compatibility shims for protocol
// versions the supported engine targets never shipped against. Do NOT add
// unsupported (future or legacy) versions here — the latest three are the
// supported set.
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

inline bool McpIsSupportedProtocolVersion(const FString& Version)
{
    return McpSupportedProtocolVersions().Contains(Version);
}
