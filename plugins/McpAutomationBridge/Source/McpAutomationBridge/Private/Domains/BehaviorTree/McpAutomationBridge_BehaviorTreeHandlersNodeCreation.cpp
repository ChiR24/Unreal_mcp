#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeHandlersPrivate.h"

#if WITH_EDITOR
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Services/BTService_DefaultFocus.h"
#include "BehaviorTree/Tasks/BTTask_FinishWithResult.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_RotateToFaceBBEntry.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraphNode.h"

namespace {

void ResolveBehaviorTreeNodeClasses(const FString& NodeType,
                                    UClass*& NodeClass,
                                    UClass*& NodeInstanceClass)
{
  NodeClass = nullptr;
  NodeInstanceClass = nullptr;
  if (NodeType == TEXT("Sequence")) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Composite"));
    NodeInstanceClass = UBTComposite_Sequence::StaticClass();
  } else if (NodeType == TEXT("Selector")) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Composite"));
    NodeInstanceClass = UBTComposite_Selector::StaticClass();
  } else if (NodeType == TEXT("SimpleParallel")) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Composite"));
    NodeInstanceClass = UBTComposite_SimpleParallel::StaticClass();
  } else if (NodeType == TEXT("Wait") || NodeType == TEXT("Task")) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Task"));
    NodeInstanceClass = UBTTask_Wait::StaticClass();
  } else if (NodeType == TEXT("MoveTo")) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Task"));
    NodeInstanceClass = UBTTask_MoveTo::StaticClass();
  } else if (NodeType == TEXT("RotateTo")) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Task"));
    NodeInstanceClass = UBTTask_RotateToFaceBBEntry::StaticClass();
  } else if (NodeType == TEXT("RunBehavior")) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Task"));
    NodeInstanceClass = UBTTask_RunBehavior::StaticClass();
  } else if (NodeType == TEXT("Fail") || NodeType == TEXT("Succeed")) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Task"));
    NodeInstanceClass = UBTTask_FinishWithResult::StaticClass();
  } else if (NodeType == TEXT("Root")) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Root"));
  } else if (NodeType == TEXT("Decorator") || NodeType == TEXT("Blackboard")) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Decorator"));
    NodeInstanceClass = UBTDecorator_Blackboard::StaticClass();
  } else if (NodeType == TEXT("Service") || NodeType == TEXT("DefaultFocus")) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Service"));
    NodeInstanceClass = UBTService_DefaultFocus::StaticClass();
  } else if (NodeType == TEXT("Composite")) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Composite"));
    NodeInstanceClass = UBTComposite_Sequence::StaticClass();
  }
}

void ResolveCustomBehaviorTreeNodeClasses(const FString& NodeType,
                                          UClass*& NodeClass,
                                          UClass*& NodeInstanceClass)
{
  if (NodeClass) return;
  UClass* Resolved = ResolveClassByName(NodeType);
  if (!Resolved) return;
  if (Resolved->IsChildOf(UBTCompositeNode::StaticClass())) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Composite"));
    NodeInstanceClass = Resolved;
  } else if (Resolved->IsChildOf(UBTTaskNode::StaticClass())) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Task"));
    NodeInstanceClass = Resolved;
  } else if (Resolved->IsChildOf(UBTDecorator::StaticClass())) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Decorator"));
    NodeInstanceClass = Resolved;
  } else if (Resolved->IsChildOf(UBTService::StaticClass())) {
    NodeClass = FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Service"));
    NodeInstanceClass = Resolved;
  }
}

}

namespace McpBehaviorTreeHandlers {

bool HandleAddNode(UMcpAutomationBridgeSubsystem* Subsystem,
                   const FRequestContext& Context,
                   const FGraphContext& GraphContext)
{
#if !MCP_HAS_BEHAVIOR_TREE_GRAPH
  Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                 TEXT("Behavior Tree graph editing requires UE 5.3+"),
                                 TEXT("NOT_SUPPORTED"));
  return true;
#else
  FString NodeType;
  Context.Payload->TryGetStringField(TEXT("nodeType"), NodeType);
  float X = 0.0f;
  float Y = 0.0f;
  const bool bHasX = Context.Payload->TryGetNumberField(TEXT("x"), X);
  const bool bHasY = Context.Payload->TryGetNumberField(TEXT("y"), Y);
  if (Context.Payload->HasField(TEXT("x")) && !bHasX) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Invalid value for 'x': expected number"),
                                   TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (Context.Payload->HasField(TEXT("y")) && !bHasY) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Invalid value for 'y': expected number"),
                                   TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ProvidedNodeId;
  Context.Payload->TryGetStringField(TEXT("nodeId"), ProvidedNodeId);
  UClass* NodeClass = nullptr;
  UClass* NodeInstanceClass = nullptr;
  ResolveBehaviorTreeNodeClasses(NodeType, NodeClass, NodeInstanceClass);
  ResolveCustomBehaviorTreeNodeClasses(NodeType, NodeClass, NodeInstanceClass);

  if (!NodeClass) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket, Context.RequestId,
        FString::Printf(TEXT("Unknown node type '%s'"), *NodeType),
        TEXT("UNKNOWN_TYPE"));
    return true;
  }

  UObject* NewNodeObj =
      NewObject<UObject>(GraphContext.Graph, NodeClass, NAME_None,
                         RF_Transactional);
  UClass* BTNodeBaseClass =
      FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode"));
  if (!NewNodeObj || !BTNodeBaseClass ||
      !NewNodeObj->GetClass()->IsChildOf(BTNodeBaseClass)) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Failed to create node object."),
                                   TEXT("CREATE_FAILED"));
    return true;
  }

  UBehaviorTreeGraphNode* NewNode =
      static_cast<UBehaviorTreeGraphNode*>(NewNodeObj);
  GraphContext.BehaviorTree->Modify();
  GraphContext.Graph->Modify();
  NewNode->Modify();
  NewNode->CreateNewGuid();
  if (NodeInstanceClass) {
    NewNode->ClassData = FGraphNodeClassData(NodeInstanceClass, TEXT(""));
  }
  FGuid NewGuid;
  if (!ProvidedNodeId.IsEmpty() && FGuid::Parse(ProvidedNodeId, NewGuid)) {
    NewNode->NodeGuid = NewGuid;
  }
  NewNode->NodePosX = X;
  NewNode->NodePosY = Y;
  GraphContext.Graph->AddNode(NewNode, true, false);
  NewNode->PostPlacedNewNode();
  // Guard against duplicate pins: some node types already allocate in
  // PostPlacedNewNode(), so only allocate when the node has no pins yet.
  if (NewNode->Pins.Num() == 0) { NewNode->AllocateDefaultPins(); }

  if (NodeInstanceClass && !NewNode->NodeInstance) {
    GraphContext.Graph->RemoveNode(NewNode);
    Subsystem->SendAutomationError(
        Context.RequestingSocket, Context.RequestId,
        TEXT("Failed to initialize Behavior Tree node instance."),
        TEXT("CREATE_FAILED"));
    return true;
  }

  UpdateBehaviorTreeAsset(GraphContext);
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
  McpHandlerUtils::AddVerification(Result, GraphContext.BehaviorTree);
  Subsystem->SendAutomationResponse(Context.RequestingSocket,
                                    Context.RequestId, true,
                                    TEXT("Node added."), Result);
  return true;
#endif
}

}
#endif
