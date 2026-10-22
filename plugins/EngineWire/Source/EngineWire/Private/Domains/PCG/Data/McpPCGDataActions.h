#pragma once

#include "Domains/PCG/Data/McpPCGDataViews.h"

namespace McpPCGHandlers
{
bool IsPCGDataAction(const FString& Action);
#if MCP_HAS_PCG_EXECUTION
bool HandlePCGData(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, FPCGDataViewStore& Store);
bool ReadPCGDataInt(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Key, int32 Default, int32 Minimum,
    int32 Maximum, int32& OutValue, FString& OutError);
bool ReadPCGDataDomain(const TSharedPtr<FJsonObject>& Payload, FString& OutDomain, FString& OutError);
bool ReadPCGDataStrings(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Key, int32 MaxLength,
    TArray<FString>& OutValues, FString& OutError);
TSharedPtr<FJsonObject> QueryPCGRootData(UPCGComponent* Component, const FString& Principal, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, FPCGDataViewStore& Store, TArray<FString>& NewHandles, FString& OutError);
TSharedPtr<FJsonObject> CapturePCGDataView(UPCGComponent* Component, const FString& Principal,
    const TSharedPtr<FJsonObject>& Payload, FPCGDataViewStore& Store, TArray<FString>& NewHandles, FString& OutError);
#endif
}
