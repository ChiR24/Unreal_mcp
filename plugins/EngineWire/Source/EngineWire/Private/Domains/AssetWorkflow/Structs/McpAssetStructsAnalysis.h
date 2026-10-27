#pragma once

#include "Domains/AssetWorkflow/Structs/McpAssetStructsShared.h"

// Per-action handlers split out of McpAssetStructsAnalysis.cpp
// so each implementation file stays within the 250 pure-line ceiling. The dispatcher in
// that file routes the action string to the matching handler below; behavior is identical
// to the original single-file implementation.
bool HandleStructAnalysisCompare(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructAnalysisSearchUsage(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructAnalysisRecompile(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
