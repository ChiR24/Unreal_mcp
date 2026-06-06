#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BehaviorTreeHandlersPrivate.h"

#if WITH_EDITOR
#include "McpHandlerUtils.h"
#include "EdGraphSchema_BehaviorTree.h"

namespace McpBehaviorTreeHandlers {

bool HandleRemoveNode(UMcpAutomationBridgeSubsystem* Subsystem,
                      const FRequestContext& Context,
                      const FGraphContext& GraphContext)
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
  GraphContext.Graph->RemoveNode(TargetNode);
  UpdateBehaviorTreeAsset(GraphContext);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, GraphContext.BehaviorTree);
  Subsystem->SendAutomationResponse(Context.RequestingSocket,
                                    Context.RequestId, true,
                                    TEXT("Node removed."), Result);
  return true;
#endif
}

bool HandleBreakConnections(UMcpAutomationBridgeSubsystem* Subsystem,
                            const FRequestContext& Context,
                            const FGraphContext& GraphContext)
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
  UpdateBehaviorTreeAsset(GraphContext);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, GraphContext.BehaviorTree);
  Subsystem->SendAutomationResponse(Context.RequestingSocket,
                                    Context.RequestId, true,
                                    TEXT("Connections broken."), Result);
  return true;
#endif
}

}
#endif
