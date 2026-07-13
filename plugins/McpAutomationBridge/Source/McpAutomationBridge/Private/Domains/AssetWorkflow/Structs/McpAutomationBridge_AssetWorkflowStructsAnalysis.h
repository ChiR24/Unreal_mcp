#pragma once

#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

// Per-action handlers split out of McpAutomationBridge_AssetWorkflowStructsAnalysis.cpp
// so each implementation file stays within the 250 pure-line ceiling. The dispatcher in
// that file routes the action string to the matching handler below; behavior is identical
// to the original single-file implementation.
bool HandleStructAnalysisCompare(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructAnalysisSearchUsage(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleStructAnalysisRecompile(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
