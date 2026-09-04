// McpAutomationBridge_BehaviorTreeHandlersGraphSync.cpp — asset -> graph synchronisation.
//
// Dogfood #60: asset-route actions (add_composite_node / add_task_node) edit UBehaviorTree
// directly, so their nodes had no UBehaviorTreeGraphNode and graph-route ids could not
// resolve them; UBehaviorTreeGraph::SpawnMissingNodes() only covers a graph created from
// scratch. This walks the asset tree and spawns a graph node for every composite/task that
// lacks one, linking it under its parent's graph node (or the Root node for the asset root).
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeHandlersPrivate.h"

#if MCP_HAS_BEHAVIOR_TREE_GRAPH
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "EdGraph/EdGraphPin.h"
#endif

namespace McpBehaviorTreeHandlers
{
#if MCP_HAS_BEHAVIOR_TREE_GRAPH
namespace
{
UEdGraphPin* PinOf(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
  if (!Node) return nullptr;
  for (UEdGraphPin* Pin : Node->Pins) { if (Pin && Pin->Direction == Direction) return Pin; }
  return nullptr;
}

UBehaviorTreeGraphNode* FindGraphNodeForInstance(UEdGraph* Graph, const UBTNode* Instance)
{
  for (UEdGraphNode* Node : Graph->Nodes)
  {
    UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(Node);
    if (BTNode && BTNode->NodeInstance == Instance) return BTNode;
  }
  return nullptr;
}

int32 SyncSubtree(UEdGraph* Graph, UBTNode* AssetNode, UEdGraphNode* ParentGraphNode, int32 ChildIndex)
{
  if (!AssetNode || !ParentGraphNode) return 0;
  int32 Spawned = 0;
  UBehaviorTreeGraphNode* GraphNode = FindGraphNodeForInstance(Graph, AssetNode);
  if (!GraphNode)
  {
    if (Cast<UBTCompositeNode>(AssetNode))
    {
      FGraphNodeCreator<UBehaviorTreeGraphNode_Composite> Creator(*Graph);
      GraphNode = Creator.CreateNode();
      Creator.Finalize();
    }
    else
    {
      FGraphNodeCreator<UBehaviorTreeGraphNode_Task> Creator(*Graph);
      GraphNode = Creator.CreateNode();
      Creator.Finalize();
    }
    GraphNode->NodeInstance = AssetNode;
    GraphNode->NodePosX = ParentGraphNode->NodePosX + ChildIndex * 320;
    GraphNode->NodePosY = ParentGraphNode->NodePosY + 160;
    ++Spawned;
  }
  UEdGraphPin* ParentOut = PinOf(ParentGraphNode, EGPD_Output);
  UEdGraphPin* ChildIn = PinOf(GraphNode, EGPD_Input);
  if (ParentOut && ChildIn && !ParentOut->LinkedTo.Contains(ChildIn)) { ParentOut->MakeLinkTo(ChildIn); }
  if (UBTCompositeNode* Composite = Cast<UBTCompositeNode>(AssetNode))
  {
    for (int32 Index = 0; Index < Composite->Children.Num(); ++Index)
    {
      UBTNode* Child = nullptr;
      if (Composite->Children[Index].ChildComposite) { Child = Composite->Children[Index].ChildComposite; }
      else { Child = Composite->Children[Index].ChildTask; }
      Spawned += SyncSubtree(Graph, Child, GraphNode, Index);
    }
  }
  return Spawned;
}
} // namespace
#endif

int32 SyncBehaviorTreeGraphFromAsset(UBehaviorTree* BehaviorTree, UEdGraph* Graph)
{
#if MCP_HAS_BEHAVIOR_TREE_GRAPH
  if (!BehaviorTree || !Graph || !BehaviorTree->RootNode) return 0;
  UEdGraphNode* RootGraphNode = nullptr;
  for (UEdGraphNode* Node : Graph->Nodes) { if (Cast<UBehaviorTreeGraphNode_Root>(Node)) { RootGraphNode = Node; break; } }
  if (!RootGraphNode) return 0;
  return SyncSubtree(Graph, BehaviorTree->RootNode, RootGraphNode, 0);
#else
  return 0;
#endif
}
}
