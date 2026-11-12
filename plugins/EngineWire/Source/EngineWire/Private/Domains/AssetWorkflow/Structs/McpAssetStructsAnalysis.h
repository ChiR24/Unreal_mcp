#pragma once

#include "Domains/AssetWorkflow/Structs/McpAssetStructsShared.h"

// The struct analysis actions McpAssetStructsAnalysis.cpp routes to: compare_structs (Compare),
// search_struct_usage (SearchUsage) and recompile_struct (Recompile).
bool HandleStructAnalysisCompare(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructAnalysisSearchUsage(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructAnalysisRecompile(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
