#pragma once
#include "Core/Compatibility/McpVersionCompatibility.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"

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
bool EnsureBehaviorTreeGraph(UBehaviorTree*& BehaviorTree, UEdGraph*& OutGraph);
#if MCP_HAS_BEHAVIOR_TREE_GRAPH
// A new BTGraph on BehaviorTree with the schema's default (root) node.
UEdGraph* CreateBehaviorTreeGraph(UBehaviorTree* BehaviorTree);
#endif
// The tree a request names: assetPath, else behaviorTreePath.
inline FString ReadBehaviorTreePath(const TSharedPtr<FJsonObject>& Payload)
{
  FString Path;
  if (!Payload->TryGetStringField(TEXT("assetPath"), Path) || Path.IsEmpty()) {
    Payload->TryGetStringField(TEXT("behaviorTreePath"), Path);
  }
  return Path;
}
// Spawns graph nodes for asset-route composites/tasks that have none; returns the count spawned.
int32 SyncBehaviorTreeGraphFromAsset(UBehaviorTree* BehaviorTree, UEdGraph* Graph);
void UpdateBehaviorTreeAsset(const FGraphContext& Context);
UEdGraphNode* FindGraphNodeByIdOrName(UEdGraph* Graph,
                                      const FString& IdOrName);
bool HandleAddNode(UMcpAutomationBridgeSubsystem* Subsystem,
                   const FRequestContext& Context,
                   const FGraphContext& GraphContext);
bool HandleConnectNodes(UMcpAutomationBridgeSubsystem* Subsystem,
                        const FRequestContext& Context,
                        const FGraphContext& GraphContext);
bool HandleUnlinkNode(UMcpAutomationBridgeSubsystem* Subsystem,
                      const FRequestContext& Context,
                      const FGraphContext& GraphContext, bool bRemove);
bool HandleSetNodeProperties(UMcpAutomationBridgeSubsystem* Subsystem,
                             const FRequestContext& Context,
                             const FGraphContext& GraphContext);
bool HandleAddSubnode(UMcpAutomationBridgeSubsystem* Subsystem,
                      const FRequestContext& Context,
                      const FGraphContext& GraphContext);

}
