#pragma once

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpVersionCompatibility.h"

#include "Dom/JsonObject.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMcpAIHandlers, Log, All);

#define GetStringFieldAI GetJsonStringField
#define GetNumberFieldAI GetJsonNumberField
#define GetBoolFieldAI GetJsonBoolField

namespace McpAIHandlers
{
inline bool SanitizeAIAssetPath(const FString& InputPath, FString& OutSanitizedPath, FString& OutError)
{
    OutSanitizedPath = McpHandlerUtils::ValidateAssetPath(InputPath.TrimStartAndEnd());
    if (OutSanitizedPath.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Invalid asset path: %s"), *InputPath);
        return false;
    }
    return true;
}

#define MCP_AI_HANDLER_DECL(Name) bool Name(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
MCP_AI_HANDLER_DECL(HandleCreateAIController);
MCP_AI_HANDLER_DECL(HandleAssignBehaviorTree);
MCP_AI_HANDLER_DECL(HandleAssignBlackboard);
MCP_AI_HANDLER_DECL(HandleCreateBlackboardAsset);
MCP_AI_HANDLER_DECL(HandleAddBlackboardKey);
MCP_AI_HANDLER_DECL(HandleSetKeyInstanceSynced);
MCP_AI_HANDLER_DECL(HandleCreateBehaviorTree);
MCP_AI_HANDLER_DECL(HandleAddCompositeNode);
MCP_AI_HANDLER_DECL(HandleAddTaskNode);
MCP_AI_HANDLER_DECL(HandleAddDecorator);
MCP_AI_HANDLER_DECL(HandleAddService);
MCP_AI_HANDLER_DECL(HandleConfigureBehaviorTreeNode);
MCP_AI_HANDLER_DECL(HandleCreateEQSQuery);
MCP_AI_HANDLER_DECL(HandleAddEQSGenerator);
MCP_AI_HANDLER_DECL(HandleAddEQSContext);
MCP_AI_HANDLER_DECL(HandleAddEQSTest);
MCP_AI_HANDLER_DECL(HandleConfigureEQSTestScoring);
MCP_AI_HANDLER_DECL(HandleAddAIPerceptionComponent);
MCP_AI_HANDLER_DECL(HandleConfigureSightConfig);
MCP_AI_HANDLER_DECL(HandleConfigureHearingConfig);
MCP_AI_HANDLER_DECL(HandleConfigureDamageSenseConfig);
MCP_AI_HANDLER_DECL(HandleSetPerceptionTeam);
MCP_AI_HANDLER_DECL(HandleCreateStateTree);
MCP_AI_HANDLER_DECL(HandleAddStateTreeState);
MCP_AI_HANDLER_DECL(HandleAddStateTreeTransition);
MCP_AI_HANDLER_DECL(HandleConfigureStateTreeTask);
MCP_AI_HANDLER_DECL(HandleCreateSmartObjectDefinition);
MCP_AI_HANDLER_DECL(HandleAddSmartObjectSlot);
MCP_AI_HANDLER_DECL(HandleConfigureSmartObjectSlotBehavior);
MCP_AI_HANDLER_DECL(HandleAddSmartObjectComponent);
MCP_AI_HANDLER_DECL(HandleCreateMassEntityConfig);
MCP_AI_HANDLER_DECL(HandleConfigureMassEntity);
MCP_AI_HANDLER_DECL(HandleAddMassSpawner);
MCP_AI_HANDLER_DECL(HandleGetAIInfo);
MCP_AI_HANDLER_DECL(HandleSetAIPerception);
MCP_AI_HANDLER_DECL(HandleCreateNavModifier);
MCP_AI_HANDLER_DECL(HandleSetAIMovement);
MCP_AI_HANDLER_DECL(HandleCreateBlackboard);
MCP_AI_HANDLER_DECL(HandleSetupPerception);
MCP_AI_HANDLER_DECL(HandleCreateNavLinkProxy);
MCP_AI_HANDLER_DECL(HandleSetFocus);
MCP_AI_HANDLER_DECL(HandleClearFocus);
MCP_AI_HANDLER_DECL(HandleSetBlackboardValue);
MCP_AI_HANDLER_DECL(HandleGetBlackboardValue);
MCP_AI_HANDLER_DECL(HandleRunBehaviorTree);
MCP_AI_HANDLER_DECL(HandleStopBehaviorTree);
#undef MCP_AI_HANDLER_DECL
}
