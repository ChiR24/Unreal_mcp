#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeHandlersPrivate.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "EdGraphSchema_BehaviorTree.h"

namespace McpBehaviorTreeHandlers {

// break_connections unlinks a node; remove_node also deletes it.
bool HandleUnlinkNode(UMcpAutomationBridgeSubsystem* Subsystem,
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

  GraphContext.BehaviorTree->Modify();
  GraphContext.Graph->Modify();
  TargetNode->Modify();
  GraphContext.Graph->GetSchema()->BreakNodeLinks(*TargetNode);
  if (bRemove) {
    GraphContext.Graph->RemoveNode(TargetNode);
  }
  UpdateBehaviorTreeAsset(GraphContext);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, GraphContext.BehaviorTree);
  Subsystem->SendAutomationResponse(Context.RequestingSocket,
                                    Context.RequestId, true,
                                    bRemove ? TEXT("Node removed.") : TEXT("Connections broken."), Result);
  return true;
#endif
}
}
