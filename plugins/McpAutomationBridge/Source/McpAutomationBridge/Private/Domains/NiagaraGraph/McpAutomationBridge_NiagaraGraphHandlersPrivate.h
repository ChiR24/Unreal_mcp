#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"

#include "Dom/JsonObject.h"

#include "NiagaraGraph.h"
#include "NiagaraSystem.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"

namespace McpNiagaraGraphHandlers
{
// A graph edit persists: dirty the system, then save it unless the caller sent save:false.
// Dirtying alone let every module add, removal and wire vanish at the next editor start.
inline void SaveNiagaraGraphEdit(UNiagaraSystem* System, const TSharedPtr<FJsonObject>& Payload, const TSharedPtr<FJsonObject>& Result)
{
    // Compiled too: until then the system ran its old scripts and a module just added had no inputs to list or set.
    System->RequestCompile(false);
    System->MarkPackageDirty();
    if (GetJsonBoolField(Payload, TEXT("save"), true))
    {
        Result->SetBoolField(TEXT("saved"), McpSafeOperations::McpSafeAssetSave(System));
    }
}

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
