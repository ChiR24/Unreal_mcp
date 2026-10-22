#pragma once

#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"

#ifndef MCP_HAS_PCG_EDITOR
#define MCP_HAS_PCG_EDITOR 0
#endif

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool IsPCGNodeHidden(UPCGNode* Node);
UPCGNode* GetPCGEditorModel(UEdGraphNode* Node);
UEdGraphNode* CreatePCGEditorCounterpart(UPCGGraph* Graph, UEdGraph* EditorGraph, UPCGNode* Model, FString& OutError);
TSharedPtr<FJsonObject> CheckPCGEditorGraph(UPCGGraph* Graph);
bool RestorePCGEditorGraphState(UPCGGraph* Graph, UEdGraph* Editor, const TArray<UEdGraphNode*>& OriginalNodes);
bool SyncPCGEditorGraph(UPCGGraph* Graph, TSharedPtr<FJsonObject>& OutResult, FString& OutError);
bool HandleSyncPCGEditor(UEngineWireSubsystem* Bridge, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath);
}
#endif
