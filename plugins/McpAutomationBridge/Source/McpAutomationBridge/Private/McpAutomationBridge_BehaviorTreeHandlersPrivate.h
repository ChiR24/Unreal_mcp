#include "McpVersionCompatibility.h"
#pragma once

#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "BehaviorTree/BehaviorTree.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "AIGraphNode.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#define MCP_HAS_BEHAVIOR_TREE_GRAPH 1
#else
#define MCP_HAS_BEHAVIOR_TREE_GRAPH 0
#endif

namespace McpBehaviorTreeHandlers {

struct FRequestContext
{
  const FString& RequestId;
  const TSharedPtr<FJsonObject>& Payload;
  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
};

struct FGraphContext
{
  UBehaviorTree* BehaviorTree;
  UEdGraph* Graph;
};

bool HandleCreate(UMcpAutomationBridgeSubsystem* Subsystem,
                  const FRequestContext& Context);
bool HandleGetTree(UMcpAutomationBridgeSubsystem* Subsystem,
                   const FRequestContext& Context);
bool LoadBehaviorTreeForGraph(UMcpAutomationBridgeSubsystem* Subsystem,
                              const FRequestContext& Context,
                              FGraphContext& OutContext);
void UpdateBehaviorTreeAsset(const FGraphContext& Context);
UEdGraphNode* FindGraphNodeByIdOrName(UEdGraph* Graph,
                                      const FString& IdOrName);
bool HandleAddNode(UMcpAutomationBridgeSubsystem* Subsystem,
                   const FRequestContext& Context,
                   const FGraphContext& GraphContext);
bool HandleConnectNodes(UMcpAutomationBridgeSubsystem* Subsystem,
                        const FRequestContext& Context,
                        const FGraphContext& GraphContext);
bool HandleRemoveNode(UMcpAutomationBridgeSubsystem* Subsystem,
                      const FRequestContext& Context,
                      const FGraphContext& GraphContext);
bool HandleBreakConnections(UMcpAutomationBridgeSubsystem* Subsystem,
                            const FRequestContext& Context,
                            const FGraphContext& GraphContext);
bool HandleSetNodeProperties(UMcpAutomationBridgeSubsystem* Subsystem,
                             const FRequestContext& Context,
                             const FGraphContext& GraphContext);
bool HandleAddSubnode(UMcpAutomationBridgeSubsystem* Subsystem,
                      const FRequestContext& Context,
                      const FGraphContext& GraphContext);

}
#endif
