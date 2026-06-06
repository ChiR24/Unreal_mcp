#pragma once

#include "McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
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
#endif
