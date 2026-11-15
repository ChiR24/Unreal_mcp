#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
class FMcpBridgeWebSocket;
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/NiagaraAuthoring/McpNiagaraContext.h"

#include "Dom/JsonObject.h"

#include "NiagaraGraph.h"
#include "NiagaraSystem.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"

namespace McpNiagaraGraphHandlers
{
// The node NodeRef names in TargetGraph: its GUID, object name or list title; null when none does.
UEdGraphNode* FindNodeByIdNameOrTitle(UNiagaraGraph* TargetGraph, const FString& NodeRef);
using McpNiagaraAuthoringHandlers::SaveNiagaraGraphEdit;

bool HandleConnectPins(
    UEngineWireSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UNiagaraSystem* System,
    UNiagaraGraph* TargetGraph);

// Removes TargetNode, reconnecting the parameter map around it; a module takes its override node and the nodes only they
// read with it. OutRemovedCount is how many nodes went.
bool RemoveNiagaraGraphNodeSafely(
    UNiagaraGraph* TargetGraph,
    UEdGraphNode* TargetNode,
    FString& OutError,
    int32& OutRemovedCount);
}
