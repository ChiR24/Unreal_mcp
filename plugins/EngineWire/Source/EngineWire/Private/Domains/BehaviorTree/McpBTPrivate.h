#pragma once
#include "Core/Compatibility/McpVersionCompatibility.h"

#include "EngineWireSubsystem.h"
#include "Dom/JsonObject.h"
#include "Foundation/McpScopedEditorTransaction.h"

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

bool HandleCreate(UEngineWireSubsystem* Subsystem,
                  const FRequestContext& Context);
bool HandleGetTree(UEngineWireSubsystem* Subsystem,
                   const FRequestContext& Context);
bool LoadBehaviorTreeForGraph(UEngineWireSubsystem* Subsystem,
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
// Rebuilds the tree from its graph and marks it dirty, inside the edit's undo step; the caller saves after the step
// closes (a save inside it is a write the undo cannot take back). Undo rebuilds the tree again (the graph's PostEditUndo).
void UpdateBehaviorTreeAsset(const FGraphContext& Context);
// The tree, its graph and the graph nodes a graph-route edit changes, for one FMcpAssetEdit: the graph and nodes are made
// transactional first (a graph EngineWire made before was not, so its changes reached no undo step).
inline TArray<UObject*> BehaviorTreeEditObjects(const FGraphContext& Context, std::initializer_list<UObject*> Nodes)
{
  TArray<UObject*> Objects = {Context.BehaviorTree, Context.Graph};
  Context.Graph->SetFlags(RF_Transactional);
  for (UObject* Node : Nodes) {
    if (Node) {
      Node->SetFlags(RF_Transactional);
      Objects.Add(Node);
    }
  }
  return Objects;
}
UEdGraphNode* FindGraphNodeByIdOrName(UEdGraph* Graph,
                                      const FString& IdOrName);
bool HandleAddNode(UEngineWireSubsystem* Subsystem,
                   const FRequestContext& Context,
                   const FGraphContext& GraphContext);
bool HandleConnectNodes(UEngineWireSubsystem* Subsystem,
                        const FRequestContext& Context,
                        const FGraphContext& GraphContext);
bool HandleUnlinkNode(UEngineWireSubsystem* Subsystem,
                      const FRequestContext& Context,
                      const FGraphContext& GraphContext, bool bRemove);
bool HandleSetNodeProperties(UEngineWireSubsystem* Subsystem,
                             const FRequestContext& Context,
                             const FGraphContext& GraphContext);
bool HandleAddSubnode(UEngineWireSubsystem* Subsystem,
                      const FRequestContext& Context,
                      const FGraphContext& GraphContext);

}
