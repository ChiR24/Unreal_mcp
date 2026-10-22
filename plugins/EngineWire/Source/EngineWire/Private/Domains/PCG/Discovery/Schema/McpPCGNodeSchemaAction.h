#pragma once

#include "Domains/PCG/McpPCGPrivate.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool HandlePCGNodeSchema(UEngineWireSubsystem* Bridge, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
TSharedPtr<FJsonObject> ReadPCGNodeSchema(UPCGGraph* Graph, UPCGNode* Node, const FString& ScopePath,
    int32 Offset, int32 Limit, FString& OutError);
#endif
}
#endif
