#pragma once

#include "Domains/PCG/McpPCGPrivate.h"

#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#define MCP_HAS_PCG_PARAMETERS 1
#include "StructUtils/PropertyBag.h"
#else
#define MCP_HAS_PCG_PARAMETERS 0
#endif

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool HandlePCGParameters(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave);
#if MCP_HAS_PCG_PARAMETERS
struct FPCGParameterTarget
{
    UPCGGraphInterface* Interface = nullptr;
    UPCGGraph* Graph = nullptr;
    UPCGComponent* Component = nullptr;
    FString Scope;
};
bool ResolvePCGParameterTarget(const TSharedPtr<FJsonObject>& Payload, FPCGParameterTarget& Target, FString& OutError);
bool BuildPCGParameterDesc(const TSharedPtr<FJsonObject>& Payload, FPropertyBagPropertyDesc& Desc, FString& OutError);
TArray<TSharedPtr<FJsonValue>> ReadPCGParameters(const FPCGParameterTarget& Target, const FString& Name = FString(),
    int32 Offset = 0, int32 Limit = MAX_int32, int32* OutTotal = nullptr);
bool StagePCGParameterValue(const FInstancedPropertyBag& Source, FName Name, const TSharedPtr<FJsonValue>& Value,
    FInstancedPropertyBag& OutBag, FString& OutError);
bool CommitPCGParameterValue(UPCGGraphInterface* Target, FName Name, const FInstancedPropertyBag& Source, FString& OutError);
TArray<TSharedPtr<FJsonValue>> FindPCGParameterReferences(UPCGGraph* Graph, FName Name, const FGuid& Id);
#endif
}
#endif
