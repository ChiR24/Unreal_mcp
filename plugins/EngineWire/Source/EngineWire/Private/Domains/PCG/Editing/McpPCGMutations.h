#pragma once

#include "Domains/PCG/Editing/McpPCGEditState.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
struct FPCGMutationContext
{
    TMap<FString, FString> TemporaryNodes;
    bool bDestructive = false;
};
bool ApplyPCGMutation(UPCGGraph* Graph, const TSharedPtr<FJsonObject>& Operation,
    FPCGMutationContext& Context, TSharedPtr<FJsonObject>& OutResult, FString& OutError, FString& OutCode);
bool HandlePCGEditOperations(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath);
}
#endif
