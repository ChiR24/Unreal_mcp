#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BehaviorTree/McpBTPrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "EdGraphSchema_BehaviorTree.h"

namespace McpBehaviorTreeHandlers {

// break_connections unlinks a node; remove_node also deletes it.
bool HandleUnlinkNode(UEngineWireSubsystem* Subsystem,
                      const FRequestContext& Context,
                      const FGraphContext& GraphContext, bool bRemove)
{
#if !MCP_HAS_BEHAVIOR_TREE_GRAPH
  Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                 TEXT("Behavior Tree graph editing requires UE 5.3+"),
                                 TEXT("NOT_SUPPORTED"));
  return true;
#else
  FString NodeId;
  Context.Payload->TryGetStringField(TEXT("nodeId"), NodeId);
  UEdGraphNode* TargetNode =
      FindGraphNodeByIdOrName(GraphContext.Graph, NodeId);
  if (!TargetNode) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Node not found."),
                                   TEXT("NODE_NOT_FOUND"));
    return true;
  }
  // The editor's own gate: the Root node (without it the rebuild makes an empty tree and nothing can be linked again)
  // and the read-only nodes of an injected subtree.
  if (bRemove && !TargetNode->CanUserDeleteNode()) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   FString::Printf(TEXT("Nothing was removed: %s cannot be deleted (the editor manages the graph's Root node and read-only nodes)."),
                                                   *TargetNode->GetName()),
                                   TEXT("PROTECTED_NODE"));
    return true;
  }

  // A decorator or service is not in the graph's node list: it hangs on a parent node and leaves through it, as the
  // editor removes one (UAIGraphNode::DestroyNode). RemoveNode on the graph found nothing and the reply said removed.
  UAIGraphNode* Subnode = bRemove ? Cast<UAIGraphNode>(TargetNode) : nullptr;
  UAIGraphNode* SubnodeParent = Subnode ? Subnode->ParentNode.Get() : nullptr;

  // One undo step: breaking a link modifies the nodes on both ends, which the engine records inside the step.
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  {
    const FMcpAssetEdit Edit(bRemove ? TEXT("Remove Behavior Tree Node") : TEXT("Break Behavior Tree Node Links"),
                             BehaviorTreeEditObjects(GraphContext, {TargetNode, SubnodeParent}), Result);
    GraphContext.Graph->GetSchema()->BreakNodeLinks(*TargetNode);
    if (SubnodeParent) {
      SubnodeParent->RemoveSubNode(Subnode);
    } else if (bRemove) {
      GraphContext.Graph->RemoveNode(TargetNode);
    }
    UpdateBehaviorTreeAsset(GraphContext);
  }
  McpSafeAssetSave(GraphContext.BehaviorTree);
  McpHandlerUtils::AddVerification(Result, GraphContext.BehaviorTree);
  Subsystem->SendAutomationResponse(Context.RequestingSocket,
                                    Context.RequestId, true,
                                    bRemove ? TEXT("Node removed.") : TEXT("Connections broken."), Result);
  return true;
#endif
}
}
