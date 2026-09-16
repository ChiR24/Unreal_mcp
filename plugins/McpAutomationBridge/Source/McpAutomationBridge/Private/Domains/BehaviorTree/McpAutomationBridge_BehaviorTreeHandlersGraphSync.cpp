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
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Service.h"
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

bool HasSubnodeFor(UBehaviorTreeGraphNode* GraphNode, const UBTNode* Instance)
{
  for (const TObjectPtr<UAIGraphNode>& Sub : GraphNode->SubNodes)
  { if (IsValid(Sub) && Sub->NodeInstance == Instance) return true; }
  return false;
}

// Asset-route add_decorator / add_service write straight onto UBehaviorTree, so
// the corresponding graph node carried no subnode. UBehaviorTreeGraph rebuilds
// the asset FROM the graph (CreateBTFromGraph empties RootDecorators and
// re-collects them from the root graph node), so the first graph-route edit
// afterwards silently deleted every decorator and service authored that way -
// the same data loss the SpawnMissingNodes crash produced, by a quieter route.
// Mirroring them into the graph in array order keeps DecoratorOps indices valid.
// Deliberately NOT UAIGraphNode::AddSubNode: that helper ends with
// GetAIGraph()->UpdateAsset(), which rebuilds the asset FROM the graph. Called
// from inside this walk it discarded the very UBTNode tree being iterated and
// left dangling instance pointers behind (an access violation the next time
// anything read SubNodes[i]->NodeInstance). This does the same attachment work
// without the re-entrant rebuild; the caller owns when UpdateAsset runs.
void AttachSubnode(UBehaviorTreeGraphNode* Parent, UBehaviorTreeGraphNode* Sub, UBTNode* Instance)
{
  Sub->SetFlags(RF_Transactional);
  Sub->NodeInstance = Instance;
  Sub->ParentNode = Parent;
  Sub->CreateNewGuid();
  Sub->PostPlacedNewNode();
  Sub->AllocateDefaultPins();
  Parent->SubNodes.Add(Sub);
  Parent->OnSubNodeAdded(Sub);
}

int32 SyncSubnodes(UEdGraph* Graph, UBehaviorTreeGraphNode* GraphNode,
                   const TArray<TObjectPtr<UBTDecorator>>& Decorators,
                   const TArray<TObjectPtr<UBTService>>& Services)
{
  if (!GraphNode) return 0;
  int32 Spawned = 0;
  for (const TObjectPtr<UBTDecorator>& Decorator : Decorators)
  {
    if (!IsValid(Decorator) || HasSubnodeFor(GraphNode, Decorator)) continue;
    AttachSubnode(GraphNode, NewObject<UBehaviorTreeGraphNode_Decorator>(Graph), Decorator);
    ++Spawned;
  }
  for (const TObjectPtr<UBTService>& Service : Services)
  {
    if (!IsValid(Service) || HasSubnodeFor(GraphNode, Service)) continue;
    AttachSubnode(GraphNode, NewObject<UBehaviorTreeGraphNode_Service>(Graph), Service);
    ++Spawned;
  }
  return Spawned;
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
    Spawned += SyncSubnodes(Graph, GraphNode, {}, Composite->Services);
    for (int32 Index = 0; Index < Composite->Children.Num(); ++Index)
    {
      UBTNode* Child = nullptr;
      if (Composite->Children[Index].ChildComposite) { Child = Composite->Children[Index].ChildComposite; }
      else { Child = Composite->Children[Index].ChildTask; }
      Spawned += SyncSubtree(Graph, Child, GraphNode, Index);
      // A child's decorators live on the PARENT composite's child entry but hang
      // off the CHILD graph node, exactly as SpawnMissingGraphNodesWorker does it.
      Spawned += SyncSubnodes(Graph, FindGraphNodeForInstance(Graph, Child),
                              Composite->Children[Index].Decorators, {});
    }
  }
  else if (UBTTaskNode* Task = Cast<UBTTaskNode>(AssetNode))
  {
    Spawned += SyncSubnodes(Graph, GraphNode, {}, Task->Services);
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
  int32 Spawned = SyncSubtree(Graph, BehaviorTree->RootNode, RootGraphNode, 0);
  // Root decorators hang off the graph node of the root COMPOSITE, not the Root
  // sentinel: that is the node CreateBTFromGraph re-collects RootDecorators from.
  Spawned += SyncSubnodes(Graph, FindGraphNodeForInstance(Graph, BehaviorTree->RootNode),
                          BehaviorTree->RootDecorators, {});
  return Spawned;
#else
  return 0;
#endif
}
}
