#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#include "NiagaraGraph.h"
#include "NiagaraSystem.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"

namespace McpNiagaraGraphHandlers
{
bool HandleConnectPins(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UNiagaraSystem* System,
    UNiagaraGraph* TargetGraph);

bool RemoveNiagaraGraphNodeSafely(
    UNiagaraGraph* TargetGraph,
    UEdGraphNode* TargetNode,
    FString& OutError);
}
